// Part of GEM daemon
// Main control loop code and supporting routines

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include <sys/timerfd.h>
#include <sys/inotify.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/times.h>

// @todo - this will be a relative path depending on where
#ifdef V_HOST
#include "../../cdcs_libs/mock_rdb_lib/rdb_operations.h"
#else
#include "rdb_ops.h"
#endif

#include "gem_api.h"
#include "gem_daemon.h"

// a character that kernel RDB driver uses as a separator when it returns
// a list of RDB variables via rdb_get_names
#define SEPARATOR_CHAR '&'

// a character that separates a list of trigger RDB variables when it is
// stored in RDB as a list trigger.
// Could use any character, but the same character as above will do
#define RDB_LIST_SEPARATOR_CHAR SEPARATOR_CHAR

#define GEM_SLEEP_TIME_US   (100000L)

extern char temp_file_directory[GEM_MAX_DIR_LENGTH];

// the one and only RDB session handle
struct rdb_session *g_rdb_session;

// state of the daemon
int gem_daemon_state = GEM_DAEMON_NORMAL;

// global variables defining triggers
gem_file_trigger_table file_triggers;
gem_rdb_trigger_table rdb_triggers;
gem_timer_trigger_table timer_triggers;

//
// set all values in trigger tables to default values
// Note : no memory is de-allocated/allocated using this function
//
static void init_trigger_tables(void)
{
    int i;

    // file table
    file_triggers.count = 0;
    for (i = 0; i < GEM_MAX_FILE_TRIGGER_TABLE_SIZE ; i++)
    {
        file_triggers.table[i].watch_dir_name = NULL;
        file_triggers.table[i].watch_file_name = NULL;
        file_triggers.table[i].wd = 0;
        file_triggers.table[i].ee_num = -1;
        file_triggers.table[i].trigger_flags = 0;
    }

    // rdb table
    rdb_triggers.count = 0;
    for (i = 0; i < GEM_MAX_RDB_TRIGGER_TABLE_SIZE ; i++)
    {
        rdb_triggers.table[i].name = NULL;
        rdb_triggers.table[i].compare_function = GEM_COMPARE_ALWAYS_TRUE;
        rdb_triggers.table[i].compare_value = NULL;
        rdb_triggers.table[i].ee_num = -1;
    }

    // timer table
    timer_triggers.count = 0;
    for (i = 0; i < GEM_MAX_TIMER_TRIGGER_TABLE_SIZE ; i++)
    {
        timer_triggers.table[i].ee_num = -1;
    }
}

//
// De-allocate dynamic memory allocated for trigger tables
//
static void deallocate_trigger_tables(void)
{
    int i;

    // it is valid/safe to free (NULL)

    // de-allocate dynamically allocated memory first
    for (i = 0; i < GEM_MAX_FILE_TRIGGER_TABLE_SIZE ; i++)
    {
        free (file_triggers.table[i].watch_dir_name);
        free (file_triggers.table[i].watch_file_name);
    }

    for (i = 0; i < GEM_MAX_RDB_TRIGGER_TABLE_SIZE ; i++)
    {
        free (rdb_triggers.table[i].name);
        free (rdb_triggers.table[i].compare_value);
    }
    init_trigger_tables(); // set all pointers to NULL
}

//
// A helper function that subscribes for exactly one RDB variable
// notification. If the RDB variable does not exist, it is created
// first.
//
// Return 0 on success, -1 otherwise
//
static int subscribe_rdb_var(char *var)
{
    int ret = 0;
    if (rdb_subscribe(g_rdb_session, var) < 0)
    {
        // create a variable if it does not exist yet.
        if ((rdb_create_string(g_rdb_session, var, "", PERSIST|STATISTICS, DEFAULT_PERM) < 0)
                || (rdb_subscribe(g_rdb_session, var) < 0))
        {
            ret = -1;
        }
    }
    return ret;
}

//
// Add a single timer trigger to the Timer Trigger table.
//
static int add_timer_trig_table_elem(int period_sec, int ee_num)
{
    gem_timer_trigger_table *p_table = &timer_triggers;
    gem_timer_trigger_table_elem *tte;

    if (p_table->count >= GEM_MAX_TIMER_TRIGGER_TABLE_SIZE)
    {
        gem_syslog(LOG_ERR, "Timer trigger table full");
        return -1;
    }

    if (period_sec <= 0)
    {
        gem_syslog(LOG_ERR, "Invalid period for timer %d", period_sec);
        return -1;
    }

    tte = &p_table->table[p_table->count];  // point at next available element
    tte->timer_props.it_value.tv_sec = tte->timer_props.it_interval.tv_sec
                                       = period_sec;

    //
    // There is a possibility of using higher resolution timers if needed
    // If this was to occur, we would change the next line
    // For now, set to microseconds to 0.
    //
    tte->timer_props.it_value.tv_nsec = tte->timer_props.it_interval.tv_nsec = 0;
    tte->ee_num = ee_num;
    p_table->count++;
    return 0;
}

