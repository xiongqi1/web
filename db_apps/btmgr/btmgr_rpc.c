/*
 * btmgr_rpc.c
 *    Bluetooth Manager RPC operations.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <glib.h>
#include <gio/gio.h>

#include <rdb_ops.h>
#include <rdb_rpc_server.h>

#include "btmgr_priv.h"
#include "bluez_support.h"

static struct rdb_session *rdb_s;
static rdb_rpc_server_session_t *rpc_s = NULL;
static GSource *rpc_source = NULL;
static int discoverable_timeout;
static struct timespec discoverable_end_time;
static struct timespec scan_end_time;
static unsigned int scan_timer_source_id = 0;

#define BTMGR_RPC_SERVICE_NAME "btmgr.rpc"

/* Maximum number of parameters for any of the btmgr commands */
#define NUM_MAX_PARMS 5

/* Describes a btmgr command */
typedef struct btmgr_command_ {
    char *name;
    char *params[NUM_MAX_PARMS];
    int num_params;
    rdb_rpc_cmd_handler_t handler;
} btmgr_command_t;

/* Set the RPC command result with a given string. Used within command handlers */
#define CMD_SET_RESULT(result_str)                              \
do {                                                            \
    if (result && result_len && (*result_len > 0)) {            \
        int tmp_len = *result_len;                              \
        snprintf(result, tmp_len, result_str);                   \
        *result_len = strlen(result) + 1;                        \
    }                                                            \
 } while (0);

/* Describes a mapping between an RDB variable name and a bluez property name */
typedef struct rdb_bluez_mapping_ {
    char *rdb_var;
    char *bluez_property;
} rdb_bluez_mapping_t;

/*
 * Mappings betteen RDB bluetooth conf variables and the corresponding bluez
 * adapter property names.
 */
static rdb_bluez_mapping_t adapter_rdb_bluez_map[] = {
    { RDB_BT_CONF_ENABLE_VAR, BLUEZ_ADAPTER_PROP_POWERED },
    { RDB_BT_CONF_NAME_VAR, BLUEZ_ADAPTER_PROP_ALIAS },
    { RDB_BT_CONF_PAIRABLE_VAR, BLUEZ_ADAPTER_PROP_PAIRABLE },
    { RDB_BT_CONF_DISC_TIMEOUT_VAR, BLUEZ_ADAPTER_PROP_DISC_TIMEOUT },
};

/*
 * Check a given condition. If the condition indicates a failure then
 * the RPC commnd result is set to the error_str and an rval is returned.
 * Used within command handlers.
 */
#define CMD_CHECK(condition, error_str, rval)                   \
do {                                                            \
    if (!(condition)) {                                         \
        CMD_SET_RESULT(error_str);                              \
        return rval;                                            \
    }                                                           \
 } while (0);

/*
 * GSource dispatch function. Called by Glib to to process an event source
 * (RDB notifications in this case).
 */
static gboolean
btmgr_rpc_server_dispatch (GSource *source, GSourceFunc callback,
                          gpointer user_data)
{
    char name_buf[BTMGR_RDB_GETNAMES_BUF_SIZE];
    char *name_buf_ptr = name_buf;
    int buf_len = sizeof(name_buf);
    int rval;

    /* Get all triggered rdb variables for the btmgr rdb session */
    do {
        if (name_buf_ptr != name_buf) {
            free(name_buf_ptr);
            name_buf_ptr = malloc(buf_len);
        }

        rval = rdb_getnames(rdb_s, "", name_buf_ptr, &buf_len, TRIGGERED);
    } while (rval == -EOVERFLOW);

    /* If there are any trigggered vars crank them through RPC processing */
    if (!rval && (buf_len > 0)) {
        dbgp("Process RPC command\n");
        rdb_rpc_server_process_commands(rpc_s, name_buf_ptr);
    }

    if (name_buf_ptr != name_buf) {
        free(name_buf_ptr);
    }

    /* G_SOURCE_CONTINUE means keep this source active */
    return G_SOURCE_CONTINUE;
}

static GSourceFuncs rpc_source_funcs = {
    NULL,
    NULL,
    btmgr_rpc_server_dispatch,
    NULL
};

/*
 * RPC command handler for the "apply_config" command. One of the UIs will have
 * set the bluetooth.conf RDB variables to reflect the current config before
 * invoking this RPC.
 */
