/*
 * Sample Nonin oximeter application (User defined executable).
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "nonin.h"
#include "dsm_bt_utils.h"
#include "dsm_gatt_rpc.h"

#include <rdb_rpc_client.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

#define MAX_BUF_LEN 1024
#define MAX_NOTIFICATIONS 5

static int dev_info_srv_idx = -1;
static int oximetry_srv_idx = -1;
static int meas_char_idx = -1;
static int cp_char_idx = -1;

static struct rdb_session *rdb_s;

#ifdef DEBUG
#define DBG_BUF_LEN 512
char dbg_buf[DBG_BUF_LEN];
#endif

static void
usage (const char *prog)
{
    dbgp("Usage: %s rdb_root service_name\n", prog);
}

/*
 * Read all rdb variables under the rdb tree specified by rdb_root.
 * This will also discover the indices for the Device Information service,
 * the Nonin Oximetry service, the Oximetry Measurement characteristic, and
 * the Control Point characteristics.
 * Since some rdb variables are just cached versions of the corresponding
 * characteristics and descriptors, they might be empty until a successful
 * read or notification.
 */
static int
read_rdb_vars (const char *rdb_root)
{
    char rdb_var_name[MAX_NAME_LENGTH];
    char buf[MAX_BUF_LEN];
    int len;
    int rval;
    int ix;
    const char *char_prefix;

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.address", rdb_root);
    rval = rdb_get_string(rdb_s, rdb_var_name, buf, sizeof buf);
    if (rval) {
        errp("Failed to read device address rdb var\n");
        goto done;
    }
    syslog(LOG_INFO, "%s.address=%s\n", rdb_root, buf);

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.name", rdb_root);
    rval = rdb_get_string(rdb_s, rdb_var_name, buf, sizeof buf);
    if (rval) {
        errp("Failed to read device name rdb var\n");
        goto done;
    }
    syslog(LOG_INFO, "%s.name=%s\n", rdb_root, buf);

    /*
     * Nonin oximeter has two services:
     * 1) device info service, UUID=0x180a, handle=6;
     * 2) nonin oximetry service, UUID=46a970e0-0d5f-11e2-8b5e-0002a5d5c51b,
     *    handle=22.
     * To identify the services, we can use either UUIDs or handles.
     */
    for (ix = 0; ix < MAX_SERVICE_INDEX; ix++) {
        snprintf(rdb_var_name, sizeof rdb_var_name, "%s.gatt.service.%d.uuid",
                 rdb_root, ix);
        rval = rdb_get_string(rdb_s, rdb_var_name, buf, sizeof buf);
        if (rval == -ENOENT) {
            dbgp("Service index %d does not exist\n", ix);
            continue;
        }
        if (rval) {
            errp("Failed to read service index %d\n", ix);
            goto done;
        }
        if (!strcmp(buf, NONIN_DEV_INFO_SERVICE_UUID)) {
            syslog(LOG_INFO,
                   "Nonin Device Information Service found @ index %d\n", ix);
            dev_info_srv_idx = ix;
        } else if(!strcmp(buf, NONIN_OXIMETRY_SERVICE_UUID)) {
            syslog(LOG_INFO,
                   "Nonin Oximetry Service found @ index %d\n", ix);
            oximetry_srv_idx = ix;
        }
        if (ix == dev_info_srv_idx || ix == oximetry_srv_idx) {
            snprintf(rdb_var_name, sizeof rdb_var_name,
                     "%s.gatt.service.%d.handle", rdb_root, ix);
            rval = rdb_get_string(rdb_s, rdb_var_name, buf, sizeof buf);
            if (rval) {
                errp("Failed to read service %d handle\n", ix);
                goto done;
            }
            syslog(LOG_INFO, "  Service handle=%s\n", buf);
        }
        if (dev_info_srv_idx >=0 && oximetry_srv_idx >= 0) {
            break; /* already got all we need */
        }
    }

    if (dev_info_srv_idx < 0) {
        errp("Failed to find Nonin Device Information service\n");
        rval = -ENOENT;
        goto done;
    }
    if (oximetry_srv_idx < 0) {
        errp("Failed to find Nonin Oximetry service\n");
        rval = -ENOENT;
        goto done;
    }

    /* Read characteristics rdb vars of device info service */
    for (ix = 0; ix < MAX_CHARACTERISTIC_INDEX; ix++) {
        snprintf(rdb_var_name, sizeof rdb_var_name,
                 "%s.gatt.service.%d.char.%d.uuid",
                 rdb_root, dev_info_srv_idx, ix);
        rval = rdb_get_string(rdb_s, rdb_var_name, buf, sizeof buf);
        if (rval == -ENOENT) {
            dbgp("Characteristic index %d does not exist in service %d\n",
                 ix, dev_info_srv_idx);
            continue;
        }
        if (rval) {
            errp("Failed to read characteristic index %d in service %d\n",
                 ix, dev_info_srv_idx);
            goto done;
        }

        if (!strcmp(buf, MANUFACTURER_NAME_UUID)) {
            char_prefix = "Manufacturer Name";
        } else if(!strcmp(buf, MODEL_NUMBER_UUID)) {
            char_prefix = "Model Number";
        } else if(!strcmp(buf, SERIAL_NUMBER_UUID)) {
            char_prefix = "Serial Number";
        } else if(!strcmp(buf, SOFTWARE_REVISION_UUID)) {
            char_prefix = "Software Revision";
        } else if(!strcmp(buf, FIRMWARE_REVISION_UUID)) {
            char_prefix = "Firmware Revision";
        } else {
            char_prefix = "Unknown Characteristic";
        }

        snprintf(rdb_var_name, sizeof rdb_var_name,
                 "%s.gatt.service.%d.char.%d.value",
                 rdb_root, dev_info_srv_idx, ix);
        len = sizeof(buf) - 1;
        rval = rdb_get(rdb_s, rdb_var_name, buf, &len);
        if (rval == -ENOENT) {
            dbgp("Characteristic value @ %d does not exist in service %d\n",
                 ix, dev_info_srv_idx);
            continue;
        }
        if (rval) {
            errp("Failed to read characteristic value @ %d in service %d\n",
                 ix, dev_info_srv_idx);
            goto done;
        }
        buf[len] = '\0';
        syslog(LOG_INFO, "%s: %s\n", char_prefix, buf);
    }

    /*
     * Locate Nonin Oximetry service characteristics
     * 1) Oximetry Measurement characteristic
     * 2) Control Point characteristic
     */
    for (ix = 0; ix < MAX_CHARACTERISTIC_INDEX; ix++) {
        snprintf(rdb_var_name, sizeof rdb_var_name,
                 "%s.gatt.service.%d.char.%d.uuid",
                 rdb_root, oximetry_srv_idx, ix);
        rval = rdb_get_string(rdb_s, rdb_var_name, buf, sizeof buf);
        if (rval == -ENOENT) {
            dbgp("Characteristic index %d does not exist in service %d\n",
                 ix, oximetry_srv_idx);
            continue;
        }
        if (rval) {
            errp("Failed to read characteristic index %d in service %d\n",
                 ix, oximetry_srv_idx);
            goto done;
        }

        if (!strcmp(buf, NONIN_OXIMETRY_MEAS_UUID)) {
            char_prefix = "Oximetry Measurement";
            meas_char_idx = ix;
        } else if(!strcmp(buf, NONIN_CONTROL_POINT_UUID)) {
            char_prefix = "Control Point";
            cp_char_idx = ix;
        }

        snprintf(rdb_var_name, sizeof rdb_var_name,
                 "%s.gatt.service.%d.char.%d.value",
                 rdb_root, oximetry_srv_idx, ix);
        len = sizeof(buf) - 1;
        rval = rdb_get(rdb_s, rdb_var_name, buf, &len);
        if (rval == -ENOENT) {
            dbgp("Characteristic value @ %d does not exist in service %d\n",
                 ix, oximetry_srv_idx);
            continue;
        }
        if (rval) {
            errp("Failed to read characteristic value @ %d in service %d\n",
                 ix, oximetry_srv_idx);
            goto done;
        }
        buf[len] = '\0';
        syslog(LOG_INFO, "%s: %s\n", char_prefix, buf);

        if (meas_char_idx >= 0 && cp_char_idx >= 0) {
            break;
        }
    }

    rval = 0;

done:
    return rval;
}