//
// Add a single file/inotify trigger to the File Trigger table.
// Passed to us in watchName is complete path to directory or file.
// We work out ourselves the directory part as we always watch the dir.
//
// Return 0 on success, -1 otherwise
//
static int add_file_trig_table_elem(char *watchName, int trigger_flags, int ee_num)
{
    int wd = -1, i;
    gem_file_trigger_table *p_table;
    gem_file_trigger_table_elem *tte;
    char *watch_dir_name, *watch_file_name;

    //  make a pointer to the table control for convenience
    p_table =  &file_triggers;

    // table full
    if (p_table->count >= GEM_MAX_FILE_TRIGGER_TABLE_SIZE)
    {
        gem_syslog(LOG_ERR, "File trigger table full");
        return -1;
    }

    // sanity check
    if (!watchName || !strlen(watchName))
    {
        gem_syslog(LOG_ERR, "Inotify watch name empty");
        return -1;
    }

    //
    // break watch name into a directory name and the file name
    // note the order is important as dirname() modifies watchName
    // So call basename() first
    //

    // get file name
    watch_file_name = basename(watchName);

    // get dir name
    watch_dir_name = dirname(watchName);

    // the first condition occurs if the watchName did not contain '/'
    if ((watch_dir_name[0] == '.') || (watch_dir_name == NULL) ||
            (watch_dir_name[0] == 0))
    {
        gem_syslog(LOG_ERR, "Inotify watch name invalid");
        return -1;
    }

    //
    // If directory is already watched, add another element with the
    // same watch descriptor, but do not ask inotify to subscribe again.
    // See if we are already watching this directory
    //
    for (i = 0 ; i < p_table->count ; i++)
    {
        if (strcmp(p_table->table[i].watch_dir_name, watch_dir_name) == 0)
        {
            // already has an entry in the table with this directory name
            wd = p_table->table[i].wd;
            break;
        }
    }

    if (wd == -1) // existing watch not found
    {
        // always subscribe for 3 events - create a new watch
        wd = gem_inotify_add_watch(watch_dir_name, IN_MODIFY | IN_CREATE | IN_DELETE);
    }

    // Wd should be positive if any of the above worked
    if (wd <= 0)
    {
        gem_syslog(LOG_ERR, "Inotify subscribe fail %s %d", watchName, trigger_flags);
        return -1;
    }

    // add new table entry
    tte = &p_table->table[p_table->count];  // point at next available element

    // fill it up
    tte->wd = wd;
    tte->watch_dir_name = malloc(strlen(watch_dir_name) + 1);
    strcpy(tte->watch_dir_name, watch_dir_name);

    tte->watch_file_name = malloc(strlen(watch_file_name) + 1);
    strcpy(tte->watch_file_name, watch_file_name);

    tte->trigger_flags = trigger_flags;
    tte->ee_num = ee_num;
    p_table->count++;

    return 0;
}

//
// Add a single RDB variable trigger to the RDB trigger table.
//
// *name contains the name of rdb variable
// compare_func - (optional) function to use for value comparison (EQ, NEQ)
// *compare_val - value to compare with
//
// Each entry in the RDB trigger table contains one (unique) RDB variable.
//
// Return 0 on success, -1 otherwise
//
int add_rdb_trig_table_elem(char *name, int compare_func, char *compare_val, int ee_num)
{
    int i;
    BOOL found = FALSE;
    gem_rdb_trigger_table *p_table;
    gem_rdb_trigger_table_elem *tte;

    //  make a pointer to the table control for convenience
    p_table =  &rdb_triggers;

    // check for table full condition
    if (p_table->count >= GEM_MAX_RDB_TRIGGER_TABLE_SIZE)
    {
        gem_syslog(LOG_ERR, "RDB trigger table full");
        return -1;
    }

    // see if there is already an RDB trigger with that name in it
    for (i = 0 ; i < p_table->count ; i++)
    {
        if (strcmp(p_table->table[i].name, name) == 0)
        {
            // already has an entry in the table with this trigger name
            found = TRUE;
            break;
        }
    }

    // just so we do not ask RDB driver to subscribe for a variable which is already subscribed (although this is probably Ok)
    if (!found)
    {
        if (subscribe_rdb_var(name) < 0)
        {
            gem_syslog(LOG_ERR, "RDB subscribe fail %s", name);
            return -1;
        }
    }

    // add new table entry
    tte = &p_table->table[p_table->count];  // point at next available element
    tte->name = malloc(strlen(name) + 1);
    strcpy(tte->name, name);

    // a bit more work for compare trigger
    tte->compare_function = compare_func;
    if (compare_func != GEM_COMPARE_ALWAYS_TRUE)
    {
        tte->compare_value = malloc(strlen(compare_val) + 1);
        strcpy(tte->compare_value, compare_val);
    }

    tte->ee_num = ee_num;
    p_table->count++;

    return 0;
}