static int
apply_config (void)
{
    char value[MAX_NAME_LENGTH];
    int len;
    int num_properties;
    rdb_bluez_mapping_t *mapping;
    int ix;
    int bt_enable = 1;

    dbgp("\n");

    num_properties = sizeof(adapter_rdb_bluez_map) / sizeof(rdb_bluez_mapping_t);

    /*
     * Read each bluetooth.conf variable and set the corresponding bluez
     * adapter property with that variable value.
     */
    for (ix = 0; ix < num_properties; ix++) {
        mapping = &(adapter_rdb_bluez_map[ix]);
        len = sizeof(value);
        INVOKE_CHK(rdb_get(rdb_s, mapping->rdb_var, value, &len),
                   "Failed to get rdb var %s", mapping->rdb_var);
        INVOKE_CHK(bz_set_adapter_property(mapping->bluez_property, value),
                   "Failed to set bluez property %s", mapping->bluez_property);

        dbgp("Set property %s to value %s\n", mapping->bluez_property, value);
        if (!strcmp(mapping->rdb_var, RDB_BT_CONF_ENABLE_VAR)) {
            bt_enable = atoi(value);
        } else if (!strcmp(mapping->rdb_var, RDB_BT_CONF_DISC_TIMEOUT_VAR)) {
            discoverable_timeout = atoi(value);
        }
    }

    if (!bt_enable) {
        dbgp("Stopping\n");
        bz_stop();
    }

    return 0;
}

/*
 * RPC command handler for the "apply_config" command. One of the UIs will have
 * set the bluetooth.conf RDB variables to reflect the current config before
 * invoking this RPC.
 *
 * RPC params: none
 *
 * RPC result string:
 *    - error message on failure
 *    - "success" on sucess
 */
static int
apply_config_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                          int params_len, char *result,
                          int *result_len)
{
    int rval;

    dbgp("\n");

    rval = apply_config();

    if (rval) {
        CMD_SET_RESULT("failed");
    } else {
        CMD_SET_RESULT("success");
    }

    return rval;
}

/*
 * Copy string from src to dest with a maximum of n bytes copied including the
 * trailing null character.
 * If a special character that matches one in special_chars is encountered,
 * it is first percent encoded before being copied.
 * Params:
 *   dest: destination buffer
 *   src: source string buffer
 *   n: the buffer length of dest (including trailing null)
 *   special_chars: a null-terminated string of special characters
 * Return:
 *   1) -EOVERFLOW if dest buffer is too short; otherwise
 *   2) the string length of dest excluding the trailing null
 */
#define NUM_ASCII_CHARS 128
#define PERCENT_ENCODED_CHAR_LEN 3
int
percent_encoded_strncpy (char *dest, const char *src, size_t n,
                         const char * special_chars)
{
    char char_map[NUM_ASCII_CHARS];
    int ix;
    char *dest_ptr = dest;

    /*
     * Build a char map for fast lookup. One entry for each possible ascii
     * char. A 0 value indicates a normal character that can be used as is.
     * A non-zero indicates a special char requiring percent encoding.
     */
    memset(char_map, 0, sizeof(char_map));
    for (ix = 0; ix < strlen(special_chars); ix++) {
        char_map[(int)special_chars[ix]] = 1;
    }

    /*
     * Copy each source character to the destination and encode
     * any special chars encountered.
     */
    for (ix = 0; ix < strlen(src); ix++) {
        if (dest_ptr >= (dest + n)) {
            return (-EOVERFLOW);
        }

        if (((unsigned char)src[ix] >= NUM_ASCII_CHARS) ||
            !char_map[(int)src[ix]]) {
            *dest_ptr = src[ix];
            dest_ptr++;
        } else {
            if (dest_ptr + PERCENT_ENCODED_CHAR_LEN >= dest + n) {
                return (-EOVERFLOW);
            }
            sprintf(dest_ptr, "%%%02x", src[ix]);
            dest_ptr += PERCENT_ENCODED_CHAR_LEN;
        }
    }

    if (dest_ptr >= (dest + n)) {
        return (-EOVERFLOW);
    } else {
        *dest_ptr = '\0';
        return (dest_ptr - dest);
    }
}

