/*
 * non3230_priv.h:
 *    Taidoc 2551 Weight Scale device handling private declarations
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
#ifndef __NON3230_PRIV_H__
#define __NON3230_PRIV_H__

#define NN_BADDR_BUF_LEN 20
#define NN_MAX_CHAR_DATA_LEN 2

#define NN_CONN_TIMEOUT_SECS 5
#define NN_SYNC_TIMEOUT_SECS 15

#define NN_SYNC_SECS 5

struct mgmt_addr_info {
        bdaddr_t bdaddr;
        uint8_t type;
}  __attribute__((packed));

#define MGMT_OP_UNPAIR_DEVICE           0x001B

struct mgmt_cp_unpair_device {
        struct mgmt_addr_info addr;
        uint8_t disconnect;
}  __attribute__((packed));

struct mgmt_hdr {
        uint16_t opcode;
        uint16_t index;
        uint16_t len;
} __attribute__((packed));

#define MGMT_HDR_SIZE   6

typedef struct {
    bt_device_data_t *data;
    int max_data_len;
    int data_len;
} nonin3230_cb_data_t;

struct att_hdr_ {
    char opcode;
    short handle;
} __attribute__((packed));

typedef struct att_hdr_ att_hdr_t;

struct nonin3230_oximetry_data_ {
    char length;
    char status;
    char battery_voltage;
    short pulse_amplitude_index;
    short counter;
    char spo2;
    short pulse_rate;
} __attribute__((packed));

typedef struct nonin3230_oximetry_data_ nonin3230_oximetry_data_t;

struct nonin3230_cp_response_ {
    char command;
    char val;
} __attribute__((packed));

typedef struct nonin3230_cp_response_ nonin3230_cp_response_t;

struct nonin3230_cp_sync_ {
    char command;
    char val;
} __attribute__((packed));

#define CP_MCOMPLETE_VAL_LEN 3
#define CP_MCOMPLETE_VAL "NMI"
typedef struct nonin3230_cp_sync_ nonin3230_cp_sync_t;

struct nonin3230_cp_mcomplete_ {
    char command;
    char value[CP_MCOMPLETE_VAL_LEN];
} __attribute__((packed));

typedef struct nonin3230_cp_mcomplete_ nonin3230_cp_mcomplete_t;


/* Oximetry Service - Measurement Characteristic */
#define NON3230_OXIMETRY_MEASUREMENT_CHAR_HND 24
#define NON3230_OXIMETRY_MEASUREMENT_CCCD_HND 25

/* Oximetry Service - Control Point Characteristic */
#define NON3230_OXIMETRY_CTRLPNT_CHAR_HND 27
#define NON3230_OXIMETRY_CTRLPNT_CCCD_HND 28

#define NON3230_OXIMETRY_CTRLPNT_SYNC_REQ      0x61
#define NON3230_OXIMETRY_CTRLPNT_SYNC_RES      0xE1
#define NON3230_OXIMETRY_CTRLPNT_MCOMPLETE_REQ 0x62
#define NON3230_OXIMETRY_CTRLPNT_MCOMPLETE_RES 0xE2

#define NON3230_OXIMETRY_CTRLPNT_ACCEPT 0
#define NON3230_OXIMETRY_CTRLPNT_DENIED 1
#define NON3230_OXIMETRY_CTRLPNT_INPROG 2

#define ATT_CCCD_NOTI_ENABLE_VAL  1
#define ATT_CCCD_NOTI_DISABLE_VAL 0

#endif /* __NON3230_PRIV_H__  */
