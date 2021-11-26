/*
 * Bluetooth sensor monitor (btsensormon).
 *
 * This monitor runs as a daemon. It manages connecting to and
 * collecting data from bluetooth sensor devices. Device client code can
 * be added to this monitor to handle specific devices.
 *
 *  Client code need only implement the APIS defined in btsm_client_t:
 *    - accept: This gets called by the monitor for each paired device.
 *              If the client returns true for a device then the client
 *              will be recorded as the handler for the device.
 *    - collect: This gets called to collect data from a device. A client's
 *               collect function will only be called if the client has
 *               accepted the device.
 *    - collect_event: This gets called when there is input data available 
 *                     from the device to collect data from a device. Only
 *                     called if not NULL and the client has accepted the
 *                     device.
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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <rdb_ops.h>
#include "btsensormon.h"

extern btsm_client_t *clients_g[];

static void connect_return_handler(GObject *source_object, GAsyncResult *res,
                                   gpointer user_data);
static void store_data(btsm_device_t *dev, btsm_device_data_t *data,
                       unsigned int num_data);

static void
alert_sound (void)
{
    system("speaker-play < /usr/lib/sounds/success");
}

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

/*
 * Async method complete handler for the ConnectProfile dbus invocation.
 * Nothing needs to be done here apart from checking whether the invocation
 * was successful. Note that a successful invocation does not mean that the
 * connection to the remote device has completed. It just means the invocation
 * to the dbus has succeeded. The org.bluez.Profile1.NewConnection callback
 * will be invoked when the connection is successful.
 */
static void
connect_return_handler (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
    btsm_device_t *device = user_data;
    btsm_context_t *ctx = device->ctx;
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_finish(ctx->connection, res, &error);

    if (error) {
        dbgp("Connect to device %s failed: %s \n", device->address,
             error->message);
        device->state = BTSM_STATE_IDLE;
    }

    if (result) {
        g_variant_unref(result);
    }

}

/*
 * Callback function for the PropertiesChanged dbus event.
 */
static void
dev_properties_changed (GDBusConnection *connection,
                        const gchar *sender_name,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *signal_name,
                        GVariant *parameters,
                        gpointer user_data)
{
    btsm_context_t *ctx = user_data;
    btsm_device_t target;
    btsm_device_t *device;
    GVariantIter *prop_iterator;
    btsm_device_data_t data[MAX_NUM_DATA];
    int rval;

    target.dbus_path = (char *)object_path;
    device = device_find(ctx, &target);
    if (!device) {
        /* Not an accepted device. */
        return;
    }

    /* Update the device with the changed property values. */
    g_variant_get(parameters, "(&sa{sv}as)", NULL, &prop_iterator, NULL);
    device_update(device, prop_iterator);
    g_variant_iter_free(prop_iterator);

    if (!device->connected) {
        dbgp("Disconnected from %s.\n", device->address);
        device->state = BTSM_STATE_IDLE;
    } else {
        dbgp("Connected to peer device %s\n", device->dbus_path);
        alert_sound();

        /*
         * Only attempt to collect data from non-profile connected devices.
         * Devices that have a profile connection need to be handled in
         * the org.bluez.Profile1.NewConnection callback as that contains
         * the fd for the connected profile.
         */
        if (!device->profile_uuid) {
            device->state = BTSM_STATE_CONNECTED;
            if (device->client->collect_poll) {
                dbgp("Poll: Collecting from %s..\n", device->address);
                rval = device->client->collect_poll(device, data,
                                                    MAX_NUM_DATA, store_data);
                if (rval > 0) {
                    store_data(device, data, rval);
                }
            }
            device->state = BTSM_STATE_COLLECTED;
        }
    }
}