//
// Add a number of RDB variable triggers to the RDB trigger table.
//
// *str contains the list of rdb variable names, separated by separator_char
// This function is called for:
// 1) Wildcard triggers
// 2) List triggers
//
// Note that "str" buffer will be spoled by this function.
//
// Return 0 on success, -1 otherwise
//
static int add_name_list(char *str, int buf_len, char separator_char, int ee_num)
{
    int i;

    //
    // str_pos is used in the loop to point at the beginning of
    // the substring just past the previous separator character
    //
    char *str_pos = str;
    for (i = 0 ; i < buf_len ; i++)
    {
        if (str[i] == separator_char)
        {
            str[i] = 0; // spoil buffer but we don't care
            if (add_rdb_trig_table_elem(str_pos, 0, NULL, ee_num) < 0)
            {
                gem_syslog(LOG_ERR, "Subscribe fail with wildcard/list trigger %s", str_pos);
                return -1;
            }
            str_pos += &str[i]-str_pos+1; // increment pointer using pointer arithmetics
        }
    }

    if (strlen(str_pos) && add_rdb_trig_table_elem(str_pos, 0, NULL, ee_num) < 0)
    {
        gem_syslog(LOG_ERR, "Subscribe fail with wildcard/list trigger %s", str_pos);
        return -1;
    }

    return 0;
}

//
// Add an indirect RDB trigger
// This means, that a part of GEM RDB root has a trigger_r_indirect
// RDB variable that is not subscribed for itself (unlike direct trigger).
// Instead, it can contain one of three things:
// 1) A name of any other RDB variable that should be used as a trigger
// 2) A wildcard, for example wild* means all existing RDB vars which match
// 3) A list of RDB variable triggers, for example "cow&bovine&falcon&goat"
//
// The caller will want to know if multiple triggers were found which
// is returned by reference in the last argument
//
// Return 0 on success, -1 otherwise
//
int add_trig_table_elem_indirect(char *name, int ee_num, BOOL *multiple_vars)
{
    char *name_buf;
    int buf_len;

    gem_syslog(LOG_INFO, "Trigger table elem indirect name %s", name);

    // check for wildcard character
    if ((name_buf = strchr(name, '*')) != NULL)
    {
        // get rid of wildcard which is always the last character in the string
        // wildcard used in indirect trigger
        *name_buf = 0;
        // just get a large chunk for names.
        name_buf = malloc(buf_len = GEM_MALLOC_SIZE);

        // don't care about flags, so set to 0
        if (name_buf && (rdb_getnames(g_rdb_session, name, name_buf, &buf_len, 0) == 0))
        {
            gem_syslog(LOG_DEBUG, "Return from get_rdb_names contains %s", name_buf);
            if (buf_len > 0)
            {
                if (add_name_list(name_buf, buf_len, SEPARATOR_CHAR, ee_num) == -1)
                {
                    gem_syslog(LOG_ERR, "Subscribe fail with wildcard trigger");
                }
            }
        }

        // don't need name_buf
        free (name_buf);

        *multiple_vars = TRUE;
        gem_syslog(LOG_DEBUG, "EE %d is wildcard, buf len %d", ee_num, buf_len);
        return (buf_len > 0) ? 0 : -1;
    }
    else if (strchr(name, RDB_LIST_SEPARATOR_CHAR) != NULL)
    {
        // the RDB variable, contains a list of trigger variables

        buf_len = strlen(name); // list length

        if (buf_len > 0)
        {
            if (add_name_list(name, buf_len, RDB_LIST_SEPARATOR_CHAR, ee_num) == -1)
            {
                gem_syslog(LOG_ERR, "Subscribe fail with list trigger");
            }
        }

        *multiple_vars = TRUE;
        return 0;
    }
    else
    {
        *multiple_vars = FALSE;

        // simple case - add one element
        return add_rdb_trig_table_elem(name, 0, NULL, ee_num);
    }
}

//
// Print list of file triggers
//
static int print_file_trigger_table(void)
{
    int i;

    gem_syslog(LOG_DEBUG, "File trigger table size is %d", file_triggers.count);
    for (i = 0 ; i < file_triggers.count ; i++)
    {
        gem_syslog(LOG_DEBUG, "Inotify trigger: dir name %s, filename %s, flags %d, ee num %d",
                   file_triggers.table[i].watch_dir_name,
                   file_triggers.table[i].watch_file_name ? file_triggers.table[i].watch_file_name : "",
                   file_triggers.table[i].trigger_flags, file_triggers.table[i].ee_num);
    }
    return 0;
}

