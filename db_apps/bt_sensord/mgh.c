/*
 * mgh.c:
 *    Entra Health MyGlucoHealth glucometer device handling
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
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>

#include "bt_sensord.h"
#include "mgh.h"

/*
 * Add the MGH checksum to a command frame.
 *
 * Parameters:
 *    frame    [inout] The command frame.
 */
static void
add_chk_sum (char *frame)
{
    mgh_frame_hdr_t *frame_hdr = (mgh_frame_hdr_t *)frame;
    int data_size = frame_hdr->size - MGH_FRAME_CMD_SIZE;
    mgh_frame_chksum_t *frame_csum = (mgh_frame_chksum_t *)
        &frame[MGH_FRAME_CHKSUM_START_IDX(frame_hdr)];
    unsigned char csum_l, csum_h;
    int ix;

    csum_l = frame_hdr->start ^ frame_hdr->size_chk;
    for (ix = 0; ix < data_size; ix += 2) {
        csum_l ^= frame[ix + MGH_FRAME_DATA_START_IDX];
    }
    csum_l = ~csum_l;

    csum_h = frame_hdr->size ^ frame[MGH_FRAME_CMD_IDX];
    for (ix = 1; ix < data_size; ix += 2) {
        csum_h ^= frame[ix + MGH_FRAME_DATA_START_IDX];
    }
    csum_h = ~csum_h;

    frame_csum->low = csum_l;
    frame_csum->high = csum_h;
}

/*
 * Send a command to an MGH device and get the response.
 *
 * Parameters:
 *    fd         [in] File descriptor for opened rfcomm device connected to the
 *               MGH.
 *    request    [in] The request command frame.
 *    response   [out] The response frame. Only valid if function success.
 *    res_len    [in] The length in bytes of the response.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
static int
send_cmd (int fd, char *request, char *response,
          unsigned int res_len)
{
    int n = 0;
    int readn = 0;
    fd_set rfds;
    struct timeval tv;
    int retval;

    add_chk_sum(request);
    write(fd, request, MGH_FRAME_SIZE(request));

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = MGH_CMD_TIMEOUT_SECS;
    tv.tv_usec = 0;

    do  {
        retval = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (retval == 1) {
            n = read(fd, &response[readn], res_len - readn);
            readn += n;
        }
    } while ((readn != res_len) && (retval == 1));

    dbgp("mgh: Command 0x%x read %d bytes, expected %d bytes\n",
         request[MGH_FRAME_CMD_IDX], readn, res_len);
    dbgp("mgh: data: ");
    for (n = 0; n < readn; n++) {
        dbgp("0x%x ", response[n]);
    }
    dbgp("\n");
    dbgp("mgh: Command 0x%x response read %s.\n",
         request[MGH_FRAME_CMD_IDX], (readn == res_len) ?
         "succeeded" : "failed");

    return (readn == res_len) ? 0 : -1;
}

/*
 * Fills in the MGH frame header
 *
 * Parameters:
 *    hdr              [out] Pointer to the frame header
 *    cmd_data_size    [in] Size in bytes of the command and data.
 */
static void
mgh_construct_frame_header (mgh_frame_hdr_t *hdr, unsigned char cmd_data_size)
{
    hdr->start = MGH_FRAME_START_VAL;
    hdr->size = cmd_data_size;
    hdr->size_chk = ~(hdr->size);
}

/*
 * See mgh.h
 */
int
mgh_cmd_get_num_records (int fd, mgh_cmd_res_get_num_records_t *response)
{
    mgh_cmd_req_get_num_records_t request;
    int res;

    mgh_construct_frame_header(&request.hdr, MGH_CMD_DATA_SIZE(request));
    request.command = MGH_CMD_CODE_GET_NUM_RECORDS;

    res = send_cmd(fd, (char *)&request.hdr, (char *)response,
                   sizeof(*response));

    dbgp("mgh: num_records=%d\n", response->num);

    return res;
}

/*
 * See mgh.h
 */
