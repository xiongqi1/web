/*
 * Model implementation of the Quectel RF module for simple_at_manager
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


#include "../util/parse_response.h"
#include "../util/string_utils.h"
#include "../util/sprintf_buffer.h"
#include "model_default.h"
#include "../model/model.h"
#include "cdcs_syslog.h"
#include "../util/scheduled.h"

#include "rdb_ops.h"
#include "../util/rdb_util.h"
#include "../util/at_util.h"
#include "../rdb_names.h"
#include "../featurehash.h"
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>


#include <stdlib.h>  // for system()
#include <errno.h>   // for errno


#include <sys/types.h> // for open()
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>  // for lseek

#include <time.h>   // for time()

// We need to convert a few durations from minutes...
const int SECONDS_PER_MINUTE = 60;

// Names for passing to scheduled_func_schedule() and friends.
#define SCHEDULE_RETRY "gpsone_update_retry"
#define SCHEDULE_CHECK "gpsone_data_file_check"
#define SCHEDULE_WAIT_FOR_RESTART "ec21_restart"

const int EC21_GRACE_RESTART_PERIOD_SECONDS = 60;
const int EC21_RESPONSE_BUFFER_MAXLEN = 4098;

// An exception flag.  Set when something bad happens during this transaction.
// Should be cleared at the start of each transaction (an external call to a
// function listed in model_quectel, or in model_quectel.commands.  This can be
// done by running set_up_common().
//
// If set then functions must return without having any effect.
// The model_quectel, and model_quectel.commands need to return -1 when the
// flag is set.
static bool exception;

// Pointer to response buffer; set by call to set_up_common()
static char *s_response;

// Size of buffer pointed to by s_response; set by call to set_up_common()
static size_t response_max_size;

// An "object" for parsing strings; set by call to set_up_common()
static ParseResponseState prs;



// Called at each of the externally called routines for model_quectel.c
// Sets up the following:
// - the response buffer which has AT command responses placed in it
// - the parse_response "object" which is used to extract particular fields from
//   the AT responses (and from rdb variables as needed).
// - the exception flag.
// It is passed a buffer that is split into two halves, one for the response and
// one for processing it.  This buffer must be created in the calling function
// on the stack.  Using a single buffer is drive by the desire to reduce the
// amount of clutter in the client functions.
//
// Use of stack buffers rather than file scope static ones is driven by the
// desire to not indiscriminantly claim memory resources for this model even if
// it is unused.
static void set_up_common(char *common_buffer, size_t size)
{
    exception = false;
    s_response = common_buffer;
    response_max_size = size / 2;
    prs = set_parse_response_buffer(
        common_buffer + response_max_size,
        size - response_max_size,
        &exception
    );

}



// Convenience function.  Does the argument value field match the test_string
// exactly?  (And does it exist and is the exception flag clear?)
static bool test_arg_value(
    const struct name_value_t *args, const char *test_string
)
{
    return (!exception) && (strcmp(args[0].value, test_string) == 0);
}


// Read an RDB variable into a field (or fields) based on a parse_response() format template.
// Check and sets the exception flag as needed.
static void parse_rdb_variable(
    const char *rdb_var_path,
    const char *rdb_var_name,
    const char *format,
    ...
)
{
    if (exception) {
        return;
    }

    // This value needs to be larger than anything we will encounter.
    // It may need to be adjusted as the function is used for more RDB
    // variables.
    const size_t MAX_RDB_VAR_SIZE = 500;
    char buffer[MAX_RDB_VAR_SIZE];

    const int err_code = rdb_get_single(
        rdb_name(rdb_var_path, rdb_var_name), buffer, MAX_RDB_VAR_SIZE
    );
    if (err_code != 0) {
        SYSLOG_ERR("rdb get failed for '%s:%s'", rdb_var_path, rdb_var_name);
        exception = true;
        return;
    }

    if (buffer[0] == '\0') {
        SYSLOG_ERR("rdb value is empty: '%s:%s'", rdb_var_path, rdb_var_name);
        exception = true;
        return;
    }
    va_list args;
    va_start(args, format);
    parse_response_v(&prs, buffer, format, args);
    va_end(args);
    return;
}


// Return the value of an RDB value as an integer.  If the RDB is not correctly representing an
// integer then the exception flag is set and 0 returned.
static int rdb_get_decimal(const char *path, const char *rdb_var_name)
{
    int value = 0;  // Default value should parse_rdb_variable() fail.
    parse_rdb_variable(path, rdb_var_name, "%d", &value);
    return value;
}


// Return the value of an RDB value as an boolean.  If the RDB is not "0" or "1" then the
// exception flag is set and false returned.
static bool rdb_get_boolean(const char *path, const char *rdb_var_name)
{
    int value = 0;  // Default value should parse_rdb_variable() fail.
    parse_rdb_variable(path, rdb_var_name, "%d", &value);
    if (value == 0) {
        return false;
    }
    if (value == 1) {
        return true;
    }
    exception = true;
    SYSLOG_WARNING("expected boolean %s:%s = '%d", path, rdb_var_name, value);
    return false;
}


// Writes to an RDB variable.
// Check and sets the exception flag as needed.
static void set_rdb_variable(const char *rdb_var_name_prefix, const char *rdb_var_name, const char *value)
{
    if (exception) {
        return;
    }
    const int err_code = rdb_set_single(rdb_name(rdb_var_name_prefix, rdb_var_name), value);
    if (err_code != 0) {
        SYSLOG_ERR("rdb set failed for '%s:%s' = '%s'", rdb_var_name_prefix, rdb_var_name, value);
        exception = true;
    }
}


// Send a AT command.  Just a wrapper to at_send().
// Checks the exception flag first.
// Checks the results values from the call to at_send(), If there's a problem
// then exception is set and NULL returned.  If p_error is NULL then any
// error reported from the module also causes exception to be set.  If it is
// non-NULL then modem errors with result in *p_error to be set to true instead
// of exception.
// If there's no fault then the response string is returned.
static const char *send_and_receive(const char *command, bool *p_error)
{
    if (exception) {
        return NULL;
    }

    int okay;

    const int ret_code = at_send(command, s_response, "", &okay, AT_RESPONSE_MAX_SIZE);

    if (ret_code != 0) {
        SYSLOG_ERR("sending AT command <%s> returned error %d", command, ret_code);
        exception = true;
        return NULL;
    } else if (!okay) {
        if (p_error == NULL) {
            SYSLOG_ERR("sending AT command <%s> was not okay (errored) <%s>", command, at_error_text());
            exception = true;
        } else {
            *p_error = true;
        }
        return NULL;
    } else {
        return s_response;
    }
}


// Gets the output from an AT command that yields a single 0 or 1.
// Should an exception be thrown, the return value will be false.
static bool get_at_boolean(const char *command, const char *template)
{
    const char *response = send_and_receive(command, 0);
    bool result = false;
    parse_response(&prs, response, template, &result);
    return result;
}


// Sets an RDB variable to a decimal integer.
static void set_rdb_decimal(const char *path, const char *name, int value)
{
    // Make the buffer large enough for the largest int we can have.
    const size_t BUFFER_SIZE = sizeof "-2147483648";
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%d", value);
    set_rdb_variable(path, name, buffer);
}




////////////////////////////////////////////////////////////////////////////////





// Is the modem device is a known Quectel one?
// return 1 for yes, 0 for no
// Doesn't access the modem or RDB variables; decision is based purely on the
// passed paramters.  If the manufacturer is Quectel, but the product unknown
// to us then a warning is raised but 0 is still returned.
static int detect_device_type(const char* manufacturer, const char* model_name)
{
    const char* valid_model_names[] = {
        "EC21",
        "EC25",
    };
    const int element_count = sizeof valid_model_names / sizeof (const char*);

    if (strcmp(manufacturer, "Quectel") != 0) {
        return 0;
    }

    int i;
    for (i = 0; i < element_count; i++) {
        if (strcmp(model_name, valid_model_names[i]) == 0) {
            return 1;
        }
    }

    SYSLOG_WARNING("Quectel device other than EC21: (%s)",model_name);
    return 0;
}


// Used by lte_band_frequency_mhz() exclusively.
typedef struct {
    int band; // The LTE band number.
    int frequency_mhz; // The carrier frequency of this band.
} LteBandFrequencyMapping;


// Returns the carrier frequency (in MHz) corresponding to a specified LTE band number.
//
// Values determined from https://en.wikipedia.org/wiki/LTE_frequency_bands
//
// If a band number is not in the table then 0 is returned.
//
// If the exception pointer is not NULL then it is checked and the function aborts without action.
// The exception flag is set to true if the band can't be found.
// then it is set to true as well.
static int lte_band_frequency_mhz(int band, bool *exception)
{
    if (exception && *exception) {
        return 0;
    }
    static const LteBandFrequencyMapping mapping [] = {
        {1, 2100}, {2, 1900}, {3, 1800}, {4, 1700}, {5, 850}, {7, 2600}, {8, 900}, {10, 1700},
        {11, 1500}, {12, 700}, {13, 700}, {14, 700}, {17, 700}, {18, 850}, {19, 850}, {20, 800},
        {21, 1500}, {22, 3500}, {24, 1600}, {25, 1900}, {26, 850}, {27, 800}, {28, 700}, {29, 700},
        {30, 2300}, {31, 450}, {32, 1500}, {33, 2100}, {34, 2100}, {35, 1900}, {36, 1900},
        {37, 1900}, {38, 2600}, {39, 1900}, {40, 2300}, {41, 2500}, {42, 3500}, {43, 3700},
        {44, 700}, {45, 1500}, {46, 5200}, {47, 5900}, {48, 3600}, {50, 1500}, {51, 1500},
        {65, 2100}, {66, 1700}, {67, 700}, {68, 700}, {69, 2600}, {70, 2000}, {71, 600},
        {72, 450}, {74, 1500}, {75, 1500}, {76, 1500}
    };
    const int LTE_MAPPING_SIZE = sizeof mapping / sizeof (LteBandFrequencyMapping);

    int i;
    for (i = 0; i < LTE_MAPPING_SIZE; i++) {
        if (mapping[i].band == band) {
            return mapping[i].frequency_mhz;
        }
    }

    if (exception) {
        *exception = true;
        SYSLOG_ERR("Unknown LTE Band: %d", band);
    } else {
        SYSLOG_WARNING("Unknown LTE Band: %d", band);
    }

    return 0;
}


// Sets the appropriate RDB variable to string that is displayed to the user in the "frequency"
// field of the status page of the WebUI.  Effectively translates the EC21's reported band text
// to a form containing a frequency value as well.
//
// Also sets the UTRA Absolute Radio Frequency Channel Number RDB varible if appropriate.
static void report_band_frequency(const char *band)
{
    int band_num;
    int band_freq_mhz;
    const int BUFFER_SIZE = sizeof "LTE Band XX - XXXXMHz";

    char band_freq_text[BUFFER_SIZE];
    SprintfBufferState state = sprintf_buffer_start(band_freq_text, BUFFER_SIZE, &exception);

    if (sscanf(band, "LTE BAND %d", &band_num)) {
        set_rdb_decimal("", RDB_NETWORK_STATUS".ChannelNumber", band_num);
        band_freq_mhz = lte_band_frequency_mhz(band_num, &exception);
        sprintf_buffer_add(&state, "LTE Band %d - %dMHz", band_num, band_freq_mhz);
    } else {
        set_rdb_variable(RDB_NETWORK_STATUS".ChannelNumber", "", "");
        if (sscanf(band, "WCDMA %d", &band_freq_mhz)) {
            sprintf_buffer_add(&state, "WCDMA %dMHz", band_freq_mhz);
        } else if (sscanf(band, "GSM %d", &band_freq_mhz)) {
            sprintf_buffer_add(&state, "GSM %dMHz", band_freq_mhz);
        } else {
            SYSLOG_WARNING("Unknown Radio Band: '%s'", band);
            set_rdb_variable(RDB_CURRENTBAND, "", band);
            return;
        }
    }
    set_rdb_variable(RDB_CURRENTBAND, "", band_freq_text);

}

// Find out what radio band the modem is using and set the RDB_CURRENTBAND
// variable accordingly.
static void update_band_status()
{
    const char *response = send_and_receive("AT+QNWINFO", 0);
    if (exception) {
        return;
    }

    if (strstr(response, "No Service")) {
        set_rdb_variable(RDB_CURRENTBAND, "", "N/A");
    } else {
        char *access_technology;
        int operator_code;
        char *band;
        int channel_id;

        parse_response(
            &prs,
            response,
            "+QNWINFO: '%s','%d','%s',%d",
            &access_technology, &operator_code, &band, &channel_id
        );

        if (exception) {
            return;
        }

        report_band_frequency(band);
    }
}


// Values to specify which network scan mode is used to select the radio band to
// operate on.  This is the value fed to AT+QCFG="nwscanmode" command.
typedef enum {
    ALL_MODES = 0,
    GSM_ONLY = 1,
    WCDMA_ONLY = 2,
    LTE_ONLY = 3
} NetworkScanMode;


// This structure is used to map a list of preferred band to select the radio
// band from.
typedef struct {
    // What is displayed in the band selection web page (and stored in the rdb
    const char* band_name;
    // The value we feed to AT+QCFG="nwscanmode"
    NetworkScanMode scan_mode;
    // The value we feed to the first field in AT+QCFG="band"
    const int gsm_mask;
    // The value we feed to the second field in AT+QCFG="band"
    const long long unsigned lte_mask;
} BandListElementType;

// Convenience function to map LTE band number onto the Quectel bit mask
#define LTE_BAND_MASK(band) (1LL << ((band) - 1))

// This is the value of the LTE mask that we read back from the EC21 when all LTE bands are
// permitted.  It consists of all bits set from 0 to 39, with the exception of bit 30.  This
// corresponds to all bands from 1 to 40 being enabled except for band 31 (450MHz).
#define ALL_LTE_BAND_MASK 0xbfbfffffffLL

// The list of band options the user may select from.  The bit masks are defined in the Quectel
// AT manual for the AT+QCFG="band" command.
static const BandListElementType ec21bands[] = {
    {"GSM 900E", GSM_ONLY, 0x0001, 0},
    {"GSM DCS 1800", GSM_ONLY, 0x0002, 0},
    {"GSM 850", GSM_ONLY, 0x0004, 0},
    {"GSM PCS 1900", GSM_ONLY, 0x0008, 0},
    {"GSM all", GSM_ONLY, 0x000F, 0},
    {"WCDMA 2100", WCDMA_ONLY, 0x0010, 0},
    {"WCDMA 1900", WCDMA_ONLY, 0x0020, 0},
    {"WCDMA 850", WCDMA_ONLY, 0x0040, 0},
    {"WCDMA 900", WCDMA_ONLY, 0x0080, 0},
    {"WCDMA 800", WCDMA_ONLY, 0x0100, 0},
    {"WCDMA 1700", WCDMA_ONLY, 0x0200, 0},
    {"WCDMA all", WCDMA_ONLY, 0x03F0, 0},
    {"LTE 900 only", LTE_ONLY, 0, LTE_BAND_MASK(8)},
    // The EC-21AU doesn't support LTE B20 (800MHz). Only EC-21E does
    // {"LTE 800 only", LTE_ONLY, 0, LTE_BAND_MASK(20)},
    {"LTE 700 only", LTE_ONLY, 0, LTE_BAND_MASK(28)},
    {"LTE 1800 only", LTE_ONLY, 0, LTE_BAND_MASK(3)},
    {"LTE 2100 only", LTE_ONLY, 0, LTE_BAND_MASK(1)},
    {"LTE 2300 only", LTE_ONLY, 0, LTE_BAND_MASK(40)},
    {"LTE 2600 only", LTE_ONLY, 0, LTE_BAND_MASK(7)},
    {"LTE all", LTE_ONLY, 0, ALL_LTE_BAND_MASK},
    {"All bands", ALL_MODES, 0xFFFF, ALL_LTE_BAND_MASK}
};
static const int BAND_LIST_LENGTH =
    sizeof ec21bands / sizeof (BandListElementType);


// If we have to reset the band list in the Quectel at all then set it to the
// last one in the list ("All bands")
static const int DEFAULT_BAND_INDEX =
    sizeof ec21bands / sizeof (BandListElementType) - 1;


    // This is used to record which, if any, item in ec21bands has been selected.  We need to do this
// so that the selection can be restored following an AT+COPS=x command.
static int current_band_selection;


// Sets the RDB_MODULEBANDLIST to the list of bands that the user may select
// from.  Does this without reference to the module itself.
//
// This function checks and sets the exception flag.
static void set_rdb_band_list(void)
{
    // size based on the total of the lengths of the band_name fields in
    // ec21bands.
    const size_t BUFFER_SIZE = 300;
    char text_buffer[BUFFER_SIZE];
    SprintfBufferState sbs = sprintf_buffer_start(
        text_buffer, BUFFER_SIZE, &exception
    );

    int i;
    for (i = 0; i < BAND_LIST_LENGTH; i++) {
        // Construct the value string from the list of bands.
        if (i > 0) {
            // insert an ampersand between each entry (ie. before all but first)
            sprintf_buffer_add(&sbs, "&");
        }
        sprintf_buffer_add(&sbs, "%02X,%s", i, ec21bands[i].band_name);
    }
    set_rdb_variable(RDB_MODULEBANDLIST, "", text_buffer);
}


// Set the preferred radio band from the RDB_BANDPARAM value.  This is an index
// (hexadecimal representation) to the ec21bands table.
//
// This function checks and sets the exception flag.
// If it has been set at any stage then the RDB_BANDSTATUS variable is set
// accordingly.
//
// Note that this used Quectel specific AT commands.
//
// Expects that set_up_common() has been called already by a function up the
// call tree.
static void set_radio_band(int band)
{
    // Set up a buffer that we can sprintf into.  The LTE mask is 10 digits (40bits) long.
    const size_t BUFFER_SIZE = sizeof "AT+QCFG='band',XXXX,XXXXXXXXXX,0,1";
    char text_buffer[BUFFER_SIZE];
    const BandListElementType element = ec21bands[band];
    SprintfBufferState sbs = sprintf_buffer_start(
        text_buffer, BUFFER_SIZE, &exception
    );

    // Prior to setting the scan mode we seem to need to remove all band restrictions.  This is in
    // order to avoid a CME ERROR 3 (operation not allowed) response.
    sprintf_buffer_add(
        &sbs, "AT+QCFG=\"band\",%X,%llX,0,1", 0xFFFF, ALL_LTE_BAND_MASK
    );
    send_and_receive(text_buffer, 0);

    // Set the scan mode to LTE, GPS, WCDMA or any
    reset_sprintf_buffer(&sbs);
    sprintf_buffer_add(&sbs, "AT+QCFG=\"nwscanmode\",%d,1", element.scan_mode);
    send_and_receive(text_buffer, 0);

    // Set the preferred bands based on the mapping.  Note that the lte_mask is a long long uint.
    reset_sprintf_buffer(&sbs);
    sprintf_buffer_add(
        &sbs, "AT+QCFG=\"band\",%X,%llX,0,1", element.gsm_mask, element.lte_mask
    );
    send_and_receive(text_buffer, 0);

}


static void update_rdb_band_param(unsigned band_index)
{
    // Set the band RDB variable (which is a string hex representation).
    // The string will realistically be 2 digits at most.
    const size_t BUFFER_SIZE = sizeof "XX";
    char text_buffer[BUFFER_SIZE];
    SprintfBufferState sbs = sprintf_buffer_start(
        text_buffer, BUFFER_SIZE, &exception
    );
    sprintf_buffer_add(&sbs, "%02X", band_index);

    // Report the band index to the RDB database.
    // set_rdb_variable(RDB_BANDPARAM, "", text_buffer);
    set_rdb_variable(RDB_BANDCURSEL, "", text_buffer);
}


// This is called from model_default::initialize_band_selection_mode() following a factory reset of
// the router.  The factory reset is meant to return all system settings to a fixed state.  As
// non-volatile storage of the band settings in the EC21 phone module rather than RDB variables, we
// need to explicitely clear them.
void reset_quectel_band_selection()
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);

    set_radio_band(DEFAULT_BAND_INDEX);
    update_rdb_band_param(DEFAULT_BAND_INDEX);
}

// Set the preferred radio band from the RDB_BANDPARAM value.  This is an index
// (hexadecimal representation) to the ec21bands table.
//
// This function checks and sets the exception flag.
// If it has been set at any stage then the RDB_BANDSTATUS variable is set
// accordingly.
static void select_band_from_rdb(void)
{

    // Read the band from the RDB variable.
    unsigned band = 0;
    parse_rdb_variable(RDB_BANDPARAM, "", "%x", &band);

    if (band >= BAND_LIST_LENGTH) {
        SYSLOG_ERR(
            "band number outside valid range (%u >= %u)",
            band, BAND_LIST_LENGTH
        );
        exception = true;
        return;
    }

    current_band_selection = band;
    // Set the set of AT commands to change the band settings.
    set_radio_band(band);

    update_rdb_band_param(band);

    // Report success - or otherwise.
    set_rdb_variable(RDB_BANDSTATUS, "", "[done]");

    if (exception) {
        rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");
    }
}


// Called from model_default.c and plmn.c following any call to AT+COPS
void quectel_band_fix(void)
{
    if (current_band_selection == -1) {
        return;
    }

    set_radio_band(current_band_selection);
}


// We read the band selection preferences from the modem and try to determine
// which RDB_BANDPARAM value that best matches.  If there is no match then
// reset the model to a defined band state (use all bands).
static int read_band_from_pm(void)
{
    // Read the masks indicating the preferred bands the EC21 is working to.
    int scan_mode, gsm_mask, tds_mask;
    unsigned long long lte_mask;
    const char *response;
    response = send_and_receive("AT+QCFG=\"band\"", 0);
    parse_response(
        &prs,
        response,
        "+QCFG: 'band',0x%x,0x%l,0x%x",
        &gsm_mask, &lte_mask, &tds_mask
    );
    (void)tds_mask; // we don't care about this

    // Now read the scan mode to determine what combination of LTE, WCDMA and
    // GSM the Quectel is communicating over.
    response = send_and_receive("AT+QCFG=\"nwscanmode\"", 0);
    parse_response(&prs, response, "+QCFG: 'nwscanmode',%d", &scan_mode);
    if (exception) {
        return -1;
    }

    // Cycle through our list of band combinations to find a match.
    int i;
    int band_index = -1;
    for (i = 0; (i < BAND_LIST_LENGTH) && (band_index < 0); i++) {
        const BandListElementType element = ec21bands[i];
        if (element.scan_mode == scan_mode) {
            // The scan mode in our list matches what the Quectel is using,
            // investigate some more.
            switch (scan_mode) {
                case GSM_ONLY:
                    // Do the preferred GSM bands match?
                    if ((element.gsm_mask & 0x000F) == (gsm_mask & 0x000F)) {
                        band_index = i;
                    }
                    break;
                case WCDMA_ONLY:
                    // Do the preferred WCDMA bands match?
                    if ((element.gsm_mask & 0x03F0) == (gsm_mask & 0x03F0)) {
                        band_index = i;
                    }
                    break;
                case LTE_ONLY:
                    // Do the preferred LTE bands match?
                    if (element.lte_mask == lte_mask) {
                        band_index = i;
                    }
                    break;
                case ALL_MODES:
                    // All bands in all masks have to match.
                    // Note that the WCDMA bands are in the GSM mask.
                    if (
                        (element.gsm_mask == gsm_mask) &&
                        (element.lte_mask == lte_mask)
                    ) {
                        band_index = i;
                    }
                    break;
                default:
                    SYSLOG_ERR("bad scan_mode: %d", scan_mode);
            }
        }

    }
    if (band_index < 0) {
        // There's no match - maybe something else has set the Quectel up.
        // Let's force it to a known state and set the rdb value accordingly.
        SYSLOG_WARNING(
            "Quectel band list in unknown state (sm=%d gsm=%x lte=%llx)",
            scan_mode, gsm_mask, lte_mask
        );
    }
    return band_index;
}


static void report_band_to_rdb(void)
{
    int band_index = read_band_from_pm();

    if (band_index < 0) {
        band_index = DEFAULT_BAND_INDEX;
        set_radio_band(band_index);
    }

    current_band_selection = band_index;
    update_rdb_band_param(band_index);
}


// Updates the two RDB variables for HSPA category (up and down).
static void update_high_speed_packet_access_category(void)
{
    const char *response;
    int hsupa_category;
    int hsdpa_category;

    response = send_and_receive("AT+QCFG=\"hsupacat\"", 0);
    parse_response(&prs, response, "+QCFG: 'hsupacat',%d", &hsupa_category);
    response = send_and_receive("AT+QCFG=\"hsdpacat\"", 0);
    parse_response(&prs, response, "+QCFG: 'hsdpacat',%d", &hsdpa_category);

    set_rdb_decimal("", RDB_HSUCAT, hsupa_category);
    set_rdb_decimal("", RDB_HSDCAT, hsdpa_category);
}


// Update all RDB variables reporting network status.
static void update_network_status(void)
{
    update_high_speed_packet_access_category();
}

// Update priid
static void update_priid()
{
    const char *response = send_and_receive("AT+QMBNCFG=\"List\"", 0);
    if (exception) {
        return;
    }

    int index;
    int selected;
    int activated;
    char *MBN_name;
    int version;
    int release_data;

    parse_response(
            &prs,
            response,
            "+QMBNCFG: \"List\",%d,%d,%d,\"%s\",%x,%d",
            &index, &selected, &activated, &MBN_name, &version, &release_data
    );

    if (exception) {
        return;
    }

    SYSLOG_INFO("MBN_name is %s", MBN_name);
    if(strstr(MBN_name,"Verizon")) {
        SYSLOG_INFO("Setting RDB variable for priid");
        set_rdb_variable("priid_carrier", "", "VZW");
    }

}

// Callback linked to from the model_t structure.
// Behaves as per the default model except for the call to local
// update_band_status()
static int set_status(
    const struct model_status_info_t* chg_status,
    const struct model_status_info_t* new_status,
    const struct model_status_info_t* err_status
)
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);

    if(!at_is_open())
    {
        SYSLOG_WARNING("!at_is_open()");
        return 0;
    }
    update_sim_hint();
    update_roaming_status();

    /* set default band sel mode to auto after SIM card is ready to prevent
     * 3G module is locked in limited service state */

    initialize_band_selection_mode(new_status);

    update_network_status();

    update_network_name();
    update_service_type();

    update_band_status();

    //update_date_and_time();

    /* skip if the variable is updated by other port manager such as cnsmgr or
     * process SIM operation if module supports +CPINC command */
    if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS) || support_pin_counter_at_cmd) {
        update_sim_status();
        /* Latest Sierra modems like MC8704, MC8801 etcs returns error for +CSQ command
         * when SIM card is locked so update signal strength with cnsmgr. */
        if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
            update_signal_strength();
        }
        //update_pdp_status();
    }
    update_ccid();
    update_imsi();

    return 0;
}


