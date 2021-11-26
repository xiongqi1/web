//
// A one shot timer utility for Platypus and any platforms with old (<2.6.25) kernels
// where timerfd is not available.
//
// This is less efficient and accurate than the Bovine utility since it relies
// on select timeouts instead of system timers for elapsed time measurements
// As a results, it is expected that the utility will overrun given time delays,
// e.g. if 10 seconds is given it may take 11 or even more.
//
// 1) Reads desired timeout, in 10's of milliseconds, from the input RDB variable
// 2) Counts down
// 3) If/When timer runs down, an output RDB variable is set to a desired value
//
// For example:
// rdb set mm.input 1000 # set to 10 seconds
// one_shot_timer -i mm.input -o mm.output -v "expired"
//
// In 10 seconds, string "expired" will be written to the mm.output RDB variable.
// and the application exits.
//
// If, before the timer runs down, "0" is written to the input, the output will
// not be affected and the application will immediately exit
//
// rdb set mm.input 1000 # set to 10 seconds
// one_shot_timer -i mm.input -o mm.output -v "expired"
// rdb set mm.input 0 # stop the timer and make the one_shot_timer application exit
//
// The input variable can be updated to non-zero value whilst the timer is still
// running down, for example:
// rdb set mm.input 1000 # set to 10 seconds
// one_shot_timer -i mm.input -o mm.output -v "expired"
// rdb set mm.input 2000 # set to 20 seconds
//
// the output will be written in 20 seconds
//
// The one_shot_timer utility never modifies the input variable.
// If the caller is interested in remaining time until the timer expiry, use -f option.
//
// -f [feedback_variable] can specify the feedback RDB variable that will
// update the time remaining for the counter to r, in the same units as
// the input variable (e.g. in 10's of milliseconds). For efficiency, and to avoid
// unnecessary writes, this is updated every 100 ms.
//
// Other things worth mentioning:
// 1) Option -d will daemonize the application e.g. the control will return to the caller (great for scripts).
// 2) if -w is not given at all, an empty value will be written on expiry to the
//  rdb output variable.
// 3) The name of input and feedback RDB variables cannot be the same.
// 4) option -a can be used instead of -t to provide absolute time rather that RDB variable which stores time
// 5) option -x instead of -o can be used to launch an arbitrary executable. This can be even
//   used to relaunch the one_shot_timer to create a periodic timer.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include "fcntl.h"
#include <pwd.h>
#include <time.h>
#include <unistd.h>

#include "rdb_ops.h"

// a wrapper for printf or syslog
void ost_syslog(int priority, const char *format, ...);

// set log verbosity - note this will NOT set daemon's verbosity, but the application's
void ost_set_log_verbosity(int priority);

// timer trigger functions
int ost_timer_init(char *fb_var_name);
void ost_timer_close(void);
int ost_timer_get_fd(void);

//
// External daemonise function that lives in daemon_lib
//
extern void initDaemon(const char *lockfile, const char *user);

// some handy types
typedef int BOOL;

#ifndef NULL
#define NULL				0
#endif

#ifndef FALSE
#define FALSE				0
#endif

#ifndef TRUE
#define TRUE				1
#endif

// do not allow to start this one-shot timer with delay of more than 24 hours
#define MAX_DELAY_VAL_SEC  (24*60*60L) // 24 hours max

// name for daemonize
#define OST_USER_NAME "ost_daemon"

// module "globals" with local scope

// these 2 vars are used  in signal handlers
static int ost_sig_term = 0;
static int ost_sig_hup = 0;

volatile int g_count_10ms = 0;
int g_delay_val_10ms = 0;

// note this only affects ost_syslog logging level
static int ost_verbosity = LOG_ERR;

//
// A signal received when the controlling terminal exits
//
static void sig_handler_hup(int signum)
{
    ost_sig_hup=1;

    fprintf(stderr, "got SIGHUP");
}

//
// Ignore but log signal handler
//
static void sig_handler_ignore(int signum)
{
    fprintf(stderr, "got SIG- %d", signum);
}

//
// Will cause the main loop to exit via
// setting of ost_sig_term
//
static void sig_handler_term(int signum)
{
    ost_sig_term = 1;

    fprintf(stderr, "terminating : got SIG - %d",signum);
}


// set verbosity of log entries done via ost_syslog. Will not affect GEM Daemon's verbosity
void ost_set_log_verbosity(int verbosity)
{
    ost_verbosity = verbosity;
}

// handy for debugging
//#define V_LOGS_TO_PRINTF

