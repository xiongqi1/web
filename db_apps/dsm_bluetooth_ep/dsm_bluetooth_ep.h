#ifndef DSM_BLUETOOTH_EP_H_11200923022016
#define DSM_BLUETOOTH_EP_H_11200923022016
/*
 * Data Stream Bluetooth Endpoint main header.
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

#include "../dsm_tool/dsm_tool.h"
#include <stdio.h>
#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <rdb_ops.h>

/* DBus bus, paths and interfaces. */
#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_PROFILEMGR_PATH "/org/bluez"
#define BLUEZ_PROFILEMGR_INTF "org.bluez.ProfileManager1"
#define BLUEZ_PROFILE_PATH "/org/bluez/Profile1"
#define BLUEZ_DEVICE_INTF "org.bluez.Device1"
#define BLUEZ_GATT_SERVICE_INTF "org.bluez.GattService1"
#define BLUEZ_GATT_CHARACTERISTIC_INTF "org.bluez.GattCharacteristic1"
#define BLUEZ_GATT_DESCRIPTOR_INTF "org.bluez.GattDescriptor1"

#define DBUS_OBJ_MGR_INTF "org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES_INTF "org.freedesktop.DBus.Properties"

#define BT_DEV_RDB_ROOT "bluetooth.device"
#define GATT_RPC_SERVICE_NAME "gatt.rpc"

/* Buffer for debug purpose */
#define DSM_BTEP_DBG_BUF_LEN 512

/* Generic buffer length */
#define DSM_BTEP_BUF_LEN 128

/* Various bluetooth buffer lengths */
#define DBUS_PATH_BUF_LEN DSM_BTEP_BUF_LEN
#define BT_ADDR_BUF_LEN 18 /* xx:xx:xx:xx:xx:xx 12 + 5 + 1 */
#define BT_NAME_BUF_LEN DSM_BTEP_BUF_LEN

/* Buffer length for UUID: 8-4-4-4-12 */
#define DSM_BTEP_UUID_BUF_LEN 37

/* Number of arguments to invoke dsm_bluetooth_ep */
#define DSM_BTEP_NUM_ARGS 4

/* Forward declarations. */
/* Bluetooth EP main context */
typedef struct dsm_btep_context_ dsm_btep_context_t;

/* GATT characteristic, service and device */
typedef struct dsm_gatt_characteristic_ dsm_gatt_characteristic_t;
typedef struct dsm_gatt_service_ dsm_gatt_service_t;
typedef struct dsm_btep_device_ dsm_btep_device_t;

/* Bluetooth EP match rule function */
typedef bool (*dsm_btep_rule_fn_t) (const char *input_val,
                                    const char *rule_val,
                                    bool negate);

typedef struct dsm_bt_endpoint_ {
    char *uuid;
    dsm_btep_rule_fn_t rule_func;
    bool rule_negate;
    unsigned int rule_property;
    char *rule_val;
    unsigned int conn_fail_retry;
    unsigned int conn_success_retry;
} dsm_bt_endpoint_t;

#define EXEC_NAME_MAX_LEN 128
typedef struct dsm_exec_endpoint_ {
    char exec_name[EXEC_NAME_MAX_LEN];
} dsm_exec_endpoint_t;

typedef struct dsm_btep_stream_ {
    struct dsm_btep_stream_ *next;
    t_end_point_types bt_ep_type;
    dsm_bt_endpoint_t bt_ep;
    t_end_point_types other_ep_type;
    union {
        dsm_exec_endpoint_t exec_ep;
    } other_ep;
} dsm_btep_stream_t;

/* GATT related struct */
typedef struct dsm_gatt_descriptor_ {
    struct dsm_gatt_descriptor_ *next;
    dsm_gatt_characteristic_t *parent;
    char *dbus_path;
    int handle;
    char *uuid;
    char *value;
    int value_len;
    int value_capacity;
    unsigned int properties;
    unsigned int index;
} dsm_gatt_descriptor_t;

