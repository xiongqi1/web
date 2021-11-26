/*
 * dsm_bluetooth_device.c
 *    Data Stream Bluetooth Device related functions.
 *    This module deals with most of device and stream stuff.
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
#include "dsm_bt_utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <gio/gio.h>

/* Forward declarations */
int parse_gatt_service(dsm_btep_context_t *ctx, const char *dbus_path,
                       GVariant *properties);
int parse_gatt_characteristic(dsm_btep_context_t *ctx, const char *dbus_path,
                              GVariant *properties);
int parse_gatt_descriptor(dsm_btep_context_t *ctx, const char *dbus_path,
                          GVariant *properties);

/*
 * Characteristic properties values.
 * It includes both basic properties (BT spec Table 3.5) as LSB (bit 0-7) and
 * extended properties (BT spec Table 3.8) as MSB (bit 8-23).
 */
static const unsigned int characteristic_properties_values[] = {
    0x01,
    0x02,
    0x04,
    0x08,
    0x10,
    0x20,
    0x40,
    0x000100,
    0x000200
};
#define N_CHAR_PROP_VALUES (sizeof(characteristic_properties_values) / \
                            sizeof(characteristic_properties_values[0]))

/*
 * Characteristic properties string names as in characteristic property Flags.
 * cf. https://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/gatt-api.txt
 * This must be in the same order as characteristic_properties_values.
 */
static const char * characteristic_properties_names[] = {
    "broadcast",                         /* 0x01 */
    "read",                              /* 0x02 */
    "write-without-response",            /* 0x04 */
    "write",                             /* 0x08 */
    "notify",                            /* 0x10 */
    "indicate",                          /* 0x20 */
    "authenticated-signed-writes",       /* 0x40 */
    "reliable-write",                    /* 0x0001 */
    "writable-auxiliaries",              /* 0x0002 */
    "encrypt-read",                      /* Unspecified by spec */
    "encrypt-write",
    "encrypt-authenticated-read",
    "encrypt-authenticated-write"
};
#define N_CHAR_PROP_NAMES (sizeof(characteristic_properties_names) / \
                           sizeof(characteristic_properties_names[0]))

/* Find the index of a string str to array */
static int
find_string (const char *array[], int n, const char *str)
{
    int ix;
    for (ix = 0; ix < n; ix ++) {
        if (!strcmp(array[ix], str)) {
            return ix;
        }
    }
    return -1;
}

/* Convert flags (GVariant "as") to an integer bitmap */
static unsigned int
flags_string_to_int (GVariant *flags)
{
    gchar *name;
    int ix;
    unsigned int result = 0;
    GVariantIter *iter;

    g_variant_get(flags, "as", &iter);
    if (!iter) {
        return 0;
    }
    while (g_variant_iter_loop(iter, "s", &name)) {
        ix = find_string(characteristic_properties_names,
                         N_CHAR_PROP_NAMES,
                         name);
        if (ix >= 0 && ix < N_CHAR_PROP_VALUES) {
            result |= characteristic_properties_values[ix];
        }
    }
    g_variant_iter_free(iter);
    return result;
}

/*
 * Update attribute struct's value with the given value (GVariant "ay").
 * Params:
 *     attribute - a pointer to either a characteristic or descriptor
 *     type - 0: characteristic; 1: descriptor
 *     value - a GVariant pointer with type "ay"
 * Return:
 *     0 on success
 *     negative error code on failure
 */
static int
update_attr_val (void *attribute, unsigned char type, GVariant *value)
{
    int ix = 0;
    unsigned int len;
    GVariantIter *iter;
    dsm_gatt_characteristic_t *characteristic;
    dsm_gatt_descriptor_t *descriptor;
    char **pvalue;
    int *pvalue_len;
    int *pvalue_capacity;

    if (!attribute || !value) {
        dbgp("NULL parameter\n");
        return -EINVAL;
    }

    g_variant_get(value, "ay", &iter);
    if (!iter) {
        dbgp("Failed to get iterator of byte array\n");
        return -EINVAL;
    }

    if (!type) {
        characteristic = attribute;
        pvalue = &characteristic->value;
        pvalue_len = &characteristic->value_len;
        pvalue_capacity = &characteristic->value_capacity;
    } else {
        descriptor = attribute;
        pvalue = &descriptor->value;
        pvalue_len = &descriptor->value_len;
        pvalue_capacity = &descriptor->value_capacity;
    }

    len = g_variant_iter_n_children(iter);
    if (len && len > *pvalue_capacity)
    {
        if (*pvalue) {
            free(*pvalue);
        }
        if (!(*pvalue = malloc(len))) {
            errp("Failed to allocate for attribute value\n");
            g_variant_iter_free(iter);
            *pvalue_capacity = 0;
            *pvalue_len = 0;
            return -ENOMEM;
        }
        *pvalue_capacity = len;
    }
    *pvalue_len = len;

    if (len) {
        while (g_variant_iter_loop(iter, "y", &(*pvalue)[ix++]));
    }

    g_variant_iter_free(iter);
    return 0;
}

/*
 * Update characteristic->value with the given value (GVariant "ay").
 * Params:
 *     characteristic - a pointer to the characteristic
 *     value - a GVariant pointer with type "ay"
 * Return:
 *     0 on success
 *     negative error code on failure
 */
int
update_char_val (dsm_gatt_characteristic_t *characteristic, GVariant *value)
{
    return update_attr_val(characteristic, 0, value);
}

/*
 * Update descriptor->value with the given value.
 * Params:
 *     descriptor - a pointer to the descriptor
 *     value - a GVariant pointer with type "ay"
 * Return:
 *     0 on success
 *     negative error code on failure
 */
int
update_desc_val (dsm_gatt_descriptor_t *descriptor, GVariant *value)
{
    return update_attr_val(descriptor, 1, value);
}

/*
 * Simply add device to the head of ctx->devices list.
 * No checking for duplicate entries.
 */
static void
device_add (dsm_btep_context_t *ctx, dsm_btep_device_t *device)
{
    device->next = ctx->devices;
    ctx->devices = device;
}

/*
 * Find a device from ctx->devices that matches the dbus_path
 */
