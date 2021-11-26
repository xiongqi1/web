/*
 * dsm_bluetooth_ep.c
 *    Data Stream Bluetooth Endpoint daemon.
 *    This module deals with most of Bluez/D-Bus stuff.
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

#include "dsm_bluetooth_ep.h"
#include "dsm_btep_rpc.h"
#include "dsm_gatt_rpc.h"
#include "dsm_bt_utils.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>

/* Forward declarations */
static void connect_return_handler(GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer user_data);

static void connect_profile_return_handler(GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data);

static int launch_ude(dsm_btep_association_t *assoc);

static int launch_ude_ble(dsm_btep_association_t *assoc);

/* The context that carries the key variables */
static dsm_btep_context_t *ctx;

/* Getter for the context */
dsm_btep_context_t *
dsm_btep_get_context (void)
{
    return ctx;
}

/* debug buffer for printing detailed info */
#ifdef DEBUG
char dbg_buf[DSM_BTEP_DBG_BUF_LEN];
#endif

/* Introspection data for the bluez Profile service. */
static const gchar profile_introspection_xml_g[] =
  "<node>"
  "  <interface name='org.bluez.Profile1'>"
  "    <method name='Release'>"
  "    </method>"
  "    <method name='NewConnection'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='h' name='fd' direction='in'/>"
  "      <arg type='a{sv}' name='fd_properties' direction='in'/>"
  "    </method>"
  "    <method name='RequestDisconnection'>"
  "      <arg type='o' name='device' direction='in'/>"
  "    </method>"
  "    <method name='Cancel'>"
  "    </method>"
  "  </interface>"
  "</node>";

static void
turn_assoc_timers(dsm_btep_device_t *device, bool on_off)
{
    dsm_btep_association_t *assoc;
    dsm_btep_context_t *ctx = device->ctx;
    for (assoc = ctx->associations; assoc; assoc = assoc->next) {
        if (assoc->device != device) {
            continue;
        }
        if (on_off) {
            if (!assoc->conn_timer) {
                assoc->conn_timer =
                    g_timeout_add_seconds(assoc->stream->bt_ep.conn_fail_retry,
                                          connect_timer_handler, assoc);
            }
        } else {
            if (assoc->conn_timer) {
                g_source_remove(assoc->conn_timer);
                assoc->conn_timer = 0;
            }
        }
    }
}

/*
 * Callback function for the device PropertiesChanged dbus event.
 * This handles the following properites changes:
 * 1) device name
 * 2) device address
 * 3) paired status
 * 4) connected status
 */
void
dev_properties_changed (GDBusConnection *connection,
                        const gchar *sender_name,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *signal_name,
                        GVariant *parameters,
                        gpointer user_data)
{
    char *iface_name;
    char *prop_name;
    GVariant *prop_val;
    GVariantIter *prop_iterator = NULL;
    dsm_btep_device_t *dev = (dsm_btep_device_t *)user_data;
    dsm_btep_context_t *ctx = dev->ctx;
    dsm_btep_association_t *assoc;

    gboolean name_changed = FALSE;

    dbgp("object %s property changed\n", object_path);

    /*
     * Loop through the changed properties and save those of interest into
     * the device structure.
     */
    g_variant_get(parameters, "(&sa{sv}as)", &iface_name, &prop_iterator, NULL);

    /* Only care about changes to device properties */
    if (!strcmp(iface_name, BLUEZ_DEVICE_INTF)) {
        while (g_variant_iter_loop(prop_iterator, "{sv}", &prop_name,
                                   &prop_val)) {
            if (!strcmp(prop_name, DSM_BTEP_DEV_PROP_NAME)) {
                strncpy(dev->name, g_variant_get_string(prop_val, NULL),
                        sizeof(dev->name));
                dev->name[sizeof(dev->name) - 1] = '\0';
                name_changed = TRUE;
                dbgp("Prop: %s=%s\n", prop_name, dev->name);
            } else if (!strcmp(prop_name, DSM_BTEP_DEV_PROP_CONNECTED)) {
                dev->connected = g_variant_get_boolean(prop_val);
                dbgp("Prop: %s=%d\n", prop_name, dev->connected);

                if (!dev->connected) {
                    dev->classic_state = DSM_BTEP_STATE_IDLE;
                    dev->ble_state = DSM_BTEP_STATE_IDLE;
                    dev->fd = -1;
                } else if (dev->ble_state == DSM_BTEP_STATE_PRECONNECTED) {
                    /*
                     * For ble devices, PRECONNECTED means ConnectReturn success
                     * but waiting for Connected=1. So it is time now to launch.
                     * For classic devices, there is no PRECONNECTED state, and
                     * Connected=1 does not mean fd is available. We have to
                     * wait for NewConnection callback to launch.
                     */
                    for (assoc = ctx->associations; assoc;
                         assoc = assoc->next) {
                        if (assoc->device == dev && !assoc->conn_timer &&
                            assoc->stream->bt_ep_type == EP_BT_GC) {
                            launch_ude_ble(assoc);
                        }
                    }
                    dev->ble_state = DSM_BTEP_STATE_CONNECTED;
                }

            } else if (!strcmp(prop_name, DSM_BTEP_DEV_PROP_PAIRED)) {
                dev->paired = g_variant_get_boolean(prop_val);
                dbgp("Prop: %s=%d\n", prop_name, dev->paired);
                if (dev->paired) {
                    /* start conn_timers for related associations */
                    turn_assoc_timers(dev, TRUE);
                } else {
                    /* stop conn_timers for related associations */
                    turn_assoc_timers(dev, FALSE);
                }
            }
        }

        /*
         * Update device-stream associations if name is changed.
         * Currently, this should never happen since bluez does not generate
         * the callback upon name changes. But the code is here in case
         * future bluez version implements that.
         */
        if (name_changed) {
            if (device_match_all_streams(ctx, dev) && dev->index < 0) {
                /* assign device index if it is matched for the first time */
                dev->index = ctx->next_device_index++;
            }
        }
    }

    if (prop_iterator) {
        g_variant_iter_free(prop_iterator);
    }
}