typedef struct dsm_gatt_characteristic_ {
    struct dsm_gatt_characteristic_ *next;
    dsm_gatt_service_t *parent;
    char *dbus_path;
    int handle;
    char *uuid;
    char *value;
    int value_len;
    int value_capacity;
    gboolean notifying;
    unsigned int properties;
    unsigned int index;

    guint property_watch_id;
    dsm_gatt_descriptor_t *descriptors;
    unsigned int next_desc_index; /* ascending for assigning descriptors */
} dsm_gatt_characteristic_t;

typedef struct dsm_gatt_service_ {
    struct dsm_gatt_service_ *next;
    dsm_btep_device_t *parent;
    char *dbus_path;
    int handle;
    char *uuid;
    unsigned int index;

    dsm_gatt_characteristic_t *characteristics;
    unsigned int next_char_index; /* ascending for assigning characteristics */
} dsm_gatt_service_t;

/*
 * These are the device's connection action states/stages, not the device's
 * connection status, which is the responsibility of device->connected).
 * There is no one-to-one relationship between state and connected.
 * e.g. a connected device might be in IDLE state because a
 * Connect/ConnectProfile has not been initiated.
 */
typedef enum dsm_btep_device_state_ {
    DSM_BTEP_STATE_IDLE,
    DSM_BTEP_STATE_CONNECTING,
    DSM_BTEP_STATE_PRECONNECTED,
    DSM_BTEP_STATE_CONNECTED,
} dsm_btep_device_state_t;

/* Device properties of interest. c.f. org.bluez.Device1 */
#define DSM_BTEP_DEV_PROP_ADDRESS "Address"
#define DSM_BTEP_DEV_PROP_NAME "Name"
#define DSM_BTEP_DEV_PROP_CONNECTED "Connected"
#define DSM_BTEP_DEV_PROP_PAIRED "Paired"

/*
 * GATT characteristic properties of interest.
 * cf. org.bluez.GattCharacteristic1
 */
#define GATT_CHAR_PROP_VALUE "Value"

typedef struct dsm_btep_device_ {
    struct dsm_btep_device_ *next;

    /* DBUS fields. */
    char dbus_path[DBUS_PATH_BUF_LEN];
    guint property_watch_id;

    /* Property fields. */
    char address[BT_ADDR_BUF_LEN];
    char name[BT_NAME_BUF_LEN];
    gboolean connected;
    gboolean paired;

    /* Main context */
    dsm_btep_context_t *ctx;

    /* Connection action states for classic and ble devices */
    dsm_btep_device_state_t classic_state, ble_state;

    /*
     * Index of the device in ctx->devices.
     * It does not necessary reflect the order of device in ctx->devices.
     * It is used in mapping the device to RDB entries.
     * If a device has never been matched to any streams, index = -1.
     * On the first time the device matches any stream, it is assigned a
     * non-negative unique number, and the number will never change.
     * Even if later on it no longer matches any streams, e.g. due to device's
     * properties change, the assigned index will stay.
     */
    int index;

    /* GATT fields */
    dsm_gatt_service_t *services;
    unsigned int next_service_index; /* ascending for assigning services */

    int fd;
} dsm_btep_device_t;

/* Stream-Device association */
typedef struct dsm_btep_association_ {
    struct dsm_btep_association_ *next;
    dsm_btep_context_t *ctx;
    dsm_btep_stream_t *stream;
    dsm_btep_device_t *device;
    guint conn_timer; /* connection timer per association */
} dsm_btep_association_t;

typedef struct dsm_btep_context_ {

    /* DBUS related fields. */
    GMainLoop *loop;
    GDBusConnection *connection;
    GDBusNodeInfo *profile_bus_node_info;
    guint bus_watch_id;
    guint iface_added_watch_id;
    guint iface_removed_watch_id;
    guint profile_registration_id;

    /* streams, devices and associations */
    dsm_btep_stream_t *streams;
    dsm_btep_device_t *devices;
    unsigned int next_device_index; /* ascending only for assigning devices */
    dsm_btep_association_t *associations;

    /* Other fields. */
    int exit_status;
    struct rdb_session *rdb_s;

} dsm_btep_context_t;

typedef enum dsm_btep_operator_ {
    OPERATOR_IS          = 0,
    OPERATOR_CONTAINS    = 1,
    OPERATOR_STARTS_WITH = 2,
    OPERATOR_ENDS_WITH   = 3,

    OPERATOR_INVALID
} dsm_btep_operator_t;

