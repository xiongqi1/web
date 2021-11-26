//
// Part of GEM daemon
// Code that supports executable entities (EEs).
//

//#define _GNU_SOURCE
//#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/times.h>

// @todo - depending on HOST/TARGET, include correct file
#ifdef V_HOST
#include "../../cdcs_libs/mock_rdb_lib/rdb_operations.h"
#else
#include "rdb_ops.h"
#endif

#include "gem_api.h"
#include "gem_daemon.h"


#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MIN_EXE_LEN 2 // the minimum length of executable

// directory for temp files.
char temp_file_directory[GEM_MAX_DIR_LENGTH];

static clock_t ticks_end = 0;
static clock_t ticks_pers_sec = 0;

gem_ee_table_type gem_ee_table; // table of EE's

///////////////////////////////////////////////////////////////////////////////

// system version of basename actually changes buffer passed in "path"
// this one doesn't
char *gem_basename(char *path)
{
    char *base = strrchr(path, '/');
    return base ? base+1 : path;
}

//
// Get number of ticks per second
//
static clock_t get_ticks_per_sec(void)
{
    return sysconf(_SC_CLK_TCK);
}

//
// Get number of ticks since system restart
//
static clock_t getTickCount(void)
{
    struct tms tm;

    return times(&tm);
}

#ifdef GEM_PERFORMANCE_MSGS_ON
// some stuff for performance monitoring
static int print_us_stamp(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    printf("GEM %lu %lu\n", t.tv_sec, t.tv_usec);
    return 0;
}
#endif

//
// Execute an executable  file given by cmd_line
// String is passed as a command line and any args are
// passed to the executable
//
//
// Return child process's pid on success, -1 otherwise
//
static int gem_execute_file(const char* cmd_line)
{
#ifdef GEM_PERFORMANCE_MSGS_ON
    print_us_stamp(); // check the performance
#endif

    pid_t pid = fork();

    // if child
    if (pid == 0)
    {
        //
        // moving this block before fork()
        // doesn't appear to make any difference
        // whatsoever - one would have thought that
        // perhaps the process table doesn't need
        // to be copied given fork() is immediately
        // followed by execv
        // Also, using vfork and execve make no difference
        // to what process table looks in respect to
        // child processes
        //
        char path_buf[1024];
        char *p_path;

        int  arg_val_num = 0;
        char *arg_val_buf[256];

        // copy
        strcpy(path_buf, cmd_line);

        p_path = path_buf;

        // splice - chopping and getting each pointer
        while (arg_val_num < sizeof(arg_val_buf) - 1)
        {
            arg_val_buf[arg_val_num++] = p_path;

            // until a space or null
            while (*p_path && (*p_path != ' '))
                p_path++;

            if (!*p_path)
                break;

            *p_path++ = 0;
        }

        // terminate with 0
        arg_val_buf[arg_val_num] = 0;

        // launch
        execv(arg_val_buf[0], arg_val_buf);

        // if invalid command line, we will return straight away
        // otherwise, we never get here

        exit(EXIT_FAILURE); // so GEM will know this did not work out as planned
    }
    // if parent
    else if (pid > 0)
    {
        // child is executing!
        return pid;
    }
    else
    {
        return -1;
    }
}

//
// Execute an template file in netcomm template format
//
// Return pid on success, -1 otherwise
//
static int gem_execute_template_file(char *templateFile, int ee_number)
{
    int pid;

    char temp_file[PATH_MAX];

    // unlike sys version, gem_basename will not stuff up the templateFile buffer
    // Also note that the name is EE unique
    sprintf(temp_file, "%s/%s_ee%d-temp", temp_file_directory, gem_basename(templateFile),
            ee_number);

    FILE *streamOut = fopen(temp_file, "wt");
    if (!streamOut)
    {
        gem_syslog(LOG_CRIT, "failed to open %s for writing", temp_file);
        return -1;
    }

    // make sure newly created temp file has appropriate permissions
    int nMode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    int fno = fileno(streamOut);
    if (fchmod(fno, nMode) < 0)
    {
        gem_syslog(LOG_CRIT, "failed to change %s mode to executable permission",
                   temp_file);
        return -1;
    }

    //
    // do run-time substitution of template file special symbols into
    // their values from RDB
    //
    if (gem_templatemgr_parse_template(templateFile, streamOut, 0) < 0)
    {
        gem_syslog(LOG_CRIT, "failed to change parse template file %s on the fly",
                   templateFile);
        return -1;
    }

    // close the temp file
    fclose(streamOut);

    // the final gotcha - the name of original template file is passed to
    // the executable (as per existing template manager
    sprintf(temp_file, "%s/%s_ee%d-temp %s", temp_file_directory, gem_basename(templateFile),
            ee_number, templateFile);

    // finally, execute the temp file
    pid = gem_execute_file(temp_file);

    // delete the temp file.
    unlink (temp_file);

    return pid;
}