/*
 * Callback function for the characteristic PropertiesChanged dbus event.
 * This only handles characteristic value changes, which is the only subscribed
 * property on a characteristic.
 * Upon called, it updates both the in-memory characteristic in ctx and
 * the corresponding rdb variable.
 */
void
char_properties_changed (GDBusConnection *connection,
                         const gchar *sender_name,
                         const gchar *object_path,
                         const gchar *interface_name,
                         const gchar *signal_name,
                         GVariant *parameters,
                         gpointer user_data)
{
    char *iface_name;
    char *prop_name;
    GVariant *prop_val;
    GVariantIter *prop_iterator = NULL;
    char rdb_var_name[MAX_NAME_LENGTH];
    dsm_gatt_characteristic_t *characteristic =
        (dsm_gatt_characteristic_t *)user_data;
    dsm_gatt_service_t *service = characteristic->parent;
    dsm_btep_device_t *device = service->parent;

    dbgp("object %s property changed\n", object_path);

    /*
     * Loop through the changed properties and save those of interest into
     * the characteristic structure.
     */
    g_variant_get(parameters, "(&sa{sv}as)", &iface_name, &prop_iterator, NULL);

    /* Only care about changes to characteristic value */
    if (!strcmp(iface_name, BLUEZ_GATT_CHARACTERISTIC_INTF)) {
        while (g_variant_iter_loop(prop_iterator, "{sv}", &prop_name,
                                   &prop_val)) {
            if (!strcmp(prop_name, GATT_CHAR_PROP_VALUE)) {
                if (update_char_val(characteristic, prop_val)) {
                    errp("Failed to update characteristic value\n");
                } else {
                    snprintf(rdb_var_name, sizeof rdb_var_name,
                             "%s.%d.gatt.service.%d.char.%d.value",
                             BT_DEV_RDB_ROOT, device->index, service->index,
                             characteristic->index);
                    if (update_rdb_blob(device->ctx->rdb_s, rdb_var_name,
                                        characteristic->value,
                                        characteristic->value_len)) {
                        errp("Failed to update characteristic value rdb var\n");
                    }
                }
            }
        }
    }
    if (prop_iterator) {
        g_variant_iter_free(prop_iterator);
    }
}

/*
 * Callback function for the InterfacesAdded dbus event.
 * The following interfaces are of interest:
 * 1) device
 * 2) service
 * 3) characteristic
 * 4) descriptor
 * cf. parse_bluez_single_object() for detail.
 */
static void
interfaces_added (GDBusConnection *connection,
                  const gchar *sender_name,
                  const gchar *object_path,
                  const gchar *interface_name,
                  const gchar *signal_name,
                  GVariant *parameters,
                  gpointer user_data)
{
    GVariant *iface_and_props;
    char *dbus_path;
    dsm_btep_context_t *ctx = (dsm_btep_context_t *)user_data;
    g_variant_get(parameters, "(&o*)", &dbus_path, &iface_and_props);
    dbgp("Interface added %s at %s\n", interface_name, dbus_path);
    parse_bluez_single_object(ctx, dbus_path, iface_and_props);
    g_variant_unref(iface_and_props);
}

/*
 * Callback function for the InterfacesRemoved dbus event.
 * The only interface of interest is device, which will remove all related
 * services, characteristics and descriptors.
 */
static void
interfaces_removed (GDBusConnection *connection,
                    const gchar *sender_name,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *signal_name,
                    GVariant *parameters,
                    gpointer user_data)
{
    char *dbus_path;
    GVariantIter *iface_iterator = NULL;
    char *iface_name;
    dsm_btep_device_t *device;
    dsm_btep_context_t *ctx = (dsm_btep_context_t *)user_data;

    /*
     * Loop through all the removed interfaces. Only interested in Device
     * removals.
     */
    g_variant_get(parameters, "(&oas)", &dbus_path, &iface_iterator);
    dbgp("Interface removed %s at %s\n", interface_name, dbus_path);
    while (g_variant_iter_loop(iface_iterator, "s", &iface_name)) {
        if (!strcmp(iface_name, BLUEZ_DEVICE_INTF)) {
            device = device_remove(ctx, dbus_path);
            if (device) {
                dbgp("Remove device (%s, %s) idx %d\n", device->address,
                     device->name, device->index);
                device_destroy(device);
            }
            break; /* there should be no more than one BLUEZ_DEVICE_INTF */
        }
    }

    g_variant_iter_free(iface_iterator);
}

