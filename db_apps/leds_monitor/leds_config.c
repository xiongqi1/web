/**
 * @file leds_config.c
 *
 * Configuration and mapping of RDB names to sysfs entries.
 *
 *//*
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
#include "leds_config.h"

#include <stdlib.h>
#include <string.h>

/**
 * Configuration for mapping RDB variables to applicable LED sysfs names
 */
static const ledMap_t s_ledConfig[] = {
#ifdef V_IOBOARD_bordeaux
    {"system.leds.power", NULL, "power_g", NULL, "off"},
    {"system.leds.voicemail", NULL, "voicemail_g", NULL, "off"},
    {"system.leds.info", NULL, "info_g", NULL, "off"},
    {"system.leds.battery", "battery_r", "battery_g", "battery_b", "off"},
    {"system.leds.antenna", "sig_ant_r", "sig_ant_g", "sig_ant_b", "off"},
    {"system.leds.signal.0", NULL, "sig_b1_g", "sig_b1_b", "off"},
    {"system.leds.signal.1", NULL, "sig_b2_g", "sig_b2_b", "off"},
    {"system.leds.signal.2", NULL, "sig_b3_g", "sig_b3_b", "off"},
    {"system.leds.signal.3", NULL, "sig_b4_g", "sig_b4_b", "off"},
#elif defined(V_IOBOARD_nrb0206) || defined(V_IOBOARD_lark)
    {"system.leds.0", "omega2:red:led0", "omega2:green:led0", NULL, "off"},
    {"system.leds.1", "omega2:red:led1", "omega2:green:led1", NULL, "off"},
    {"system.leds.2", "omega2:red:led2", "omega2:green:led2", NULL, "off"},
#endif
    {NULL, NULL, NULL, NULL, NULL}
};

/**
 * Configuration for colours
 */
static const colourMap_t s_colourConfig[] = {
    {"red", 0xff0000},
    {"green", 0x00ff00},
    {"blue", 0x0000ff},
    {"yellow", 0xffff00},
    {"magenta", 0xff00ff},
    {"cyan", 0x00ffff},
    {"white", 0xffffff},
    {"on", 0xffffff},
    {"off", 0x000000},
    {NULL, 0}
};


/*
 * getLedMapByName
 */
const ledMap_t *getLedMapByName(const char *name)
{
    const ledMap_t *p_config = s_ledConfig;
    while (p_config->name) {
        if (strcmp(p_config->name, name) == 0) {
            return p_config;
        }
        ++p_config;
    }
    return NULL;
}

/*
 * iterateLedMapByName
 */
void iterateLedMapByName(nameCallback_f callback, bool stopOnFalse)
{
    const ledMap_t *p_config = s_ledConfig;
    while (p_config->name) {
        if (!callback(p_config->name) && stopOnFalse) {
            break;
        }
        ++p_config;
    }
}

/*
 * findColourByName
 */
bool findColourByName(const char *name, uint32_t *p_colour)
{
    // Look in config table first
    const colourMap_t *p_config = s_colourConfig;
    while (p_config->name) {
        if (strcasecmp(p_config->name, name) == 0) {
            *p_colour = p_config->colour;
            return true;
        }
        ++p_config;
    }

    // Attempt to parse name as a hex sequence instead
    char *p_end = NULL;
    uint32_t colour = strtoul(name, &p_end, 16);
    if (p_end && (p_end != name) && (*p_end == '\0')) {
        *p_colour = colour;
        return true;
    }

    // Could not parse
    return false;
}
