/**
 * @file leds_handler.c
 *
 * Implementation for LED action handlers/pattern processing.
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
#include "logger.h"
#include "leds_config.h"
#include "leds_handler.h"
#include "leds_sysfs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 */
static const char SYSFS_LEDS_ENTRY_CDCS_BRIGHTNESS_ON[] = "brightness_on";
static const char SYSFS_LEDS_ENTRY_CDCS_BRIGHTNESS_OFF[] = "brightness_off";
static const char SYSFS_LEDS_ENTRY_CDCS_TEMPO[] = "trigger_pulse";
static const char SYSFS_LEDS_ENTRY_CDCS_REST[] = "trigger_dead";
static const char SYSFS_LEDS_ENTRY_CDCS_PATTERN[] = "pattern";
static const char SYSFS_LEDS_ENTRY_CDCS_MODE[] = "force";

static const char LEDS_TRIGGER_CDCS_BEAT[] = "cdcs-beat";
static const char LEDS_TRIGGER_CDCS_MODE_OFF[] = "off";
static const char LEDS_TRIGGER_CDCS_MODE_ON[] = "on";
static const char LEDS_TRIGGER_CDCS_MODE_PATTERN[] = "trigger";


/**
 * Function prototype for LED action handlers
 */
typedef bool (* handler_f)(const ledMap_t *p_ledMap, const char *name, const char *params);

/**
 * Map handler names to a specific handler function
 */
typedef struct {
    const char *name;
    handler_f handler;
} handlerMap_t;


/*
 * Forward declarations
 */
static bool blinkHandler(const ledMap_t *p_ledMap, const char *, const char *params);
static bool fastblinkHandler(const ledMap_t *p_ledMap, const char *, const char *params);


/**
 * Configuration mapping for handler names to their specific functions
 *
 * Handler names are not case sensitive.
 *
 * @note Because parameter names are searched for against this list before attemping to process
 *       using the default colour handler, handler names should NOT be made the same as an
 *       existing (or possible) colour name.
 */
static const handlerMap_t s_handlerConfig[] = {
    {"blink", &blinkHandler},
    {"fastblink", &fastblinkHandler},
    {NULL, NULL}
};


/**
 * Parses a parameter string for a blink pattern
 *
 * @param params This is expected to be in the form "[pattern:]tempo/cycle", where:
 *        - pattern is a string of 1 and 0 digits describing a binary value, with a length
 *          between 1 and 32 digits.
 *        - tempo is a decimal value (in msec) that must be greater than 0.
 *        - cycle is a decimal value (in msec) that must be greater than or equal to 0.
 *        - if params is empty or NULL, then a pattern of 1 will be used (tempo and cycle of 0)
 *          i.e LED is set to steady on state.
 *        - if only a tempo and cycle are given, a pattern of 2 will be used.
 *        - see invokeDefaultHandler for more on this.
 * @param[out] p_bitPattern The pattern value determined by the function (if true returned).
 * @param[out] p_bitMask The mask for the pattern indicating which bits are included (if true
 *             returned).
 * @param[out] p_tempoMsec The tempo value determined by the function (if true returned).
 * @param[out] p_cycleMsec The cycleMsec value determined by the function (if true returned).
 * @return true if the params was interpreted as a valid patterns,; or false otherwise, in which
 *         case none of the out parameters will be altered.
 */
