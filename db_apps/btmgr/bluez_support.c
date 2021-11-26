/*
 * bluez_support.c
 *    Support functions for interfacing to Bluez.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#include <rdb_ops.h>
#include "btmgr_priv.h"
#include "bluez_support.h"

/*
 * Bluez device property names. These are not all the properties of a device
 * as defined by Bluez but a subset of those that are of interest.
 * NOTE: The entries in this array MUST be in the same order as the
 * enum values in bz_device_prop_idx_t;
 */
char *BZ_DEVICE_PROPERTIES[] = {
    BLUEZ_PROP_DEV_ADDRESS,
    BLUEZ_PROP_DEV_NAME,
    BLUEZ_PROP_DEV_PAIRED,
};

static GMainLoop *loop;
static GDBusConnection *dbus_conn = NULL;
static guint watcher_id;
static guint owner_id;
static guint registration_id;
static char *adapter_dbus_path = NULL;
static bz_client_t *client = NULL;
static bz_device_t *device_list = NULL;
static bz_device_free_list_t device_free_list;
static bz_pairing_operation_t pairing_operation;
static struct rdb_session *rdb_s;

static void properties_changed(GDBusConnection *connection,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer user_data);

static GDBusNodeInfo *agent_bus_node_info = NULL;

/* Introspection data for the bluez Agent service we are exporting */
static const gchar agent_introspection_xml[] =
  "<node>"
  "  <interface name='org.bluez.Agent1'>"
  "    <method name='Release'>"
  "    </method>"
  "    <method name='RequestPinCode'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='s' name='pincode' direction='out'/>"
  "    </method>"
  "    <method name='DisplayPinCode'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='s' name='pincode' direction='in'/>"
  "    </method>"
  "    <method name='RequestPasskey'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='u' name='passkey' direction='out'/>"
  "    </method>"
  "    <method name='DisplayPassKey'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='u' name='passkey' direction='in'/>"
  "      <arg type='q' name='entered' direction='in'/>"
  "    </method>"
  "    <method name='RequestConfirmation'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='u' name='passkey' direction='in'/>"
  "    </method>"
  "    <method name='RequestAuthorization'>"
  "      <arg type='o' name='device' direction='in'/>"
  "    </method>"
  "    <method name='AuthorizeService'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='s' name='uuid' direction='in'/>"
  "    </method>"
  "    <method name='Cancel'>"
  "    </method>"
  "  </interface>"
  "</node>";

#define DBUS_OWN_BUS_NAME "com.netcommwireless.btmgr"

/*
 * Converts a GVariant value to a string representation.
 */
static void
g_variant_strncpy (char *dest, unsigned int dest_size, GVariant *value)
{
    char *str;

    /*
     * For string variants g_variant_print returns the strings enclosed in
     * single quotes which is not wanted. So for string variants get the
     * string directly otherwise just use g_variant_print.
     */
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
        strncpy(dest, g_variant_get_string(value, NULL), dest_size);
    } else {
        str = g_variant_print(value, FALSE);
        strncpy(dest, str, dest_size);
        free(str);
    }
}

/*
 * Get the device structure that has a given address and/or dbus path.
 */
static bz_device_t *
device_get (char *address, char *dbus_path)
{
    bz_device_t *dev;

    dev = device_list;
    while (dev) {
        if ((!address || !strcmp(dev->properties[PROP_DEV_ADDRESS], address)) &&
            (!dbus_path || !strcmp(dev->dbus_path, dbus_path))) {
            break;
        }
        dev = dev->next;
    }

    return dev;
}

/*
 * Allocates a new device structure. Will first try to recycle a previously used
 * device structure by looking in the device free list. New memory is allocated
 * if the free list is empty. Done this way to minimise memory fragmentation.
 */
static bz_device_t *
device_alloc (void)
{
    bz_device_t *dev;

    if (device_free_list.count > 0) {
        dev = device_free_list.free_list;
        device_free_list.free_list = device_free_list.free_list->next;
        device_free_list.count--;
        dbgp("Recycle dev mem, free count = %d\n", device_free_list.count);
    } else {
        dev = malloc(sizeof(bz_device_t));
        dbgp("Alloc new mem\n");
    }

    if (dev) {
        memset(dev, 0, sizeof(bz_device_t));
    }
    return dev;
}

