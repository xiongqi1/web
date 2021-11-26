/*
 * @file rdb_help.c
 * @brief rdb relevant functions
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef PC_SIMULATOR
#include <rdb_ops.h>
#endif

#include "nc_util.h"
#include "rdb_help.h"

#define RDB_OWA_LEGACY_STATUS "owa.legacy.connection"
#define RDB_BAUD_RATE "owa.legacy.baudrate"
#define RDB_BATTERY_STATUS "system.battery.charging_state"
#define RDB_BATTERY_CAPACITY "system.battery.capacity"
#define RDB_HW_VER "system.product.hwver"
#define RDB_HW_NAME "system.product.model"
#define RDB_SW_VER "sw.version"
#define RDB_LED_CTRL "system.leds."
#define RDB_NIT_MODE "install_tool.mode"
#define RDB_POWER_OFF "service.system.poweroff"

#ifndef PC_SIMULATOR
static struct rdb_session * rdb_s;
static int rdbfd;

int rdb_init()
{
     if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        return -1;
    }
    rdbfd = rdb_fd(rdb_s);
    if (rdbfd < 0) {
        BLOG_ERR("Failed to get rdbfd\n");
        return -1;
    }

    rdb_subscribe(rdb_s, RDB_NIT_MODE);
    return 0;
}

int rdb_getfd()
{
    return rdbfd;
}

void rdb_cleanup()
{
    rdb_unsubscribe(rdb_s, RDB_NIT_MODE);
    rdb_close(&rdb_s);
}

int rdb_set_legacy_owa_status(int status)
{
    int ret = 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", status);
    ret = rdb_set_string(rdb_s, RDB_OWA_LEGACY_STATUS, buf);
    if (ret) {
        BLOG_ERR("Failed to set RDB %s\n", RDB_OWA_LEGACY_STATUS);
    }
    return ret;
}

int rdb_shutting_down()
{
    int ret = 0;
    char buf[64];
    ret = rdb_get_string(rdb_s, RDB_NIT_MODE, buf, sizeof(buf));
    if (ret) {
        BLOG_ERR("Failed to get RDB %s\n", RDB_NIT_MODE);
        return 0;
    }

    if (!strcmp(buf, "power_off")) {
        (void)rdb_set_led(0, 0, 0);
        (void)rdb_set_led(1, 0, 0);
        (void)rdb_set_led(2, 0, 0);

        ret = rdb_set_string(rdb_s, RDB_POWER_OFF, "2");
        if (ret) {
            BLOG_ERR("Failed to set RDB %s\n", RDB_POWER_OFF);
            return 0;
        }
        return 1;
    }
    return 0;
}

int rdb_set_baudrate(int baudrate)
{
    int ret = 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", baudrate);
    ret = rdb_set_string(rdb_s, RDB_BAUD_RATE, buf);
    if (ret) {
        BLOG_ERR("Failed to set RDB %s\n", RDB_BAUD_RATE);
    }
    return ret;
}

unsigned char rdb_get_battery_level()
{
    int level = 0;
    if (rdb_get_int(rdb_s, RDB_BATTERY_CAPACITY, &level)) {
        BLOG_ERR("Failed to get RDB %s\n", RDB_BATTERY_CAPACITY);
        return -1;
    }
    return (unsigned char) level;
}

char rdb_get_battery_status()
{
    int ret = 0;
    char buf[64];
    ret = rdb_get_string(rdb_s, RDB_BATTERY_STATUS, buf, sizeof(buf));
    /* current lark charing status:
     * unknown, none, trickle, fast, done, fault, need translate to titan's
     * discharge(0), charge(1), full(2), fault(3)
     */
    if (ret) {
        BLOG_ERR("Failed to set RDB %s\n", RDB_BATTERY_STATUS);
        return -1;
    }
    if (!strcmp(buf, "unknow") || !strcmp(buf, "none")) { //discharge
        ret = 0;
    } else if (!strcmp(buf, "trickle") || !strcmp(buf, "fast")) {
        ret = 1;
    } else if (!strcmp(buf, "done")) {
        ret = 2;
    } else {
        ret = 3;
    }
    return (char) ret;
}