//
// Print list of rdb triggers
//
static int print_rdb_trigger_table(void)
{
    int i;

    gem_syslog(LOG_DEBUG, "RDB trigger table size is %d", rdb_triggers.count);
    for (i = 0 ; i < rdb_triggers.count ; i++)
    {
        gem_syslog(LOG_DEBUG, "Trigger name %s, compare func %d, compare val %s, ee num %d",
                   rdb_triggers.table[i].name, rdb_triggers.table[i].compare_function,
                   rdb_triggers.table[i].compare_value ? rdb_triggers.table[i].compare_value : "",
                   rdb_triggers.table[i].ee_num);
    }
    return 0;
}

//
// Print list of timer triggers
//
static int print_timer_trigger_table(void)
{
    int i;
    gem_timer_trigger_table_elem *tte;

    gem_syslog(LOG_DEBUG, "Timer trigger table size is %d", timer_triggers.count);
    for (i = 0 ; i < timer_triggers.count ; i++)
    {
        tte = &timer_triggers.table[i];

        gem_syslog(LOG_DEBUG, "Elem %d, value sec=%ld, interval sec=%ld,"
                   " value usec=%ld, interval usec=%ld, ee number=%d",
                   i, tte->timer_props.it_value.tv_sec, tte->timer_props.it_interval.tv_sec,
                   tte->timer_props.it_value.tv_nsec, tte->timer_props.it_interval.tv_nsec,
                   tte->ee_num);

    }
    return 0;
}


//
// A helper function to set one interger rdb variable
//
static int set_single_int(char *rdb_var, int int_val)
{
    char rdb_val[GEM_MAX_RDB_VAL_LENGTH];
    sprintf(rdb_val, "%d", int_val);
    return rdb_set_string(g_rdb_session, rdb_var, rdb_val);
}