/*
 * Free a device structure. The device structure will be placed into the free
 * list for later recycling unless the free list count has reached the max
 * threshold. Done this way to balance memory fragementation minimisation and
 * memory usage.
 */
static void
device_free (bz_device_t *dev)
{
    if (device_free_list.count < BZ_DEV_FREELIST_MAX_SIZE) {
        dev->next = device_free_list.free_list;
        device_free_list.free_list = dev;
        device_free_list.count++;
    } else {
        free(dev);
    }
}

/*
 * Creates a new device with the given dbus path and device properties.
 */
static bz_device_t *
device_create (char *dbus_path, GVariant *device_properties)
{
    bz_device_prop_idx_t prop_ix;
    GVariant *prop_val;
    gboolean res;
    bz_device_t *dev;

    dev = device_get(NULL, dbus_path);
    if (dev) {
        errp("Attempt to create an existing device %s\n", dbus_path);
        return NULL;
    }

    if (strlen(dbus_path) > (BZ_BUF_LEN - 1)) {
        errp("Device dbus_path (%s) too long\n", dbus_path);
        return (NULL);
    }

    dev = device_alloc();
    if (!dev) {
        errp("Failed to alloc new device %s\n", dbus_path);
        return (NULL);
    }

    strcpy(dev->dbus_path, dbus_path);

    /* Store away the device properties of interest */
    for (prop_ix = 0; prop_ix < PROP_DEV_MAX; prop_ix++) {
        res = g_variant_lookup(device_properties, BZ_DEVICE_PROPERTIES[prop_ix],
                               "*", &prop_val);
        if (!res) {
            /* Some of the properties are optional */
            dbgp("Property not present: %s\n", BZ_DEVICE_PROPERTIES[prop_ix]);
        }
        else {
            g_variant_strncpy(dev->properties[prop_ix],
                              sizeof(dev->properties[prop_ix]), prop_val);
        }
    }

    /*
     * The bluez device name property is optional. If it is not set then
     * use the device address.
     */
    if (strlen(dev->properties[PROP_DEV_NAME]) == 0) {
        strncpy(dev->properties[PROP_DEV_NAME],
                dev->properties[PROP_DEV_ADDRESS],
                sizeof(dev->properties[PROP_DEV_NAME]));
    }

    /* Subscribe to changes in the device properties */
    dev->property_watch_id =
        g_dbus_connection_signal_subscribe(dbus_conn,
                                           BLUEZ_BUS_NAME,
                                           PROPERTIES_INTF,
                                           "PropertiesChanged",
                                           dbus_path,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           properties_changed,
                                           NULL,
                                           NULL);

    dev->next = device_list;
    device_list = dev;

    return dev;
}

/*
 * Destroy the device structure that has the given dbus path.
 */
static void
device_destroy (char *dev_path)
{
    bz_device_t *dev = device_list;
    bz_device_t *prev = NULL;

    /* Find the device and deqeue it */
    while (dev) {
        if (!strcmp(dev->dbus_path, dev_path)) {
            if (!prev) {
                device_list = dev->next;
            } else {
                prev->next = dev->next;
            }
            break;
        }
        prev = dev;
        dev = dev->next;
    }

    g_dbus_connection_signal_unsubscribe(dbus_conn, dev->property_watch_id);
    device_free(dev);
}

/*
 * Parses a single dbus bluez object.
 * iface_and_props has the GVariant format: a{sa{sv}}
 */
static void
parse_bluez_single_object (char *path, GVariant *iface_and_props)
{
    GVariantIter interface_iter;
    GVariant *properties;
    char *interface_name;
    bz_device_t *dev;

    g_variant_iter_init(&interface_iter, iface_and_props);
    while (g_variant_iter_loop(&interface_iter, "{s*}", &interface_name,
                               &properties)) {
        if (!strcmp(interface_name, BLUEZ_DEVICE_INTF)) {
            /* Object is a device */
            dev = device_create(path, properties);
            if (!dev) {
                errp("Failed to create new device\n");
                return;
            }
            if (client && client->device_added) {
                client->device_added(dev, client->user_data);
            }
        } else if (!strcmp(interface_name, BLUEZ_ADAPTER_INTF)) {
            /* Object is an adapter */
            if (!adapter_dbus_path) {
                adapter_dbus_path = strdup(path);
            }
        }
    }
}

/*
 * Parse a list of dbus bluez objects.
 * objects has the GVariant format: (a{oa{sa{sv}}})
 */
