//
// Part of GEM daemon
// Code that handles RDB triggers
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/inotify.h>
#include <syslog.h>

#ifdef V_HOST
#include "../../cdcs_libs/mock_rdb_lib/rdb_operations.h"
#else
#include "rdb_ops.h"
#endif

#include "gem_api.h"
#include "gem_daemon.h"

//
// Based on compare function, expected value and the actual value, return TRUE or FALSE
// To be extended for more advanced Boolean logic.
//
static BOOL gem_is_condition_true(int compare_func, char *expected_value, char *compare_value)
{
    BOOL ret = FALSE;
    switch (compare_func)
    {
    case GEM_COMPARE_ALWAYS_TRUE:
        ret = TRUE;
        break;

    case GEM_COMPARE_EQ:
        ret = (strcmp(expected_value, compare_value) == 0) ? TRUE : FALSE;
        break;

    case GEM_COMPARE_NEQ:
        ret = (strcmp(expected_value, compare_value) != 0) ? TRUE : FALSE;
        break;

        // @todo - not supported. Do we want to support this on strings?
#if 0
    case GEM_COMPARE_G;
        break;

    case GEM_COMPARE_GEQ:
        break;

    case GEM_COMPARE_L:
        // @todo - not supported. Do we want to support this on strings?
        break;

    case GEM_COMPARE_LEQ:
        // @todo - not supported. Do we want to support this on strings?
        break;
#endif

    case GEM_COMPARE_MAX:
    default:
        gem_syslog(LOG_ERR, "Invalid compare condition %d (value %s)", compare_func,
                   compare_value);
        break;
    }
    return ret;
}

//
// Process RDB trigger. Called when a notification is received from the RDB driver
// trigger_name is the name of RDB variable for which we are subscribed and which has changed
//
static int process_rdb_trigger(char *trigger_name)
{
    int i, ee_num, count = 0, len;
    char buf[GEM_MAX_RDB_VAL_LENGTH];

    buf[0] = 0;

    if ((trigger_name == NULL) || (trigger_name[0] == 0))
    {
        gem_syslog(LOG_ERR, "Process RDB trigger name empty");
        return -1;
    }

    gem_syslog(LOG_DEBUG, "Processing RDB trigger %s", trigger_name);
    gem_stats_add_record(-1, GEM_STATS_RDB_TRIGGER, trigger_name);

    for (i = 0; i < rdb_triggers.count ; i++)
    {
        if (strcmp(rdb_triggers.table[i].name, trigger_name)==0)
        {
            // clear RDB variable's trigger flag by reading it
            len = sizeof(buf);
            rdb_get(g_rdb_session, trigger_name, buf, &len);

            //
            // for compare triggers, evaluate if the input condition is true
            // for other trigger types, we only look fo the fact that
            // the RDB variable has changed
            //
            if (rdb_triggers.table[i].compare_function != GEM_COMPARE_ALWAYS_TRUE)
            {
                if (!gem_is_condition_true(rdb_triggers.table[i].compare_function,
                                           rdb_triggers.table[i].compare_value, buf))
                {
                    gem_syslog(LOG_DEBUG, "Trigger on compare variable %s, "
                               "but compare condition is FALSE", trigger_name);
                    continue;
                }
            }

            ee_num = rdb_triggers.table[i].ee_num;
            if ((ee_num != -1) && (ee_num < gem_ee_table.count))
            {
                if (gem_ee_table.ee[ee_num].kill_in_secs == -1)// self restarting process
                {
                    //
                    // potentially, we could have this handled for other than RDB
                    // trigger types. For now, only RDB triggers can be used with
                    // process monitors. The way it work now, is simple:
                    // If the RDB variable is empty, the process should
                    // be stopped. Otherwise, it should be running
                    //
                    gem_ee_table.ee[ee_num].required_state = (buf[0] == 0) ? FALSE : TRUE;
                }
                else
                {
                    gem_ee_table.ee[ee_num].trigger_flag = TRUE; // trigger this EE
                }
                count++;
            }
            else
            {
                gem_syslog(LOG_ERR, "Invalid EE reference in RDB trigger table");
            }
        }
        else
        {
            // @todo - we have received a notification about something that is
            // NOT in trigger table. This must be unsubscribed from
            // (if only this was available through the driver!)
            //
            // @todo - when/if unsubscribe becomes available
            //  rdb_unsubscribe(trigger_name);
        }
    }
    return count; // number of EEs triggered
}

//
// Top level function called from GEM's control loop.
// Process RDB triggers.
// Called if select on rdb has returned >0
//
// Gets a list of TRIGGERED variables if read from RDB fd results in
// a non-zero return.
//
// At that point, the list will contain all TRIGGERED variables,
// potentially even not subscribed to by this process (tbc).
//
// Either way, it breaks the list into individual variables, and then calls the
// process_rdb_trigger with each individual rdb variable name to
// trigger EEs if the rdb trigger has a matching trigger in the trigger table.
//
// The other job of this function is to always check for reload variable change.
//
// Return:
// >0 - new trigger detected
// -1 - something bad
// 0 - no new signals detected
//
int gem_process_triggers_rdb(char *rdb_root)
{
    int i, buf_len;
    char *name; // shorthand
    char *name_buf;
    char rdb_reload_var[GEM_MAX_RDB_NAME_LENGTH];
    BOOL do_reload = FALSE;

    sprintf(rdb_reload_var, "%s.%s",rdb_root, GEM_RELOAD_RDB_VAR);

    name_buf = malloc(buf_len = GEM_MALLOC_SIZE);

    if (name_buf && (rdb_getnames(g_rdb_session, "", name_buf, &buf_len, TRIGGERED) == 0))
    {
        gem_syslog(LOG_DEBUG, "Triggered variable names %s", name_buf);
        if (buf_len > 0)
        {
            name = name_buf; // start of the buffer
            for (i = 0 ; i < buf_len ; i++)
            {
                if (name_buf[i] == SEPARATOR_CHAR)
                {
                    // spoil the buffer but we don't care
                    name_buf[i] = 0;
                    // Process all triggers, and mark reload for later if it is there.
                    // We would like to finish all trigger processing before the reload
                    if (strcmp(name, rdb_reload_var) == 0)
                    {
                        do_reload = TRUE;
                    }
                    else
                    {
                        process_rdb_trigger(name);
                    }
                    // increment pointer using pointer arithmetics
                    name += &name_buf[i]-name+1;
                }
            }
            // last (or possibly the one and only) rdb variable - there is no SEPARATOR CHAR after this one
            if (strcmp(name, rdb_reload_var) == 0)
            {
                do_reload = TRUE;
            }
            else
            {
                process_rdb_trigger(name);
            }
        }
    }
    free (name_buf);

    if (do_reload)
    {
        gem_handle_conf_change(rdb_root);
    }

    return 0;
}
