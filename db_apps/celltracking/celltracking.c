/**
 * cell track log generator
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
*/
/* local headers */
#include "log.h"
#include "rdb.h"
#include "celltracking.h"

/* standard headers */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#define DAEMON_NAME "celltracking"

static const char *SERVINGCELL0 = LOG_STORAGE_PATH "/servingcell.0"; // two custom log files
static const char *SERVINGCELL1 = LOG_STORAGE_PATH "/servingcell.1";

static volatile int g_term_sig = 0;
int g_event_count = 0;

/*
Count event number in /opt/servingcell.0 once when program start.
servingcell.0 is current log file
*/
void count_events()
{
    char line[CELLTRACKING_LINE_MAX];
    FILE *file = fopen(SERVINGCELL0, "r");
    g_event_count = 0;
    if (file) {
        while (fgets(line, CELLTRACKING_LINE_MAX, file)) {
            g_event_count++;
        }
        fclose(file);
    }
}

/*
If this function is kicked, either PCI or earfcn have changed
The idea is to write various parameters such as RSRP and so on, into a file.
*/
void write_celltracking_info(int report_pci, int report_earfcn,
                             const char *syslog_line, const char *celllog_line)
{
    FILE *file;

    if (g_event_count >= CELLTRACKING_EVENT_MAX) { //rename SERVINGCELL0
        rename(SERVINGCELL0, SERVINGCELL1);
        g_event_count = 0;
    }
    file = fopen(SERVINGCELL0, "a");
    if (file == NULL) {
        ERR("Cannot open file %s", SERVINGCELL0);
        return;
    }

    /* syslog:<timestamp> new serving cell earfcn=<earfcn>, pci=<pci>,  .... */
    NOTICE("new serving cell earfcn=%d, pci=%d, %s", report_earfcn, report_pci, syslog_line);
    /* servicecell.{0,1}:<timestamp> earfcn=<earfcn>,  ... */
    struct tm *ltime;
    time_t now = time(NULL);
    ltime = localtime(&now);
    char timestamp[80];
    strftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", ltime);
    fprintf(file, "%s earfcn=%d, pci=%d, %s\n", timestamp, report_earfcn, report_pci, celllog_line);
    g_event_count++;
    fclose(file);
}

static void sig_handler(int sig)
{
    switch (sig) {
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        DEBUG("signal %d caught, terminate", sig);
        g_term_sig = sig;
        break;
    case SIGSEGV:
        DEBUG("SIGSEGV caught, terminate");
        exit(-1);
        break;
    default:
        DEBUG("sig(%d) caught", sig);
        break;
    }
}

static void usage(FILE *fd)
{
    fprintf(fd,
            "\n"
            "Usage: celltrack [options]\n"
            "\n"
            "	Options:\n"
            "\t-h print this usage screen\n"
            "\t-v print debug message on console\n"
            "\n");
}

int main(int argc, char *argv[])
{
    int opt;
    int verbosity = 0;
    int options;
    int report_pci = 0;
    int report_earfcn = 0;
    int do_write_cellinfo = FALSE;
    char syslog_line[CELLTRACKING_LINE_MAX];
    char celllog_line[CELLTRACKING_LINE_MAX];

    while ((opt = getopt(argc, argv, "vh?")) != EOF) {
        switch (opt) {

        case 'h':
            usage(stdout);
            exit(0);

        case 'v':
            verbosity = 1;
            break;

        case ':':
            fprintf(stderr, "missing argument - %c\n", opt);
            usage(stderr);
            exit(-1);

        case '?':
            fprintf(stderr, "unknown option - %c\n", opt);
            usage(stderr);
            exit(-1);

        default:
            usage(stderr);
            exit(-1);
            break;
        }
    }
    options = LOG_PID;
    if (verbosity) {
        options |= LOG_PERROR;
    }
    openlog(DAEMON_NAME, options, LOG_LOCAL5);

    INFO("initiate signal handlers");
    signal(SIGCHLD, sig_handler);
    signal(SIGHUP, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGPIPE, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGSEGV, sig_handler);
    signal(SIGTERM, sig_handler);

    INFO("initiate RDB");
    rdb_init();

    INFO("count previous events");
    count_events();

    while (g_term_sig == 0) {

        if (detect_cell_change(&report_pci, &report_earfcn)) {
            do_write_cellinfo = TRUE;
        }
        // cell changed, then try to write cell tracking information
        if (do_write_cellinfo) {
            /*
            * The various values for the signal info may take some time to
            * become available after the cell change has occurred. If all the
            * info is not yet available, get_signal_info will return FALSE. In
            * that case the info is not written out and the write will be
            * re-attempted on the next poll cycle.
            */
            if (get_signal_info(syslog_line, celllog_line, CELLTRACKING_EVENT_MAX)) {
                write_celltracking_info(report_pci, report_earfcn, syslog_line, celllog_line);
                do_write_cellinfo = FALSE;
            }
        }
        usleep(1000000);
    }

    INFO("finalize rdb");
    rdb_fini();

    closelog();

    return 0;
}
