/*
 * leds_model_bordeaux.c
 *
 * Implementing Bordeaux LEDs model
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

#include "leds_model.h"
#include "leds_model_base.h"
#include "leds_model_bordeaux.h"
#include "rdb_helper.h"
#include "leds_ops.h"

#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

typedef enum {
    NO_SIM_STATUS,
    SIM_ERROR,
    NO_SERVICE,
    SERVICE_4G,
    SERVICE_3G
} cellular_status;

#define POWER "power"
#define BATTERY "battery"
#define VOICEMAIL "voicemail"
#define INFO "info"
#define ANTENNA "antenna"
#define SIGNAL_BAR_FMT "signal.%d"

#define NUM_SIGNAL_LEDS 4

/*
 * Voice mail event trigger handler
 * @param trigger related event trigger
 */
static void voicemail_rdb_trigger_handler(event_trigger *trigger) {
    char *rdb_value = get_rdb(trigger->rdb_name);

    if (rdb_value && !strcmp(rdb_value, "1")) {
        set_led(VOICEMAIL, "blink");
    } else {
        set_led(VOICEMAIL, "off");
    }
}

/*
 * Convert string to number
 * @param string string to convert
 * @return converted number. Notice: always return 0 in error cases.
 */
long int string_to_long_int(char *string) {
    char *endptr;
    long int val;

    val = strtol(string, &endptr, 10);
    if (*endptr == '\0'){
        return val;
    } else {
        return 0;
    }
}

/*
 * Set LEDs for signal strength bars.
 * @param bar_num Number of bar LEDs will be set bright as specified by parameter mode.
 * Other bar LEDs will be set off.
 * @param mode Mode to set bright bar LEDs
 * @return 0 on success, a number on error
 */
static int set_signal_strength_bar_leds(int bar_num, char *mode) {
    static char led_name[16];
    int i, rval;

    if (bar_num > 0 && mode) {
        for (i = 0; i < bar_num; i++) {
            rval = snprintf(led_name, sizeof(led_name), SIGNAL_BAR_FMT, i);
            if (rval >= sizeof(led_name) || rval < 0) {
                return rval;
            }
            set_led(led_name, mode);
        }
    }

    for (i = bar_num; i < NUM_SIGNAL_LEDS; i++) {
        rval = snprintf(led_name, sizeof(led_name), SIGNAL_BAR_FMT, i);
        if (rval >= sizeof(led_name) || rval < 0) {
            return rval;
        }

        set_led(led_name, "off");
    }

    return 0;
}

/*
 * Set cellular signal strength and antenna LEDs
 * @param status cellular status
 * @param bar_num Number of bar LEDs to set bright
 */
static void set_cellular_signal_strength_antenna_leds(cellular_status status, int bar_num) {
    char *led_mode;
    switch (status) {
    case NO_SIM_STATUS:
        set_led(ANTENNA, "off");
        set_signal_strength_bar_leds(0, NULL);
        break;
    case SIM_ERROR:
        set_led(ANTENNA, "red@10:500/1000");
        set_signal_strength_bar_leds(0, NULL);
        break;
    case NO_SERVICE:
        set_led(ANTENNA, "red");
        set_signal_strength_bar_leds(0, NULL);
        break;
    case SERVICE_4G:
    case SERVICE_3G:
        led_mode = status == SERVICE_4G ? "green" : "blue";
        set_led(ANTENNA, led_mode);
        set_signal_strength_bar_leds(bar_num, led_mode);
        break;
    }
}

/*
 * Periodic monitor handler for cellular signal strength and antenna LEDs
 * @param monitor related periodic monitor entry
 */
static void cellular_signal_strength_antenna(periodic_monitor *monitor) {
    cellular_status status = NO_SIM_STATUS;
    int bar_num = 0;
    char *sim_status = get_rdb("wwan.0.sim.status.status");
    if (sim_status) {
        if (!strcmp(sim_status, "SIM OK")) {
            char *network_reg_status = get_rdb("wwan.0.system_network_status.reg_stat");
            if (network_reg_status && !strcmp(network_reg_status, "1")) {
                char *network_type = get_rdb("wwan.0.system_network_status.service_type");
                if (network_type && (strlen(network_type) > 0) && strcmp(network_type, "none")) {
                    if (!strncmp(network_type, "lte", 3)) {
                        // 4G
                        char *rsrp_str = get_rdb("wwan.0.signal.0.rsrp");
                        long int rsrp = 0;

                        status = SERVICE_4G;
                        if (rsrp_str) {
                            rsrp = string_to_long_int(rsrp_str);
                        }
                        if (rsrp < 0) {
                            // Reference: a) AT&T document 13340, CDR-RBP-1030, Table 8.3; b) FR-18514
                            if (rsrp < -118) {
                                bar_num = 0;
                            } else if (rsrp < -112) {
                                bar_num = 1;
                            } else if (rsrp < -106) {
                                bar_num = 2;
                            } else if (rsrp < -100) {
                                bar_num = 3;
                            } else {
                                bar_num = 4;
                            }
                        } else {
                            bar_num = 0;
                        }
                    } else if (!strncmp(network_type, "umts", 4)) {
                        // 3G
                        char *rscp_str = get_rdb("wwan.0.cell_measurement.rscp");
                        long int rscp = 0;

                        status = SERVICE_3G;

                        if (rscp_str) {
                            rscp = string_to_long_int(rscp_str);
                        }
                        if (rscp < 0) {
                            // Reference: a) AT&T document 13340, CDR-RBP-1030, Table 8.2; b) FR-18514
                            if (rscp <= -106) {
                                bar_num = 0;
                            } else if (rscp <= -100) {
                                bar_num = 1;
                            } else if (rscp <= -90) {
                                bar_num = 2;
                            } else if (rscp <= -80) {
                                bar_num = 3;
                            } else {
                                bar_num = 4;
                            }
                        } else {
                            bar_num = 0;
                        }
                    } else {
                        status = NO_SERVICE;
                    }
                } else {
                    // SIM OK, no service
                    status = NO_SERVICE;
                }
            } else {
                // SIM OK, no service
                status = NO_SERVICE;
            }
        } else {
            // SIM is not OK, consider this case as "SIM card error"
            status = SIM_ERROR;
        }
    } else {
        // unable to get SIM status
        status = NO_SIM_STATUS;
    }

    set_cellular_signal_strength_antenna_leds(status, bar_num);
}

