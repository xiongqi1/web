//
// Part of GEM daemon
// 'C' code entry point
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include "fcntl.h"
#include <pwd.h>
#include <unistd.h>
#include "gem_api.h"
#include "gem_daemon.h"

extern void initDaemon(const char *lockfile, const char *user);

// two globals that are set in signal handlers
int gem_sig_term = 0;
int gem_sig_hup = 0;

// this will be the name of the user which runs the daemon process
#define GEM_USER_NAME "gemdaemon"

//
// A signal received when the controlling terminal exits
//
static void sig_handler_hup(int signum)
{
    gem_sig_hup=1;

    gem_syslog(LOG_INFO, "got SIGHUP");
}

//
// Ignore but log signal handler
//
static void sig_handler_ignore(int signum)
{
    gem_syslog(LOG_INFO, "got SIG- %d",signum);
}

//
// Will cause the GEM's main loop to exit via
// setting of gem_sig_term
//
static void sig_handler_term(int signum)
{
    gem_sig_term = 1;

    gem_syslog(LOG_INFO, "terminating : got SIG - %d",signum);
}

//
// Show the user how to invoke this programme
//
static void usage(char **argv)
{
    fprintf(stderr, "Usage: %s -r rdb_root -t temp_dir [-d daemon_mode (0/1)]"
            "  [-s stats_dir] [-v verbosity (0-7)]\n", argv[0]);

    // @todo - more user friendly output is possible
}

//
// C code entry point for GEM daemon
//
int main(int argc, char *argv[])
{
    BOOL be_daemon = FALSE;
    BOOL do_stats = FALSE;
    int verbosity = LOG_ERR;
    int opt;
    char *rdb_root = NULL;
    char *stats_dir = NULL;
    char *temp_dir = NULL;

    // parse command line arguments for:
    // 1) rdb root
    // 2) daemonize or not
    // 3) location of temp files
    // 4) location of stats/perf manager logs
    // 5) verbosity

    while ((opt = getopt(argc, argv, "r:ds:t:v:")) != -1)
    {
        switch (opt)
        {
        case 'r':
            rdb_root = optarg;
            break;

        case 'd':
            be_daemon = TRUE;
            break;

        case 's':
            do_stats = TRUE;
            stats_dir = optarg;
            break;

        case 't':
            temp_dir = optarg;
            break;

        case 'v':
            verbosity = atoi(optarg);
            break;

        default:
            usage(argv);
            exit(EXIT_FAILURE);
        }
    }

    if ((!rdb_root) || (!temp_dir))
    {
        usage(argv);
        exit(EXIT_FAILURE);
    }


    // configure signals - these are potentially all changed by daemonize anyway
    signal(SIGHUP, sig_handler_hup);
    signal(SIGINT, sig_handler_term);
    signal(SIGTERM, sig_handler_term);

    // no need for a SIGCHLD handler - we detect child process status explicitely
    //signal(SIGCHLD, SIG_DFL);

    signal(SIGTTIN, sig_handler_ignore);
    signal(SIGTTOU, sig_handler_ignore);
    signal(SIGUSR2, sig_handler_ignore);

    if (be_daemon)
    {
        // @todo - lock file should be rdb_root specific! - do not pass NULL
        initDaemon(NULL, GEM_USER_NAME);
    }

    return gem_start_daemon(rdb_root, do_stats, stats_dir, temp_dir, verbosity);
}