typedef enum dsm_btep_property_ {
    PROPERTY_NAME    = 0,
    PROPERTY_ADDRESS = 1,

    PROPERTY_INVALID
} dsm_btep_property_t;

#define BT_EP_FAIL_RETRY_MIN 1
#define BT_EP_FAIL_RETRY_MAX 3600
#define BT_EP_SUCCESS_RETRY_MIN 1
#define BT_EP_SUCCESS_RETRY_MAX 604800

#define SPP_UUID "spp"

#ifdef DEBUG
extern char dbg_buf[DSM_BTEP_DBG_BUF_LEN];
#endif

#define bail(ctx, status, msg_fmt, ...) do {        \
        ctx->exit_status = status;                  \
        g_main_loop_quit(ctx->loop);                \
        errp(msg_fmt, ##__VA_ARGS__);               \
        return;                                     \
    } while (0)


/* External functions */

/* Context getter */
extern dsm_btep_context_t *dsm_btep_get_context(void);

/* Device and stream related functions */
extern dsm_btep_device_t *device_find(dsm_btep_context_t *ctx,
                                      const char *dbus_path);

extern dsm_btep_device_t *device_remove(dsm_btep_context_t *ctx,
                                        const char *dbus_path);

extern bool device_match_all_streams(dsm_btep_context_t *ctx,
                                     dsm_btep_device_t *device);

extern void stream_destroy(dsm_btep_stream_t *stream);

extern void device_destroy(dsm_btep_device_t *device);

extern void dsm_btep_match_all_devices_with_stream(dsm_btep_context_t *ctx,
                                                   dsm_btep_stream_t *stream);

extern dsm_btep_stream_t *stream_find(dsm_btep_context_t *ctx,
                                      dsm_btep_stream_t *stream);

extern int construct_stream(struct rdb_session *rdb_s, char *argv[],
                            dsm_btep_stream_t **ppstream);

extern void delete_device_rdb_tree(dsm_btep_context_t *ctx,
                                   dsm_btep_device_t *device);

/* PropertiesChanged callbacks */
extern void dev_properties_changed(GDBusConnection *connection,
                                   const gchar *sender_name,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *signal_name,
                                   GVariant *parameters,
                                   gpointer user_data);

extern void char_properties_changed(GDBusConnection *connection,
                                    const gchar *sender_name,
                                    const gchar *object_path,
                                    const gchar *interface_name,
                                    const gchar *signal_name,
                                    GVariant *parameters,
                                     gpointer user_data);

/* bluez object parsers */
extern void parse_bluez_single_object(dsm_btep_context_t *ctx,
                                      const char *object_path,
                                      GVariant *ifaces_and_props);

extern void parse_bluez_objects(dsm_btep_context_t *ctx);

/* timer handler */
extern gboolean connect_timer_handler(gpointer user_data);

/* formatting functions */
extern int snprint_stream(char *buf, size_t len,
                          const dsm_btep_stream_t * stream);

extern int snprint_device(char *buf, size_t len,
                          const dsm_btep_device_t * device);

extern int snprint_association(char *buf, size_t len,
                               const dsm_btep_association_t * association);

/* GATT related functions */
extern int update_char_val(dsm_gatt_characteristic_t *characteristic,
                           GVariant *value);
extern int update_desc_val(dsm_gatt_descriptor_t *descriptor, GVariant *value);

extern dsm_gatt_characteristic_t *
characteristic_find_by_idx(dsm_btep_context_t *ctx, int dev_idx, int srv_idx,
                           int char_idx);

extern dsm_gatt_descriptor_t *
descriptor_find_by_idx(dsm_btep_context_t *ctx, int dev_idx, int srv_idx,
                       int char_idx, int desc_idx);

extern gboolean
attribute_find_by_handle(dsm_btep_context_t *ctx, int dev_idx, int handle,
                         dsm_gatt_characteristic_t **pchar,
                         dsm_gatt_descriptor_t **pdesc);

#endif /* DSM_BLUETOOTH_EP_H_11200923022016 */
