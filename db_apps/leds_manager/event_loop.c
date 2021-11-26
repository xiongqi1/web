/*
 * event_loop.c
 * Event loop handling timers and reader file descriptors
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
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
#include "linked_list.h"
#include "event_loop.h"

/* timer in event loop */
struct event_loop_timer {
    struct linked_list list;
    struct timeval timeout;
    unsigned int secs;
    unsigned int usecs;
    void *data;
    event_loop_timer_handler handler;
};
/* structure for a fd being monitored in event loop */
struct event_loop_fd {
    struct linked_list list;
    int fd;
    void *data;
    event_loop_fd_handler handler;
};
/* event loop internal data */
struct event_loop_data {
    int max_fd;
    fd_set fdset;
    struct linked_list fd_list;
    int fd_list_changed;
    struct linked_list timer_list;
    volatile int terminate;
};
static struct event_loop_data event_loop;

/*
 * signal handler which terminates event loop
 * @param sig signal to be handled
 */
static void event_loop_terminate(int sig)
{
    event_loop.terminate = 1;
}

/*
 * Initialise event loop. See event_loop.h
 */
int event_loop_init(void)
{
    memset(&event_loop, 0, sizeof(event_loop));
    linked_list_init(&event_loop.timer_list);
    linked_list_init(&event_loop.fd_list);

    /* register signal handler */
    if (signal(SIGINT, event_loop_terminate) == SIG_ERR
            || signal(SIGQUIT, event_loop_terminate) == SIG_ERR
            || signal(SIGTERM, event_loop_terminate) == SIG_ERR)
    {
        return -errno;
    }

    return 0;
}

/*
 * free event loop.
 * Precondition: event loop must not be running.
 */
void event_loop_free(void)
{
    struct event_loop_timer *p_timer, *p_timer_saved;
    struct event_loop_fd *p_fd, *p_fd_saved;

    linked_list_for_each_safe(p_timer, p_timer_saved, &event_loop.timer_list, struct event_loop_timer, list) {
        linked_list_del(&p_timer->list);
        free(p_timer);
    }

    linked_list_for_each_safe(p_fd, p_fd_saved, &event_loop.fd_list, struct event_loop_fd, list) {
        linked_list_del(&p_fd->list);
        free(p_fd);
    }
}

/*
 * add a fd to fd list. See event_loop.h
 */
int event_loop_add_fd(int fd, event_loop_fd_handler handler, void *data)
{
    struct event_loop_fd *s_fd, *fd_iter;

    if (fd<=0){
        return -EINVAL;
    }

    /* check whether fd is already in the list */
    linked_list_for_each(fd_iter, &event_loop.fd_list, struct event_loop_fd, list) {
        if (fd_iter->fd == fd){
            /* update attributes */
            fd_iter->data = data;
            fd_iter->handler = handler;
            return 0;
        }
    }

    /* fd is not in the list, create and add */
    s_fd = (struct event_loop_fd*)malloc(sizeof(*s_fd));
    if (!s_fd){
        return -ENOMEM;
    }
    s_fd->fd = fd;
    s_fd->data = data;
    s_fd->handler = handler;

    linked_list_add_head(&event_loop.fd_list, &s_fd->list);

    if (fd > event_loop.max_fd){
        event_loop.max_fd = fd;
    }

    FD_SET(fd, &event_loop.fdset);
    event_loop.fd_list_changed = 1;

    return 0;
}

/*
 * delete a fd from fd list. See event_loop.h
 */
int event_loop_del_fd(int fd)
{
    struct event_loop_fd *fd_iter;
    int found = 0;

    if (fd<=0){
        return -EINVAL;
    }
    /* find and delete fd */
    linked_list_for_each(fd_iter, &event_loop.fd_list, struct event_loop_fd, list) {
        if (fd_iter->fd == fd){
            linked_list_del(&fd_iter->list);
            FD_CLR(fd_iter->fd, &event_loop.fdset);
            free(fd_iter);
            found = 1;
            break;
        }
    }
    if (found){
        event_loop.fd_list_changed = 1;
        /* update max_fd */
        if (event_loop.max_fd == fd){
            event_loop.max_fd = 0;
            linked_list_for_each(fd_iter, &event_loop.fd_list, struct event_loop_fd, list) {
                if (fd_iter->fd > event_loop.max_fd){
                    event_loop.max_fd = fd_iter->fd;
                }
            }
        }
        return 0;
    }
    else {
        return -ENOENT;
    }
}

/*
 * get monotonic clock time in timeval struct
 *
 * @param timeval_result output timeval
 *
 * @return 0 if success, negative error code otherwise
 */
static int event_loop_get_monotonic_timeval(struct timeval *timeval_result)
{
    struct timespec timespec_result;

    if (clock_gettime(CLOCK_MONOTONIC, &timespec_result)) {
        return -errno;
    }
    timeval_result->tv_sec = timespec_result.tv_sec;
    timeval_result->tv_usec = timespec_result.tv_nsec/1000;
    return 0;
}

