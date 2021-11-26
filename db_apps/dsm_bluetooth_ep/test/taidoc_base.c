/*
 * Utility functions for Taidoc ear thermometer application.
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

#include "taidoc.h"
#include "dsm_bt_utils.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

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

/* read exactly len bytes from stdin */
#define MAX_EAGAIN_RETRIES 6
#define RETRY_WAIT 100000000L
static int
read_a_resp (char *buf, size_t len)
{
    struct timespec req = { 0, RETRY_WAIT }, rem;
    ssize_t rval;
    size_t nread = 0;
    int retries = 0;
    do {
        rval = read(STDIN_FILENO, buf + nread, len - nread);
        if (rval < 0) {
            if (errno != EAGAIN) {
                dbgp("Fatal\n");
                return -errno;
            }
            dbgp("EAGAIN\n");
            /* retry after a while (0.1s) */
            nanosleep(&req, &rem);
        } else {
            nread += rval;
            dbgp("Read %d/%d bytes\n", rval, nread);
        }
        retries++;
    } while (nread < len && retries < MAX_EAGAIN_RETRIES);
    return retries < MAX_EAGAIN_RETRIES ? 0 : -1;
}

/*
 * Send a command to the sensor and get the response.
 */
#define NUM_RETRY_MAX 5
#define DBG_BUF_LEN 64
static int
send_cmd (char *request, int req_len, char *response, int res_len)
{
    int n;
    int retry = NUM_RETRY_MAX;
    int rval;

#ifdef DEBUG
    char dbg_buf[DBG_BUF_LEN];
#endif

    add_chk_sum(request, req_len);

    do {
        dbgp("About to write command 0x%02x\n", request[FRAME_CMD_ID_IDX]);
        n = write(STDOUT_FILENO, request, req_len);
        if (n != req_len) {
            dbgp("Command 0x%02x request write failed: %s\n",
                 request[FRAME_CMD_ID_IDX], strerror(errno));
            n = -1;
            continue;
        }

        dbgp("Write finished\n");
        if (!response) {
            break;
        }

        dbgp("About to read response\n");
        sleep(1);
        rval = read_a_resp(response, res_len);
        if (rval) {
            dbgp("Command 0x%x response read failed (%d)\n",
                 request[FRAME_CMD_ID_IDX], rval);
            sleep(1);
            n = -1;
            continue;
        }

#ifdef DEBUG
        snprint_bytes(dbg_buf, sizeof dbg_buf, response, res_len, " ");
        dbgp("Response: %s\n", dbg_buf);
#endif
    } while ((n < 0 ||
              request[FRAME_CMD_ID_IDX] != response[FRAME_CMD_ID_IDX])
             && --retry > 0);

    return retry > 0 ? 0 : -1;
}

int
cmd_read_dev_model (td_cmd_dev_model_resp_t *resp)
{
    td_cmd_dev_model_req_t req;
    int rval;

    CMD_INIT(&req, CMD_READ_DEV_MODEL_ID, resp);

    rval = send_cmd((char *)&req, sizeof req, (char *)resp, sizeof *resp);
    if (rval) {
        errp("Failed to read dev model\n");
    } else {
        dbgp("Model: 0x%04x\n", resp->model);
    }

    return rval;
}

int
cmd_read_storage_num_data (void)
{
    td_cmd_storage_num_data_req_t req;
    td_cmd_storage_num_data_resp_t resp;
    int rval;

    CMD_INIT(&req, CMD_READ_STORAGE_NUM_DATA_ID, &resp);

    rval = send_cmd((char *)&req, sizeof req, (char *)&resp, sizeof resp);

    if (rval) {
        errp("Failed to read storage number of data\n");
        return rval;
    } else {
        dbgp("Storage number=%d\n", resp.num);
        return resp.num;
    }
}

#define IGNORE_TURNOFF_FAIL

int
cmd_turn_off (void)
{
    td_cmd_turn_off_req_t req;
    td_cmd_turn_off_resp_t resp;
    int rval;

    CMD_INIT(&req, CMD_TURN_OFF_ID, &resp);

    rval = send_cmd((char *)&req, sizeof req, (char *)&resp, sizeof resp);

#ifdef IGNORE_TURNOFF_FAIL
    /* turn off does not respond correctly, skip response */
    (void)rval;
    return 0;
#else
    if (rval) {
        errp("Failed to turn off device\n");
    } else {
        dbgp("Device turned off successfully\n");
    }
    return rval;
#endif
}
