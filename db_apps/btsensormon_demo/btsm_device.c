/*
 * Bluetooth sensor monitor (btsensormon) device handling.
 *
 * This monitor runs as a daemon. It manages connecting to and
 * collecting data from bluetooth sensor devices. Device client code can be
 * added to this monitor to handle specific devices.
 *
 *  Client code need only implement the APIS defined in btsm_client_t:
 *    - accept: This gets called by the monitor for each paired device.
 *              If the client returns true for a device then the client
 *              will be recorded as the handler for the device.
 *    - collect_poll: This gets periodically called to collect data from a
 *                    device. Only called if not NULL and the client has
 *                    accepted the device.
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
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <gio/gio.h>

#include "btsensormon.h"

/*
 * Finds a device that matches the target device.
 */
btsm_device_t *
device_find (btsm_context_t *ctx, btsm_device_t *target)
{
    btsm_device_t *dev = ctx->devices;

    while (dev && strcmp(dev->dbus_path, target->dbus_path)) {
        dev = dev->next;
    }

    return dev;
}

/*
 * Update a device with the given property values.
 */
void
device_update (btsm_device_t *device, GVariantIter *prop_iterator)
{
    char *prop_name;
    GVariant *prop_val;

    dbgp("Device %s properties changed\n", device->address);
    while (g_variant_iter_loop(prop_iterator, "{sv}", &prop_name, &prop_val)) {
        if (!strcmp(prop_name, "Connected")) {
            g_variant_get(prop_val, "b", &device->connected);
            dbgp("Connected prop = %d\n", device->connected);
        } else if (!strcmp(prop_name, "Paired")) {
            g_variant_get(prop_val, "b", &device->paired);
            dbgp("Paired prop = %d\n", device->paired);
        }
    }
}

/*
 * Add a new accepted device.
 */
static void
device_add (btsm_context_t *ctx, btsm_device_t *device,
            GDBusSignalCallback dev_prop_change_cb)
{
    device->next = ctx->devices;
    ctx->devices = device;
    ctx->num_devices++;

    /* Subscribe to changes in the device properties */
    device->property_watch_id =
        g_dbus_connection_signal_subscribe(ctx->connection,
                                           BLUEZ_BUS_NAME,
                                           DBUS_PROPERTIES_INTF,
                                           "PropertiesChanged",
                                           device->dbus_path,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           dev_prop_change_cb,
                                           ctx,
                                           NULL);
}

/*
 * Check whether a device should be accepted. Only accepted devices will
 * be connected to and data collected from.
 */
static bool
device_accept (btsm_context_t *ctx, btsm_device_t *device)
{
    btsm_client_t *client;
    int ix;
    int num_dev = 0;
    int rval;
    char *dev_address;
    int dev_address_len;
    char rdb_var_name[MAX_NAME_LENGTH];
    char rdb_val[MAX_RDB_BT_VAL_LEN];

    if (device_find(ctx, device)) {
        /* Device already previously accepted. */
        return false;
    }

    ix = 0;
    while ((client = ctx->clients[ix++])) {
        if (client->accept(device)) {
            break;
        }
    }

    if (client) {
        dbgp("Device %s (%s, type %d) accepted\n", device->address,
             device->description, device->type);

        rval = rdb_get_int(ctx->rdb_s, RDB_VAR_BT_SENSOR_NUM_DEV, &num_dev);
        if (rval && (rval != -ENOENT)) {
            errp("Unable to get number of devices\n");
            return false;
        }

        dev_address = NULL;
        dev_address_len = 0;
        for (ix = 0; ix < num_dev; ix++) {
            MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), ix,
                              RDB_VAR_BT_SENSOR_DEV_ADDR);
            rval = rdb_get_alloc(ctx->rdb_s, rdb_var_name, &dev_address,
                                 &dev_address_len);

            if (!strcmp(dev_address, device->address)) {
                /* RDB device tree already created. */
                device->index = ix;
                dbgp("Device found at index %d, skip rdb dev tree creation\n",
                     ix);
                break;
            }
        }
        free(dev_address);

        if (ix == num_dev) {
            /* Need to create new RDB device tree. */
            dbgp("Create rdb dev tree\n");
            MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), num_dev,
                              RDB_VAR_BT_SENSOR_DEV_ADDR);
            rdb_update_string(ctx->rdb_s, rdb_var_name, device->address,
                              RDB_CREATE_FLAGS, 0);

            MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), num_dev,
                              RDB_VAR_BT_SENSOR_DEV_TYPE);
            snprintf(rdb_val, sizeof(rdb_val), "%d", device->type);
            rdb_update_string(ctx->rdb_s, rdb_var_name, rdb_val,
                              RDB_CREATE_FLAGS, 0);

            MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), num_dev,
                              RDB_VAR_BT_SENSOR_DEV_DESCRIPTION);
            rdb_update_string(ctx->rdb_s, rdb_var_name, device->description,
                              RDB_CREATE_FLAGS, 0);

            MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), num_dev,
                              RDB_VAR_BT_SENSOR_DEV_DATA_COUNT);
            rdb_update_string(ctx->rdb_s, rdb_var_name, "0",
                              RDB_CREATE_FLAGS, 0);

            MK_SENSOR_DEV_VAR(rdb_var_name, sizeof(rdb_var_name), num_dev,
                              RDB_VAR_BT_SENSOR_DEV_DATA_START_IDX);
            rdb_update_string(ctx->rdb_s, rdb_var_name, "0",
                              RDB_CREATE_FLAGS, 0);

            num_dev++;
            snprintf(rdb_val, sizeof(rdb_val), "%d", num_dev);
            rdb_update_string(ctx->rdb_s, RDB_VAR_BT_SENSOR_NUM_DEV, rdb_val,
                              RDB_CREATE_FLAGS, 0);
        }

        device->client = client;
        return true;
    } else {
        return false;
    }
}

