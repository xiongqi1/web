//
// Part of GEM daemon
// Code that handles timer triggers
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <sys/times.h>
#include "gem_api.h"
#include "gem_daemon.h"


static int timer_fd = -1; // timer file descriptor

//
// A helper to arm out one and only 1-second timer
//
static int gem_timer_set_time(const struct itimerspec *new_value)
{
    return timerfd_settime(timer_fd, 0, new_value, NULL);
}

//
// Initialize timer module
//
// Return 0 on success, -1 on error
//
int gem_timer_init(void)
{
    struct itimerspec timeout_onesec;

    timeout_onesec.it_interval.tv_sec = 1;
    timeout_onesec.it_interval.tv_nsec = 0;
    timeout_onesec.it_value.tv_sec = 1;
    timeout_onesec.it_value.tv_nsec = 0;

    timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd == -1)
    {
        gem_syslog(LOG_CRIT, "Could not create timer fd");
        return -1;
    }

    // start the one and only timer that kicks all our gem timers
    return gem_timer_set_time(&timeout_onesec);
}

//
// Close timer file descriptor
//
void gem_timer_close(void)
{
    if (timer_fd >= 0)
    {
        (void)close(timer_fd);
    }
    timer_fd = -1;

}

//
// Helper to get the timer fd
//
int gem_timer_get_fd(void)
{
    return timer_fd;
}

//
// Top level function called from GEM's control loop.
// Process 1-second timer expiration event.
//
// Called when select on timer's fd returns >0
// which will happen every second!
//
// Timer trigger table contains remaining value of
// each timer in seconds.
//
// Very simple processing where each entry in timer trigger
// table is decremented. If it reaches zero, a corresponding
// EE is triggered.
//
// Could have implemented as delta queue but questionable if
// it is necessarily more efficient.
//
int gem_process_triggers_timer(void)
{
    long long foo;

    int fd = gem_timer_get_fd();


    int i;
    gem_timer_trigger_table_elem *tte;

    if (fd < 0)
    {
        gem_syslog(LOG_CRIT,"Timer get fd returns %d", fd);
        return -1;
    }

    // clear the timer so select will no longer return immediately
    if (read(fd, &foo, sizeof(foo)) < 0)
    {
        gem_syslog(LOG_CRIT,"Timer read fd returns error");
        return -1;
    }

    for (i = 0 ; i < timer_triggers.count ; i++)
    {
        tte = &timer_triggers.table[i];

        if (tte->timer_props.it_value.tv_sec)
        {
            tte->timer_props.it_value.tv_sec--;
            if (tte->timer_props.it_value.tv_sec == 0)
            {
                // reload timer value from the interval
                tte->timer_props.it_value.tv_sec = tte->timer_props.it_interval.tv_sec;

                if ((tte->ee_num != -1) && (tte->ee_num < gem_ee_table.count))
                {
                    // set the EE to run next time
                    gem_ee_table.ee[tte->ee_num].trigger_flag = TRUE;
                }
                else
                {
                    gem_syslog(LOG_ERR, "EE number wrong %d in timer trigger %d",
                               tte->ee_num, i);
                }
            }
        }
    }
    return 0;
}