// a simple wrapper around printf or syslog, qualified by verbosity
void ost_syslog(int priority, const char *format, ...)
{
    va_list fmtargs;
    char buffer[1024];

    va_start(fmtargs,format);
    vsnprintf(buffer,sizeof(buffer)-1, format, fmtargs);
    va_end(fmtargs);

    if (priority <= ost_verbosity)
    {
#ifdef V_LOGS_TO_PRINTF
        printf("one shot timer log, priority %d: %s\n", priority, buffer);
#else
        syslog(priority, "%s", buffer);
#endif
    }
}

//
// Show the user how to invoke this programme
//
static void usage(char **argv)
{
    fprintf(stderr, "Platypus One-Shot Timer Utility. Usage:\n"
            "%s -t [rdb Time variable, with value in 10s of milliseconds]\n"
            "-a [Absolute value in 10s of milliseconds (alternative to -t)]\n"
            "-o [rdb Output variable to write when timer expires]\n"
            "-x [eXecute statement] (alternative to -o)\n"
            "-w [value to Write to the RDB output variable] (optional)\n"
            "-f [rdb Feedback variable] (optional)\n"
            "-v [Verbosity] (optional)\n"
            "-d (Daemonize - optional)\n",
            argv[0]);
}

//
// Update the feedback rdb variable
// Since the original time rdb delay variable is never modified by us,
// and the caller may want to know how much time is left till expiry
// we provide a feedback via a different RDB variable.
// So, this function simply reads the value of the "countdown" timer and
// writes it to the feeback rdb variable. This is done in same units
// e.g. 10s of milliseconds
//
int ost_update_feedback_timer_rdb(char *feedback_timer_rdb, int curr_value_10ms)
{
    char val[16];
    if (feedback_timer_rdb)
    {
        if (curr_value_10ms < 0)
        {
            curr_value_10ms = 0;
        }
        // already in 10's of milliseconds
        sprintf(val, "%d", curr_value_10ms);
        if (rdb_update_single(feedback_timer_rdb, val, 0, 0, 0, 0) == 0)
        {
            return 0;
        }
    }
    return -1;
}

//
// When the countdown timer expires, write 0 to feedback variable
//
int ost_write_final_fb(char *feedback_timer_rdb)
{
    if (feedback_timer_rdb && rdb_update_single(feedback_timer_rdb, "0", 0, 0, 0, 0) == 0)
    {
        return 0;
    }

    return -1;
}


//
// We have subscribed to the original time rdb variable so we can do 2 things:
// Reload the countdown timer if the value is non-zero
// Exit if the value is zero.
//
int ost_process_trigger_rdb(char *rdb_var, char *fb_timer_rdb, int *stop_timer)
{
    int  buf_len;
    char *name_buf;

    // the utility was invoked with absolute timeout (not through RDB variable with a value)
    if (!rdb_var)
    {
        ost_syslog(LOG_ERR, "RDB trigger when using absolute timeout value");
        return 0;
    }

    *stop_timer = FALSE;

    name_buf = malloc(buf_len = 10000);

    //
    // Check if the expected variable has been triggered
    // This is probably a major overkill because we only subscribed to one
    // notification. But nevertheless, just for the sake of error handling,
    // check the name
    //
    if (name_buf && (rdb_get_names(rdb_var, name_buf, &buf_len, TRIGGERED) == 0))
    {
        name_buf[buf_len] = 0; // just in case
        if (strcmp(name_buf, rdb_var)==0)
        {
            g_delay_val_10ms = 0;
            if ((rdb_get_single_int(rdb_var, &g_delay_val_10ms) != 0) ||
                (g_delay_val_10ms < 0) || (g_delay_val_10ms > (MAX_DELAY_VAL_SEC * 100)))
            {
                free (name_buf);
                ost_syslog(LOG_ERR, "Invalid reload value for timer");
                return -1;
            }

            if (g_delay_val_10ms == 0)
            {
                *stop_timer = TRUE;
                ost_write_final_fb(fb_timer_rdb);
                ost_syslog(LOG_INFO, "Timer stopped by writing zero to RDB var");
            }
            else
            {
                g_count_10ms = 0; // restart counting
            }
        }
        else
        {
            // cannot happen - this is an error
            free (name_buf);
            ost_syslog(LOG_ERR, "Incorrect trigger buffer %s", name_buf);
            return -1;
        }
    }
    free (name_buf);

    return 0;
}