#ifdef GPS_ON_AT


// File scope variable, recording how our attempts at a scheduled download of a Qualcomm almanac
// file are going.  Value starts off at sensors.gps.0.gpsone.auto_update.max_retry_count at the
// start of every scheduled download and decremented on each failure.
static int gpsone_file_load_attempts_left;


// Wraps a system call in the models_quectel exception system.
// A non-zero return from the invoked process is treated as an exception.
static void system_call(const char *command_text)
{
    if (exception) {
        return;
    }
    const int ret_val = system(command_text);
    if (ret_val != 0) {
        SYSLOG_ERR("system call '%s' failed with %d", command_text, ret_val);
        exception = true;
    }
}


// How big is the specified file (in bytes)?  Errors set the exception flag.
static size_t get_file_size_bytes(const char *file_path)
{
    if (exception) {
        return 0;
    }

    const int file_desc = open(file_path, O_RDONLY);
    if (file_desc < 0) {
        SYSLOG_ERR("Couldn't open %s: errno: %d", file_path, errno);
        exception = true;
        return 0;
    }

    const size_t size_bytes =  lseek(file_desc, 0L, SEEK_END);

    if (size_bytes < 0) {
        SYSLOG_ERR("Couldn't seek to file end %s: errno: %d", file_path, errno);
        (void)close(file_desc);
        exception = true;
        return 0;
    }

    if (close(file_desc) != 0) {
        SYSLOG_ERR("Couldn't close file %s: errno: %d", file_path, errno);
        exception = true;
    }
    return size_bytes;
}

