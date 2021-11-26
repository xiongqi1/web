/*
 * scrm_bluetooth_rpc.c
 *    Bluetooth Screen UI support. Bluetooth RPC operations. Mostly
 *    wrappers to invoke BT RPC services from the btmgr daemon.
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#include <rdb_rpc_client.h>
#include <scrm.h>
#include <scrm_ops.h>

#include "scrm_bluetooth.h"

#define BTMGR_RPC_SERVICE_NAME "btmgr.rpc"
#define SCRM_BT_RPC_RESULT_BUF_LEN 2048
#define SCRM_BT_RPC_TIMEOUT_SECS 1
#define SCRM_BT_RPC_DEV_DELIMITER  "&"
#define SCRM_BT_RPC_PROP_DELIMITER ";"
#define SCRM_BT_RPC_PROP_VAL_DELIMITER '='

/*
 * Generic function to invoke a btmgr RPC service.
 */
static int
scrm_bt_rpc_invoke (char *cmd,
                    rdb_rpc_cmd_param_t *params,
                    int params_len,
                    char *result_buf,
                    int *result_len)
{
    rdb_rpc_client_session_t *rpc_s = NULL;
    int rval = 0;

    INVOKE_CHK(rdb_rpc_client_connect(BTMGR_RPC_SERVICE_NAME, &rpc_s),
               "Unable to connect to RPC service %s", BTMGR_RPC_SERVICE_NAME);

    INVOKE_CHK(rdb_rpc_client_invoke(rpc_s, cmd, params,
                                     params_len,
                                     SCRM_BT_RPC_TIMEOUT_SECS,
                                     result_buf, result_len),
               "Unable to invoke %s RPC", cmd);

 done:
    if (rpc_s) {
        rdb_rpc_client_disconnect(&rpc_s);
    }

    return rval;
}

/*
 * Fetch the device list from btmgr and return it in the dev_list parameter.
 * The caller is responsible for freeing dev_list by calling
 * scrm_bt_free_dev_list.
 */
int
scrm_bt_get_devices (scrm_bt_device_t **dev_list, bool paired)
{
    int rval = 0;
    char result_buf[SCRM_BT_RPC_RESULT_BUF_LEN];
    int result_len = sizeof(result_buf);
    char *device_saveptr;
    char *device_str = NULL;
    char *property_str;
    char *property_saveptr;
    char *property_val_str;
    scrm_bt_device_t *dev;
    scrm_bt_device_t dev_tmp;

    if (!dev_list) {
        return -EINVAL;
    }

    *dev_list = NULL;

    INVOKE_CHK(scrm_bt_rpc_invoke("get_devices", NULL, 0, result_buf,
                                  &result_len),
               "Unable to invoke rpc");

    dbgp("get_devices: %d, %s\n", result_len, result_buf);

    /*
     * Parse the returned device list string. The list string is a
     * list of delimited devices and each device string is a list of
     * delimited properties.
     *
     * Example string:
     *
     * Address=80:00:6E:F1:44:BF;Name=marketing_mac3;Paired=false&Address=5C:C5:D4:49:1C:FD;Name=NTCLT0109;Paired=false&Address=00:1A:7D:DA:71:0A;Name=aau-linux-pc-0;Paired=true
     */
    if (result_len > 0) {
        device_str = strtok_r(result_buf, SCRM_BT_RPC_DEV_DELIMITER,
                              &device_saveptr);
    }
    while (device_str) {
        /*
         * device_str points to a single device string. Parse it to
         * get the device's property values.
         */

        /* Parse each device property */
        property_str = strtok_r(device_str, SCRM_BT_RPC_PROP_DELIMITER,
                                &property_saveptr);

        memset(&dev_tmp, 0, sizeof(dev_tmp));
        while (property_str) {
            property_val_str = strchr(property_str,
                                      SCRM_BT_RPC_PROP_VAL_DELIMITER);
            if (!property_val_str) {
                rval = -EINVAL;
                goto done;
            }

            /*
             * Remove the value delimiter ('=') and move past it.
             * property_str will then point to the property name string and
             * property_val_str will point to the property value string.
             */
            *property_val_str = '\0';
            property_val_str++;

            if (!strcmp(property_str, "Address")) {
                dev_tmp.address = property_val_str;
            } else if (!strcmp(property_str, "Name")) {
                dev_tmp.name = scrm_bt_percent_decode(property_val_str);
            } else if (!strcmp(property_str, "Paired")){
                dev_tmp.paired = !strcmp(property_val_str, "true");
            } else {
                errp("Unexpected BT property %s=%s", property_str,
                     property_val_str);
            }

            property_str = strtok_r(NULL, SCRM_BT_RPC_PROP_DELIMITER,
                                    &property_saveptr);
        }

        /* Add the device to the list if it is in the requested pair state. */
        if ((dev_tmp.paired && paired) || (!dev_tmp.paired && !paired)) {
            dev = calloc(1, sizeof(*dev));
            if (!dev) {
                rval = -errno;
                goto done;
            }

            memcpy(dev, &dev_tmp, sizeof(*dev));
            dev->address = strdup(dev_tmp.address);
            dev->name = strdup(dev_tmp.name);
            if (!dev->address || !dev->name) {
                rval = -errno;
                goto done;
            }

            /*
             * Queue it now. If anything fails the cleanup code will free
             * the whole list.
             */
            dev->next = *dev_list;
            *dev_list = dev;

            dbgp("Device: %s, %s, %d\n", (*dev_list)->name,
                 (*dev_list)->address,
                 (*dev_list)->paired);
        }

        device_str = strtok_r(NULL, SCRM_BT_RPC_DEV_DELIMITER,
                              &device_saveptr);
    }

 done:
    if (rval) {
        scrm_bt_free_dev_list(dev_list);
    }

    return rval;
}