//
// A dispatcher function that works out all types of triggers (file, rdb, timers).
// and loads them
// It reads the trigger type RDB variable for a particular EE, works out the type
// and then looks for more entries needed for that particular trigger type.
//
// Return 0 on success, -1 otherwise
static int load_triggers(char *rdb_root, int count, char *template_file, int in_file,
                         BOOL *required_state, BOOL *rdb_trigger, BOOL *multiple_vars)
{
    int len;
    int trigger_type;
    char rdb_var[GEM_MAX_RDB_NAME_LENGTH], rdb_val[GEM_MAX_RDB_VAL_LENGTH],
        rdb_val2[GEM_MAX_RDB_VAL_LENGTH], compare_str[16];

    int compare_func;
    int rdb_int;

    // set BOOLeans we return by reference to default
    *required_state = FALSE;
    *rdb_trigger = FALSE;
    *multiple_vars = FALSE;

    //
    // Firstly, work out the trigger type.
    //
    sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_TRIGGER_TYPE_VAR);
    if (rdb_get_int(g_rdb_session, rdb_var, &trigger_type) == 0)
    {
        if ((trigger_type <= GEM_TRIGGER_INVALID) || (trigger_type >= GEM_TRIGGER_MAX))
        {
            gem_syslog(LOG_ERR, "Invalid Trigger Type %s", rdb_var);
            return -1;
        }
    }

    switch (trigger_type)
    {
    case GEM_TRIGGER_DIRECT_RDB:
        sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_RDB_TRIGGER_DIRECT_VAR);
        len = sizeof(rdb_val);
        if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
        {
            //
            // found direct trigger. Note we are passing the variable name,
            // not the value to add_trigger
            //
            if (add_rdb_trig_table_elem(rdb_var, 0, NULL, count) < 0)
            {
                gem_syslog(LOG_ERR, "Could not add direct trigger element");
                return -1;
            }

            //
            // When used in process monitor way, the fact that the
            // rdb variable is non-empty, means the process should be running
            //
            *required_state = (rdb_val[0] != 0);
        }
        else
        {
            gem_syslog(LOG_ERR, "Could not find direct trigger var");
            return -1;
        }
        *rdb_trigger = TRUE;
        break;

    case GEM_TRIGGER_COMPARE_RDB:
        sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_RDB_TRIGGER_COMPARE_VAR);
        len = sizeof(rdb_val);
        if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
        {
            sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_RDB_TRIGGER_VALUE_VAR);
            len = sizeof(rdb_val2);
            if (rdb_get(g_rdb_session, rdb_var, rdb_val2, &len) != 0)
            {
                gem_syslog(LOG_ERR, "Compare trigger value variable missing");
                return -1;
            }

            sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_RDB_TRIGGER_COMPARE_FUNC_VAR);
            len = sizeof(compare_str);
            if (rdb_get(g_rdb_session, rdb_var, compare_str, &len) != 0)
            {
                gem_syslog(LOG_ERR, "Compare trigger function incorrect/missing");
                return -1;
            }

            // this will be rewritten if more than EQ and NEQ are introduced
            if (strcmp(compare_str, GEM_RDB_TRIGGER_COMPARE_EQ_STR) == 0)
            {
                compare_func = GEM_COMPARE_EQ;
            }
            else if (strcmp(compare_str, GEM_RDB_TRIGGER_COMPARE_NEQ_STR) == 0)
            {
                compare_func = GEM_COMPARE_NEQ;
            }
            else
            {
                compare_func = GEM_COMPARE_ALWAYS_TRUE;
            }

            // compare trigger. Not only the RDB var has to change, but also become
            // equal/not-equal to a certain value
            // Compare trigger is also indirect in a sense that trigger_r_compare
            // RDB variable contains a name of another variable that will be
            // used as a trigger. This is more flexible.
            if (add_rdb_trig_table_elem(rdb_val, compare_func, rdb_val2, count) < 0)
            {
                gem_syslog(LOG_ERR, "Could not add compare trigger element");
                return -1;
            }
        }
        else
        {
            gem_syslog(LOG_ERR, "Could not find compare trigger var");
            return -1;
        }
        *rdb_trigger = TRUE;
        break;

    case GEM_TRIGGER_INDIRECT_RDB:
        sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_RDB_TRIGGER_REDIRECT_VAR);
        len = sizeof(rdb_val);
        if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
        {
            //
            // redirect trigger, the "trigger_redirect" rdb variable contain
            // the name of ANOTHER rdb variable that is used to subscribed to
            //

            // the first argument is NOT a bug
            if (add_trig_table_elem_indirect(rdb_val, count, multiple_vars) < 0)
            {
                gem_syslog(LOG_ERR, "Could not add indirect trigger element");
                return -1;
            }

            //
            // some complications for process monitor-type EEs
            // Can only use as a process monitor if there was no multiple
            // variable triggers (as it is ambiguous)
            //
            len = sizeof(rdb_val2);
            if ((*multiple_vars == FALSE) && rdb_get(g_rdb_session, rdb_val, rdb_val2, &len) == 0)
            {
                // rdb variable non-empty, means the process should be running
                *required_state = (rdb_val2[0] != 0);
                gem_syslog(LOG_DEBUG, "Required state of rdb variable %s is %d",
                           rdb_val, rdb_val2[0]!=0 ? 1 : 0);
            }
        }
        else
        {
            gem_syslog(LOG_ERR, "Could not find indirect trigger var");
            return -1;
        }
        *rdb_trigger = TRUE;
        break;

    case GEM_TRIGGER_FILE:
        sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_INOTIFY_TRIGGER_WATCH);
        len = sizeof(rdb_val);
        if (rdb_get(g_rdb_session, rdb_var, rdb_val, &len) == 0)
        {
            sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_INOTIFY_TRIGGER_FLAGS);

            len = sizeof(rdb_val2);
            if (rdb_get(g_rdb_session, rdb_var, rdb_val2, &len) == 0)
            {
                rdb_int = 0;
                // analyze characters and form a bitmap
                if (strchr(rdb_val2, 'C'))
                {
                    rdb_int |= IN_CREATE;
                }

                if (strchr(rdb_val2, 'D'))
                {
                    rdb_int |= IN_DELETE;
                }

                if (strchr(rdb_val2, 'M'))
                {
                    rdb_int |= IN_MODIFY;
                }

                // must have at least one bit set out of 3 that we are watching
                if (rdb_int == 0)
                {
                    gem_syslog(LOG_ERR, "Could not find valid trigger flags var");
                    return -1;
                }

                if (add_file_trig_table_elem(rdb_val, rdb_int, count) < 0)
                {
                    // this can happen if watched file/dir does not exist yet
                    gem_syslog(LOG_ERR, "Could not add file trigger element");
                }
            }
            else
            {
                gem_syslog(LOG_ERR, "Could not find valid trigger flags var");
                return -1;
            }
        }
        else
        {
            gem_syslog(LOG_ERR, "Could not find file trigger flag var");
            return -1;
        }
        break;

    case GEM_TRIGGER_TIMER:
        // timer trigger. Very simple - only one RDB variable is required
        sprintf(rdb_var, "%s.%d.%s",rdb_root, count, GEM_TIMER_TRIGGER_VAR);
        if (rdb_get_int(g_rdb_session, rdb_var, &rdb_int) == 0)
        {
            if (add_timer_trig_table_elem(rdb_int, count) < 0)
            {
                gem_syslog(LOG_ERR, "Could not add file trigger element");
                return -1;
            }
        }
        else
        {
            gem_syslog(LOG_ERR, "Could not find timer trigger var");
            return -1;
        }
        break;

    case GEM_TRIGGER_TEMPLATE:
        // template trigger
        if (!template_file || !in_file)
        {
            gem_syslog(LOG_ERR, "Template trigger - no file");
            return -1;
        }

        if (!strstr(template_file, TEMPLATELIST_FILE_EXT))
        {
            gem_syslog(LOG_ERR, "Template trigger - file name incorrect");
            return -1;
        }

        if (gem_templatemgr_parse_template(template_file, NULL, count) < 0)
        {
            gem_syslog(LOG_ERR, "Template manager parse - failed");
            return -1;
        }

        break;

        //case GEM_TRIGGER_IO:
    default:
        gem_syslog(LOG_ERR, "Unsupported trigger type %d", trigger_type);
        return -1;

    }

    return 0;
}