/*
 * This daemon implements two independent finite state machines (FSMs),
 * one for classic device and the other for BLE device.
 * The states are for devices, which are presented in device->classic_state &
 * device->ble_state. They represent stages of device connection actions, and
 * do not necessary reflect devices' connection status (which is represented
 * by device->connected).
 *
 * Four states are defined: IDLE, CONNECTING, PRECONNECTED, CONNECTED.
 * PRECONNECTED is specific to BLE devices, while others are common for both.
 *
 * Four types of events drive the state transitions:
 * 1) connect timer ==> connect_timer_handler()
 * 2) asynchronous connection call return ==> connect_profile_return_handler()
 *    (for classic) & connect_return_handler() (for BLE)
 * 3) device's Connected property changes ==> dev_properties_changed()
 * 4) NewConnection profile callback ==> profile_handle_method_call().
 *    This is for classic devices only.
 *
 * A normal state change cycle for BLE may appear like this:
 *
 *       timeout/Connect
 * IDLE -----------------> CONNECTING -----------------------> PRECONNECTED
 *  ^   <----------------      |       connect return success     |
 *  |     connect return       \                                  | Connected=1
 *  |     failure               \ connect return success &        | ----------
 *  |                            \ Connected=1                    | Launch UDEs
 *  |                             \-------------------------->    V
 *  |<------------------------------------------------------- CONNECTED
 *                    Connected=0
 */

static int
connect_return_helper (dsm_btep_device_t *device, GAsyncResult *res, bool ble)
{
    dsm_btep_context_t *ctx = device->ctx;
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_finish(ctx->connection, res, &error);

    /*
     * Redflag:
     * Sometimes, a "software caused connection abort" error occurs,
     * and connection will never succeed after that.
     * The issue is likely in bluez stack implementation.
     * To be further investigated.
     */

    /* Connect(Profile) should succeed even if device is already connected */
    if (error) {
        dbgp("Failed to connect to device %s: %s \n", device->address,
             error->message);
        if (ble) {
            device->ble_state = DSM_BTEP_STATE_IDLE;
        } else {
            device->classic_state = DSM_BTEP_STATE_IDLE;
        }
        if (device->paired) {
            turn_assoc_timers(device, TRUE);
        }
    }

    if (result) {
        g_variant_unref(result);
    }

    return error ? -1 : 0;
}

/*
 * Async method complete handler for the ConnectProfile dbus invocation.
 * For classic devices.
 * Note that a successful invocation does not mean that the connection to the
 * remote device has completed. It just means the invocation to the dbus has
 * succeeded. The org.bluez.Profile1.NewConnection callback will be invoked
 * when the connection is successful.
 * It is observed that NewConnection callback might arrive earlier than this
 * return handler gets called. So the implementation should not rely on any
 * specific order of the two events.
 */
static void
connect_profile_return_handler (GObject *source_object, GAsyncResult *res,
                                gpointer user_data)
{
    dsm_btep_device_t *device = (dsm_btep_device_t *)user_data;

    dbgp("connect profile return handler for device (%s, %s)\n",
         device->address, device->name);
    connect_return_helper(device, res, FALSE);
}

/*
 * Async method complete handler for the Connect dbus invocation.
 * For BLE device.
 * Note that a successful invocation does not mean that the connection to the
 * remote device has completed. It just means the invocation to the dbus has
 * succeeded. The PropertiesChanged signal will be received upon a successful
 * connection.
 * It is observed that the connected property change might come earlier than
 * this connect return callback, AND the connection is only reliable after
 * both events have happened. That is why we introduced a PRECONNECTED state
 * for BLE devices, which represents a state where connect return succeeds but
 * the connected property is still FALSE.
 */
static void
connect_return_handler (GObject *source_object, GAsyncResult *res,
                        gpointer user_data)
{
    dsm_btep_device_t *device = (dsm_btep_device_t *)user_data;
    dsm_btep_context_t *ctx = device->ctx;
    dsm_btep_association_t *assoc;

    dbgp("connect return handler for BLE device (%s, %s)\n",
         device->address, device->name);

    if (connect_return_helper(device, res, TRUE)) {
        return;
    }

    /* Upon this point, the connect return is successful */

    /* device->connected will affect what could happen next */
    if (device->ble_state == DSM_BTEP_STATE_CONNECTING) {
        if (device->connected) {
            /*
             * Redflag:
             * Sometimes, bluez incorrectly reports a wrong connected status.
             * The property "Connected" of BLUEZ_DEVICE_INTF is TRUE but
             * the device is actually turned off.
             * This has to be further investigated (likely in bluetoothd).
             */

            /*
             * If already connected, there would be no PropertiesChanged
             * callbacks. So launch the UDEs now.
             */
            for (assoc = ctx->associations; assoc; assoc = assoc->next) {
                if (device == assoc->device && !assoc->conn_timer) {
                    if (assoc->stream->bt_ep_type == EP_BT_GC) {
                        launch_ude_ble(assoc);
                    }
                }
            }
            device->ble_state = DSM_BTEP_STATE_CONNECTED;
        } else {
            /* We have to wait for connected property change */
            device->ble_state = DSM_BTEP_STATE_PRECONNECTED;
        }
    }
}

/*
 * Launch UDE for the classic device in assoc.
 * If ude is launched successfully, a conn_success_retry timer will be started.
 * Otherwise, a conn_fail_retry timer will be started.
 * Note, the new timer will be saved in assoc->conn_timer. So the caller should
 * backup any existing timer if necessary.
 */