/*
 * RPC command handler for the "get_devices" command. Invoked by a client
 * to get the current list of devices and their property values.
 * The return result is a string that represents a list of devices. Each
 * device is composed of key-value pairs where the key is the property name and
 * the value is the property value. Each device is delimited by an '&'. Each
 * property is delimited by a ';'.
 *
 * RPC params: none
 *
 * RPC result string:
 *    - error message on failure
 *    - device list string on success. Example device list string:
 *
 * Address=00:1A:7D:DA:71:0A;Name=aau-linux-pc-0;Paired=true&Address=CF:97:35:34:B1:BE;Name=CF:97:35:34:B1:BE;Paired=false&Address=00:1A:6B:BD:2B:C4;Name=test-0;Paired=false&Address=00:1B:B1:15:A4:C2;Name=J-PC;Paired=false&Address=00:1A:8A:B4:58:7E;Name=00:1A:8A:B4:58:7E;Paired=false
 */
static int
get_devices_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                         int params_len, char *result,
                         int *result_len)
{
    bz_device_t *device = bz_get_device_list();
    int buf_len;
    char *buf_ptr;
    char *device_delim;
    char *property_delim;
    int ix;
    int rval;
    char *property_name;
    char *property_value;

    dbgp("\n");

    CMD_CHECK((result && result_len && (*result_len > 0)),
              "Invalid result params", -EINVAL);

    /* Empty string for the no device case */
    result[0] = '\0';

    buf_len = *result_len;
    device_delim = "";
    property_delim = "";
    buf_ptr = result;
    while (device) {
        for (ix = 0; ix < PROP_DEV_MAX; ix++) {
            property_name = BZ_DEVICE_PROPERTIES[ix];
            property_value = device->properties[ix];

            rval = snprintf(buf_ptr, buf_len, "%s%s%s=", device_delim,
                            property_delim, property_name);
            if ((rval < 0) || (rval >= buf_len)) {
                CMD_CHECK(0, "Overflow", -EOVERFLOW);
            }
            buf_ptr += rval;
            buf_len -= rval;
            /* property value can contain & ; %, so percent encode it */
            rval = percent_encoded_strncpy(buf_ptr, property_value, buf_len,
                                           "&;%");
            if ((rval < 0) || (rval >= buf_len)) {
                CMD_CHECK(0, "Overflow", -EOVERFLOW);
            }
            buf_ptr += rval;
            buf_len -= rval;

            device_delim = "";
            property_delim = ";";
        }

        device_delim = "&";
        property_delim = "";
        device = device->next;
    }

    /* Add one to include the terminating NULL byte */
    *result_len = *result_len - buf_len + 1;

    dbgp("result: %s, result_len: %d\n", result, *result_len);
    return 0;
}

/*
 * RPC command handler for the "discoverable" command. This makes the local
 * device either discoverable or invisible.
 *
 * RPC params:
 *    - param name: "enable"
 *      param value: "1" to turn on discoverability,
 *                   "0" to turn off discoverability
 *
 * RPC result string:
 *    - error message on failure
 *    - "success" on success.
 */
static int
discoverable_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                          int params_len, char *result,
                          int *result_len)
{
    int rval;
    int enable;
    struct timespec now;

    dbgp("enable: %s\n", params[0].value);

    enable = !!(atoi(params[0].value));

    rval = bz_set_adapter_property(BLUEZ_ADAPTER_PROP_DISCOVERABLE,
                                   params[0].value);
    CMD_CHECK(!rval, "Unable set adapter Discoverable", rval);

    if (enable) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        discoverable_end_time.tv_sec = now.tv_sec + discoverable_timeout;
        discoverable_end_time.tv_nsec = now.tv_nsec;
    } else {
        discoverable_end_time.tv_sec = 0;
        discoverable_end_time.tv_nsec = 0;
    }

    dbgp("end_time: %d, %ld:%ld\n", discoverable_timeout,
         discoverable_end_time.tv_sec,
         discoverable_end_time.tv_nsec);

    CMD_SET_RESULT("success");
    return 0;
}

/*
 * RPC command handler for the "discoverable_status" command. This is used
 * to find out whether discoverable is currently on or off and if on then for
 * how much longer.
 *
 * RPC params: none
 *
 * RPC result string:
 *    - error message on failure
 *    - On success, the number of seconds remaining before discoverable is
 *      turned off. A value of 0 means that discoverable is currently off.
 *      A value of -1 means that discoverable is currently on and there is no
 *      timeout (ie, always on).
 */
