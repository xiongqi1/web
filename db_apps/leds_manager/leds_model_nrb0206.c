/*
 * leds_model_nrb0206.c
 *
 * Implementing Lark LEDs model
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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

#include "leds_model.h"
#include "leds_model_base.h"
#include "leds_model_nrb0206.h"
#include "rdb_helper.h"
#include "leds_ops.h"

#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

/*
 * Assume this RDB is set properly by other daemons/services
 * possible values: battery, calibration, normal, scan, flash_owa, peer, flash_self
 */
#define INSTALL_TOOL_MODE_RDB "install_tool.mode"
// all possible modes
typedef enum {
    IT_MODE_BATTERY,     // battery check mode (disconnected to OWA)
    IT_MODE_CALIBRATION, // calibration mode (disconnected to OWA)
    IT_MODE_NORMAL,      // normal operation mode (connected to OWA)
    IT_MODE_SCAN,        // cell scanning mode (connected to OWA)
    IT_MODE_FLASH_OWA,   // flashing OWA in progress (connected to OWA)
    IT_MODE_PEER,        // peer searching mode (handshaking with OWA)
    IT_MODE_FLASH_SELF,  // flashing Lark in progress (disconnected to OWA)
    IT_MODE_POWEROFF,    // system starts to power off
} it_mode_t;

// this global is updated from INSTALL_TOOL_MODE_RDB and controls LED bahaviours
static it_mode_t it_mode = IT_MODE_BATTERY;

// 3 LEDs are available in Lark
#define MAX_LEDS 3
const char * leds_names[MAX_LEDS] = {
    "0", "1", "2",
};

#define SYSTEM_POWEROFF_RDB "service.system.poweroff"

/*
 * The following 3 groups of LED behaviours can be customised by RDBs from OWA
 *
 * * battery level
 * * WAN signal strength
 * * WAN link state
 *
 * Upon connecting to OWA (handshaking), relevant RDBs are retrieved from OWA
 */

/* this is current battery level, needs to be set by battery manager */
#define BATTERY_LEVEL_RDB "system.battery.capacity"

#define MAX_BATTERY_LEVELS 8
/*
 * configurable from RDB: system.battery.thresholds.
 * e.g. "1,20,35,50,65,80,95,1000"
 */
#define BATTERY_THRESHOLDS_RDB "system.battery.thresholds"
static int battery_level_thresholds[MAX_BATTERY_LEVELS];
// default values - LARK-78
static const int def_battery_level_thresholds[MAX_BATTERY_LEVELS] = {
    0,    // <0 cannot power on
    11,   // <11 extremely low
    26,   // <26 low
    41,   // <41 medium
    56,   // <56 high medium
    71,   // <71 high
    86,   // <86 almost full
    1000, // 95+ full
};
/*
 * configurable from RDB: system.battery.led_patterns.
 * e.g. "off,off,off;off,off,red;off,off,green@500/1000;off,off,green;..."
 * battery levels are separated by semicolons, LEDs are separated by commas
 * LED pattern values are defined in db_apps/leds_monitor
 */
#define BATTERY_PATTERNS_RDB "system.battery.led_patterns"
static char * battery_leds_vals[MAX_BATTERY_LEVELS][MAX_LEDS];
// default values - LARK-78
static const char * const def_battery_leds_vals[MAX_BATTERY_LEVELS][MAX_LEDS] = {
    {"off", "off", "off"},
    {"off", "off", "red"},
    {"off", "off", "green@500/1000"},
    {"off", "off", "green"},
    {"off", "green@500/1000", "green"},
    {"off", "green", "green"},
    {"green@500/1000", "green", "green"},
    {"green", "green", "green"},
};

/*
 * This is OWA WWAN signal strength, needs to be set by OWA.
 * Note: this is an abstracted signal strength. It could be RSRP, RSSI, etc.
 * OWA is responsible to set it with proper value along with its corresponding
 * LED patterns override as defined below
 */
#define SIGNAL_STRENGTH_RDB "owa.wan.signal.strength"

#define MAX_SIGNAL_LEVELS 4
/*
 * configurable from RDB: owa.wan.signal.thresholds.
 * e.g. "-140,-98,-75,0"
 */
