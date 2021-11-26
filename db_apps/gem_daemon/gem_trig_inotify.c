//
// Part of GEM daemon
// Code that handles inotify triggers
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include "fcntl.h"
#include <unistd.h>
#include <sys/times.h>
#include "gem_api.h"
#include "gem_daemon.h"

// inotify watches use these
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

extern gem_file_trigger_table file_triggers;

// inotify file descriptor
int inotify_fd = -1;

//
// Initialize inotify module.
//
// Return 0 on success, -1 otherwise
//
int gem_inotify_init(BOOL non_block)
{

    inotify_fd = inotify_init();

    if (inotify_fd < 0)
    {
        perror( "inotify_init" );
        return -1;
    }

    if (non_block)
    {
        // set inotify file descriptor to non-blocking
        int cntrl_flags = fcntl(inotify_fd, F_GETFL, 0);
        fcntl(inotify_fd, F_SETFL, cntrl_flags | O_NONBLOCK);
    }

    return 0;
}

//
// Add an inotify watch
//
// Return watch descriptor on success, -1 otherwise
//
int gem_inotify_add_watch(char *dir_or_file, int flags)
{
    if (inotify_fd >= 0)
    {
        return inotify_add_watch( inotify_fd, dir_or_file, flags);
    }
    return -1;
}

//
// Close inotify module and set file descriptor to -1
//
void gem_inotify_close(void)
{
    //  ( void ) inotify_rm_watch( inotify_fd, inotify_wd );
    if (inotify_fd >= 0)
    {
        (void)close(inotify_fd);
    }
    inotify_fd = -1;
}

//
// Helper to get the timer fd
//
int gem_inotfiy_get_fd(void)
{
    return inotify_fd;
}

//
// Process inotify trigger. Called when an event is received from inotify's file descriptor
// All inotify triggers are stored in a table.
//
// A slighly confusing aspect of inotify is that if the change is directory related
// then event's length is 0, and this function will be called with trigger_name=NULL.
//
// Return number of triggers that have matched. 0 is perfectly valid simply meaning that
// we are watching the directory but not the file in it that has changed.
//
static int process_inotify_trigger(int watch_descriptor, int event_mask, char *trigger_name)
{
    int i, count = 0, ee_num;
    char combined_str[1024];

    gem_file_trigger_table_elem *tte;

    for (i = 0; i < file_triggers.count ; i++)
    {
        // shorthand to table element
        tte = &file_triggers.table[i];

        //
        // Look for a watch id match,
        // as well as trigger flags match. The second part is required because
        // We could be watching the same file for different conditions.
        // Note we are looking for any bit to match, not all of them
        //
        if ((watch_descriptor != tte->wd) || ((event_mask & tte->trigger_flags) == 0))
        {
            continue;
        }

        //
        // trigger_name may be empty if a change occurred on a watched directory itself.
        // If it is not empty, it should match the file name in the trigger table
        //
        if ((trigger_name) && strcmp(tte->watch_file_name, trigger_name) &&
            (strcmp(tte->watch_file_name, "*")))
        {
            continue;
        }

        // if we get here, we have a trigger match

        // do some logging
        gem_syslog(LOG_DEBUG, "Inotify trigger on %s %s", tte->watch_dir_name, tte->watch_file_name);

        sprintf(combined_str, "%s/%s", tte->watch_dir_name, tte->watch_file_name);
        gem_stats_add_record(-1, GEM_STATS_INOTIFY_TRIGGER, combined_str);

        ee_num = tte->ee_num;
        if ((ee_num != -1) && (ee_num < gem_ee_table.count))
        {
            // To date, we have not implemented process monitor type application
            // (e.g. gem acting as an arbitrator for processes
            // that are normally running at all times and restaring stopping themt)
            // with inotify triggers. It is implemented for RDB triggers only.
            // The reason for this is because it is unclear how
            // file triggers should affect the status of the process - for example,
            // should the process run if the file is present and die otherwise?
            // This causes an issue with iselect because it is not possible to subscribe to a file
            // unless it already exists. So, it is unclear what should happen if the process is not
            // to run on startup. However, some tips are left in #if 0 block for future reference.

#if 0
            if (gem_ee_table.ee[ee_num].kill_in_secs == -1)// self restarting process
            {
                gem_syslog(LOG_DEBUG, "File %s will be used for process monitor", trigger_name);
                // if monitoring for file, then trigger_name is non empty
                if ((event_mask & (IN_CREATE|IN_DELETE)) == (IN_CREATE|IN_DELETE)) // both bits are set
                {
                    // work out what to do by checking if the file exists or not
                    // @todo
                }
                else
                {
                    // only one of the CREATE or DELETE bits is set
                    gem_ee_table.ee[ee_num].required_state = (event_mask & IN_CREATE) ? TRUE : FALSE;
                    gem_ee_table.ee[ee_num].required_state = (event_mask & IN_DELETE) ? FALSE : TRUE;
                }
            }
            else
            {
#endif
                gem_ee_table.ee[ee_num].trigger_flag = TRUE; // that's it!
#if 0
            }
#endif
            // count matches in the trigger table. Should have at least one.
            count++;
        }
        else
        {
            gem_syslog(LOG_ERR, "Invalid EE reference in file trigger table");
        }
    }

    return count; // number of EEs triggered. 0 is perfectly ok.
}

//
// Top level function called from GEM's control loop.
// Detect and process inotify triggers.
// Always uses non-blocking read.
//
// Return:
// >0 - new signals detected
// -1 - something bad
// 0 - no new signals detected
//
int gem_process_triggers_inotify(char *rdb_root)
{
    int length, i = 0, ret = 0;
    char buffer[BUF_LEN];

    // this will be non-blocking since we initialized it in non-blocking mode
    length = read(inotify_fd, buffer, BUF_LEN);

    // check if there is anything of interest
    if (length < 0)
    {
        if ((length == -1) && (errno==EAGAIN))
        {
            // all good - nothing to read
            return 0;
        }
        gem_syslog(LOG_ERR, "Inotify read error");
        return -1;
    }

    //
    // if watching a directory, event->len is non-zero and the event->name is the file name changed
    // if watching a file, event->len is zero and event->name is meaningless
    //
    while (i < length)
    {
        struct inotify_event *event = (struct inotify_event *)&buffer[i];

        if (event->mask & IN_CREATE)
        {
            if (event->mask & IN_ISDIR)
            {
                //gem_syslog(LOG_DEBUG, "The directory %s was created.", event->name );
            }
            else
            {
                //gem_syslog(LOG_DEBUG, "Watched file was created.");
            }
        }
        else if (event->mask & IN_DELETE)
        {
            if (event->mask & IN_ISDIR)
            {
                //gem_syslog(LOG_DEBUG, "The directory %s was deleted.", event->name );
            }
            else
            {
                //gem_syslog(LOG_DEBUG, "Watched file was deleted.");
            }
        }
        else if (event->mask & IN_MODIFY)
        {
            if (event->mask & IN_ISDIR)
            {
                //gem_syslog(LOG_DEBUG, "The directory %s was modified.", event->name );
            }
            else
            {
                //gem_syslog(LOG_DEBUG, "Watched file was modified.");
            }
        }

        ret = process_inotify_trigger(event->wd, event->mask, event->len ? event->name : NULL);

        i += EVENT_SIZE + event->len;

    }
    return ret;
}