/*
 * Periodic monitor handler for new incoming SMS
 * @param monitor related periodic monitor entry
 */
static void new_sms_check(periodic_monitor *monitor) {
    // SMS incoming message directory
    static const char *incoming_sms_dir = "/usr/local/cdcs/conf/sms/incoming";
    DIR *dir;
    struct dirent *ent;
    int new_sms = 0;
    if ((dir = opendir(incoming_sms_dir)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            char *suffix;
            if (ent->d_type == DT_REG && !strncmp(ent->d_name, "rxmsg", 5)
                    && (suffix = strstr(ent->d_name, "_unread")) != NULL
                    && !strcmp(suffix, "_unread")) {
                new_sms = 1;
                break;
            }
        }
        closedir(dir);
    }

    if (new_sms) {
        set_led(INFO, "blink");
    } else {
        set_led(INFO, "off");
    }
}

/*
 * Check power status and set power LEDs
 * Reference: Requirement FR-18461
 */
static void power_battery_check() {
    char *battery_present_val = get_rdb("system.battery.present");
    if (battery_present_val) {
        if (!strcmp(battery_present_val, "1")) {
            // battery is present
            // check power source to set power LED
            char *dcin_present_val = get_rdb("system.battery.dcin_present");
            if (dcin_present_val && !strcmp(dcin_present_val, "1")) {
                // AC power is present --> On AC power
                // Power LED: green steady
                set_led(POWER, "green");
            } else {
                // AC power is not present --> On battery power
                // Power LED: green blinking
                set_led(POWER, "green@10:500/1000");
            }
            // check battery capacity to set battery LED
            char *battery_capacity_val = get_rdb("system.battery.capacity");
            if (battery_capacity_val) {
                long int battery_capacity = string_to_long_int(battery_capacity_val);
                if (battery_capacity > 50) {
                    // battery LED: green steady
                    set_led(BATTERY, "green");
                } else if (battery_capacity <= 50 && battery_capacity >= 20) {
                    // battery LED: yellow steady
                    set_led(BATTERY, "yellow");
                } else {
                    // battery LED: red blinking
                    set_led(BATTERY, "red@10:1000/2000");
                }
            } else {
                // battery capacity has not been available
                // do no changes to battery LED
            }
        } else {
            // battery is not present: must be on AC power
            // Power LED: green steady
            // battery LED: Off
            set_led(POWER, "green");
            set_led(BATTERY, "off");
        }
    } else {
        // battery presence status RDB, if it does not exist, will be created during subscribing
        // hence the program should never come to this point.
        // However for completeness and just in case where something is wrong, turn power LED green
        // in this case as the system must have power, assuming AC, to run to this point.
        set_led(POWER, "green");
    }
}

/*
 * Periodic monitor handler for power
 * @param monitor related periodic monitor entry
 */
static void power_battery_periodic_monitor(periodic_monitor *monitor) {
    power_battery_check();
}

/*
 * Power source presence change event trigger handler
 * @param trigger related event trigger
 */
static void power_battery_trigger(event_trigger *trigger) {
    power_battery_check();
}

/* LEDs model implementation */

/*
 * init. See leds_model.h.
 */
static int init(leds_model *self) {
    // add triggers and period monitors

    // power triggers to have fastest response to power source change event
    event_trigger *power_ac_presence = event_trigger__new("system.battery.dcin_present", self,
            power_battery_trigger);
    if (power_ac_presence) {
        self->add_trigger(self, power_ac_presence);
    }
    event_trigger *power_battery_presence = event_trigger__new("system.battery.present", self,
            power_battery_trigger);
    if (power_battery_presence) {
        self->add_trigger(self, power_battery_presence);
    }
    // power monitor in case where trigger is not necessary (e.g battery capacity update)
    periodic_monitor *power_monitor = periodic_monitor__new(self, power_battery_periodic_monitor);
    if (power_monitor) {
        self->add_periodic_monitor(self, power_monitor);
    }

    event_trigger *voice_mail = event_trigger__new("wwan.0.mwi.voicemail.active", self,
            voicemail_rdb_trigger_handler);
    if (voice_mail) {
        self->add_trigger(self, voice_mail);
    }

    periodic_monitor *cellular_leds = periodic_monitor__new(self, cellular_signal_strength_antenna);
    if (cellular_leds) {
        self->add_periodic_monitor(self, cellular_leds);
    }

    periodic_monitor *sms = periodic_monitor__new(self, new_sms_check);
    if (sms) {
        self->add_periodic_monitor(self, sms);
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
 * bordeaux_leds_model__new. See leds_model_bordeaux.h.
 */
leds_model *bordeaux_leds_model__new(leds_model *base_model) {
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
