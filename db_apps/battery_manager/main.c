/**
 * @file main.c
 * @brief main entry for battery manager
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "utils.h"
#include "battery_rdb.h"
#include "charger.h"
#include "monitor.h"

#define POLL_INTERVAL_SECS 1
#define WATCHDOG_INTERVAL_SECS 10
#define WATCHDOG_KICK (WATCHDOG_INTERVAL_SECS/POLL_INTERVAL_SECS)
#define TURN_OFF_DELAY_SECS 60
#define NEAR_FULL_THRESHOLD 95
#define LOW_CHARGE_CURRENT_MA 100
#define LOW_CHARGE_ALLOWED_TIME_SECS  (1*60*60)
#define AUTO_TURNOFF_DELAY_SEC  5

#define BATTERY_WORKING_TEMP_MAX 55
#define BATTERY_WORKING_TEMP_MIN -10

#define BATTERY_CHARGING_TEMP_MAX 45
#define BATTERY_CHARGING_TEMP_MIN 0

#define MAX_ERRORS 5

volatile static int terminate = 0;

static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
}

int main_loop(void)
{

    struct charge_stats ch_stats;
    struct monitor_stats mo_stats;
    uint8_t charging = 0;
    int err_cnt = 0;
    int wdog_cnt = 0;
    int turn_off_delay = 0;

    int rdbfd;
    fd_set fdset;
    struct timeval timeout;
    int ret;

    int charge_terminated = 0, fully_charged = 0;
    time_t terminated_time, low_current_time, discharging_time, now;

    rdbfd = get_rdb_fd();
    if (rdbfd < 0) {
        BLOG_ERR("Failed to get rdb_fd\n");
        return -1;
    }

    // align charger IC status with variable charging
    if (disable_charge() < 0) {
        BLOG_ERR("Failed to disable charging\n");
    }

    time(&low_current_time);
    time(&discharging_time);

    while (!terminate) {
        if (++wdog_cnt >= WATCHDOG_KICK) {
            // it is time to kick watchdog
            if (charger_kick_wdog() < 0) {
                BLOG_ERR("Failed to kick watchdog\n");
                goto err_handle;
            } else {
                wdog_cnt = 0;
                BLOG_DEBUG("Kicked watchdog\n");
            }
        }

        if (charger_get_stats(&ch_stats) < 0) {
            BLOG_ERR("Failed to get charger stats\n");
            goto err_handle;
        }

        time(&now);
        if ((fully_charged || ch_stats.charge_stat == CHARGE_STAT_CHGDONE) &&
            ch_stats.power_good && !is_owa_connected()) {
            // once battery is fully charged, wait for 60 seconds and then
            // set charger into hiz, disable watchdog and turn off power
            if(!charge_terminated) {
                time(&terminated_time);
                charge_terminated = 1;
            } else {
                if(difftime(now, terminated_time) > TURN_OFF_DELAY_SECS) {
                    initiate_poweroff(0);
                }
            }

        } else {
            charge_terminated = 0;
        }

        if (!charging && ch_stats.power_good && !is_owa_connected()) {
            // enable charging when charger is plugged in and OWA is disconnected
            BLOG_NOTICE("charger plugged in & OWA disconnected: enable charging\n");
            if (enable_charge() < 0) {
                BLOG_ERR("Failed to enable charging\n");
                goto err_handle;
            }
            charging = 1;
        } else if (charging && (!ch_stats.power_good || is_owa_connected())) {
            // disable charging if charger is unplugged or OWA is connected
            BLOG_NOTICE("charger unplugged or OWA connected: disable charging\n");
            if (disable_charge() < 0) {
                BLOG_ERR("Failed to disable charging\n");
                goto err_handle;
            }
            charging = 0;
        }

        BLOG_DEBUG("charge_stat=%d, vbus_stat=%d, power_good=%d, fault_ntc=%d\n",
                   ch_stats.charge_stat, ch_stats.vbus_stat, ch_stats.power_good, ch_stats.fault_ntc);
        if (update_charge_rdbs(&ch_stats) < 0) {
            BLOG_ERR("Failed to update charge RDBs\n");
        }

        if (monitor_get_stats(&mo_stats, ch_stats.fault_ntc) < 0) {
            BLOG_WARNING("Failed to get monitor stats. Battery is flat?\n");
        } else {
            if((fully_charged || ch_stats.charge_stat == CHARGE_STAT_CHGDONE) &&
               mo_stats.percentage < NEAR_FULL_THRESHOLD) {
                BLOG_DEBUG("percent rectify: %d -> %d\n", mo_stats.percentage, NEAR_FULL_THRESHOLD);
                mo_stats.percentage = NEAR_FULL_THRESHOLD;
            }

            BLOG_DEBUG("volt=%d, curr=%d, cap=%d, per=%d, full=%d, temp=%d, temp_valid=%d\n",
                       mo_stats.voltage, mo_stats.current, mo_stats.capacity,
                       mo_stats.percentage, mo_stats.full_capacity,
                       mo_stats.temperature, mo_stats.temp_valid);
            if (update_monitor_rdbs(&mo_stats) < 0) {
                BLOG_ERR("Failed to update monitor RDBs\n");
            }

            if ( ((mo_stats.temperature >= BATTERY_WORKING_TEMP_MAX) || (mo_stats.temperature <= BATTERY_WORKING_TEMP_MIN)) &&
                mo_stats.temp_valid ) {
                BLOG_ERR("Power off - temperature out of range\n");
                initiate_poweroff(1);
            }

            // When the battery level drops to 0%, count for AUTO_TURNOFF_DELAY_SEC waiting
            // io_manager to turn off ODU's power supply, and then turn off itself. A slight voltage
            // rise might be observed after ODU power supply is stopped.
            if ( !ch_stats.power_good && mo_stats.percentage == 0 ) {
                turn_off_delay++;
            }
            else {
                turn_off_delay = 0;
            }
            if ( turn_off_delay > AUTO_TURNOFF_DELAY_SEC ) {
                BLOG_ERR("Power off - battery level is zero\n");
                initiate_poweroff(1);
            }

            if(!ch_stats.power_good || mo_stats.current > 0 || is_owa_connected()) {
                discharging_time = now;
            }
            else if(difftime(now, discharging_time) > TURN_OFF_DELAY_SECS) {
                BLOG_ERR("Power off - discharging with power supply\n");
                initiate_poweroff(1);
            }

            if(mo_stats.current >= LOW_CHARGE_CURRENT_MA || !ch_stats.power_good) {
                low_current_time = now;
                fully_charged = 0;
            }
            else if(difftime(now, low_current_time) > LOW_CHARGE_ALLOWED_TIME_SECS) {
                fully_charged = 1;
            }
        }
        err_cnt = 0;

        if (ch_stats.power_good && !mo_stats.temp_valid &&
            (FAULT_NTC_HOT == ch_stats.fault_ntc || FAULT_NTC_COLD == ch_stats.fault_ntc)) {
            BLOG_ERR("Power off - JEITA state is hot/cold\n");
            initiate_poweroff(1);
        }

        if (ch_stats.power_good && mo_stats.temp_valid &&
            (mo_stats.temperature >= BATTERY_CHARGING_TEMP_MAX || mo_stats.temperature <= BATTERY_CHARGING_TEMP_MIN)) {
            BLOG_ERR("Power off - temperature out of range while charging \n");
            initiate_poweroff(1);
        }

        // wait for rdb trigger
        FD_ZERO(&fdset);
        FD_SET(rdbfd, &fdset);
        timeout.tv_sec = POLL_INTERVAL_SECS;
        timeout.tv_usec = 0;
        ret = select(rdbfd + 1, &fdset, NULL, NULL, &timeout);
        if (ret < 0) { // error
            if (errno == EINTR) { // normal exit
                return 0;
            }
            BLOG_ERR("select returned %d, errno: %d\n", ret, errno);
            goto err_handle;
        } else if (ret > 0) { // rdb triggered
            if (poweroff_rdb_set()) {
                set_persisted_level(mo_stats.percentage);
                BLOG_DEBUG("battery level saved: %d", mo_stats.percentage);
                sleep(3); // wait 3 seconds giving enough time for the rdb persistence to be accomplished.

                poweroff();
                poweroff_rdb_finalise();
                return 0;
            }
        } else { // timeout. nothing to do
        }

        continue;

    err_handle:
        err_cnt++;
        if (err_cnt > MAX_ERRORS) {
            BLOG_ERR("%d consecutive errors occurred. Quiting\n", err_cnt);
            return -1;
        }
        sleep(POLL_INTERVAL_SECS);
    }

    return 0;
}

int main(void)
{
    int ret = 0;

    openlog("battman", LOG_CONS, LOG_USER);

    ret = init_rdb();
    if (ret) {
        BLOG_ERR("Failed to init rdb\n");
        goto fin_rdb;
    }

    ret = charger_init();
    if (ret) {
        BLOG_ERR("Failed to initialise charger\n");
        goto fin_charger;
    }

    ret = monitor_init();
    if (ret) {
        /*
         * Monitor chip is powered by Vbatt.
         * If battery is absent or flat, monitor chip might not respond
         */
        BLOG_WARNING("Failed to initialise monitor. Battery is flat?\n");
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        ret = -errno;
        BLOG_ERR("Failed to register signal handler\n");
        goto fin_monitor;
    }

    ret = main_loop();
    if (ret < 0) {
        BLOG_ERR("main loop terminated unexpectedly\n");
    } else {
        BLOG_NOTICE("main loop exited normally\n");
    }

fin_monitor:
    monitor_term();
fin_charger:
    charger_term();
fin_rdb:
    term_rdb();
    closelog();

    return ret;
}
