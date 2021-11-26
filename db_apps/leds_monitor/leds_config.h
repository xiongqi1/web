#ifndef LEDS_CONFIG_H_10405501082018
#define LEDS_CONFIG_H_10405501082018
/**
 * @file leds_config.h
 *
 * Header file for configuration and mapping of RDB names to sysfs entries.
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
#include <stdint.h>

/*
 * Constants
 */
const uint32_t RGB24_SHIFT_R = 16;
const uint32_t RGB24_SHIFT_G = 8;
const uint32_t RGB24_SHIFT_B = 0;

/**
 * Map RDB variable to applicable LED sysfs entries
 *
 * Each RDB can be mapped to three LEDs corresponding to red, green, and blue colours, depending
 * on which control lines are available.  Calling code that doesn't care about what the LED
 * colours are can use generic values in RDB (such as "on") which will be mapped to the
 * actual/available LEDs via this structure.
 */
typedef struct {
    const char *name;       ///< name of corresponding RDB variable
    const char *ledR;       ///< sysfs entry name for red LED (or NULL)
    const char *ledG;       ///< sysfs entry name for green LED (or NULL)
    const char *ledB;       ///< sysfs entry name for blue LED (or NULL)
    const char *reset;      ///< initial/reset value for RDB if RDB not set
} ledMap_t;

/**
 * Map colour names to their corresponding RGB24 value
 */
typedef struct {
    const char *name;   ///< name or alias for colour
    uint32_t colour;    ///< RGB24 format value for colour (e.g., 0xff0000 for "red")
} colourMap_t;

/**
 * Function prototype for ledMap_t name callbacks
 */
typedef bool (* nameCallback_f)(const char *name);


/**
 * Return RDB LED mapping entry for a given RDB name
 *
 * @param name Name of the RDB variable - this is case sensitive.
 * @return pointer to matching ledMap_t entry, or NULL if not found.
 */
const ledMap_t *getLedMapByName(const char *name);

/**
 * Iterate RDB LED mapping and perform callback for each entry name.
 *
 * Iterates through the current ledMap_t list, calling the callback function for each name in the
 * list.
 *
 * @param callback Callback function to be invoked with each ledMap_t name.
 * @param stopOnFalse Iteration stops if the called function returns false.
 */
void iterateLedMapByName(nameCallback_f callback, bool stopOnFalse = false);

/**
 * Return colour value in RGB24 format for a given colour name
 *
 * Colour names are assigned a numeric value using RGB24 format (8 bits each for red, green, and
 * blue, packed in a single 24-bit value).
 *
 * @param name Colour name, or alias (e.g., "on", "red", "white", etc.), or a RGB24 value encoded
 *        as hexadecimal (e.g., "ffa07a").  Note that the comparsion is case-insensitive.
 * @param[out] p_colour RGB24 value matching the given name
 * @return true if the colour name was found or successfully parsed, or false otherwise
 */
bool findColourByName(const char *name, uint32_t *p_colour);

/**
 * Return red component of given RGB24 colour value
 */
static inline uint8_t getRgb24R(uint32_t colour) {
    return (uint8_t)((colour >> RGB24_SHIFT_R) & 0xff);
}

/**
 * Return green component of given RGB24 colour value
 */
static inline uint8_t getRgb24G(uint32_t colour) {
    return (uint8_t)((colour >> RGB24_SHIFT_G) & 0xff);
}

/**
 * Return blue component of given RGB24 colour value
 */
static inline uint8_t getRgb24B(uint32_t colour) {
    return (uint8_t)((colour >> RGB24_SHIFT_B) & 0xff);
}

#endif  // LEDS_CONFIG_H_10405501082018
