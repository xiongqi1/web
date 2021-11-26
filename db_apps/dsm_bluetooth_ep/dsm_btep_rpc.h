#ifndef DSM_BTEP_RPC_H_12350923021016
#define DSM_BTEP_RPC_H_12350923021016
/*
 * Data Stream Bluetooth Endpoint RPC Server.
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

#define BTEP_RPC_SERVICE_NAME "dsm.btep.rpc"
#define BTEP_RPC_MAX_PARAMS 4

#define COMMAND(NAME, PARAMS, NPARAMS) { #NAME, PARAMS, NPARAMS, \
                                         NAME ## _cmd_handler }
#define ADD_STREAM_CMD "add_stream"
#define ADD_STREAM_PARAMS { "epa_rdb_root", "epa_type", \
                            "epb_rdb_root", "epb_type"}
#define ADD_STREAM_NPARAMS 4

#define GET_STREAMS_CMD "get_streams"
#define GET_STREAMS_PARAMS { }
#define GET_STREAMS_NPARAMS 0

#define GET_DEVICES_CMD "get_devices"
#define GET_DEVICES_PARAMS { }
#define GET_DEVICES_NPARAMS 0

#define GET_ASSOCIATIONS_CMD "get_associations"
#define GET_ASSOCIATIONS_PARAMS { }
#define GET_ASSOCIATIONS_NPARAMS 0

int dsm_btep_rpc_server_init(void);
void dsm_btep_rpc_server_destroy(void);

#endif /* DSM_BTEP_RPC_H_12350923021016 */