static int
discoverable_status_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                                 int params_len, char *result,
                                 int *result_len)
{
    struct timespec now;
    long secs_remaining = 0; /* Default - discoverable off */

    if ((discoverable_end_time.tv_sec != 0) ||
        (discoverable_end_time.tv_nsec != 0)) {
        /* Discoverable on */
        if (discoverable_timeout > 0) {
            /* Time out configured */
            clock_gettime(CLOCK_MONOTONIC_RAW, &now);

            secs_remaining = (discoverable_end_time.tv_sec - now.tv_sec) +
                (discoverable_end_time.tv_nsec - now.tv_nsec) / 1000000000;

            if (secs_remaining < 0) {
                secs_remaining = 0;
            }
        } else {
            /* No timeout */
            secs_remaining = -1;
        }
    }

    dbgp("secs_remaining: %ld\n", secs_remaining);
    snprintf(result, *result_len, "%ld", secs_remaining);
    return 0;
}

/*
 * Stops a previously started scan.
 *
 * Parameters:
 *    remove_source: 1 if the scan timer source should be removed/stopped.
 *                   0 otherwise.
 */
static void
scan_stop (int remove_source)
{
     bz_scan_stop();
     scan_end_time.tv_sec = scan_end_time.tv_nsec = 0;
     if (remove_source) {
         g_source_remove(scan_timer_source_id);
     }
     scan_timer_source_id = 0;

     dbgp("Scan stopped\n");
}

/*
 * Tick handler for the scan timer.
 */
static gboolean
scan_timer_handler (gpointer user_data)
{
    struct timespec now;
    long seconds_left = 0;
    int rval;

    dbgp("\n");

    /*
     * Calculate the time remaining for the previously started scan operation.
     * seconds_left is rounded up to the nearest second.
     */
    rval = clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    if (!rval) {
        seconds_left = (scan_end_time.tv_sec - now.tv_sec) +
            ((scan_end_time.tv_nsec - now.tv_nsec + 999999999) / 1000000000);
    }

    /*
     * In most cases, when this tick handler fires the scan end time has
     * passed and the scan is ready to be stopped (if case). The less
     * frequent case is when a subsequent scan was started when a scan was
     * already in operation (else case). For that case, the scan timer was
     * already running so only the scan end time was updated/extended.
     */
    if (seconds_left <= 0) {
        /*
         * Call scan_stop with remove_source param set to false.
         * No need to remove the source as it will be removed by the timer
         * mechanism when this handler function returns FALSE.
         */
        scan_stop(0);
    } else {
        /*
         * Need to extend the scan time. Create a new timer that will fire
         * after the extension time.
         */
        scan_timer_source_id = g_timeout_add_seconds((int)seconds_left,
                                                     scan_timer_handler, NULL);
        dbgp("Extend scan timeout by %d seconds\n", (int)seconds_left);
    }

    /* Stops timer */
    return FALSE;
}

/*
 * RPC command handler for the "scan" command. This is used
 * to find out whether discoverable is currently on or off and if on then for
 * how much longer.
 *
 * RPC params:
 *    - param name: "time"
 *      param value: Number of seconds to run the scan for. A 0 value
 *                   will stop the running scan (if any) early.
 *
 * RPC result string:
 *    - error message on failure
 *    - "success" on success
 */
static int
scan_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                  int params_len, char *result,
                  int *result_len)
{
    long val;
    char *endptr;
    int rval;
    struct timespec now;

    dbgp("\n");

    /* Get the scan time */
    errno = 0;
    val = strtol(params[0].value, &endptr, 0);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
        (errno != 0 && val == 0) || (*endptr != '\0') ||
        (val != (int)val)) {
        CMD_CHECK(0, "Time param is not valid", -EINVAL);
    }

    dbgp("timeout: %d seconds\n", (int)val);

    if (val == 0) {
        scan_stop(1);
    } else {
        rval = clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        CMD_CHECK(!rval, "Unable to get current time", -errno);

        if (scan_end_time.tv_sec == 0) {
            /*
             * Scan not already running. Start the actual scan and create
             * a timer to fire when the scan time expires.
             */
            rval = bz_scan_start();
            CMD_CHECK(!rval, "Failed to start device scan", rval);
            scan_timer_source_id = g_timeout_add_seconds((int)val,
                                                         scan_timer_handler,
                                                         NULL);
            dbgp("Scan started\n");
        } else {
            /*
             * Scan already running. Nothing extra to do. The scan time will
             * be extended below. When the (already running) timer fires, the
             * handler will see the time extension and keep the scan running
             * for the extended time.
             */
            dbgp("Scan extended\n");
        }

        scan_end_time.tv_sec = now.tv_sec + val;
        scan_end_time.tv_nsec = now.tv_nsec;
    }

    CMD_SET_RESULT("success");
    return 0;
}

