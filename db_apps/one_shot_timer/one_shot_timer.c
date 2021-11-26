//
// A simple one shot timer utility
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
// 2) if -v is not given at all, an empty value will be written on expiry to the
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
#include <sys/timerfd.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#include <time.h>

#include "rdb_ops.h"

// a wrapper for printf or syslog
void ost_syslog(int priority, const char *format, ...);

// set log verbosity - note this will NOT set daemon's verbosity, but the application's
void ost_set_log_verbosity(int priority);

// timer trigger functions
int ost_timer_init(void);
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

// do not allow to start this one-shot timer with delay of more than 100 days
#define MAX_DELAY_VAL_SEC  (100*24*60*60L)

// name for daemonize
#define OST_USER_NAME "ost_daemon"

// module "globals" with local scope

// these 2 vars are used  in signal handlers
static int ost_sig_term = 0;
static int ost_sig_hup = 0;

// 2 file descriptors for timers
static int timer_fd = -1; // timer file descriptor
static int fb_timer_fd = -1; // feedback timer - the one that writes remaining value to the RDB

// note this only affects ost_syslog logging level
static int ost_verbosity = LOG_ERR;

static BOOL exit_condition = FALSE;
static char *timestamp_var = NULL;
static time_t last_timestamp = 0;
static char *fb_timer_rdb;
static struct rdb_session *rdb;
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
    fprintf(stderr, "Bovine One-Shot Timer Utility. Usage:\n"
            "%s -t [rdb Time variable, with value in 10s of milliseconds]\n"
            "-a [Absolute value in 10s of milliseconds (alternative to -t)]\n"
            "-s [RDB timestamp variable] (optional)\n"
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
int ost_update_feedback_timer_rdb()
{
    char val[16];
    struct itimerspec curr_value;
    if (fb_timer_rdb && timerfd_gettime(timer_fd, &curr_value) == 0)
    {
        // convert into 10's milliseconds
        sprintf(val, "%ld", curr_value.it_value.tv_sec * 100 + curr_value.it_value.tv_nsec / 10000000);
        if (rdb_update_string(rdb, fb_timer_rdb, val, 0, 0) == 0)
        {
            return 0;
        }
    }
    return -1;
}

//
// When the countdown timer expires, write 0 to feedback variable
//
int ost_write_final_fb()
{
    if (fb_timer_rdb && rdb_update_string(rdb, fb_timer_rdb, "0", 0, 0) == 0)
    {
        return 0;
    }

    return -1;
}

//
// A helper to read the timestamp var and calculate the delay.
// if not init then it may make a shortcut to avoid a loop (because
// ost_timer_set_time() rewrites the timestamp which would cause a retrigger)
//
static int ost_calc_delay_from_timestamp(int init)
{
    long long delay_val_10ms = 0;
    // if timestamp var exists and is nonzero then treat as a time_t
    rdb_get_int(rdb, timestamp_var, &delay_val_10ms);
    if ((!init) && (delay_val_10ms == last_timestamp))
    {
        return -1;
    }
    ost_syslog(LOG_INFO, "Read timestamp:%d\n", delay_val_10ms);
    if (delay_val_10ms)
    {
        // need to calculate delay by subtracting the current time
        delay_val_10ms -= time(NULL);
        // and converting to 10ms periods
        delay_val_10ms *= 100;
        ost_syslog(LOG_INFO, "Calculated delay:%d\n", delay_val_10ms);
        // but it's not possible to go back in time! (0 would stop)
        if (delay_val_10ms <= 0)
        {
            delay_val_10ms = 1;
        }
    }
    return (int) delay_val_10ms;
}

//
// A helper to (re)start the one and only one-shot timer
//
int ost_timer_set_time(long int delay_val_10ms)
{
    if (!delay_val_10ms)
    {
        exit_condition = TRUE;
        ost_write_final_fb();
        ost_syslog(LOG_INFO, "Timer stopped by writing zero to RDB var");
        return 0;
    }
    else if ((delay_val_10ms < 0) || (delay_val_10ms > (MAX_DELAY_VAL_SEC * 100)))
    {
        return -1;
    }

    if (timestamp_var)
    {
        last_timestamp = delay_val_10ms / 100 + time(NULL);
        char val[16];
        sprintf(val, "%ld", last_timestamp);
        rdb_update_string(rdb, timestamp_var, val, 0, 0);
    }

    struct itimerspec timeout_onesec;

    timeout_onesec.it_value.tv_sec = delay_val_10ms/100;
    //
    // The following line looks suspicious for overflow problems and I was going
    // to change it but then I wrote a program to figure out why it ever worked
    // and it turns out to be 100% correct while delay_val_10ms is positive
    // (it also works for negative multiples of 100). That means that
    // tv_nsec won't be <0 or >999,999,999 until the timer is >248 days!
    //
    timeout_onesec.it_value.tv_nsec = (delay_val_10ms * 10000000) - (timeout_onesec.it_value.tv_sec * 1000000000);

    // one shot timer, interval should be 0
    timeout_onesec.it_interval.tv_sec = 0;
    timeout_onesec.it_interval.tv_nsec = 0;

    return timerfd_settime(timer_fd, 0, &timeout_onesec, NULL);
}

//
// Initialize timer module
// Create, but do not start the countdown timer
// If requested, create and start the feedback timer
//
// Return 0 on success, -1 on error
//
int ost_timer_init()
{

    // start the one shot timer
    timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd == -1)
    {
        ost_syslog(LOG_CRIT, "Could not create timer fd");
        return -1;
    }

    //
    // if specified, start the feedback timer.
    // if *fb_timer_rdb, there is no need to start this extra timer.
    //
    if (fb_timer_rdb)
    {
        fb_timer_fd = timerfd_create(CLOCK_REALTIME, 0);
        if (fb_timer_fd == -1)
        {
            ost_syslog(LOG_CRIT, "Could not create feedback timer fd");
            return -1;
        }

        struct itimerspec timeout_onesec;

        // 100 msecond resolution - so updates occur every 100 ms
        timeout_onesec.it_value.tv_sec = 0;
        timeout_onesec.it_value.tv_nsec = 100000000L;

        // same value written to interval since timer is self-restarting
        timeout_onesec.it_interval.tv_sec = 0;
        timeout_onesec.it_interval.tv_nsec = 100000000L;

        timerfd_settime(fb_timer_fd, 0, &timeout_onesec, NULL);
    }

    return 0;
}

