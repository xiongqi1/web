/*!
 * Netcomm RDB interface.
 *
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

#include "rdb.h"

#include <regex.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>


static char _val[RDB_VARIABLE_MAX_LEN];
static char _var[RDB_VARIABLE_NAME_MAX_LEN];
static char enumbuf[RDB_ENUM_MAX_LEN];

static struct rdb_session* _rdb = 0;

/**
 * @brief Subscribes a RDB variable. This function creates if the RDB variable does not exists.
 *
 * @param rdb_var is a RDB variable to create.
 * @param persist is a flag to create a persist RDB variable only if the RDB does not exist.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int rdb_subscribe_own(const char* rdb_var, int persist)
{
    int flags = 0;

    /* use persist flags if required */
    if (persist) {
        flags |= PERSIST;
    }

    syslog(LOG_DEBUG, "rdb_set (var=%s,persist=%d)", rdb_var, persist);

    /* subscribe */
    if (rdb_subscribe(_rdb, rdb_var) < 0) {

        /* return error if failed */
        if (errno != ENOENT) {
            syslog(LOG_ERR, "failed to subscribe (rdb=%s) - %s", rdb_var, strerror(errno));
            goto err;
        }

        /* create rdb */
        if (rdb_create_string(_rdb, rdb_var, "", flags, 0) < 0) {
            syslog(LOG_ERR, "failed to create rdb to subscribe (rdb=%s) - %s", rdb_var, strerror(errno));
            goto err;
        }

        /* re-subscribe */
        if (rdb_subscribe(_rdb, rdb_var) < 0) {
            syslog(LOG_ERR, "failed to subscribe after creating rdb to subscribe (rdb=%s) - %s", rdb_var, strerror(errno));
            goto err;
        }
    }

    return 0;
err:
    return -1;
}

/**
 * @brief Return the RDB handle.
 *
 * @return RDB handle.
 */
int rdb_get_handle()
{
    if (!_rdb) {
        return -1;
    }

    return rdb_fd(_rdb);
}

/**
 * @brief Return RDB structure pointer.
 *
 * @return  RDB structure pointer.
 */
struct rdb_session* rdb_get_struct()
{
    return _rdb;
}

/**
 * @brief Enumerates all RDB variables by flags.
 *
 * @param name is a RDB name prefix to enumerate.
 * @param flags is a flag to enumerate RDB variables.
 *
 * @return Semicolon delimited RDB variables. NULL when it fails.
 */
char* rdb_enum(const char* name, int flags)
{
    int len;

    len = sizeof(enumbuf) - 1;

    *enumbuf = 0;
    if (rdb_getnames(_rdb, name, enumbuf, &len, flags) < 0) {
        return NULL;
    }

    if (len < 0) {
        return NULL;
    }

    enumbuf[len] = 0;

    return enumbuf;
}

/**
 * @brief Enumerate RDB variables that are matching by a regular expression.
 *
 * @param name is a RDB name prefix to enumerate.
 * @param regex is a regular expression to match RDB names.
 * @param cb is a call-back function to call when a RDB name is matched.
 * @param ref is a user reference for the call-back function.
 *
 * @return
 */
int rdb_regex_enum(const char* name, const char* regex, rdb_regex_enum_callback cb, int ref)
{
    char* names;
    char* sp;
    char* token;
    int r;

    regex_t re;

    /* compile regex */
    if (regcomp(&re, regex, REG_NOSUB) != 0) {
        syslog(LOG_ERR, "failed to compile regex (wildcard=%s) - %s", regex, strerror(errno));
        goto err;
    }

    /* get name */
    names = rdb_enum(name, 0);

    /* bypass if no RDB */
    if (!names) {
        syslog(LOG_DEBUG, "no hwdev RDBs found (name=%s)", name);
        goto fini;
    }

    syslog(LOG_DEBUG, "got RDBs (name=%s,names=%s)", name, names);

    token = strtok_r(names, ";&", &sp);
    while (token) {

        syslog(LOG_DEBUG, "got token (token=%s)", token);

        r = regexec(&re, token, 0, NULL, 0);

        /* call callback if matching */
        if (r == 0) {
            syslog(LOG_DEBUG, "call callback function (token=%s,ref=%d)", token, ref);
            if (cb(token, ref) < 0) {
                break;
            }
        }

        /* get token */
        token = strtok_r(NULL, ";&", &sp);
    }

fini:
    return 0;
err:
    return -1;
}