//
// De-allocate all memory
// Unsubscribe from notifications where possible
// Exit the daemon.
//
static void cleanup_and_exit(void)
{
    // terminate all EEs that may be running. This will block until all children are killed
    //
    // same as sending SIGTERM - the loop will exit, so will the daemon,
    // having cleaned up correctly
    //
    gem_sig_term = TRUE;
}

//
// Does complete configuration re-load from a specified rdb root.
// All existing EEs and triggers are unloaded, and new ones are
// loaded.
//
// This takes place when reload variable e.g. RDB_ROOT.reload is
// set to 2.
//
// GEM daeamon always subscribes to the reload variable.
//
// Return 0 on success, -1 otherwise
static int do_reload(char *rdb_root)
{
    static int debug_cnt = 0;
    int ee_count;
    gem_exec_entry ee;
    BOOL is_rdb_trigger, multiple_vars;

    // restart Inotify
    gem_inotify_close();
    gem_inotify_init(FALSE);

    // terminate all EEs that may be running. This will block until all children are killed
    gem_terminate_all_ees(TRUE);

    // delete all trigger tables
    deallocate_trigger_tables();

    // delete all EEs
    gem_delete_all_ees();

    gem_syslog(LOG_DEBUG, "Reloading %d", debug_cnt++);

    // loop for each EE found in RDB, which needs to do basically, 2 things for each entry
    // 1) Load triggers
    // 2) Load EEs
    for (ee_count = 0 ; ee_count < GEM_MAX_EXEC_ENTRIES ; ee_count++)
    {
        // initialize all ee fields every time in the loop
        int ret = gem_load_ee(rdb_root, ee_count, &ee);
        if (ret == 1)
        {
            // meaning there are no more EEs
            break;
        }
        else if (ret == -1)
        {
            // error
            gem_syslog(LOG_ERR, "Load EEs failed EE num=%d", ee_count);
            return -1;
        }

        //
        // Deal with trigger types
        // to complicate things, some triggers may be in template files,
        // hence we pass name of this file.
        //
        if (load_triggers(rdb_root, ee_count, ee.file_exe, ee.in_file, &ee.required_state,
                          &is_rdb_trigger, &multiple_vars) < 0)
        {
            gem_syslog(LOG_DEBUG, "Load triggers failed, EE num=%d", ee_count);
            // de-allocate memory that may have been allocated by LoadEEs
            gem_delete_ee(&ee);
            return -1;
        }

        //
        // At the moment, we can only use rdb triggers for process monitor type applications,
        // so do a simple error check.
        // Also, we cannot use wildcards for process monitor type applications,
        // as it is unclear and ambiguous on which conditions should these be executed
        //
        if ((!is_rdb_trigger) || (multiple_vars))
        {
            if (ee.kill_in_secs == -1)
            {
                ee.kill_in_secs = GEM_DEFAULT_TIMEOUT;
            }
        }

        // self-restarting processes only supported when kill_in_secs is set to -1
        if (ee.kill_in_secs != -1)
        {
            ee.required_state = FALSE;
        }

        // add EE to EE table
        if (gem_add_ee(&ee)<0)
        {
            gem_syslog(LOG_ERR, "Failed to add EE");
            // de-allocate memory that may have been allocated
            gem_delete_ee(&ee);
            return -1;
        }
        else
        {
            gem_syslog(LOG_DEBUG, "Added EE %d", ee_count);
        }
    }

    // log some debug info
    gem_syslog(LOG_DEBUG, "Found %d valid EEs", ee_count);

    // in debug mode, print trigger tables
    (void)print_file_trigger_table();
    (void)print_rdb_trigger_table();
    (void)print_timer_trigger_table();

    // .. and the EE table
    gem_print_ee_table();

    return 0;
}