static bool parseBlinkPattern(const char *params, uint32_t *p_bitPattern, uint32_t *p_bitMask, uint32_t *p_tempoMsec, uint32_t *p_cycleMsec)
{
    uint32_t tempoMsec = 0;
    uint32_t cycleMsec = 0;
    uint32_t bitPattern = 0;
    uint32_t bitMask = 0;
    bool valid = true;

    if (params && (*params != '\0')) {
        const char *p_start = params;
        char *p_end = NULL;

        // Read pattern (binary)
        bitPattern = strtoul(p_start, &p_end, 2);
        if (p_end && (p_end != p_start) && (*p_end == PATTERN_DELIMITER)) {
            // we have a pattern
            bitMask = (1 << ((uint32_t)(p_end - p_start))) - 1;
            p_start = (const char *)(p_end + 1);
        }
        else {
            // no pattern given, assume default (b10) and leave p_start as it was
            bitPattern = 2;
            bitMask = 3;
        }

        // Read tempo and cycle (decimal)
        tempoMsec = strtoul(p_start, &p_end, 10);
        if (p_end && (*p_end == PATTERN_TEMPO_DELIMITER) && (tempoMsec > 0)) {
            p_start = (const char *)(p_end + 1);
            cycleMsec = strtoul(p_start, &p_end, 10);
            if (!p_end || (*p_end != '\0')) {
                // not a valid cycle
                valid = false;
            }
        }
        else {
            // not a valid tempo
            valid = false;
        }
    }
    else {
        // no pattern expression, treat as "on"
        bitPattern = 1;
        bitMask = 1;
    }

    if (valid) {
        *p_bitPattern = bitPattern;
        *p_bitMask = bitMask;
        *p_tempoMsec = (bitMask > 1) ? tempoMsec : 0;
        *p_cycleMsec = (bitMask > 1) ? cycleMsec : 0;
    }
    return valid;
}

/**
 * Applies a blink pattern to a specific LED via CDCS triggers
 *
 * @param name Name of the LED device in the sysfs file system
 * @param brightness Brightness value to apply to the LED
 * @param cdcsPattern Pattern for the cdcs-beat trigger handler
 * @param tempoMsec Tempo value for the cdcs-beat trigger handler
 * @param restMsec Rest value for the cdcs-beat trigger handler
 * @param invert Flag to indicate if brightness value should be inverted for the cdcs-beat pattern
 */
static void setLedForCdcs(const char *name, uint32_t brightness, uint32_t cdcsPattern, uint32_t tempoMsec, uint32_t restMsec, bool invert)
{
    assert(name);

    // The LED trigger driver will not do actual change if same mode is set.

    if (setSysLed(name, SYSFS_LEDS_ENTRY_TRIGGER, LEDS_TRIGGER_CDCS_BEAT)) {
        if (brightness == 0) {
            // Set LED to off, no pattern
            if (setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_BRIGHTNESS_OFF, 0)
                    && setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_MODE, LEDS_TRIGGER_CDCS_MODE_OFF)) {
                logDebug("Set LED %s to off", name);
            }
            else {
                logWarning("Could not set LED %s to off", name);
            }
        }
        else if (cdcsPattern == 0) {
            // set LED to on, no pattern
            if (setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_BRIGHTNESS_ON, brightness)
                    && setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_MODE, LEDS_TRIGGER_CDCS_MODE_ON)) {
                logDebug("Set LED %s to on (brightness %u)", name, brightness);
            }
            else {
                logWarning("Could not set LED %s to on", name);
            }
        }
        else {
            // set LED to pattern
            if (setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_BRIGHTNESS_ON, invert ? 0 : brightness)
                    && setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_BRIGHTNESS_OFF, invert ? brightness : 0)
                    && setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_TEMPO, tempoMsec)
                    && setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_REST, restMsec)
                    && setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_PATTERN, cdcsPattern)
                    && setSysLed(name, SYSFS_LEDS_ENTRY_CDCS_MODE, LEDS_TRIGGER_CDCS_MODE_PATTERN)) {
                logDebug("Set LED %s to brightness %u/%u, pattern 0x%08x, tempo %u, rest %u", name, invert ? 0 : brightness, invert ? brightness : 0, cdcsPattern, tempoMsec, restMsec);
            }
            else {
                logWarning("Could not set LED %s to pattern 0x%08x", name, cdcsPattern);
            }
        }
    }
    else {
        logWarning("Could not set LED %s trigger to \"%s\"", name, LEDS_TRIGGER_CDCS_BEAT);
    }
}

