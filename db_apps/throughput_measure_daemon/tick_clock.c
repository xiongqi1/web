/*!
 * Tick clock reads system clock and provides timer.
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

#include "tick_clock.h"

#include <string.h>

static time_ms_t _ms;
static time_t _now;

/**
 * @brief Reads system clock.
 */
void tick_clock_update()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    _ms = (time_ms_t)(ts.tv_nsec / 1000000L) + ((time_ms_t)(ts.tv_sec)) * 1000L;
    _now = time(0);
}

/**
 * @brief Returns Epoch system clock that is read by tick_clock_update().
 *
 * @return Epoch system time in sec.
 */
time_t tick_clock_get_time()
{
    return _now;
}

/**
 * @brief Returns system clock that is read by tick_clock_update().
 *
 * @return system time uptime in msec.
 */
time_ms_t tick_clock_get_ms()
{
    return _ms;
}

/**
 * @brief Triggers a timer.
 *
 * @param tc is a tick clock object.
 */
void tick_clock_do_trigger(struct tick_clock_t* tc)
{
    tc->last_triggered_ms = _ms;
    tc->last_triggered_ms_valid = 1;
}

/**
 * @brief Returns duration since a timer is triggered.
 *
 * @param tc is a tick clock object.
 *
 * @return 0 when the timer is invalid. Otherwise, duration since the timer is triggered.
 */
time_ms_t tick_clock_get_duration(struct tick_clock_t* tc)
{
    return tc->last_triggered_ms_valid ? _ms - tc->last_triggered_ms : 0;
}

/**
 * @brief Checks if a timer is expired.
 *
 * @param tc is a tick clock object.
 *
 * @return true when the timer is expired. Otherwise, false.
 */
int tick_clock_is_expired(struct tick_clock_t* tc)
{
    time_ms_t diff = _ms - tc->last_triggered_ms;
    return tc->last_triggered_ms_valid && !(diff < tc->interval_ms);

}

/**
 * @brief Reprograms a timer.
 *
 * @param tc is a tick clock object.
 * @param interval is a new interval in msec. for the timer.
 */
void tick_clock_reinit(struct tick_clock_t* tc, uint32_t interval_ms)
{
    /* reset members */
    memset(tc, 0, sizeof(*tc));

    tc->interval_ms = interval_ms;
}

/**
 * @brief Initiate a timer to use.
 *
 * @param tc is a tick clock object.
 * @param interval_ms is an interval for the timer.
 */
void tick_clock_init(struct tick_clock_t* tc, uint32_t interval_ms)
{
    tick_clock_reinit(tc, interval_ms);
}

/**
 * @brief Finalize a timer to destroy.
 *
 * @param tc is a tick clock object.
 */
void tick_clock_fini(struct tick_clock_t* tc)
{
}

