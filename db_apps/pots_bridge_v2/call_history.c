/*
 * call_history module maintains voice call history.
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

#include "call_track.h"
#include "ezrdb.h"
#include "qmirdbctrl.h"
#include "uthash.h"
#include "dbenum.h"
#include "utils.h"
#include <math.h>
#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#define RDB_CALL_HISTORY_PREFIX "voice_call.call_history."
#define RDB_CALL_HISTORY_INDEX "voice_call.call_history.index"

#define RDB_CALL_HISTORY_COUNT 100
#define CALL_HISTORY_MAX_ENTRY_LEN 1024

static int _last_written_rdb_index = -1;
static struct dbenum_t* _dbenum;
struct rdb_session* _rdb_session;

#define UID_MAX 999

static int _uid = -1;

/* key structure for call history */
struct call_history_key_t {
    int rdb_index;
    int uid;
};

/* call history entry */
struct call_history_entry_t {
    struct call_history_key_t key;
    char* value;

    UT_hash_handle hh;
};

struct call_history_entry_t* _call_history_hash_table;

/**
 * @brief compares one Call history entry to the other with RDB index.
 *
 * @param a is a Call history entry.
 * @param b is a Call history entry.
 *
 * @return negative value when a<b, 0 when a==b and positive value when a>b.
 */
int call_history_sort_by_rdb_index(struct call_history_entry_t* a, struct call_history_entry_t* b)
{
    int a_rdb_index;
    int b_rdb_index;

    a_rdb_index = a->key.rdb_index > _last_written_rdb_index ? a->key.rdb_index : (a->key.rdb_index +
                  RDB_CALL_HISTORY_COUNT);
    b_rdb_index = b->key.rdb_index > _last_written_rdb_index ? b->key.rdb_index : (b->key.rdb_index +
                  RDB_CALL_HISTORY_COUNT);

    return a_rdb_index - b_rdb_index;
}

/**
 * @brief destroys Call history hash table.
 */
void call_history_mgmt_unload(void)
{
    struct call_history_entry_t* call_history_entry;
    struct call_history_entry_t* tmp;

    HASH_ITER(hh, _call_history_hash_table, call_history_entry, tmp) {

        HASH_DEL(_call_history_hash_table, call_history_entry);

        free(call_history_entry->value);
        free(call_history_entry);
    }
}

/**
 * @brief saves Call history hash table into RDB.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int call_history_mgmt_save(void)
{
    struct dbenumitem_t* item;
    int rdb_idex;

    struct call_history_entry_t* call_history_entry;
    struct call_history_entry_t* tmp;

    char* rdb_index_str;

    char rdb[sizeof(RDB_CALL_HISTORY_PREFIX) + (int)(log10(RDB_CALL_HISTORY_COUNT)) + 1];

    HASH_SRT(hh, _call_history_hash_table, call_history_sort_by_rdb_index);

    /* delete RDBs */
    item = dbenum_findFirst(_dbenum);
    while (item) {

        if (!strncmp(item->szName, RDB_CALL_HISTORY_PREFIX, strlen_static(RDB_CALL_HISTORY_PREFIX))) {

            /* bypass if not a number */
            rdb_index_str = item->szName + strlen_static(RDB_CALL_HISTORY_PREFIX);
            if (isdigit(*rdb_index_str)) {
                rdb_delete(_rdb_session, item->szName);
            }

        }

        item = dbenum_findNext(_dbenum);
    }

    /* initiate last writen rdb index */
    _last_written_rdb_index = -1;
    ezrdb_set_int_persist(RDB_CALL_HISTORY_INDEX, _last_written_rdb_index);

    /* write all history */
    rdb_idex = 0;
    HASH_ITER(hh, _call_history_hash_table, call_history_entry, tmp) {
        snprintf(rdb, sizeof(rdb), "%s%d", RDB_CALL_HISTORY_PREFIX, rdb_idex);
        ezrdb_set_str_persist(rdb, call_history_entry->value);

        _last_written_rdb_index = rdb_idex;
        ezrdb_set_int_persist(RDB_CALL_HISTORY_INDEX, _last_written_rdb_index);

        rdb_idex++;
    }

    return 0;
}

/**
 * @brief deletes a Call history entry from Call history hash table.
 *
 * @param id is a string ID in the format of "<RDB index>:<UID>"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int call_history_mgmt_del(const char* id)
{
    int param_cnt;

    struct call_history_key_t key;
    struct call_history_entry_t* call_history_entry;

    /* <rdb index>:<uid> */
    param_cnt = sscanf(id, "%d:%d", &key.rdb_index, &key.uid);
    if (param_cnt != 2) {
        syslog(LOG_ERR, "call history ID has incorrect format (id=%s)", id);
        goto err;
    }

    HASH_FIND(hh, _call_history_hash_table, &key, sizeof(key), call_history_entry);

    /* delete entry */
    if (call_history_entry) {
        syslog(LOG_DEBUG, "[CALL-HISTORY] delete (rdb=%d,uid=%d,val='%s')", call_history_entry->key.rdb_index,
               call_history_entry->key.uid, call_history_entry->value);

        HASH_DEL(_call_history_hash_table, call_history_entry);
        free(call_history_entry->value);
        free(call_history_entry);
    } else {
        syslog(LOG_DEBUG, "[CALL-HISTORY] entry not found (id='%s')", id);
    }

    return 0;

err:
    return -1;
}

