/*
 * dsm_btep_rpc.c
 *    Data Stream Bluetooth Endpoint RPC server.
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

#include "dsm_btep_rpc.h"
#include "dsm_bt_utils.h"
#include "dsm_bluetooth_ep.h"

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

#define DSM_BTEP_RDB_GETNAMES_BUF_SIZE 128

typedef struct dsm_btep_command_ {
    char *name;
    char *params[BTEP_RPC_MAX_PARAMS];
    int num_params;
    rdb_rpc_cmd_handler_t handler;
} dsm_btep_command_t;

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
dsm_btep_rpc_server_dispatch (GSource *source, GSourceFunc callback,
                              gpointer user_data)
{
    char *name_buf;
    int buf_len = DSM_BTEP_RDB_GETNAMES_BUF_SIZE;
    int rval;

    if (!(name_buf = malloc(DSM_BTEP_RDB_GETNAMES_BUF_SIZE))) {
        errp("Failed to allocate buffer for rdb getnames\n");
        return G_SOURCE_CONTINUE;
    }

    /* Get all triggered rdb variables for the dsm_btep rdb session */
    rval = rdb_getnames_alloc(rdb_s, "", &name_buf, &buf_len, TRIGGERED);

    /* If there are any trigggered vars crank them through RPC processing */
    if (!rval && (buf_len > 0)) {
        dbgp("Process RPC command\n");
        rdb_rpc_server_process_commands(rpc_s, name_buf);
    }

    free(name_buf);

    /* G_SOURCE_CONTINUE means keep this source active */
    return G_SOURCE_CONTINUE;
}

static GSourceFuncs rpc_source_funcs = {
    NULL,
    NULL,
    dsm_btep_rpc_server_dispatch,
    NULL
};

/*
 * RPC command handler for the "add_stream" command.
 * This is the main command provided by the dsm_btep_rpc server.
 * It is invoked by dsm_bluetooth_ep.sh script that acts as an RPC client
 * when a running dsm_btep_rpc server has been detected.
 *
 * RPC params:
 *    - param name: "epa_rdb_root"
 *      param value: The rdb prefix for endpoint A of the stream
 *
 *    - param name: "epa_type"
 *      param value: The type of endpoint A
 *
 *    - param name: "epb_rdb_root"
 *      param value: The rdb prefix for endpoint B of the stream
 *
 *    - param name: "epb_type"
 *      param value: The type of endpoint B
 *
 * RPC result string:
 *    - error message on failure
 *    - "Success" on success
 */
static int
add_stream_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                        int params_len, char *result,
                        int *result_len)
{
    int ix;
    int rval = 0;
    dsm_btep_stream_t *stream = NULL, *existing_stream;
    dsm_btep_context_t *ctx = dsm_btep_get_context();

    char * argv[ADD_STREAM_NPARAMS];

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == ADD_STREAM_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    /* gather parameters in the right order */
    for (ix = 0; ix < params_len; ix++) {
        if (!strcmp(params[ix].name, "epa_rdb_root")) {
            argv[0] = params[ix].value;
        } else if (!strcmp(params[ix].name, "epa_type")) {
            argv[1] = params[ix].value;
        } else if (!strcmp(params[ix].name, "epb_rdb_root")) {
            argv[2] = params[ix].value;
        } else if (!strcmp(params[ix].name, "epb_type")) {
            argv[3] = params[ix].value;
        }
    }

    rval = construct_stream(ctx->rdb_s, argv, &stream);
    if (rval) {
        CMD_SET_RESULT("Failed to construct stream");
        return rval;
    }

    /* check if the stream is already in ctx */
    existing_stream = stream_find(ctx, stream);
    if (existing_stream) {
        CMD_SET_RESULT("Stream already exists");
        return 0; // duplicate is simply ignored
    }

    /* insert into ctx */
    stream->next = ctx->streams;
    ctx->streams = stream;

    /* update all devices in ctx */
    dsm_btep_match_all_devices_with_stream(ctx, stream);

    CMD_SET_RESULT("Success");
    return 0;
}

static int
get_streams_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                         int params_len, char *result,
                         int *result_len)
{
    dsm_btep_stream_t *stream;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int rval;
    char *ptr = result;
    int len = *result_len;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == GET_STREAMS_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    for (stream = ctx->streams; stream; stream = stream->next) {
        if (stream != ctx->streams) { /* need a separator between streams */
            if (len < 2) { /* no space for ';' */
                break;
            }
            *ptr++ = ';';
            len--;
        }
        rval = snprint_stream(ptr, len, stream);
        if (rval < 0) {
            break;
        }
        ptr += rval;
        len -= rval;
        if (len <= 0) {
            break;
        }
    }

    *result_len = strlen(result) + 1;

    return 0;
}

