/*!
 * RDB interface for the project
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

#include "rdb.h"
#include "def.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
        ### rdb functions ###
*/

/* rdb session handle */
struct rdb_session *_s = NULL;
/* rdb mutex */
static pthread_mutex_t rdb_mutex;

char _rdb_prefix[RDB_MAX_NAME_LEN];

void rdb_enter_csection()
{
    pthread_mutex_lock(&rdb_mutex);
}

void rdb_leave_csection()
{
    pthread_mutex_unlock(&rdb_mutex);
}

/*
 Close RDB.

 Params:
  None

 Return:
  None
*/
void rdb_fini()
{
    if (_s)
        rdb_close(&_s);

    /* destroy rdb mutex */
    pthread_mutex_destroy(&rdb_mutex);
}

/*
 Open RDB.

 Params:
  None

 Return:
  0 = success. Otherwise, failure.
*/
int rdb_init()
{
    DEBUG("open rdb");
    if (rdb_open(NULL, &_s) < 0) {
        ERR("cannot open Netcomm RDB (errno=%d,str='%s')", errno, strerror(errno));
        goto err;
    }

    /* create rdb mutex */
    pthread_mutex_init(&rdb_mutex, NULL);

    return 0;
err:
    return -1;
}

/*
 Subscribe RDB.

 Params:
  rdb : rdb variable to subscribe.

 Return:
  0 = success. Otherwise, failure.
*/
int __rdb_subscribe(const char *rdb)
{
    rdb_create_string(_s, rdb, "", 0, 0);

    if (rdb_subscribe(_s, rdb) < 0) {
        ERR("[RDB] cannot subscribe variable (rdb=%s) - %s", rdb, strerror(errno));
        goto err;
    }

    return 0;

err:
    return -1;
}

/*
 Read RDB with no error logging.

 Params:
  str : buffer to get RDB value.
  str_len : size of buffer.
  rdb : rdb to read.

 Return:
  Return RDB value. Otherwise, blank string.
*/
char *_rdb_get_str_quiet(char *str, int str_len, const char *rdb)
{
    int stat;

    if (str_len)
        *str = 0;

    if ((stat = rdb_get(_s, rdb, str, &str_len)) < 0) {
        *str = 0;
    }

    return str;
}

int _rdb_exists(const char *rdb)
{
    int stat;
    char dummy;
    int len = sizeof(dummy);

    stat = rdb_get(_s, rdb, &dummy, &len);

    return !(stat < 0) || (stat == -EOVERFLOW);
}

/*
 Read RDB.

 Params:
  str : buffer to get RDB value.
  str_len : size of buffer.
  rdb : rdb to read.

 Return:
  Return RDB value. Otherwise, blank string.
*/
char *_rdb_get_str(char *str, int str_len, const char *rdb)
{
    int stat;

    if ((stat = rdb_get(_s, rdb, str, &str_len)) < 0) {
        ERR("[RDB] failed to get RDB (rdb='%s',stat=%d,str='%s')", rdb, stat, strerror(errno));
        *str = 0;
    }

    return str;
}

/*
 Read RDB as long long integer.

 Params:
  rdb : rdb to read.

 Return:
  Return integer RDB value. Otherwise, zero.
*/
long long _rdb_get_int(const char *rdb)
{
    char str[RDB_MAX_VAL_LEN];
    int str_len = sizeof(str);

    return atoll(_rdb_get_str(str, str_len, rdb));
}

/*
 Read RDB as long long integer with no error logging.

 Params:
  rdb : rdb to read.

 Return:
  Return integer RDB value. Otherwise, zero.
*/
long long _rdb_get_int_quiet(const char *rdb)
{
    char str[RDB_MAX_VAL_LEN];
    int str_len = sizeof(str);

    return atoll(_rdb_get_str_quiet(str, str_len, rdb));
}

/*
 Write a value to RDB

 Params:
  rdb : rdb to write.
  val : value to write the rdb variable.

 Return:
  0 = success. Otherwise, failure.

 Note:
  This function creates RDB variable when the value does not exist.
*/
int _rdb_set_str(const char *rdb, const char *val)
{
    int stat;

    /* use blank string if val is NULL */
    if (!val)
        val = "";

    /* set rdb */
    if ((stat = rdb_set_string(_s, rdb, val)) < 0) {
        if (stat == -ENOENT) {
            stat = rdb_create_string(_s, rdb, val, 0, 0);
        }
    }

    /* log error message if it failed */
    if (stat < 0)
        ERR("[RDB] failed to get RDB (rdb='%s',stat=%d,str='%s')", rdb, stat, strerror(errno));
    else
        DEBUG("[RDB] write '%s' ==> [%s]", rdb, val);

    return stat;
}

int _rdb_set_reset(const char *rdb)
{
    return _rdb_set_str(rdb, "");
}

int _rdb_set_uint(const char *rdb, unsigned long long val)
{
    char str[RDB_MAX_VAL_LEN];

    snprintf(str, sizeof(str), "%llu", val);
    return _rdb_set_str(rdb, str);
}

int _rdb_set_tenths_decimal(const char *rdb, long double val)
{
    char str[RDB_MAX_VAL_LEN];

    snprintf(str, sizeof(str), "%.1LF", val);
    return _rdb_set_str(rdb, str);
}

int _rdb_set_int(const char *rdb, long long val)
{
    char str[RDB_MAX_VAL_LEN];

    snprintf(str, sizeof(str), "%lld", val);
    return _rdb_set_str(rdb, str);
}

const char *_rdb_get_prefix_rdb(char *var, int var_len, const char *rdb)
{
    snprintf(var, var_len, "%s%s", _rdb_prefix, rdb);

    return var;
}

const char *_rdb_get_suffix_rdb(char *var, int var_len, const char *rdb, const char *suffix)
{
    snprintf(var, var_len, "%s%s%s", _rdb_prefix, rdb, suffix);

    return var;
}

/*
 Reset RDB values that starts with the prefix

 Params:
  prefix : prefix to reset.

 Return:
  None
*/
void reset_rdb_sets(const char *prefix)
{
    struct dbenum_t *dbenum;
    struct dbenumitem_t *item;
    int total_rdbs;

    char *rdb_sets_prefix;
    int rdb_sets_prefix_len;

    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    rdb_sets_prefix = strdup(_rdb_get_prefix_rdb(var, var_len, prefix));
    if (!rdb_sets_prefix) {
        ERR("failed to allocate for rdb variable");
        goto err;
    }

    rdb_sets_prefix_len = strlen(rdb_sets_prefix);

    /* reset voice call status rdb */
    dbenum = dbenum_create(_s, 0);
    if (dbenum) {
        DEBUG("search rdb RDB to reset (prefix=%s)", rdb_sets_prefix);

        total_rdbs = dbenum_enumDbByNames(dbenum, rdb_sets_prefix);

        DEBUG("init. voice status RDBs (total_rdbs=%d)", total_rdbs);

        item = dbenum_findFirst(dbenum);
        while (item) {
            if (!strncmp(item->szName, rdb_sets_prefix, rdb_sets_prefix_len)) {
                if (rdb_set_string(_s, item->szName, "") < 0) {
                    DEBUG("rdb_set_string(%s) failed in start_devices() - %s", item->szName, strerror(errno));
                } else {
                    DEBUG("reset RDB [%s]", item->szName);
                }
            }

            item = dbenum_findNext(dbenum);
        }

        dbenum_destroy(dbenum);
    }

    free(rdb_sets_prefix);

err:
    __noop();
    return;
}
