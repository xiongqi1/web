/*
 * dsm_gatt_rpc.c
 *    Data Stream Bluetooth GATT Client RPC server.
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

#include "dsm_gatt_rpc.h"
#include "dsm_bluetooth_ep.h"
#include "dsm_bt_utils.h"

#include <rdb_ops.h>
#include <rdb_rpc_server.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <glib.h>
#include <gio/gio.h>

static struct rdb_session *rdb_s;
static rdb_rpc_server_session_t *rpc_s = NULL;
static GSource *rpc_source = NULL;

#define GATT_RDB_GETNAMES_BUF_SIZE 128
#define GATT_CALL_SYNC_TIMEOUT 3000

typedef struct dsm_gatt_command_ {
    char *name;
    char *params[GATT_RPC_MAX_PARAMS];
    int num_params;
    rdb_rpc_cmd_handler_t handler;
} dsm_gatt_command_t;

/*
 * Set the RPC command result with a given string.
 * Used within command handlers.
 */
#define CMD_SET_RESULT(result_str, ...)                         \
do {                                                            \
    if (result && result_len && (*result_len > 0)) {            \
        int tmp_len = *result_len;                              \
        snprintf(result, tmp_len, result_str, ##__VA_ARGS__);   \
        *result_len = strlen(result) + 1;                       \
    }                                                           \
} while (0)

/*
 * Check a given condition. If the condition indicates a failure then
 * the RPC commnd result is set to the error_str and an rval is returned.
 * Used within command handlers.
 */
#define CMD_CHECK(condition, rval, error_str, ...)              \
do {                                                            \
    if (!(condition)) {                                         \
        CMD_SET_RESULT(error_str, ##__VA_ARGS__);               \
        return rval;                                            \
    }                                                           \
} while (0)

/*
 * GSource dispatch function. Called by Glib to to process an event source
 * (RDB notifications in this case).
 */
static gboolean
dsm_gatt_rpc_server_dispatch (GSource *source, GSourceFunc callback,
                              gpointer user_data)
{
    char *name_buf;
    int buf_len = GATT_RDB_GETNAMES_BUF_SIZE;
    int rval;

    if (!(name_buf = malloc(GATT_RDB_GETNAMES_BUF_SIZE))) {
        errp("Failed to allocate buffer for rdb getnames\n");
        return G_SOURCE_CONTINUE;
    }

    /* Get all triggered rdb variables for the dsm_btep rdb session */
    rval = rdb_getnames_alloc(rdb_s, "", &name_buf, &buf_len, TRIGGERED);

    /* If there are any trigggered vars crank them through RPC processing */
    if (!rval && (buf_len > 0)) {
        dbgp("Process RPC command %s\n", name_buf);
        rdb_rpc_server_process_commands(rpc_s, name_buf);
    }

    free(name_buf);

    /* G_SOURCE_CONTINUE means keep this source active */
    return G_SOURCE_CONTINUE;
}

static GSourceFuncs rpc_source_funcs = {
    NULL,
    NULL,
    dsm_gatt_rpc_server_dispatch,
    NULL
};

static int
read_characteristic (dsm_btep_context_t *ctx,
                     dsm_gatt_characteristic_t *characteristic,
                     char *result, int *result_len)
{
    int rval = 0;
    int dev_idx = -1, srv_idx = -1, char_idx = -1;
    char rdb_var_name[MAX_NAME_LENGTH];
    GVariant *reply = NULL;
    GError *error = NULL;
    GVariant *value = NULL;

    char_idx = characteristic->index;
    srv_idx = characteristic->parent->index;
    dev_idx = characteristic->parent->parent->index;

    reply = g_dbus_connection_call_sync(ctx->connection,
                                        BLUEZ_BUS_NAME,
                                        characteristic->dbus_path,
                                        BLUEZ_GATT_CHARACTERISTIC_INTF,
                                        "ReadValue",
                                        NULL,
                                        G_VARIANT_TYPE("(ay)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        GATT_CALL_SYNC_TIMEOUT,
                                        NULL,
                                        &error);

    if (error) {
        CMD_SET_RESULT("Failed to read characteristic: %s", error->message);
        return -error->code;
    }

    /* update characteristic->value */
    g_variant_get(reply, "(*)", &value);
    if (value) {
        rval = update_char_val(characteristic, value);
        g_variant_unref(value);
    }
    if (rval) {
        CMD_SET_RESULT("Failed to update characteristic value");
        goto cleanup;
    }

    /* update rdb */
    /* the following code can handle value_len==0 case */
    snprintf(rdb_var_name, sizeof rdb_var_name,
             "%s.%d.gatt.service.%d.char.%d.value",
             BT_DEV_RDB_ROOT, dev_idx, srv_idx, char_idx);
    rval = update_rdb_blob(ctx->rdb_s, rdb_var_name, characteristic->value,
                           characteristic->value_len);
    if (rval) {
        CMD_SET_RESULT("Failed to update rdb var %s", rdb_var_name);
        goto cleanup;
    }

    /* prepare result */
    if (characteristic->value_len > *result_len) {
        CMD_SET_RESULT("Result buffer is too small");
        rval = -1;
        goto cleanup;
    }
    memcpy(result, characteristic->value, characteristic->value_len);
    *result_len = characteristic->value_len;
    rval = 0;

cleanup:
    if (reply) {
        g_variant_unref(reply);
    }

    return rval;
}

/*
 * RPC command handler for the "read_characteristic" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "service"
 *      param value: rdb numeric index of a service exported by the device
 *
 *    - param name: "characteristic"
 *      param value: rdb numeric index of a characteristic within the service
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - data value of the characteristic on success
 *    - error message on failure
 */
static int
read_characteristic_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                                 int params_len, char *result,
                                 int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, srv_idx = -1, char_idx = -1;
    dsm_gatt_characteristic_t *characteristic;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == READ_CHAR_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "service")) {
            srv_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "characteristic")) {
            char_idx = strtol(params[ix].value, NULL, 10);
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((srv_idx >= 0), -EINVAL, "Invalid service index");
    CMD_CHECK((char_idx >= 0), -EINVAL, "Invalid characteristic index");

    characteristic = characteristic_find_by_idx(ctx, dev_idx, srv_idx,
                                                char_idx);
    CMD_CHECK(characteristic, -ENOENT, "Failed to find characteristic");

    return read_characteristic(ctx, characteristic, result, result_len);
}