#define SIGNAL_THRESHOLDS_RDB "owa.wan.signal.thresholds"
static int signal_level_thresholds[MAX_SIGNAL_LEVELS];
// default values - LARK-80
static const int def_signal_level_thresholds[MAX_SIGNAL_LEVELS] = {
    -140, // <-140 not detected
    -98,  // <-98 low
    -75,  // <-75 medium
    0,    // -75+ high
};
/*
 * configurable from RDB: owa.wan.signal.led_patterns.
 * e.g. "off,off,red@500/1000;off,off,red;off,yellow,yellow;green,green,green"
 * battery levels are separated by semicolons, LEDs are separated by commas
 * LED pattern values are defined in db_apps/leds_monitor
 */
#define SIGNAL_PATTERNS_RDB "owa.wan.signal.led_patterns"
static char *signal_leds_vals[MAX_SIGNAL_LEVELS][MAX_LEDS];
// default values - LARK-80
static const char * const def_signal_leds_vals[MAX_SIGNAL_LEVELS][MAX_LEDS] = {
    {"off", "off", "red@500/1000"},
    {"off", "off", "red"},
    {"off", "yellow", "yellow"},
    {"green", "green", "green"},
};

/*
 * This is OWA WAN link state RDB, to be set by OWA
 * Note: this is an abstracted WAN link state. It could be a combined result
 * of a few WAN stats, such as register state, attach status, pdn connectivity.
 * OWA needs to set this RDB along with the following LED patterns override
 */
#define WAN_STATE_RDB "owa.wan.link.state"
#define MAX_WAN_STATES 5
/*
 * configurable from RDB: owa.wan.link.led_patterns.
 * e.g. "red,yellow@250/500,yellow,green@250/500,green"
 * LED patterns are separated by commas
 * LED pattern values are defined in db_apps/leds_monitor
 */
#define WAN_LINK_PATTERNS_RDB "owa.wan.link.led_patterns"
static char * wan_state_vals[MAX_WAN_STATES];
// default values - LARK-81
static const char * def_wan_state_vals[MAX_WAN_STATES] = {
    "red",            // not registered (deregistered-xxx)
    "yellow@250/500", // registering (registering-xxx)
    "yellow",         // registered (registered-disconnected)
    "green@250/500",  // attaching (registered-establishing)
    "green",          // attached (registered-established)
};

/*
 * Other RDBs that determine LED behaviour
 *
 * The related LED patterns are not customisable by OWA at present.
 */

/* Whether Lark is supplying power to OWA: 1/0 - Lark sets this */
#define OWA_POWER_ENABLED_RDB "owa.power.enabled"

/* Whether OWA is connected to PoE: 1/0 - OWA needs to set this */
#define OWA_ETH_CONNECTED_RDB "owa.eth.connected"

/* OWA Ethernet link activity indicator: 1/0 - OWA needs to set this */
#define OWA_ETH_ACT_RDB "owa.eth.act"

/*
 * Lark compass/orientation sensors calibration status:
 * 0 - calibration needed/in progress
 * 1 - calibration done/not needed
 */
#define COMPASS_CALIB_STATUS_RDB "owa.orien.status"

/*
 * Convert string to integer number
 * @param string string to convert
 * @return converted number. Notice: always return 0 in error cases.
 * @note A string representing a float pointer number will be truncated to an integer
 */
static long int string_to_long_int(char *string) {
    char *endptr;
    long int val;

    val = strtol(string, &endptr, 10);
    if (*endptr == '\0' || *endptr == '.'){
        return val;
    } else {
        return 0;
    }
}

/*
 * A helper function to reset LED thresholds and patterns to default values
 *
 * @param max_levels The maximum number of levels in the thresholds.
 * @param thresholds An array of the thresholds to be reset (length=max_levels)
 * @param def_thresholds An array of the default threshold values
 * @param vals A two dimensional array of the LED patterns to be reset
 * @param def_vals A two dimensional array of the default LED patterns
 * @param objstr A string to be used in error message
 */