//
// Execute a script (not Lua) stored in RDB variable
// The algorithm is to write a temp file and execute it
// script buffer was already base64 decoded and CRC checked
//
//
// Return pid on success, -1 otherwise
//
static int execute_rdb_snippet(char* script, int num_bytes, int ee_number)
{
    int pid;
    FILE *streamOut = NULL;
    char temp_file[PATH_MAX];

    if (!script || (script[0] == 0) || (num_bytes == 0))
        return -1;

    // open a temp file for writing in the temporary directory
    sprintf(temp_file, "%s/ee%d-temp", temp_file_directory, ee_number);

    // open output temp file
    streamOut = fopen(temp_file, "wt");
    if (!streamOut)
    {
        gem_syslog(LOG_CRIT,"failed to open %s for writing", temp_file);
        return -1;
    }

    int nMode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    int fno = fileno(streamOut);
    if (fchmod(fno, nMode) < 0)
    {
        gem_syslog(LOG_CRIT,"failed to change %s mode to executable permission", temp_file);
        return -1;
    }

    // write the content of the buffer to the temp file
    fwrite(script, num_bytes, 1, streamOut);

    // close the temp file
    fclose(streamOut);

    // run the temp file using common function
    pid = gem_execute_file(temp_file);

    // possibly delete it - but why should we?
    //unlink(temp_file);

    return pid;
}

//
// This function is required because in some applications using GEM may rely on modifying
// executables stored in RDB just before they are meant to be executed.
//
// This is not going to work unless GEM refreshes the script from the RDB before
// execution. By default, executable is read on startup and never read again (unless
// reload is done)
//
// Therefore, it is possible to request, on EE-by-EE basis, to use reread mode, where this will
// actually occur. By default this mode is off.
//
// Dynamic memory where script is stored is reallocated as required in this function.
//
// Return 0 on success, -1 otherwise
//
int reread_ee(char *rdb_root, int ee_count, gem_exec_entry *ee)
{
    int len;
    char rdb_var[GEM_MAX_RDB_NAME_LENGTH], rdb_val[GEM_MAX_RDB_VAL_LENGTH];

    sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_SNIPPET_VAR);
    len = sizeof(rdb_val);
    if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
    {
        // @todo- realloc doesn't work for some reason?. So free and malloc instead
        //ee->rdb_var_exe = realloc(ee->rdb_var_exe, strlen(rdb_val)+1);
        free (ee->rdb_var_exe);
        ee->rdb_var_exe = malloc(strlen(rdb_val)+1);
        strcpy(ee->rdb_var_exe, rdb_val);

        if (ee->rdb_var_exe)
        {
            // if CRC32 is provided, re-read it
            sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_CRC32_VAR);
            len = sizeof(rdb_val);
            if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
            {
                ee->crc32 = atol(rdb_val);
            }
            else
            {
                // otherwise, treat as if there is no CRC protection
                ee->crc32 = 0;
            }
        }
        return 0;
    }

    return -1;
}

static int gem_get_num_running_processes(void)
{
    int i, count = 0;
    for (i = 0 ; i < gem_ee_table.count ; i++)
    {
        if (gem_ee_table.ee[i].pid > 0)
        {
            count++;
        }
    }
    return count;
}