#define BUILD_UINT16(x) ((x[0] << 8) | x[1])
/* parse binary blob into Nonin Data Format 19 struct */
static int
parse_serial_data_format (const char *data, int len,
                          nonin_data_format19_t *result)
{
    if (!data) {
        return -EINVAL;
    }
    if (len < 10) {
        return -EINVAL;
    }
    result->len = *(data++);
    if (result->len < 10) {
        return -EINVAL;
    }
    result->status = *(data++);
    result->batt_volt = *(data++);
    result->pi = BUILD_UINT16(data);
    data += 2;
    result->counter = BUILD_UINT16(data);
    data += 2;
    result->spo2 = *(data++);
    result->pulse_rate = BUILD_UINT16(data);

    return 0;
}

/*
 * Read all characteristics from the device information service using the
 * read_handle rpc call.
 */
static int
read_device_info (rdb_rpc_client_session_t *rpc_client_s, const char *dev_idx)
{
    char *cmd;
    rdb_rpc_cmd_param_t params[GATT_RPC_MAX_PARAMS];
    int num_params;
    char result[MAX_BUF_LEN];
    int result_len;
    const char *val_prefix;
    int ix;

    /* attribute handles for device information service */
    const int handles[] = { MANUFACTURER_NAME_HANDLE,
                            MANUFACTURER_NAME_DESC_HANDLE,
                            MODEL_NUMBER_HANDLE,
                            MODEL_NUMBER_DESC_HANDLE,
                            SERIAL_NUMBER_HANDLE,
                            SERIAL_NUMBER_DESC_HANDLE,
                            SOFTWARE_REVISION_HANDLE,
                            SOFTWARE_REVISION_DESC_HANDLE,
                            FIRMWARE_REVISION_HANDLE,
                            FIRMWARE_REVISION_DESC_HANDLE };

    char handle[MAX_BUF_LEN];

    memset(params, 0, sizeof params);
    cmd = READ_HANDLE_CMD;
    params[0].name = "device";
    params[0].value = (char *)dev_idx;
    params[0].value_len = strlen(params[0].value) + 1;
    params[1].name = "handle";
    num_params = READ_HANDLE_NPARAMS;

    syslog(LOG_INFO, "Reading Device Information Characteristics...\n");
    for(ix = 0; ix < sizeof handles / sizeof handles[0]; ix++) {
        snprintf(handle, sizeof handle, "%d", handles[ix]);
        params[1].value = handle;
        params[1].value_len = strlen(params[1].value) + 1;
        result_len = sizeof result;
        INVOKE_CHK(rdb_rpc_client_invoke(rpc_client_s, cmd, params, num_params,
                                         0, result, &result_len),
                   "Failed to invoke command %s: %s\n", cmd, result);
        switch(handles[ix]) {
        case MANUFACTURER_NAME_HANDLE:
            val_prefix = "Manufacturer Name";
            break;
        case MANUFACTURER_NAME_DESC_HANDLE:
            val_prefix = "Manufacturer Name Descriptor";
            break;
        case MODEL_NUMBER_HANDLE:
            val_prefix = "Model Number";
            break;
        case MODEL_NUMBER_DESC_HANDLE:
            val_prefix = "Model Number Descriptor";
            break;
        case SERIAL_NUMBER_HANDLE:
            val_prefix = "Serial Number";
            break;
        case SERIAL_NUMBER_DESC_HANDLE:
            val_prefix = "Serial Number Descriptor";
            break;
        case SOFTWARE_REVISION_HANDLE:
            val_prefix = "Software Revision";
            break;
        case SOFTWARE_REVISION_DESC_HANDLE:
            val_prefix = "Software Revision Descriptor";
            break;
        case FIRMWARE_REVISION_HANDLE:
            val_prefix = "Firmware Revision";
            break;
        case FIRMWARE_REVISION_DESC_HANDLE:
            val_prefix = "Firmware Revision Descriptor";
            break;
        default:
            val_prefix = "Unknown Attribute";
        }
        result[result_len < sizeof(result) ? result_len : sizeof(result)-1]
            = '\0';
        syslog(LOG_INFO, "%s: %s\n", val_prefix, result);
    }
    return 0;
}