dsm_btep_device_t *
device_find (dsm_btep_context_t *ctx, const char *dbus_path)
{
    dsm_btep_device_t *dev = ctx->devices;

    while (dev && strcmp(dev->dbus_path, dbus_path)) {
        dev = dev->next;
    }

    return dev;
}

/*
 * Remove the device from ctx->devices that matches dbus_path.
 * The removed device is returned to the caller who is responsible
 * for releasing the memory. NULL is returned if not found.
 */
dsm_btep_device_t *
device_remove (dsm_btep_context_t *ctx, const char *dbus_path)
{
    dsm_btep_device_t *dev, *prev = NULL;
    for (dev = ctx->devices; dev; dev = dev->next) {
        if (!strcmp(dev->dbus_path, dbus_path)) {
            break;
        }
        prev = dev;
    }
    if (dev) { /* device found */
        if (prev) {
            prev->next = dev->next;
        } else { /* target is the first device */
            ctx->devices = dev->next;
        }
        dev->next = NULL;
    }
    return dev;
}

/*
 * Match a device against the rule in a stream.
 * If matched, a device-stream association will be appended to ctx->associations
 * with connection timer started and return TRUE;
 * otherwise, return FALSE.
 * Note: existing associations are not touched and no duplicate detection
 * is carried out.
 * It it important that the caller makes sure that the given stream and device
 * have been/will be included in ctx->streams/devices.
 */
static bool
device_match_stream (dsm_btep_stream_t *stream, dsm_btep_device_t *device)
{
    dsm_btep_association_t *assoc;
    const char *property;
    dsm_btep_context_t * ctx = device->ctx;

    switch (stream->bt_ep.rule_property) {
    case (PROPERTY_NAME):
        property = device->name;
        break;
    case (PROPERTY_ADDRESS):
        property = device->address;
        break;
    default:
        errp("Invalid stream btep rule property (%d)",
             stream->bt_ep.rule_property);
        property = NULL;
        break;
    }

    if (!property) {
        return FALSE;
    }

    if (stream->bt_ep.rule_func(property, stream->bt_ep.rule_val,
                                stream->bt_ep.rule_negate)) {
        /* construct the device-stream association */
        assoc = calloc(1, sizeof *assoc);
        if (assoc) {
            assoc->ctx = ctx;
            assoc->stream = stream;
            assoc->device = device;
            if (device->paired) {
                assoc->conn_timer =
                    g_timeout_add_seconds(stream->bt_ep.conn_fail_retry,
                                          connect_timer_handler,
                                          assoc);
                if (assoc->conn_timer) {
                    assoc->next = ctx->associations;
                    ctx->associations =  assoc;
                    dbgp("Assoc added %s->%s\n", device->address,
                         stream->other_ep.exec_ep.exec_name);
                    return TRUE;
                } else {
                    errp("Failed to start connection timer\n");
                    free(assoc);
                }
            } else {
                /*
                 * Matched but not paired yet.
                 * In this case, we still have to add the association
                 * but without starting the conn_timer, so that when
                 * it is later paired, we simply start conn_timer.
                 */
                assoc->next = ctx->associations;
                ctx->associations = assoc;
                dbgp("Assoc added %s->%s\n", device->address,
                     stream->other_ep.exec_ep.exec_name);
                return TRUE;
            }
        } else {
            errp("Failed to allocate association\n");
        }
    }

    return FALSE;
}

static void
association_destroy(dsm_btep_association_t *association)
{
    if (!association) {
        return;
    }
    if (association->conn_timer) {
        /* stop the timer so it will not access non existing device */
        g_source_remove(association->conn_timer);
        association->conn_timer = 0;
    }
    free(association);
}

/*
 * clear associations that involve the given device
 */
static void
clear_associations_by_device (dsm_btep_device_t *device)
{
    dsm_btep_context_t *ctx = device->ctx;
    dsm_btep_association_t *assoc, *next_assoc, *prev_assoc = NULL;
    for (assoc = ctx->associations; assoc; assoc = next_assoc) {
        next_assoc = assoc->next;
        if (!strcmp(assoc->device->dbus_path, device->dbus_path)) {
            if (prev_assoc) {
                prev_assoc->next = next_assoc;
            } else {
                ctx->associations = next_assoc;
            }
            /* be careful of destroyed stream */
            dbgp("Destroy assoc %s->%s\n", device->address,
                 assoc->stream->other_ep.exec_ep.exec_name);
            association_destroy(assoc);
        } else {
            prev_assoc = assoc;
        }
    }
}

/*
 * Match device against bt_ep rule of all streams in ctx->streams.
 * For each matched stream an association will be added to ctx->associations.
 * Existing stream associations with the device will be cleared first.
 * Return true if at least one stream is matched, false otherwise.
 */
bool
device_match_all_streams (dsm_btep_context_t *ctx, dsm_btep_device_t *device)
{
    bool matched = FALSE;
    dsm_btep_stream_t *stream;

     /* clear existing stream associations */
    clear_associations_by_device(device);
    for (stream = ctx->streams; stream; stream = stream->next) {
        if (device_match_stream(stream, device)) {
            matched = TRUE;
        }
    }

    return matched;
}


/*
 * Release internal memory used by the given bt endpoint, but not itself.
 */
static void
bt_endpoint_clear (dsm_bt_endpoint_t *ep)
{
    if (!ep) {
        return;
    }
    if (ep->uuid) {
        free(ep->uuid);
        ep->uuid = NULL;
    }
    if (ep->rule_val) {
        free(ep->rule_val);
        ep->rule_val = NULL;
    }
}

/*
 * Release memory of the given stream.
 */
void
stream_destroy (dsm_btep_stream_t *stream)
{
    if (!stream) {
        return;
    }
#ifdef DEBUG
    snprint_stream(dbg_buf, sizeof dbg_buf, stream);
    dbgp("Destroy stream %s\n", dbg_buf);
#endif
    bt_endpoint_clear(&stream->bt_ep);
    free(stream);
}

