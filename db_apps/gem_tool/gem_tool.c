//
// A CLI interface to GEM API, that allows scripts (not written in C)
// to easily create RDB entries for GEM daemon
// Generally gem_tool works by adding one entry (e.g. one ee/trigger pair) into the RDB.
// Therefore, it can be called in a loop from a script to create multiple entries.
// It does NOT start the daemon automatically.
//
// Please see usage for options.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h> // for opendir(), readdir(), closedir()
#include <sys/stat.h> // for stat()
#include <sys/inotify.h>
#include <syslog.h>
#include <stdarg.h>
#include <signal.h>
#include <limits.h>
#include "fcntl.h"

#include "rdb_ops.h"
#include "gem_api.h"

//
// A helper to get file size
//
static int check_file_size(char *file_name)
{
    struct stat sb;

    if (stat(file_name, &sb) == 0)
    {
        return sb.st_size;
    }

    return -1;
}

//
// generate entries in RDB for GEM so it can run as a RDB template manager
// 1:1 replacement
//
static int generate_template_gem_rdb_root(char *rdb_root, char *template_dir, int do_reload)
{
    int num_template_files, i;
    const char *file_name;

    struct dirent **ppNameList;

    gem_api_ee_entry eae;
    int last_index = 0;

    gem_clear_api_entry(&eae);
    eae.tr_type = GEM_TRIGGER_TEMPLATE;
    eae.ee_type = GEM_TEMPLATE_EE;
    eae.file_name = NULL;
    eae.timeout = 30;  // some templates take ages to execute
    eae.restart = 0;

      // set reload value to 0
    if (!rdb_root || !template_dir)
    {
        fprintf(stderr, "Template generate - parameters wrong\n");
        return -1;
    }

    gem_delete_rdb_root(rdb_root, TRUE, TRUE);

    if (gem_set_reload_value(rdb_root, GEM_DAEMON_NORMAL) != 0)
    {
        fprintf(stderr, "Failed to set reload value\n");
        return -1;
    }

    // find all template files.
    num_template_files = scandir(template_dir, &ppNameList, NULL, alphasort);
    if (num_template_files < 0)
    {
        fprintf(stderr, "failed to enumerate template directory (name=%s)", template_dir);
        return -1;
    }

    // go through all the file names in the template directory
    for (i = 0; i < num_template_files; i++)
    {
        file_name = ppNameList[i]->d_name;

        // find all template files
        if (!strstr(file_name, ".template"))
        {
            continue;
        }

        // format full file name of the template file
        eae.file_name  = malloc(1024);
        sprintf(eae.file_name, "%s/%s", template_dir, file_name);

        eae.name = malloc (1024);
        sprintf(eae.name, "Template replacement for %s", file_name);

        if (gem_add_api_entry(rdb_root, &eae, NULL, &last_index) == -1)
        {
            fprintf(stderr, "Failed to add template API entry for %s", eae.file_name);
            free (eae.file_name);
            free (eae.name);
            return -1;
        }

        free (eae.file_name);
        free (eae.name);
    }

    free (ppNameList);

    // set to reload so the daemon can start without extra steps
    if (do_reload)
    {
        gem_set_reload_value(rdb_root, GEM_DAEMON_RELOAD);
    }

    return 0;
}


// options
static const char *options = "d:M:r:n:f:Rt:e:LST:F:o:p:v:c:V:COsh";

// usage
static const char *usage_str = "\ngem_tool provides a CLI interface to GEM API\n"
                                "Special mode options:\n"
                                "-d rdb_root to delete all RDB entries in rdb_root\n"
                                "-r rdb_root [-s] -M path_to_template_dir to generate all RDB data required for GEM to replace the RDB template manager\n\n"
                                "Single EE mode options (one EE/trigger entry will be created in RDB):\n"
                                "-r [rdb_root]\n"
                                "-O instead of adding the entry in the next available slot, it will take up EE slot #0 and overwrite the old record #0\n"
                                "-s will set the reload value of GEM daemon to reload so daemon will automatically load this RDB root if running\n"
                                "-n [executable_entry_name]\n"
                                "-f [ee_file_path] (full path to where the executable is stored)\n"
                                "-R to place and load the executable in an RDB variable (instead of a file)\n"
                                "-C add CRC32 to protect the executable stored in RDB (only use this with -R option)\n"
                                "-t [tr_type] type of trigger: 1-direct RDB, 2-compare RDB, 3-indirect RDB, 4-timer, 5-file/inotify, 7-template\n"
                                "-e [ee_type] type of executable: 1-generic, 2-template, 3-Lua snippet\n"
                                "-L enable reLoad mode\n"
                                "-S enable reStart mode\n"
                                "-T [timeout] allowed time in seconds that EE can run for, -1 will cause it to never expire\n"
                                "-F [watch_name] for file/infotify triggers only, the full path to file/directory to watch\n"
                                "-o [CDM] for file/infotify triggers only, a combination of C(Create), D(Delete) and M(Modify) characters\n"
                                "-p [period] for timer triggers only, the regular period, in seconds, at which the EE runs\n"
                                "-v [rdb_names] for rdb indirect and compare triggers only (types 2&3), the name of RDB variable(s) to trigger the EE\n"
                                "-c [EQ/NEQ] for rdb compare trigger only (type 2), the name of compare function\n"
                                "-V [value] for rdb compare trigger only (type 2), the value to compare with\n"
                                "Example 1 (direct rdb trigger):gem_tool -r logic_builder -n NAME -f /etc/bin/trigger_io -t 1 -e 1\n"
                                "Example 2 (template manager replacement):gem_tool -r template_manager -M /etc/cdcs/conf/mgr_templates\n"
                                "Example 3 (delete rdb entries):gem_tool -d logic_builder\n";