//
// An important helper that does all the work to execute an EE.
//
//
// Annoyingly, it ended up with a pointer to the RDB root.
// the only reason this has a pointer to rdb_root is because in reread mode
// it is necessary to re-read executable snippets just before execution
//
// Return child pid on success, -1 otherwise
//
static int gem_start_process(gem_exec_entry *ee, int ee_number, char *rdb_root,
                             BOOL *is_limit, BOOL *optimized)
{
    char *decoded_buf;
    int pid = -1;
    int size;

    *optimized = FALSE;


#ifdef GEM_MAX_PROCESS_LIMIT_ENABLE
    if (gem_get_num_running_processes() >= GEM_MAX_RUNNING_PROCESS_LIMIT)
    {
        *is_limit = TRUE;
        return -1;
    }
#endif

    *is_limit = FALSE;

    // there are trivial cases where EE will not be able to be launched
    // for example, an executable file may not be present at all.
    // If we do not have a limit here, GEM will be continuously trying to
    // launch the EE that may have no chance of starting. It is Ok, but
    // inefficient. The drawback is that if a file re-appeared, the EE
    // will never be launched again. So we still try after reaching a certain
    // number of errors, but at 1/100 of normal rate
    //
    if (ee->err_count >= GEM_EE_ERR_MAX)
    {
        if (ee->err_count == GEM_EE_ERR_MAX)
        {
            gem_syslog(LOG_ERR, "Demoting E %d, executable %s due to errors, count %d",
                       ee_number, ee->rdb_var_exe, ee->err_count);
        }

        // now only try once in every 100 iterations
        if (ee->err_count % 100)
        {
            *optimized = TRUE;
            return -1;
        }
        //printf("GEM Retrying\n");
    }

    // some sanity checks
    if ((ee->type<=GEM_EXEC_INVALID) || (ee->type>=GEM_EXEC_MAX))
    {
        // fatal error
        gem_syslog(LOG_CRIT,"Error ee %d type wrong %d", ee_number, ee->type);
        return -1;
    }

    if (ee->in_file && ee->file_exe)
    {
        // script or binary is in a file - run it

        // is it a template EE?
        if ((ee->type == GEM_TEMPLATE_EE) && strstr(ee->file_exe,
                TEMPLATELIST_FILE_EXT))
        {
            // yes, execute it
            pid =  gem_execute_template_file(ee->file_exe, ee_number);
        }
        else
        {
            // run differently depending on whether it is a Lua or generic EE
            pid =  (ee->type == GEM_LUA_SCRIPT_EE) ? gem_execute_lua_file(ee->file_exe)
                   : gem_execute_file(ee->file_exe);
        }
        gem_syslog((pid != -1) ? LOG_DEBUG : LOG_ERR,
                   "Executed EE %d, type %d, pid %d, name %s, executable %s",
                   ee_number, ee->type, pid, ee->name, ee->file_exe);
        gem_stats_add_record(ee_number, GEM_STATS_EXECUTED, NULL);
    }
    else if (!ee->in_file && ee->rdb_var_exe)
    {
        //
        // in re-read mode, executables stored in rdb variables are refreshed from
        // RDB just before execution, as the users of GEM daemon may rely on
        // writing a different executable snippet before triggering
        // If we do not do that, we will execute whatever was present in
        // RDB when "reload config" flag changed
        //
        if (ee->reread_mode)
        {
            if (reread_ee(rdb_root, ee_number, ee) < 0)
            {
                gem_syslog(LOG_ERR, "Re-read executable failed EE number %d, name %s",
                           ee_number, ee->name);
                return -1;
            }
        }

        if (strlen(ee->rdb_var_exe) < MIN_EXE_LEN)
        {
            gem_syslog(LOG_ERR, "Executable length too short EE number %d, executable %s",
                       ee_number, ee->rdb_var_exe);
            return -1;
        }

        // convert snippet to memory buffer
        decoded_buf = malloc(gem_base64_get_decoded_len(strlen(ee->rdb_var_exe)));
        if (decoded_buf)
        {
            size = gem_base64decode(ee->rdb_var_exe, strlen(ee->rdb_var_exe), decoded_buf, 0);
            if (size > 0)
            {
                //
                // Has CRC, need to check it!
                // -1 because 0 was added for termination
                //
                if ((ee->crc32) && (ee->crc32 != gem_crc32(decoded_buf, size-1, 0)))
                {
                    free (decoded_buf);
                    gem_syslog(LOG_ERR, "CRC check failed on EE number %d, name %s", ee_number, ee->name);
                    return -1;
                }

                pid = (ee->type == GEM_LUA_SCRIPT_EE) ? gem_execute_lua_rdb_snippet(decoded_buf, size - 1)
                      : execute_rdb_snippet(decoded_buf, size - 1, ee_number);
                gem_syslog(LOG_DEBUG, "Executed EE %d, type %d, pid %d, name %s", ee_number, ee->type,
                           pid, ee->name);
                gem_stats_add_record(ee_number, GEM_STATS_EXECUTED, NULL);
            }
            else
            {
                gem_syslog(LOG_ERR, "Failed to decode %s", ee->rdb_var_exe);
            }
            free (decoded_buf);
        }
        else
        {
            gem_syslog(LOG_CRIT, "Malloc error");
        }
    }
    else
    {
        // handle somehow
        gem_syslog(LOG_ERR, "EE file or RDB var not found %d", ee_number);
    }

    return pid;
}

