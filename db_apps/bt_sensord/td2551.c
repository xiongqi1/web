/*
 * td2551.c:
 *    Taidoc 2551 Weight Scale device handling.
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Pty. Ltd.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include "bt_sensord.h"
#include "td2551.h"
#include "td2551_priv.h"

/*
 * Add the TD2551 checksum to a command frame.
 *
 * Parameters:
 *    cmd    [inout] The command frame.
 *    sz     [in] The size in bytes of the command frame.
 */
static void
add_chk_sum(char *cmd, int sz)
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
 * Send a command to a TD2551 device and get the response.
 *
 * Parameters:
 *    fd         [in] File descriptor for opened rfcomm device connected to the
 *               td2551.
 *    request    [in] The request command frame.
 *    req_len    [in] The length in bytes of the request.
 *    response   [out] The response frame. Only valid if function success.
 *    res_len    [in] The length in bytes of the response.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
static int
send_cmd(int fd, char *request, int req_len, char *response, int res_len)
{
    int n;

    add_chk_sum(request, req_len);
    write(fd, request, req_len);

    n = read(fd, response, res_len);
    if (n != res_len) {
        dbgp("Command 0x%x response read failed\n", request[FRAME_CMD_ID_IDX]);
        return -1;
    }

    return 0;
}

/*
 * See td2551.h
 */
int
td2551_read_dev_clock_cmd (int fd, td2551_device_clock_t *dev_clock)
{
    int n;
    char cmd[] = { FRAME_START_VAL, CMD_GET_CLOCK_ID, 0, 0, 0, 0,
                   FRAME_STOP_VAL, 0 };
    char response[CMD_GET_CLOCK_RES_LEN];

    if (!dev_clock) {
        return -1;
    }

    add_chk_sum(cmd, CMD_GET_CLOCK_REQ_LEN);
    write(fd, cmd, CMD_GET_CLOCK_REQ_LEN);

    n = read(fd, response, CMD_GET_CLOCK_RES_LEN);
    if (n != CMD_GET_CLOCK_RES_LEN) {
        dbgp("%s: read error\n", __FUNCTION__);
        return -1;
    }

    dev_clock->day = response[2] & 0x1f;
    dev_clock->month = ((response[3] & 0x1) << 3) | ((response[2] & 0xb0) >> 5);
    dev_clock->year = response[3] >> 1;
    dev_clock->minute = response[4] & 0x3f;
    dev_clock->hour = response[5] & 0x1f;
    return 0;
}

/*
 * See td2551.h
 */
int
td2551_set_dev_clock_cmd (int fd, td2551_device_clock_t *dev_clock)
{
    int n;
    char cmd[] = { FRAME_START_VAL, CMD_SET_CLOCK_ID, 0, 0, 0, 0,
                   FRAME_STOP_VAL, 0 };
    char response[CMD_SET_CLOCK_RES_LEN];

    cmd[2] = ((((dev_clock->month & 0x7) << 5) | (dev_clock->day & 0x1f))
                  & 0xff);
    cmd[3] = (((dev_clock->year & 0x7f) << 1) |
                  ((dev_clock->month >> 3) & 0x1)) & 0xff;
    cmd[4] = dev_clock->minute & 0x3f;
    cmd[5] = dev_clock->hour & 0x1f;

    add_chk_sum(cmd, CMD_SET_CLOCK_REQ_LEN);

    write(fd, cmd, CMD_SET_CLOCK_REQ_LEN);

    n = read(fd, response, CMD_SET_CLOCK_RES_LEN);
    if (n == CMD_SET_CLOCK_RES_LEN) {
        return 0;
    } else {
        dbgp("%s: read error\n", __FUNCTION__);
        return 1;
    }
}

/*
 * See td2551.h
 */
int
td2551_read_data_cmd (int fd, td2551_weight_data_t *wdata, short data_idx)
{
    int n;
    char cmd[] = { FRAME_START_VAL, CMD_READ_DATA_ID, 2, 0, 0, FRAME_STOP_VAL,
                   0 };

    cmd[3] = (data_idx & 0xff);
    cmd[4] = (data_idx >> 8) & 0xff;

    add_chk_sum(cmd, CMD_READ_DATA_REQ_LEN);
    write(fd, cmd, CMD_READ_DATA_REQ_LEN);

    n = read(fd, wdata, CMD_READ_DATA_RES_LEN);
    if (n != CMD_READ_DATA_RES_LEN) {
        dbgp("%s: read error read=%d\n", __FUNCTION__, n);
        return -1;
    }

    return 0;
}

/*
 * See td2551.h
 */
int
td2551_read_device_model_cmd (int fd, td2551_device_model_t *device_model)
{
    int r;
    char cmd[] = { FRAME_START_VAL, CMD_READ_DEVICE_MODEL_ID, 0, 0, 0, 0,
                   FRAME_STOP_VAL, 0 };

    r = send_cmd(fd, cmd, CMD_READ_DEVICE_MODEL_REQ_LEN, (char *)device_model,
                 CMD_READ_DEVICE_MODEL_RES_LEN);

    return r;
}