static int
launch_ude (dsm_btep_association_t *assoc)
{
    pid_t pid;
    int rval = 0;
    dsm_btep_device_t *device = assoc->device;

    pid = fork();
    if (pid == 0) {
        /* Child. */
        rval = dup2(device->fd, STDIN_FILENO);
        if (rval == -1) {
            errp("dup2 stdin");
            exit(-1);
        }
        rval = dup2(device->fd, STDOUT_FILENO);
        if (rval == -1) {
            errp("dup2 stdout");
            exit(-1);
        }
        dbgp("Launch UDE %s\n", assoc->stream->other_ep.exec_ep.exec_name);
        rval = execlp(assoc->stream->other_ep.exec_ep.exec_name,
                      assoc->stream->other_ep.exec_ep.exec_name,
                      device->address, NULL);
        /* we should not get here unless error */
        errp("Failed to call excelp");
        exit(rval);
    } else if (pid > 0) {
        /* Parent */
        if (device->paired) {
            assoc->conn_timer = g_timeout_add_seconds(
                assoc->stream->bt_ep.conn_success_retry,
                connect_timer_handler, assoc);
        }
    } else {
        errp("Failed to fork\n");
        rval = -1;
        if (device->paired) {
            assoc->conn_timer = g_timeout_add_seconds(
                assoc->stream->bt_ep.conn_fail_retry,
                connect_timer_handler, assoc);
        }
    }
    if (!assoc->conn_timer && device->paired) {
        errp("Failed to start conn_timer\n");
        rval = -1;
    }

    return rval;
}

#define INVOKE_CHK_EXIST(invocation, err_str_fmt, ...) do {             \
        int rval = (invocation);                                        \
        if (rval && rval != -EEXIST) {                                  \
            if (err_str_fmt) {                                          \
                errp(err_str_fmt, ##__VA_ARGS__);                       \
            }                                                           \
            return rval;                                                \
        }                                                               \
    } while(0)

#define DESC_RDB_FMT_PREFIX "%s.%d.gatt.service.%d.char.%d.desc.%d"
/*
 * Create descriptor rdb tree. Only non-existing entries will be created,
 * while existing variables will not be updated.
 */
static int
create_descriptor_rdb_tree (dsm_btep_context_t *ctx,
                            dsm_btep_device_t *device,
                            dsm_gatt_service_t *service,
                            dsm_gatt_characteristic_t *characteristic,
                            dsm_gatt_descriptor_t *descriptor)
{
    char rdb_var_name[MAX_NAME_LENGTH];

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".handle",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    INVOKE_CHK_EXIST(create_rdb_int(ctx->rdb_s, rdb_var_name,
                                    descriptor->handle, "%d"),
                     "Failed to create rdb var - descriptor handle\n");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".uuid",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    INVOKE_CHK_EXIST(rdb_create_string(ctx->rdb_s, rdb_var_name,
                                       descriptor->uuid, 0, 0),
                     "Failed to create rdb var - descriptor uuid\n");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".value",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    INVOKE_CHK_EXIST(create_rdb_blob(ctx->rdb_s, rdb_var_name,
                                     descriptor->value, descriptor->value_len),
                     "Failed to create rdb var - descriptor value\n");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".properties",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    INVOKE_CHK_EXIST(create_rdb_int(ctx->rdb_s, rdb_var_name,
                                    descriptor->properties, "%d"),
                     "Failed to create rdb var - descriptor properties\n");

    return 0;
}

/* Delete the rdb tree associated with the given descriptor. */
static void
delete_descriptor_rdb_tree (dsm_btep_context_t *ctx,
                            dsm_btep_device_t *device,
                            dsm_gatt_service_t *service,
                            dsm_gatt_characteristic_t *characteristic,
                            dsm_gatt_descriptor_t *descriptor)
{
    char rdb_var_name[MAX_NAME_LENGTH];

    if (device->index < 0) {
        return;
    }

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".handle",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".uuid",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".value",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             DESC_RDB_FMT_PREFIX ".properties",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index, descriptor->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);
}

#define CHAR_RDB_FMT_PREFIX "%s.%d.gatt.service.%d.char.%d"
/*
 * Create characteristic rdb tree. Only non-existing entries will be created,
 * while existing variables will not be updated.
 */
static int
create_characteristic_rdb_tree (dsm_btep_context_t *ctx,
                                dsm_btep_device_t *device,
                                dsm_gatt_service_t *service,
                                dsm_gatt_characteristic_t *characteristic)
{
    char rdb_var_name[MAX_NAME_LENGTH];
    dsm_gatt_descriptor_t *descriptor;

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".handle",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    INVOKE_CHK_EXIST(create_rdb_int(ctx->rdb_s, rdb_var_name,
                                    characteristic->handle, "%d"),
                     "Failed to create rdb var - characteristic handle\n");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".uuid", BT_DEV_RDB_ROOT,
             device->index, service->index, characteristic->index);
    INVOKE_CHK_EXIST(rdb_create_string(ctx->rdb_s, rdb_var_name,
                                       characteristic->uuid, 0, 0),
                     "Failed to create rdb var - characteristic uuid\n");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".value", BT_DEV_RDB_ROOT,
             device->index, service->index, characteristic->index);
    INVOKE_CHK_EXIST(create_rdb_blob(ctx->rdb_s, rdb_var_name,
                                     characteristic->value,
                                     characteristic->value_len),
                     "Failed to create rdb var - characteristic value\n");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".notifying",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    INVOKE_CHK_EXIST(create_rdb_int(ctx->rdb_s, rdb_var_name,
                                    characteristic->notifying, "%d"),
                     "Failed to create rdb var - characteristic notifying");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".properties",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    INVOKE_CHK_EXIST(create_rdb_int(ctx->rdb_s, rdb_var_name,
                                    characteristic->properties, "%d"),
                     "Failed to create rdb var - characteristic properties");

    for (descriptor = characteristic->descriptors; descriptor;
         descriptor = descriptor->next) {
        INVOKE_CHK(create_descriptor_rdb_tree(ctx, device, service,
                                              characteristic, descriptor),
                   "Failed to create descriptor rdb tree");
    }
    return 0;
}

