/*
 *  Management service process for legacy OWA backward compatible support
 *
 * This daemon will start once OWA is connected. it will try to identify
 * if the OWA connected is legacy serial port OWA by repeat sending hello message
 * to serial port, once get proper feedback from peer OWA. It will set up relevant
 * RDB keys and take legacy management service provided like legacy assistant installation
 * tool such as LED control, battery status report, etc..
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY Casa Systems ``AS IS''
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include "message_protocol.h"
#include "service_timer.h"
#include "com_interface.h"
#include "manage_app.h"
#include "rdb_help.h"
#include "nc_util.h"

#define TIMER_SIG SIGUSR1

volatile static int terminate = 0;

/* exit signal handler
 * to notice the process to exit and release all resources
*/
static void sig_handler(int signum, siginfo_t *siginfo, void *uc)
{
    (void)signum;
    (void)siginfo;
    (void)uc;
    terminate = 1;
}

/*
 * initialize communication interface, timer service and etc..
*/
static void main_init(){
    struct sigaction  sa;
    struct sigevent sigevt;

#ifdef USE_POSIX_TIMER
    /* Connect the timer handler to timer notification signal */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(TIMER_SIG, &sa, NULL) == -1)
    {
       BLOG_ERR("Failed to set up signal handler\n");
       exit(EXIT_FAILURE);
    }
    sigevt.sigev_signo = TIMER_SIG;
    sigevt.sigev_notify = SIGEV_SIGNAL;
#endif
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sig_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1  ||
        sigaction(SIGQUIT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        BLOG_ERR("Failed to register signal handler\n");
        exit(EXIT_FAILURE);
    }

    /* Ignore SIGPIPE, let fifo write side to determine the peer side fifo status */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        BLOG_ERR("Failed to register timer handler\n");
        exit(EXIT_FAILURE);
    }
    service_timer_init(&sigevt);
    rdb_init();
    com_channels_init();
}


int main(int argc, char *argv[])
{
    int fd_fifo_read, fd_response_timer, fd_keepalive_timer, fd_battery_report_timer, fd_rdb;
    int nfds=0;
    fd_set readfds;
    int sel_status;
    char msg[1024];
    int msg_len = 0;
    int ret = 0;
    uint64_t timer_count = 0;
    state_event_t evt;

    openlog("legacy_owa_mgm", LOG_CONS, LOG_USER);
    main_init();
    fd_fifo_read = com_get_read_fd();
    nfds = MAX(nfds, fd_fifo_read);
    fd_response_timer = get_response_timer_fd();
    nfds = MAX(nfds, fd_response_timer);
    fd_keepalive_timer = get_keepalive_timer_fd();
    nfds = MAX(nfds, fd_keepalive_timer);
    fd_battery_report_timer = get_battery_report_timer_fd();
    nfds = MAX(nfds, fd_battery_report_timer);
    fd_rdb = rdb_getfd();
    nfds = MAX(nfds, fd_rdb);

    while (!terminate) {
        service_state_run();
        FD_ZERO(&readfds);
        FD_SET(fd_fifo_read, &readfds);
        FD_SET(fd_response_timer, &readfds);
        FD_SET(fd_keepalive_timer, &readfds);
        FD_SET(fd_battery_report_timer, &readfds);
        FD_SET(fd_rdb, &readfds);

        sel_status = select(nfds+1, &readfds, NULL, NULL,NULL );
        if (sel_status == -1) {
            if (errno == EINTR) {
                /*interrupt by timer or some other signals*/
            } else {
                BLOG_DEBUG("abnormal exit");
                ret = -1;
                break;
            }
        } else if (sel_status) {
            BLOG_DEBUG("%d fd triggered\n",sel_status);
            if (FD_ISSET(fd_rdb, &readfds) && rdb_shutting_down()) {
                BLOG_DEBUG("abnormal exit - power off");
                ret = 0;
                break;
            }

            if (FD_ISSET(fd_response_timer, &readfds)) {
                if(read(fd_response_timer, &timer_count, sizeof(uint64_t)) != -1) {
                    evt.event = RESPONSE_TIMEOUT;
                    if(set_state_event(evt)==-1) BLOG_ERR("Set app state machine error\n");
                }
            }
            if (FD_ISSET(fd_fifo_read, &readfds)) {
                msg_len = read_message((char *)&msg);
                if (msg_len == 0) {
                    /* readable pipe with read of size zero is closed on the write side */
                    BLOG_DEBUG("exit: pipe is closed");
                    ret = 0;
                    break;
                }
                if (msg_len >0) {
                    process_message((char *)&msg, msg_len);
                }
            }
            if (FD_ISSET(fd_keepalive_timer, &readfds)) {
                if(read(fd_keepalive_timer, &timer_count, sizeof(uint64_t)) != -1) {
                    send_msg_keepalive();
                }
            }
            if (FD_ISSET(fd_battery_report_timer, &readfds)) {
                if (read(fd_battery_report_timer, &timer_count, sizeof(uint64_t)) != -1) {
                    send_msg_battery_ind();
                }
            }
        }
    }
    com_channels_close();
    rdb_cleanup();
    service_timer_close();
    closelog();
    if (ret == -1) {
        exit(EXIT_FAILURE);
    } else {
        exit(EXIT_SUCCESS);
    }
}