/*
 * Callback function for the InterfacesAdded dbus event.
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
    btsm_context_t *ctx = user_data;
    GVariant *ifaces_and_props;
    char *dbus_path;

    g_variant_get(parameters, "(&o*)", &dbus_path, &ifaces_and_props);
    parse_bluez_single_object(ctx, dbus_path, ifaces_and_props,
                              dev_properties_changed);
    g_variant_unref(ifaces_and_props);
}

/* Stores device reading data values into RDB */
static void
store_data (btsm_device_t *dev, btsm_device_data_t *data,
            unsigned int num_data)
{
    int ix;
    char rdb_var_name[MAX_NAME_LENGTH];
    char rdb_val[MAX_RDB_BT_VAL_LEN];
    int data_count;
    int start_idx;

    MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), dev->index,
                      RDB_VAR_BT_SENSOR_DEV_DATA_COUNT);
    rdb_get_int(dev->ctx->rdb_s, rdb_var_name, &data_count);

    MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), dev->index,
                      RDB_VAR_BT_SENSOR_DEV_DATA_START_IDX);
    rdb_get_int(dev->ctx->rdb_s, rdb_var_name, &start_idx);

    dbgp("num data: %d\n", num_data);
    for (ix = 0; ix < num_data; ix++) {
        dbgp("data: %s\n", data[ix].value);

        if (data_count >= MAX_NUM_DATA) {
            /* Delete first data idx and move start */
            MK_SENSOR_DATA_VAR(rdb_var_name, sizeof(rdb_var_name), dev->index,
                               start_idx, RDB_VAR_BT_SENSOR_DATA_VALUE);
            rdb_delete(dev->ctx->rdb_s, rdb_var_name);
        }

        /* Store data value after the previous last one. */
        MK_SENSOR_DATA_VAR(rdb_var_name, sizeof(rdb_var_name), dev->index,
                           start_idx + data_count,
                           RDB_VAR_BT_SENSOR_DATA_VALUE);
        rdb_update_string(dev->ctx->rdb_s, rdb_var_name, data[ix].value,
                          RDB_CREATE_FLAGS, 0);

        if (data_count < MAX_NUM_DATA) {
            data_count++;
        } else {
            start_idx++;
        }
    }

    MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), dev->index,
                      RDB_VAR_BT_SENSOR_DEV_DATA_COUNT);
    snprintf(rdb_val, sizeof(rdb_val), "%d", data_count);
    rdb_update_string(dev->ctx->rdb_s, rdb_var_name, rdb_val,
                      RDB_CREATE_FLAGS, 0);

    MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), dev->index,
                      RDB_VAR_BT_SENSOR_DEV_DATA_START_IDX);
    snprintf(rdb_val, sizeof(rdb_val), "%d", start_idx);
    rdb_update_string(dev->ctx->rdb_s, rdb_var_name, rdb_val,
                      RDB_CREATE_FLAGS, 0);

    alert_sound();
}

static gboolean
io_dispatch (GSource *source, GSourceFunc callback,
             gpointer user_data)
{
    btsm_device_t *device = user_data;
    btsm_device_data_t data[MAX_NUM_DATA];
    int rval;

    if (!device->connected) {
        dbgp("remove source\n");
        close(device->fd);
        device->fd = -1;
        return G_SOURCE_REMOVE;
    }

    dbgp("Event: Collecting from %s..\n", device->address);
    rval = device->client->collect_event(device, data, MAX_NUM_DATA);
    if (rval > 0) {
        store_data(device, data, MAX_NUM_DATA);
    }
    device->state = BTSM_STATE_COLLECTED;

    return G_SOURCE_CONTINUE;
}

static GSourceFuncs io_source_funcs = {
    NULL,
    NULL,
    io_dispatch,
    NULL,
};