// reload all EEs based on the status of EE root
// Return 0 on success, -1 otherwise
int gem_handle_conf_change(char *rdb_root)
{
    char rdb_var[GEM_MAX_RDB_NAME_LENGTH];

    int ret = 0;

    // read the reload variable
    sprintf(rdb_var, "%s.%s",rdb_root, GEM_RELOAD_RDB_VAR);

    if (rdb_get_int(g_rdb_session, rdb_var, &gem_daemon_state) < 0)
    {
        gem_syslog(LOG_ERR, "reload value does not exist");
        return -1;
    }

    gem_syslog(LOG_DEBUG, "Reload value %d", gem_daemon_state);

    gem_stats_add_record(-1, GEM_STATS_RELOAD, "reload variable changed");


    // check the value of the reload variable
    switch (gem_daemon_state)
    {

    case GEM_DAEMON_NORMAL:
        // nothing to do
        break;

    case GEM_DAEMON_SUSPENDED:
        gem_syslog(LOG_INFO, "Suspending daemon!");
        break;

    case GEM_DAEMON_RELOAD:
        ret = do_reload(rdb_root);
        set_single_int(rdb_var, GEM_DAEMON_NORMAL);
        gem_daemon_state = GEM_DAEMON_NORMAL;
        gem_syslog(LOG_INFO, "Completed configuration reload");
        break;

    case GEM_DAEMON_KILL_EES:
        gem_terminate_all_ees(FALSE);
        set_single_int(rdb_var, GEM_DAEMON_NORMAL);
        gem_syslog(LOG_INFO, "Killed all EEs");
        break;

    case GEM_DAEMON_HEALTH_CHECK:
        set_single_int(rdb_var, GEM_DAEMON_NORMAL);
        gem_daemon_state = GEM_DAEMON_NORMAL;
        break;

    default:
        ret = -1;
        set_single_int(rdb_var, GEM_DAEMON_NORMAL);
        gem_daemon_state = GEM_DAEMON_NORMAL;
        break;

    case GEM_DAEMON_EXIT:
        // exit the daemon
        cleanup_and_exit();
        break;

    }
    return ret;
}

// @todo - future implementation
//int gem_detect_triggers_io(void)
//{
//    return 0;
//}

// on daemon startup, it ALWAYS subscribes RDB reload variable
// Return the value read from reload variable on success, -1 otherwise
static int subscribe_reload_var(char *rdb_root)
{
    char rdb_var[GEM_MAX_RDB_NAME_LENGTH];
    int reload_val;

    sprintf(rdb_var, "%s.%s",rdb_root, GEM_RELOAD_RDB_VAR);

    // check that RDB variable exists
    if (rdb_get_int(g_rdb_session, rdb_var, &reload_val) != 0)
    {
        if ((rdb_create_string(g_rdb_session, rdb_var, "0", PERSIST|STATISTICS,DEFAULT_PERM)) != 0)
        {
            return -1;
        }
        reload_val = 0;
    }

    if (rdb_subscribe(g_rdb_session, rdb_var) < 0)
    {
        return -1;
    }

    return reload_val;
}

//
// The "never ending" GEM control loop.
// Detect 4 sources of triggers via a combined fd (blocking with short
// timeout.
// Process EEs, this needs to be done even if no triggers
// were detected in the particular iteration of this loop.
// 100 ms sleep as detects are non-blocking
//
void gem_control_loop(char *rdb_root)
{
    fd_set fdsetR;
    int inotify_stat = 0;
    int rdb_stat = 0;
    int timer_stat= 0;
    int io_stat = 0; // future implementation

    while (TRUE)
    {

        // combine timers, rdb and inotify in the same select
        int db_fd = rdb_fd(g_rdb_session);
        int timer_fd = gem_timer_get_fd();
        int inotfiy_fd = gem_inotfiy_get_fd();

        // sanity check
        if ((db_fd < 0) || (timer_fd < 0) || (inotfiy_fd < 0))
        {
            gem_syslog(LOG_CRIT, "Get fd returns %d %d %d", db_fd, timer_fd, inotfiy_fd);
            break;
        }

        // determine the highest fd out of three
        int max_fd = db_fd > timer_fd ? db_fd : timer_fd;
        if (max_fd < inotfiy_fd)
        {
            max_fd = inotfiy_fd;
        }

        // add 3 fds to fdset
        FD_ZERO(&fdsetR);
        FD_SET(db_fd, &fdsetR);
        FD_SET(timer_fd, &fdsetR);
        FD_SET(inotfiy_fd, &fdsetR);

        // zeroise every time
        inotify_stat = 0;
        rdb_stat = 0;
        timer_stat= 0;
        io_stat = 0; // future implementation

        // select will return >0 if anything of interest occurs
        struct timeval tv = {0, 100000};
        int ret = select(max_fd + 1, &fdsetR, NULL, NULL, &tv);

        if (ret > 0)
        {
            // act on those that had a change
            if (FD_ISSET(inotfiy_fd, &fdsetR))
            {
                inotify_stat = gem_process_triggers_inotify(rdb_root);
            }

            if (FD_ISSET(db_fd, &fdsetR))
            {
                rdb_stat = gem_process_triggers_rdb(rdb_root);
            }

            if (FD_ISSET(timer_fd, &fdsetR))
            {
                timer_stat = gem_process_triggers_timer();
            }

            // @todo - add io detect when available
        }

        //
        // None of the above should return a negative
        //
        if ((rdb_stat < 0) || (inotify_stat < 0) || (timer_stat < 0) ||
                (io_stat < 0) || gem_sig_term)
        {
            // exit the loop
            break;
        }

        //
        // The GEM daemon has been suspended for some reason.
        // Keep detecting triggers but do not execute EEs
        //
        if (gem_daemon_state != GEM_DAEMON_NORMAL)
        {
            usleep(GEM_SLEEP_TIME_US);
            continue;
        }

        //
        // See if any EEs now need to run.
        // We have to check this even if no new triggers
        // have occured in the current iteration of the loop
        //
        if (gem_process_ees(rdb_root) < 0)
        {
            break;
        }
    }

    gem_syslog(LOG_INFO,
               "Exiting daemon loop rdb_stat=%d, inotify_stat=%d, timer_stat=%d,"
               " io_stat=%d, sigterm=%d",
               rdb_stat, inotify_stat, timer_stat, io_stat, gem_sig_term);
}