//
// Usage shown when -h is entered at command line
//
static void usage()
{
    fprintf(stderr, usage_str);
}

//
// Main function for GEM tool.
// This is a command line interface for usage by scripts to the API
//
// Command line arguments supported
//
#define MAX_FILE_IN_RDB_SIZE_PLAIN (500)
#define MIN_FILE_IN_RDB_SIZE_PLAIN (5)
#define MAX_FILE_IN_RDB_SIZE_ENCODED (800)

//
// Main entry point
//
int main(int argc, char *argv[])
{
    int ret;

    char *rdb_root = NULL;
    char *file_or_snippet = NULL;
    char *template_folder = NULL;
    char *inotify_flags = NULL;
    int ee_in_rdb = FALSE; // is executable in RDB (or file otherwise)
    int add_crc = FALSE; // add CRC protection
    int last_index = -1;
    int do_reload = FALSE;
    char name_buf[64];

    gem_api_ee_entry eae;
    gem_api_f_trigger_entry file_trigger;
    gem_api_r_trigger_entry rdb_trigger;
    gem_api_t_trigger_entry tmr_trigger;

    gem_clear_api_entry(&eae);
    eae.name = "EE name"; // some default name
    eae.timeout = 10; // some default timeout

    file_trigger.watch_name = NULL;
    file_trigger.flags = 0;

    rdb_trigger.compare_value = NULL;
    rdb_trigger.compare_func = "";
    rdb_trigger.indirect_var_name = NULL;

    tmr_trigger.period_sec = -1; // set to invalid value

    int rdb_fd = rdb_open_db();
    if (rdb_fd < 0)
    {
        fprintf(stderr, "Could not open RDB\n");
        return 1;
    }

    while ((ret = getopt(argc, argv, options)) != EOF)
    {
        switch (ret)
        {
        case 'd':
            // delete RDB root
            rdb_root = optarg;
            printf("Deleting rdb root %s\n", rdb_root);
            gem_delete_rdb_root(rdb_root, TRUE, FALSE);
            rdb_close_db();
            return 0;

        case 'M':
            // automatic conversion of templates to RDB
            template_folder = optarg;

            // make sure Rdb root is valid
            if (rdb_root && template_folder)
            {
                generate_template_gem_rdb_root(rdb_root, template_folder, do_reload);
            }

            // this operation is done automatically and we exit
            rdb_close_db();
            return 0;

        case 'r':
            // name of rdb root
            rdb_root = optarg;
            break;

        case 'n':
            // name of EE
            strcpy(name_buf, optarg);
            eae.name = name_buf;
            break;

        case 'f':
            // file where the EE is located
            file_or_snippet = optarg;
            break;

        case 'R':
            // if specified, the EE file will be encoded into an RDB variable
            ee_in_rdb = TRUE;
            break;

        case 't':
            // type of trigger
            eae.tr_type = atoi(optarg);
            break;

        case 'e':
            // type of EE
            eae.ee_type = atoi(optarg);
            break;

        case 'L':
            // optional reload/reread mode
            eae.reread_mode = TRUE;
            break;

        case 'S':
            // optional restart mode
            eae.restart = TRUE;
            break;

        case 'T':
            // optional timeout, e.g. time when EE will be killed
            eae.timeout = atoi(optarg);
            if (eae.timeout == 0)
            {
                // EE is never killed
                eae.timeout = -1;
            }
            break;

        case 'F':
            // for inotify triggers, directory or file to watch
            file_trigger.watch_name = optarg;
            break;

        case 'o':
            // CDM flags for create, delete or modify for
            // inotify triggers only
            inotify_flags = optarg;
            break;

        case 'p':
            // for timer triggered EEs, timer period in seconds
            tmr_trigger.period_sec = atoi(optarg);
            break;

        case 'v':
            // for indirect and compare RDB triggers, the rdb variable name
            // (also can be a wildcard or a list)
            rdb_trigger.indirect_var_name = optarg;
            break;

        case 'c':
            //  for compare RDB triggers, the compare function (EQ/NEQ)
            rdb_trigger.compare_func = optarg;
            break;

        case 'V':
            //  for compare RDB triggers, the compare value
            rdb_trigger.compare_value = optarg;
            break;

        case 'C':
            // optional CRC32 protection for EEs stored in RDB
            add_crc = TRUE;
            break;

        case 'O':
            // overwrite the very first entry
            last_index = 0;
            break;

        case 's':
            // write "Reload" to RDB reload variable
            do_reload = TRUE;
            break;

        case 'h':
            usage();
            rdb_close_db();
            return 0;

        }
    }

    ret = 0;

    if ((eae.ee_type != GEM_GENERIC_EE) &&
        (eae.ee_type != GEM_TEMPLATE_EE) &&
        (eae.ee_type != GEM_LUA_SCRIPT_EE))
    {
        fprintf(stderr, "Incorrect executable entry type %d\n", eae.ee_type);
        usage();
        rdb_close_db();
        return 1;
    }

    // work out flags
    if (inotify_flags)
    {
        file_trigger.flags = 0;
        if (strchr(inotify_flags, 'C'))
            file_trigger.flags |= IN_CREATE;
        if (strchr(inotify_flags, 'D'))
            file_trigger.flags |= IN_DELETE;
        if (strchr(inotify_flags, 'M'))
            file_trigger.flags |= IN_MODIFY;
    }

    if (ee_in_rdb)
    {
        // user wants us to put contents of the file specified in file_or_snippet
        // file into rdb
        // 1) Check the file size (as we cannot put crazily large things in RDB)
        // 2) Read file into a buffer
        // 3) Encode it using base64
        // 4) Assign
        int size = check_file_size(file_or_snippet);

        if ((size < MIN_FILE_IN_RDB_SIZE_PLAIN) || (size > MAX_FILE_IN_RDB_SIZE_PLAIN))
        {
            fprintf(stderr, "File %s size is incorrect for storing in rdb\n", file_or_snippet);
            rdb_close_db();
            return 1;
        }

        char *file_buf = malloc(size+1);
        memset(file_buf, 0, size+1);

        FILE *f = fopen(file_or_snippet, "r");
        if (fread(file_buf, size, 1, f) != 1)
        {
            fprintf(stderr, "Failed to read file %s\n", file_or_snippet);
            rdb_close_db();
            free (file_buf);
            return 1;
        }
        fclose(f);

        // check for max size - let's say 500 bytes is max
        // before encoding
        eae.snippet = malloc(gem_base64_get_encoded_len(size));
        if (gem_base64encode(file_buf, size, eae.snippet, 0) <=0 )
        {
            fprintf(stderr, "Failed to encode file %s\n", file_or_snippet);
            rdb_close_db();
            free (file_buf);
            free (eae.snippet);
            return 1;
        }

        if (add_crc)
        {
            eae.crc32 = gem_crc32(file_buf, strlen(file_buf), 0);
        }
    }
    else
    {
        eae.file_name = file_or_snippet;
    }

    switch (eae.tr_type)
    {
        case GEM_TRIGGER_DIRECT_RDB:
        case GEM_TRIGGER_COMPARE_RDB:
        case GEM_TRIGGER_INDIRECT_RDB:
            if (gem_add_api_entry(rdb_root, &eae, (void *)&rdb_trigger, &last_index) == -1)
            {
                fprintf(stderr, "Failed to add GEM RDB entry\n");
                ret = 1;
            }
            break;

        case GEM_TRIGGER_TIMER:
            if (gem_add_api_entry(rdb_root, &eae, (void *)&tmr_trigger, &last_index) == -1)
            {
                fprintf(stderr, "Failed to add GEM RDB entry\n");
                ret = 1;
            }
            break;

        case GEM_TRIGGER_FILE:
            if (gem_add_api_entry(rdb_root, &eae, (void *)&file_trigger, &last_index) == -1)
            {
                fprintf(stderr, "Failed to add GEM RDB entry\n");
                ret = 1;
            }
            break;

        case GEM_TRIGGER_TEMPLATE:
            // for templates, there is some coupling between ee and trigger types
            if (eae.ee_type != GEM_TEMPLATE_EE)
            {
                fprintf(stderr, "Incorrect executable entry type %d for template\n", eae.ee_type);
                ret = 1;
            }

            if (gem_add_api_entry(rdb_root, &eae, NULL, &last_index) == -1)
            {
                fprintf(stderr, "Failed to add GEM RDB entry\n");
                ret = 1;
            }
            break;

        default:
            fprintf(stderr, "Invalid trigger type %d\n", eae.tr_type);
            usage();
            ret = 1;
            break;
    }

    //
    // gem_add_api_entry is messy in a sense that it leaves partial records
    // if it has failed. We want to clean them up!
    // Last_index is pointing at the next entry hence -1
    //
    if ((ret != 0) && (last_index >= 1))
    {
        // delete only records for one last EE that was unsuccessful
        sprintf(name_buf, "%s.%d", rdb_root, last_index-1);
        gem_delete_rdb_root(name_buf, FALSE, FALSE);
    }

    if (do_reload)
    {
        gem_set_reload_value(rdb_root, GEM_DAEMON_RELOAD);
    }

    // close rdb
    rdb_close_db();

    // free any memory allocated (free(0) is safe)
    free (eae.snippet);

    return ret;
}