static void reset_patterns(int max_levels,
                           int thresholds[], const int def_thresholds[],
                           char * vals[][MAX_LEDS],
                           const char * const def_vals[][MAX_LEDS],
                           const char * objstr)
{
    int level, led;
    for (level = 0; level < max_levels; level++) {
        thresholds[level] = def_thresholds[level];
    }
    for (level = 0; level < max_levels; level++) {
        for (led = 0; led < MAX_LEDS; led++) {
            if (vals[level][led]) {
                free(vals[level][led]);
            }
            vals[level][led] = strdup(def_vals[level][led]);
            if (!vals[level][led]) {
                syslog(LOG_ERR, "Failed to reset %s patterns\n", objstr);
                return;
            }
        }
    }
}

/*
 * A helper function to parse a single LED pattern
 *
 * @param pattern A string containing comma-separated entries to be parsed.
 *  Note the string will be changed in place by this function.
 * @param entries The number of entries expected in the pattern.
 * @param vals An array to store the parsed entries
 * @param objstr A string to be used in error message
 * @return The number of parsed entries on success; or -1 on error
 */
static int parse_pattern(char * pattern, int entries, char * vals[], const char * objstr)
{
    char * end_token;
    char * token = strtok_r(pattern, ",", &end_token);
    int cnt = 0;
    while (token && cnt < entries) {
        if (vals[cnt]) {
            free(vals[cnt]);
        }
        vals[cnt] = strdup(token);
        if (!vals[cnt]) {
            syslog(LOG_ERR, "Failed to override %s pattern (cnt=%d)\n", objstr, cnt);
            return -1;
        }
        token = strtok_r(NULL, ",", &end_token);
        cnt++;
    }
    return cnt;
}

/*
 * A helper function to override LED thresholds and patterns from RDBs
 *
 * @param max_levels The maximum number of levels in the thresholds
 * @param thresholds_rdb The RDB key holding the new comma-separated thresholds
 * @param thresholds An array of the thresholds to be set (length=max_levels)
 * @param patterns_rdb The RDB key holding the new LED patterns
 * @param vals A two dimensional array of the LED patterns to be set
 * @param objstr A string to be used in error message
 */
static int override_patterns(int max_levels,
                             const char * thresholds_rdb, int thresholds[],
                             const char * patterns_rdb, char * vals[][MAX_LEDS],
                             const char * objstr)
{
    int ret;
    char * rdb_val = get_rdb(thresholds_rdb);
    if (!rdb_val) {
        syslog(LOG_INFO, "%s does not exist. skipped overriding\n", thresholds_rdb);
        return 0;
    }
    char * token = strtok(rdb_val, ",");
    int level = 0;
    while (token && level < max_levels) {
        char * endptr;
        int th = strtol(token, &endptr, 10);
        if (*endptr) {
            syslog(LOG_ERR, "invalid threshold %s\n", token);
            return -1;
        }
        thresholds[level++] = th;
        token = strtok(NULL, ",");
    }
    if (level == 0) {
        syslog(LOG_INFO, "%s is empty. skipped overriding\n", thresholds_rdb);
        return 0;
    }
    if (level != max_levels) {
        syslog(LOG_ERR, "%s has wrong size %d\n", thresholds_rdb, level);
        return -1;
    }

    char * patterns = get_rdb(patterns_rdb);
    if (!patterns) {
        syslog(LOG_ERR, "%s does not exist. but %s does\n",
               patterns_rdb, thresholds_rdb);
        return -1;
    }
    char * end_token;
    token = strtok_r(patterns, ";", &end_token);
    level = 0;
    while (token && level < max_levels) {
        ret = parse_pattern(token, MAX_LEDS, vals[level], objstr);
        if (ret != MAX_LEDS) {
            syslog(LOG_ERR, "wrong %s LED patterns (led=%d)\n", objstr, ret);
            return -1;
        }
        token = strtok_r(NULL, ";", &end_token);
        level++;
    }
    if (token || level != max_levels) {
        syslog(LOG_ERR, "wrong %s LED patterns (level=%d)\n", objstr, level);
        return -1;
    }
    return 0;
}

/*
 * Reset battery thresholds and LED patterns to default values
 */
