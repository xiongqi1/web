/*
 * Bluetooth sensor monitor (btsensormon) taidoc device client.
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
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
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "btsensormon.h"
#include "taidoc.h"

/*
 * Calculate and add the checksum byte to a command frame.
 */
static void
add_chk_sum (char *cmd, int sz)
{
    int csum = 0;
    int ix;

    for (ix = 0; ix < (sz - 1); ix++) {
        csum += cmd[ix];
    }

    csum &= 0xff;

    cmd[sz - 1] = csum;
}

/*
 * Send a command to the sensor and get the response.
 */
#define NUM_RETRY_MAX 5
static int
send_cmd (int fd, char *request, int req_len, char *response, int res_len)
{
    int n;
    int retry = NUM_RETRY_MAX;

    add_chk_sum(request, req_len);

    do {
        n = write(fd, request, req_len);
        if (n != res_len) {
            dbgp("Command 0x%x request write failed: %s\n",
                 request[FRAME_CMD_ID_IDX], strerror(errno));
            retry--;
            continue;
        }

        if (!response) {
            break;
        }

        n = read(fd, response, res_len);
        if (n != res_len) {
            dbgp("Command 0x%x response read failed: %s\n",
                 request[FRAME_CMD_ID_IDX], strerror(errno));
            sleep(1);
        }
    } while (((n != res_len) ||
              (request[FRAME_CMD_ID_IDX] != response[FRAME_CMD_ID_IDX]))
             && --retry > 0);

    return 0;
}

/*
 * Comamnd to turn off the device.
 */
static int
cmd_turn_off (int fd)
{
    td_cmd_turn_off_req_t req;
    td_cmd_turn_off_resp_t resp;
    int rval;

    CMD_INIT(&req, CMD_TURN_OFF_ID, &resp);

    rval = send_cmd(fd, (char *)&req, sizeof(req), (char *)&resp, sizeof(resp));
    if (rval) {
        errp("Failed to turn off device.\n");
    } else {
        dbgp("Device turned off.\n");
    }

    return rval;
}

/*
 * Comamnd to clear the device data memory.
 */
static int
cmd_clr_mem (int fd, int user)
{
    td_cmd_clr_mem_req_t req;
    td_cmd_clr_mem_resp_t resp;
    int rval;

    CMD_INIT(&req, CMD_CLR_MEM_ID, &resp);

    rval = send_cmd(fd, (char *)&req, sizeof(req), (char *)&resp, sizeof(resp));
    if (rval) {
        errp("Failed to clear memory.\n");
    } else {
        dbgp("Device memory cleared.\n");
    }

    return rval;
}

/*
 * Comamnd to read the number of stored data items on the device for
 * a given user.
 */
static int
cmd_read_storage_num_data (int fd, int user,
                           td_cmd_storage_num_data_resp_t *resp)
{
    td_cmd_storage_num_data_req_t req;
    int rval;

    CMD_INIT(&req, CMD_READ_STORAGE_NUM_DATA_ID, resp);
    req.user = user;

    rval = send_cmd(fd, (char *)&req, sizeof(req), (char *)resp, sizeof(*resp));
    if (rval) {
        errp("Failed to read storage number of data.\n");
    } else {
        dbgp("Storage number of data: 0x%x%x\n", resp->num_high, resp->num_low);
    }

    return rval;
}

/*
 * Command to read part1 of the stored data at the given index for a given user.
 */
static int
cmd_read_storage_data1 (int fd, int user, int index, char *resp, int resp_len)
{
    td_cmd_data1_req_t req;
    int rval;

    CMD_INIT(&req, CMD_READ_DATA1_ID, resp);
    req.user = user;
    req.index = index;

    rval = send_cmd(fd, (char *)&req, sizeof(req), resp, resp_len);
    if (rval) {
        errp("Failed to read data1.\n");
    }

    return rval;
}

/*
 * Command to read part2 of the stored data at the given index for a given user.
 */
static int
cmd_read_storage_data2 (int fd, int user, int index, char *resp, int resp_len)
{
    td_cmd_data1_req_t req;
    int rval;

    CMD_INIT(&req, CMD_READ_DATA2_ID, resp);
    req.user = user;
    req.index = index;

    rval = send_cmd(fd, (char *)&req, sizeof(req), resp, resp_len);
    if (rval) {
        errp("Failed to read data2.\n");
    }

    return rval;
}

/*
 * btsm device client accept function. Currently it applies a very basic filter
 * (device name). More detailed filters such as Manufacturer data, device
 * model, serial numbers and MAC adddress can be applied if required.
 */
static bool
taidoc_accept (btsm_device_t *device)
{
    struct {
        const char *dev_name;
        const char *dev_description;
        btsm_device_type_t type;
     } supported_devices[] = {
        { "TAIDOC TD3128", "Taidoc Blood Pressure Monitor (TD3128)",
          DEV_TYPE_TD3128_BPMETER },
        { "TAIDOC TD1261", "Taidoc Ear Thermometer (TD1261)",
          DEV_TYPE_TD1261_THERMOMETER},
        { NULL, NULL, DEV_TYPE_UNKNOWN }
    };
    unsigned int ix = 0;
    const char *name;

    while ((name = supported_devices[ix].dev_name)) {
        /*
         * Some devices do not have a name. Those devices are never accepted
         * as only interested in taidoc named devices.
         */
        if (device->name && !strcmp(device->name, name)) {
            device->description = supported_devices[ix].dev_description;
            device->type = supported_devices[ix].type;
            device->profile_uuid = SPP_UUID;
            return true;
        }

        ix++;
    }

    return false;
}