int rdb_get_hw_ver (char * hw_str, int * len)
{
    int ret = 0;
    char buf[64];
    ret = rdb_get_string(rdb_s, RDB_HW_VER, buf, sizeof(buf));
    if (ret) {
        BLOG_ERR("Failed to set RDB %s\n", RDB_HW_VER);
        return -1;
    }
    strcpy(hw_str, buf);
    *len = strlen(buf);
    return 0;
}

int rdb_get_sw_ver (char * sw_str, int * len)
{
    int ret = 0;
    char buf[64];
    ret = rdb_get_string(rdb_s, RDB_SW_VER, buf, sizeof(buf));
    if (ret) {
        BLOG_ERR("Failed to set RDB %s\n", RDB_SW_VER);
        return -1;
    }
    strcpy(sw_str, buf);
    *len = strlen(buf);
    return 0;
}

int rdb_get_hw_model (char * model_str, int * len)
{
    int ret = 0;
    char buf[64];
    ret = rdb_get_string(rdb_s, RDB_HW_NAME, buf, sizeof(buf));
    if (ret) {
        BLOG_ERR("Failed to set RDB %s\n", RDB_HW_NAME);
        return -1;
    }
    strcpy(model_str, buf);
    *len = strlen(buf);
    return 0;
}

int rdb_set_led(int led_num, int led_color, int led_flash_feq)
{
    char rdb_key[32];
    char rdb_value[64];
    int ret = 0;
    //the led format is: color@pattern: temp/period such as yellow@0:10/1000
    if (led_num >= 3) return (-1);
    snprintf(rdb_key, sizeof(rdb_key), "%s%d",RDB_LED_CTRL, led_num);
    switch (led_color) {
        case 0:  //off
            snprintf(rdb_value, sizeof(rdb_value), "%s", "off" );
            break;
        case 1:
            if (led_flash_feq == 0) {
                snprintf(rdb_value, sizeof(rdb_value), "%s", "red");
            } else {
                snprintf(rdb_value, sizeof(rdb_value), "%s@01:1000/%d", "red", led_flash_feq);
            }
            break;
        case 2:
            if (led_flash_feq == 0) {
                snprintf(rdb_value, sizeof(rdb_value), "%s", "green");
            } else {
                snprintf(rdb_value, sizeof(rdb_value), "%s@01:1000/%d", "green", led_flash_feq);
            }
            break;
        case 3:
            if (led_flash_feq == 0) {
                snprintf(rdb_value, sizeof(rdb_value), "%s", "yellow");
            } else {
                snprintf(rdb_value, sizeof(rdb_value), "%s@01:1000/%d", "yellow", led_flash_feq);
            }
            break;
    }
    ret = rdb_set_string(rdb_s, rdb_key, rdb_value);
    if (ret) {
        BLOG_ERR("Failed to set RDB %s\n", rdb_key);
        return -1;
    }
    return 0;
}
#else
int rdb_init() {
    return 0;
}
int rdb_getfd()
{
    return -1;
}
void rdb_cleanup()
{

}
int rdb_set_baudrate(int baudrate)
{
    return 0;
}

char rdb_get_battery_level()
{
    return 50;
}

char rdb_get_battery_status()
{
    return 2;
}

int rdb_get_hw_ver (char * hw_str, int * len)
{
    const char hw_ver[] = "revA";
    strcpy(hw_str, hw_ver);
    *len = sizeof(hw_ver);
    return 0;
}

int rdb_get_sw_ver (char * sw_str, int * len)
{
    const char sw_ver[] = "x.x.x";
    strcpy(sw_str, sw_ver);
    *len = sizeof(sw_ver);
    return 0;
}

int rdb_get_hw_model (char * model_str, int * len)
{
    const char name[] = "nrb-0200";
    strcpy(model_str, name);
    *len = sizeof(name);
    return 0;
}

int rdb_set_led(int led_num, int led_color, int led_flash_feq)
{
    (void) led_num;
    (void) led_color;
    (void) led_flash_feq;
    return 0;
}

int rdb_set_legacy_owa_status(int status)
{
    return 0;
}

int rdb_shutting_down()
{
    return 0;
}
#endif
