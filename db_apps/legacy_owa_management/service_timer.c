/**
 * @file service_timer.c
 * @brief provide 5s , 30s and adjustable timer for legacy management service
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

#define _POSIX_C_SOURCE 199309
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include "message_protocol.h"
#include "com_interface.h"
#include "service_timer.h"
#include "manage_app.h"
#include "nc_util.h"

#define CLOCKID CLOCK_REALTIME

#ifdef USE_POSIX_TIMER
static timer_t bat_rep_timer, keep_alive_timer, response_timer;

int service_timer_init(struct sigevent *sigevt)
{
    sigevt->sigev_value.sival_ptr = &bat_rep_timer;
    if ( timer_create(CLOCKID, sigevt, &bat_rep_timer) == -1 ) {
            return -1;
    }
    sigevt->sigev_value.sival_ptr = &keep_alive_timer;
    if ( timer_create(CLOCKID, sigevt, &keep_alive_timer) == -1 ) {
            return -1;
    }
    sigevt->sigev_value.sival_ptr = &response_timer;
    if ( timer_create(CLOCKID, sigevt, &response_timer) == -1 ) {
            return -1;
    }
    return 0;
}

void timer_handler(int signum, siginfo_t *siginfo, void *uc)
{
    state_event_t evt;
    long timer_id_by_addr = 0;
    timer_id_by_addr = (long)siginfo->si_value.sival_ptr;
    if (timer_id_by_addr) {
        if (timer_id_by_addr == (long) &bat_rep_timer) {
            send_msg_battery_ind();
        } else if (timer_id_by_addr == (long) &keep_alive_timer) {
            send_msg_keepalive();
        } else if (timer_id_by_addr == (long) &response_timer) {
            evt.event = RESPONSE_TIMEOUT;
            if(set_state_event(evt)==-1) BLOG_ERR("Set app state machine error\n");
        } else {
            BLOG_ERR("Mgm App Timer signal handler error\n");
        }
    }
}

int start_keepalive_timer()
{
    int ret =0;
    const struct itimerspec start_time = {.it_interval = {0, 0}, \
                                          .it_value = {KEEP_ALIVE_TIME_S, 0} };
    ret = timer_settime(keep_alive_timer, 0, &start_time, NULL);
    return ret;
}
int start_battery_report_timer()
{
    int ret =0;
    const struct itimerspec start_time = {.it_interval = {0,0}, \
                                          .it_value = {BATTERY_REP_TIME_S,0}};
    ret = timer_settime(bat_rep_timer, 0, &start_time, NULL);
    return ret;
}

int start_response_timer(int second)
{
    int ret =0;
    struct itimerspec response_time = {{0,0}, {0,0}};
    response_time.it_value.tv_sec = second;
    ret = timer_settime(response_timer, 0, (const struct itimerspec *) &response_time, NULL);
    return ret;
}

int stop_response_timer()
{
    return start_response_timer(0);
}

void stop_all_timers()
{
    static struct itimerspec stop_time = {{0,0}, {0,0}};
    timer_settime(keep_alive_timer, 0, &stop_time, NULL);
    timer_settime(response_timer, 0, &stop_time, NULL );
    timer_settime(bat_rep_timer, 0, &stop_time, NULL );
}

void service_timer_close()
{
    stop_all_timers();
    timer_delete(keep_alive_timer);
    timer_delete(response_timer);
    timer_delete(bat_rep_timer);
}
#else  //use linux timerfd
static int bat_rep_timer, keep_alive_timer, response_timer;

int service_timer_init(struct sigevent *sigevt)
{
    (void) sigevt;
    if ((bat_rep_timer = timerfd_create(CLOCKID, 0)) == -1 || \
        (keep_alive_timer = timerfd_create(CLOCKID, 0)) == -1 || \
        (response_timer = timerfd_create(CLOCKID, 0)) == -1) {
        return -1;
    }
    return 0;
}

int start_keepalive_timer()
{
    int ret =0;
    const struct itimerspec start_time = {.it_interval = {KEEP_ALIVE_TIME_S, 0}, \
                                          .it_value = {KEEP_ALIVE_TIME_S, 0} };
    ret = timerfd_settime(keep_alive_timer, 0, &start_time, NULL);
    return ret;
}
int start_battery_report_timer()
{
    int ret =0;
    const struct itimerspec start_time = {.it_interval = {BATTERY_REP_TIME_S,0}, \
                                          .it_value = {BATTERY_REP_TIME_S,0}};
    ret = timerfd_settime(bat_rep_timer, 0, &start_time, NULL);
    return ret;
}

int start_response_timer(int second)
{
    int ret =0;
    struct itimerspec response_time = {{0,0}, {0,0}};
    response_time.it_value.tv_sec = second;
    ret = timerfd_settime(response_timer, 0, (const struct itimerspec *) &response_time, NULL);
    return ret;
}

int stop_response_timer()
{
    return start_response_timer(0);
}

void stop_all_timers()
{
    static struct itimerspec stop_time = {{0,0}, {0,0}};
    timerfd_settime(keep_alive_timer, 0, &stop_time, NULL);
    timerfd_settime(response_timer, 0, &stop_time, NULL );
    timerfd_settime(bat_rep_timer, 0, &stop_time, NULL );
}

int get_response_timer_fd()
{
    return response_timer;
}

int get_keepalive_timer_fd()
{
    return keep_alive_timer;
}

int get_battery_report_timer_fd()
{
    return bat_rep_timer;
}

void service_timer_close()
{
    stop_all_timers();
    close(response_timer);
    close(keep_alive_timer);
    close(bat_rep_timer);
}
#endif
