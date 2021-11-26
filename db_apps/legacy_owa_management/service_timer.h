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
#ifndef SERVICE_TIMER_H_11020009122019
#define SERVICE_TIMER_H_11020009122019

#include <signal.h>

#define TIMER_NUMBERS               3
#define BATTERY_REP_TIME_S          10
#define KEEP_ALIVE_TIME_S           5
#define CONNECTION_LOST_TIME_S      15

/*
 * timer initialize
 * @param sigevt pointer to struct sigevent to user identify which timer trigger signal
 * @return 0 on success; non-zero error code on failure
 */
int service_timer_init(struct sigevent *sigevt);


/* start keep alive timer
 *
 * @return 0 on success; non-zero error code on failure
 */
int start_keepalive_timer();

/* start battery report timer
 *
 * @return 0 on success; non-zero error code on failure
 */
int start_battery_report_timer();

/*
 * start response timer
 *
 * @param second set up the seconds in next expire time
 * @return 0 on success; non-zero error code on failure
 */
int start_response_timer(int second);

/* stop response timer
 *
 * @return 0 on success; non-zero error code on failure
 */
int stop_response_timer();

/* stop all timer
 * it will stop keep alive timer, battery report timer and response timer
 */
void stop_all_timers();

/* stop and delete all timer resource
 *
 */
void service_timer_close();

#ifdef USE_POSIX_TIMER
/*
 * timer handler
 * @param signum the signal number of the triggered signal
 * @param siginfo pointer of siginfo_t type , stored timer absolute address
 * @param uc a void type pointer, not use
 */
void timer_handler(int signum, siginfo_t *siginfo, void *uc);
#else
int get_response_timer_fd();
int get_keepalive_timer_fd();
int get_battery_report_timer_fd();
#endif

#endif
