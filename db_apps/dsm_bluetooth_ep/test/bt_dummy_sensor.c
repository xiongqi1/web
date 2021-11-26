/*
 * A dummy sensor running on a host.
 * It implemented SPP profile and waits for connection from a peer device.
 * Once connected, it responds to two commands:
 * 1) "read\n": replies with a string representation of current date time.
 * 2) "bye\n": replies with "Bye @ current date time", and then disconnects
 *    in 1 second.
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

#include "bt_dummy_sensor.h"
#include "dsm_bt_utils.h"

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>


/* Forward declarations. */
static gboolean io_dispatch(GSource *source, GSourceFunc callback,
                            gpointer user_data);
static void profile_handle_method_call(GDBusConnection *connection,
                                       const gchar *sender,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *method_name,
                                       GVariant *parameters,
                                       GDBusMethodInvocation *invocation,
                                       gpointer user_data);
static void on_name_appeared(GDBusConnection *connection,
                             const gchar *name,
                             const gchar *name_owner,
                             gpointer user_data);

/*
 * Function table for the org.bluez.Profile1 interface.
 */
static const GDBusInterfaceVTable profile_interface_vtable_g = {
    profile_handle_method_call,
    NULL,
    NULL
};

/*
 * Function table for handling IO source events.
 */
static GSourceFuncs io_source_funcs = {
    NULL,
    NULL,
    io_dispatch,
    NULL,
};

/* Signal handlers for SIGINT & SIGTERM */
static gboolean
sigint_handler (gpointer user_data)
{
    sensor_context_t *ctx = (sensor_context_t *)user_data;
    dbgp("%s received (%d). Stopping main loop\n", "SIGINT", SIGINT);
    ctx->exit_status = 0;
    g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE; /* source should be removed */
}

static gboolean
sigterm_handler (gpointer user_data)
{
    sensor_context_t *ctx = (sensor_context_t *)user_data;
    dbgp("%s received (%d). Stopping main loop\n", "SIGTERM", SIGTERM);
    ctx->exit_status = 0;
    g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE; /* source should be removed */
}

int
main (int argc, char *argv[])
{
    sensor_context_t *ctx;
    int rval;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return -1;
    }
    ctx->fd = -1;

    dbgp("Starting dummy sensor\n");

    g_unix_signal_add(SIGINT, sigint_handler, ctx);
    g_unix_signal_add(SIGTERM, sigterm_handler, ctx);

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
        errp("Unable to watch dbus\n");
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
    /* Clean up before exiting. */
    if (ctx->fd != -1) {
        close(ctx->fd);
    }
    if (ctx->peer_source) {
        g_source_destroy(ctx->peer_source);
    }
    if (ctx->profile_bus_node_info) {
        g_dbus_node_info_unref(ctx->profile_bus_node_info);
    }
    if (ctx->loop) {
        g_main_loop_unref(ctx->loop);
    }
    if (ctx->bus_watch_id != 0) {
        g_bus_unwatch_name(ctx->bus_watch_id);
    }

    rval = ctx->exit_status;
    free(ctx);
    return rval;
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
    sensor_context_t *ctx = (sensor_context_t *) user_data;
    GError *error = NULL;
    GVariant *reply;
    GVariantBuilder *builder;

    dbgp("bus name appeared: %s, owner=%s\n", name, name_owner);

    ctx->connection = connection;

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
                                      BT_DUMMY_SENSOR_PATH,
                                      ctx->profile_bus_node_info->
                                      interfaces[0],
                                      &profile_interface_vtable_g,
                                      ctx,
                                      NULL,
                                      &error);
    if (error) {
        bail(ctx, -error->code, "Register object failed: %s\n", error->message);
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
                                                      BT_DUMMY_SENSOR_PATH,
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
        bail(ctx, -error->code, "RegisterProfile Failed: %s\n", error->message);
    }
}

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
    sensor_context_t *ctx = user_data;
    int fd_index = -1;
    char *device = NULL;
    GDBusMessage *message = g_dbus_method_invocation_get_message(invocation);
    GUnixFDList *fdlist = g_dbus_message_get_unix_fd_list(message);
    GError *error = NULL;

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
    g_variant_get(parameters, "(&oha{sv})", &device, &fd_index, NULL);
    ctx->fd = g_unix_fd_list_get(fdlist, fd_index, &error);
    if (error) {
        bail(ctx, -1, "Unable to get fd: %s\n", error->message);
    }

    dbgp("Connected to peer device %s\n", device);

    /* Keep a copy of device dbus path for disconnect */
    strncpy(ctx->dev_dbus_path, device, sizeof(ctx->dev_dbus_path)-1);

    /*
     * Create an new event source for the socket fd connected to the
     * peer and attach it to the main context. io_source_funcs will be
     * invoked to dispatch any new events for this source.
     */
    ctx->peer_source = g_source_new(&io_source_funcs, sizeof(GSource));
    if (!ctx->peer_source) {
        bail(ctx, -1, "Failed to create peer source\n");
    }
    g_source_set_callback(ctx->peer_source, NULL, ctx, NULL);
    g_source_add_unix_fd(ctx->peer_source, ctx->fd, G_IO_IN);
    g_source_attach(ctx->peer_source, NULL);
}