static void
descriptor_destroy (dsm_gatt_descriptor_t *descriptor)
{
    if (!descriptor) {
        return;
    }
    if (descriptor->dbus_path) {
        free(descriptor->dbus_path);
    }
    if (descriptor->uuid) {
        free(descriptor->uuid);
    }
    if (descriptor->value) {
        free(descriptor->value);
    }
    free(descriptor);
}

static void
characteristic_destroy (dsm_gatt_characteristic_t *characteristic)
{
    dsm_gatt_descriptor_t *descriptor, *next_desc;
    dsm_btep_context_t *ctx;
    if (!characteristic) {
        return;
    }
    if (characteristic->dbus_path) {
        free(characteristic->dbus_path);
    }
    if (characteristic->uuid) {
        free(characteristic->uuid);
    }
    if (characteristic->value) {
        free(characteristic->value);
    }
    if (characteristic->property_watch_id) {
        ctx = characteristic->parent->parent->ctx;
        g_dbus_connection_signal_unsubscribe(ctx->connection,
                                             characteristic->property_watch_id);
    }
    for (descriptor = characteristic->descriptors; descriptor;
         descriptor = next_desc) {
        next_desc = descriptor->next;
        descriptor_destroy(descriptor);
    }
    free(characteristic);
}

static void
service_destroy (dsm_gatt_service_t *service)
{
    dsm_gatt_characteristic_t *characteristic, *next_char;

    if (!service) {
        return;
    }

    if (service->dbus_path) {
        free(service->dbus_path);
    }
    if (service->uuid) {
        free(service->uuid);
    }

    for (characteristic = service->characteristics; characteristic;
         characteristic = next_char) {
        next_char = characteristic->next;
        characteristic_destroy(characteristic);
    }
    free(service);
}

/*
 * Release all resources related to the given device.
 * Includes: unsubscribe from properties changed signal
 *           clear related associations
 *           destroy related services
 *           free memory
 */
void
device_destroy (dsm_btep_device_t *device)
{
    dsm_gatt_service_t *service, *next_service;

    if (!device) {
        return;
    }
    dbgp("Destroy device (%s,%s)\n", device->address, device->name);

    /* delete rdb variables tree for this device */
    delete_device_rdb_tree(device->ctx, device);

    if (device->property_watch_id) {
        g_dbus_connection_signal_unsubscribe(device->ctx->connection,
                                             device->property_watch_id);
    }
    clear_associations_by_device(device);
    for (service = device->services; service; service = next_service) {
        next_service = service->next;
        service_destroy(service);
    }

    if (device->fd > 0) {
        close(device->fd);
    }

    free(device);
}

/*
 * Parse a dbus bluetooth device at the given dbus_path and with the
 * given properties.
 * Create and add the device to ctx->devices.
 * Existing device in ctx->devices will be removed first.
 * The device will be matched against rules in ctx->streams and matched
 * stream(s) will be recorded in ctx->associations.
 * Connection timer for the association will be started if there is a match.
 */
static void
parse_bluetooth_device (dsm_btep_context_t *ctx, const char *dbus_path,
                        GVariant *properties)
{
    int rval = 0;
    dsm_btep_device_t *device, *existing_dev;
    char *name = NULL, *address = NULL;
    int existing_dev_index = -1;

    device = calloc(1, sizeof *device);
    if (!device) {
        errp("Failed to alloc memory for device\n");
        return;
    }
    device->fd = -1;
    device->ctx = ctx;
    device->classic_state = device->ble_state = DSM_BTEP_STATE_IDLE;

    strncpy(device->dbus_path, dbus_path, sizeof(device->dbus_path));
    if (device->dbus_path[sizeof(device->dbus_path) - 1]) {
        /* last char should be NULL, otherwise, overlength is detected */
        errp("bus_path %s is too long\n", dbus_path);
        rval = -1;
        goto done;
    }

    if (!g_variant_lookup(properties, "Address", "s", &address)) {
        errp("Unable to get Address of device %s\n", dbus_path);
        rval = -1;
        goto done;
    }
    strncpy(device->address, address, sizeof(device->address));
    if (device->address[sizeof(device->address) - 1]) { /* check overlength */
        errp("Device address %s is too long\n", address);
        rval = -1;
        goto done;
    }

    /*
     * Note: The Connected and Paired properties are independent. Either one
     * can be 1 or 0 regardless of the other. Implementation should not rely
     * on any dependency.
     */
    if (!g_variant_lookup(properties, "Connected", "b", &device->connected)) {
        errp("Unable to get Connected of device %s\n", dbus_path);
        rval = -1;
        goto done;
    }

    if (!g_variant_lookup(properties, "Paired", "b", &device->paired)) {
        errp("Unable to get Paired of device %s\n", dbus_path);
        rval = -1;
        goto done;
    }

    /* Name is an optional parameter so not an error if this lookup fails. */
    if (!g_variant_lookup(properties, "Name", "s", &name)) {
        strncpy(device->name, device->address, sizeof(device->name));
    } else {
        strncpy(device->name, name, sizeof(device->name));
        if (device->name[sizeof(device->name) - 1]) {
            errp("Device name %s is too long\n", name);
            rval = -1;
            goto done;
        }
    }

    /* Existing device will be removed first in case its properties changed */
    if ((existing_dev = device_remove(ctx, device->dbus_path))) {
        dbgp("Existing device (%s, %s) idx %d. Destroying\n",
             device->address, device->name, existing_dev->index);
        /* reuse index if the same device just changed properties */
        existing_dev_index = existing_dev->index;
        device_destroy(existing_dev);
    }

    if (device_match_all_streams(ctx, device)) {
        device->index = (existing_dev_index >= 0) ? existing_dev_index :
                                                    ctx->next_device_index++;
    } else {
        device->index = -1; /* marked as not yet matching any stream */
    }

    /* Subscribe to changes in the device properties */
    device->property_watch_id =
        g_dbus_connection_signal_subscribe(ctx->connection,
                                           BLUEZ_BUS_NAME,
                                           DBUS_PROPERTIES_INTF,
                                           "PropertiesChanged",
                                           dbus_path,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           dev_properties_changed,
                                           device,
                                           NULL);
    if (!device->property_watch_id) {
        errp("Failed to watch for device properties changes\n");
        goto done;
    }

    /*
     * Add the device to ctx->devices even if no matching, so that we do not
     * need to retrieve all devices from ObjectManager each time a new stream
     * is added. device->index reflects if it ever matched any stream.
     */
    device_add(ctx, device);

#ifdef DEBUG
    snprint_device(dbg_buf, sizeof dbg_buf, device);
    dbgp("Device added: %s\n", dbg_buf);
#endif

done:
    if (name) {
        g_free(name);
    }
    if (address) {
        g_free(address);
    }
    if (rval) {
        free(device);
    }
}

