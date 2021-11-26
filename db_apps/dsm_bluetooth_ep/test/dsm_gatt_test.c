/*
 * This is a test client for dsm_gatt_rpc server.
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
#include "dsm_bt_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <rdb_ops.h>
#include <rdb_rpc_client.h>

static void
usage (const char *prog_name)
{
    printf("Usage:\n");
    printf("\t%s [options] <cmd_name> (<param_name> <param_value>)*\n",
           prog_name);
    printf("\tOptions:\n");
    printf("\t -s <service name>\t [default=%s]\n", GATT_RPC_SERVICE_NAME);
    printf("e.g.\n");
    printf("\t%s read_handle device 0 handle 10\n",
           prog_name);
}

#define TEST_MAX_RESULT_LEN 1024

int
main (int argc, char *argv[])
{
    int opt;
    char service_name[MAX_NAME_LENGTH];
    int ix, num_params;
    rdb_rpc_cmd_param_t params[GATT_RPC_MAX_PARAMS];
    rdb_rpc_client_session_t *rpc_client_s = NULL;
    char *cmd;
    char result[TEST_MAX_RESULT_LEN];
    int result_len;
    char buf[TEST_MAX_RESULT_LEN];
    int rval;

    strncpy(service_name, GATT_RPC_SERVICE_NAME, sizeof service_name);
    while ((opt = getopt(argc, argv, "hs:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            exit(-1);
        case 's':
            strncpy(service_name, optarg, sizeof service_name);
            break;
        default:
            usage(argv[0]);
            exit(-1);
        }
    }
    service_name[sizeof(service_name) - 1] = '\0';

    if (optind >= argc) {
        usage(argv[0]);
        return -1;
    }

    cmd = strdup(argv[optind++]);
    dbgp("Command=%s\n", cmd);
    num_params = 0;
    for (ix = optind; ix < argc -1; ix += 2) {
        params[num_params].name = strdup(argv[ix]);
        params[num_params].value = strdup(argv[ix + 1]);
        params[num_params].value_len = strlen(params[num_params].value) + 1;
        dbgp("Param name=%s, value=%s\n", params[num_params].name,
             params[num_params].value);
        num_params++;
    }

    INVOKE_CHK(rdb_rpc_client_connect(service_name, &rpc_client_s),
               "Failed to connect to service\n");

    result_len = sizeof(result);
    rval = rdb_rpc_client_invoke(rpc_client_s, cmd, params, num_params, 0,
                                 result, &result_len);
    if (rval) {
        fprintf(stderr, "Failed to invoke command\n");
        fprintf(stderr, "%s\n", result);
    } else {
        if (!strcmp(cmd, "read_characteristic") ||
            !strcmp(cmd, "read_descriptor") ||
            !strcmp(cmd, "read_handle")) {
            snprint_bytes(buf, sizeof buf, result, result_len, "-");
            printf("Value=%s\n", buf);
        } else {
            printf("%s\n", result);
        }
    }

    INVOKE_CHK(rdb_rpc_client_disconnect(&rpc_client_s),
               "Failed to disconnect from service\n");

    return 0;
}
