/**
 * @file battery_rdb.c
 * @brief RDB utilities for battery manager
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
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "battery_rdb.h"

#define RDB_CHARGE_STAT "system.battery.charging_state"
#define RDB_VBUS_STAT "system.battery.vbus_state"
#define RDB_POWER_GOOD "system.battery.dcin_present"
#define RDB_VOLTAGE "system.battery.voltage"
#define RDB_CURRENT "system.battery.current"
#define RDB_CAPACITY_MAH "system.battery.capacity_mah"
#define RDB_CAPACITY "system.battery.capacity"
#define RDB_FULL_CAPACITY_MAH "system.battery.full_capacity_mah"
#define RDB_TEMPERATURE "system.battery.temperature"

#define RDB_POWEROFF "service.system.poweroff"
#define RDB_OWA_CONNECTED "owa.connected"
#define RDB_BATTERY_DIDT "system.battery.didt"
#define RDB_CAPACITY_LAST "system.battery.capacity.last"

#define RDB_SET_CHECK(rdbk, val) do {               \
        int ret = rdb_set_string(rdb_s, rdbk, val); \
        if (ret) { \
            BLOG_ERR("Failed to set RDB %s\n", rdbk); \
            return ret; \
        } \
    } while(0)

#define RDB_SET_INT_CHECK(rdbk, buf, ival) do { \
        snprintf(buf, sizeof(buf), "%d", ival); \
        RDB_SET_CHECK(rdbk, buf); \
    } while(0)


static struct rdb_session * rdb_s;

int init_rdb(void)
{
    int ret;
    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        return -1;
    }

    ret = rdb_subscribe(rdb_s, RDB_POWEROFF);
    if (ret && ret != -ENOENT) {
        BLOG_ERR("Failed to watch RDB %s\n", RDB_POWEROFF);
        return ret;
    }
    return 0;
}

int get_rdb_fd(void)
{
    return rdb_fd(rdb_s);
}

int update_charge_rdbs(const struct charge_stats * stats)
{
    const char * val;

    switch (stats->charge_stat) {
    case CHARGE_STAT_UNKNOWN:
        val = "unknown";
        break;
    case CHARGE_STAT_IDLE:
        val = "none";
        break;
    case CHARGE_STAT_PRECHG:
        val = "trickle";
        break;
    case CHARGE_STAT_FASTCHG:
        val = "fast";
        break;
    case CHARGE_STAT_CHGDONE:
        val = "done";
        break;
    case CHARGE_STAT_FAULT:
        val = "fault";
        break;
    default:
        BLOG_ERR("Unknown charge_stat %d\n", stats->charge_stat);
        return -1;
    }
    RDB_SET_CHECK(RDB_CHARGE_STAT, val);

    switch (stats->vbus_stat) {
    case VBUS_STAT_UNKNOWN:
        val = "unknown";
        break;
    case VBUS_STAT_NONE:
        val = "none";
        break;
    case VBUS_STAT_USB:
        val = "usb";
        break;
    case VBUS_STAT_ADAPTER:
        val = "adapter";
        break;
    case VBUS_STAT_OTG:
        val = "otg";
        break;
    default:
        return -1;
    }
    RDB_SET_CHECK(RDB_VBUS_STAT, val);

    static uint8_t last_power_good = 2;
    if (last_power_good != stats->power_good) {
        RDB_SET_CHECK(RDB_POWER_GOOD, stats->power_good ? "1" : "0");
        last_power_good = stats->power_good;
    }

    return 0;
}

int update_monitor_rdbs(const struct monitor_stats * stats)
{
    char buf[64]; // sufficient to hold any integer

    RDB_SET_INT_CHECK(RDB_VOLTAGE, buf, stats->voltage);

    RDB_SET_INT_CHECK(RDB_CURRENT, buf, stats->current);

    RDB_SET_INT_CHECK(RDB_CAPACITY_MAH, buf, stats->capacity);

    RDB_SET_INT_CHECK(RDB_CAPACITY, buf, stats->percentage);

    RDB_SET_INT_CHECK(RDB_FULL_CAPACITY_MAH, buf, stats->full_capacity);

    RDB_SET_INT_CHECK(RDB_TEMPERATURE, buf, stats->temperature);

    return 0;
}

int initiate_poweroff(int force)
{
    char buf[8] = "";

    rdb_get_string(rdb_s, RDB_POWEROFF, buf, sizeof(buf));

    // if the variable is empty, or '0', or '1' and force is requested
    if(!strlen(buf) || buf[0] == '0' || (force && !strcmp(buf, "1")) ) {
        RDB_SET_CHECK(RDB_POWEROFF, force ? "11" : "1");
    }

    return 0;
}

int poweroff_rdb_set(void)
{
    char buf[8];
    int ret;
    ret = rdb_get_string(rdb_s, RDB_POWEROFF, buf, sizeof(buf));
    return !ret && !strcmp(buf, "2");
}

int poweroff_rdb_finalise(void)
{
    // signals other system components to shut down properly before power cut off
    RDB_SET_CHECK(RDB_POWEROFF, "3");
    return 0;
}

int is_owa_connected(void)
{
    int connected = 0;
    rdb_get_int(rdb_s, RDB_OWA_CONNECTED, &connected);
    return connected;
}

void term_rdb(void)
{
    rdb_close(&rdb_s);
}

int write_didt(int didt)
{
    char buf[8];
    RDB_SET_INT_CHECK(RDB_BATTERY_DIDT, buf, didt);
    return 0;
}

int set_persisted_level(uint8_t level)
{
    char buf[8];
    RDB_SET_INT_CHECK(RDB_CAPACITY_LAST, buf, level);
    return 0;
}

uint8_t get_persisted_level(void)
{
    int ret, level;
    ret = rdb_get_int(rdb_s, RDB_CAPACITY_LAST, &level);

    if(ret) {
        level = 0xFF;
    }

    return (uint8_t)level;
}