/*
 * Parse a single bluez object given by object_path
 */
void
parse_bluez_single_object (dsm_btep_context_t *ctx, const char *object_path,
                           GVariant *ifaces_and_props)
{
    GVariantIter interface_iter;
    char *interface_name;
    GVariant *properties;

    g_variant_iter_init(&interface_iter, ifaces_and_props);

    /*
     * Extract out the interface name and interface properties. If
     * the interface is one of interest then send it off
     * to the relevent parse function.
     */
    while (g_variant_iter_loop(&interface_iter, "{s*}", &interface_name,
                               &properties)) {
        if (!strcmp(interface_name, BLUEZ_DEVICE_INTF)) {
            parse_bluetooth_device(ctx, object_path, properties);
        } else if (!strcmp(interface_name, BLUEZ_GATT_SERVICE_INTF)) {
            parse_gatt_service(ctx, object_path, properties);
        } else if (!strcmp(interface_name, BLUEZ_GATT_CHARACTERISTIC_INTF)) {
            parse_gatt_characteristic(ctx, object_path, properties);
        } else if (!strcmp(interface_name, BLUEZ_GATT_DESCRIPTOR_INTF)) {
            parse_gatt_descriptor(ctx, object_path, properties);
        }
    }
}

/*
 * Retrieves all bluez dbus objects from ObjectManager and parses the ones
 * of interest and added to ctx->devices.
 */
void
parse_bluez_objects (dsm_btep_context_t *ctx)
{
    GError *error = NULL;
    GVariant *reply;
    GVariantIter iter;
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
    if (!objs) {
        bail(ctx, -1, "Failed to get objects\n");
    }
    g_variant_iter_init(&iter, objs);
    while (g_variant_iter_loop(&iter, "{o*}", &object_path,
                               &ifaces_and_props)) {
        parse_bluez_single_object(ctx, object_path, ifaces_and_props);
    }

    g_variant_unref(objs);
    g_variant_unref(reply);
}

/*
 * Match all devices in ctx with the given stream and add corresponding
 * associations for matches.
 * Note: caller must make sure that stream is already in ctx->streams.
 */
void
dsm_btep_match_all_devices_with_stream (dsm_btep_context_t *ctx,
                                        dsm_btep_stream_t *stream)
{
    dsm_btep_device_t *dev;
    for (dev = ctx->devices; dev; dev = dev->next) {
        if (device_match_stream(stream, dev) && dev->index < 0) {
            dev->index = ctx->next_device_index++;
        }
    }
}

static bool
bt_ep_equal (dsm_bt_endpoint_t *btep1, dsm_bt_endpoint_t *btep2)
{
    if (btep1 == btep2) {
        return TRUE;
    }
    if (!btep1 || !btep2) {
        return FALSE;
    }

    if (btep1->uuid != btep2->uuid) {
        if (!btep1->uuid || !btep2->uuid) {
            return FALSE;
        }
        if (strcmp(btep1->uuid, btep2->uuid)) {
            return FALSE;
        }
    }

    if (btep1->rule_func != btep2->rule_func) {
        return FALSE;
    }
    if (btep1->rule_negate != btep2->rule_negate) {
        return FALSE;
    }
    if (btep1->rule_property != btep2->rule_property) {
        return FALSE;
    }
    if (btep1->rule_val != btep2->rule_val) {
        if (!btep1->rule_val || !btep2->rule_val) {
            return FALSE;
        }
        if (strcmp(btep1->rule_val, btep2->rule_val)) {
            return FALSE;
        }
    }
    if (btep1->conn_fail_retry != btep2->conn_fail_retry) {
        return FALSE;
    }
    if (btep1->conn_success_retry != btep2->conn_success_retry) {
        return FALSE;
    }
    return TRUE;
}

/* check if two streams are equal based on contents */
static bool
stream_equal (dsm_btep_stream_t *stream1, dsm_btep_stream_t *stream2)
{
    if (stream1 == stream2) {
        return TRUE;
    }
    if (!stream1 || !stream2) {
        return FALSE;
    }
    if (!bt_ep_equal(&stream1->bt_ep, &stream2->bt_ep)) {
        return FALSE;
    }
    if (stream1->other_ep_type != stream2->other_ep_type) {
        return FALSE;
    }
    switch(stream1->other_ep_type) {
    case EP_GENERIC_EXEC:
        if (strcmp(stream1->other_ep.exec_ep.exec_name,
                   stream2->other_ep.exec_ep.exec_name)) {
            return FALSE;
        }
        break;
    default:
        errp("Unknown other endpoint type %d\n", stream1->other_ep_type);
        return FALSE;
    }
    return TRUE;
}

/*
 * Find a stream in ctx->streams that matches the given stream (by content).
 * Return the found stream or NULL if not found.
 */
dsm_btep_stream_t *
stream_find (dsm_btep_context_t *ctx, dsm_btep_stream_t *stream)
{
    dsm_btep_stream_t *stm;
    for (stm = ctx->streams; stm; stm = stm->next) {
        if (stream_equal(stm, stream)) {
            break;
        }
    }
    return stm;
}

static bool
rule_is (const char *input_val, const char *rule_val, bool negate)
{
    bool result = (strcmp(input_val, rule_val) == 0);

    return (negate ? !result : result);
}

static bool
rule_contains (const char *input_val, const char *rule_val, bool negate)
{
    bool result = (strstr(input_val, rule_val) != NULL);

    return (negate ? !result : result);
}

static  bool
rule_starts_with (const char *input_val, const char *rule_val, bool negate)
{
    bool result = (strstr(input_val, rule_val) == input_val);

    return (negate ? !result : result);
}