/*
 * Async method complete handler for the Disconnect dbus invocation.
 * The invocation result can be obtained from the GAsyncResult argument.
 */
static void
disconnect_return_handler (GObject *source_object, GAsyncResult *res,
                           gpointer user_data)
{
    sensor_context_t *ctx = (sensor_context_t *)user_data;
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_finish(ctx->connection, res, &error);
    if (error) {
        errp("Disconnect failed: %s\n", error->message);
    }
    dbgp("Disconnect returns success\n");
    if (result) {
        g_variant_unref(result);
    }
    ctx->dev_dbus_path[0] = '\0';
}

/*
 * Have to use Disconnect here.
 * DisconnectProfile will not notify the peer device.
 */
static gboolean
disconnect_timer_handler (gpointer user_data)
{
    sensor_context_t *ctx = (sensor_context_t *)user_data;
    dbgp("Initiating DisconnectProfile\n");
    g_dbus_connection_call(ctx->connection,
                           BLUEZ_BUS_NAME,
                           ctx->dev_dbus_path,
                           BLUEZ_DEVICE_INTF,
                           "Disconnect",
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback)
                           disconnect_return_handler,
                           ctx);
    return G_SOURCE_REMOVE;
}

/*
 * Called to handle IO events from the peer device.
 */
static gboolean
io_dispatch (GSource *source, GSourceFunc callback,
             gpointer user_data)
{
    int r;
    char buf[IO_BUF_MAX_LEN];
    sensor_context_t *ctx = user_data;
    time_t rawtime;
    char *strtime;
    gboolean bye = FALSE;

    dbgp("Entered io_dispatch\n");
    /*
     * The only source is from remote device ctx->fd.
     */
    r = read(ctx->fd, buf, sizeof(buf) - 1);
    if (r < 0) { /* read error means peer device disconnected */
        dbgp("Peer source removed\n");
        ctx->peer_source = NULL;
        return G_SOURCE_REMOVE;
    } else if (r > 0) {
        buf[r] = 0; /* Ensure NUL termination. */
        dbgp("Received request: %s\n", buf);
        time(&rawtime);
        strtime = ctime(&rawtime);
        if (strstr(buf, "read") == buf) {
            snprintf(buf, sizeof buf, "Now it is %s\n", strtime);
        } else if (strstr(buf, "bye") == buf) {
            bye = TRUE;
            snprintf(buf, sizeof buf, "Bye @ %s\n", strtime);
        } else {
            snprintf(buf, sizeof buf, "Unknown request @ %s\n", strtime);
        }
        dbgp("Responding with: %s", buf);
        /* Never write NULL character while the other end uses fgets */
        if (write(ctx->fd, buf, strlen(buf)) < 0) {
            errp("Failed to write: (%d) %s\n", errno, strerror(errno));
        }
    }

    if (bye) { /* disconnect in 1s */
        g_timeout_add_seconds(1, disconnect_timer_handler, ctx);
        ctx->peer_source = NULL;
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}