// Download filename from url to working_dir using wget.  Or set exception trying.
static void download_file(
    const char *url, const char *file_path,
    size_t *file_size_bytes
)
{
    //TODO justify this size
    const size_t BUFFER_SIZE = 300;
    char text_buffer[BUFFER_SIZE];
    SprintfBufferState sbs = sprintf_buffer_start(
        text_buffer, BUFFER_SIZE, &exception
    );

    // delete the file if it already exists.
    sprintf_buffer_add(&sbs, "rm -f %s", file_path);
    system_call(text_buffer);
    reset_sprintf_buffer(&sbs);

    // Download the file using the wget tool.
    sprintf_buffer_add(
        &sbs,
        "/bin/wget --ca-certificate=/etc/ssl/certs/ca-certificates.crt --output-document=%s %s",
        file_path, url
    );
    system_call(text_buffer);

    const size_t size = get_file_size_bytes(file_path);
    if (!exception) {
        *file_size_bytes = size;
    }
}


// Tries to write exactly size_bytes of buffer to the AT interface.  If we fail exception is set.
void at_write_raw_safe(const char *buffer, size_t size_bytes)
{
    if (exception) {
        return;
    }
    const int bytes_written = at_write_raw(buffer, size_bytes);
    if (bytes_written < 0) {
        SYSLOG_ERR("Error writing %d bytes to AT port: errno: %d", size_bytes, errno);
        exception = true;
        return;
    }
    if (bytes_written < size_bytes) {
        SYSLOG_ERR("Wrote only %d of %d bytes to AT port", bytes_written, size_bytes);
        exception = true;
        return;
    }
}