int
mgh_cmd_get_one_data(int fd, unsigned char record_num,
                     mgh_cmd_res_get_one_data_t *response)
{
    mgh_cmd_req_get_one_data_t request;
    int res;

    mgh_construct_frame_header(&request.hdr, MGH_CMD_DATA_SIZE(request));
    request.command = MGH_CMD_CODE_GET_ONE_DATA;
    request.record_num = record_num;

    res = send_cmd(fd, (char *)&request.hdr, (char *)response,
                   sizeof(*response));

    dbgp("mgh: data record %d: dd-mm-yy: %02d-%02d-%02d value=%d\n",
         response->record_num, MGH_MEASURE_DAY(response),
         MGH_MEASURE_MONTH(response), MGH_MEASURE_YEAR(response),
         MGH_MEASURE_VALUE(response));

    return res;
}

/*
 * See mgh.h
 */
int
mgh_cmd_get_sw_version (int fd, mgh_cmd_res_get_sw_ver_t *response)
{
    mgh_cmd_req_get_sw_ver_t request;
    int res;

    mgh_construct_frame_header(&request.hdr, MGH_CMD_DATA_SIZE(request));
    request.command = MGH_CMD_CODE_GET_SW_VER;

    res = send_cmd(fd, (char *)&request, (char *)response, sizeof(*response));

    dbgp("mgh: sw version %d.%d\n", response->ver_high, response->ver_low);

    return res;
}

/*
 * See mgh.h
 */
int
mgh_cmd_set_measure_unit(int fd, unsigned char unit,
                             mgh_cmd_res_set_measure_unit_t *response)
{
    mgh_cmd_req_set_measure_unit_t request;
    int res;

    mgh_construct_frame_header(&request.hdr, MGH_CMD_DATA_SIZE(request));
    request.command = MGH_CMD_CODE_SET_MEASURE_UNIT;
    request.unit = unit;

    res = send_cmd(fd, (char *)&request, (char *)response, sizeof(*response));

    return res;
}

/*
 * See mgh.h
 */
int
mgh_read_data (bdaddr_t *src, bdaddr_t *dst, bt_device_data_t *data,
               int max_data_len)
{
    int sock;
    int dev = 1;
    int num_data = 0;
    int fd = 0;
    int res;
    char *portname = "/dev/rfcomm1";

    // FIXME: Channel should be obtained from SDP but MGH device
    // does not seem to respond to browse requests.
    int rfcomm_chan = 1;

    sock = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_RFCOMM);
    if (sock < 0) {
        dbgp("mgh: Can't open RFCOMM control socket");
        goto done;
    }

    rfcomm_bind_dev(sock, dev, 0, src, dst, rfcomm_chan);

    dbgp("mgh: Opening rfcomm%d...", dev);
    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        dbgp("mgh: error %d opening %s: %s\n", errno, portname, strerror (errno));
        goto done;
    }
    dbgp("mgh: opened\n");

    /* set speed to 9600 bps, 8n1 (no parity) and no blocking */
    set_interface_attribs(fd, B9600, 0);

    /*
     * Always just get the latest measurement. Technically all the device data
     * should be erased after reading so that the next read does not
     * get the same data. However this is not done because consumables are
     * required for each measurement and this is undersirable under test/demo
     * conditions that this application is targetted for.
     */
    mgh_cmd_res_get_one_data_t one_data_response;
    res = mgh_cmd_get_one_data(fd, 0, &one_data_response);

    if (!res && (one_data_response.record_num != MGH_INVALID_RECORD_NUM)) {
        snprintf(data[0].value, MAX_RDB_BT_VAL_LEN, "%d",
                 MGH_MEASURE_VALUE(&one_data_response));

        // Device only supports years from 2000
        data[0].timestamp.year =
            MGH_MEASURE_YEAR(&one_data_response) + 2000;
        data[0].timestamp.month = MGH_MEASURE_MONTH(&one_data_response);
        data[0].timestamp.day = MGH_MEASURE_DAY(&one_data_response);
        data[0].timestamp.hour = MGH_MEASURE_HOUR(&one_data_response);
        data[0].timestamp.minute = MGH_MEASURE_MIN(&one_data_response);

        num_data = 1;
    }

 done:
    if (fd >= 0) {
        close(fd);
    }

    return num_data;
}

