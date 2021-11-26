//
// GEM daemon INTERNAL header file
// Not meant to be included by other applications
//
#ifndef _GEM_DAEMON_H
#define _GEM_DAEMON_H

#include <time.h>

// Bool type for internal use
typedef int BOOL;

// max size of trigger tables.
#define GEM_MAX_TIMER_TRIGGER_TABLE_SIZE    (128)
#define GEM_MAX_RDB_TRIGGER_TABLE_SIZE      (512)
#define GEM_MAX_FILE_TRIGGER_TABLE_SIZE     (128)

// default maximum allowed timeouts in seconds - this is the time
// EE is allowed to run before it is killed by GEM
#define GEM_DEFAULT_TIMEOUT	(10)
#define GEM_MAX_TIMEOUT	(300)

#define GEM_MAX_DIR_LENGTH              (250)       // max length of log or temp directory

// as it is impractical to allow more processes to run
// simultaneously, if necessary, do not start new
// processes if
#define GEM_MAX_PROCESS_LIMIT_ENABLE
#define GEM_MAX_RUNNING_PROCESS_LIMIT       (20)

//
// after this many failures to launch EE, GEM stops trying as hard
// in respect to this EE only
//
#define GEM_EE_ERR_MAX                      (50)

//
// types of stats/events logged by GEM stats/performance manager
//
typedef enum
{
    GEM_STATS_EXECUTED,
    GEM_STATS_KILLED,
    GEM_STATS_EXITED,
    GEM_STATS_KILLED_ALL,
    GEM_STATS_INOTIFY_TRIGGER,
    GEM_STATS_RDB_TRIGGER,
    GEM_STATS_RELOAD
} gem_stats_event_types;

// externs
extern int gem_sig_term, gem_sig_hup;
extern int add_rdb_trig_table_elem(char *name, int compare_func, char *compare_val, int ee_num);

//
// Describes a single element in file trigger table
//
typedef struct
{
    int  wd;
    char *watch_dir_name;
    char *watch_file_name;
    int  trigger_flags;
    int  ee_num;
} gem_file_trigger_table_elem;

//
// File trigger table object declaration
//
typedef struct
{
    int count;
    gem_file_trigger_table_elem table[GEM_MAX_FILE_TRIGGER_TABLE_SIZE];
} gem_file_trigger_table;


//
// Describes a single element in RDB trigger table
//
typedef struct
{
    char *name;     // name of RDB variable name trigger
    int  compare_function; // only used for Compare triggers
    char *compare_value; // only used for Compare triggers
    int  ee_num;    // reference to EE number
} gem_rdb_trigger_table_elem;

//
// RDB trigger table object declaration
//
typedef struct
{
    int count;
    gem_rdb_trigger_table_elem table[GEM_MAX_RDB_TRIGGER_TABLE_SIZE];
} gem_rdb_trigger_table;

//
// Describes a single element in timer trigger table
//
typedef struct
{
    struct  itimerspec timer_props;
    int  	ee_num;	// reference to ee to trigger
} gem_timer_trigger_table_elem;

//
// Timer trigger table object declaration
//
typedef struct
{
    int count;
    gem_timer_trigger_table_elem table[GEM_MAX_TIMER_TRIGGER_TABLE_SIZE];
} gem_timer_trigger_table;

//
// defines a single executable entry
//
typedef struct
{
    // static properties
    int type;           // as per gem_exec_types
    int restart;        // should it be restarted
    int kill_in_secs;	    // maximum timeout permitted before the child process is terminated
    int pid;		        // child process id
    clock_t ticks_start;    // time when EE started
    char *name;         // name of EE
    BOOL in_file;       // is the executable in a file or RDB
    char *file_exe;     // if in file, then this is the name of the file (full path) where the executable is stored
    char *rdb_var_exe;  // if not in file, this is the name of rdb variable where the executable is stored
    unsigned long crc32;// for executables stored in rdb variables it is optionally possible to protect it using crc32
    BOOL reread_mode;   // causes the executable to be re-read from RDB just before execution
    int err_count;      // the counter of consequtive failures to launch an EE

    // run-time properties
    BOOL trigger_flag;      // needs to run, one time flag
    BOOL required_state;	// should be running (last state of RDB variable change)
} gem_exec_entry;

//
// EE table object declaration
//
typedef struct
{
    int count;       // number of EE entries used
    gem_exec_entry ee[GEM_MAX_EXEC_ENTRIES]; // store for EE's
} gem_ee_table_type;

//
// Externs for functions
//

// functions to support EEs
extern void gem_init_ee(gem_exec_entry *ee);
extern void gem_init_all_ees();
extern void gem_delete_ee(gem_exec_entry *ee);
extern void gem_delete_all_ees(void);
extern void gem_terminate_all_ees(BOOL wait_for_child_exit);
extern int gem_add_ee(gem_exec_entry *ee);
extern int gem_load_ee(char *rdb_root, int ee_count, gem_exec_entry *ee);
extern void gem_print_ee_table(void);
extern int gem_process_ees(char *rdb_root);
extern int gem_templatemgr_parse_template(char *template_file_name, FILE* streamOut, int ee_count);


// timer trigger functions
extern int gem_timer_init(void);
extern void gem_timer_close(void);
extern int gem_process_triggers_timer(void);
extern int gem_timer_get_fd(void);

// inotify trigger functions
extern int gem_process_triggers_inotify(char *rdb_root);
extern int gem_inotify_init(BOOL non_block);
extern void gem_inotify_close(void);
extern int gem_inotify_add_watch(char *dir_or_file, int flags);
extern int gem_inotfiy_get_fd(void);

// lua execute functions
extern int gem_execute_lua_file(const char* fileName);
extern int gem_execute_lua_rdb_snippet(char* script, int num_bytes);

// rdb trigger functions
extern int gem_handle_conf_change(char *rdb_root);
extern int gem_process_triggers_rdb(char *rdb_root);


// start the loop
extern int gem_start_daemon(char *rdb_root, BOOL do_stats, char *stats_dir, char *temp_dir, int verbosity);

// replacement for sys basename()
extern char *gem_basename(char *path);

// stats stuff
extern int gem_stats_init(char *dir);
extern void gem_stats_close(void);
extern int gem_stats_add_record(int ee_number, int record_type, char *extra_info);

// externs for data structures shared by GEM daemon files
extern gem_ee_table_type gem_ee_table; // ee table
extern gem_rdb_trigger_table rdb_triggers;
extern gem_file_trigger_table file_triggers;
extern gem_timer_trigger_table timer_triggers;

// a global RDB session handle
extern struct rdb_session *g_rdb_session;

#endif /* _GEM_DAEMON_H */

