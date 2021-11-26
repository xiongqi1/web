/*
 * ezrdb provides RDB access functions.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE

#include "ezrdb.h"
#include "qmirdbctrl.h"
#include <rdb_ops.h>
#include <stddef.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static struct rdb_session* _s = NULL; /* rdb session handle */


struct rdb_session* ezrdb_get_session(void)
{
    return _s;
}

/**
 * @brief closes RDB handle.
 */
void ezrdb_close(void)
{
    if (_s)
        rdb_close(&_s);
    _s = NULL;
}

/**
 * @brief opens RDB handle.
 *
 * @return
 */
int ezrdb_open(void)
{
    /* open rdb */
    if (rdb_open(NULL, &_s) < 0) {
        syslog(LOG_ERR, "cannot open Netcomm RDB");
        goto err;
    }

    return 0;
err:
    return -1;
}

/**
 * @brief sets RDB variable.
 *
 * @param rdb is RDB variable.
 * @param val is RDB value.
 *
 * @return
 */
int ezrdb_set_str(const char* rdb, const char* val)
{
    int stat;

    /* set rdb */
    if ((stat = rdb_set_string(_s, rdb , val)) < 0) {
        if (stat == -ENOENT) {
            stat = rdb_create_string(_s, rdb, val, 0, 0);
        }
    }

    /* log error message if it failed */
    if (stat < 0) {
        syslog(LOG_ERR, "[RDB] failed to set RDB (rdb='%s',val='%s',stat=%d) - %s", rdb, val, stat, strerror(errno));
    }

    return stat;
}

/**
 * @brief sets persistant RDB variable.
 *
 * @param rdb is RDB variable.
 * @param val is RDB value.
 *
 * @return
 */
int ezrdb_set_str_persist(const char* rdb, const char* val)
{
    int stat;

    /* set rdb */
    stat = rdb_create_string(_s, rdb, val, PERSIST, 0);
    if (stat == -EEXIST) {
        stat = rdb_set_string(_s, rdb , val);
    }

    /* log error message if it failed */
    if (stat < 0) {
        syslog(LOG_ERR, "[RDB] failed to set RDB (rdb='%s',val='%s',stat=%d) - %s", rdb, val, stat, strerror(errno));
    }

    return stat;
}

/**
 * @brief subscribes RDB variable.
 *
 * @param rdb is RDB variable to subscribe.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int ezrdb_subscribe(const char* rdb)
{
    rdb_create_string(_s, rdb, "", 0, 0);

    if (rdb_subscribe(_s, rdb) < 0)  {
        syslog(LOG_ERR, "cannot subscribe variable (rdb=%s) - %s", rdb, strerror(errno));
        goto err;
    }

    return 0;

err:
    return -1;
}

/**
 * @brief performs RDB action to an array of RDB variables.
 *
 * @param rdb[] is an array of RDB variables.
 * @param rdb_count is total number of RDB variables.
 * @param cb is call back function to call with each of RDB variables.
 * @param ref is reference data for call back function.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int ezrdb_perform_array(const char* rdb[], int rdb_count, _ez_batch_rdb_perform_cb cb, void* ref)
{
    int i;

    for (i = 0; i < rdb_count; i++) {
        if (cb(*rdb++, ref) < 0) {
            goto err;
        }
    }

    return 0;

err:
    return -1;
}

/**
 * @brief call back function to subscribe an array of RDB variables.
 *
 * @param rdb is RDB variable to subscribe.
 * @param ref is reference data.
 *
 * @return always 0.
 */
int ezrdb_subscribe_array_cb(const char* rdb, void* ref)
{
    ezrdb_subscribe(rdb);

    return 0;
}

/**
 * @brief subscribe an array of RDB variables.
 *
 * @param rdb[] is an array of RDB variables.
 * @param rdb_count is total number of RDB varaibles.
 *
 * @return 0 when succeeds. Otherwise, it returns negative error code.
 */
int ezrdb_subscribe_array(const char* rdb[], int rdb_count)
{
    return ezrdb_perform_array(rdb, rdb_count, ezrdb_subscribe_array_cb, NULL);
}

/**
 * @brief call back function to set an array of RDB variables.
 *
 * @param rdb is RDB variable to subscribe.
 * @param ref is reference data.
 *
 * @return always 0.
 */
int ezrdb_set_array_cb(const char* rdb, void* ref)
{
    const char* val = (const char*)ref;

    ezrdb_set_str(rdb, val);

    return 0;
}

/**
 * @brief sets an array of RDB variables.
 *
 * @param rdb[] is an array of RDB variables.
 * @param rdb_count is total number of RDB varaibles.
 * @param val is RDB value.
 *
 * @return 0 when succeeds. Otherwise, it returns negative error code.
 */
int ezrdb_set_array(const char* rdb[], int rdb_count, const char* val)
{
    return ezrdb_perform_array(rdb, rdb_count, ezrdb_set_array_cb, (void*)val);
}

/**
 * @brief gets RDB variable.
 *
 * @param szName is RDB variable.
 * @param pValue is a pointer to RDB value buffer.
 * @param len is length of RDB value buffer.
 *
 * @return
 */
int ezrdb_get(const char* szName, char* pValue, int len)
{
    return rdb_get_string(_s, szName, pValue, len);
}


/**
 * @brief gets RDB variable.
 *
 * @param rdb is RDB variable to get.
 *
 * @return RDB value when it succeeds. Otherwise, it returns zero-length string.
 */
const char* ezrdb_get_str(const char* rdb)
{
    static char str[RDB_MAX_VAL_LEN];
    int str_len = sizeof(str);
    int stat;

    if ((stat = rdb_get(_s, rdb, str, &str_len)) < 0) {
        syslog(LOG_ERR, "[RDB] failed to get RDB (rdb='%s',stat=%d) - %s", rdb, stat, strerror(errno));
        *str = 0;
    }

    return str;
}

/**
 * @brief gets RDB variable as integer.
 *
 * @param rdb is RDB variable to get.
 *
 * @return
 */
uint64_t ezrdb_get_int(const char* rdb)
{
    return atoll(ezrdb_get_str(rdb));
}

int ezrdb_set_int(const char* rdb, uint64_t val)
{
    char str[RDB_MAX_VAL_LEN];

    snprintf(str, sizeof(str), "%llu", val);
    return ezrdb_set_str(rdb, str);
}

int ezrdb_set_int_persist(const char* rdb, uint64_t val)
{
    char str[RDB_MAX_VAL_LEN];

    snprintf(str, sizeof(str), "%lld", val);
    return ezrdb_set_str_persist(rdb, str);
}