/*
 * Read measurement characteristic from the Nonin Oximetry service.
 * Nonin Oximetry Measurement Characteristic does not support direct reading.
 * It only supports notifying. So we have to turn on notify and then wait for
 * notifications.
 */
static int
read_measurement(const char *rdb_root, rdb_rpc_client_session_t *rpc_client_s,
                 const char *dev_idx)
{
    int rval;
    char *cmd;
    rdb_rpc_cmd_param_t params[GATT_RPC_MAX_PARAMS];
    int num_params;
    char result[MAX_BUF_LEN];
    int result_len;
    int ix;
    char handle[MAX_BUF_LEN];
    char rdb_var_name[MAX_NAME_LENGTH];
    int fd;
    fd_set set;
    nonin_data_format19_t sdf;

    syslog(LOG_INFO, "Start notification on measurement...\n");
    memset(params, 0, sizeof params);
    cmd = SET_HANDLE_NOTIFY_CMD;
    params[0].name = "device";
    params[0].value = (char *)dev_idx;
    params[0].value_len = strlen(params[0].value) + 1;
    params[1].name = "handle";
    snprintf(handle, sizeof handle, "%d", NONIN_OXIMETRY_MEAS_HANDLE);
    params[1].value = handle;
    params[1].value_len = strlen(params[1].value) + 1;
    params[2].name = "value";
    params[2].value = "1";
    params[2].value_len = strlen(params[2].value) + 1;
    num_params = SET_HANDLE_NOTIFY_NPARAMS;
    result_len = sizeof result;
    rval = rdb_rpc_client_invoke(rpc_client_s, cmd, params, num_params, 0,
                                 result, &result_len);
    if (rval) {
        errp("Failed to invoke command %s: %s\n", cmd, result);
        goto done;
    }

    snprintf(rdb_var_name, sizeof rdb_var_name,
             "%s.gatt.service.%d.char.%d.value",
             rdb_root, oximetry_srv_idx, meas_char_idx);

#if 0
    /* read measurement value from rdb var every second */
    syslog(LOG_INFO, "Reading Measurements rdb var...\n");
    for (ix = 0; ix < MAX_NOTIFICATIONS; ix++) {
        sleep(1);
        result_len = sizeof(result);
        rval = rdb_get(rdb_s, rdb_var_name, result, &result_len);
        if (rval) {
            errp("Failed to read measurement characteristic value rdb var\n");
            goto stop_notify;
        }
        syslog(LOG_INFO, "Oximetry Measurement %d:\n", ix);
        if (!parse_serial_data_format(result, result_len, &sdf)) {
            syslog(LOG_INFO, "  Status=%02x, BattVolt=%d, PI=%d, Counter=%d, "
                   "SpO2=%d, PulseRate=%d\n", sdf.status, sdf.batt_volt,
                   sdf.pi, sdf.counter, sdf.spo2, sdf.pulse_rate);
        }
    }
#endif

    /* a better way is to use rdb subscription */
    rval = rdb_subscribe(rdb_s, rdb_var_name);
    if (rval) {
        errp("Failed to subscribe to rdb var %s\n", rdb_var_name);
        goto stop_notify;
    }
    fd = rdb_fd(rdb_s);
    if (fd < 0) {
        errp("Failed to obtain rdb file descriptor\n");
        rval = -1;
        goto stop_notify;
    }
    ix = 0;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    syslog(LOG_INFO, "Waiting for Measurement Notifications...\n");
    while (select(FD_SETSIZE, &set, NULL, NULL, NULL) > 0) {
        result_len = sizeof(result);
        rval = rdb_get(rdb_s, rdb_var_name, result, &result_len);
        if (rval) {
            errp("Failed to read measurement characteristic value rdb var\n");
            goto stop_notify;
        }
#ifdef DEBUG
        snprint_bytes(dbg_buf, sizeof dbg_buf, result, result_len, " ");
        syslog(LOG_INFO, "Oximetry Measurement #%d: %s\n", ix, dbg_buf);
#endif
        if (!parse_serial_data_format(result, result_len, &sdf)) {
            syslog(LOG_INFO, "  Status=%02x, BattVolt=%d, PI=%d, Counter=%d, "
                   "SpO2=%d, PulseRate=%d\n", sdf.status, sdf.batt_volt,
                   sdf.pi, sdf.counter, sdf.spo2, sdf.pulse_rate);
        }
        if (++ix >= MAX_NOTIFICATIONS) {
            break;
        }
    }

stop_notify:
    syslog(LOG_INFO, "Stopping notification on measurements...\n");
    params[2].value = "0";
    result_len = sizeof result;
    rval = rdb_rpc_client_invoke(rpc_client_s, cmd, params, num_params, 0,
                                 result, &result_len);
    if (rval) {
        errp("Failed to invoke command %s: %s\n", cmd, result);
        goto done;
    }
    /*
     * Note that this only turns off notification sent from bluez to update
     * the cached measurement characteristic value. Since rdb does not support
     * unsubscribe, if the characteristic rdb var is written/deleted, any
     * future select/poll will still be actioned upon until rdb_close.
     */
done:
    return rval;
}

