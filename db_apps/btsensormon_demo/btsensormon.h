/*
 * Bluetooth sensor monitor (btsensormon) definitions.
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

#ifndef BTSENSORMON_H_120000100216
#define BTSENSORMON_H_120000100216

#include <stdio.h>
#include <stdbool.h>
#include <syslog.h>
#include <glib.h>
#include <gio/gio.h>
#include <rdb_ops.h>

/* DBus bus, paths and interfaces. */
#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_PROFILEMGR_PATH "/org/bluez"
#define BLUEZ_PROFILEMGR_INTF "org.bluez.ProfileManager1"
#define BLUEZ_PROFILE_PATH "/org/bluez/Profile1"
#define BLUEZ_DEVICE_INTF "org.bluez.Device1"
#define BLUEZ_GATT_CHARACTERISTIC_INTF "org.bluez.GattCharacteristic1"
#define DBUS_OBJ_MGR_INTF "org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES_INTF "org.freedesktop.DBus.Properties"

#define RDB_VAR_BT_SENSOR_PREFIX "bluetooth.sensor"
#define RDB_VAR_BT_SENSOR_NUM_DEV RDB_VAR_BT_SENSOR_PREFIX".num_dev"
#define RDB_VAR_BT_SENSOR_SMS RDB_VAR_BT_SENSOR_PREFIX".sms"
#define RDB_VAR_BT_SENSOR_DEV RDB_VAR_BT_SENSOR_PREFIX".%d"
#define RDB_VAR_BT_SENSOR_POLL_SEC RDB_VAR_BT_SENSOR_PREFIX".poll_sec"
#define RDB_VAR_BT_SENSOR_DATA RDB_VAR_BT_SENSOR_DEV".data.%d"

#define RDB_VAR_BT_SENSOR_DEV_ADDR "address"
#define RDB_VAR_BT_SENSOR_DEV_DESCRIPTION "description"
#define RDB_VAR_BT_SENSOR_DEV_TYPE "type"
#define RDB_VAR_BT_SENSOR_DEV_DATA_COUNT "data.count"
#define RDB_VAR_BT_SENSOR_DEV_DATA_START_IDX "data.start_idx"

#define RDB_VAR_BT_SENSOR_DATA_VALUE "value"
#define RDB_VAR_BT_SENSOR_DATA_TIME "time"

#define MK_SENSOR_DEV_VAR(buf, len, sensor_idx, var_name) \
    snprintf(buf, len, RDB_VAR_BT_SENSOR_DEV".%s", sensor_idx, var_name);

#define MK_SENSOR_DATA_VAR(buf, len, sensor_idx, data_idx, data_name) \
    snprintf(buf, len, RDB_VAR_BT_SENSOR_DATA".%s", sensor_idx, \
             data_idx, data_name);

#define SPP_UUID "spp"

/* Seconds between connection attempts. */
#define CONNECT_PERIOD_SEC 10

#define RDB_CREATE_FLAGS PERSIST

/* Forward declarations. */
typedef struct btsm_client_ btsm_client_t;
typedef struct btsm_context_ btsm_context_t;

typedef enum btsm_device_state_ {
    BTSM_STATE_IDLE,
    BTSM_STATE_CONNECTING,
    BTSM_STATE_CONNECTED,
    BTSM_STATE_COLLECTED,
} btsm_device_state_t;

/*
 * Valid sensor devices.
 */
typedef enum {
    DEV_TYPE_UNKNOWN = 0,
    DEV_TYPE_TD3128_BPMETER,
    DEV_TYPE_TD1261_THERMOMETER,
    DEV_TYPE_NN3350_PULSE_OXIMETER,
    DEV_TYPE_MAX,
} btsm_device_type_t;

typedef struct btsm_device_ {
    struct btsm_device_ *next;

    /* DBUS fields. */
    char *dbus_path;
    guint property_watch_id;

    /* Property fields. */
    char *address;
    char *name;
    gboolean connected;
    gboolean paired;

    /* Fields set by each client */
    const char *description;
    btsm_device_type_t type;
    const char *profile_uuid;

    /* Other fields. */
    btsm_context_t *ctx;
    btsm_client_t *client;
    btsm_device_state_t state;
    unsigned int index;
    int fd;
} btsm_device_t;

#define MAX_RDB_BT_VAL_LEN 32
#define MAX_NUM_DATA 10

typedef struct {
    unsigned short year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
} btsm_timestamp_t;

/*
 * A single device data entry.
 */
typedef struct {
    btsm_timestamp_t ts;
    char value[MAX_RDB_BT_VAL_LEN];
} btsm_device_data_t;

typedef void (*btsm_process_data_fn)(btsm_device_t *dev,
                                     btsm_device_data_t *data,
                                     unsigned int max_data);

typedef struct btsm_client_ {
    bool (*accept)(btsm_device_t *device);
    int (*collect_poll)(btsm_device_t *device, btsm_device_data_t *data,
                        unsigned int max_data,
                        btsm_process_data_fn process_fn);
    int (*collect_event)(btsm_device_t *device, btsm_device_data_t *data,
                         unsigned int max_data);
} btsm_client_t;

typedef struct btsm_context_ {

    /* DBUS related fields. */
    GMainLoop *loop;
    GDBusConnection *connection;
    GDBusNodeInfo *profile_bus_node_info;
    guint bus_watch_id;
    guint interface_watch_id;

    /* Device related fields. */
    btsm_device_t *devices;
    btsm_client_t **clients;
    unsigned int num_devices;

    /* Other fields. */
    int exit_status;
    guint connect_timer;
    struct rdb_session *rdb_s;
} btsm_context_t;

#ifdef DEBUG
#define dbgp(x, ...) printf(x, ##__VA_ARGS__)
#define errp(x, ...) printf("Error: "x, ##__VA_ARGS__)
#else
#define dbgp(x, ...) syslog(LOG_DEBUG, x, ##__VA_ARGS__)
#define errp(x, ...) syslog(LOG_ERR, x, ##__VA_ARGS__)
#endif

#define bail(ctx, status, msg_fmt, ...) do {        \
        ctx->exit_status = status;                  \
        g_main_loop_quit(ctx->loop);                \
        errp(msg_fmt, ##__VA_ARGS__);               \
        return;                                     \
    } while (0);

void parse_bluez_objects(btsm_context_t *ctx,
                         GDBusSignalCallback dev_prop_change_cb);
void parse_bluez_single_object(btsm_context_t *ctx, char *object_path,
                               GVariant *ifaces_and_props,
                               GDBusSignalCallback dev_prop_change_cb);
btsm_device_t *device_find(btsm_context_t *ctx, btsm_device_t *target);
void device_update(btsm_device_t *device, GVariantIter *prop_iterator);

#endif /* BTSENSORMON_H_120000100216 */