static bool
rule_ends_with (const char *input_val, const char *rule_val, bool negate)
{
    bool result = (strstr(input_val, rule_val) ==
                   (input_val + strlen(input_val) - strlen(rule_val)));

    return (negate ? !result : result);
}

static dsm_btep_rule_fn_t g_rule_funcs[] = {
    rule_is, rule_contains, rule_starts_with, rule_ends_with
};

#define STREAM_NUM_EP 2
#define EPA_IDX 0
#define EPB_IDX 1

/*
 * Construct a stream based on given argv and the corresponding rdb variables.
 * Params:
 *     rdb_s [in]: a pointer to rdb session
 *     argv [in]: array of four strings containing in the following sequence
 *                {epa_rdb_root, epa_type, epb_rdb_root, epb_type}
 *     ppstream [out]: pointer to pointer of stream.
 *                     On success, a dsm_btep_stream_t will be allocated
 *                     with ppstream pointing to it, and the caller is
 *                     responsible for freeing the memory (using free).
 *                     On failure, ppstream will point to NULL.
 * Return:
 *     0 for success; non-zero for failure.
 */
int
construct_stream (struct rdb_session *rdb_s, char *argv[],
                  dsm_btep_stream_t **ppstream)
{
    struct {
        const char *rdb_prefix;
        int type;
    } ep_arg[STREAM_NUM_EP] = {
        { argv[0], atoi(argv[1]) },
        { argv[2], atoi(argv[3]) },
    }, *ep_arg_bt = NULL, *ep_arg_other = NULL;

    dsm_btep_stream_t *stream;
    int rval = 0;
    char rdb_var_name[MAX_NAME_LENGTH];
    int int_val;
    int len;

    /*
     * Work out which is the BT endpoint args and which is the other endpoint
     * args. For easier reference in subsequent code.
     */
    if (ep_arg[EPA_IDX].type == EP_BT_SPP || ep_arg[EPA_IDX].type == EP_BT_GC) {
        ep_arg_bt = &ep_arg[EPA_IDX];
        ep_arg_other = &ep_arg[EPB_IDX];
    } else if (ep_arg[EPB_IDX].type == EP_BT_SPP ||
               ep_arg[EPB_IDX].type == EP_BT_GC) {
        ep_arg_bt = &ep_arg[EPB_IDX];
        ep_arg_other = &ep_arg[EPA_IDX];
    }

    CHECK_COND(ep_arg_bt, -1,
               "Neither endpoint is a Bluetooth endpoint (%d, %d)",
               ep_arg[EPA_IDX].type, ep_arg[EPB_IDX].type);

    stream = calloc(1, sizeof *stream);
    CHECK_COND(stream, -errno, "Unable to allocate stream");

    if (ep_arg_bt->type == EP_BT_SPP) { /* only spp ep has uuid */
        snprintf(rdb_var_name, sizeof rdb_var_name, "%s.uuid",
                 ep_arg_bt->rdb_prefix);
        len = 0;
        INVOKE_CLEAN(rdb_get_alloc(rdb_s, rdb_var_name,
                                   &stream->bt_ep.uuid, &len),
                     "Unable to read uuid");
    }

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.rule_operator",
             ep_arg_bt->rdb_prefix);
    INVOKE_CLEAN(rdb_get_int(rdb_s, rdb_var_name, &int_val),
                 "Unable to read rule_operator");
    rval = -1;
    CHECK_CLEAN(int_val >= 0 && int_val < OPERATOR_INVALID,
                "Invalid rule operator (%d)\n", int_val);
    stream->bt_ep.rule_func = g_rule_funcs[int_val];

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.rule_negate",
             ep_arg_bt->rdb_prefix);
    INVOKE_CLEAN(rdb_get_int(rdb_s, rdb_var_name, &int_val),
                 "Unable to read rule_negate\n");
    rval = -1;
    CHECK_CLEAN(int_val == 0 || int_val == 1,
                "Invalid rule_negate (%d)\n", int_val);
    stream->bt_ep.rule_negate = !!int_val;

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.rule_property",
             ep_arg_bt->rdb_prefix);
    INVOKE_CLEAN(rdb_get_int(rdb_s, rdb_var_name,
                             (int *)&stream->bt_ep.rule_property),
                 "Unable to read rule_property");
    rval = -1;
    CHECK_CLEAN(stream->bt_ep.rule_property < PROPERTY_INVALID,
                "Invalid rule property (%d)", stream->bt_ep.rule_property);

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.rule_value",
             ep_arg_bt->rdb_prefix);
    len = 0;
    INVOKE_CLEAN(rdb_get_alloc(rdb_s, rdb_var_name,
                               &stream->bt_ep.rule_val, &len),
                 "Unable to read rule_property");

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.conn_fail_retry",
             ep_arg_bt->rdb_prefix);
    INVOKE_CLEAN(rdb_get_int(rdb_s, rdb_var_name, &int_val),
                 "Unable to read conn_fail_retry");
    rval = -1;
    CHECK_CLEAN(int_val >= BT_EP_FAIL_RETRY_MIN &&
                int_val <= BT_EP_FAIL_RETRY_MAX,
                "Invalid conn_fail_retry (%d)", int_val);
    stream->bt_ep.conn_fail_retry = int_val;

    snprintf(rdb_var_name, sizeof rdb_var_name, "%s.conn_success_retry",
             ep_arg_bt->rdb_prefix);
    INVOKE_CLEAN(rdb_get_int(rdb_s, rdb_var_name, &int_val),
                 "Unable to read conn_success_retry");
    rval = -1;
    CHECK_CLEAN(int_val >= BT_EP_SUCCESS_RETRY_MIN &&
                int_val <= BT_EP_SUCCESS_RETRY_MAX,
                "Invalid conn_success_retry (%d)", int_val);
    stream->bt_ep.conn_success_retry = int_val;

    switch (ep_arg_bt->type) {
    case EP_BT_SPP:
    case EP_BT_GC:
        stream->bt_ep_type = ep_arg_bt->type;
        break;
    default:
        errp("Unsupported bluetooth endpoint type (%d)\n", ep_arg_bt->type);
        rval = -1;
        goto cleanup;
    }

    switch (ep_arg_other->type) {
    case EP_GENERIC_EXEC:
        stream->other_ep_type = EP_GENERIC_EXEC;
        snprintf(rdb_var_name, sizeof rdb_var_name, "%s.exec_name",
                 ep_arg_other->rdb_prefix);
        INVOKE_CLEAN(rdb_get_string(rdb_s, rdb_var_name,
                                    stream->other_ep.exec_ep.exec_name,
                                    sizeof(stream->other_ep.exec_ep.exec_name)
                         ),
                     "Unable to read exec_name");
        break;
    default:
        errp("Unsupported other endpoint type (%d)", ep_arg_other->type);
        rval = -1;
        goto cleanup;
    }