/*
 * This handler was registered with Dbus to handle all the methods in
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
    btsm_context_t *ctx = user_data;
    int fd_index = -1;
    char *device_path = NULL;
    GDBusMessage * message = g_dbus_method_invocation_get_message(invocation);
    GUnixFDList *fdlist = g_dbus_message_get_unix_fd_list(message);
    GError *error = NULL;
    btsm_device_t target;
    btsm_device_t *device;
    btsm_device_data_t data[MAX_NUM_DATA];
    int rval;

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
    target.dbus_path = device_path;
    device = device_find(ctx, &target);
    if (!device) {
        errp("NewConnection from unexpected device %s\n", device_path);
        return;
    }
    device->fd = g_unix_fd_list_get(fdlist, fd_index, &error);
    if (error) {
        bail(ctx, -1, "Unable to get fd: %s\n", error->message);
    }

    device->state = BTSM_STATE_CONNECTED;
    dbgp("Connected to peer device %s\n", device_path);

    if (device->client->collect_poll) {
        dbgp("Poll: Collecting from %s..\n", device->address);
        rval = device->client->collect_poll(device, data, MAX_NUM_DATA,
                                            store_data);
        if (rval > 0) {
            store_data(device, data, rval);
        }
        device->state = BTSM_STATE_COLLECTED;
    } if (device->client->collect_event) {
        GSource *peer_source = g_source_new(&io_source_funcs, sizeof(GSource));
        g_source_set_callback(peer_source, NULL, device, NULL);
        g_source_add_unix_fd(peer_source, device->fd, G_IO_IN);
        g_source_attach(peer_source, NULL);
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

static gboolean
connect_timer_handler (gpointer user_data)
{
    btsm_context_t *ctx = user_data;
    const char *connect_function;
    GVariant *connect_params;
    int num_tried;
    static btsm_device_t *device = NULL;

    dbgp("connect_timer_handler\n");

    for (num_tried = 0; (num_tried < ctx->num_devices); num_tried++) {
        if (!device) {
            device = ctx->devices;
        }
        if (device->paired && (device->state == BTSM_STATE_IDLE)) {
            break;
        }
        device = device->next;
    }

    if (num_tried == ctx->num_devices) {
        dbgp("No devices ready for connecting to\n");
        return G_SOURCE_CONTINUE;
    }

    if (device->profile_uuid) {
        dbgp("Connecting to device %s profile %s..\n",
             device->address, device->profile_uuid);
        connect_function = "ConnectProfile";
        connect_params = g_variant_new("(s)", device->profile_uuid);
    } else {
        dbgp("Connecting to device %s, any profile\n",
             device->address);
        connect_function = "Connect";
        connect_params = NULL;
    }
        
    g_dbus_connection_call(ctx->connection,
                           BLUEZ_BUS_NAME,
                           device->dbus_path,
                           BLUEZ_DEVICE_INTF,
                           connect_function,
                           connect_params,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback)
                           connect_return_handler,
                           device);
        
    device->state = BTSM_STATE_CONNECTING;
    device = device->next;

    return G_SOURCE_CONTINUE;
}

/*
 * Handler that is invoked when the bluez bus is available.
 */
static void
on_name_appeared (GDBusConnection *connection,
                  const gchar *name,
                  const gchar *name_owner,
                  gpointer user_data)
{
    btsm_context_t *ctx = user_data;
    GError *error = NULL;
    GVariant *reply;
    GVariantBuilder *builder;
    int poll_sec;
    int rval;

    ctx->connection = connection;

    ctx->interface_watch_id =
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
    parse_bluez_objects(ctx, dev_properties_changed);
    if (ctx->exit_status) {
        return;
    }

    rval = rdb_get_int(ctx->rdb_s, RDB_VAR_BT_SENSOR_POLL_SEC, &poll_sec);
    if (rval) {
        dbgp("Use default poll seconds\n");
        poll_sec = CONNECT_PERIOD_SEC;
    }

    dbgp("Using %d poll seconds\n", poll_sec);

    ctx->connect_timer = g_timeout_add_seconds(poll_sec,
                                               connect_timer_handler,
                                               ctx);

    if (ctx->connect_timer == 0) {
        bail(ctx, -1, "Unable to start connect timer\n");
    }
}

int
main (int argc, char *argv[])
{
    btsm_context_t *ctx;
    int rval;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return -1;
    }
    rval = rdb_open(NULL, &ctx->rdb_s);
    ctx->clients = clients_g;

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
    g_main_loop_run(ctx->loop);

 done:
    rval = ctx->exit_status;
    if (ctx->bus_watch_id != 0) {
        g_bus_unwatch_name(ctx->bus_watch_id);
    }
    if (ctx->loop) {
        g_main_loop_unref(ctx->loop);
    }
    rdb_close(&ctx->rdb_s);
    free(ctx);
    return rval;
}