/*
 * Parse a dbus bluetooth device at the given dbus_path and with the
 * given properties.
 */
static void
parse_bluetooth_device (btsm_context_t *ctx, const char *dbus_path,
                        GVariant *properties,
                        GDBusSignalCallback dev_prop_change_cb)
{
    int rval = 0;
    btsm_device_t *device;

    device = calloc(1, sizeof *device);
    if (!device) {
        bail(ctx, -errno, "Failed to alloc memory for device\n");
    }
    device->state = BTSM_STATE_IDLE;
    device->ctx = ctx;

    /*
     * num_devices will be incremented in device_add only if the
     * device is accepted below.
     */
    device->index = ctx->num_devices;

    device->dbus_path = strdup(dbus_path);
    if (!device->dbus_path) {
        free(device);
        bail(ctx, -errno, "Failed to alloc memory for deviced bus_path\n");
    }

    if (!g_variant_lookup(properties, "Address", "s", &device->address)) {
        errp("Unable to get device %s Address\n", dbus_path);
        rval = -1;
        goto done;
    }

    if (!g_variant_lookup(properties, "Connected", "b", &device->connected)) {
        errp("Unable to get device %s Connected\n", dbus_path);
        rval = -1;
        goto done;
    }

    if (!g_variant_lookup(properties, "Paired", "b", &device->paired)) {
        errp("Unable to get device %s Paired\n", dbus_path);
        rval = -1;
        goto done;
    }

    /* Name is an optional parameter so not an error if this lookup fails. */
    g_variant_lookup(properties, "Name", "s", &device->name);

    dbgp("Device parsed: name=%s, address=%s, connected=%d, paired=%d\n",
         device->name ? device->name : device->address, device->address,
         device->connected, device->paired);

 done:
    if (rval || !device_accept(ctx, device)) {
        free(device);
    } else {
        device_add(ctx, device, dev_prop_change_cb);
    }
}

/*
 * Parse a single bluez object.
 */
void
parse_bluez_single_object (btsm_context_t *ctx, char *object_path,
                           GVariant *ifaces_and_props,
                           GDBusSignalCallback dev_prop_change_cb)
{
    GVariantIter interface_iter;
    char *interface_name;
    GVariant *properties;

    g_variant_iter_init(&interface_iter, ifaces_and_props);

    /*
     * Extract out the interface name and interface properties. If
     * the interface is one of interest (Device1) then send it off
     * to the relevent parse function.
     */
    while (g_variant_iter_loop(&interface_iter, "{s*}", &interface_name,
                               &properties)) {
        if (!strcmp(interface_name, BLUEZ_DEVICE_INTF)) {
            parse_bluetooth_device(ctx, object_path, properties,
                                   dev_prop_change_cb);
        }
    }
}

/*
 * Retrieves all bluez dbus objects and parses the ones of interest.
 */
void
parse_bluez_objects (btsm_context_t *ctx,
                     GDBusSignalCallback dev_prop_change_cb)
{
    GError *error = NULL;
    GVariant *reply;
    GVariantIter giter;
    GVariant *objs;
    char *object_path;
    GVariant *ifaces_and_props;

    /*
     * Get all the bluez objects. The dbus GetManagedObjects API is
     * defined as:
     *    GetManagedObjects(out DICT<OBJPATH,
     *                                DICT<STRING,DICT<STRING,VARIANT>>>
     *                      objpath_interfaces_and_properties);
     */
    reply = g_dbus_connection_call_sync(ctx->connection,
                                        BLUEZ_BUS_NAME,
                                        "/",
                                        DBUS_OBJ_MGR_INTF,
                                        "GetManagedObjects",
                                        NULL,
                                        G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (error) {
        bail(ctx, -1, "Unable to get bluez objects: %s\n", error->message);
    }

    g_variant_get(reply, "(*)", &objs);
    g_variant_iter_init(&giter, objs);
    while (g_variant_iter_loop(&giter, "{o*}", &object_path,
                               &ifaces_and_props)) {
        parse_bluez_single_object(ctx, object_path, ifaces_and_props,
                                  dev_prop_change_cb);
    }

    g_variant_unref(reply);
    g_variant_unref(objs);
}