static void
parse_bluez_objects (GVariant *objects)
{
    GVariantIter giter;
    GVariant *objs;
    char *object_path;
    GVariant *iface_and_props;

    g_variant_get(objects, "(*)", &objs);

    g_variant_iter_init(&giter, objs);
    while (g_variant_iter_loop(&giter, "{o*}", &object_path,
                               &iface_and_props)) {
        dbgp("Parse object: %s\n", object_path);
        parse_bluez_single_object(object_path, iface_and_props);
    }

    g_variant_unref(objs);
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
    GVariant *iface_and_props;
    char *dbus_path;

    g_variant_get(parameters, "(&o*)", &dbus_path, &iface_and_props);
    parse_bluez_single_object(dbus_path, iface_and_props);
    g_variant_unref(iface_and_props);
}

/*
 * Callback function for the InterfacesRemoved dbus event.
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
    GVariantIter *intf_iterator = NULL;
    char *intf_name;
    bz_device_t *dev;

    dbgp("\n");

    /*
     * Loop through all the removed interfaces. Only interested in Device
     * removals.
     */
    g_variant_get(parameters, "(&oas)", &dbus_path, &intf_iterator);
    while (g_variant_iter_loop(intf_iterator, "s", &intf_name)) {
        if (!strcmp(intf_name, BLUEZ_DEVICE_INTF)) {
            dev = device_get(NULL, dbus_path);
            if (dev) {
                if (client && client->device_removed) {
                    client->device_removed(dev, client->user_data);
                }
                device_destroy(dbus_path);
            }
            break;
        }
    }

    g_variant_iter_free(intf_iterator);
}

/*
 * Callback function for the PropertiesChanged dbus event.
 */
static void
properties_changed (GDBusConnection *connection,
                    const gchar *sender_name,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *signal_name,
                    GVariant *parameters,
                    gpointer user_data)
{
    bz_device_t *dev;
    char *intf_name;
    char *prop_name;
    int ix;
    GVariant *prop_val;
    GVariantIter *prop_iterator = NULL;

    dbgp("object: %s\n", object_path);

    /* Only care about changes to device properties */

    /* Find the device that this properties change is for */
    dev = device_get(NULL, (char *)object_path);
    if (!dev) {
        /* Unknown device */
        errp("Property change on unknown device: %s\n", object_path);
        return;
    }

    /*
     * Loop through the changed properties and save those of interest into
     * the device structure.
     */
    g_variant_get(parameters, "(&sa{sv}as)", &intf_name, &prop_iterator, NULL);
    if (!strcmp(intf_name, BLUEZ_DEVICE_INTF)) {
        while (g_variant_iter_loop(prop_iterator, "{sv}", &prop_name,
                                   &prop_val)) {
            dbgp("Prop: %s\n", prop_name);
            for (ix = 0; ix < PROP_DEV_MAX; ix++) {
                if (!strcmp(BZ_DEVICE_PROPERTIES[ix], prop_name)) {
                    g_variant_strncpy(dev->properties[ix],
                                      sizeof(dev->properties[ix]),
                                      prop_val);
                    break;
                }
            }
        }

        g_variant_iter_free(prop_iterator);
    }
}

/*
 * Updates the appropriate rdb variable with the current pairing state.
 * Clients (e.g. screen_manager) can trigger on this rdb variable to perform
 * pairing operations.
 */
static void
pair_op_rdb_update (void)
{
    char status_buf[BZ_MAX_STATUS_LEN];
    int len = sizeof(status_buf);
    int rval = 0;

    rval = bz_get_pairing_status_string(status_buf, len);
    if (rval) {
        errp("Failed to get pairing status string: %d", rval);
        return;
    }

    rdb_update_string(rdb_s, RDB_BT_OP_PAIR_STATUS_VAR, status_buf, 0, 0);
}

/*
 * Handles method invocations of the Agent1 bluez interface. The bluez
 * bluetoothd will invoke this interface whenever there is a pairing request
 * that requires Agent interaction.
 *
 * At the moment, only the RequestConfirmation method is handled. This is
 * because the btmgr Agent registers with capability DisplayYesNo. So all
 * pairings will either have no passkey/pin interaction (simple pairing) or
 * involve only a passkey confirmation.
 */
