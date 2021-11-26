/*
 * td2551.h:
 *    Taidoc 2551 Weight Scale device handling public declarations
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
#ifndef __TD2551_H__
#define __TD2551_H__

struct td2551_device_clock_ {
    unsigned char year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
} __attribute__((packed));

typedef struct td2551_device_clock_ td2551_device_clock_t;

struct td2551_weight_data_ {
    char frame_start;
    char cmd_id;
    char length;
    char unused;
    char year;
    char month;
    char day;
    char hour;
    char minute;
    char code;
    char gender;
    char height_cm;
    short height_in;
    char age;
    char cal_unit;
    short weight_kg;
    short weight_lb;
    short bmi;
    short bmr;
    short bf;
    short bm;
    short bn;
    short bw;
    short status;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td2551_weight_data_ td2551_weight_data_t;

struct td2551_device_model_ {
    char frame_start;
    char cmd_id;
    short model;
    short ununsed;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td2551_device_model_ td2551_device_model_t;

struct td2551_storage_num_data_ {
    char frame_start;
    char cmd_id;
    short num;
    short ununsed;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td2551_storage_num_data_ td2551_storage_num_data_t;

struct td2551_clear_data_ {
    char frame_start;
    char cmd_id;
    int unused;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td2551_clear_data_ td2551_clear_data_t;

struct td2551_turn_off_ {
    char frame_start;
    char cmd_id;
    int unused;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td2551_turn_off_ td2551_turn_off_t;

/*
 * td2551 Read device clock command.
 *
 * Parameters:
 *    fd           [in] File descriptor or opened rfcomm device connected to
 *                 a  td2551.
 *    dev_clock    [out] Command response data.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int td2551_read_dev_clock_cmd(int fd, td2551_device_clock_t *dev_clock);

/*
 * td2551 Set device clock command.
 *
 * Parameters:
 *    fd           [in] File descriptor or opened rfcomm device connected to
 *                 a  td2551.
 *    dev_clock    [in] Clock values to set.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int td2551_set_dev_clock_cmd(int fd, td2551_device_clock_t *dev_clock);

/*
 * td2551 Read single data command.
 *
 * Parameters:
 *    fd           [in] File descriptor or opened rfcomm device connected to
 *                 a  td2551.
 *    wdata        [out] Weight data.
 *    data_idx     [in] Index of the data to be read.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int td2551_read_data_cmd(int fd, td2551_weight_data_t *wdata,
                                short data_idx);

/*
 * td2551 Read device model command.
 *
 * Parameters:
 *    fd              [in] File descriptor or opened rfcomm device connected to
 *                    a  td2551.
 *    device_model    [out] Device model data.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int td2551_read_device_model_cmd(int fd,
                                        td2551_device_model_t *device_model);

/*
 * td2551 Read number of stored data entries command.
 *
 * Parameters:
 *    fd          [in] File descriptor or opened rfcomm device connected to
 *                a  td2551.
 *    num_data    [out] Num data response value.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int td2551_read_storage_num_data_cmd(int fd,
                                            td2551_storage_num_data_t *num_data);

/*
 * td2551 Clear all stored data command..
 *
 * Parameters:
 *    fd          [in] File descriptor or opened rfcomm device connected to
 *                a  td2551.
 *    clear_data  [out] Clear data response.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int td2551_clear_data_cmd(int fd, td2551_clear_data_t *clear_data);

/*
 * td2551 Turn off device command.
 *
 * Parameters:
 *    fd          [in] File descriptor or opened rfcomm device connected to
 *                a  td2551.
 *    clear_data  [out] Command response.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
extern int td2551_turn_off_cmd(int fd, td2551_turn_off_t *turn_off_resp);

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
extern int td2551_read_data(bdaddr_t *src, bdaddr_t *dst,
                            bt_device_data_t *data, int max_data_len);

#endif /* __TD2551_H__ */
