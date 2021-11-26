#ifndef DSM_BT_UTILS_H_12360923022016
#define DSM_BT_UTILS_H_12360923022016
/*
 * Data Stream Bluetooth Endpoint Utilities.
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

#include <rdb_ops.h>
#include <stdio.h>
#include <syslog.h>

#ifdef DEBUG
#define DAEMON_LOG_LEVEL LOG_DEBUG
#define dbgp(x, ...) syslog(LOG_DEBUG, "%s:%d: " x, __func__, __LINE__, \
                            ##__VA_ARGS__)
#define errp(x, ...) syslog(LOG_ERR, "%s:%d: " x, __func__, __LINE__, \
                            ##__VA_ARGS__)
#else
#define DAEMON_LOG_LEVEL LOG_INFO
#define dbgp(x, ...)
#define errp(x, ...) syslog(LOG_ERR, x, ##__VA_ARGS__)
#endif /* DEBUG */

#define INVOKE_CHK(invocation, err_str_fmt, ...) do {           \
        int rval = (invocation);                                \
        if (rval) {                                             \
            if (err_str_fmt) {                                  \
                errp(err_str_fmt, ##__VA_ARGS__);               \
            }                                                   \
            return (rval);                                      \
        }                                                       \
    } while (0)

#define INVOKE_CLEAN(invocation, err_str_fmt, ...) do {         \
        rval = (invocation);                                    \
        if (rval) {                                             \
            if (err_str_fmt) {                                  \
                errp(err_str_fmt, ##__VA_ARGS__);               \
            }                                                   \
            goto cleanup;                                       \
        }                                                       \
    } while (0)

#define CHECK_COND(condition, rval, err_str_fmt, ...) do {      \
        if (!(condition)) {                                     \
            if (err_str_fmt) {                                  \
                errp(err_str_fmt, ##__VA_ARGS__);               \
            }                                                   \
            return rval;                                        \
        }                                                       \
    } while (0)

#define CHECK_CLEAN(condition, err_str_fmt, ...) do {           \
        if (!(condition)) {                                     \
            if (err_str_fmt) {                                  \
                errp(err_str_fmt, ##__VA_ARGS__);               \
            }                                                   \
            goto cleanup;                                       \
        }                                                       \
    } while (0)

#define UNUSED(x) (void)(x)

extern int snprint_bytes(char *buf, size_t len, const char *bytes,
                         int nbytes, const char *delimiter);
extern int sscan_bytes(char *bytes, size_t *nbytes, const char *buf,
                       const char *delimiter);

extern int update_rdb_blob(struct rdb_session *rdb_s, const char *rdb_var_name,
                           const char *blob, int blob_len);

extern int update_rdb_int(struct rdb_session *rdb_s, const char *rdb_var_name,
                          int val, const char *format);

extern int create_rdb_blob(struct rdb_session *rdb_s, const char *rdb_var_name,
                           const char *blob, int blob_len);

extern int create_rdb_int(struct rdb_session *rdb_s, const char *rdb_var_name,
                          int val, const char *format);

#endif /* DSM_BT_UTILS_H_12360923022016 */