static int
read_descriptor (dsm_btep_context_t *ctx, dsm_gatt_descriptor_t *descriptor,
                 char *result, int *result_len)
{
    int rval = 0;
    int dev_idx = -1, srv_idx = -1, char_idx = -1, desc_idx = -1;
    char rdb_var_name[MAX_NAME_LENGTH];
    GVariant *reply = NULL;
    GError *error = NULL;
    GVariant *value = NULL;

    desc_idx = descriptor->index;
    char_idx = descriptor->parent->index;
    srv_idx = descriptor->parent->parent->index;
    dev_idx = descriptor->parent->parent->parent->index;

    reply = g_dbus_connection_call_sync(ctx->connection,
                                        BLUEZ_BUS_NAME,
                                        descriptor->dbus_path,
                                        BLUEZ_GATT_DESCRIPTOR_INTF,
                                        "ReadValue",
                                        NULL,
                                        G_VARIANT_TYPE("(ay)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        GATT_CALL_SYNC_TIMEOUT,
                                        NULL,
                                        &error);

    if (error) {
        CMD_SET_RESULT("Failed to read descriptor: %s", error->message);
        return -error->code;
    }

    /* update descriptor->value */
    g_variant_get(reply, "(*)", &value);
    rval = update_desc_val(descriptor, value);
    if (value) {
        g_variant_unref(value);
    }
    if (rval) {
        CMD_SET_RESULT("Failed to update descriptor value");
        goto cleanup;
    }

    /* update rdb */
    /* the following code can handle value_len==0 case */
    snprintf(rdb_var_name, sizeof rdb_var_name,
             "%s.%d.gatt.service.%d.char.%d.desc.%d.value",
             BT_DEV_RDB_ROOT, dev_idx, srv_idx, char_idx, desc_idx);
    rval = update_rdb_blob(ctx->rdb_s, rdb_var_name, descriptor->value,
                           descriptor->value_len);
    if (rval) {
        CMD_SET_RESULT("Failed to update rdb var %s", rdb_var_name);
        goto cleanup;
    }
    if (descriptor->value_len > *result_len) {
        CMD_SET_RESULT("Result buffer is too small");
        rval = -1;
        goto cleanup;
    }
    memcpy(result, descriptor->value, descriptor->value_len);
    *result_len = descriptor->value_len;
    rval = 0;

cleanup:
    if (reply) {
        g_variant_unref(reply);
    }

    return rval;
}

/*
 * RPC command handler for the "read_descriptor" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "service"
 *      param value: rdb numeric index of a service exported by the device
 *
 *    - param name: "characteristic"
 *      param value: rdb numeric index of a characteristic within the service
 *
 *    - param name: "descriptor"
 *      param value: rdb numeric index of a descriptor for the characteristic
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - data value of the descriptor on success
 *    - error message on failure
 */
static int
read_descriptor_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                             int params_len, char *result,
                             int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, srv_idx = -1, char_idx = -1, desc_idx = -1;
    dsm_gatt_descriptor_t *descriptor;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == READ_DESC_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "service")) {
            srv_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "characteristic")) {
            char_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "descriptor")) {
            desc_idx = strtol(params[ix].value, NULL, 10);
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((srv_idx >= 0), -EINVAL, "Invalid service index");
    CMD_CHECK((char_idx >= 0), -EINVAL, "Invalid characteristic index");
    CMD_CHECK((desc_idx >= 0), -EINVAL, "Invalid descriptor index");

    descriptor = descriptor_find_by_idx(ctx, dev_idx, srv_idx, char_idx,
                                        desc_idx);
    CMD_CHECK(descriptor, -ENOENT, "Failed to find descriptor");

    return read_descriptor(ctx, descriptor, result, result_len);
}