//
// A top level function that processes the EE table and runs EEs that
// are ready.
//
// the only reason this has a pointer to rdb_root is because in reread mode
// it is necessary to re-read executable snippets just before execution
//
// Return 0 on success, -1 otherwise
//
int gem_process_ees(char *rdb_root)
{
    int i, pid = -1;
    gem_exec_entry *ee;
    BOOL exited, signalled;
    int exit_status, sig_status, status;
    BOOL is_limit, optimized;

    ticks_pers_sec = get_ticks_per_sec();

    for (i = 0 ; i < gem_ee_table.count ; i++)
    {
        exited = signalled = FALSE;
        exit_status = sig_status = -1;

        ee = &gem_ee_table.ee[i];

        if (ee->pid > 0)
        {
            // non blocking, see if any children have exited
            pid = waitpid(ee->pid, &status, WNOHANG);

            // non-zero if child process has exited
            if (pid > 0)
            {
                // work out what happened - has the child exited or was killed by us
                if (WIFEXITED(status))
                {
                    exited = TRUE;
                    exit_status = WEXITSTATUS(status);
                }

                if (WIFSIGNALED(status))
                {
                    signalled = TRUE;
                    sig_status = WTERMSIG(status);
                }

#ifdef GEM_PERFORMANCE_MSGS_ON
                print_us_stamp(); // check the performance
#endif

                //
                // Increment the error count. For example, exe file is simply not
                // there and at some point we should give up (or at least slow)
                // down our attempts to start it
                //
                if ((exit_status == EXIT_FAILURE) || (sig_status == EXIT_FAILURE))
                {
                    ee->err_count++;
                }

                // log appropriately
                gem_syslog(((exit_status == EXIT_FAILURE) || (sig_status == EXIT_FAILURE))
                           ? LOG_ERR : LOG_DEBUG, "Child %s, PID = %d, status %d",
                           exited ? "exited" : "killed", pid, status);
                gem_stats_add_record(i, exited ? GEM_STATS_EXITED : GEM_STATS_KILLED, NULL);

                //
                // Sanity check
                // this should not happen as waitpid only waits on exited children and should not
                // return non-zero value for any other signals that the child may have received
                //
                if (!exited && !signalled)
                {
                    gem_syslog(LOG_CRIT, "Unexpected signal from child: pid=%d, status=%d, "
                               "exit_status=%d, sig_status=%d",
                               pid, status, exit_status, sig_status);
                }

                // mark as not running
                ee->pid = -1;

                //
                // If a flag is set to not retrigger when running,
                // the process will not restart if it was triggered again whilst it was running
                //
                if (!ee->restart)
                {
                    ee->trigger_flag = FALSE;
                }

                // restart a process that should have not terminated
                if (ee->required_state)
                {
                    //
                    // if required state is set to TRUE, this can only be happen if
                    // kill_in_secs was -1 (e.g. this ee is used as process monitor)
                    //
                    if (ee->kill_in_secs == -1)
                    {
                        pid = gem_start_process(ee, i, rdb_root, &is_limit, &optimized);
                        if (pid > 0)
                        {
                            ee->pid = pid;
                        }
                        else
                        {
                            if (is_limit)
                            {
                                gem_syslog(LOG_ERR, "Process limit reached %d", i);
                            }
                            else
                            {
                                ee->err_count++; // will eventually overflow, but this is Ok
                                if (!optimized)
                                {
                                    // only log when not optimized
                                    gem_syslog(LOG_ERR, "Failed to start process %d", i);
                                }
                            }
                        }
                    }
                    else
                    {
                        gem_syslog(LOG_CRIT, "Misconifuration in EE %d", i);
                    }
                }
            }
            else
            {
                // the process is running
                ticks_end = getTickCount();

                // reset the error count.
                ee->err_count = 0;

                //
                // kill a process if it either exceeded its permitted time to run,
                // or required state has become FALSE
                //
                if (((ee->kill_in_secs >= 0) && ((ticks_end - ee->ticks_start)
                                                 / ticks_pers_sec >= ee->kill_in_secs)) ||
                        ((ee->kill_in_secs == -1) && (ee->required_state == FALSE)))
                {
                    kill(ee->pid, SIGKILL);

                    //
                    // do nothing else here. When process terminates, we will detect it and process it.
                    // We do not want to block here in case if child process doesn't
                    // exit straight away or at all.
                    // in the same way as if it exited by itself
                    // (in TRUE section of this if/else statement). Just log a debug message
                    //
                    if (ee->kill_in_secs == -1)
                    {
                        gem_syslog(LOG_DEBUG, "Kill sent to child process (to=-1): PID = %d", ee->pid);
                    }
                    else
                    {
                        gem_syslog(LOG_DEBUG, "Kill sent to overdue process: PID = %d allowed"
                                   " timeout %d, elapsed %d",
                                   ee->pid, ee->kill_in_secs, (ticks_end - ee->ticks_start) / ticks_pers_sec);
                    }
                }
            }
        }
        else if (ee->pid == -1)
        {
            if ((ee->trigger_flag) || ((ee->kill_in_secs == -1) && (ee->required_state == TRUE)))
            {
                gem_syslog(LOG_DEBUG, "Starting process %d", i);
                pid = gem_start_process(ee, i, rdb_root, &is_limit, &optimized);

                // clear trigger flag so we do not keep trying if start_process failed.
                ee->trigger_flag = FALSE;

                // reset the trigger flag as we successfully started the process.
                if (pid > 0)
                {
                    // note the time when this occurred
                    ee->ticks_start = getTickCount();
                    ee->pid = pid;
                }
                else
                {
                    if (is_limit)
                    {
                        gem_syslog(LOG_ERR, "Process limit reached %d", i);
                    }
                    else
                    {
                        ee->err_count++; // will eventually overflow, but this is Ok
                        if (!optimized)
                        {
                            // only log when not optimized
                            gem_syslog(LOG_ERR, "Failed to start process %d", i);
                        }
                    }
                }
            }
        }
    }
    return 0;
}