/*
 * See td2551.h
 */
int
td2551_read_storage_num_data_cmd (int fd, td2551_storage_num_data_t *num_data)
{
    int r;
    char cmd[] = { FRAME_START_VAL, CMD_READ_NUM_DATA_ID, 0, 0, 0, 0,
                   FRAME_STOP_VAL, 0 };

    r = send_cmd(fd, cmd, CMD_READ_NUM_DATA_REQ_LEN, (char *)num_data,
                 CMD_READ_NUM_DATA_RES_LEN);

    return r;
}

/*
 * See td2551.h
 */
int
td2551_clear_data_cmd (int fd, td2551_clear_data_t *clear_data)
{
    int r;
    char cmd[] = { FRAME_START_VAL, CMD_CLEAR_DATA_ID, 0, 0, 0, 0,
                   FRAME_STOP_VAL, 0 };

    r = send_cmd(fd, cmd, CMD_CLEAR_DATA_REQ_LEN, (char *)clear_data,
                 CMD_CLEAR_DATA_RES_LEN);

    return r;
}

/*
 * See td2551.h
 */
int
td2551_turn_off_cmd (int fd, td2551_turn_off_t *turn_off_resp)
{
    int r;
    char cmd[] = { FRAME_START_VAL, CMD_TURN_OFF_ID, 0, 0, 0, 0,
                   FRAME_STOP_VAL, 0 };

    r = send_cmd(fd, cmd, CMD_TURN_OFF_REQ_LEN, (char *)turn_off_resp,
                 CMD_TURN_OFF_RES_LEN);

    return r;
}

/*
 * Connects to a td2551 and reads all available data from it.
 *
 * Parameters:
 *   src              [in] The source BT address (ie, local adapter).
 *   dst              [in] The destination BT address (ie, sensor device).
 *   data             [out] Any read data will be stored in this array.
 *   max_data_len     [in] Max entries that can be stored in the data array.
 *
 * Returns:
 *    The number of read and stored data entries.
 */
int
td2551_read_data (bdaddr_t *src, bdaddr_t *dst, bt_device_data_t *data,
                  int max_data_len)
{
    int sock;
    int dev = 1;
    int ret, num_data = 0;
    int fd = 0;
    char *portname = "/dev/rfcomm1";

    sock = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_RFCOMM);
    if (sock < 0) {
        dbgp("Can't open RFCOMM control socket");
        goto done;
    }

    rfcomm_bind_dev(sock, dev, 0, src, dst, 6);

    dbgp("Opening rfcomm%d...", dev);
    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        dbgp("error %d opening %s: %s\n", errno, portname, strerror (errno));
        goto done;
    }
    dbgp("opened\n");

    /* set speed to 19200 bps, 8n1 (no parity) and no blocking */
    set_interface_attribs (fd, B19200, 0);

    td2551_storage_num_data_t storage_num_data;
    memset(&storage_num_data, 0, sizeof(td2551_storage_num_data_t));
    ret = td2551_read_storage_num_data_cmd(fd, &storage_num_data);
    if (!ret) {
        dbgp("Num data: %d\n", storage_num_data.num);
    } else {
        goto done;
    }

    if (storage_num_data.num > 0) {

        /* Read data starting from highest index as that is the oldest entry */
        int data_idx = storage_num_data.num - 1;

        while (storage_num_data.num-- > 0) {
            td2551_weight_data_t wdata;

            if (num_data == max_data_len) {
                break;
            }

            memset(&wdata, 0, sizeof(td2551_weight_data_t));
            ret = td2551_read_data_cmd(fd, &wdata, data_idx--);

            if (!ret) {
                dbgp("Weight: %.1flb on %02d-%02d-%02d at %02d:%02d\n",
                     ((wdata.weight_lb & 0xff) << 8 |
                      ((wdata.weight_lb & 0xff00) >> 8)) / 10.0,
                     wdata.day, wdata.month, wdata.year, wdata.hour,
                     wdata.minute);

                // Device only supports years from 2000
                data[num_data].timestamp.year = wdata.year + 2000;
                data[num_data].timestamp.month = wdata.month;
                data[num_data].timestamp.day = wdata.day;
                data[num_data].timestamp.hour = wdata.hour;
                data[num_data].timestamp.minute = wdata.minute;

                snprintf(data[num_data].value, MAX_RDB_BT_VAL_LEN,
                         "%.1f",
                         ((wdata.weight_lb & 0xff) << 8 |
                          ((wdata.weight_lb & 0xff00) >> 8)) / 10.0);

                num_data++;
            }
        }

        td2551_clear_data_t clear_data;
        td2551_clear_data_cmd(fd, &clear_data);
    }

    td2551_turn_off_t turn_off_resp;
    td2551_turn_off_cmd(fd, &turn_off_resp);

 done:

    if (fd >= 0) {
        close(fd);
    }

    return num_data;
}
