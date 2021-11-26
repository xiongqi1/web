#ifndef NONIN_H_12380923022016
#define NONIN_H_12380923022016
/*
 * Nonin oximeter sample application
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

#define MAX_SERVICE_INDEX 4
#define MAX_CHARACTERISTIC_INDEX 8

#define UUID_PREFIX "0000"
#define UUID_SUFFIX "-0000-1000-8000-00805f9b34fb"

#define NONIN_DEV_INFO_SERVICE_UUID (UUID_PREFIX "180a" UUID_SUFFIX)
#define NONIN_OXIMETRY_SERVICE_UUID "46a970e0-0d5f-11e2-8b5e-0002a5d5c51b"

#define MANUFACTURER_NAME_UUID (UUID_PREFIX "2a29" UUID_SUFFIX)
#define MODEL_NUMBER_UUID (UUID_PREFIX "2a24" UUID_SUFFIX)
#define SERIAL_NUMBER_UUID (UUID_PREFIX "2a25" UUID_SUFFIX)
#define SOFTWARE_REVISION_UUID (UUID_PREFIX "2a28" UUID_SUFFIX)
#define FIRMWARE_REVISION_UUID (UUID_PREFIX "2a26" UUID_SUFFIX)

#define MANUFACTURER_NAME_HANDLE 7
#define MANUFACTURER_NAME_DESC_HANDLE 9
#define MODEL_NUMBER_HANDLE 10
#define MODEL_NUMBER_DESC_HANDLE 12
#define SERIAL_NUMBER_HANDLE 13
#define SERIAL_NUMBER_DESC_HANDLE 15
#define SOFTWARE_REVISION_HANDLE 16
#define SOFTWARE_REVISION_DESC_HANDLE 18
#define FIRMWARE_REVISION_HANDLE 19
#define FIRMWARE_REVISION_DESC_HANDLE 21

#define NONIN_OXIMETRY_MEAS_UUID "0aad7ea0-0d60-11e2-8e3c-0002a5d5c51b"
#define NONIN_CONTROL_POINT_UUID "1447af80-0d60-11e2-88b6-0002a5d5c51b"

#define NONIN_OXIMETRY_MEAS_HANDLE 23
#define NONIN_OXIMETRY_MEAS_DESC_HANDLE 25
#define NONIN_CONTROL_POINT_HANDLE 26
#define NONIN_CONTROL_POINT_DESC_HANDLE 28

typedef struct nonin_data_format19_ {
    unsigned char len;
    unsigned char status;
    unsigned char batt_volt;
    unsigned short pi;
    unsigned short counter;
    unsigned char spo2;
    unsigned short pulse_rate;
} nonin_data_format19_t;

enum nonin_cp_cmd {
    DISPLAY_SYNC  = 0x61,
    SET_OUI_RANGE = 0x60,
    MEAS_COMPLETE = 0x62,
    DELETE_BOND   = 0x63,
    SET_SEC_MODE  = 0x64
};

enum nonin_cp_rsp {
    DISPLAY_SYNC_RSP  = 0xe1,
    SET_OUI_RANGE_RSP = 0xe0,
    MEAS_COMPLETE_RSP = 0xe2,
    DELETE_BOND_RSP   = 0xe3,
    SET_SEC_MODE_RSP  = 0xe4
};

enum nonin_cp_rsp_status {
    ACCEPT = 0,
    DENIED = 1,
    SYNC_IN_PROGRESS = 2
};

#define MAX_CP_CMD_PARAMS 4
struct nonin_cp_cmd_ {
    unsigned char cmd;
    unsigned char params[MAX_CP_CMD_PARAMS];
    unsigned int nparams;
} __attribute__((packed));
typedef struct nonin_cp_cmd_ nonin_cp_cmd_t;

struct nonin_cp_rsp_ {
    unsigned char rsp;
    unsigned char status;
} __attribute__((packed));
typedef struct nonin_cp_rsp_ nonin_cp_rsp_t;

#endif /* NONIN_H_12380923022016 */