//
// Execute command give in the argument
//
static int ost_execute(const char* execute_statement)
{

    pid_t pid = fork();

    // if child
    if (pid == 0)
    {
        char path_buf[1024];
        char *p_path;

        int  arg_val_num = 0;
        char *arg_val_buf[256];

        // copy
        strcpy(path_buf, execute_statement);

        p_path = path_buf;

        // splice - chopping and getting each pointer
        while (arg_val_num < sizeof(arg_val_buf)/sizeof(arg_val_buf[0]) - 1)
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

        //
        // Use execvp rather than execv.
        // This will allow name search of the
        // calling terminal to be applied (as opposed to allowing
        // abolsute path only)
        //
        execvp(arg_val_buf[0], arg_val_buf);

        // if invalid command line, we will return straight away
        // otherwise, we never get here
        exit (EXIT_FAILURE);
    }
    // if parent
    else if (pid > 0)
    {
        // child is executing!
        return 0;
    }
    else
    {
        // fork failed
        return -1;
    }
}


//
// The "never ending" control loop.
// Detect RDB trigger (and uses a 100ms short timeout).
//
// 1) On rdb select>0, re-reads the time variable and either restart the timer, or (if zero read)
//  cleans up and exits the application
//
//
// rdb_var can be  NULL
//
void ost_control_loop(char *rdb_var, char *output_str, char *value, char *fb_timer_rdb, BOOL execute_statement)
{
    fd_set fdsetR;
    int rdb_stat = 0, exec_stat = 0, fb_stat = 0;
    BOOL exit_condition = FALSE;

    g_count_10ms = 0;
    int tmp = 0;
    while (!exit_condition)
    {
        // combine one or two timers, and one rdb into the same select
        int rdb_fd = rdb_get_fd();

        // sanity check
        if (rdb_fd < 0)
        {
            ost_syslog(LOG_CRIT, "Get fd returns %d", rdb_fd);
            break;
        }

        FD_ZERO(&fdsetR);
        FD_SET(rdb_fd, &fdsetR);

        // zeroise every time
        rdb_stat = 0;
        exec_stat = 0;
        fb_stat = 0;

        // select will return >0 if anything of interest occurs

        // instead of timeout value of 10ms, we use 100ms which
        // makes the application more efficient, but only allows
        // timer resolution in 100ms. Since the existing Bovine one shot timer
        // uses 10ms time base it would be incorrect to make the two
        // incompatible by switching this simple utility to 100 ms base
        // In other words, command line is the same for either full or simple
        // one shot timer, with all timeouts given in 10's milliseconds
        struct timeval tv = {0, 100000};
        int ret = select(rdb_fd + 1, &fdsetR, NULL, NULL, &tv);

        if (ret > 0)
        {
            // 1) RDB trigger
            if (FD_ISSET(rdb_fd, &fdsetR))
            {
                rdb_stat = ost_process_trigger_rdb(rdb_var, fb_timer_rdb, &exit_condition);
            }
        }
        else if (ret == 0)
        {
            // since we running this 10 times slower, increment by 10
            g_count_10ms += 10;

            // check timer expiry condition
            if (g_count_10ms >= g_delay_val_10ms)
            {
                // write 0 to feedback rdb
                ost_write_final_fb(fb_timer_rdb);

                //
                // 2 possibilities on expiry of the timer
                // either set an RDB variable, or execute an arbitrary command line
                //
                if (execute_statement)
                {
                    //
                    // execute an arbitrary statement - rather than setting an RDB variable
                    // this is always done via fork and our main process exits
                    //
                    exec_stat = ost_execute(output_str);
                }
                else
                {
                    //
                    // Set the output rdb variable to required value.
                    // The value can be empty if it has not been passed to us
                    //
                    rdb_stat = (rdb_update_single(output_str, value, 0, 0, 0, 0) == 0) ? 0 : -1;
                }

                // one shot timer expires - exit the app without errors
                exit_condition = TRUE;
            }

            // update if necessary
            if (fb_timer_rdb)
            {
                fb_stat = ost_update_feedback_timer_rdb(fb_timer_rdb, g_delay_val_10ms - g_count_10ms);
            }

        }
        else // negative return from select
        {
            break;
        }

        //
        // None of the above should return a negative
        //
        if ((rdb_stat < 0)  || (exec_stat < 0) || (fb_stat < 0) || ost_sig_term)
        {
            // exit the loop
            break;
        }
    }

    ost_syslog(LOG_INFO,
               "Exiting daemon loop rdb_stat=%d, exec_stat=%d, "
               "ost_sig_term=%d, exit condition %d",
               rdb_stat, exec_stat, ost_sig_term, exit_condition);

}