/* Delete the rdb tree associated with the given characteristic. */
static void
delete_characteristic_rdb_tree (dsm_btep_context_t *ctx,
                                dsm_btep_device_t *device,
                                dsm_gatt_service_t *service,
                                dsm_gatt_characteristic_t *characteristic)
{
    char rdb_var_name[MAX_NAME_LENGTH];
    dsm_gatt_descriptor_t *descriptor;

    if (device->index < 0) {
        return;
    }

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".handle",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".uuid",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".value",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".notifying",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             CHAR_RDB_FMT_PREFIX ".properties",
             BT_DEV_RDB_ROOT, device->index, service->index,
             characteristic->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    for (descriptor = characteristic->descriptors; descriptor;
         descriptor = descriptor->next) {
        delete_descriptor_rdb_tree(ctx, device, service, characteristic,
                                   descriptor);
    }
}

#define SVC_RDB_FMT_ROOT "%s.%d.gatt.service.%d"
/*
 * Create service rdb tree. Only non-existing entries will be created,
 * while existing variables will not be updated.
 */
static int
create_service_rdb_tree (dsm_btep_context_t *ctx,
                         dsm_btep_device_t *device,
                         dsm_gatt_service_t *service)
{
    char rdb_var_name[MAX_NAME_LENGTH];
    dsm_gatt_characteristic_t *characteristic;

    snprintf(rdb_var_name, sizeof rdb_var_name,
             SVC_RDB_FMT_ROOT ".handle", BT_DEV_RDB_ROOT,
             device->index, service->index);
    INVOKE_CHK_EXIST(create_rdb_int(ctx->rdb_s, rdb_var_name,
                                    service->handle, "%d"),
                     "Failed to create rdb var - service handle\n");

    snprintf(rdb_var_name, sizeof rdb_var_name,
             SVC_RDB_FMT_ROOT ".uuid", BT_DEV_RDB_ROOT,
             device->index, service->index);
    INVOKE_CHK_EXIST(rdb_create_string(ctx->rdb_s, rdb_var_name,
                                       service->uuid, 0, 0),
                     "Failed to create rdb var - service uuid\n");

    for (characteristic = service->characteristics; characteristic;
         characteristic = characteristic->next) {
        INVOKE_CHK(create_characteristic_rdb_tree(ctx, device, service,
                                                  characteristic),
                   "Failed to create characteristic rdb tree\n");
    }
    return 0;
}

/* Delete the rdb tree associated with the given service. */
static void
delete_service_rdb_tree (dsm_btep_context_t *ctx,
                         dsm_btep_device_t *device,
                         dsm_gatt_service_t *service)
{
    char rdb_var_name[MAX_NAME_LENGTH];
    dsm_gatt_characteristic_t *characteristic;

    if (device->index < 0) {
        return;
    }

    snprintf(rdb_var_name, sizeof rdb_var_name,
             SVC_RDB_FMT_ROOT ".handle",
             BT_DEV_RDB_ROOT, device->index, service->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name,
             SVC_RDB_FMT_ROOT ".uuid",
             BT_DEV_RDB_ROOT, device->index, service->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    for (characteristic = service->characteristics; characteristic;
         characteristic = characteristic->next) {
        delete_characteristic_rdb_tree(ctx, device, service, characteristic);
    }
}

/*
 * Create the full device rdb tree according to the given device struct.
 * Only non-existing rdb variables will be created and initialised.
 * Existing rdb variables will be intact.
 */
static int
create_device_rdb_tree (dsm_btep_device_t *device)
{
    char rdb_var_name[MAX_NAME_LENGTH];
    dsm_btep_context_t *ctx;
    dsm_gatt_service_t *service;

    ctx = device->ctx;

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.%d.address",
             BT_DEV_RDB_ROOT, device->index);
    INVOKE_CHK_EXIST(rdb_create_string(ctx->rdb_s, rdb_var_name,
                                       device->address, 0, 0),
                     "Failed to create rdb var - device address\n");

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.%d.name",
             BT_DEV_RDB_ROOT, device->index);
    INVOKE_CHK_EXIST(rdb_create_string(ctx->rdb_s, rdb_var_name,
                                       device->name, 0, 0),
                     "Failed to create rdb var - device name\n");

    for (service = device->services; service; service = service->next) {
        INVOKE_CHK(create_service_rdb_tree(ctx, device, service),
                   "Failed to create service rdb tree\n");
    }

    return 0;
}

/* Delete the rdb tree associated with the given device. */
void
delete_device_rdb_tree (dsm_btep_context_t *ctx,
                        dsm_btep_device_t *device)
{
    char rdb_var_name[MAX_NAME_LENGTH];
    dsm_gatt_service_t *service;

    if (device->index < 0) { /* skip devices that have not been associated */
        return;
    }

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.%d.address",
             BT_DEV_RDB_ROOT, device->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.%d.name",
             BT_DEV_RDB_ROOT, device->index);
    rdb_delete(ctx->rdb_s, rdb_var_name);

    for (service = device->services; service; service = service->next) {
        delete_service_rdb_tree(ctx, device, service);
    }
}

/*
 * Launch UDE for the BLE device in assoc.
 * The rdb tree of the device will be created if not yet exists.
 * If ude is launched successfully, a conn_success_retry timer will be started.
 * Otherwise, a conn_fail_retry timer will be started.
 * Note, the new timer will be saved in assoc->conn_timer. So the caller should
 * backup any existing timer if necessary.
 */