/**
 * Applies a blink pattern to a LED map using the CDCS triggers
 *
 * See invokeDefaultHandler for more details on the bitPattern/bitMask, tempoMsec, and cycleMsec
 * and how these are intended to work.
 *
 * @param p_ledMap Describes the LEDs on which the pattern must be applied.
 * @param colour The RGB24 colour value to be used.
 * @param bitPattern The blink bit pattern to be used.
 * @param bitMask A bit mask indicating which bits of bitPattern are relevant.
 * @param tempoMsec The timing interval for each bit in the pattern, in msec.
 * @param cycleMsec The overall timing interval for one cycle of the bit pattern, in msec.
 * @return true if the parameters were successfully applied, or false otherwise.
 */
static bool applyBlinkPatternForCdcs(const ledMap_t *p_ledMap, uint32_t colour, uint32_t bitPattern, uint32_t bitMask, uint32_t tempoMsec, uint32_t cycleMsec)
{
    logDebug("applyBlinkPatternForCdcs(\"%s\", 0x%06x, 0x%08x, 0x%08x, %u, %u)", p_ledMap->name, colour, bitPattern, bitMask, tempoMsec, cycleMsec);

    uint32_t cdcsPattern = 0;
    uint32_t restMsec = 0;
    bool invert = false;

    if (bitPattern == 0) {
        // Force colour to zero to set all LEDs off
        colour = 0;
    }
    else if (bitPattern < bitMask) {
        // A valid pattern has been given.  The cdcs-beat timer feature processes bits in a
        // right-to-left order, and always sets the LED to brightness-off when not running the
        // pattern (when in a rest state), so we need to both reverse the bit-order, and
        // potentially invert the on/off sense (so "on" means to actually turn LED off) if the
        // final bit our left-to-right pattern is a 1, as that is the bit we "repeat" until the
        // sequence finishes.  Likewise cdcs-beat will stop (go to rest state) as soon as there
        // are no more bits in the pattern, so we have to adjust the rest timings appropriately.
        if (bitPattern & 0x01) {
            invert = true;
            bitPattern = ~bitPattern & bitMask;
        }
        while ((bitPattern & 0x01) == 0) {
            restMsec += tempoMsec;
            bitPattern >>= 1;
        }
        // e.g., pattern b10111000 --> cdcsPattern b11101 (rest 300)
        uint32_t totalMsec = 0;
        while (bitPattern > 0) {
            cdcsPattern = (cdcsPattern << 1) | (bitPattern & 0x01);
            totalMsec += tempoMsec;
            bitPattern >>= 1;
        }
        // e.g., "10111000:100/1000" --> cdcsPattern b11101, tempo 100, rest 500
        if (cycleMsec > totalMsec) {
            restMsec = cycleMsec - totalMsec;
        }
    }

    // Apply to LEDs
    if (p_ledMap->ledR && (*p_ledMap->ledR != '\0')) {
        setLedForCdcs(p_ledMap->ledR, getRgb24R(colour), cdcsPattern, tempoMsec, restMsec, invert);
    }
    if (p_ledMap->ledG && (*p_ledMap->ledG != '\0')) {
        setLedForCdcs(p_ledMap->ledG, getRgb24G(colour), cdcsPattern, tempoMsec, restMsec, invert);
    }
    if (p_ledMap->ledB && (*p_ledMap->ledB != '\0')) {
        setLedForCdcs(p_ledMap->ledB, getRgb24B(colour), cdcsPattern, tempoMsec, restMsec, invert);
    }

    return true;
}

/**
 * Provides a 1Hz blink option
 *
 * @param p_ledMap Describes the LEDs on which the handler must act.
 * @param name Name of the handler (UNUSED)
 * @param params Colour to apply the blink action to.
 * @return true if successfully processed, or false otherwise.
 */