/*
 * Parse a control point response from binary data into rsp struct.
 */
static int
parse_cp_response(const char *data, int len, nonin_cp_rsp_t *rsp)
{
    if (!data) {
        return -EINVAL;
    }
    if (len < 2) {
        return -EINVAL;
    }
    rsp->rsp = *(data++);
    rsp->status = *data;
    return 0;
}

/*
 * Write to the control point characteristic with command Measurement Complete.
 * 1) start notify on cp characteristic
 * 2) write command to cp characteristic
 * 3) wait for notification (response)
 * Upon a successful procedure, the device will turn off its bluetooth radio.
 */
static int
write_cp (const char *rdb_root, rdb_rpc_client_session_t *rpc_client_s,
          const char *dev_idx)
{
    int rval;
    char *cmd;
    rdb_rpc_cmd_param_t params[GATT_RPC_MAX_PARAMS];
    int num_params;
    char result[MAX_BUF_LEN];
    int result_len;
    char handle[MAX_BUF_LEN];
    char rdb_var_name[MAX_NAME_LENGTH];
    int fd;
    fd_set set;
    nonin_cp_cmd_t cp_cmd;
    nonin_cp_rsp_t cp_rsp;
    struct timeval timeout;

    syslog(LOG_INFO, "Start notification on cp...\n");
    memset(params, 0, sizeof params);
    cmd = SET_HANDLE_NOTIFY_CMD;
    params[0].name = "device";
    params[0].value = (char *)dev_idx;
    params[0].value_len = strlen(params[0].value) + 1;
    params[1].name = "handle";
    snprintf(handle, sizeof handle, "%d", NONIN_CONTROL_POINT_HANDLE);
    params[1].value = handle;
    params[1].value_len = strlen(params[1].value) + 1;
    params[2].name = "value";
    params[2].value = "1";
    params[2].value_len = strlen(params[2].value) + 1;
    num_params = SET_HANDLE_NOTIFY_NPARAMS;
    result_len = sizeof result;
    rval = rdb_rpc_client_invoke(rpc_client_s, cmd, params, num_params, 0,
                                 result, &result_len);
    if (rval) {
        errp("Failed to invoke command %s: %s\n", cmd, result);
        goto done;
    }

    snprintf(rdb_var_name, sizeof rdb_var_name,
             "%s.gatt.service.%d.char.%d.value",
             rdb_root, oximetry_srv_idx, cp_char_idx);
    rval = rdb_subscribe(rdb_s, rdb_var_name);
    if (rval) {
        errp("Failed to subscribe to rdb var %s\n", rdb_var_name);
        goto stop_notify;
    }
    fd = rdb_fd(rdb_s);
    if (fd < 0) {
        errp("Failed to obtain rdb file descriptor\n");
        rval = -1;
        goto stop_notify;
    }
    FD_ZERO(&set);
    FD_SET(fd, &set);

    /* send Measurement Complete command */
    cmd = WRITE_HANDLE_CMD;
    cp_cmd.cmd = MEAS_COMPLETE;
    cp_cmd.params[0] = 'N'; /* fixed parameters for complete command */
    cp_cmd.params[1] = 'M';
    cp_cmd.params[2] = 'I';
    cp_cmd.nparams = 3;
    params[2].value = (char *)&cp_cmd;
    params[2].value_len = cp_cmd.nparams + 1;
    num_params = WRITE_HANDLE_NPARAMS;
    result_len = sizeof result;
    rval = rdb_rpc_client_invoke(rpc_client_s, cmd, params, num_params, 0,
                                 result, &result_len);
    if (rval) {
        errp("Failed to invoke command %s: %s\n", cmd, result);
        goto stop_notify;
    }

    syslog(LOG_INFO, "Waiting for CP response...\n");
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    while (select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0) {
        result_len = sizeof result;
        rval = rdb_get(rdb_s, rdb_var_name, result, &result_len);
        if (rval) {
            errp("Failed to read cp value rdb var\n");
            goto stop_notify;
        }
        syslog(LOG_INFO, "CP response: \n");
        if (!parse_cp_response(result, result_len, &cp_rsp)) {
            syslog(LOG_INFO, "  rsp=%02x, status=%d\n", cp_rsp.rsp,
                   cp_rsp.status);
            if (cp_rsp.rsp == MEAS_COMPLETE_RSP &&
                (cp_rsp.rsp == ACCEPT || cp_rsp.rsp == SYNC_IN_PROGRESS)) {
                break;
            }
        } else {
            errp("Failed to parse CP response\n");
        }
    }

stop_notify:
    syslog(LOG_INFO, "Stopping notification on cp...\n");
    cmd = SET_HANDLE_NOTIFY_CMD;
    params[2].value = "0";
    params[2].value_len = strlen(params[2].value) + 1;
    num_params = SET_HANDLE_NOTIFY_NPARAMS;
    result_len = sizeof result;
    rval = rdb_rpc_client_invoke(rpc_client_s, cmd, params, num_params, 0,
                                 result, &result_len);
    if (rval) {
        errp("Failed to invoke command %s: %s\n", cmd, result);
        goto done;
    }

done:
    return rval;
}