/*
 * RPC command handler for the "pair" command. The pair command can require
 * user interaction so can take some time to complete. Hence this RPC
 * intiates a pair request asynchronously.
 *
 * RPC params:
 *    - param name: "address"
 *      param value: The BT address of the device to be paired.
 *
 * RPC result string:
 *    - error message on failure. For specific errors that the callers are
 *      expecting the error message needs to be an org.bluez.Error code.
 *    - "success" on success
 */
static int
pair_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                  int params_len, char *result,
                  int *result_len)
{
    int rval;

    dbgp("address: %s\n", params[0].value);

    CMD_CHECK(!bz_pairing_in_progress(), "org.bluez.Error.AlreadyInProgress",
              -EINPROGRESS);

    rval = bz_pair(params[0].value);
    CMD_CHECK(!rval, "Unable to pair device", rval);

    CMD_SET_RESULT("success");
    return 0;
}

/*
 * RPC command handler for the "pair_status" command. This is used by clients
 * to retrieve the status of the currently active pairing operation.
 *
 * RPC params:
 *    - param name: "address"
 *      param value: The BT address of the device that is being paired and
 *                   for which status is required. Note that this address
 *                   is not actually needed to retrieve the status because there
 *                   can only be one active pairing. It is provided as a
 *                   check to verify that the client is asking for the current
 *                   pairing operation.
 *
 * RPC result string:
 *    - error message on failure
 *    - On success one of the following result strings will be returned:
 *        - "not_pairing": No pairing operation has been initiated.
 *        - "in_progress": A pairing operation is currently in progress and
 *                         is not waiting for any local user response.
 *        - "confirm_passkey;<passkey>": A pairing operation is in progress and
 *                                       is awaiting user confirm/reject of
 *                                       <passkey>.
 *        - "success": The last pairing operation succeed.
 *        - "fail;<error_msg>": The last pairing operation failed with reason
 *                              <err_msg>.
 *
 */
static int
pair_status_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                         int params_len, char *result,
                         int *result_len)
{
    int rval = 0;
    dbgp("address: %s\n", params[0].value);

    if (!result || !result_len || (*result_len <= 0)) {
        return (-EINVAL);
    }

    rval = bz_get_pairing_status_string(result, *result_len);
    if (rval) {
        CMD_CHECK(0, "Failed to get pairing status", rval);
    }
    *result_len = strlen(result) + 1;
    return 0;
}

/*
 * RPC command handler for the "unpair" command.
 *
 * RPC params:
 *    - param name: "address"
 *      param value: The BT address of the device that is being unpaired.
 *
 * RPC result string:
 *    - Error message on failure. For specific errors that the callers are
 *      expecting the error message needs to be an org.bluez.Error code.
 *    - "success" on success
 */
static int
unpair_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                    int params_len, char *result,
                    int *result_len)
{
    int rval;

    dbgp("address: %s\n", params[0].value);

    rval = bz_unpair(params[0].value);

    switch (rval) {
    case (0):
        CMD_SET_RESULT("success");
        break;
    case (-ENOENT):
        CMD_CHECK(0, "org.bluez.Error.InvalidDevice", rval);
        break;
    case (-EALREADY):
        CMD_CHECK(0, "org.bluez.Error.NotPaired", rval);
        break;
    default:
        CMD_CHECK(0, "org.bluez.Error.Failed", rval);
        break;
    }

    return 0;
}

/*
 * RPC command handler for the "passkey_confirm" command. This is invoked
 * by clients to confirm/reject a passkey authentication that has resulted
 * from a pairing operation.
 *
 * RPC params:
 *    - param name: "address"
 *      param value: The BT address of the device being paired.
 *
 *    - param name: "passkey"
 *      param value: The passkey to be confirmed/rejected.
 *
 *    - param name: "confirm"
 *      param value: The user's confirmation response. "true" to confirm,
 *                   "false" to reject.
 *
 * RPC result string:
 *    - error message on failure
 *    - "success" on success
 */