static bool blinkHandler(const ledMap_t *p_ledMap, const char *, const char *params)
{
    // "blink[@colour]" --> "colour@10:500/1000"
    return invokeDefaultHandler(p_ledMap, params ? params : "on", "10:500/1000");
}

/**
 * Provides a 10Hz blink option
 *
 * @param p_ledMap Describes the LEDs on which the handler must act.
 * @param name Name of the handler (UNUSED)
 * @param params Colour to apply the blink action to.
 * @return true if successfully processed, or false otherwise.
 */
static bool fastblinkHandler(const ledMap_t *p_ledMap, const char *, const char *params)
{
    // "fastblink[@colour]" --> "colour@10:50/100"
    return invokeDefaultHandler(p_ledMap, params ? params : "on", "10:50/100");
}


/*
 * invokeHandlerByName
 */
bool invokeHandlerByName(const ledMap_t *p_ledMap, const char *name, const char *params)
{
    assert(p_ledMap);
    assert(name);

    // Look in handler config table first
    const handlerMap_t *p_config = s_handlerConfig;
    while (p_config->name) {
        if (strcasecmp(p_config->name, name) == 0) {
            return p_config->handler(p_ledMap, name, params);
        }
        ++p_config;
    }

    // Try default handler instead
    return invokeDefaultHandler(p_ledMap, name, params);
}

/*
 * invokeDefaultHandler
 */
bool invokeDefaultHandler(const ledMap_t *p_ledMap, const char *name, const char *params)
{
    uint32_t colour = 0;
    uint32_t bitPattern = 0;
    uint32_t bitMask = 0;
    uint32_t tempoMsec = 0;
    uint32_t cycleMsec = 0;

    if (!findColourByName(name, &colour)) {
        logWarning("Could not parse \"%s\" as a colour", name);
        return false;
    }

    if (!parseBlinkPattern(params, &bitPattern, &bitMask, &tempoMsec, &cycleMsec)) {
        logWarning("Could not parse \"%s\" as a pattern", params);
        return false;
    }

    return applyBlinkPatternForCdcs(p_ledMap, colour, bitPattern, bitMask, tempoMsec, cycleMsec);
}


/*
 * doLedUpdate
 */
bool doLedUpdate(const char *rdbName, const char *rdbValue)
{
    assert(rdbName);
    assert(rdbValue);

    logDebug("doLedUpdate(\"%s\", \"%s\")", rdbName, rdbValue);
    const ledMap_t *p_ledMap = getLedMapByName(rdbName);
    if (!p_ledMap) {
        logError("Could not find LED map for \"%s\"", rdbName);
        return false;
    }

    // Split rdbValue into name@params pair, allocating new memory for the name string if required
    size_t nameLen = 0;
    char *name = NULL;
    const char *params = rdbValue;
    while (*params != '\0') {
        if (*params == PARAM_DELIMITER) {
            name = strndup(rdbValue, nameLen);
            if (!name) {
                logWarning("Could not allocate strdup memory (%u characters)", (unsigned)nameLen);
                return false;
            }
            ++params;
            break;
        }
        ++nameLen;
        ++params;
    }

    // Ignore params if empty
    if (*params == '\0') {
        params = NULL;
    }

    bool result = invokeHandlerByName(p_ledMap, name ? name : rdbValue, params);

    if (name) {
        free(name);
    }

    return result;
}

/*
 * doLedReset
 */
bool doLedReset(const char *rdbName)
{
    assert(rdbName);

    const ledMap_t *p_ledMap = getLedMapByName(rdbName);
    if (!p_ledMap) {
        logError("Could not find LED map for \"%s\"", rdbName);
        return false;
    }
    if (!p_ledMap->reset || (*p_ledMap->reset == '\0')) {
        logNotice("No reset value provided for RDB \"%s\"", rdbName);
        return false;
    }

    return doLedUpdate(rdbName, p_ledMap->reset);
}