// Writes the contents of file file_path (which is on the router's filesystem to the AT port.
// For use wth file transfer commands like the Quectel AT+QFUPL.
void write_file_to_at_port(const char *file_path) {
    if (exception) {
        return;
    }
    const int file_desc = open(file_path, O_RDONLY);
    if (file_desc < 0) {
        SYSLOG_ERR("Couldn't open file %s: errno: %d", file_path, errno);
        exception = true;
        return;
    }
    const size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    do {
        bytes_read = read(file_desc, buffer, BUFFER_SIZE);
        if (bytes_read < 0) {
            SYSLOG_ERR("Error reading from file %s: errno: %d", file_path, errno);
            exception = true;
            (void)close(file_desc);
            return;
        }

        if (bytes_read > 0) {
            at_write_raw_safe(buffer, bytes_read);
            if (exception) {
                SYSLOG_ERR("Error copying %s to AT port", file_path);
                (void)close(file_desc);
                return;
            }
        }
   } while (bytes_read > 0);

   if (close(file_desc) != 0) {
        SYSLOG_ERR("Error closing file %s: errno: %d", file_path, errno);
        exception = true;
   }
}


// Forward declarations of various GPS related static functions.
static void clear_agps_schedule(void);
static void handle_command_gpsone_enable(void);
static void get_gps_xtra_data_times(
    int *seconds_until_invalid,
    bool *better_than,
    int *almanac_creation_time
);
static void get_random_url(char *result, size_t max_size);
static void load_in_new_gps_one_xtra_file(void);
static void attempt_gpsone_file_update(void);
static void start_attempts_to_download_agps_file(void);
static void start_off_gpsone_download_schedule(void);
static bool disable_gps_engine(void);
static void enable_gps_engine(void);
static int handle_command_gps(const struct name_value_t* args);

