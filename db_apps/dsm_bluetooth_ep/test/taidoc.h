#ifndef TAIDOC_H_123923022016
#define TAIDOC_H_123923022016
/*
 * Taidoc sample application
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

#include <string.h>

#define FRAME_START_VAL 0x51
#define FRAME_STOP_VAL  0xA3

#define FRAME_CMD_ID_IDX     1
#define FRAME_DATA_START_IDX 2

#define CMD_READ_DEV_MODEL_ID            0x24
#define CMD_READ_DATA1_ID                0x25
#define CMD_READ_DATA2_ID                0x26
#define CMD_READ_STORAGE_NUM_DATA_ID     0x2B
#define CMD_TURN_OFF_ID                  0x50

#define CMD_INIT(req, cmd, resp) {                          \
        memset((resp), 0, sizeof *(resp));                  \
        memset((req), 0, sizeof *(req));                    \
        (req)->frame_start = FRAME_START_VAL;               \
        (req)->cmd_id = cmd;                                \
        (req)->frame_stop = FRAME_STOP_VAL;                 \
}

struct td_cmd_dev_model_req_ {
    char frame_start;
    char cmd_id;
    char unused[4];
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

struct td_cmd_dev_model_resp_ {
    char frame_start;
    char cmd_id;
    unsigned short model;
    char unused[2];
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td_cmd_dev_model_req_ td_cmd_dev_model_req_t;
typedef struct td_cmd_dev_model_resp_ td_cmd_dev_model_resp_t;

struct td_cmd_data1_req_ {
    char frame_start;
    char cmd_id;
    short index;
    char unused[1];
    char user;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

struct td_cmd_data1_resp_ {
    char frame_start;
    char cmd_id;
    char date[2];
    char time_arrhy[2];
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td_cmd_data1_req_ td_cmd_data1_req_t;
typedef struct td_cmd_data1_resp_ td_cmd_data1_resp_t;

struct td_cmd_data2_req_ {
    char frame_start;
    char cmd_id;
    short index;
    char unused[1];
    char user;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

struct td_cmd_data2_resp_ {
    char frame_start;
    char cmd_id;
    char systolic;
    char map;
    char diastolic;
    char pulse;
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td_cmd_data2_req_ td_cmd_data2_req_t;
typedef struct td_cmd_data2_resp_ td_cmd_data2_resp_t;

struct td_cmd_storage_num_data_req_ {
    char frame_start;
    char cmd_id;
    char user;
    char unused[3];
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

struct td_cmd_storage_num_data_resp_ {
    char frame_start;
    char cmd_id;
    unsigned short num;
    char unused[2];
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td_cmd_storage_num_data_req_ td_cmd_storage_num_data_req_t;
typedef struct td_cmd_storage_num_data_resp_ td_cmd_storage_num_data_resp_t;

struct td_cmd_turn_off_req_ {
    char frame_start;
    char cmd_id;
    char unused[4];
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

struct td_cmd_turn_off_resp_ {
    char frame_start;
    char cmd_id;
    char proj_code_low;
    char proj_code_high;
    char unused[2];
    char frame_stop;
    char chk_sum;
} __attribute__((packed));

typedef struct td_cmd_turn_off_req_ td_cmd_turn_off_req_t;
typedef struct td_cmd_turn_off_resp_ td_cmd_turn_off_resp_t;

#define TD_DEFAULT_USER 1 /* Currently only support first user. */
#define NUM_READINGS_MAX 5 /* Max number of readings to record. */
#define TD_RDB_BUF_LEN 32
#define TD_MAX_NAME_LEN 64

int cmd_read_dev_model(td_cmd_dev_model_resp_t *resp);
int cmd_read_storage_num_data(void);
int cmd_turn_off(void);

#endif /* TAIDOC_H_123923022016 */
