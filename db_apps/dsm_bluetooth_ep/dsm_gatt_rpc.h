#ifndef DSM_GATT_RPC_H_12370923022016
#define DSM_GATT_RPC_H_12370923022016
/*
 * Data Stream Bluetooth GATT RPC Server.
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

#include <stdio.h>
#include <syslog.h>

#define GATT_RPC_SERVICE_NAME "gatt.rpc"
#define GATT_RPC_MAX_PARAMS 5

#define COMMAND(NAME, PARAMS, NPARAMS) { #NAME, PARAMS, NPARAMS, \
                                         NAME ## _cmd_handler }

#define READ_CHAR_CMD "read_characteristic"
#define READ_CHAR_PARAMS {"device", "service", "characteristic"}
#define READ_CHAR_NPARAMS 3

#define READ_DESC_CMD "read_descriptor"
#define READ_DESC_PARAMS {"device", "service", "characteristic", "descriptor"}
#define READ_DESC_NPARAMS 4

#define READ_HANDLE_CMD "read_handle"
#define READ_HANDLE_PARAMS {"device", "handle"}
#define READ_HANDLE_NPARAMS 2

#define WRITE_CHAR_CMD "write_characteristic"
#define WRITE_CHAR_PARAMS {"device", "service", "characteristic", "value"}
#define WRITE_CHAR_NPARAMS 4

#define WRITE_DESC_CMD "write_descriptor"
#define WRITE_DESC_PARAMS {"device", "service", "characteristic", \
                           "descriptor", "value"}
#define WRITE_DESC_NPARAMS 5

#define WRITE_HANDLE_CMD "write_handle"
#define WRITE_HANDLE_PARAMS {"device", "handle", "value"}
#define WRITE_HANDLE_NPARAMS 3

#define SET_CHAR_NOTIFY_CMD "set_characteristic_notify"
#define SET_CHAR_NOTIFY_PARAMS {"device", "service", "characteristic", "value"}
#define SET_CHAR_NOTIFY_NPARAMS 4

#define SET_HANDLE_NOTIFY_CMD "set_handle_notify"
#define SET_HANDLE_NOTIFY_PARAMS {"device", "handle", "value"}
#define SET_HANDLE_NOTIFY_NPARAMS 3


int dsm_gatt_rpc_server_init(void);
void dsm_gatt_rpc_server_destroy(void);

#endif /* DSM_GATT_RPC_H_12370923022016 */