// Forward declaration of functions called exclusively through the scheduled_func_schedule()
// mechanism.
static void attempt_gpsone_file_update_callback(void *dummy);
static void start_attempts_to_download_agps_file_callback(void *dummy);
static void start_off_gpsone_download_schedule_callback(void *dummy);
static void update_rdb_almanac_dates(int seconds_left, int almanac_creation_time);


// Abort any of the threee schedules we use for A-GPS.
static void clear_agps_schedule(void) {

    // Regardless of what's happening, halt the existing update schedule, if one exists.
    scheduled_clear(SCHEDULE_CHECK);

    // Just in case we are still retrying to get a file stop doing that too.
    scheduled_clear(SCHEDULE_RETRY);

    // In case we've gpt a restart in progress.
    scheduled_clear(SCHEDULE_WAIT_FOR_RESTART);
}


// The operator has hit the save button on the WebUI A-GPS page, or GPS has been enabled, or SAM
// has just started up.
// This means that A-GPS may need to be turned on or off.  The gpsone file download parameters may
// have changed so we might like to check the file.
static void handle_command_gpsone_enable(void)
{
    clear_agps_schedule();

    const bool agps_meant_to_be_enabled = rdb_get_boolean(RDB_GPS_PREFIX, RDB_GPS_GPSONE_EN);
    const bool gps_extra_running = get_at_boolean("AT+QGPSXTRA?", "+QGPSXTRA: %b");

    if (exception) {
        return;
    }

    if (agps_meant_to_be_enabled) {
        if (gps_extra_running) {
            // We are meant to be running A-GPS and we are already.  Simply redo the schedule.
            start_off_gpsone_download_schedule();
        } else {
            // We need to restart the PM to turn on the A-GPS
            send_and_receive("AT+QGPSXTRA=1", 0);
            SYSLOG_WARNING("Resetting the modem to activate gpsXtra");
            send_and_receive("AT+CFUN=1,1", 0);
            system("reboot_module.sh");

            // Complete the setup of the A-GPS after we've had time to restart.
            scheduled_func_schedule(
                SCHEDULE_WAIT_FOR_RESTART,
                start_off_gpsone_download_schedule_callback,
                EC21_GRACE_RESTART_PERIOD_SECONDS
            );
        }
    } else {
        if (gps_extra_running) {
            // We need to stop the A-GPS
            send_and_receive("AT+QGPSXTRA=0", 0);
        } else {
            // We're not running and aren't meant to be so do nothing.
        }
    }

}


// A crude method to check for an expected output, potentially preceded by other lines.
// The final line must start with the expected text.
// A quirk is that the number of lines must be less than or equal to max_wait_seconds.
static bool wait_for_at_response(const char *expected, int max_wait_seconds)
{
    if (exception) {
        return false;
    }
    const int READ_TIMEOUT_SECONDS = 1;
    int attempts_left = max_wait_seconds;
    do {
        const char *result = direct_at_read(READ_TIMEOUT_SECONDS);
        if (strncmp(result, expected, strlen(expected)) == 0) {
            return true;
        }
        if (--attempts_left == 0) {
            SYSLOG_ERR("Didn't get '%s' response: '%s'",  expected, result);
            exception = true;
            return false;
        }
    } while (true);
}


