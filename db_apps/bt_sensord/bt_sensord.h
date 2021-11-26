/*
 * bt_sensord.h
 *    Bluetooth sensor daemon public declarations
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
#ifndef __BT_SENSORD_H__
#define __BT_SENSORD_H__

#include <stdio.h>
#include <bluetooth/bluetooth.h>

//#define DBG
#ifdef DBG
#define dbgp(x...) printf(x)
#else
#define dbgp(x...)
#endif

#define MAX_BT_DEVICES 10
#define MAX_RDB_BT_VAL_LEN 32
#define MAX_RDB_BT_VAR_LEN 64

/* Max number of data entries read per polling cycle per device */
#define MAX_SENSOR_DATA_LEN 16

#define MAX_SENSOR_SERIAL_NUM_LEN 16

#define RDB_DATA_GLOBAL_IDX (-1)

/*
 * Valid sensor devices.
 */
typedef enum {
    DEV_TYPE_UNKNOWN = 0,
    DEV_TYPE_TD2251_WEIGHT_SCALE,
    DEV_TYPE_NN3350_PULSE_OXIMETER,
    DEV_TYPE_MGH_GLUCOMETER,
    DEV_TYPE_MAX,
} bt_device_type_t;

/*
 * Describes a bluetooth device
 */
typedef struct {
    bdaddr_t addr;
    bt_device_type_t type;
    char serial_num[MAX_SENSOR_SERIAL_NUM_LEN];
} bt_device_t;

typedef struct {
    unsigned short year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
} bt_timestamp_t;

/*
 * A single device data entry.
 */
typedef struct {
    bt_timestamp_t timestamp;
    char value[MAX_RDB_BT_VAL_LEN];
} bt_device_data_t;

/*
 * Every sensor device must implement one of these functions. The function
 * needs to be added to the "sensor_reader" array at the index corresponding
 * to its bt_device_type_t enum value.
 */
typedef int (*sensor_read_fn_t)(bdaddr_t *src, bdaddr_t *dst,
                                bt_device_data_t *data, int max_data_len);

typedef int (*data_post_fn_t)(bt_device_t *device, bt_device_data_t *data,
                              int max_data_len);

/* RDB bluetooth variable paths and path creator macros */
#define RDB_BT_PREFIX "bluetooth"
#define RDB_BT_DEV_PREFIX RDB_BT_PREFIX ".device"

#define RDB_BT_DEV_NAME(var, len, dev_idx) \
    (rdb_construct_dev_var(var, len, "name", dev_idx))
#define RDB_BT_DEV_ADDR(var, len, dev_idx) \
    (rdb_construct_dev_var(var, len, "addr", dev_idx))
#define RDB_BT_DEV_TYPE(var, len, dev_idx) \
    (rdb_construct_dev_var(var, len, "type", dev_idx))
#define RDB_BT_DEV_SERIAL_NUM(var, len, dev_idx)                  \
    (rdb_construct_dev_var(var, len, "serial_num", dev_idx))

#define RDB_BT_DATA_START_IDX(var, len, dev_idx)            \
    (rdb_construct_data_var(var, len, "start_idx", dev_idx, \
                            RDB_DATA_GLOBAL_IDX))
#define RDB_BT_DATA_COUNT(var, len, dev_idx) \
    (rdb_construct_data_var(var, len, "count", dev_idx, \
                            RDB_DATA_GLOBAL_IDX))
#define RDB_BT_DATA_MAX_COUNT(var, len, dev_idx) \
    (rdb_construct_data_var(var, len, "max_count", dev_idx, \
                            RDB_DATA_GLOBAL_IDX))
#define RDB_BT_DATA_TIME(var, len, dev_idx, data_idx) \
    (rdb_construct_data_var(var, len, "timestamp", dev_idx, data_idx))
#define RDB_BT_DATA_VAL(var, len, dev_idx, data_idx)                    \
    (rdb_construct_data_var(var, len, "value", dev_idx, data_idx))

extern int rfcomm_bind_dev(int sock, int dev, uint32_t flags, bdaddr_t *src,
                           bdaddr_t *dst, int channel);

extern int set_interface_attribs(int fd, int speed, int parity);

extern int hpoint_data_post(bt_device_t *device, bt_device_data_t *data,
                            int num_data);

#endif /* __BT_SENSORD_H__ */