//
// Print EE table
//
void gem_print_ee_table(void)
{
    int i;
    gem_exec_entry *ee;
    gem_syslog(LOG_DEBUG, "EE table size is %d", gem_ee_table.count);
    for (i = 0 ; i < gem_ee_table.count ; i++)
    {
        ee = &gem_ee_table.ee[i];
        gem_syslog(LOG_DEBUG, "Entry %d, type %d, restartable %d, name %s, timeout %d, executable %s",
                   i, ee->type, ee->restart, ee->name, ee->kill_in_secs, ee->in_file ? ee->file_exe : ee->rdb_var_exe);
    }
}

//
// Init an EE passed to this function
// Poor man's constructor
//
void gem_init_ee(gem_exec_entry *ee)
{

    // structure is defined in such a way that we can set all fields to 0
    memset(ee, 0, sizeof(*ee));
    ee->kill_in_secs = GEM_DEFAULT_TIMEOUT; // default to compile-time setting
    ee->pid = -1;		// invalidate pid meaning ee is not running
    // ee->crc32 = 0; already set by memset
}

//
// Init all EEs
//
void gem_init_all_ees(void)
{
    int i;
    for (i = 0 ; i < GEM_MAX_EXEC_ENTRIES ; i++)
    {
        gem_init_ee(&gem_ee_table.ee[i]);
    }
    gem_ee_table.count = 0;
}

//
// Delete EE and memory allocated
//
void gem_delete_ee(gem_exec_entry *ee)
{
    free (ee->name);
    free (ee->file_exe);
    free (ee->rdb_var_exe);

    gem_init_ee(ee);
}

//
// Delete all EE's
//
void gem_delete_all_ees(void)
{
    int i;
    for (i = 0 ; i < GEM_MAX_EXEC_ENTRIES ; i++)
    {
        gem_delete_ee(&gem_ee_table.ee[i]);
    }
    gem_ee_table.count = 0;
}

//
// Terminate all EEs
//
void gem_terminate_all_ees(BOOL wait_for_child_exit)
{
    int i, pid = -1;
    gem_exec_entry *ee;
    int status;
    for (i = 0 ; i < gem_ee_table.count ; i++)
    {
        ee = &gem_ee_table.ee[i];

        if (ee->pid > 0)
        {
            // try to kill all running child processes
            kill(ee->pid, SIGKILL);

            //
            // wait until child exits. This is dangerous in a sense that
            // parent may hang if child refuses to exit so may be we could
            // do it slighly better here
            //
            if (wait_for_child_exit)
            {
                pid = waitpid(ee->pid, &status, 0);
                if ((WIFEXITED(status) || WIFSIGNALED(status)) && (pid == ee->pid))
                {
                    ee->pid = -1;
                    gem_syslog(LOG_DEBUG, "Child process killed by terminate all EEs function,"
                               " PID = %d, status %d",
                               pid, status);
                }
            }
            else
            {
                //
                // do not set the pid to -1. We will have exit of child
                // processed in gem_process_ees() in a streamline fashion.
                // Otherwise, we will leave zombie processes
                //
                gem_syslog(LOG_DEBUG, "Kill signal sent to child by terminate all EEs function,"
                           " PID = %d, status %d",
                           ee->pid, status);
            }

            gem_stats_add_record(i, GEM_STATS_KILLED_ALL, NULL);
        }

        // reset the trigger flag. In this case it is unconditional on restart flag.
        if (ee->trigger_flag)
        {
            ee->trigger_flag = 0;
        }
    }
}