/*
 * Invoke gatt rpc service to:
 * 1) read characteristics from device information service;
 * 2) read measurement characteristic using notification;
 * 3) write to the control point characteristic with the Measurement Complete
 *    command to turn off the device's bluetooth radio.
 */
static int
invoke_service (const char *rdb_root, const char *service_name,
                const char *dev_idx)
{
    rdb_rpc_client_session_t *rpc_client_s = NULL;
    int rval;

    INVOKE_CHK(rdb_rpc_client_connect((char*)service_name, &rpc_client_s),
               "Failed to connect to service %s\n", service_name);

    rval = read_device_info(rpc_client_s, dev_idx);
    if (rval) {
        errp("Failed to read device information service\n");
        goto done;
    }

    rval = read_measurement(rdb_root, rpc_client_s, dev_idx);
    if (rval) {
        errp("Failed to read measurement\n");
        goto done;
    }

    rval = write_cp(rdb_root, rpc_client_s, dev_idx);
    if (rval) {
        errp("Failed to write control point\n");
        goto done;
    }

done:
    INVOKE_CHK(rdb_rpc_client_disconnect(&rpc_client_s),
               "Failed to disconnect from service %s\n", service_name);
    return rval;
}

/*
 * Get the device index substring from rdb_root.
 * Note: rdb_root might be overwritten.
 */