//
// Close timer file descriptors
//
void ost_timer_close(void)
{
    if (timer_fd >= 0)
    {
        (void)close(timer_fd);
    }
    timer_fd = -1;

    if (fb_timer_fd >= 0)
    {
        (void)close(fb_timer_fd);
    }
    fb_timer_fd = -1;
}

//
// Processes the feedback timer select()
// This will occur every 100 ms
//
int ost_process_fb_timer()
{
    long long foo;

    // clear the timer so select will no longer return immediately
    if (read(fb_timer_fd, &foo, sizeof(foo)) < 0)
    {
        ost_syslog(LOG_ERR,"Feedback timer read fd returns error");
        return -1;
    }

    ost_update_feedback_timer_rdb();

    return 0;
}

//
// We have subscribed to the original time rdb variable so we can do 2 things:
// Reload the countdown timer if the value is non-zero
// Exit if the value is zero.
//
int ost_process_trigger_rdb(char *rdb_var)
{
    int  buf_len;
    char *name_buf;

    // the utility was invoked with absolute timeout (not through RDB variable with a value)
    if (!(rdb_var || timestamp_var))
    {
        ost_syslog(LOG_ERR, "RDB trigger when using absolute timeout value");
        return 0;
    }

    //not required? exit_condition = FALSE;

    name_buf = malloc(buf_len = 10000);

    //
    // Check if the expected variable has been triggered
    //
    if (name_buf && (rdb_getnames(rdb, "", name_buf, &buf_len, TRIGGERED) == 0))
    {
        name_buf[buf_len] = 0; // just in case
        if (rdb_var && strcmp(name_buf, rdb_var)==0)
        {
            int delay_val_10ms = 0;
            if ((rdb_get_int(rdb, rdb_var, &delay_val_10ms) != 0) ||
                (delay_val_10ms < 0) || (delay_val_10ms > (MAX_DELAY_VAL_SEC * 100)))
            {
                free (name_buf);
                ost_syslog(LOG_ERR, "Invalid reload value for timer");
                return -1;
            }

            if (ost_timer_set_time(delay_val_10ms) != 0)
            {
                free (name_buf);
                ost_syslog(LOG_ERR, "Failed to reload timer to %d", delay_val_10ms);
                return -1;
            }
        }
        else if (timestamp_var && !strcmp(name_buf, timestamp_var))
        {
            ost_timer_set_time(ost_calc_delay_from_timestamp(0));
        }
        else
        {
            // cannot happen - this is an error
            ost_syslog(LOG_ERR, "Incorrect trigger buffer %s", name_buf);
            free (name_buf);
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
// Detect 3 sources of triggers via a combined fd (blocking with short
// timeout).
//
// 1) On rdb select>0, re-reads the time variable and either restart the timer, or (if zero read)
//  cleans up and exits the application
//
// 2) On (optional) feedback select>0, updates the rdb feedback variable to let others know
//  how long is left until the timer expiry
//
// 3) On countdown timer select>0, updates the rdb output variable to requested value (or sets
//  an empty value if the -v option is not specified) and exits the application
//
// rdb_var can be  NULL
//
void ost_control_loop(char *rdb_var, char *output_str, char *value, BOOL execute_statement)
{
    fd_set fdsetR;
    int rdb_stat = 0, timer_stat = 0, fb_timer_stat = 0, exec_stat = 0;
    exit_condition = FALSE;

    while (!exit_condition)
    {
        // combine one or two timers, and one rdb into the same select
        int rdbfd = rdb_fd(rdb);

        // sanity check
        if ((rdbfd < 0) || (timer_fd < 0))
        {
            ost_syslog(LOG_CRIT, "Get fd returns %d %d", rdbfd, timer_fd);
            break;
        }

        // determine the highest fd out of three
        int max_fd = rdbfd > timer_fd ? rdbfd : timer_fd;
        if (fb_timer_fd > max_fd)
        {
            max_fd = fb_timer_fd;
        }

        // add 2 or 3 (if feedback timer is used) fds to fdset
        FD_ZERO(&fdsetR);
        FD_SET(rdbfd, &fdsetR);
        FD_SET(timer_fd, &fdsetR);
        if (fb_timer_fd > 0)
        {
            FD_SET(fb_timer_fd, &fdsetR);
        }

        // zeroise every time
        rdb_stat = 0;
        timer_stat = 0;
        fb_timer_stat = 0;
        exec_stat = 0;

        //
        // Select will return >0 if anything of interest occurs.
        // We can use NULL instead of timeout as select will
        // return on timer events
        //
        int ret = select(max_fd + 1, &fdsetR, NULL, NULL, NULL);

        if (ret > 0)
        {
            // 1) RDB trigger
            if (FD_ISSET(rdbfd, &fdsetR))
            {
                rdb_stat = ost_process_trigger_rdb(rdb_var);
            }

            // 2) One shot timer trigger
            if (FD_ISSET(timer_fd, &fdsetR))
            {
                // write 0 to feedback rdb
                ost_write_final_fb();

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
                    rdb_stat = (rdb_update_string(rdb, output_str, value, 0, 0) == 0) ? 0 : -1;
                }

                // one shot timer expires - exit the app without errors
                exit_condition = TRUE;
            }

            // 3) Optional feedback timer
            if (fb_timer_fd > 0)
            {
                if (FD_ISSET(fb_timer_fd, &fdsetR))
                {
                    fb_timer_stat = ost_process_fb_timer();
                }
            }
        }

        //
        // None of the above should return a negative
        //
        if ((rdb_stat < 0) || (timer_stat < 0) || (fb_timer_stat < 0) || (exec_stat < 0) ||
            ost_sig_term || (ret < 0))
        {
            // exit the loop
            break;
        }
    }

    ost_syslog(LOG_INFO,
               "Exiting daemon loop rdb_stat=%d, timer_stat=%d, exec_stat=%d, "
               "ost_sig_term=%d, exit condition %d",
               rdb_stat, timer_stat, exec_stat, ost_sig_term, exit_condition);

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
    int delay_val_10ms = 0;
    BOOL execute_statement = FALSE;
    BOOL rdb_output_statement = FALSE;

    // parse command line arguments:
    while ((opt = getopt(argc, argv, "t:a:o:w:f:v:x:s:dh")) != -1)
    {
        switch (opt)
        {
        case 't':
            // read RDB variable that has the delay
            time_var = optarg;
            break;

        case 'a':
            // absolute value in 10's of milliseconds
            delay_val_10ms = atoi(optarg);
            break;

        case 's':
            // timestamp RDB - will be set if -t or -a also provided
            // or otherwise read and the difference to now used as -a
            timestamp_var = optarg;
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
            fb_timer_rdb = optarg;
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

    // check mutually exlusive options
    if ((delay_val_10ms > 0) && time_var)
    {
        fprintf(stderr, "Cannot have both -a and -t options\n");
        exit(EXIT_FAILURE);
    }

    if (rdb_output_statement && execute_statement)
    {
        fprintf(stderr, "Cannot have both -o and -x options\n");
        exit(EXIT_FAILURE);
    }

    if (!output_str)
    {
        fprintf(stderr, "No output option selected\n");
        exit(EXIT_FAILURE);
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
    if (time_var && fb_timer_rdb && (strcmp(time_var, fb_timer_rdb) == 0))
    {
        fprintf(stderr, "Cannot have the same RDB variable for delay and feedback variables (%s)\n",
            fb_timer_rdb);
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

    // open log
    openlog("one_shot_timer", LOG_PID, LOG_UPTO(LOG_DEBUG));

    // set verbosity level
    ost_set_log_verbosity(ost_verbosity);

    // 1. Get RDB file descriptor
    int rdbfd = rdb_open(NULL, &rdb);
    if (rdbfd < 0)
    {
        ost_syslog(LOG_CRIT, "Could not open RDB");
        return EXIT_FAILURE;
    }

    // has to either have an abs delay or RDB time in variable
    if (!time_var && (delay_val_10ms == 0))
    {
        if (timestamp_var)
        {
            // if timestamp var exists and is nonzero then treat as a time_t
            delay_val_10ms = ost_calc_delay_from_timestamp(1);
        }
        if (!delay_val_10ms)
        {
            fprintf(stderr, "Invalid timeout\n");
            exit(EXIT_FAILURE);
        }
    }


    // 2. subscribe the time variable
    if (timestamp_var)
    {
        rdb_subscribe(rdb, timestamp_var); //not an error if this doesn't work
    }
    if (time_var)
    {
        if ((rdb_get_int(rdb, time_var, &delay_val_10ms) != 0) || (rdb_subscribe(rdb, time_var) < 0))
        {
            fprintf(stderr, "Incorrect delay variable %s\n", time_var);
            return EXIT_FAILURE;
        }
    }

    if (delay_val_10ms == 0)
    {
        // no point doing anything note that in timestamp mode
        // even if it is in the past, delay_val_10ms = 1
        fprintf(stderr, "Incorrect delay variable %s\n", time_var);
        return EXIT_FAILURE;
    }

    // 3. Initialize the timers
    if (ost_timer_init() < 0)
    {
        ost_syslog(LOG_ERR, "Could not initialize timer module");
        return EXIT_FAILURE;
    }

    // 4. start countdown
    if (ost_timer_set_time(delay_val_10ms) != 0)
    {
        ost_syslog(LOG_ERR, "Could not start timer");
        return EXIT_FAILURE;
    }

    //
    // enter the main loop that returns only when it is time for us to exit, e.g, either:
    // 1) the countdown timer expired
    // 2) we were stopped by writing "0" to the input variable (timer value)
    //
    ost_control_loop(time_var, output_str, value, execute_statement);

    // cleanup

    // close the timer modules
    ost_timer_close();

    // close rdb
    rdb_close(&rdb);

    // close log
    closelog();

    // back to calling terminal
    printf("\n");

    return (EXIT_SUCCESS);
}