/*
 * Frees a device list.
 */
void
scrm_bt_free_dev_list (scrm_bt_device_t **dev_list)
{
    scrm_bt_device_t *dev;
    scrm_bt_device_t *next;

    if (!dev_list || !(*dev_list)) {
        return;
    }

    dev = *dev_list;
    while (dev) {
        next = dev->next;
        free(dev->name);
        free(dev->address);
        free(dev);
        dev = next;
    }

    *dev_list = NULL;
}

/*
 * Perform a BT scan operation for the given number of seconds.
 */
int
scrm_bt_scan (char *seconds)
{
    char result_buf[SCRM_BT_RPC_RESULT_BUF_LEN];
    int result_len = sizeof(result_buf);
    rdb_rpc_cmd_param_t params[] = {
        { "time", seconds, strlen(seconds) + 1 }
    };

    return (scrm_bt_rpc_invoke("scan", params,
                               sizeof(params) / sizeof(rdb_rpc_cmd_param_t),
                               result_buf, &result_len));
}

/*
 * Intiate a BT pairing.
 */
int
scrm_bt_pair (char *address)
{
    char result_buf[SCRM_BT_RPC_RESULT_BUF_LEN];
    int result_len = sizeof(result_buf);
    rdb_rpc_cmd_param_t params[] = {
        { "address", address, strlen(address) + 1 }
    };

    return (scrm_bt_rpc_invoke("pair", params,
                               sizeof(params) / sizeof(rdb_rpc_cmd_param_t),
                               result_buf, &result_len));
}

/*
 * Unpair a BT device.
 */
int
scrm_bt_unpair (char *address)
{
    char result_buf[SCRM_BT_RPC_RESULT_BUF_LEN];
    int result_len = sizeof(result_buf);
    rdb_rpc_cmd_param_t params[] = {
        { "address", address, strlen(address) + 1 }
    };

    return (scrm_bt_rpc_invoke("unpair", params,
                               sizeof(params) / sizeof(rdb_rpc_cmd_param_t),
                               result_buf, &result_len));
}

/*
 * Respond to a passkey request with a confirm/deny.
 */
int
scrm_bt_passkey_confirm (char *address, char *passkey, bool confirm)
{
    char result_buf[SCRM_BT_RPC_RESULT_BUF_LEN];
    int result_len = sizeof(result_buf);
    char *confirm_str = confirm ? "true" : "false";
    rdb_rpc_cmd_param_t params[] = {
        { "address", address, strlen(address) + 1 },
        { "passkey", passkey, strlen(passkey) + 1 },
        { "confirm", confirm_str, strlen(confirm_str) + 1 }
    };

    return (scrm_bt_rpc_invoke("passkey_confirm", params,
                               sizeof(params) / sizeof(rdb_rpc_cmd_param_t),
                               result_buf, &result_len));
}

/*
 * Enable or disable Discoverable.
 */
int
scrm_bt_discoverable (bool enable)
{
    char result_buf[SCRM_BT_RPC_RESULT_BUF_LEN];
    int result_len = sizeof(result_buf);
    char *enable_str = enable ? "1" : "0";
    rdb_rpc_cmd_param_t params[] = {
        { "enable", enable_str, strlen(enable_str) + 1 }
    };

    return (scrm_bt_rpc_invoke("discoverable", params,
                               sizeof(params) / sizeof(rdb_rpc_cmd_param_t),
                               result_buf, &result_len));
}

/*
 * Get the current Discoverable status.
 *  Return status :
 *      >0 gives the current discoverable time remaining in seconds.
 *      <0 means currently discoverable but with no timeout.
 *      =0 means currently not discoverable.
 */
int
scrm_bt_discoverable_status (int *status)
{
    char result_buf[SCRM_BT_RPC_RESULT_BUF_LEN];
    int result_len = sizeof(result_buf);
    int rval;
    long int status_val;
    char *endptr;

    if (!status) {
        return -EINVAL;
    }

    rval = scrm_bt_rpc_invoke("discoverable_status", NULL, 0, result_buf,
                              &result_len);
    if (rval) {
        return rval;
    }

    errno = 0;
    status_val = strtol(result_buf, &endptr, 10);
    if ((errno == ERANGE && (status_val == LONG_MAX || status_val == LONG_MIN))
        || (errno != 0 && status_val == 0)) {
        return -errno;
    } else if ((endptr == result_buf) || (status_val > INT_MAX) ||
               (status_val < -INT_MAX)) {
        return -EINVAL;
    } else {
        *status = (int)status_val;
        return 0;
    }

}

/*
 * Waits for the btmgr rpc service to come online. Needed to ensure that any
 * RPCs invocations to it will succeed and also to know that the btmgr
 * rdb variables have been created and can be subscribed to.
 */
#define SCRM_BT_NUM_RETRIES 10
int
scrm_bt_rpc_server_wait (void)
{
    rdb_rpc_client_session_t *rpc_s = NULL;
    int count = 0;
    int rval;

    do {
        rval = rdb_rpc_client_connect(BTMGR_RPC_SERVICE_NAME, &rpc_s);
        count++;
        if (rval) {
            sleep(1);
        }
    } while (rval && (count < SCRM_BT_NUM_RETRIES));

    dbgp("Wait for btmgr %s after %d tries\n",
         (rval == 0) ? "succeeded" : "failed", count);

    if (rpc_s) {
        rdb_rpc_client_disconnect(&rpc_s);
    }

    return rval;
}