/**
 * @brief loads Call history RDB into Call history hash table.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int call_history_mgmt_load(void)
{
    struct dbenumitem_t* item;
    char call_history_ent_str[CALL_HISTORY_MAX_ENTRY_LEN];

    struct call_history_entry_t* call_history_entry;

    const char* rdb_index_str;

    const char* val;

    int total_rdb_loaded;

    dbenum_enumDbByNames(_dbenum, RDB_CALL_HISTORY_PREFIX);

    total_rdb_loaded = 0;
    item = dbenum_findFirst(_dbenum);
    while (item) {

        call_history_entry = NULL;

        /* verify prefix */
        if (strncmp(item->szName, RDB_CALL_HISTORY_PREFIX, strlen_static(RDB_CALL_HISTORY_PREFIX))) {
            goto loop_cont;
        }

        /* bypass if not a number */
        rdb_index_str = item->szName + strlen_static(RDB_CALL_HISTORY_PREFIX);
        if (!isdigit(*rdb_index_str)) {
            goto loop_cont;
        }

        /* get history entry from RDB */
        if (ezrdb_get(item->szName, call_history_ent_str, sizeof(call_history_ent_str)) < 0) {
            syslog(LOG_ERR, "failed to read RDB (rdb='%s',error='%s')", item->szName, strerror(errno));
            goto loop_err;
        }

        /* allocate history entry */
        call_history_entry = calloc(1, sizeof(*call_history_entry));
        if (!call_history_entry) {
            syslog(LOG_ERR, "failed to allocate call history entry");
            goto loop_err;
        }

        /*
            <uid>,<call direction>,<caller ID>,<privacy information>,<timestamp>,<duration>
        */

        /* get rdb index */
        call_history_entry->key.rdb_index = atoi(rdb_index_str);
        /* get uid */
        call_history_entry->key.uid = atoi(call_history_ent_str);
        /* get value */
        call_history_entry->value = strdup(call_history_ent_str);
        if (!call_history_entry->value) {
            syslog(LOG_ERR, "failed to allocate call history entry value");
            goto loop_err;
        }

        total_rdb_loaded++;

        if (call_history_entry->key.uid > _uid) {
            _uid = call_history_entry->key.uid;
        }

        if (call_history_entry->key.rdb_index > _last_written_rdb_index) {
            _last_written_rdb_index = call_history_entry->key.rdb_index;
        }

        HASH_ADD(hh, _call_history_hash_table, key, sizeof(struct call_history_key_t), call_history_entry);

loop_cont:
        item = dbenum_findNext(_dbenum);
        continue;

loop_err:
        if (call_history_entry) {
            free(call_history_entry->value);
        }
        free(call_history_entry);

        item = dbenum_findNext(_dbenum);
    }

    /* get index */
    val = ezrdb_get_str(RDB_CALL_HISTORY_INDEX);
    if (*val) {
        _last_written_rdb_index = atoll(val);
    }

    syslog(LOG_DEBUG, "[CALL-HISTORY] RDB loaded (total_rdb=%d,rdb_index=%d,uid=%d", total_rdb_loaded,
           _last_written_rdb_index, _uid);
    return 0;
}

/**
 * @brief write a call history entry to RDB
 *
 * @param call is a call to write
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int call_history_write(struct call_track_t* call)
{
    char rdb[RDB_MAX_VAL_LEN];
    char val[RDB_MAX_VAL_LEN];
    int  duration = -1;

    _uid = (_uid + 1) % UID_MAX;
    _last_written_rdb_index = (_last_written_rdb_index + 1) % RDB_CALL_HISTORY_COUNT;

    snprintf(rdb, sizeof(rdb), "%s%d", RDB_CALL_HISTORY_PREFIX, _last_written_rdb_index);

    /* remove the next existing record for call history readers to avoid race-condition */
    ezrdb_set_str_persist(rdb, "");

    /* set new index */
    ezrdb_set_int_persist(RDB_CALL_HISTORY_INDEX, _last_written_rdb_index);

    /*
        <uid>,<call direction>,<caller ID>,<privacy information>,<timestamp>,<duration>
    */

    if (call->mt_call_declined) {
        duration = -3;
    } else if (call->call_blocked) {
        duration = -2;
    } else if (call->duration_valid) {
        duration = call->duration;
    }
    snprintf(val,
             sizeof(val),
             "%d,%s,%s,%s,%d,%d,%d",
             _uid,
             call->call_dir == call_dir_incoming ? "in" : "out",
             call->cid_num ? call->cid_num : "",
             call->cid_num_pi ? call->cid_num_pi : "",
             (int)call->timestamp_created,
             duration,
             (int)call->system_timeoffset
            );

    /* set rdb */
    ezrdb_set_str_persist(rdb, val);

    syslog(LOG_DEBUG, "[CALL-HISTORY] write '%s' ==> '%s'", val, rdb);

    return 0;
}

/**
 * @brief initializes call history
 *
 * @return 0 when it succeeds. Otherwise, -1
 */
int call_history_init(void)
{
    _rdb_session = ezrdb_get_session();
    _dbenum = dbenum_create(_rdb_session, 0);

    /* read index and destroy hash table immediately - not needed to maintain the hash table */
    call_history_mgmt_load();
    call_history_mgmt_unload();

    syslog(LOG_DEBUG, "[CALL-HISTORY] call_history index read (index=%d)", _last_written_rdb_index);

    return 0;
}

/**
 * @brief finalizes call history
 *
 */
void call_history_fini(void)
{

    dbenum_destroy(_dbenum);
}