static void
agent_handle_method_call (GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
    char *device_dbus_path = NULL;
    unsigned int passkey;
    bz_device_t *dev;

    dbgp("method=%s\n", method_name);

    if (!strcmp(method_name, BLUEZ_AGENT_METHOD_REQ_CONF)) {
        g_variant_get(parameters, "(&ou)", &device_dbus_path, &passkey);
        dbgp("dev path %s, passkey %d\n", device_dbus_path, passkey);

        dev = device_get(NULL, device_dbus_path);
        if (!dev) {
            /* Unknown device */
            g_dbus_method_invocation_return_error(invocation, G_IO_ERROR,
                                                  G_IO_ERROR_INVALID_ARGUMENT,
                                                  "Unknown device");
            return;
        }

        /*
         * Don't reply to bluetoothd yet. Just store away the passkey details.
         * The reply will be sent after the user confirms or rejects the
         * passkey (via one of the UIs).
         */
        pairing_operation.state = PAIRING_CONFIRM_PASSKEY;
        pairing_operation.device = dev;
        pairing_operation.passkey = passkey;
        pairing_operation.pending_invocation = invocation;
        pair_op_rdb_update();
    } else if (!strcmp(method_name, BLUEZ_AGENT_METHOD_CANCEL)) {
        pairing_operation.state = PAIRING_CANCEL;
        pair_op_rdb_update();
    }
}

static const GDBusInterfaceVTable agent_interface_vtable =
{
  agent_handle_method_call,
  NULL,
  NULL
};

/*
 * Callback when a bus name ownership request successfully connects to the bus.
 */
static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
    dbgp("\n");

    /* Register a new Agent object */
    registration_id =
        g_dbus_connection_register_object(connection,
                                          BLUEZ_AGENT_PATH,
                                          agent_bus_node_info->interfaces[0],
                                          &agent_interface_vtable,
                                          NULL,  /* user_data */
                                          NULL,  /* user_data_free_func */
                                          NULL); /* GError** */

    g_assert(registration_id > 0);
}

/*
 * Callback when a bus name owership request successfully acquires the
 * requested name.
 */