/*
 * reschedule a timer
 *
 * @param timer timer to be reschedule and added if necessary
 *
 * @return 0 if success, negative error code otherwise
 */
static int event_loop_reschedule_timer(struct event_loop_timer *timer)
{
    struct event_loop_timer *tmp;
    time_t sec_now;
    int retval;

    if (timer == NULL){
        return -EINVAL;
    }

    retval = event_loop_get_monotonic_timeval(&timer->timeout);
    if (retval) {
        return retval;
    }

    sec_now = timer->timeout.tv_sec;
    timer->timeout.tv_sec += timer->secs;
    timer->timeout.tv_usec += timer->usecs;
    while (timer->timeout.tv_usec >= 1000000) {
        timer->timeout.tv_sec++;
        timer->timeout.tv_usec -= 1000000;
    }

    if (timer->timeout.tv_sec < sec_now){
        /* integer overflow */
        return -ERANGE;
    }

    /* reschedule timers in order of increasing time */
    linked_list_for_each(tmp, &event_loop.timer_list, struct event_loop_timer, list) {
        if (timercmp(&timer->timeout, &tmp->timeout, <)) {
            linked_list_add_tail(&tmp->list, &timer->list);
            return 0;
        }
    }
    linked_list_add_tail(&event_loop.timer_list, &timer->list);

    return 0;
}

/*
 * add a timer. See event_loop.h.
 */
int event_loop_add_timer(unsigned int secs, unsigned int usecs, event_loop_timer_handler handler, void *data)
{
    struct event_loop_timer *timer, *iter;
    int res;

    linked_list_for_each(iter, &event_loop.timer_list, struct event_loop_timer, list) {
        if (iter->handler == handler && iter->data == data){
            return -EEXIST;
        }
    }

    timer = (struct event_loop_timer *)malloc(sizeof(*timer));
    if (timer == NULL){
        return -ENOMEM;
    }
    /* set attributes */
    timer->handler = handler;
    timer->data = data;
    timer->secs = secs;
    timer->usecs = usecs;
    /* add and schedule timer */
    res = event_loop_reschedule_timer(timer);
    if (res){
        free(timer);
        return res;
    }
    else{
        return 0;
    }
}

/*
 * delete a timer. See event_loop.h.
 */
int event_loop_del_timer(event_loop_timer_handler handler, void *data)
{
    struct event_loop_timer *iter;

    if (!handler){
        return -EINVAL;
    }

    linked_list_for_each(iter, &event_loop.timer_list, struct event_loop_timer, list) {
        if (iter->handler == handler && iter->data == data){
            linked_list_del(&iter->list);
            free(iter);
            return 0;
        }
    }

    return -ENOENT;
}

/*
 * event loop run. See event_loop.h.
 */
int event_loop_run(void)
{
    int res;
    fd_set fdset;
    struct timeval now, select_timeout;
    struct timeval *p_select_timeout;
    struct event_loop_timer *timer;
    struct event_loop_fd *iter, *iter_save;
    event_loop_timer_handler timer_handler;
    void *timer_data;

    while (!event_loop.terminate &&
            (!linked_list_empty(&event_loop.timer_list) || !linked_list_empty(&event_loop.fd_list))) {
        /* process first timer */
        timer = linked_list_first(&event_loop.timer_list, struct event_loop_timer, list);
        if (timer) {
            /* get current time */
            res = event_loop_get_monotonic_timeval(&now);
            if (res) {
                return res;
            }
            /* check whether timer has expired */
            if (timercmp(&timer->timeout, &now, >)) {
                /* timer has not expired, calculate timeout for select */
                timersub(&timer->timeout, &now, &select_timeout);
                p_select_timeout = &select_timeout;
            }
            else{
                /* timer has expired: invoke timer handler, then continue */
                timer_handler = timer->handler;
                timer_data = timer->data;
                linked_list_del(&timer->list);
                free(timer);
                timer_handler(timer_data);
                continue;
            }
        }
        else {
            p_select_timeout = NULL;
        }

        /* block on fd set */
        fdset = event_loop.fdset;
        res = select(event_loop.max_fd ? event_loop.max_fd + 1 : 0, event_loop.max_fd ? &fdset : NULL, NULL, NULL, p_select_timeout);
        if (res < 0){
            return -errno;
        }
        else if (res > 0){
            /* process fd now, use linked_list_for_each_safe as fd can be deleted by handler */
            event_loop.fd_list_changed = 0;
            linked_list_for_each_safe(iter, iter_save, &event_loop.fd_list, struct event_loop_fd, list){
                if (FD_ISSET(iter->fd, &fdset)){
                    /* invoke handler */
                    iter->handler(iter->fd, iter->data);
                }
                /* if handler have changed fd list, break the handler dispatching loop to turn back to select */
                if (event_loop.fd_list_changed){
                    break;
                }
            }
        }
        else {
            /* timeout, do nothing as timers, if any, will be handled in next loop run */
        }
    }

    return 0;
}