/*
 * RPC command handler for the "read_handle" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "handle"
 *      param value: integer value of the attribute handle
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - data value of the characteristic or descriptor on success
 *    - error message on failure
 */
static int
read_handle_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                         int params_len, char *result,
                         int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, handle = -1;
    dsm_gatt_characteristic_t *characteristic;
    dsm_gatt_descriptor_t *descriptor;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == READ_HANDLE_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "handle")) {
            handle = strtol(params[ix].value, NULL, 10);
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((handle >= 0), -EINVAL, "Invalid handle");

    if (!attribute_find_by_handle(ctx, dev_idx, handle, &characteristic,
                                 &descriptor)) {
        CMD_SET_RESULT("Failed to find attribute");
        return -ENOENT;
    }

    if (characteristic) {
        dbgp("Reading characteristic value by handle %d...\n", handle);
        return read_characteristic(ctx, characteristic, result, result_len);
    } else {
        dbgp("Reading descriptor value by handle %d...\n", handle);
        return read_descriptor(ctx, descriptor, result, result_len);
    }
}

/* Generally we should not update cached attributes and rdb vars on write */
#define UPDATE_ON_WRITE 0

static int
write_characteristic (dsm_btep_context_t *ctx,
                      dsm_gatt_characteristic_t *characteristic,
                      const char *value, int value_len,
                      char *result, int *result_len)
{
    int ix;
    int rval = 0;
    GVariant *reply;
    GError *error = NULL;
    GVariantBuilder *builder = NULL;

#if UPDATE_ON_WRITE
    char rdb_var_name[MAX_NAME_LENGTH];
    int char_idx = characteristic->index;
    int srv_idx = characteristic->parent->index;
    int dev_idx = characteristic->parent->parent->index;
#endif

    CMD_CHECK(value_len >= 0, -EINVAL, "Negative value_len");

    builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));
    CMD_CHECK(builder, -ENOMEM, "Failed to allocate builder");

    /* this can handle value_len == 0 case */
    for (ix = 0; ix < value_len; ix++) {
        g_variant_builder_add(builder, "y", value[ix]);
    }

    reply = g_dbus_connection_call_sync(ctx->connection,
                                        BLUEZ_BUS_NAME,
                                        characteristic->dbus_path,
                                        BLUEZ_GATT_CHARACTERISTIC_INTF,
                                        "WriteValue",
                                        g_variant_new("(ay)", builder),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        GATT_CALL_SYNC_TIMEOUT,
                                        NULL,
                                        &error);

    if (error) {
        CMD_SET_RESULT("Failed to write characteristic: %s", error->message);
        rval = -error->code;
        goto cleanup;
    }

