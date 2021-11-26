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


#include "timer.h"
#include "uthash.h"

#include <syslog.h>
#include <stdlib.h>

typedef void (*timer_walk_through_callback_t)(struct timer_entry_t* te, void* ref);

struct timer_entry_t {
    UT_hash_handle hh;

    timer_monotonic_t start_msec;
    int timeout_msec;

    char* name;

    timer_timeout_callback_t timeout_cb;
    void* ref;
};

static struct timer_entry_t* _timer_head = NULL;
static timer_monotonic_t _now;


/**
 * @brief gets monotonic clock value from timespec structure.
 *
 * @param ts is a pointer to timespec structure.
 *
 * @return monotonic clock value.
 */
timer_monotonic_t timer_get_msec_from_ts(struct timespec* ts)
{
    return (timer_monotonic_t)(ts->tv_sec * 1000 + ts->tv_nsec / 1000000);
}

/**
 * @brief gets monotonic clock value from timeval structure.
 *
 * @param tv is a pointer to timeval structure.
 *
 * @return monotonic clock value.
 */
timer_monotonic_t timer_get_msec_from_tv(struct timeval* tv)
{
    struct timespec ts;
    TIMEVAL_TO_TIMESPEC(tv, &ts);

    return timer_get_msec_from_ts(&ts);
}

/**
 * @brief gets current monotonic clock value.
 *
 * @return  monotonic clock value.
 */
timer_monotonic_t timer_get_current_msec(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return timer_get_msec_from_ts(&ts);
}

/**
 * @brief updates current monotonic clock value.
 */
void timer_update_current_msec(void)
{
    _now = timer_get_current_msec();
}


/**
 * @brief walks through each of timers.
 *
 * @param cb is call-back.
 * @param ref is a reference.
 */
static void timer_walk_through(timer_walk_through_callback_t cb, void* ref)
{
    struct timer_entry_t* ent;
    struct timer_entry_t* tmp;

    HASH_ITER(hh, _timer_head, ent, tmp) {
        cb(ent, ref);
    }
}

/**
 * @brief frees a timer entry.
 *
 * @param te is a timer entry.
 */
static void timer_free_timer_entry(struct timer_entry_t* te)
{
    free(te->name);
    free(te);
}

/**
 * @brief cancels a timer by pointer.
 *
 * @param te is a timer entry.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int timer_cancel_by_entry(struct timer_entry_t* te)
{
    HASH_DEL(_timer_head, te);
    timer_free_timer_entry(te);

    return 0;
}

/**
 * @brief cancels a timer by name.
 *
 * @param te is a timer entry.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int timer_cancel(const char* timer_name)
{
    struct timer_entry_t* te;

    HASH_FIND_STR(_timer_head, timer_name, te);

    if (!te) {
        goto err;
    }

    timer_cancel_by_entry(te);

err:
    return -1;
}

/**
 * @brief checks to see if timer exists.
 *
 * @param timer_name is timer's name.
 *
 * @return TRUE when the timer exists. Otherwise, FALSE.
 */
int timer_is_set(const char* timer_name)
{
    struct timer_entry_t* te;

    HASH_FIND_STR(_timer_head, timer_name, te);

    return te != NULL;
}

/**
 * @brief schedules a timer.
 *
 * @param timer_name is timer's name.
 * @param timeout_msec is time-out.
 * @param timeout_cb is call-back.
 * @param ref is a reference pointer.
 *
 * @return timer entry pointer when it succeeds. Otherwise, NULL.
 */
struct timer_entry_t* timer_set(const char* timer_name, int timeout_msec, timer_timeout_callback_t timeout_cb,
                                void* ref)
{
    struct timer_entry_t* te;

    HASH_FIND_STR(_timer_head, timer_name, te);

    if (!te) {
        /* allocate timer entry */
        te = calloc(1, sizeof(*te));
        if (!te) {
            syslog(LOG_ERR, "failed to allocate timer entry");
            goto err;
        }

        /* set timer name */
        te->name = strdup(timer_name);
        if (!te->name) {
            syslog(LOG_ERR, "failed to allocate timer name (timer_name=%s)", timer_name);
            goto err;
        }

        /* add to hash */
        HASH_ADD_STR(_timer_head, name, te);

        syslog(LOG_DEBUG, "[timer] timer is set (name=%s,timeout=%d,now=%u)", te->name, timeout_msec, _now);
    } else {
        syslog(LOG_DEBUG, "[timer] reprogram timer (name=%s,timeout=%d,now=%u)", te->name, timeout_msec, _now);
    }

    /* update members */
    te->ref = ref;
    te->start_msec = _now;
    te->timeout_msec = timeout_msec;
    te->timeout_cb = timeout_cb;

    return te;

err:
    timer_free_timer_entry(te);
    return NULL;
}

/**
 * @brief is call-back function to process a timer.
 *
 * @param te is timer entry.
 * @param ref is reference pointer.
 */
void timer_process_cb(struct timer_entry_t* te, void* ref)
{
    /* if expired */
    if (_now - te->start_msec >= te->timeout_msec) {
        HASH_DEL(_timer_head, te);

        syslog(LOG_DEBUG, "[timer] timer is expired (name=%s,timeout=%d,now=%u)", te->name, te->timeout_msec, _now);
        te->timeout_cb(te, _now, te->ref);

        timer_free_timer_entry(te);
    }
}

/**
 * @brief process all timers.
 */
void timer_timer_process(void)
{
    timer_walk_through(timer_process_cb, NULL);
}

/**
 * @brief initiates timer object.
 */
void timer_init(void)
{
    timer_update_current_msec();
}

/**
 * @brief deletes timer.
 *
 * @param te
 * @param ref
 */
static void timer_fini_cb(struct timer_entry_t* te, void* ref)
{
    HASH_DEL(_timer_head, te);
    timer_free_timer_entry(te);
}

/**
 * @brief finalize timer.
 */
void timer_fini(void)
{
    timer_walk_through(timer_fini_cb, NULL);
}

