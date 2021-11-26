/*
 * Bluetooth sensor monitor (btsensormon) nonin device client.
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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

#include "btsensormon.h"
#include "nonin.h"

/*
 * btsm device client accept function. Currently it applies a very basic filter
 * (device name). More detailed filters such as Manufacturer data, device
 * model, serial numbers and MAC adddress can be applied if required.
 */
static bool
nonin_accept (btsm_device_t *device)
{
    struct {
        const char *dev_name_prefix;
        const char *dev_description;
        btsm_device_type_t type;
    } supported_devices[] = {
        { "Nonin3230", "Nonin Oximeter (Non3230)",
          DEV_TYPE_NN3350_PULSE_OXIMETER },
        { NULL, NULL, DEV_TYPE_UNKNOWN }
    };
    unsigned int ix = 0;
    const char *name_prefix;

    while ((name_prefix = supported_devices[ix].dev_name_prefix)) {
        /*
         * Some devices do not have a name. Those devices are never accepted
         * as only interested in specifcally named devices. The nonin device
         * names end with the serial number so only check that it starts with
         * the expected prefix.
         */
        if (device->name &&
            (strstr(device->name, name_prefix) == device->name)) {
            device->description = supported_devices[ix].dev_description;
            device->type = supported_devices[ix].type;
            device->profile_uuid = NULL;
            return true;
        }

        ix++;
    }

    return false;
}

/*
 * Display a characteristic's value.
 */
int
store_value (GVariant *value, nonin_context_t *nctx)
{
    GVariantIter *iter;
    char raw_data[sizeof(nonin3230_oximetry_data_t)];
    nonin3230_oximetry_data_t *nonin_data =
        (nonin3230_oximetry_data_t *)raw_data;
    btsm_device_data_t data[1];
    int ix = 0;
    int pulse;
    time_t now;
    static time_t last_measure_time;

    dbgp("Data: ");
    g_variant_get(value, "ay", &iter);
    while (g_variant_iter_loop(iter, "y", &raw_data[ix++])) {
        dbgp("0x%02x ", raw_data[ix-1]);
    }

    pulse = (nonin_data->pulse_rate >> 8) |
        ((nonin_data->pulse_rate & 0xff) << 8);
    dbgp("Nonin: spo2 %d, pulse %d\n", nonin_data->spo2, pulse);

    if (pulse < 0) {
        dbgp("Nonin: unsynced data ignored\n");
        return -1;
    }

    now = time(NULL);
    if ((now - last_measure_time) < DEBOUNCE_SECS) {
        /* Ignore repeated measurements in debounce period. */
        dbgp("Nonin: Debounce: Measurement ignored\n");
        return 0;
    }
    last_measure_time = now;

    snprintf(data[0].value, sizeof(data[0].value), "%d,%d", nonin_data->spo2,
             pulse);
    nctx->process_fn(nctx->device, data, 1);

    g_variant_iter_free(iter);

    return 0;
}

static void
properties_changed (GDBusConnection *connection,
                    const gchar *sender_name,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *signal_name,
                    GVariant *parameters,
                    gpointer user_data)
{
    nonin_context_t *nctx = user_data;
    btsm_device_t *device = nctx->device;
    btsm_context_t *ctx = device->ctx;
    char *intf_name;
    char *prop_name;
    GVariant *prop_val;
    GVariantIter *prop_iterator = NULL;
    int rval;
    /*
     * Iterate through the changed properties and act on the ones of
     * interest.
     */
    g_variant_get(parameters, "(&sa{sv}as)", &intf_name, &prop_iterator, NULL);
    while (g_variant_iter_loop(prop_iterator, "{&sv}", &prop_name, &prop_val)) {
        if (!strcmp(prop_name, "Value")) {
            /*
             * This must be a characteristic Value property change as "notify"
             * command is the only case that subscribes to changes to a "Value"
             * property. Display the current value and exit if displayed
             * for the requested number of times.
             */
            rval = store_value(prop_val, nctx);

            if (!rval) {
                g_dbus_connection_call(ctx->connection,
                                       BLUEZ_BUS_NAME,
                                       object_path,
                                       BLUEZ_GATT_CHARACTERISTIC_INTF,
                                       "StopNotify",
                                       NULL,
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       NULL,
                                       NULL);
            }
        }
    }
}

/*
 * btsm device client collect function. Reads data for the default
 * user, stores it into the out_data buffer.
 */
static int
nonin_collect (btsm_device_t *device, btsm_device_data_t *out_data,
               unsigned int max_data, btsm_process_data_fn process_fn)
{
    char att_path[512];
    GVariant *reply;
    GError *error = NULL;

    dbgp("nonin collect\n");

    snprintf(att_path, sizeof(att_path), "%s/service0016/char0017",
             device->dbus_path);

    dbgp("Enable notify for char %s\n", att_path);

    reply = g_dbus_connection_call_sync(device->ctx->connection,
                                        BLUEZ_BUS_NAME,
                                        att_path,
                                        BLUEZ_GATT_CHARACTERISTIC_INTF,
                                        "StartNotify",
                                        NULL,
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

    if (error) {
        errp("Unable to get start notify: %s\n", error->message);
        system("speaker-play < /usr/lib/sounds/fail");
    }  else {
        nctx_g.device = device;
        nctx_g.process_fn = process_fn;
        g_dbus_connection_signal_subscribe(device->ctx->connection,
                                           BLUEZ_BUS_NAME,
                                           DBUS_PROPERTIES_INTF,
                                           "PropertiesChanged",
                                           att_path,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           properties_changed,
                                           &nctx_g,
                                           NULL);
    }

    if (reply) {
        g_variant_unref(reply);
    }

    return 0;
}

btsm_client_t nonin_client = {
    .accept = nonin_accept,
    .collect_poll = nonin_collect,
    .collect_event = NULL,
};
