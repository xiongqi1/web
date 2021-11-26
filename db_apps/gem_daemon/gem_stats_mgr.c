//
// Part of GEM daemon
// Statistics/performance manager
// Very early implementation.
// It is based on rolling log of N files, each containing up to M
// comma-separated text strings of predefined format
//
// Files will be created in the directory which is passed
// to this module through initialization function
//
// It is important that if multiple GEM daemons are used, that their
// log directories do not clash.
//
// At the moment, all records will be lost when a new GEM session starts
// This is probably not a bad thing, especially on an embedded target.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>

#ifdef V_HOST
#include "../../cdcs_libs/mock_rdb_lib/rdb_operations.h"
#else
#include "rdb_ops.h"
#endif

#include "gem_api.h"
#include "gem_daemon.h"

#include <stdio.h>
#include <string.h>

#define MAX_LINE_LENGTH             (250)       // max line length
#define MAX_LOG_FILE_NAME_LENGTH    (GEM_MAX_DIR_LENGTH + 32)

// change these to values suitable for release
#define MAX_LOG_FILE_SIZE           (10000)   // around 10K will be reasonable, small size is good for testing
#define MAX_NUM_LOG_FILES           (2)     // number of rolling logs

// file handle for log file currently being written to
FILE* log_fp = NULL;

static BOOL enabled = FALSE;

// directory for log files.
char log_file_directory[GEM_MAX_DIR_LENGTH];

// prefix for log files
static const char* prefix = "logs_";

// extension / postfix
static const char*  postfix = ".txt";

//
// Get current timestamp
//
static void get_time_stamp(char *time_buf, int str_length)
{
    time_t now;
    struct tm *ts;

    time(&now);
    ts = localtime(&now);
    strftime(time_buf, str_length, "%a %d %b %Y %H:%M:%S %Z", ts);
}

//
// Initialization function.
// Note that there could be multiple copies of GEM running, so
// it is important that they are given a different directory each.
//
// Before, proceeding, the function checks that a file can be created in this directory.
//
// Return 0 success, -1 otherwise
//
int gem_stats_init(char *dir)
{
    // just check that we can create a file in the given directory
    char file1[MAX_LOG_FILE_NAME_LENGTH];

    // initialize buffer
    log_file_directory[0] = 0;

    sprintf(file1, "%s/tmp.txt", dir);
    log_fp = fopen(file1, "a" );

    // all good, can open a test log file
    if (log_fp)
    {
        fclose(log_fp);
        strncpy(log_file_directory, dir, strlen(dir)+1);

        gem_syslog(LOG_INFO, "log directory will be %s", log_file_directory);

        enabled = TRUE;

        //unlink(file1);
        log_fp = NULL;
        return 0;
    }

    return -1;
}

// close the log file.
void gem_stats_close(void)
{
    if (log_fp)
    {
        fclose(log_fp);
    }

    enabled = FALSE;
}

//
// Formats one record in CSV format which becomes one line in a log file
//
// record format is CSV, with the following columns
// EE number (if relevant, or -1)
// PID
// name of the EE or, if not relevant, empty
// Record type (as per record type passed to this function)
// Extra information (text string)
// Time of the event
//
static void format_record(int ee_number, int record_type, char *extra_text, char *s)
{
    char event_str[32];
    char time_buf[128];

    switch (record_type)
    {
    case GEM_STATS_EXECUTED:
        strcpy(event_str, "Executed");
        break;
    case GEM_STATS_KILLED:
        strcpy(event_str, "Killed");
        break;
    case GEM_STATS_EXITED:
        strcpy(event_str, "Exited");
        break;
    case GEM_STATS_KILLED_ALL:
        strcpy(event_str, "Killed all");
        break;
    case GEM_STATS_INOTIFY_TRIGGER:
        strcpy(event_str, "Inotify trigger");
        break;
    case GEM_STATS_RDB_TRIGGER:
        strcpy(event_str, "RDB trigger");
        break;
    case GEM_STATS_RELOAD:
        strcpy(event_str, "Reload");
        break;
    default:
        strcpy(event_str, "UNKNOWN EVENT");
        break;
    }

    get_time_stamp(time_buf, sizeof(time_buf));
    if ((ee_number >= 0) && (ee_number < gem_ee_table.count))
    {
        sprintf(s, "EE %d,%d,%s,%s,%s,%s", ee_number, gem_ee_table.ee[ee_number].pid,
                gem_ee_table.ee[ee_number].name, event_str, "", time_buf);
    }
    else
    {
        sprintf(s, "EE %d,%d,%s,%s,%s,%s", ee_number, -1, "", event_str,
                extra_text ? extra_text : "", time_buf);
    }
}

//
// Add one record to the log file. The record can be EE based, in which case EE is
// valid, or not related to a particular EE (for example, it could log a
// trigger event
//
int gem_stats_add_record(int ee_number, int record_type, char *extra_text)
{
    char line[MAX_LINE_LENGTH];
    char file1[MAX_LOG_FILE_NAME_LENGTH], file2[MAX_LOG_FILE_NAME_LENGTH]; // 2 filenames
    int i;

    // logging is not enabled
    if (!enabled)
    {
        return -1;
    }

    // format a record into "line" buffer
    format_record(ee_number, record_type, extra_text, line);

    // this will overwrite log files every session - do we want this?
    if (!log_fp)
    {
        sprintf(file1, "%s/%s%d%s", log_file_directory, prefix, 0, postfix );
        log_fp = fopen( file1, "a" );
    }

    if (log_fp)
    {
        if (ftell(log_fp) > MAX_LOG_FILE_SIZE)
        {
            fclose(log_fp);
            log_fp = 0;

            for (i = (MAX_NUM_LOG_FILES - 1); i >= 0; i--)
            {
                sprintf(file1, "%s/%s%d%s", log_file_directory, prefix, i, postfix);
                sprintf(file2, "%s/%s%d%s", log_file_directory,  prefix, i+1, postfix);
                rename( file1, file2 );
            }

            sprintf(file1, "%s/%s%d%s", log_file_directory, prefix, 0, postfix);
            log_fp = fopen(file1, "a");
        }

        fputs(line, log_fp);
        fputs("\n", log_fp);
        fflush( log_fp );
    }

    return (0);
}

