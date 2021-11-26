#ifndef __RDB_H__16052018
#define __RDB_H__16052018

/*!
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless limited.
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

#include "rdb_ops.h"

#define RDB_VARIABLE_MAX_LEN	1024
#define RDB_VARIABLE_NAME_MAX_LEN	256
#define RDB_ENUM_MAX_LEN	4096

int rdb_set_value(const char* var, const char* val, int persist);
const char* rdb_get_value(const char* var);
int rdb_del(const char* var);

char* rdb_get_printf(const char* fmt, ...);
int rdb_set_printf(const char* rdb, const char* fmt, ...);
int rdb_del_printf(const char* var, const char* fmt, ...);

const char* rdb_var_printf(const char* fmt, ...);

int rdb_init(void);
void rdb_fini(void);
char* rdb_enum(const char* name, int flags);

int rdb_get_handle();
struct rdb_session* rdb_get_struct();

typedef int (*rdb_regex_enum_callback)(const char* name, int ref);
int rdb_regex_enum(const char* name, const char* regex, rdb_regex_enum_callback cb, int ref);

int rdb_subscribe_own(const char* rdb_var, int persist);

#endif