// Copies a file from local_file_path on the router file system to modem_file_path on the modem's
// file system. The file must be file_size_bytes long.
//
// If anything unexpected happens operations stop and exception is set.
//
// Potentially we might leave the PM in a state where it is still expecting file input when we are
// feeding it AT commands.  If this is the case, the PM's file read timeout should kick in around
// 6 seconds after starting a 60kB almanac file download.
//
//TODO checking the checksum.
static void load_file_to_quectel_modem(
    const char *local_file_path,
    const char *modem_file_path,
    size_t file_size_bytes
)
{
    //TODO justify
    const size_t BUFFER_SIZE = 100;
    char text_buffer[BUFFER_SIZE];
    SprintfBufferState sbs = sprintf_buffer_start(
        text_buffer, BUFFER_SIZE, &exception
    );

    // Allow for a link speed of 10kB/s
    const int timeout_seconds = 1 + file_size_bytes / (10 * 1024);

    sprintf_buffer_add(
        &sbs,
        "AT+QFUPL=\"%s\",%d,%d\r\n",
        modem_file_path, file_size_bytes, timeout_seconds
    );

    if (exception) {
        return;  // before we attempt to measure an undefined string's length.
    }

    const int string_length = strlen(text_buffer);
    at_write_raw_safe(text_buffer, string_length);

    const int maximum_wait_seconds = 3;
    wait_for_at_response("CONNECT", maximum_wait_seconds);

    if (exception) {
        return;
    }

    write_file_to_at_port(local_file_path);

    if (exception) {
        return;
    }

    // 1 second delay after last char as per Quectel doco.
    sleep(1);
    const int plus_plus_plus_length = 3;
    at_write_raw_safe("+++", plus_plus_plus_length);

    wait_for_at_response("OK", maximum_wait_seconds);

}


// Interogate the EC21 to determine the use-by date of the almanac file stored in it.
// * seconds_until_invalid: how much longer is the file go for (0 if no longer good.
// * better_than: true if the file life is potentially longer than seconds_until_invalid
// * almanac_creation_time: when was the file generated by Qualcomm (seconds since epoch)
// If any of the pointers is set to NULL then we don't attempt to update the corresponding value.
static void get_gps_xtra_data_times(
    int *seconds_until_invalid,
    bool *better_than,
    int *almanac_creation_time
)
{
    int year, month, day, hour, minute, second;
    int valid_time_left_minutes;
    const int MAX_REPORTED_DURATION = 10080;

    const char *response = send_and_receive("AT+QGPSXTRADATA?", 0);
    parse_response(
        &prs,
        response,
        "+QGPSXTRADATA: %d,'%d/%d/%d,%d:%d:%d'",
        &valid_time_left_minutes, &year, &month, &day, &hour, &minute, &second
    );
    if (exception) {
        return;
    }
    if (almanac_creation_time) {
        struct tm stm = {
            .tm_year = year - 1900,
            .tm_mon = month - 1,
            .tm_mday = day,
            .tm_hour = hour,
            .tm_min = minute,
            .tm_sec = second
        };
        *almanac_creation_time = mktime(&stm);
    }
    if (seconds_until_invalid) {
        *seconds_until_invalid = valid_time_left_minutes * SECONDS_PER_MINUTE;
    }
    if (better_than) {
        *better_than = (valid_time_left_minutes == MAX_REPORTED_DURATION);
    }
}

// Select, at random, one of the URLs from the comma separated list in the RDB variable
// sensors.gps.0.gpsone.urls. Write the string to the *result* buffer.
//
// We seed our random number generator on the first call to this function rather than during
// start-up because the time might not be adjusted correctly before sam is initiated and therefore
// the time based seeding might always get the same value.
static void get_random_url(char *result, size_t max_size)
{
    static bool seeded = false;

    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }

    const size_t MAX_RDB_VAR_SIZE = 500;
    char buffer[MAX_RDB_VAR_SIZE];

    const int err_code = rdb_get_single(
        rdb_name(RDB_GPS_PREFIX, RDB_GPS_GPSONE_URLS), buffer, MAX_RDB_VAR_SIZE
    );

    if (err_code != 0) {
        SYSLOG_ERR("rdb get failed: %d", err_code);
        exception = true;
        return;
    }

    const size_t MAX_URL_COUNT = 5;
    char *urls[MAX_URL_COUNT];
    size_t count = MAX_URL_COUNT;
    const bool okay = split_string_into_list(buffer, urls, &count, ',');
    if (!okay) {
        SYSLOG_WARNING("More than %d URLs in gpsone.urls RDB var", MAX_URL_COUNT);
    }

    const int random_index = rand() % count;

    const int written = snprintf(result, max_size, "%s", urls[random_index]);
    if ((written < 0) || (written >= max_size)) {
        SYSLOG_ERR(
            "URL writing failed: %d chars written of <%s> (errno: %d)",
            written, urls[random_index], errno
        );
        exception = true;
        return;
    }

}


// Download the latest Qualcomm gpsOneXTRA file from the Internet and then upload it to the modem
// RAM file system. Once it's there we follow the injection procedure outlined in the Quectel GNSS
// manual.
//TODO check for file presence on device prior to upload.
//TODO justify uncertainty_seconds value
static void load_in_new_gps_one_xtra_file(void)
{

    const size_t BUFFER_SIZE = 300;
    char text_buffer[BUFFER_SIZE];
    SprintfBufferState sbs = sprintf_buffer_start(
        text_buffer, BUFFER_SIZE, &exception
    );

    size_t file_size_bytes = 0;

    const bool gps_was_running = disable_gps_engine();
    const size_t MAX_URL_LENGTH = 100;
    char url[MAX_URL_LENGTH];
    get_random_url(url, MAX_URL_LENGTH);

    download_file(url, "/tmp/xtra2.bin", &file_size_bytes);

    load_file_to_quectel_modem("/tmp/xtra2.bin", "RAM:xtra2.bin", file_size_bytes);

    const time_t seconds_since_epoch = time(NULL);
    const struct tm *now = gmtime(&seconds_since_epoch);
    const int uncertainty_seconds = 5;  //TEMP!!!

    // Pass or fail, we update this time.
    set_rdb_decimal(RDB_GPS_PREFIX, RDB_GPS_GPSONE_XTRA_INFO_GNSS_TIME, seconds_since_epoch);

    // First 0 means "Inject gpsOneXTRA time"
    // Date/time stamp in quotes
    // First 1 means "Use UTC time"
    // Second 1 means "Force acceptance"
    sprintf_buffer_add(
        &sbs, "AT+QGPSXTRATIME=0,\"%4d/%02d/%02d,%02d:%02d:%02d\",1,1,%d",
        now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
        now->tm_hour, now->tm_min, now->tm_sec,
        uncertainty_seconds
    );
    send_and_receive(text_buffer, 0);

    reset_sprintf_buffer(&sbs);
    sprintf_buffer_add(&sbs, "AT+QGPSXTRADATA=\"%s\"", "RAM:xtra2.bin");
    send_and_receive(text_buffer, 0);

    reset_sprintf_buffer(&sbs);
    sprintf_buffer_add(&sbs, "AT+QFDEL=\"%s\"", "RAM:xtra2.bin");
    send_and_receive(text_buffer, 0);

    if (gps_was_running) {
        enable_gps_engine();
    }
    if (!exception) {
        SYSLOG_INFO("injection successful");
        sleep(5);  //TEMP!!! otherwise next operation fails.
        // The NTC-6200 Series user manual states that this field is the file "creation" time.  We
        // don't have access to when the file was constructed, so let's just record the download
        // timestamp.
        int seconds_left;
        int almanac_creation_time;
        get_gps_xtra_data_times(&seconds_left, NULL, &almanac_creation_time);
        update_rdb_almanac_dates(seconds_left, almanac_creation_time);
    }
}