#if UPDATE_ON_WRITE
    /* update characteristic->value */
    if (value_len && value_len > characteristic->value_capacity) {
        characteristic->value_capacity = 0;
        characteristic->value_len = 0;
        if (characteristic->value) {
            free(characteristic->value);
            characteristic->value = NULL;
        }
        if(!(characteristic->value = malloc(value_len))) {
            CMD_SET_RESULT("Failed to allocate characteristic value\n");
            rval = -ENOMEM;
            goto cleanup;
        }
        characteristic->value_capacity = value_len;
    }
    characteristic->value_len = value_len;
    memcpy(characteristic->value, value, value_len);

    /* update rdb */
    /* the following code can handle value_len==0 case */
    snprintf(rdb_var_name, sizeof rdb_var_name,
             "%s.%d.gatt.service.%d.char.%d.value",
             BT_DEV_RDB_ROOT, dev_idx, srv_idx, char_idx);
    rval = update_rdb_blob(ctx->rdb_s, rdb_var_name, value, value_len);
    if (rval) {
        CMD_SET_RESULT("Failed to update rdb var %s", rdb_var_name);
        goto cleanup;
    }
#endif

    CMD_SET_RESULT("Success");
    rval = 0;

cleanup:
    if (reply) {
        g_variant_unref(reply);
    }
    g_variant_builder_unref(builder);
    return rval;
}

/*
 * RPC command handler for the "write_characteristic" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "service"
 *      param value: rdb numeric index of a service exported by the device
 *
 *    - param name: "characteristic"
 *      param value: rdb numeric index of a characteristic within the service
 *
 *    - param name: "value"
 *      param value: buffer containing data to be written
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - "Success" on success
 *    - error message on failure
 */
static int
write_characteristic_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                                  int params_len, char *result,
                                  int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, srv_idx = -1, char_idx = -1;
    dsm_gatt_characteristic_t *characteristic;
    char *value = NULL;
    int value_len = -1;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == WRITE_CHAR_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "service")) {
            srv_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "characteristic")) {
            char_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "value")) {
            value = params[ix].value;
            value_len = params[ix].value_len;
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((srv_idx >= 0), -EINVAL, "Invalid service index");
    CMD_CHECK((char_idx >= 0), -EINVAL, "Invalid characteristic index");
    CMD_CHECK((value_len == 0 || (value_len > 0 && value)), -EINVAL,
              "Invalid value and value_len");

    characteristic = characteristic_find_by_idx(ctx, dev_idx, srv_idx,
                                                char_idx);
    CMD_CHECK(characteristic, -ENOENT, "Failed to find characteristic");

    return write_characteristic(ctx, characteristic, value, value_len,
                                result, result_len);
}

static int
write_descriptor (dsm_btep_context_t *ctx, dsm_gatt_descriptor_t *descriptor,
                  const char *value, int value_len,
                  char *result, int *result_len)
{
    int ix;
    int rval = 0;
    GVariant *reply;
    GError *error = NULL;
    GVariantBuilder *builder = NULL;

#if UPDATE_ON_WRITE
    char rdb_var_name[MAX_NAME_LENGTH];
    int desc_idx = descriptor->index;
    int char_idx = descriptor->parent->index;
    int srv_idx = descriptor->parent->parent->index;
    int dev_idx = descriptor->parent->parent->parent->index;
#endif

    CMD_CHECK(value_len >= 0, -EINVAL, "Negative value_len");

    builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));
    CMD_CHECK(builder, -ENOMEM, "Failed to allocate builder");

    /* this can handle value_len == 0 case */
    for (ix = 0; ix < value_len; ix++) {
        g_variant_builder_add(builder, "y", value[ix]);
    }

    reply = g_dbus_connection_call_sync(ctx->connection,
                                        BLUEZ_BUS_NAME,
                                        descriptor->dbus_path,
                                        BLUEZ_GATT_DESCRIPTOR_INTF,
                                        "WriteValue",
                                        g_variant_new("(ay)", builder),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        GATT_CALL_SYNC_TIMEOUT,
                                        NULL,
                                        &error);

    if (error) {
        CMD_SET_RESULT("Failed to write descriptor: %s", error->message);
        rval = -error->code;
        goto cleanup;
    }