/**
 * @brief Delete a RDB variable.
 *
 * @param var is a RDB variable name to delete.
 *
 * @return 0 when it succeeds. Otherwise, corresponding errors.
 */
int rdb_del(const char* var)
{
    return rdb_delete(_rdb, var);
}

/**
 * @brief Generate a formatted RDB variable name.
 *
 * @param fmt is a printf() format to generate a RDB variable name.
 * @param ... are printf() arguments.
 *
 * @return Generated RDB variable name.
 */
const char* rdb_var_printf(const char* fmt, ...)
{
    static char var[RDB_VARIABLE_NAME_MAX_LEN];

    va_list ap;

    va_start(ap, fmt);

    vsnprintf(var, sizeof(var), fmt, ap);

    va_end(ap);

    return var;
}

/**
 * @brief Write a string into a RDB varaible.
 *
 * @param var is a RDB variable to write a string into.
 * @param fmt is a printf() format for the RDB value.
 * @param ... are printf() arguments to write.
 *
 * @return
 */
int rdb_set_printf(const char* var, const char* fmt, ...)
{
    int stat;

    va_list ap;

    va_start(ap, fmt);

    vsnprintf(_val, sizeof(_val), fmt, ap);
    stat = rdb_set_value(var, _val, 0);

    va_end(ap);

    return stat;
}


/**
 * @brief Read a RDB variable as a string.
 *
 * @param fmt is a RDB variable to read.
 * @param ... are printf() arguments to read.
 *
 * @return
 */
char* rdb_get_printf(const char* fmt, ...)
{
    va_list ap;
    int len;

    va_start(ap, fmt);

    vsnprintf(_var, sizeof(_var), fmt, ap);
    len = sizeof(_val);

    if (rdb_get(_rdb, _var, _val, &len) < 0) {
        *_val = 0;
    }

    va_end(ap);

    return _val;
}

/**
 * @brief Write a string to a RDB variable.
 *
 * @param var is a RDB variable.
 * @param val is a string to write.
 * @param persist 0 if it is volatile. Otherwise, 1.
 *
 * @return 0 when it succeeds. Otherwise, corresponding errors.
 */
int rdb_set_value(const char* var, const char* val, int persist)
{
    int rc;

    int flags = 0;

    syslog(LOG_DEBUG, "rdb_set (var=%s,val=\"%s\",persist=%d)", var, val, persist);

    if (persist) {
        flags |= PERSIST;
    }

    if ((rc = rdb_set_string(_rdb, var, val)) < 0) {
        if (errno == ENOENT) {
            rc = rdb_create_string(_rdb, var, val, flags, 0);
        }
    }

    if (rc < 0) {
        syslog(LOG_ERR, "failed to write to rdb(var=%s,val=\"%s\") - %s", var, val, strerror(errno));
    }

    return rc;
}

/**
 * @brief Read an RDB variable.
 *
 * @param var is an RDB variable name to read.
 *
 * @return RDB variable.
 */
const char* rdb_get_value(const char* var)
{
    int len;

    len = sizeof(_val);

    if (rdb_get(_rdb, var, _val, &len) < 0) {
        return NULL;
    }

    _val[sizeof(_val) - 1] = 0;

    return _val;
}


/**
 * @brief Initiates RDB to use.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int rdb_init(void)
{
    // open rdb database
    if (rdb_open(NULL, &_rdb) < 0) {
        syslog(LOG_ERR, "failed to open rdb driver - %s", strerror(errno));
        goto err;
    }

    return 0;
err:
    return -1;
}


/**
 * @brief Finalise RDB to destroy.
 */
void rdb_fini(void)
{
    if (_rdb) {
        rdb_close(&_rdb);
    }
}