// Harness function to allow calling of attempt_gpsone_file_update() through the
// scheduled_func_schedule() mechanism.  (Sets up the common text buffers.)
static void attempt_gpsone_file_update_callback(void *dummy)
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);
    attempt_gpsone_file_update();
}


// We've got to the point we need to automatically update the almanac data file - or we are retrying
// to cover for a previous failure.  Either way, try again.  If this attempt fails check if we have
// more attempts up our sleeve.  If so, schedule the next attempt.
static void attempt_gpsone_file_update(void)
{
    // try to load a file.
    load_in_new_gps_one_xtra_file();
    if (exception) {
        SYSLOG_WARNING("update attempt failed; %d left", gpsone_file_load_attempts_left);
        exception = false;
        if (gpsone_file_load_attempts_left <= 0) {
            SYSLOG_ERR("Ran out of retries to get gpsone file");
            scheduled_clear(SCHEDULE_RETRY);
            return;
        }
        gpsone_file_load_attempts_left -= 1;

    } else {
        // Success; make no further attempts.
        scheduled_clear(SCHEDULE_RETRY);
    }

}


// Harness function to allow calling of start_attempts_to_download_agps_file() through the
// scheduled_func_schedule() mechanism.  (Sets up the common text buffers.)
static void start_attempts_to_download_agps_file_callback(void *dummy)
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);
    start_attempts_to_download_agps_file();
}


// Start up a schedule to make attempts to download an almanac file.
// The schedule is halted upon success or excess failed attempts.
// This function itself is either called via the scheduling framework or directly
static void start_attempts_to_download_agps_file(void)
{
    // Reset the static counter to the permitted number.
    gpsone_file_load_attempts_left = rdb_get_decimal(
        RDB_GPS_PREFIX, RDB_GPS_GPSONE_AUTO_UPDATE_MAX_RETRY_CNT
    );

    // Determine the interval between attempts.
    const int seconds_between_retries = rdb_get_decimal(
        RDB_GPS_PREFIX, RDB_GPS_GPSONE_AUTO_UPDATE_RETRY_DELAY
    );

    // Start up a regime of attempts.  This will be cleared on success or once
    // gpsone_file_load_attempts_left hits 0.
    scheduled_func_schedule(
        SCHEDULE_RETRY, attempt_gpsone_file_update_callback, seconds_between_retries
    );

}


// Start a routine schedule of A-GPS almanac file downloads.  This function is only called from
// begin_gpsone_file_check_schedule()
// Because the scheduling mechanism waits a full period before the first trigger, we also call the
// download function immediately.
static void begin_gpsone_file_check_callback(void *dummy) {
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);
    const int update_interval_minutes = rdb_get_decimal(
        RDB_GPS_PREFIX, RDB_GPS_GPSONE_AUTO_UPDATE_PERIOD
    );
    const int update_interval_seconds = update_interval_minutes * SECONDS_PER_MINUTE;
    if (update_interval_seconds == 0) {
        SYSLOG_ERR("Didn't expect to have this callback called");
        return;
    }
    scheduled_func_schedule(
        SCHEDULE_CHECK, start_attempts_to_download_agps_file_callback, update_interval_seconds
    );

    start_attempts_to_download_agps_file();
}


// Create a schedule for downloading A-GPS almanac files.  Begin this in delay_seconds.
// If 0 seconds is specified then increase this to 1 seconds, just to satisfy the scheduler.
static void begin_gpsone_file_check_schedule(int delay_seconds)
{
    if (delay_seconds == 0) {
        delay_seconds++;
    }
    SYSLOG_INFO("gpsOne file download schedule to start in %d seconds", delay_seconds);
    one_shot_func_schedule(
        SCHEDULE_CHECK, begin_gpsone_file_check_callback, delay_seconds
    );
}



// Harness function to allow calling of start_off_gpsone_download_schedule() through the
// scheduled_func_schedule() mechanism.  (Sets up the common text buffers.)
// Called exclusively from handle_command_gpsone_enable()
static void start_off_gpsone_download_schedule_callback(void *dummy)
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);
    start_off_gpsone_download_schedule();
}


// Either called directly from within handle_command_gpsone_enable() or, if we are having to restart
// the EC21, via scheduled callback.  If the user wants a scheduled update of gpsone files then
// start off this process.
//
// Check if the existing data file is current, if not update now.
// Determine last update.
// Add RDB_GPS_GPSONE_AUTO_UPDATE_PERIOD
// subtract current time.
// If we're due get the file now.  Otherwise schedule its download
static void start_off_gpsone_download_schedule()
{
    const int update_interval_minutes = rdb_get_decimal(
        RDB_GPS_PREFIX, RDB_GPS_GPSONE_AUTO_UPDATE_PERIOD
    );
    const int update_interval_seconds = update_interval_minutes * SECONDS_PER_MINUTE;
    if (update_interval_seconds == 0) {
        // Automatic update is turned off, so don't load a new file regardless of the state of the
        // loaded data.
        return;
    }
    int seconds_left;
    int almanac_creation_time;
    bool better_than;
    get_gps_xtra_data_times(&seconds_left, &better_than, &almanac_creation_time);
    update_rdb_almanac_dates(seconds_left, almanac_creation_time);
    SYSLOG_INFO("gpsXtra data valid for %s %d more seconds", (better_than ? ">" : ""),  seconds_left);
    if (seconds_left <= 0) {
        // The data in the PM is past its "use by" date; update it right now, and set the schedule
        // from this point.
        begin_gpsone_file_check_schedule(0);
        return;
    }

    // The almanac data in the phone module is still good.  Work out when we want to do the next
    // upload this will be the minimum of the next schedule or the expiry date of what's in the
    // module.
    const int last_update_time = rdb_get_decimal(RDB_GPS_PREFIX, RDB_GPS_GPSONE_XTRA_INFO_GNSS_TIME);
    const time_t now = time(NULL);
    const int next_scheduled_time = last_update_time + update_interval_seconds;
    const int seconds_to_next_scheduled_update = next_scheduled_time - now;
    if (seconds_to_next_scheduled_update < 0) {
        // We missed the scheduled update, update now
        begin_gpsone_file_check_schedule(0);
    } else if (seconds_left < seconds_to_next_scheduled_update) {
        // The current data will expire before the scheduled update; update now
        begin_gpsone_file_check_schedule(0);
    } else {
        // Let's resume the set schedule.
        begin_gpsone_file_check_schedule(seconds_to_next_scheduled_update);
    }
}

// Checks if the Quectel is running GNSS and, if so, turn it off.
static bool disable_gps_engine(void)
{
    const bool enabled = get_at_boolean("AT+QGPS?", "+QGPS: %b");
    if (enabled) {
        send_and_receive("AT+QGPSEND", 0);
    } else {
        SYSLOG_INFO("GPS wasn't running");
    }
    return enabled;
}