cleanup:
    if (rval) {
        free(stream);
        *ppstream = NULL;
    } else {
        *ppstream = stream;
#ifdef DEBUG
        snprint_stream(dbg_buf, sizeof dbg_buf, stream);
        dbgp("Stream constructed: %s", dbg_buf);
#endif
    }

    return rval;
}

static dsm_gatt_service_t *
service_find (dsm_btep_device_t *device, const char *dbus_path)
{
    dsm_gatt_service_t *service;
    if (!device) {
        return NULL;
    }
    for (service = device->services; service; service = service->next) {
        if (!strcmp(service->dbus_path, dbus_path)) {
            return service;
        }
    }
    return NULL;
}

static dsm_gatt_service_t *
service_find_from_ctx (dsm_btep_context_t *ctx, const char *dbus_path)
{
    dsm_btep_device_t *device;
    dsm_gatt_service_t *service;
    for (device = ctx->devices; device; device = device->next) {
        if ((service = service_find(device, dbus_path))) {
            return service;
        }
    }
    return NULL;
}

static dsm_gatt_characteristic_t *
characteristic_find (dsm_gatt_service_t *service, const char *dbus_path)
{
    dsm_gatt_characteristic_t *characteristic;
    if (!service) {
        return NULL;
    }
    for (characteristic = service->characteristics; characteristic;
         characteristic = characteristic->next) {
        if (!strcmp(characteristic->dbus_path, dbus_path)) {
            return characteristic;
        }
    }
    return NULL;
}

static dsm_gatt_characteristic_t *
characteristic_find_from_ctx (dsm_btep_context_t *ctx, const char *dbus_path)
{
    dsm_btep_device_t *device;
    dsm_gatt_service_t *service;
    dsm_gatt_characteristic_t *characteristic;
    for (device = ctx->devices; device; device = device->next) {
        for (service = device->services; service; service = service->next) {
            if ((characteristic = characteristic_find(service, dbus_path))) {
                return characteristic;
            }
        }
    }
    return NULL;
}

dsm_gatt_characteristic_t *
characteristic_find_by_idx (dsm_btep_context_t *ctx, int dev_idx, int srv_idx,
                            int char_idx)
{
    dsm_btep_device_t *device;
    dsm_gatt_service_t *service;
    dsm_gatt_characteristic_t *characteristic;
    for (device = ctx->devices; device; device = device->next) {
        if (device->index != dev_idx) {
            continue;
        }
        for (service = device->services; service; service = service->next) {
            if (service->index != srv_idx) {
                continue;
            }
            for (characteristic = service->characteristics; characteristic;
                 characteristic = characteristic->next) {
                if (characteristic->index == char_idx) {
                    return characteristic;
                }
            }
        }
    }
    return NULL;
}

static dsm_gatt_descriptor_t *
descriptor_find (dsm_gatt_characteristic_t *characteristic,
                 const char *dbus_path)
{
    dsm_gatt_descriptor_t *descriptor;
    if (!characteristic) {
        return NULL;
    }
    for (descriptor = characteristic->descriptors; descriptor;
         descriptor = descriptor->next) {
        if (!strcmp(descriptor->dbus_path, dbus_path)) {
            return descriptor;
        }
    }
    return NULL;
}

dsm_gatt_descriptor_t *
descriptor_find_by_idx (dsm_btep_context_t *ctx, int dev_idx, int srv_idx,
                        int char_idx, int desc_idx)
{
    dsm_btep_device_t *device;
    dsm_gatt_service_t *service;
    dsm_gatt_characteristic_t *characteristic;
    dsm_gatt_descriptor_t *descriptor;
    for (device = ctx->devices; device; device = device->next) {
        if (device->index != dev_idx) {
            continue;
        }
        for (service = device->services; service; service = service->next) {
            if (service->index != srv_idx) {
                continue;
            }
            for (characteristic = service->characteristics; characteristic;
                 characteristic = characteristic->next) {
                if (characteristic->index != char_idx) {
                    continue;
                }
                for (descriptor = characteristic->descriptors; descriptor;
                     descriptor = descriptor->next) {
                    if (descriptor->index == desc_idx) {
                        return descriptor;
                    }
                }
            }
        }
    }
    return NULL;
}

/*
 * Find an attribute (characteristic or descriptor) given its handle.
 * pchar is a pointer to hold the address of the found characteristic.
 * pdesc is a pointer to hold the address of the found descriptor.
 * Either pchar or pdesc can be NULL, in which case the search of the
 * corresponding attribute will be skipped.
 */
