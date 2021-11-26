/*
 * mgh.c:
 *    Entra Health MyGlucoHealth glucometer device handling defines
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
#ifndef __MGH_H_
#define __MGH_H__

#include <arpa/inet.h>


/* Request and response frame structures */

struct mgh_frame_hdr_ {
    unsigned char start;
    unsigned char size;
    unsigned char size_chk;
} __attribute__((packed));

typedef struct mgh_frame_hdr_ mgh_frame_hdr_t;

struct mgh_frame_chksum_ {
    unsigned char low;
    unsigned char high;
} __attribute__((packed));

typedef struct mgh_frame_chksum_ mgh_frame_chksum_t;

struct mgh_frame_nodata_ {
    mgh_frame_hdr_t hdr;
    unsigned char command;
    mgh_frame_chksum_t chksum;
} __attribute__((packed));

typedef struct mgh_frame_nodata_ mgh_frame_nodata_t;

typedef mgh_frame_nodata_t mgh_cmd_req_get_sw_ver_t;

struct mgh_cmd_res_get_sw_ver_ {
    mgh_frame_hdr_t hdr;
    unsigned char command;
    unsigned char ver_high;
    unsigned char ver_low;
    mgh_frame_chksum_t chksum;
} __attribute__((packed));

typedef struct mgh_cmd_res_get_sw_ver_ mgh_cmd_res_get_sw_ver_t;

typedef mgh_frame_nodata_t mgh_cmd_req_get_num_records_t;

struct mgh_cmd_res_get_num_records_ {
    mgh_frame_hdr_t hdr;
    unsigned char command;
    unsigned char num;
    mgh_frame_chksum_t chksum;
} __attribute__((packed));

typedef struct mgh_cmd_res_get_num_records_ mgh_cmd_res_get_num_records_t;

struct mgh_cmd_req_get_one_data_ {
    mgh_frame_hdr_t hdr;
    unsigned char command;
    unsigned char record_num;
    mgh_frame_chksum_t chksum;
} __attribute__((packed));

typedef struct mgh_cmd_req_get_one_data_ mgh_cmd_req_get_one_data_t;

struct mgh_cmd_res_get_one_data_ {
    mgh_frame_hdr_t hdr;
    unsigned char command;
    unsigned char record_num;
    unsigned short date;
    unsigned short temp_val;
    unsigned short event_hour_min;
    mgh_frame_chksum_t chksum;
} __attribute__((packed));

typedef struct mgh_cmd_res_get_one_data_ mgh_cmd_res_get_one_data_t;

struct mgh_cmd_measure_unit_ {
    mgh_frame_hdr_t hdr;
    unsigned char command;
    unsigned char unit;
    mgh_frame_chksum_t chksum;
} __attribute__((packed));

typedef struct mgh_cmd_measure_unit_ mgh_cmd_req_set_measure_unit_t;
typedef struct mgh_cmd_measure_unit_ mgh_cmd_res_set_measure_unit_t;

/* Command codes */
#define MGH_CMD_CODE_GET_NUM_RECORDS   (0x00)
#define MGH_CMD_CODE_GET_ONE_DATA      (0x01)
#define MGH_CMD_CODE_SET_SYS_DATE      (0x04)
#define MGH_CMD_CODE_SET_MEASURE_UNIT  (0x05)
#define MGH_CMD_CODE_SET_AVG_DAY       (0x06)
#define MGH_CMD_CODE_SET_ALARM         (0x07)
#define MGH_CMD_CODE_SET_DEV_ID        (0x08)
#define MGH_CMD_CODE_GET_DEV_ID        (0x09)
#define MGH_CMD_CODE_GET_SW_VER        (0x0A)
#define MGH_CMD_CODE_TURN_OFF          (0x0B)
#define MGH_CMD_CODE_SEND_DATA         (0x0D)
#define MGH_CMD_CODE_GET_MEASURE_UNIT  (0x0E)
#define MGH_CMD_CODE_GET_TEMP_UNITS    (0x0F)
#define MGH_CMD_CODE_GET_SYS_TIME      (0x10)
#define MGH_CMD_CODE_GET_STRIP_CODE    (0x11)
#define MGH_CMD_CODE_ERASE_DATA        (0x14)
#define MGH_CMD_CODE_SET_CTRL_SOLN_OPT (0x15)
#define MGH_CMD_CODE_SET_DELETE_OPT    (0x16)
#define MGH_CMD_CODE_SET_TIME_OPT      (0x17)
#define MGH_CMD_CODE_SET_ACT_CODE      (0x18)