//
// Add an EE to the EE table
//
// Return 0 on success, -1 otherwise
//
int gem_add_ee(gem_exec_entry *ee)
{
    if (gem_ee_table.count < GEM_MAX_EXEC_ENTRIES)
    {
        memcpy(&gem_ee_table.ee[gem_ee_table.count], ee, sizeof(*ee));
        gem_ee_table.count++;
        return 0;
    }
    return -1;
}

//
// Load an EE based on the record in RDB
//
// Return 0 on success, -1 otherwise
//
int gem_load_ee(char *rdb_root, int ee_count, gem_exec_entry *ee)
{
    int len;
    char rdb_var[GEM_MAX_RDB_NAME_LENGTH], rdb_val[GEM_MAX_RDB_VAL_LENGTH];
    int re_read_mode = 0;

    gem_init_ee(ee);

    sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_EE_TYPE_VAR);
    if (rdb_get_int(g_rdb_session, rdb_var, &ee->type) == 0)
    {
        if ((ee->type <= GEM_EXEC_INVALID) || (ee->type >= GEM_EXEC_MAX))
        {
            gem_syslog(LOG_ERR, "Invalid EE Type %s", rdb_var);
            return -1;
        }
    }
    else
    {
        // no problem - simply no more EEs
        return 1;
    }

    // this is optional, by default the EE is not restartable
    ee->restart = 0;
    sprintf(rdb_var, "%s.%d.%s", rdb_root, ee_count, GEM_RESTART_VAR);
    rdb_get_int(g_rdb_session, rdb_var, &ee->restart);

    sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_TIMEOUT_VAR);
    if (rdb_get_int(g_rdb_session, rdb_var, &ee->kill_in_secs) == 0)
    {
        if ((ee->kill_in_secs == 0) || (ee->kill_in_secs > GEM_MAX_TIMEOUT))
        {
            ee->kill_in_secs = GEM_DEFAULT_TIMEOUT;
        }
        else if (ee->kill_in_secs == -1) // use the EH as process monitor. Never times out
        {
            gem_syslog(LOG_DEBUG, "Will use EE %d as a process monitor", ee_count);
        }
    }
    else
    {
        ee->kill_in_secs = GEM_DEFAULT_TIMEOUT;
    }

    sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_NAME_VAR);
    len = sizeof(rdb_val);
    if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
    {
        // read name
        ee->name = malloc(strlen(rdb_val)+1);
        strcpy(ee->name, rdb_val);
    }
    else
    {
        gem_syslog(LOG_ERR, "No name given for EE %d", ee_count);
        return -1;
    }

    sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_FILENAME_VAR);
    len = sizeof(rdb_val);
    if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
    {
        ee->in_file = TRUE;
        ee->file_exe = malloc(strlen(rdb_val)+1);
        ee->rdb_var_exe = NULL;
        strcpy(ee->file_exe, rdb_val);
    }
    else
    {
        sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_SNIPPET_VAR);
        len = sizeof(rdb_val);
        if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
        {
            ee->rdb_var_exe = malloc(strlen(rdb_val)+1);
            ee->file_exe = NULL;
            strcpy(ee->rdb_var_exe, rdb_val);

            // if CRC32 is provided, read it
            sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_CRC32_VAR);
            len = sizeof(rdb_val);
            if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
            {
                ee->crc32 = atol(rdb_val);
            }

            // see if re-read mode is turned on.
            sprintf(rdb_var, "%s.%d.%s",rdb_root, ee_count, GEM_REREAD_VAR);
            if (rdb_get_int(g_rdb_session, rdb_var, &re_read_mode) == 0)
            {
                ee->reread_mode = re_read_mode;
            }
        }
        else
        {
            gem_syslog(LOG_ERR, "No executable file or snippet for EE %d", ee_count);
            // de-allocate memory that may have been allocated
            gem_delete_ee(ee);
            return -1;
        }
    }
    return 0;
}