static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    GError *error = NULL;
    GVariant *reply;

    dbgp("\n");

    /* Register the Agent with bluetoothd */
    reply = g_dbus_connection_call_sync(connection,
                                        BLUEZ_BUS_NAME,
                                        BLUEZ_AGENTMGR_PATH,
                                        BLUEZ_AGENTMGR_INTF,
                                        "RegisterAgent",
                                        g_variant_new("(os)",
                                                      BLUEZ_AGENT_PATH,
                                                      BTMGR_AGENT_CAPABILITY),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (reply) {
        g_variant_unref(reply);
    }

    if (error) {
        errp("RegisterAgent Failed: %s\n", error->message);
    } else {
        dbgp("Agent Registered\n");
    }

    /* Register the Agent as the default system agent */
    reply = g_dbus_connection_call_sync(connection,
                                        BLUEZ_BUS_NAME,
                                        BLUEZ_AGENTMGR_PATH,
                                        BLUEZ_AGENTMGR_INTF,
                                        "RequestDefaultAgent",
                                        g_variant_new("(o)",
                                                      BLUEZ_AGENT_PATH),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (reply) {
        g_variant_unref(reply);
    }

    if (error) {
        errp("RegisterDefaultAgent Failed: %s\n", error->message);
    } else {
        dbgp("Default Agent Registered\n");
    }
}

/*
 * Callback handler for when a Bluez bus is found.
 */
static void
on_name_appeared (GDBusConnection *connection,
                  const gchar *name,
                  const gchar *name_owner,
                  gpointer user_data)
{
    GError *error = NULL;
    GVariant *reply = NULL;

    dbgp("\n");

    dbus_conn = connection;

    /* Subscribe to all the dbus events of interest */

    g_dbus_connection_signal_subscribe(connection,
                                       BLUEZ_BUS_NAME,
                                       OBJ_MGR_INTF,
                                       "InterfacesAdded",
                                       "/",
                                       NULL,
                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                       interfaces_added,
                                       NULL,
                                       NULL);

    g_dbus_connection_signal_subscribe(connection,
                                       BLUEZ_BUS_NAME,
                                       OBJ_MGR_INTF,
                                       "InterfacesRemoved",
                                       "/",
                                       NULL,
                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                       interfaces_removed,
                                       NULL,
                                       NULL);

    /* Grab ownership of own bus (for Agent registration) */
    owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                              DBUS_OWN_BUS_NAME,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              NULL,
                              NULL,
                              NULL);

    /* Get and parse all the bluez objects */
    reply = g_dbus_connection_call_sync(connection,
                                        BLUEZ_BUS_NAME,
                                        "/",
                                        OBJ_MGR_INTF,
                                        "GetManagedObjects",
                                        NULL,
                                        G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (reply) {
        parse_bluez_objects(reply);
        g_variant_unref(reply);
    }

    if (client && client->ready) {
        client->ready(client->user_data);
    }
}

/*
 * Intialise bluez support.
 */
int
bz_init (bz_client_t *bz_client)
{
    GError *error = NULL;
    int rval;

    dbgp("\n");

    if (client) {
        return -EALREADY;
    }

    rval = rdb_open(NULL, &rdb_s);
    if (rval) {
        return rval;
    }

    client = bz_client;

    device_free_list.count = 0;
    device_free_list.free_list = NULL;
    memset(&pairing_operation, 0, sizeof(pairing_operation));
    pairing_operation.state = PAIRING_NONE;
    pair_op_rdb_update();

    agent_bus_node_info = g_dbus_node_info_new_for_xml(agent_introspection_xml,
                                                       &error);
    if (!agent_bus_node_info) {
        errp("Unable to create Agent bus node info: %s (%d)\n", error->message,
             error->code);
        return (-error->code);
    }

    watcher_id = g_bus_watch_name(G_BUS_TYPE_SYSTEM,
                                  BLUEZ_BUS_NAME,
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  on_name_appeared,
                                  NULL,
                                  NULL,
                                  NULL);

    return 0;
}

/*
 * Kicks off the main loop processing. This function does not return unless
 * the main loop is stopped.
 */
int
bz_run (void)
{
    bz_device_t *dev, *next;

    dbgp("\n");

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    g_bus_unwatch_name(watcher_id);

    /* Free device list */
    dev = device_list;
    while (dev) {
        next = dev->next;
        device_destroy(dev->dbus_path);
        dev = next;
    }

    /* Free device free list */
    dev = device_free_list.free_list;
    while (dev) {
        next = dev->next;
        free(dev);
        dev = next;
    }

    free(adapter_dbus_path);
    adapter_dbus_path = NULL;

    dbgp("Main loop stopped");
    return 0;
}

/*
 * Stop bluez processing.
 */
void
bz_stop (void)
{
    if (registration_id > 0) {
        g_dbus_connection_unregister_object(dbus_conn, registration_id);
    }

    g_main_loop_quit(loop);

    if (rdb_s) {
        rdb_close(&rdb_s);
    }
}

/*
 * Sets an adapter property with the given prop_name and prop_value.
 */
int
bz_set_adapter_property (char *prop_name, char *prop_value)
{
    GVariant *prop_val_variant = NULL;
    int prop_val_int;
    GVariant *reply;
    GError *error = NULL;

    /*
     * The exact GVariant type of the property value depends on the property. So
     * create the appropriate GVariant type.
     */
    if (!strcmp(BLUEZ_ADAPTER_PROP_ALIAS, prop_name)) {
        /* String properties */
        prop_val_variant = g_variant_new_string(prop_value);
    } else if (!strcmp(BLUEZ_ADAPTER_PROP_POWERED, prop_name) ||
               !strcmp(BLUEZ_ADAPTER_PROP_PAIRABLE, prop_name) ||
               !strcmp(BLUEZ_ADAPTER_PROP_DISCOVERABLE, prop_name)) {
        /* Boolean properties */
        prop_val_int = !!(atoi(prop_value));
        prop_val_variant = g_variant_new_boolean(prop_val_int);
    } else if (!strcmp(BLUEZ_ADAPTER_PROP_DISC_TIMEOUT, prop_name)) {
        /* Unsigned int32 properties */
        prop_val_int = atoi(prop_value);
        prop_val_variant = g_variant_new_uint32(prop_val_int);
    } else {
        errp("Unknown or unsupported adapter property: %s\n", prop_name);
        return (-EINVAL);
    }

    /* Send it off to bluez to change the underlying adapter property */
    reply = g_dbus_connection_call_sync(dbus_conn,
                                        BLUEZ_BUS_NAME,
                                        adapter_dbus_path,
                                        PROPERTIES_INTF,
                                        "Set",
                                        g_variant_new("(ssv)",
                                                      BLUEZ_ADAPTER_INTF,
                                                      prop_name,
                                                      prop_val_variant),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (reply) {
        g_variant_unref(reply);
    }

    if (error) {
        errp("Unable to set adapter property: %s (%d)\n", error->message,
             error->code);
        return (-error->code);
    }

    return 0;
}

/*
 * Get the device list.
 */
bz_device_t *
bz_get_device_list (void)
{
    return device_list;
}

/*
 * Start a bluetooth scan.
 */
int
bz_scan_start (void)
{
    GVariant *reply;
    GError *error = NULL;

    dbgp("\n");

    if (!dbus_conn || !adapter_dbus_path) {
        return (-EINVAL);
    }

    reply = g_dbus_connection_call_sync(dbus_conn,
                                        BLUEZ_BUS_NAME,
                                        adapter_dbus_path,
                                        BLUEZ_ADAPTER_INTF,
                                        "StartDiscovery",
                                        NULL,
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (reply) {
        g_variant_unref(reply);
    }

    if (error) {
        errp("Failed to start device scan: %s\n", error->message);
        return (-error->code);
    } else {
        dbgp("Scan started\n");
        return 0;
    }
}

/*
 * Stop a bluetooth scan.
 */
int
bz_scan_stop (void)
{
    GVariant *reply;
    GError *error = NULL;

    dbgp("\n");

    if (!dbus_conn || !adapter_dbus_path) {
        return (-EINVAL);
    }

    reply = g_dbus_connection_call_sync(dbus_conn,
                                        BLUEZ_BUS_NAME,
                                        adapter_dbus_path,
                                        BLUEZ_ADAPTER_INTF,
                                        "StopDiscovery",
                                        NULL,
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (reply) {
        g_variant_unref(reply);
    }

    if (error) {
        errp("Failed to stop device scan: %s\n", error->message);
        return (-error->code);
    } else {
        dbgp("Scan stopped\n");
        return 0;
    }
}

/*
 * Handles the DBUS reply for a previously initiated async pair request.
 */
static void
pair_dbus_reply (GObject *source_object,
                 GAsyncResult *res,
                 gpointer user_data)
{
    GVariant *result;
    GError *error = NULL;
    char *error_code;

    dbgp("\n");

    /* Get the pair request result */
    result = g_dbus_connection_call_finish(dbus_conn, res, &error);

    if (error) {
        pairing_operation.state = PAIRING_FAIL;

        /*
         * Store the error so that it can be retrieved via a pair status
         * request later on.
         */
        error_code = g_dbus_error_get_remote_error(error);

        /* In case error_code was not retrieved successfully */
        pairing_operation.last_error[0] = '\0';

        if (error_code) {
            snprintf(pairing_operation.last_error,
                    sizeof(pairing_operation.last_error), "%s", error_code);

            g_free(error_code);
        }

        dbgp("Failed to pair device: %s %d:%d\n", error->message, error->domain,
             error->code);
    } else {
        pairing_operation.state = PAIRING_SUCCESS;
        dbgp("Device Paired\n");
    }

    pair_op_rdb_update();

    if (result) {
        g_variant_unref(result);
    }
}

/*
 * Intiates a pairing request.
 */
int
bz_pair (char *address)
{
    bz_device_t *device;

    device = device_get(address, NULL);
    if (!device) {
        return (-EINVAL);
    }

    g_dbus_connection_call(dbus_conn,
                           BLUEZ_BUS_NAME,
                           device->dbus_path,
                           BLUEZ_DEVICE_INTF,
                           "Pair",
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           G_MAXINT,
                           NULL,
                           pair_dbus_reply,
                           NULL);

    pairing_operation.state = PAIRING_INPROGRESS;
    pairing_operation.device = device;
    pair_op_rdb_update();

    return 0;
}

/*
 * Writes a pairing status string into a given status_buf. The status string
 * contains different data depending on the pair state. The pair state is always
 * the first item in the string:
 *
 * If pair_state = confirm_passkey:
 *    confirm_passkey;<passkey>;<address>;<name>
 * If pair_state = fail:
 *    fail;<error string>;<address>;<name>
 * Else:
 *    <pair_state>;<address>;<name>
 */
int
bz_get_pairing_status_string (char *status_buf, int len)
{
    int bytes_w = 0;
    int buf_size = len;

    /* Add pair state and any relevant pair data to the string */
    switch (pairing_operation.state) {
    case (PAIRING_NONE):
        bytes_w = snprintf(status_buf, len, "%s", "not_pairing");
        break;
    case (PAIRING_INPROGRESS):
        bytes_w = snprintf(status_buf, len, "%s", "in_progress");
        break;
    case (PAIRING_CONFIRM_PASSKEY):
        bytes_w = snprintf(status_buf, len, "%s;%d", "confirm_passkey",
                           pairing_operation.passkey);
        break;
    case (PAIRING_PASSKEY_CONFIRMED):
        bytes_w = snprintf(status_buf, len, "passkey_confirmed");
        break;
    case (PAIRING_CANCEL):
        bytes_w = snprintf(status_buf, len, "cancel");
        break;
    case (PAIRING_SUCCESS):
        bytes_w = snprintf(status_buf, len, "%s", "success");
        break;
    case (PAIRING_FAIL):
        bytes_w = snprintf(status_buf, len, "%s;%s", "fail",
                           pairing_operation.last_error);
        break;
    default:
        return -EINVAL;
    }

    if (bytes_w >= len) {
        return -EOVERFLOW;
    }

    /* Add device address and name to the string */
    if (pairing_operation.device) {
        len -= bytes_w;
        bytes_w =
            snprintf(&(status_buf[buf_size - len]), len, ";%s;",
                     pairing_operation.device->properties[PROP_DEV_ADDRESS]);

        if (bytes_w >= len) {
            return -EOVERFLOW;
        }

        len -= bytes_w;
        /**
         * dev_name needs to be percent encoded for ; and %
         * since ; is delimiter in pair_status and % is part of percent
         * encoded char sequence
         */
        bytes_w = percent_encoded_strncpy(&status_buf[buf_size - len],
                    pairing_operation.device->properties[PROP_DEV_NAME],
                    len, ";%");
        if (bytes_w < 0 || bytes_w >= len) {
            return -EOVERFLOW;
        }
    }

    return 0;
}

/*
 * Checks whether a pairing operation is currently in progress.
 */
int
bz_pairing_in_progress (void)
{
    return (pairing_operation.state == PAIRING_INPROGRESS);
}

/*
 * Sends a DBUS passkey confirm/reject.
 */
int
bz_passkey_confirm (char *address, unsigned int passkey, int confirm)
{
    GDBusMethodInvocation *inv;

    dbgp("address: %s, confirm: %d, passkey %d\n", address, confirm, passkey);

    /*
     * Verify that the confirmation is for the current active pairing
     * operation.
     */
    if ((pairing_operation.state != PAIRING_CONFIRM_PASSKEY) ||
        (pairing_operation.passkey != passkey) ||
        (strcmp(pairing_operation.device->properties[PROP_DEV_ADDRESS],
                address))) {
        return -EINVAL;
    }

    inv = pairing_operation.pending_invocation;
    if (confirm) {
        /*
         * A NULL returned to Bluez indicates that the passkey has been
         * confirmed.
         */
        g_dbus_method_invocation_return_value(inv, NULL);
    } else {
        /*
         * An error returned to Bluez indicates that the passkey has been
         * rejected.
         */
        g_dbus_method_invocation_return_dbus_error(inv,
                                                   "org.bluez.Error.Rejected",
                                                   "Rejected");
    }

    pairing_operation.state = PAIRING_PASSKEY_CONFIRMED;
    pair_op_rdb_update();

    return 0;
}

/*
 * Unpair a device. The Bluez API doesn't actually provide a way to just
 * unpair a device. Unpairing is done by removing the device completely.
 */
int
bz_unpair (char *address)
{
    bz_device_t *device;
    GVariant *reply;
    GError *error = NULL;

    dbgp("address: %s\n", address);

    device = device_get(address, NULL);
    if (!device) {
        dbgp("Invalid device\n");
        return (-ENOENT);
    }

    if (strcmp(device->properties[PROP_DEV_PAIRED], "true")) {
        dbgp("Device not paired\n");
        return (-EALREADY);
    }

    reply = g_dbus_connection_call_sync(dbus_conn,
                                        BLUEZ_BUS_NAME,
                                        adapter_dbus_path,
                                        BLUEZ_ADAPTER_INTF,
                                        "RemoveDevice",
                                        g_variant_new("(&o)",
                                                      device->dbus_path),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (reply) {
        g_variant_unref(reply);
    }

    if (error) {
        return (-error->code);
    } else {
        return 0;
    }
}
