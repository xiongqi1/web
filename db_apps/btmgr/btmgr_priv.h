/*
 * btmgr_priv.h
 *    Private Bluetooth Manager definitions
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
#ifndef __BTMGR_PRIV_H__
#define __BTMGR_PRIV_H__

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include <rdb_ops.h>

//#define DBG
#ifdef DBG
#define DAEMON_LOG_LEVEL LOG_DEBUG
#define dbgp(x, ...) syslog(LOG_DEBUG, "%s:%d: " x, __func__,__LINE__, ##__VA_ARGS__)
#else
#define DAEMON_LOG_LEVEL LOG_INFO
#define dbgp(x, ...)
#endif

#define errp(x, ...) syslog(LOG_ERR, x, ##__VA_ARGS__)

#define INVOKE_CHK(invocation, err_str_fmt, ...) do {           \
        int rval = (invocation);                                \
        if (rval) {                                             \
            syslog(LOG_ERR, err_str_fmt, ##__VA_ARGS__);        \
            return (rval);                                           \
        }                                                       \
    } while (0);

#define RDB_BT_VAR "bluetooth"
#define RDB_BT_CONF_VAR RDB_BT_VAR".conf"
#define RDB_BT_CONF_ENABLE_VAR RDB_BT_CONF_VAR".enable"
#define RDB_BT_CONF_NAME_VAR RDB_BT_CONF_VAR".name"
#define RDB_BT_CONF_PAIRABLE_VAR RDB_BT_CONF_VAR".pairable"
#define RDB_BT_CONF_DISC_TIMEOUT_VAR RDB_BT_CONF_VAR".discoverable_timeout"

#define RDB_BT_OP_VAR RDB_BT_VAR".op"
#define RDB_BT_OP_PAIR_VAR RDB_BT_OP_VAR".pair"
#define RDB_BT_OP_PAIR_STATUS_VAR RDB_BT_OP_PAIR_VAR".status"

#define BTMGR_RDB_GETNAMES_BUF_SIZE 128

extern int btmgr_rpc_server_init(void);
extern void btmgr_rpc_server_destroy(void);
extern void btmgr_rpc_server_process(void);

// FIXME: This is an unofficial RDB api.
extern int rdb_delete_vars(struct rdb_session *s, char *var_prefix);

extern int percent_encoded_strncpy(char *dest, const char *src, size_t n,
                                   const char *special_chars);

#endif /* __BTMGR_PRIV_H__ */
