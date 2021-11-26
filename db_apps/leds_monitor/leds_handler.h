#ifndef LEDS_HANDLER_H_10405501082018
#define LEDS_HANDLER_H_10405501082018
/**
 * @file leds_handler.h
 *
 * Header file for LED action handlers/pattern processing.
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

/*
 * Constants
 */
const char PARAM_DELIMITER = '@';
const char PATTERN_DELIMITER = ':';
const char PATTERN_TEMPO_DELIMITER = '/';

/**
 * Finds the handler matching the given name and calls it
 *
 * @param p_ledMap Describes the LEDs on which the handler must act.
 * @param name This is the case-insensitive name of the handler to use.
 * @param params This is handler-specific parameters to be processed by the handler.
 * @return true if successfully processed, or false otherwise.
 */
bool invokeHandlerByName(const ledMap_t *p_ledMap, const char *name, const char *params);

/**
 * Provides default colour-and-pattern blink option
 *
 * @param p_ledMap Describes the LEDs on which the handler must act.
 * @param name This is the case-insensitive name of a colour (e.g., "red", "green", "yellow"), an
 *        alias (e.g., "on", "off"), or an RGB24 colour value in hex (e.g., "ffa07a").
 * @param params This is expected to be in the form "[pattern:]tempo/cycle", where:
 *        - pattern is a string of 1 and 0 digits describing the pattern in first (leftmost) to
 *          last (rightmost) order, where a 1 digit indicates the LED is on (brightness as per
 *          colour), and a 0 digit indicates it is off (brightness 0).  If no pattern is given
 *          but a tempo and cycle value is, then a default pattern of "10" is assumed.  Up to 32
 *          digits can be specified in the pattern.
 *        - tempo is the duration (in msec) to "play" each of the pattern digits for.
 *        - cycle is the desired duration (in msec) in which the entire pattern is repeated, where
 *          the duration of the last-digit of the pattern is extended (above its normal tempo
 *          duration) to meet this overall cycle time.  Note that the cycle time does not need to
 *          be an exact multiple of the tempo, and if the pattern length in tempo units is longer
 *          than the cycle duration, then the pattern will cycle without extending the duration of
 *          the last digit.
 *        - if params is empty or NULL, then the LED will simply be turned on (no pattern) unless
 *          the color (name parameter) is zero brightness for that LED, in which case it will be
 *          set to off instead.
 * @return true if successfully processed, or false otherwise.
 */
bool invokeDefaultHandler(const ledMap_t *p_ledMap, const char *name, const char *params);

/**
 * Performs an update of the LEDs for a given RDB variable and value
 *
 * @param rdbName Name of the RDB variable
 * @param rdbValue Value assigned to the RDB variable, which will be parsed as being of the form
 *        "name[@param]", where name is a the name of a handler (see invokeHandlerByName).
 * @return true if successfully updated, or false otherwise.
 */
bool doLedUpdate(const char *rdbName, const char *rdbValue);

/**
 * Performs a reset of the LEDs for a given RDB variable
 *
 * @param rdbName Name of the RDB variable
 * @return true if successfully reset, or false otherwise.
 */
bool doLedReset(const char *rdbName);

#endif  // LEDS_HANDLER_H_10405501082018