static void reset_battery_patterns(void)
{
    reset_patterns(MAX_BATTERY_LEVELS, battery_level_thresholds,
                   def_battery_level_thresholds,
                   battery_leds_vals, def_battery_leds_vals, "battery");
}

/*
 * Override battery thresholds and LED patterns from RDBs
 *
 * @return 0 on success; -1 on errors
 */
static int override_battery_patterns(void)
{
    return override_patterns(MAX_BATTERY_LEVELS,
                             BATTERY_THRESHOLDS_RDB, battery_level_thresholds,
                             BATTERY_PATTERNS_RDB, battery_leds_vals,
                             "battery");
}

/*
 * Reset WAN signal thresholds and LED patterns to default values
 */
static void reset_wan_signal_patterns(void)
{
    reset_patterns(MAX_SIGNAL_LEVELS, signal_level_thresholds,
                   def_signal_level_thresholds,
                   signal_leds_vals, def_signal_leds_vals, "signal");
}

/*
 * Override WAN signal thresholds and LED patterns from RDBs
 *
 * @return 0 on success; -1 on errors
 */
static int override_wan_signal_patterns(void)
{
    return override_patterns(MAX_SIGNAL_LEVELS,
                             SIGNAL_THRESHOLDS_RDB, signal_level_thresholds,
                             SIGNAL_PATTERNS_RDB, signal_leds_vals, "signal");
}

/*
 * Reset WAN link state LED patterns to default values
 */
static void reset_wan_state_patterns(void)
{
    int state;
    for (state = 0; state < MAX_WAN_STATES; state++) {
        if (wan_state_vals[state]) {
            free(wan_state_vals[state]);
        }
        wan_state_vals[state] = strdup(def_wan_state_vals[state]);
        if (!wan_state_vals[state]) {
            syslog(LOG_ERR, "Failed to reset WAN link state LED patterns\n");
            return;
        }
    }
}

/*
 * Override WAN link state LED patterns from RDBs
 *
 * @return 0 on success; -1 on errors
 */
static int override_wan_state_patterns(void)
{
    int ret;
    char * patterns = get_rdb(WAN_LINK_PATTERNS_RDB);
    if (!patterns) {
        syslog(LOG_INFO, "%s does not exist. skipped overriding\n", WAN_LINK_PATTERNS_RDB);
        return 0;
    }

    ret = parse_pattern(patterns, MAX_WAN_STATES, wan_state_vals, "WAN state");
    if (ret == 0) {
        syslog(LOG_INFO, "%s is empty. skipped overriding\n", WAN_LINK_PATTERNS_RDB);
        return 0;
    }
    if (ret != MAX_WAN_STATES) {
        syslog(LOG_ERR, "%s has wrong size %d\n", WAN_LINK_PATTERNS_RDB, ret);
        return -1;
    }

    return 0;
}

/*
 * Reset all LED bahaviours to default
 */
static void reset_all_patterns(void)
{
    reset_battery_patterns();
    reset_wan_signal_patterns();
    reset_wan_state_patterns();
}

/*
 * Override all LED bahaviours from RDBs.
 *
 * @note If error occurs, the bahaviour will be reverted to default
 */
static void override_all_patterns(void)
{
    if (override_battery_patterns()) {
        reset_battery_patterns();
    }
    if (override_wan_signal_patterns()) {
        reset_wan_signal_patterns();
    }
    if (override_wan_state_patterns()) {
        reset_wan_state_patterns();
    }
}

/*
 * Check battery level and set battery LEDs
 */
static void check_battery(void) {
    int level, led;
    char *battery_capacity_val = get_rdb(BATTERY_LEVEL_RDB);
    if (battery_capacity_val) {
        long int battery_capacity = string_to_long_int(battery_capacity_val);
        for (level = 0; level < MAX_BATTERY_LEVELS; level++) {
            if (battery_capacity < battery_level_thresholds[level]) {
                break;
            }
        }
        if (level == MAX_BATTERY_LEVELS) { // sanity
            level = MAX_BATTERY_LEVELS - 1;
        }
        for (led = 0; led < MAX_LEDS; led++) {
            set_led(leds_names[led], battery_leds_vals[level][led]);
        }
    }
}

