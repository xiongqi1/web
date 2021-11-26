/*!
 * C header file of RDB interface for the project
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#ifndef __RDB_H__
#define __RDB_H__

#include "dbenum.h"
#include "rdb_ops.h"

#define RDB_MAX_VAL_LEN 1024
#define RDB_MAX_NAME_LEN 256

#define _rdb_prefix_call(func, rdb, ...) func(_rdb_get_prefix_rdb(var, var_len, rdb), ##__VA_ARGS__)
#define _rdb_suffix_call(func, rdb, suffix, ...) func(_rdb_get_suffix_rdb(var, var_len, rdb, suffix), ##__VA_ARGS__)

extern struct rdb_session *_s;
extern char _rdb_prefix[RDB_MAX_NAME_LEN];

/* rdb.c */
void rdb_enter_csection(void);
void rdb_leave_csection(void);
void rdb_fini(void);
int rdb_init(void);
int __rdb_subscribe(const char *rdb);
char *_rdb_get_str_quiet(char *str, int str_len, const char *rdb);
int _rdb_exists(const char *rdb);
char *_rdb_get_str(char *str, int str_len, const char *rdb);
long long _rdb_get_int(const char *rdb);
long long _rdb_get_int_quiet(const char *rdb);
int _rdb_set_str(const char *rdb, const char *val);
int _rdb_set_reset(const char *rdb);
int _rdb_set_uint(const char *rdb, unsigned long long val);
int _rdb_set_int(const char *rdb, long long val);
const char *_rdb_get_prefix_rdb(char *var, int var_len, const char *rdb);
const char *_rdb_get_suffix_rdb(char *var, int var_len, const char *rdb, const char *suffix);
void reset_rdb_sets(const char *prefix);
int _rdb_set_tenths_decimal(const char *rdb, long double val);
#endif