static int
get_devices_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                         int params_len, char *result,
                         int *result_len)
{
    dsm_btep_device_t *device;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int rval;
    char *ptr = result;
    int len = *result_len;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == GET_DEVICES_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    for (device = ctx->devices; device; device = device->next) {
        if (device != ctx->devices) { /* need a separator between devices */
            if (len < 2) { /* no space for ';' */
                break;
            }
            *ptr++ = ';';
            len--;
        }
        rval = snprint_device(ptr, len, device);
        if (rval < 0) {
            break;
        }
        ptr += rval;
        len -= rval;
        if (len <= 0) {
            break;
        }
    }

    *result_len = strlen(result) + 1;

    return 0;
}

static int
get_associations_cmd_handler (char *cmd, rdb_rpc_cmd_param_t params[],
                              int params_len, char *result,
                              int *result_len)
{
    dsm_btep_association_t *assoc;
    dsm_btep_context_t *ctx = dsm_btep_get_context();
    int rval;
    char *ptr = result;
    int len = *result_len;

    dbgp("Handling command %s, params_len=%d\n", cmd, params_len);
    CMD_CHECK((result && result_len && (*result_len > 0)),
              -EINVAL, "Invalid result params");
    CMD_CHECK((params_len == GET_ASSOCIATIONS_NPARAMS),
              -EINVAL, "Invalid params_len %d", params_len);

    for (assoc = ctx->associations; assoc; assoc = assoc->next) {
        if (assoc != ctx->associations) { /* need a separator between assocs */
            if (len < 2) { /* no space for ';' */
                break;
            }
            *ptr++ = ';';
            len--;
        }
        rval = snprint_association(ptr, len, assoc);
        if (rval < 0) {
            break;
        }
        ptr += rval;
        len -= rval;
        if (len <= 0) {
            break;
        }
    }

    *result_len = strlen(result) + 1;

    return 0;
}

/*
 * Initialises the dsm_btep RPC server. The RPC server runs in the same
 * single threaded process as the dbus handling. The dbus handling is done
 * via a GLib main loop. So the RPC server hooks itself into the main loop
 * by adding the RDB file descriptor as a main loop event source.
 */
int
dsm_btep_rpc_server_init (void)
{
    int ix;
    int rval = 0;

    /* all implemented rpc commands */
    dsm_btep_command_t commands[] = {
        COMMAND(add_stream, ADD_STREAM_PARAMS, ADD_STREAM_NPARAMS),
        COMMAND(get_streams, GET_STREAMS_PARAMS, GET_STREAMS_NPARAMS),
        COMMAND(get_devices, GET_DEVICES_PARAMS, GET_DEVICES_NPARAMS),
        COMMAND(get_associations, GET_ASSOCIATIONS_PARAMS,
                GET_ASSOCIATIONS_NPARAMS)
    };

    INVOKE_CHK(rdb_open(NULL, &rdb_s), "Failed to open rdb session\n");

    rpc_source = g_source_new(&rpc_source_funcs, sizeof(GSource));
    rval = -1;
    CHECK_CLEAN(rpc_source, "Failed to create dsm_btep rpc source\n");

    g_source_add_unix_fd(rpc_source, rdb_fd(rdb_s), G_IO_IN);
    g_source_attach(rpc_source, NULL);

    /* Init the RPC server */
    INVOKE_CLEAN(rdb_rpc_server_init(BTEP_RPC_SERVICE_NAME, &rpc_s),
                 "Failed to init dsm_btep rpc server\n");

    /* Add each dsm_btep command */
    for (ix = 0; ix < (sizeof commands / sizeof commands[0]); ix++) {
        dsm_btep_command_t *cmd = &commands[ix];
        INVOKE_CLEAN(rdb_rpc_server_add_command(rpc_s, cmd->name, cmd->params,
                                                cmd->num_params, cmd->handler),
                     "Failed to add command '%s'\n", cmd->name);
    }

    /* Other initialization steps */

    /* Start the server running in manual mode */
    INVOKE_CLEAN(rdb_rpc_server_run(rpc_s, rdb_s),
                 "Failed to run dsm_btep server\n");

cleanup:
    if (rval) { // clean up on failure
        dsm_btep_rpc_server_destroy();
    }

    return rval;
}

/*
 * Destroy the RPC server.
 */
void
dsm_btep_rpc_server_destroy (void)
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