const char *
get_device_index (char *rdb_root)
{
    char *token;
    const char *index = NULL;
    const char *delim = ".";
    token = strtok(rdb_root, delim);
    while (token) {
        index = token;
        token = strtok(NULL, delim);
    }
    return index;
}

int
main (int argc, char *argv[])
{
    char *rdb_root;
    const char *dev_idx;
    int rval;


    if (argc != 3) {
        usage(argv[0]);
        return -1;
    }

    dbgp("nonin_app started\n");

    dbgp("rdb_root=%s, service_name=%s\n", argv[1], argv[2]);

    /* duplicate string since get_device_index below will overwrite it */
    rdb_root = strdup(argv[1]);
    if (!rdb_root) {
        errp("Failed to duplicate argv[1] %s\n", argv[1]);
        return -1;
    }

    dev_idx = get_device_index(rdb_root);
    if (!dev_idx) {
        errp("Failed to get device index from %s\n", rdb_root);
        rval = -1;
        goto done;
    }

    rval = rdb_open(NULL, &rdb_s);
    if (rval) {
        errp("Failed to open rdb session");
        goto done;
    }

    syslog(LOG_INFO, "Reading rdb var tree %s...\n", argv[1]);
    rval = read_rdb_vars(argv[1]);
    if (rval) {
        errp("Failed to read rdb vars\n");
        goto done;
    }

    syslog(LOG_INFO, "Invoking rpc service %s...\n", argv[2]);
    rval = invoke_service(argv[1], argv[2], dev_idx);
    if (rval) {
        errp("Failed to invoke rpc service\n");
        goto done;
    }

    rval = 0;

done:
    free(rdb_root);
    if (rdb_s) {
        rdb_close(&rdb_s);
    }

    dbgp("nonin_app ended\n");

    return rval;
}
