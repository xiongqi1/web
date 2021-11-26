/*
 * Button manager main.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <syslog.h>

#include "buttonaction.h"
#include "buttonexec.h"
#include "evdev.h"
#include "timer.h"


static int term = 0;

/**
 * @brief handles EV events by calling the corresponding button action via Button execution.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 * @param depressed is a depressed flag (0=released, 1=depressed).
 */
static void evdev_event_callback(struct evdev_t* evdev, int key_type, int key_code, int depressed)
{
    syslog(LOG_DEBUG, "got event (dev_name=%s,key_type=%d,key_code=%d,depressed=%d)", evdev->dev_name, key_type, key_code,
           depressed);

    button_exec_call_func(evdev, key_type, key_code, depressed);
}

/**
 * @brief prints usage.
 *
 * @param fp is a FILE handle to print.
 */
static void print_usage(FILE* fp)
{
    fprintf(fp,
            "buttonmgr v1.0\n"
            "\n"
            "usage:\n"
            "\n"
            "\t buttonmgr -s <notifying script>\n"
            "\n"
            "parameters:\n"
            "\t -s \t specify a script to get notification\n"
            "\t -h \t print this usage\n"
            "\n"
           );
}

/**
 * @brief handles signals.
 *
 * @param sig_no is signal number.
 */
static void sig_handler(int sig_no)
{
    switch (sig_no) {
        case SIGINT:
        case SIGTERM:
            term = 1;
            break;
    }
}


/**
 * @brief is main function.
 *
 * @param argc is total number of parameters.
 * @param argv[] is an array of parameters.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int main(int argc, char* argv[])
{

    fd_set fdr;
    int nfds;
    int stat;
    int opt;

    char* noti_script = NULL;

    struct timeval timeout;


    while ((opt = getopt(argc, argv, "hs:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(stdout);
                goto fini;
                break;

            case 's':
                noti_script = optarg;
                break;

            default :
                fprintf(stderr, "Unknown option %c, exiting\n", opt);
                return -1;
                break;
        }
    }

    if (!noti_script) {
        fprintf(stderr, "mandatory option -s missing\n");

        print_usage(stderr);
        exit(-1);
    }

    // remap signals
    signal(SIGHUP, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGUSR1, sig_handler);

    /* initiate timer */
    timer_init();

    /* initiate button action */
    buttonaction_init();

    /* initiate button executor */
    button_exec_init(noti_script);

    /* initiate evdev */
    evdev_collection_init(evdev_event_callback);


    /* synchronise key bitmap */
    evdev_collection_update_key_bitmap();


    nfds = -1;

    while (!term) {

        /* initiate timeout */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        /* prepare fd set */
        FD_ZERO(&fdr);
        evdev_collection_set_fdset(&nfds, &fdr);

        /* select */
        stat = select(1 + nfds, &fdr, NULL, NULL, &timeout);

        /* update clock and process timer */
        timer_update_current_msec();
        timer_timer_process();

        /* if there is an error */
        if (stat < 0) {
            if (errno != EINTR) {
                syslog(LOG_ERR, "failed from select() - %s", strerror(errno));
                break;
            }
        }
        /* if there are events */
        else if (stat > 0) {
            evdev_collection_process(&fdr);
        }
    }

fini:
    /* destory evdev */
    evdev_collection_fini();

    /* destory button executor */
    button_exec_fini();

    /* destory button action */
    buttonaction_fini();

    /* destory timer */
    timer_fini();

    return 0;
}