/*
 * Display self check pattern
 *
 * @note This will block the process for 1.5s. It should only be called in init
 */
static void self_check(void) {
    int led;
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], "green");
    }
    usleep(500000);
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], "red");
    }
    usleep(500000);
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], "yellow");
    }
    usleep(500000);
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], "off");
    }
}

/*
 * Display peer search pattern
 *
 * @note This should be invoked in monitor handler so that the pattern proceeds
 */
static void check_peer(void) {
    static int sequence = 0;
    int led;
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], led == sequence ? "green" : "off");
    }
    sequence = (sequence + 1) % MAX_LEDS;
}

/*
 * Display OWA flashing in progress pattern
 */
static void check_flash_owa(void) {
    int led;
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], "red@250/500");
    }
}

/*
 * Display Lark flashing in progress pattern
 */
static void check_flash_self(void) {
    int led;
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], "yellow@250/500");
    }
}

/*
 * Check cell scan mode and set respective LEDs
 */
static void check_scan(void) {
    char * strength = get_rdb(SIGNAL_STRENGTH_RDB);
    int led;
    int level = 0;
    if (strength) {
        int val = string_to_long_int(strength);
        for (level = 0; val < 0 && level < MAX_SIGNAL_LEVELS; level++) {
            if (val < signal_level_thresholds[level]) {
                break;
            }
        }
        if (level == MAX_SIGNAL_LEVELS) { // sanity
            level = MAX_SIGNAL_LEVELS - 1;
        }
    }
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], signal_leds_vals[level][led]);
    }
}

/*
 * Check normal operation mode and set respective LEDs
 */
static void check_normal(void) {
    // LED0: owa power status: on/off
    char *owa_power_enabled = get_rdb(OWA_POWER_ENABLED_RDB);
    if (owa_power_enabled && !strcmp(owa_power_enabled, "1")) {
        set_led(leds_names[0], "green");
    } else {
        set_led(leds_names[0], "off");
    }

    // LED1: LTE link status
    char *reg_stat = get_rdb(WAN_STATE_RDB);
    int reg_no;
    if (!reg_stat) {
        reg_no = 0;
    } else {
        reg_no = string_to_long_int(reg_stat);
    }
    if (reg_no >= MAX_WAN_STATES) {
        reg_no = MAX_WAN_STATES - 1;
    }
    set_led(leds_names[1], wan_state_vals[reg_no]);

    // LED2: Ethernet activity
    char *eth_conn = get_rdb(OWA_ETH_CONNECTED_RDB);
    if (!eth_conn || strcmp(eth_conn, "1")) {
        set_led(leds_names[2], "off");
    } else {
        char *eth_act = get_rdb(OWA_ETH_ACT_RDB);
        if (!eth_act || strcmp(eth_act, "1")) {
            set_led(leds_names[2], "green");
        } else {
            set_led(leds_names[2], "green@50/100");
        }
    }
}

/*
 * Check if compass calibration is done and set LED respectively
 */
static void check_calibration(void)
{
    char *cal_stat = get_rdb(COMPASS_CALIB_STATUS_RDB);
    int led;
    if (!cal_stat || (strcmp(cal_stat, "0") && strcmp(cal_stat, "2"))) { // need calibration
        for (led = 0; led < MAX_LEDS; led++) {
            set_led(leds_names[led], "yellow@1010:100/1000");
        }
    } else {
        for (led = 0; led < MAX_LEDS; led++) { // calibration done
            set_led(leds_names[led], "green@750/1000");
        }
    }
}

/*
 * Once in power off state, turn off all LED
 */
static void check_system_going_poweroff(void)
{
    int led;
    syslog(LOG_INFO, "Entering into POWER OFF\n");
    for (led = 0; led < MAX_LEDS; led++) {
        set_led(leds_names[led], "off");
    };
    set_rdb(SYSTEM_POWEROFF_RDB, "2");
}

/*
 * Update LEDs according to current install tool mode
 */