//
// C code entry point for one-shot-timer (OST tool)
//
int main(int argc, char *argv[])
{
    // vars that can be overwritten from command line
    BOOL be_daemon = FALSE;
    int opt;
    char *time_var = NULL;
    char *output_str = NULL;
    char *value = ""; // default to empty value - but not NULL!
    char *feedback_var = NULL;
    BOOL execute_statement = FALSE;
    BOOL rdb_output_statement = FALSE;

    // parse command line arguments:
    while ((opt = getopt(argc, argv, "t:a:o:w:f:v:x:dh")) != -1)
    {
        switch (opt)
        {
        case 't':
            // read RDB variable that has the delay
            time_var = optarg;
            break;

        case 'a':
            // absolute value in 10's of milliseconds
            g_delay_val_10ms = atoi(optarg);
            break;

        case 'o':
            // RDB output variable to write to when timeout expires
            output_str = optarg;
            rdb_output_statement = TRUE;
            break;

        case 'x':
            output_str = optarg;
            execute_statement = TRUE;
            break;

        case 'w':
            // value to write to the RDB output variable on completion
            value = optarg;
            break;

        case 'f':
            // if given, the remaining timeout will be mirrored here
            feedback_var = optarg;
            break;

        case 'v':
            // change default verbosity from LOG_ERR to something
            ost_verbosity = atoi(optarg);
            break;

        case 'd':
            // set daemon mode (should always be the case)
            be_daemon = TRUE;
            break;

        default:
            usage(argv);
            exit(EXIT_FAILURE);
        }
    }

    // has to either have an abs delay or RDB time in variable
    if (!time_var && (g_delay_val_10ms == 0))
    {
        fprintf(stderr, "Invalid timeout\n");
        exit(EXIT_FAILURE);
    }

    // check mutually exlusive options
    if ((g_delay_val_10ms > 0) && time_var)
    {
        fprintf(stderr, "Cannot have both -a and -t options\n");
        exit(EXIT_FAILURE);
    }

    if (rdb_output_statement && execute_statement)
    {
        fprintf(stderr, "Cannot have both -o and -x options\n");
        exit(EXIT_FAILURE);
    }

    // must either have an absolute timeout, or RDB variable
    if (g_delay_val_10ms > 0) // absolute timeout value was used
    {
        if (!output_str)
        {
            fprintf(stderr, "No output option selected\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (time_var)
    {
        if (!output_str)
        {
            fprintf(stderr, "No output option selected\n");
            exit(EXIT_FAILURE);
        }
    }

    //
    // check that we are not using the same RDB var for time and f/b
    //
    // The other common RDB var is ok - note that we could even make the output variable the
    // same as the time variable if it made sense in any user scenario
    // rdb set test.time 1000
    // one_shot_timer -t test.time -o test.time
    //
    // After 10 seconds, test.time will contain "" empty value
    //
    if (time_var && feedback_var && (strcmp(time_var, feedback_var) == 0))
    {
        fprintf(stderr, "Cannot have the same RDB variable for delay and feedback variables (%s)\n",
            feedback_var);
        exit(EXIT_FAILURE);
    }

    // configure signals
    signal(SIGHUP, sig_handler_hup);
    signal(SIGINT, sig_handler_term);
    signal(SIGTERM, sig_handler_term);
    signal(SIGTTIN, sig_handler_ignore);
    signal(SIGTTOU, sig_handler_ignore);
    signal(SIGUSR2, sig_handler_ignore);
    signal(SIGCHLD, sig_handler_ignore);

    // daemonize if wanted
    if (be_daemon)
    {
        initDaemon(NULL, OST_USER_NAME);
    }

    // open log - @todo - last argument is incorrect
    openlog("one_shot_timer", LOG_PID, LOG_UPTO(LOG_DEBUG));

    // set verbosity level
    ost_set_log_verbosity(ost_verbosity);

    // 1. Get RDB file descriptor
    int rdb_fd = rdb_open_db();
    if (rdb_fd < 0)
    {
        ost_syslog(LOG_CRIT, "Could not open RDB");
        return EXIT_FAILURE;
    }

    // 2. subscribe the time variable
    if (time_var)
    {
        if ((rdb_get_single_int(time_var, &g_delay_val_10ms) != 0) || (rdb_subscribe_variable(time_var) < 0))
        {
            fprintf(stderr, "Incorrect delay variable %s\n", time_var);
            return EXIT_FAILURE;
        }
    }

    if ((g_delay_val_10ms <= 0) || (g_delay_val_10ms > (MAX_DELAY_VAL_SEC * 100)))
    {
        fprintf(stderr, "Invalid timeout\n");
        return EXIT_FAILURE;
    }

    //
    // enter the main loop that returns only when it is time for us to exit, e.g, either:
    // 1) the countdown timer expired
    // 2) we were stopped by writing "0" to the input variable (timer value)
    //
    ost_control_loop(time_var, output_str, value, feedback_var, execute_statement);

    // cleanup

    // close rdb
    rdb_close_db();

    // close log
    closelog();

    // back to calling terminal
    printf("\n");

    return (EXIT_SUCCESS);
}