// Checks if the Quectel is running GNSS and, if not, turn it on.
static void enable_gps_engine(void)
{
    const bool enabled = get_at_boolean("AT+QGPS?", "+QGPS: %b");
    if (!enabled) {
        const bool agps_meant_to_be_enabled = rdb_get_boolean(RDB_GPS_PREFIX, RDB_GPS_GPSONE_EN);
        // 2 for MS-based GPS, 1 for standalone GPS
        if (agps_meant_to_be_enabled) {
            send_and_receive("AT+QGPS=2", 0);
        } else {
            send_and_receive("AT+QGPS=1", 0);
        }
    } else {
        SYSLOG_INFO("GPS was already running");
    }
}


// The GPS engine has been sent a command.
static int handle_command_gps(const struct name_value_t* args)
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);

    /* bypass if incorrect argument */
    if (!args || !args[0].value) {
        SYSLOG_ERR("expected command, got NULL");
        return -1;
    }

    SYSLOG_DEBUG("Command '%s'", args[0].value);
    // Clear any previous GPS errors.
    set_rdb_variable(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE, "");

    if (test_arg_value(args, "disable")) {
        (void)disable_gps_engine();
        clear_agps_schedule();
    } else if (test_arg_value(args, "enable")) {
        enable_gps_engine();
        handle_command_gpsone_enable();
    } else if (test_arg_value(args, "gpsone_enable")) {
        handle_command_gpsone_enable();
    } else if (test_arg_value(args, "gpsone_disable")) {
        handle_command_gpsone_enable();
    } else if (test_arg_value(args, "gpsone_update")) {
        load_in_new_gps_one_xtra_file();
        set_rdb_decimal(RDB_GPS_PREFIX, RDB_GPS_GPSONE_UPDATED, 1);
    } else {
        SYSLOG_ERR(
            "don't know how to handle '%s':'%s'",
            args[0].name, args[0].value
        );
        return -1;
    }

    if (exception) {
        set_rdb_variable(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS, "[error]");
        return -1;
    }
    set_rdb_variable(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS, "[done]");

    return 0;
}


// Read the A-GPS almanac data status from the phone module and update the associated RDB variables.
// The values passed are the raw ones reported by the AT+QGPSXTRADATA? command.  This function
// adjusts them so that if they are invalid/expired then the corresponding RDB variables are set to
// 0.
static void update_rdb_almanac_dates(int seconds_left, int almanac_creation_time)
{
    // If the EC21 doesn't have almanac data at all then the expiry data is reported as
    // 1980-01-05.  We want to report this case by setting the corresponding RDB variable to 0.
    // The following threshold date that we check against corresponds to 1980-02-22.
    const int quectel_invalid_date_threshold = 320000000;

    if (almanac_creation_time > quectel_invalid_date_threshold) {
        set_rdb_decimal(RDB_GPS_PREFIX, RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME, almanac_creation_time);
    } else {
        set_rdb_decimal(RDB_GPS_PREFIX, RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME, 0);
    }
    if (seconds_left > 0) {
        const time_t now = time(NULL);
        set_rdb_decimal(RDB_GPS_PREFIX, RDB_GPS_GPSONE_XTRA_INFO_VALID_TIME, seconds_left + now);
    } else {
        set_rdb_decimal(RDB_GPS_PREFIX, RDB_GPS_GPSONE_XTRA_INFO_VALID_TIME, 0);
    }
}


#endif  /* GPS_ON_AT */


// Callback linked to from the model_t structure.
// Sets up static variables for future use and writes the list of valid band
// to the RDB.
static int initialize_device(void)
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);

    // disable quectel internal dhcp-server which interfere with dhcp-relay function
    bool command_failed;
    send_and_receive("AT+QCFG=\"dhcppktfltr\",1", &command_failed);
    if (command_failed) {
        SYSLOG_WARNING("Failed to disable internal dhcp-server");
    }

    set_rdb_band_list();

    // Read the band restriction regime stored in the PM.
    current_band_selection = read_band_from_pm();
    update_rdb_band_param(current_band_selection);


    // Enable network registration unsolicited result code with location information
    // Doing this allows model_option::option_update_misc() to read the location area code and cell
    // ID.
    send_and_receive("AT+CREG=2", 0);

    #ifdef GPS_ON_AT
    {
        const bool gps_enabled = rdb_get_boolean(RDB_GPS_PREFIX, "enable");
        if (gps_enabled) {
            enable_gps_engine();
            handle_command_gpsone_enable();
        } else {
            (void)disable_gps_engine();
            clear_agps_schedule();
        }

        const bool agps_enabled = rdb_get_boolean(RDB_GPS_PREFIX, RDB_GPS_GPSONE_EN);
        // Only run the following if A-GPS is enabled, otherwise the AT+QGPSXTRADATA? command will
        // fail with "CME ERROR: 509"
        if (agps_enabled) {
            int seconds_left;
            int almanac_creation_time;
            get_gps_xtra_data_times(&seconds_left, NULL, &almanac_creation_time);
            update_rdb_almanac_dates(seconds_left, almanac_creation_time);
        }

    }
    #endif

#if defined(MODULE_EC21)
    // update priid
    char *revision;
    int ok;
    char response[EC21_RESPONSE_BUFFER_MAXLEN];

    const int err_code=(at_send("ATI", response, "", &ok, 0)==0) && ok;

    /* bypass if it fails */
    if(!err_code) {
        SYSLOG_ERR("failed to get response from ATI");
        return -1;
    }

    revision=strstr(response,"Revision: ");
    if(!revision) {
        SYSLOG_ERR("Revision does not exist in the response of ATI");
        return -1;
    }

    SYSLOG_INFO("revision is %s", revision);
    if (strstr(revision, "EC21V")) {
        SYSLOG_INFO("Calling update_priid() ");
        update_priid();
    }
#endif

    return 0;
}


// Get or set the details of the preferred radio bands used by the Quectel.
static int handle_command_band(const struct name_value_t* args)
{
    char buffer[2 * AT_RESPONSE_MAX_SIZE];
    set_up_common(buffer, sizeof buffer);

    if (test_arg_value(args, "get")) {
        report_band_to_rdb();
    } else if (test_arg_value(args, "set")) {
        select_band_from_rdb();
    } else {
        return -1;
    }
    if (exception) {
        return -1;
    } else {
        return 0;
    }

}


// This structure maps particular RDB command variables onto routines expected
// to service them. This is assigned to the model_quectel.commands field.
const static struct command_t commands[] =
{
    #ifdef GPS_ON_AT
       {.name = RDB_GPS_PREFIX".0."RDB_GPS_CMD, .action = handle_command_gps},
    #endif
    {.name = RDB_BANDCMMAND, .action = handle_command_band},

    {0,}
};



// This structure is accessed by model.c:model_find()
// It needs to be non-const because (at least) model.c::model_init() writes to
// the .variants field.
struct model_t model_quectel = {
    .name = "quectel",
    .detect = detect_device_type,
    .init = initialize_device,

    .get_status = model_default_get_status,
    .set_status = set_status,
    .commands = commands,
    .notifications = model_default_notifications
};