#if UPDATE_ON_WRITE
    /* update descriptor->value */
    if (value_len && value_len > descriptor->value_capacity) {
        descriptor->value_capacity = 0;
        descriptor->value_len = 0;
        if (descriptor->value) {
            free(descriptor->value);
            descriptor->value = NULL;
        }
        if(!(descriptor->value = malloc(value_len))) {
            CMD_SET_RESULT("Failed to allocate descriptor value\n");
            rval = -ENOMEM;
            goto cleanup;
        }
        descriptor->value_capacity = value_len;
    }
    descriptor->value_len = value_len;
    memcpy(descriptor->value, value, value_len);

    /* update rdb */
    /* the following code can handle value_len==0 case */
    snprintf(rdb_var_name, sizeof rdb_var_name,
             "%s.%d.gatt.service.%d.char.%d.desc.%d.value",
             BT_DEV_RDB_ROOT, dev_idx, srv_idx, char_idx, desc_idx);
    rval = update_rdb_blob(ctx->rdb_s, rdb_var_name, value, value_len);
    if (rval) {
        CMD_SET_RESULT("Failed to update rdb var %s", rdb_var_name);
        goto cleanup;
    }
#endif

    CMD_SET_RESULT("Success");
    rval = 0;

cleanup:
    if (reply) {
        g_variant_unref(reply);
    }
    g_variant_builder_unref(builder);
    return rval;
}

/*
 * RPC command handler for the "write_descriptor" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "service"
 *      param value: rdb numeric index of a service exported by the device
 *
 *    - param name: "characteristic"
 *      param value: rdb numeric index of a characteristic within the service
 *
 *    - param name: "descriptor"
 *      param value: rdb numeric index of a descriptor for the characteristic
 *
 *    - param name: "value"
 *      param value: buffer containing data to be written
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - "Success" on success
 *    - error message on failure
 */
static int
write_descriptor_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                              int params_len, char *result,
                              int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, srv_idx = -1, char_idx = -1, desc_idx = -1;
    dsm_gatt_descriptor_t *descriptor;
    char *value = NULL;
    int value_len = -1;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == WRITE_DESC_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "service")) {
            srv_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "characteristic")) {
            char_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "descriptor")) {
            desc_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "value")) {
            value = params[ix].value;
            value_len = params[ix].value_len;
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((srv_idx >= 0), -EINVAL, "Invalid service index");
    CMD_CHECK((char_idx >= 0), -EINVAL, "Invalid characteristic index");
    CMD_CHECK((desc_idx >= 0), -EINVAL, "Invalid descriptor index");
    CMD_CHECK((value_len == 0 || (value_len > 0 && value)), -EINVAL,
              "Invalid value and value_len");

    descriptor = descriptor_find_by_idx(ctx, dev_idx, srv_idx, char_idx,
                                        desc_idx);
    CMD_CHECK(descriptor, -ENOENT, "Failed to find descriptor");

    return write_descriptor(ctx, descriptor, value, value_len,
                            result, result_len);
}

/*
 * RPC command handler for the "write_handle" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "handle"
 *      param value: integer value of the attribute handle
 *
 *    - param name: "value"
 *      param value: buffer containing data to be written
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - "Success" on success
 *    - error message on failure
 */
static int
write_handle_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                          int params_len, char *result,
                          int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, handle = -1;
    dsm_gatt_characteristic_t *characteristic;
    dsm_gatt_descriptor_t *descriptor;
    char *value = NULL;
    int value_len = -1;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == WRITE_HANDLE_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "handle")) {
            handle = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "value")) {
            value = params[ix].value;
            value_len = params[ix].value_len;
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((handle >= 0), -EINVAL, "Invalid handle");
    CMD_CHECK((value_len == 0 || (value_len > 0 && value)), -EINVAL,
              "Invalid value and value_len");

    if (!attribute_find_by_handle(ctx, dev_idx, handle, &characteristic,
                                  &descriptor)) {
        CMD_SET_RESULT("Failed to find attribute");
        return -ENOENT;
    }

    if (characteristic) {
        dbgp("Writing characteristic value by handle %d...\n", handle);
        return write_characteristic(ctx, characteristic, value, value_len,
                                    result, result_len);
    } else {
        dbgp("Writing descriptor value by handle %d...\n", handle);
        return write_descriptor(ctx, descriptor, value, value_len,
                                result, result_len);
    }
}