static void mode_update(void)
{
    syslog(LOG_DEBUG, "mode_update: %d\n", it_mode);

    switch(it_mode) {
    case IT_MODE_BATTERY:
        check_battery();
        break;
    case IT_MODE_CALIBRATION:
        check_calibration();
        break;
    case IT_MODE_NORMAL:
        check_normal();
        break;
    case IT_MODE_SCAN:
        check_scan();
        break;
    case IT_MODE_FLASH_OWA:
        check_flash_owa();
        break;
    case IT_MODE_PEER:
        check_peer();
        break;
    case IT_MODE_FLASH_SELF:
        check_flash_self();
        break;
    case IT_MODE_POWEROFF:
        check_system_going_poweroff();
        break;
    default:
        syslog(LOG_ERR, "unknown install tool mode %d\n", it_mode);
    }
}

/*
 * install tool mode trigger handler
 * @param trigger related event trigger
 */
static void mode_event_handler(event_trigger *trigger) {
    char *mode = get_rdb(trigger->rdb_name);
    static it_mode_t prev_it_mode = IT_MODE_BATTERY;
    syslog(LOG_DEBUG, "mode_event_handler: %s\n", mode);
    if (!mode) {
        return;
    }
    if (!strcmp(mode, "battery")) {
        it_mode = IT_MODE_BATTERY;
    } else if (!strcmp(mode, "calibration")) {
        it_mode = IT_MODE_CALIBRATION;
    } else if (!strcmp(mode, "normal")) {
        if (prev_it_mode != IT_MODE_SCAN && prev_it_mode != IT_MODE_NORMAL && prev_it_mode != IT_MODE_FLASH_OWA) {
            // OWA is connected, might override LEDs behaviour
            override_all_patterns();
        }
        it_mode = IT_MODE_NORMAL;
    } else if (!strcmp(mode, "scan")) {
        if (prev_it_mode != IT_MODE_SCAN && prev_it_mode != IT_MODE_NORMAL && prev_it_mode != IT_MODE_FLASH_OWA) {
            // OWA is connected, might override LEDs behaviour
            override_all_patterns();
        }
        it_mode = IT_MODE_SCAN;
    } else if (!strcmp(mode, "peer")) {
        it_mode = IT_MODE_PEER;
    } else if (!strcmp(mode, "flash_owa")) {
        it_mode = IT_MODE_FLASH_OWA;
    } else if (!strcmp(mode, "flash_self")) {
        it_mode = IT_MODE_FLASH_SELF;
    } else if (!strcmp(mode, "power_off")) {
        it_mode = IT_MODE_POWEROFF;
    } else {
        syslog(LOG_ERR, "invalid it mode: %s\n", mode);
        return;
    }
    mode_update();
    prev_it_mode = it_mode;
}

/*
 * Periodic monitor handler
 * @param monitor related periodic monitor entry
 */
static void periodic_handler(periodic_monitor *monitor) {
    mode_update();
}

/* LEDs model implementation */

/*
 * init. See leds_model.h.
 */
static int init(leds_model *self) {
    // load default LED patterns
    reset_all_patterns();

    // show self check pattern at start up
    self_check();

    // add triggers and period monitors

    // mode triggers to have fastest response to mode change event
    event_trigger *mode_event = event_trigger__new(INSTALL_TOOL_MODE_RDB,
                                                   self,
                                                   mode_event_handler);
    if (mode_event) {
        self->add_trigger(self, mode_event);
    }

    // periodic monitor
    periodic_monitor *monitor = periodic_monitor__new(self, periodic_handler);
    if (monitor) {
        self->add_periodic_monitor(self, monitor);
    }

    self->base->init(self);

    return 0;
}

/*
 * deinit. See leds_model.h.
 */
static void deinit(leds_model *self) {
    //invoke additional derived deinit procedure here

    // delete base
    leds_model__delete(self->base);
    self->base = NULL;
}

/*
 * lark_leds_model__new. See leds_model_nrb0206.h.
 */
leds_model *lark_leds_model__new(leds_model *base_model) {
    if (!base_model) {
        base_model = base_leds_model__new(NULL);
        if (!base_model) {
            return NULL;
        }
    }

    leds_model *self = base_leds_model__new(base_model);
    self->init = init;
    self->deinit = deinit;

    return self;
}