#define MGH_MEASURE_UNIT_MG_DL  0
#define MGH_MEASURE_UNIT_MMOL_L 1

#define MGH_FRAME_START_VAL (0x80)

#define MGH_FRAME_CMD_SIZE 1

#define MGH_CMD_TIMEOUT_SECS 10

#define MGH_FRAME_SIZE(frame_p) \
    (sizeof(mgh_frame_hdr_t) + ((mgh_frame_hdr_t *)frame_p)->size + \
     sizeof(mgh_frame_chksum_t))

#define MGH_CMD_DATA_SIZE(frame) \
    (sizeof(frame) - sizeof(frame.hdr) - sizeof(frame.chksum))

#define MGH_FRAME_DATA_START_IDX \
    (sizeof(mgh_frame_hdr_t) + MGH_FRAME_CMD_SIZE)

#define MGH_FRAME_CMD_IDX (sizeof(mgh_frame_hdr_t))

#define MGH_FRAME_CHKSUM_START_IDX(frame_hdr_p) \
    (sizeof(mgh_frame_hdr_t) + frame_hdr_p->size)

/* Macros to extract the different data components from a response */
#define MGH_MEASURE_DAY(response_p) (htons((response_p)->date) & 0x1F)
#define MGH_MEASURE_MONTH(response_p) ((htons((response_p)->date) >> 5) & 0xF)
#define MGH_MEASURE_YEAR(response_p) (htons((response_p)->date) >> 9)
#define MGH_MEASURE_HOUR(response_p) \
    ((htons((response_p)->event_hour_min) >> 6) & 0x1F)
#define MGH_MEASURE_MIN(response_p) (htons((response_p)->event_hour_min) & 0x3F)
#define MGH_MEASURE_VALUE(response_p) (htons((response_p)->temp_val) & 0x3FF)
#define MGH_MEASURE_TEMPERATURE(response_p) \
    (htons((response_p)->temp_val) >> 10)
#define MGH_MEASURE_BMEL(response_p) (htons((response_p)->event_hour_min) >> 15)
#define MGH_MEASURE_MEAL(response_p) \
    ((htons((response_p)->event_hour_min) >> 14) & 0x1)
#define MGH_MEASURE_MEDI(response_p) \
    ((htons((response_p)->event_hour_min) >> 13) & 0x1)
#define MGH_MEASURE_EXER(response_p) \
    ((htons((response_p)->event_hour_min) >> 12) & 0x1)
#define MGH_MEASURE_ATTN(response_p) \
    ((htons((response_p)->event_hour_min) >> 11) & 0x1)

/* Device returns this record number if an invalid record is requested */
#define MGH_INVALID_RECORD_NUM (0xFF)

/*
 * MGH command: Read single data record.
 *
 * Parameters:
 *    fd           [in] File descriptor or opened rfcomm device connected to
 *                 the device
 *    record_num   [in] Number of the record to get.
 *    response     [out] The response data.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int mgh_cmd_get_one_data(int fd, unsigned char record_num,
                                mgh_cmd_res_get_one_data_t *response);

/*
 * MGH command: Get the number of data records on the device.
 *
 * Parameters:
 *    fd           [in] File descriptor or opened rfcomm device connected to
 *                 the device
 *    response     [out] The response data.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int mgh_cmd_get_num_records(int fd,
                                   mgh_cmd_res_get_num_records_t *response);

/*
 * MGH command: Set the units (mg/dl or mmol/l) used for display.
 *
 * Parameters:
 *    fd           [in] File descriptor or opened rfcomm device connected to
 *                 the device
 *    unit         [in] the measurement unit (MGH_MEASURE_UNIT_MG_DL or
 *                 MGH_MEASURE_UNIT_MMOL_L)
 *    response     [out] The response data.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int mgh_cmd_set_measure_unit(int fd, unsigned char unit,
                                    mgh_cmd_res_set_measure_unit_t *response);

/*
 * MGH command: Read the device software version.
 *
 * Parameters:
 *    fd           [in] File descriptor or opened rfcomm device connected to
 *                 the device.
 *    response     [out] The response data.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int mgh_cmd_get_sw_version(int fd, mgh_cmd_res_get_sw_ver_t *response);

/*
 * Connects to the device and reads all available data from it.
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
extern int mgh_read_data(bdaddr_t *src, bdaddr_t *dst, bt_device_data_t *data,
                         int max_data_len);

#endif /* __MGH_H__ */