static int
set_characteristic_notify (dsm_btep_context_t *ctx,
                           dsm_gatt_characteristic_t *characteristic,
                           int value, char *result, int *result_len)
{
    int rval = 0;
    int dev_idx = -1, srv_idx = -1, char_idx = -1;
    char rdb_var_name[MAX_NAME_LENGTH];
    GVariant *reply;
    GError *error = NULL;

    char_idx = characteristic->index;
    srv_idx = characteristic->parent->index;
    dev_idx = characteristic->parent->parent->index;

    reply = g_dbus_connection_call_sync(ctx->connection,
                                        BLUEZ_BUS_NAME,
                                        characteristic->dbus_path,
                                        BLUEZ_GATT_CHARACTERISTIC_INTF,
                                        value ? "StartNotify" : "StopNotify",
                                        NULL,
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        GATT_CALL_SYNC_TIMEOUT,
                                        NULL,
                                        &error);

    if (error) {
        CMD_SET_RESULT("Failed to set characteristic notify: %s",
                       error->message);
        return -error->code;
    }

    /* update characteristic->notifying */
    characteristic->notifying = value ? 1 : 0;

    /* update rdb */
    snprintf(rdb_var_name, sizeof rdb_var_name,
             "%s.%d.gatt.service.%d.char.%d.notifying",
             BT_DEV_RDB_ROOT, dev_idx, srv_idx, char_idx);
    rval = rdb_update_string(ctx->rdb_s, rdb_var_name, value ? "1" : "0",
                             0, 0);
    if (rval) {
        CMD_SET_RESULT("Failed to update rdb variable");
        goto cleanup;
    }

    /* watch/unwatch PropertiesChanged event for the char value */
    if (!characteristic->notifying && characteristic->property_watch_id) {
        g_dbus_connection_signal_unsubscribe(ctx->connection,
                                             characteristic->property_watch_id);
        characteristic->property_watch_id = 0;
    }
    if (characteristic->notifying && !characteristic->property_watch_id) {
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
            CMD_SET_RESULT("Failed to subscribe to characteristic properties "
                           "change signal");
            rval = -1;
            goto cleanup;
        }
    }
    CMD_SET_RESULT("Success");
    rval = 0;

cleanup:
    if (reply) {
        g_variant_unref(reply);
    }
    return rval;
}

/*
 * RPC command handler for the "set_characteristic_notify" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "service"
 *      param value: rdb numeric index of a service exported by the device
 *
 *    - param name: "characteristic"
 *      param value: rdb numeric index of a characteristic within the service
 *
 *    - param name: "value"
 *      param value: a value (1/0) indicating whether to enable/disable
 *                   notifications
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - "Success" on success
 *    - error message on failure
 */
static int
set_characteristic_notify_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                                       int params_len, char *result,
                                       int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, srv_idx = -1, char_idx = -1;
    dsm_gatt_characteristic_t *characteristic;
    int value = -1;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == SET_CHAR_NOTIFY_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "service")) {
            srv_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "characteristic")) {
            char_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "value")) {
            value = strtol(params[ix].value, NULL, 10);
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((srv_idx >= 0), -EINVAL, "Invalid service index");
    CMD_CHECK((char_idx >= 0), -EINVAL, "Invalid characteristic index");
    CMD_CHECK((value == 0 || value == 1), -EINVAL, "Invalid value");

    characteristic = characteristic_find_by_idx(ctx, dev_idx, srv_idx,
                                                char_idx);
    CMD_CHECK(characteristic, -ENOENT, "Failed to find characteristic");

    return set_characteristic_notify(ctx, characteristic, value,
                                     result, result_len);
}

/*
 * RPC command handler for the "set_handle_notify" command.
 *
 * RPC params:
 *    - param name: "device"
 *      param value: rdb numeric index of device
 *
 *    - param name: "handle"
 *      param value: integer value of the characteristic handle
 *
 *    - param name: "value"
 *      param value: a value (1/0) indicating whether to enable/disable
 *                   notifications
 *
 * Return value:
 *    - 0 on success
 *    - negative error code on failure
 *
 * RPC result buffer:
 *    - "Success" on success
 *    - error message on failure
 */
