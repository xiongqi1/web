#ifndef __TIMER_H_20181025_
#define __TIMER_H_20181025_

/*
 * Timer.
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

#include <stdint.h>
#include <time.h>
#define __USE_GNU
#include <sys/time.h>

typedef uint32_t timer_monotonic_t;
struct timer_entry_t;

typedef void (*timer_timeout_callback_t)(struct timer_entry_t* te, timer_monotonic_t now_msec, void* ref);

timer_monotonic_t timer_get_msec_from_ts(struct timespec *ts);
timer_monotonic_t timer_get_msec_from_tv(struct timeval *tv);
timer_monotonic_t timer_get_current_msec(void);
int timer_cancel_by_entry(struct timer_entry_t* te);
int timer_cancel(const char* timer_name);
struct timer_entry_t* timer_set(const char* timer_name, int timeout_msec, timer_timeout_callback_t timeout_cb,
                                void* ref);
void timer_process_cb(struct timer_entry_t *te, void *ref);
void timer_timer_process(void);
void timer_init(void);
void timer_fini(void);
void timer_update_current_msec(void);
int timer_is_set(const char* timer_name);
#endif