static int
td3128_read_data (int fd, btsm_device_data_t *out_data,
                  unsigned int num_data)
{
    int rval;
    td3128_cmd_data1_resp_t data1_resp;
    td3128_cmd_data2_resp_t data2_resp;
    int ix;

    /*
     * Read starts from the highest index which stores the least recent
     * data. That is, the data in out_data is stored in ascending
     * chronological order.
     */
    for (ix = 0 ; ix < num_data; ix++) {
        rval = cmd_read_storage_data1(fd, TD_DEFAULT_USER, (num_data - ix - 1),
                                      (char *)&data1_resp, sizeof(data1_resp));
        if (rval) {
            break;
        }

        rval = cmd_read_storage_data2(fd, TD_DEFAULT_USER, (num_data - ix - 1),
                                      (char *)&data2_resp, sizeof(data2_resp));
        if (rval) {
            break;
        }

        out_data[ix].ts.day = data1_resp.date[0] & 0x1F;
        out_data[ix].ts.month = ((data1_resp.date[0] & 0xE0) >> 5) |
            ((data1_resp.date[1] & 1) << 3);
        out_data[ix].ts.year = data1_resp.date[1] >> 1;
        out_data[ix].ts.hour = data1_resp.time_arrhy[1] & 0x1F;
        out_data[ix].ts.minute = data1_resp.time_arrhy[0] & 0x3F;

        snprintf(out_data[ix].value, sizeof(out_data[ix].value),
                 "%d,%d,%d", data2_resp.systolic, data2_resp.diastolic,
                 data2_resp.pulse);
    }

    return ix;
}

static int
td1261_read_data (int fd, btsm_device_data_t *out_data,
                  unsigned int num_data)
{
    int rval;
    td1261_cmd_data1_resp_t data1_resp;
    td1261_cmd_data2_resp_t data2_resp;
    int ix;

    /*
     * Read starts from the highest index which stores the least recent
     * data. That is, the data in out_data is stored in ascending
     * chronological order.
     */
    for (ix = 0 ; ix < num_data; ix++) {
        rval = cmd_read_storage_data1(fd, TD_DEFAULT_USER, (num_data - ix - 1),
                                      (char *)&data1_resp, sizeof(data1_resp));
        if (rval) {
            break;
        }

        rval = cmd_read_storage_data2(fd, TD_DEFAULT_USER, (num_data - ix - 1),
                                      (char *)&data2_resp, sizeof(data2_resp));
        if (rval) {
            break;
        }

        out_data[ix].ts.day = data1_resp.date[0] & 0x1F;
        out_data[ix].ts.month = ((data1_resp.date[0] & 0xE0) >> 5) |
            ((data1_resp.date[1] & 1) << 3);
        out_data[ix].ts.year = data1_resp.date[1] >> 1;
        out_data[ix].ts.hour = data1_resp.hour & 0x1F;
        out_data[ix].ts.minute = data1_resp.minute_type & 0x3F;

        snprintf(out_data[ix].value, sizeof(out_data[ix].value),
                 "%.1f",
                 ((data2_resp.object[1] << 8) | data2_resp.object[0]) / 10.0);

        dbgp("td1261: object temp = %s\n", out_data[ix].value);
    }

    return ix;
}

/*
 * btsm device client collect function. Reads data for the default
 * user, stores it into the out_data buffer.
 */
static int
taidoc_collect (btsm_device_t *device, btsm_device_data_t *out_data,
                unsigned int max_data, btsm_process_data_fn process_fn)
{
    int rval;
    td_cmd_storage_num_data_resp_t num_data_resp;
    int num_data;
    int num_read = 0;

    rval = cmd_read_storage_num_data(device->fd, TD_DEFAULT_USER,
                                     &num_data_resp);
    if (rval) {
        return -1;
    }

    num_data = (num_data_resp.num_high << 8) | num_data_resp.num_low;
    if (num_data > max_data) {
        num_data = max_data;
    }

    switch (device->type) {
    case DEV_TYPE_TD3128_BPMETER:
        num_read = td3128_read_data(device->fd, out_data, num_data);
        break;
    case DEV_TYPE_TD1261_THERMOMETER:
        num_read = td1261_read_data(device->fd, out_data, num_data);
        break;
    default:
        errp("Unsupported taidoc device type %d\n", device->type);
        break;
    }

    cmd_clr_mem(device->fd, TD_DEFAULT_USER);
    cmd_turn_off(device->fd);

    return num_read;
}

btsm_client_t taidoc_client = {
    .accept = taidoc_accept,
    .collect_poll = taidoc_collect,
    .collect_event = NULL,
};