static int
set_handle_notify_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                               int params_len, char *result,
                               int *result_len)
{
    int ix;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int dev_idx = -1, handle=-1;
    dsm_gatt_characteristic_t *characteristic;
    int value = -1;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == SET_HANDLE_NOTIFY_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "device")) {
            dev_idx = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "handle")) {
            handle = strtol(params[ix].value, NULL, 10);
        } else if (!strcmp(params[ix].name, "value")) {
            value = strtol(params[ix].value, NULL, 10);
        }
    }

    CMD_CHECK((dev_idx >= 0), -EINVAL, "Invalid device index");
    CMD_CHECK((handle >= 0), -EINVAL, "Invalid handle");
    CMD_CHECK((value == 0 || value == 1), -EINVAL, "Invalid value");

    if (!attribute_find_by_handle(ctx, dev_idx, handle,
                                  &characteristic, NULL)) {
        CMD_SET_RESULT("Failed to find characteristic");
        return -ENOENT;
    }

    dbgp("Setting characteristic notify by handle %d...\n", handle);
    return set_characteristic_notify(ctx, characteristic, value,
                                     result, result_len);
}

/*
 * Initialises the dsm_gatt RPC server. The RPC server runs in the same
 * single threaded process as the dbus handling. The dbus handling is done
 * via a GLib main loop. So the RPC server hooks itself into the main loop
 * by adding the RDB file descriptor as a main loop event source.
 */
int
dsm_gatt_rpc_server_init (void)
{
    int ix;
    int rval = 0;

    /* all implemented rpc commands */
    dsm_gatt_command_t commands[] = {
        COMMAND(read_characteristic, READ_CHAR_PARAMS, READ_CHAR_NPARAMS),
        COMMAND(read_descriptor, READ_DESC_PARAMS, READ_DESC_NPARAMS),
        COMMAND(read_handle, READ_HANDLE_PARAMS, READ_HANDLE_NPARAMS),
        COMMAND(write_characteristic, WRITE_CHAR_PARAMS, WRITE_CHAR_NPARAMS),
        COMMAND(write_descriptor, WRITE_DESC_PARAMS, WRITE_DESC_NPARAMS),
        COMMAND(write_handle, WRITE_HANDLE_PARAMS, WRITE_HANDLE_NPARAMS),
        COMMAND(set_characteristic_notify, SET_CHAR_NOTIFY_PARAMS,
                SET_CHAR_NOTIFY_NPARAMS),
        COMMAND(set_handle_notify, SET_HANDLE_NOTIFY_PARAMS,
                SET_HANDLE_NOTIFY_NPARAMS)
    };

    INVOKE_CHK(rdb_open(NULL, &rdb_s), "Failed to open rdb session\n");

    rpc_source = g_source_new(&rpc_source_funcs, sizeof(GSource));
    rval = -1;
    CHECK_CLEAN(rpc_source, "Failed to create dsm_gatt rpc source\n");

    g_source_add_unix_fd(rpc_source, rdb_fd(rdb_s), G_IO_IN);
    g_source_attach(rpc_source, NULL);

    /* Init the RPC server */
    INVOKE_CLEAN(rdb_rpc_server_init(GATT_RPC_SERVICE_NAME, &rpc_s),
                 "Failed to init dsm_gatt rpc server\n");

    /* Add each dsm_gatt command */
    for (ix = 0; ix < (sizeof commands / sizeof commands[0]); ix++) {
        dsm_gatt_command_t *cmd = &commands[ix];
        INVOKE_CLEAN(rdb_rpc_server_add_command(rpc_s, cmd->name, cmd->params,
                                                cmd->num_params, cmd->handler),
                     "Failed to add command '%s'\n", cmd->name);
    }

    /* Other initialization steps */

    /* Start the server running in manual mode */
    INVOKE_CLEAN(rdb_rpc_server_run(rpc_s, rdb_s),
                 "Failed to run dsm_gatt server\n");

cleanup:
    if (rval) { /* clean up on failure */
        dsm_gatt_rpc_server_destroy();
    }

    return rval;
}

/*
 * Destroy the RPC server.
 */
void
dsm_gatt_rpc_server_destroy (void)
{
    if (rpc_s) {
        rdb_rpc_server_stop(rpc_s);
        rdb_rpc_server_destroy(&rpc_s);
    }

    if (rpc_source) {
        g_source_destroy(rpc_source); /* remove from context and destroy */
        g_source_unref(rpc_source); /* reduce refcount to 0 so it gets freed */
        rpc_source = NULL;
    }

    if (rdb_s) {
        rdb_close(&rdb_s);
    }
}