gboolean
attribute_find_by_handle (dsm_btep_context_t *ctx, int dev_idx, int handle,
                          dsm_gatt_characteristic_t **pchar,
                          dsm_gatt_descriptor_t **pdesc)
{
    dsm_btep_device_t *device;
    dsm_gatt_service_t *service;
    dsm_gatt_characteristic_t *characteristic;
    dsm_gatt_descriptor_t *descriptor;
    if (!pchar && !pdesc) {
        return FALSE;
    }
    if (pchar) {
        *pchar = NULL;
    }
    if (pdesc) {
        *pdesc = NULL;
    }
    for (device = ctx->devices; device; device = device->next) {
        if (device->index != dev_idx) {
            continue;
        }
        for (service = device->services; service; service = service->next) {
            for (characteristic = service->characteristics; characteristic;
                 characteristic = characteristic->next) {
                if (pchar && characteristic->handle == handle) {
                    *pchar = characteristic;
                    return TRUE;
                }
                if (!pdesc) {
                    continue;
                }
                for (descriptor = characteristic->descriptors; descriptor;
                     descriptor = descriptor->next) {
                    if (descriptor->handle == handle) {
                        *pdesc = descriptor;
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

/*
 * Get Gatt services from a given device.
 * The property GattServices on interface org.bluez.Device1 will be used to
 * retrieve all services.
 * The retrieved objects should implement org.bluez.GattService1
 */
int
parse_gatt_service (dsm_btep_context_t *ctx, const char *dbus_path,
                    GVariant *properties)
{
    char *dev_opath;
    gboolean rval;
    dsm_gatt_service_t *service;
    dsm_btep_device_t *device;

    rval = g_variant_lookup(properties, "Device", "&o", &dev_opath);
    if (!rval) {
        errp("Failed to get service Device from %s\n", dbus_path);
        return -1;
    }

    if (!(device = device_find(ctx, dev_opath))) {
        dbgp("Service for unknown device %s. Discarded\n", dev_opath);
        return 0;
    }

    service = service_find(device, dbus_path);
    if (service) {
        dbgp("Service exists %s [%s]. Skipped\n", dbus_path, service->uuid);
        return 0;
    }

    dbgp("New service at %s\n", dbus_path);

    service = calloc(1, sizeof *service);
    CHECK_COND(service, -1, "Failed to allocate new service\n");

    service->dbus_path = strdup(dbus_path);
    CHECK_CLEAN(service->dbus_path, "Failed to allocate service dbus_path\n");

    rval = g_variant_lookup(properties, "UUID", "s", &service->uuid);
    CHECK_CLEAN(rval, "Failed to get service UUID\n");
    dbgp("UUID=%s\n", service->uuid);

    /* get the handle from the last four characters of dbus_path */
    rval = (sscanf(dbus_path + strlen(dbus_path) - 4, "%x", &service->handle)
            == 1);
    CHECK_CLEAN(rval, "Failed to get service handle\n");
    dbgp("Handle=%x\n", service->handle);

    service->index = device->next_service_index++;
    dbgp("Index=%d\n", service->index);

    service->next = device->services;
    device->services = service;
    service->parent = device;

    return 0;

cleanup:
    if (service->uuid) {
        free(service->uuid);
    }
    if (service->dbus_path) {
        free(service->dbus_path);
    }
    free(service);
    return -1;
}

int
parse_gatt_characteristic (dsm_btep_context_t *ctx, const char *dbus_path,
                           GVariant *properties)
{
    char *prop_val;
    gboolean rval;
    dsm_gatt_service_t *service;
    dsm_gatt_characteristic_t *characteristic;
    GVariant *value;

    rval = g_variant_lookup(properties, "Service", "&o", &prop_val);
    CHECK_COND(rval, -1, "Failed to get characteristic Service from %s\n",
               dbus_path);

    if (!(service = service_find_from_ctx(ctx, prop_val))) {
        dbgp("Characteristic for unknown service %s\n", prop_val);
        return 0;
    }

    /* Find existing characteristic. */
    characteristic = characteristic_find(service, dbus_path);
    if (characteristic) {
        dbgp("Characteristic exists %s [%s].\n", dbus_path,
             characteristic->uuid);
        return 0;
    }

    dbgp("New characteristic at %s\n", dbus_path);

    /* Create and add new characteristic. */
    characteristic = calloc(1, sizeof *characteristic);
    CHECK_COND(characteristic, -1, "Failed to allocate new characteristic\n");

    characteristic->dbus_path = strdup(dbus_path);
    CHECK_CLEAN(characteristic->dbus_path,
                "Failed to allocate characteristic dbus_path\n");

    rval = g_variant_lookup(properties, "UUID", "s", &characteristic->uuid);
    CHECK_CLEAN(rval, "Failed to get characteristic UUID\n");
    dbgp("UUID=%s\n", characteristic->uuid);

    value = g_variant_lookup_value(properties, "Value", G_VARIANT_TYPE("ay"));
    if (!value) {
        dbgp("Failed to get characteristic Value\n");
        rval = TRUE; /* it is ok to have no value */
    } else {
        if (update_char_val(characteristic, value)) {
            g_variant_unref(value);
            goto cleanup;
        }
        g_variant_unref(value);
    }
#ifdef DEBUG
    snprint_bytes(dbg_buf, sizeof dbg_buf, characteristic->value,
                  characteristic->value_len, "-");
    dbgp("Value=%s\n", dbg_buf);
#endif

    rval = g_variant_lookup(properties, "Notifying", "b",
                            &characteristic->notifying);
    CHECK_CLEAN(rval, "Failed to get characteristic notifying\n");

    value = g_variant_lookup_value(properties, "Flags", G_VARIANT_TYPE("as"));
    if (!value) {
        errp("Failed to get characteristic flags\n");
        goto cleanup;
    }
    characteristic->properties = flags_string_to_int(value);
    g_variant_unref(value);
    dbgp("Properties=%x\n", characteristic->properties);

    /* get the handle from the last four characters of dbus_path */
    rval = (sscanf(dbus_path + strlen(dbus_path) - 4, "%x",
                   &characteristic->handle) == 1);
    CHECK_CLEAN(rval, "Failed to get characteristic handle\n");
    dbgp("Handle=%x\n", characteristic->handle);

    characteristic->index = service->next_char_index++;
    dbgp("Index=%d\n", characteristic->index);

    characteristic->next = service->characteristics;
    service->characteristics = characteristic;
    characteristic->parent = service;

    if (characteristic->notifying) {
        characteristic->property_watch_id =
            g_dbus_connection_signal_subscribe(ctx->connection,
                                               BLUEZ_BUS_NAME,
                                               DBUS_PROPERTIES_INTF,
                                               "PropertiesChanged",
                                               characteristic->dbus_path,
                                               NULL,
                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                               char_properties_changed,
                                               characteristic,
                                               NULL);
        if (!characteristic->property_watch_id) {
            errp("Failed to subscribe to characteristic properties change\n");
        }
    }

    return 0;

cleanup:
    if (characteristic->value) {
        free(characteristic->value);
    }
    if (characteristic->uuid) {
        free(characteristic->uuid);
    }
    if (characteristic->dbus_path) {
        free(characteristic->dbus_path);
    }
    free(characteristic);
    return -1;
}

int
parse_gatt_descriptor (dsm_btep_context_t *ctx, const char *dbus_path,
                       GVariant *properties)
{
    char *prop_val;
    gboolean rval;
    dsm_gatt_descriptor_t *descriptor;
    dsm_gatt_characteristic_t *characteristic;
    GVariant *value;

    rval = g_variant_lookup(properties, "Characteristic", "&o", &prop_val);
    CHECK_COND(rval, -1, "Failed to get descriptor Characteristic from %s\n",
               dbus_path);

    if (!(characteristic = characteristic_find_from_ctx(ctx, prop_val))) {
        dbgp("Descriptor for unknown characteristic %s\n", prop_val);
        return 0;
    }

    /* Find existing descriptor. */
    descriptor = descriptor_find(characteristic, dbus_path);
    if (descriptor) {
        dbgp("Descriptor exists %s [%s].\n", dbus_path,
             descriptor->uuid);
        return 0;
    }

    dbgp("New descriptor at %s\n", dbus_path);

    /* Create and add new descriptor. */
    descriptor = calloc(1, sizeof *descriptor);
    CHECK_COND(descriptor, -1, "Failed to allocate new descriptor\n");

    descriptor->dbus_path = strdup(dbus_path);
    CHECK_CLEAN(descriptor->dbus_path,
                "Failed to allocate descriptor dbus_path\n");

    rval = g_variant_lookup(properties, "UUID", "s", &descriptor->uuid);
    CHECK_CLEAN(rval, "Failed to get descriptor UUID\n");
    dbgp("UUID=%s\n", descriptor->uuid);

    value = g_variant_lookup_value(properties, "Value", G_VARIANT_TYPE("ay"));
    if (!value) {
        dbgp("Failed to get descriptor Value\n");
        rval = TRUE; /* it is ok to have no value */
    } else {
        if (update_desc_val(descriptor, value)) {
            g_variant_unref(value);
            goto cleanup;
        }
        g_variant_unref(value);
    }
#ifdef DEBUG
    snprint_bytes(dbg_buf, sizeof dbg_buf, descriptor->value,
                  descriptor->value_len, "-");
    dbgp("Value=%s\n", dbg_buf);
#endif

    value = g_variant_lookup_value(properties, "Flags", G_VARIANT_TYPE("as"));
    if (!value) {
        dbgp("Failed to get descriptor flags\n");
        rval = TRUE; /* it is ok to have no flags for descriptors */
    } else {
        descriptor->properties = flags_string_to_int(value);
        g_variant_unref(value);
    }
    dbgp("Properties=%x\n", descriptor->properties);

    /* get the handle from the last four characters of dbus_path */
    rval = (sscanf(dbus_path + strlen(dbus_path) - 4, "%x", &descriptor->handle)
            == 1);
    CHECK_CLEAN(rval, "Failed to get descriptor handle\n");
    dbgp("Handle=%x\n", descriptor->handle);

    descriptor->index = characteristic->next_desc_index++;
    dbgp("Index=%d\n", descriptor->index);

    descriptor->next = characteristic->descriptors;
    characteristic->descriptors = descriptor;
    descriptor->parent = characteristic;

    return 0;

cleanup:
    if (descriptor->value) {
        free(descriptor->value);
    }
    if (descriptor->uuid) {
        free(descriptor->uuid);
    }
    if (descriptor->dbus_path) {
        free(descriptor->dbus_path);
    }
    free(descriptor);
    return -1;
}

static int
snprint_bt_ep (char *buf, size_t len, const dsm_bt_endpoint_t *ep)
{
    return snprintf(buf, len, "%d:%d:%s:%d:%d",
                    ep->rule_negate, ep->rule_property, ep->rule_val,
                    ep->conn_fail_retry, ep->conn_success_retry);
}

static int
snprint_exec_ep (char *buf, size_t len, const dsm_exec_endpoint_t *ep)
{
    return snprintf(buf, len, ep->exec_name);
}

/* Print stream content to buf. cf. snprintf for params and return */
int
snprint_stream (char *buf, size_t len, const dsm_btep_stream_t *stream)
{
    int rval1, rval2, rval3;

    rval1 = snprintf(buf, len, "%p#", stream);
    if (rval1 < 0) { /* error */
        return rval1;
    }
    if (rval1 < len) { /* buf fits */
        buf += rval1;
        len -= rval1;
    } else { /* buf too small */
        buf += len - 1; /* buf points to the last position */
        len = 1; /* only one byte to hold NULL character */
    }

    /*
     * Even if buffer is too small, we still continue in order to get the
     * required buffer length
     */
    rval2 = snprint_bt_ep(buf, len, &stream->bt_ep);
    if (rval2 < 0) {
        return rval2;
    }
    if (rval2 < len -1) { /* -1 for the connector */
        buf[rval2] = '-'; /* connector between bt_ep & exec_ep */
        buf += rval2 + 1;
        len -= rval2 + 1;
    } else {
        buf += len - 1;
        len = 1;
    }

    rval3 = snprint_exec_ep(buf, len, &stream->other_ep.exec_ep);
    if (rval3 < 0) {
        return rval3;
    }
    return rval1 + rval2 + 1 + rval3;
}

/* Print device content to buf. cf. snprintf for params and return */
int
snprint_device (char *buf, size_t len, const dsm_btep_device_t *device)
{
    return snprintf(buf, len, "%p#%s|%d|%s|%s|%d|%d|%d:%d|%d@%d",
                    device, device->dbus_path, device->property_watch_id,
                    device->address, device->name, device->connected,
                    device->paired, device->classic_state, device->ble_state,
                    device->fd, device->index);
}

/* Print association content to buf. cf. snprintf for params and return */
int
snprint_association (char *buf, size_t len, const dsm_btep_association_t *assoc)
{
    return snprintf(buf, len, "%p-%p:%d",
                    assoc->device, assoc->stream, assoc->conn_timer);
}