//
// Prepares everything and starts the daemon loop. This does not return if daemon starts ok.
//
int gem_start_daemon(char *rdb_root, BOOL do_stats, char *stats_dir, char *temp_dir, int verbosity)
{
    int reload_val;

    // 0. open log and set syslog verbosity
    // note that we do not see point in adding LOG_PERROR bit
    // which would also send messages to stderr
    // This can be achieved by compiling gem daemon with
    // V_LOGS_TO_PRINTF option in gem_api.h
    openlog("gem_daemon", LOG_PID, LOG_UPTO(LOG_DEBUG));
    gem_set_log_verbosity(verbosity);

    ///////////////////////////////// INIT SEQUENCE //////////////////////////////////

    // 1. Open RDB session
    if (rdb_open(NULL, &g_rdb_session) < 0)
    {
        gem_syslog(LOG_CRIT, "Could not open RDB");
        return -1;
    }

    // 2. start the stats module
    if (do_stats)
    {
        if (gem_stats_init((char *)stats_dir) < 0)
        {
            gem_syslog(LOG_ERR, "Could not initialize statistics logging");
            return -1;
        }
    }

    // 3. prepare inotify file descriptors
    if (gem_inotify_init(FALSE) < 0)
    {
        gem_syslog(LOG_ERR, "Could not initialize iNotify");
        return -1;
    }

    // 4. start a single 1 second timer used to kick our timer triggers
    if (gem_timer_init() < 0)
    {
        gem_syslog(LOG_ERR, "Could not initialize timer module");
        return -1;
    }

    // 5. initialize EE tables
    gem_init_all_ees();

    // 5a. Record the directory where temp files should be generated
    strncpy(temp_file_directory, temp_dir, strlen(temp_dir)+1);

    // 6. initialize trigger tables
    init_trigger_tables();

    // 7. subscribe the reload variable
    reload_val = subscribe_reload_var(rdb_root);
    if (reload_val < 0)
    {
        gem_syslog(LOG_ERR, "Could not subscribe to reload variable %s.%s",
                   rdb_root, GEM_RELOAD_RDB_VAR);
        return -1;
    }

    //
    // If the reload value was GEM_DAEMON_RELOAD on daemon startup,
    // re-write it to initiate reload!
    // This is because we did not receive the first notification
    // as we only just subscribed to it.
    //
    if (reload_val == GEM_DAEMON_RELOAD)
    {
        if (gem_set_reload_value(rdb_root, GEM_DAEMON_RELOAD) < 0)
        {
            gem_syslog(LOG_ERR, "Failed to set reload value of %s to %d",
                       rdb_root, GEM_DAEMON_RELOAD);
            // can still proceed after this error
        }
    }

    // never returns in case of normal operation
    gem_control_loop(rdb_root);

    ///////////////////////// CLEANUP SEQUENCE //////////////////////////////////////////
    gem_syslog(LOG_INFO, "GEM daemon exiting");

    // if any abnormal operation occured clean up in reverse order
    gem_terminate_all_ees(TRUE);

    // de-allocate trigger tables
    deallocate_trigger_tables();

    // delete EEs
    gem_delete_all_ees();

    // close the timer modules
    gem_timer_close();

    // close inotify module
    gem_inotify_close();

    // close statistics module/log files
    gem_stats_close();

    // close rdb
    rdb_close(&g_rdb_session);

    // close log
    closelog();

    return 0;
}