static int
launch_ude_ble (dsm_btep_association_t *assoc)
{
    pid_t pid;
    int rval = 0;
    dsm_btep_device_t *device = assoc->device;
    char rdb_var_name[MAX_NAME_LENGTH];

    /* create rdb variables tree if necessary */
    rval = create_device_rdb_tree(device);
    if (rval) {
        errp("Failed to create device rdb variable tree\n");
        return rval;
    }

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.%d",
             BT_DEV_RDB_ROOT, device->index);
    pid = fork();
    if (pid == 0) {
        /* Child. */
        dbgp("Launch BLE UDE %s\n", assoc->stream->other_ep.exec_ep.exec_name);
        rval = execlp(assoc->stream->other_ep.exec_ep.exec_name,
                      assoc->stream->other_ep.exec_ep.exec_name,
                      rdb_var_name, GATT_RPC_SERVICE_NAME, NULL);
        /* we should not get here unless error */
        errp("excelp");
        exit(rval);
    } else if (pid > 0) {
        /* Parent */
        if (device->paired) {
            assoc->conn_timer =
                g_timeout_add_seconds(assoc->stream->bt_ep.conn_success_retry,
                                      connect_timer_handler,
                                      assoc);
        }
    } else {
        errp("Failed to fork\n");
        rval = -1;
        if (device->paired) {
            assoc->conn_timer =
                g_timeout_add_seconds(assoc->stream->bt_ep.conn_fail_retry,
                                      connect_timer_handler,
                                      assoc);
        }
    }
    if (!assoc->conn_timer && device->paired) {
        errp("Failed to start conn_timer");
        rval = -1;
    }
    return rval;
}

/*
 * This handler processes all timers from all associations.
 * Both classic and BLE devices share the same timer handler.
 * Params:
 *    user_data - the device-stream association
 * Note:
 *    When a timeout for an association happens, the UDE is awaiting being
 *    launched.
 *    However, before launching, we have to make sure the device is connected.
 *    To that end, Connect(Profile) will be called.
 */
gboolean
connect_timer_handler (gpointer user_data)
{
    dsm_btep_association_t *assoc = (dsm_btep_association_t *)user_data;
    dsm_btep_context_t *ctx = assoc->ctx;
    dsm_btep_device_t *device = assoc->device;
    dsm_btep_stream_t *stream = assoc->stream;
    gboolean ble = stream->bt_ep_type == EP_BT_GC;
    guint saved_timer;

    dbgp("connect timer handler for association %s -> %s\n",
         device->address, stream->other_ep.exec_ep.exec_name);
    dbgp("device state=%d, connected=%d, paired=%d (%s)\n",
         ble ? device->ble_state : device->classic_state,
         device->connected, device->paired,
         ble ? "BLE" : "Classic");

    /*
     * Initiate Connect(Profile) call if we have not done it.
     * It is guaranteed that connect_(profile)_return_handler will be called.
     */
    if (ble) {
        if (device->ble_state == DSM_BTEP_STATE_IDLE) {
            dbgp("Connect to BLE device %s...\n", device->address);
            g_dbus_connection_call(ctx->connection,
                                   BLUEZ_BUS_NAME,
                                   device->dbus_path,
                                   BLUEZ_DEVICE_INTF,
                                   "Connect",
                                   NULL,
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   (GAsyncReadyCallback)
                                   connect_return_handler,
                                   device);
            device->ble_state = DSM_BTEP_STATE_CONNECTING;
        } else if (device->ble_state == DSM_BTEP_STATE_CONNECTED) {
            /* Since we are in timer handler, the timer must be active */
            saved_timer = assoc->conn_timer;
            if (launch_ude_ble(assoc)) {
                errp("Failed to launch ude ble\n");
                if (assoc->conn_timer) {
                    /* remove the current timer since new one has started */
                    return G_SOURCE_REMOVE;
                } else {
                    /* launch_ude failed to start a new timer, reuse saved */
                    assoc->conn_timer = saved_timer;
                    return G_SOURCE_CONTINUE;
                }
            } else {
                return G_SOURCE_REMOVE;
            }
        }
    } else {
        if (device->classic_state == DSM_BTEP_STATE_IDLE) {
            dbgp("ConnectProfile to classic device %s...\n", device->address);
            g_dbus_connection_call(ctx->connection,
                                   BLUEZ_BUS_NAME,
                                   device->dbus_path,
                                   BLUEZ_DEVICE_INTF,
                                   "ConnectProfile",
                                   g_variant_new("(s)", SPP_UUID),
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   (GAsyncReadyCallback)
                                   connect_profile_return_handler,
                                   device);
            device->classic_state = DSM_BTEP_STATE_CONNECTING;
        } else if (device->classic_state == DSM_BTEP_STATE_CONNECTED) {
            /* Since we are in timer handler, the timer must be active */
            saved_timer = assoc->conn_timer;
            if (launch_ude(assoc)) {
                errp("Failed to launch ude\n");
                if (assoc->conn_timer) {
                    /* remove the current timer since new one has started */
                    return G_SOURCE_REMOVE;
                } else {
                    /* launch_ude failed to start a new timer, reuse saved */
                    assoc->conn_timer = saved_timer;
                    return G_SOURCE_CONTINUE;
                }
            } else {
                return G_SOURCE_REMOVE;
            }
        }
    }

    assoc->conn_timer = 0; /* mark timer ends */
    return G_SOURCE_REMOVE; /* one off timer */
}

/*
 * This handler was registered with DBus to handle all the methods in
 * the org.bluez.Profile1 interface. It gets called when bluez invokes
 * any methods in that interface.
 */