static int
passkey_confirm_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                             int params_len, char *result,
                             int *result_len)
{
    char *address = NULL;
    char *passkey = NULL;
    char *confirm = NULL;
    long passkey_num;
    char *endptr;
    int ix;
    int rval;

    dbgp("\n");

    /* Extract the RPC parameters */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "address")) {
            address = params[ix].value;
        } else if (!strcmp(params[ix].name, "passkey")) {
            passkey = params[ix].value;
        } else if (!strcmp(params[ix].name, "confirm")) {
            confirm = params[ix].value;
        } else {
            CMD_CHECK(0, "Internal error: unexpected param", -EINVAL);
        }
    }

    CMD_CHECK((address && passkey && confirm),
              "Internal error: missing params", -EINVAL);

    dbgp("address: %s, passkey: %s, confirm: %s\n", address, passkey, confirm);

    /* Convert the passkey from a string to an int */
    errno = 0;
    passkey_num = strtol(passkey, &endptr, 0);
    if ((errno == ERANGE && (passkey_num == LONG_MAX ||
                             passkey_num == LONG_MIN)) ||
        (errno != 0 && passkey_num == 0) || (*endptr != '\0') ||
        (passkey_num != (unsigned int)passkey_num)) {
        CMD_CHECK(0, "passkey param is not valid", -EINVAL);
    }

    /* Do the confirm */
    rval = bz_passkey_confirm(address, passkey_num, !strcmp(confirm, "true"));
    CMD_CHECK(!rval, "Invalid passkey confirm", rval);

    CMD_SET_RESULT("success");
    return 0;
}

/*
 * Initialises the btmr RPC server. The RPC server runs in the same
 * single threaded process as the dbus handling. The dbus handling is done
 * via a GLib main loop. So the RPC server hooks itself into the main loop
 * by adding the RDB file descriptor as a main loop event source.
 */
struct rdb_session *rdb_s2 = NULL;
int
btmgr_rpc_server_init (void)
{
    int ix;

    btmgr_command_t commands[] = {
        { "apply_config", { }, 0, apply_config_cmd_handler },
        { "get_devices", { }, 0, get_devices_cmd_handler },
        { "discoverable", { "enable" }, 1, discoverable_cmd_handler },
        { "discoverable_status", { }, 0, discoverable_status_cmd_handler },
        { "scan", { "time" }, 1, scan_cmd_handler },
        { "pair", { "address" }, 1, pair_cmd_handler },
        { "pair_status", { "address" }, 1, pair_status_cmd_handler },
        { "passkey_confirm", { "address", "passkey", "confirm" }, 3,
          passkey_confirm_cmd_handler },
        { "unpair", { "address" }, 1, unpair_cmd_handler },
    };

    INVOKE_CHK(rdb_open(NULL, &rdb_s), "Failed to open rdb session\n");

    rpc_source = g_source_new(&rpc_source_funcs, sizeof(GSource));
    if (!rpc_source) {
        errp("Failed to create rpc source\n");
        return (-1);
    }

    g_source_add_unix_fd(rpc_source, rdb_fd(rdb_s), G_IO_IN);
    g_source_attach(rpc_source, NULL);

    /* Init the RPC server */
    INVOKE_CHK(rdb_rpc_server_init(BTMGR_RPC_SERVICE_NAME, &rpc_s),
               "Failed to init btmgr rpc server\n");

    /* Add each btmgr command */
    for (ix = 0; ix < (sizeof(commands) / sizeof(btmgr_command_t)); ix++) {
        btmgr_command_t *cmd = &(commands[ix]);
        INVOKE_CHK(rdb_rpc_server_add_command(rpc_s, cmd->name, cmd->params,
                                              cmd->num_params, cmd->handler),
                   "Failed to add command '%s'\n", cmd->name);
    }

    INVOKE_CHK(apply_config(), "Failed to apply initial config\n");

    /* Start the server running in manual mode */
    INVOKE_CHK(rdb_rpc_server_run(rpc_s, rdb_s), "Failed to run server\n");

    return 0;
}

/*
 * Destroy the RPC server.
 */
void
btmgr_rpc_server_destroy(void)
{
    if (rpc_s) {
        rdb_rpc_server_stop(rpc_s);
        rdb_rpc_server_destroy(&rpc_s);
    }

    if (rdb_s) {
        rdb_close(&rdb_s);
    }
}