static void
profile_handle_method_call (GDBusConnection *connection,
                            const gchar *sender,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *method_name,
                            GVariant *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{
    dsm_btep_context_t *ctx = user_data;
    int fd_index = -1;
    char *device_path = NULL;
    GDBusMessage * message = g_dbus_method_invocation_get_message(invocation);
    GUnixFDList *fdlist = g_dbus_message_get_unix_fd_list(message);
    GError *error = NULL;
    dsm_btep_device_t *device;
    dsm_btep_association_t *assoc;

    g_dbus_method_invocation_return_value(invocation, NULL);
    if (strcmp(method_name, "NewConnection")) {
        /* Other method handlers are not implemented. */
        return;
    }

    /*
     * Retrieve the parameters. Method signature:
     *     void NewConnection(object device, fd, dict fd_properties)
     * fd is the file descriptor index of the socket fd set up for this
     * this connection.
     */
    g_variant_get(parameters, "(&oha{sv})", &device_path, &fd_index, NULL);
    device = device_find(ctx, device_path);
    if (!device) {
        errp("NewConnection from unexpected device %s\n", device_path);
        return;
    }

    device->fd = g_unix_fd_list_get(fdlist, fd_index, &error);
    if (error) {
        bail(ctx, -1, "Unable to get fd: %s\n", error->message);
    }

    /* Mark CONNECTED so fd is valid for use by timer handler */
    device->classic_state = DSM_BTEP_STATE_CONNECTED;
    dbgp("Connected to peer device %s\n", device_path);

    for (assoc = ctx->associations; assoc; assoc = assoc->next) {
        /* An active conn_timer means it is not the time yet */
        if (device == assoc->device && !assoc->conn_timer) {
            /* launch a ude for each association of classic device */
            if (assoc->stream->bt_ep_type == EP_BT_SPP) {
                launch_ude(assoc);
            }
        }
    }
}

/*
 * Function table for the org.bluez.Profile1 interface.
 */
static const GDBusInterfaceVTable profile_interface_vtable_g = {
    profile_handle_method_call,
    NULL,
    NULL
};

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar *name,
                  const gchar *name_owner,
                  gpointer user_data)
{
    dsm_btep_context_t *ctx = (dsm_btep_context_t *)user_data;
    GError *error = NULL;
    GVariant *reply;
    GVariantBuilder *builder;

    dbgp("DBus name appeared %s, owner %s\n", name, name_owner);
    ctx->connection = connection;

    ctx->iface_added_watch_id =
        g_dbus_connection_signal_subscribe(ctx->connection,
                                           BLUEZ_BUS_NAME,
                                           DBUS_OBJ_MGR_INTF,
                                           "InterfacesAdded",
                                           "/",
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           interfaces_added,
                                           ctx,
                                           NULL);

    ctx->iface_removed_watch_id =
        g_dbus_connection_signal_subscribe(ctx->connection,
                                           BLUEZ_BUS_NAME,
                                           DBUS_OBJ_MGR_INTF,
                                           "InterfacesRemoved",
                                           "/",
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           interfaces_removed,
                                           ctx,
                                           NULL);
    /*
     * Create a dbus node that describes the org.bluez.Profile1 interface.
     */
    ctx->profile_bus_node_info =
        g_dbus_node_info_new_for_xml(profile_introspection_xml_g,
                                     &error);
    if (!ctx->profile_bus_node_info) {
        bail(ctx, -1, "Unable to create Profile bus node info: %s (%d)\n",
             error->message, error->code);
    }

    /*
     * Create an object that implements the org.bluez.Profile1 interface.
     */
    ctx->profile_registration_id =
        g_dbus_connection_register_object(connection,
                                          BLUEZ_PROFILE_PATH,
                                          ctx->profile_bus_node_info->
                                            interfaces[0],
                                          &profile_interface_vtable_g,
                                          ctx,
                                          NULL,
                                          &error);
    if (error) {
        bail(ctx, -1, "Register object failed: %s\n", error->message);
    }

    /*
     * Register the Profile1 object with bluez (bluetoothd daemon to
     * be exact). The profile will be registered with a particular
     * UUID and with service authorization not required (devices
     * still need to be paired).
     *
     * bluez will invoke the interface methods for this Profile1 object when
     * a connection to a peer device with the given profile UUID is
     * established (as well as for other profile events as defined in the
     * Profile1 interface).
     */
    builder = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add(builder, "{sv}", "RequireAuthorization",
                          g_variant_new_boolean(FALSE));

    /* Currently only SPP profile is implemented */
    reply = g_dbus_connection_call_sync(connection,
                                        BLUEZ_BUS_NAME,
                                        BLUEZ_PROFILEMGR_PATH,
                                        BLUEZ_PROFILEMGR_INTF,
                                        "RegisterProfile",
                                        g_variant_new("(osa{sv})",
                                                      BLUEZ_PROFILE_PATH,
                                                      SPP_UUID,
                                                      builder),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    g_variant_builder_unref(builder);

    if (reply) {
        g_variant_unref(reply);
    }

    if (error) {
        bail(ctx, -1, "RegisterProfile Failed: %s\n", error->message);
    }

    /* Get all the bluez objects of interest. */
    parse_bluez_objects(ctx);
    if (ctx->exit_status) {
        bail(ctx, -1, "Failed to parse bluez objects\n");
    }

    dbgp("Starting dsm_btep rpc server\n");
    if (dsm_btep_rpc_server_init()) {
        bail(ctx, -1, "Failed to start dsm_btep RPC server\n");
    }

    dbgp("Starting dsm_gatt rpc server\n");
    if (dsm_gatt_rpc_server_init()) {
        bail(ctx, -1, "Failed to start dsm_gatt RPC server\n");
    }
}

static void
usage (const char *prog_name)
{
    printf("Usage:\n");
    printf("\t%s <epa_rdb_root> <epa_type> <epb_rdb_root> <epb_type>\n",
           prog_name);
    printf("e.g.\n");
    printf("\t%s service.dsm.ep.conf.bt_ep 16 service.dsm.ep.conf.exec_ep 7\n",
           prog_name);
}

/*
 * Construct a stream according to argv and relevant rdb variables
 * and insert the stream into the head of ctx->streams.
 */
static int
parse_cmdline (dsm_btep_context_t *ctx, int argc, char *argv[])
{
    int rval = 0;
    dsm_btep_stream_t *stream = NULL;

    if (argc == 1) { /* do not add any stream */
        return 0;
    }
    if (argc != DSM_BTEP_NUM_ARGS + 1) {
        usage(argv[0]);
        return -1;
    }

    /* add one stream */
    rval = construct_stream(ctx->rdb_s, argv + 1, &stream);
    if (!rval) {
        stream->next = ctx->streams;
        ctx->streams = stream;
    }

    return rval;
}

/* final clean up upon main loop exits */
static void
clean_up_ctx (dsm_btep_context_t *ctx)
{
    dsm_btep_stream_t *stream;
    dsm_btep_device_t *device;

    if (!ctx) {
        return;
    }

    for (device = ctx->devices; device; device = ctx->devices) {
        ctx->devices = device->next;
        device_destroy(device); /* this will also destroy assocations */
    }
    for (stream = ctx->streams; stream; stream = ctx->streams) {
        ctx->streams = stream->next;
        stream_destroy(stream);
    }

    if (ctx->profile_registration_id) {
        g_dbus_connection_unregister_object(ctx->connection,
                                            ctx->profile_registration_id);
    }

    if (ctx->profile_bus_node_info) {
        g_dbus_node_info_unref(ctx->profile_bus_node_info);
    }

    if (ctx->iface_added_watch_id) {
        g_dbus_connection_signal_unsubscribe(ctx->connection,
                                             ctx->iface_added_watch_id);
    }
    if (ctx->iface_removed_watch_id) {
        g_dbus_connection_signal_unsubscribe(ctx->connection,
                                             ctx->iface_removed_watch_id);
    }

    if (ctx->bus_watch_id) {
        g_bus_unwatch_name(ctx->bus_watch_id);
    }

    dsm_gatt_rpc_server_destroy();

    dsm_btep_rpc_server_destroy();

    if (ctx->loop) {
        g_main_loop_unref(ctx->loop);
    }

    if (ctx->rdb_s) {
        rdb_close(&ctx->rdb_s);
    }

    free(ctx);
}

/* Signal handlers for SIGINT & SIGTERM */
static gboolean
sigint_handler (gpointer user_data)
{
    dsm_btep_context_t *ctx = (dsm_btep_context_t *)user_data;
    dbgp("%s received (%d). Stopping main loop\n", "SIGINT", SIGINT);
    ctx->exit_status = 0;
    g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE; /* source should be removed */
}

static gboolean
sigterm_handler (gpointer user_data)
{
    dsm_btep_context_t *ctx = (dsm_btep_context_t *)user_data;
    dbgp("%s received (%d). Stopping main loop\n", "SIGTERM", SIGTERM);
    ctx->exit_status = 0;
    g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE; /* source should be removed */
}

int
main (int argc, char *argv[])
{
    int rval;

    ctx = calloc(1, sizeof *ctx);
    if (!ctx) {
        errp("Unable to allocate context!");
        exit(-1);
    }

    /* Set SIGCHLD handler to SIG_IGN to automatically reap zombie processes */
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        errp("Failed to set SIGCHLD handler\n");
        ctx->exit_status = -1;
        goto done;
    }

    /*
     * Set up signal handlers. GLib supports SIGHUP, SIGINT, SIGTERM
     * It is NOT safe to do GLib clean-ups from regular unix signal handlers.
     */
    g_unix_signal_add(SIGINT, sigint_handler, ctx);
    g_unix_signal_add(SIGTERM, sigterm_handler, ctx);

    ctx->exit_status = rdb_open(NULL, &ctx->rdb_s);
    if (ctx->exit_status) {
        errp("Failed to open rdb");
        goto done;
    }

    ctx->exit_status = parse_cmdline(ctx, argc, argv);
    if (ctx->exit_status) {
        goto done;
    }

    /*
     * All further operations need to wait until the bluez bus is available so
     * register a callback to be invoked when that occurs.
     */
    ctx->bus_watch_id = g_bus_watch_name(G_BUS_TYPE_SYSTEM,
                                         BLUEZ_BUS_NAME,
                                         G_BUS_NAME_WATCHER_FLAGS_NONE,
                                         on_name_appeared,
                                         NULL,
                                         ctx,
                                         NULL);
    if (ctx->bus_watch_id == 0) {
        ctx->exit_status = -1;
        errp("Unable to watch dbus");
        goto done;
    }

    /*
     * Start the main loop. The run function does not return until
     * g_main_loop_quit is called. All operations from this point are handled
     * via functions dispatched by the main loop management.
     */
    ctx->loop = g_main_loop_new(NULL, FALSE);
    dbgp("Starting g_main_loop\n");
    g_main_loop_run(ctx->loop);
    dbgp("g_main_loop exited. Start cleaning up...\n");

done:
    /* Clean up */
    rval = ctx->exit_status;
    clean_up_ctx(ctx);

    dbgp("All done! Bye...\n");
    return rval;
}
