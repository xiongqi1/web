/*
 * block calls module maintains phone numbers to block.
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

#include "call_prefix.h"
#include "block_calls.h"
#include "ezrdb.h"
#include "utils.h"
#include "pbx_common.h"

#include <dbenum.h>
#include <rdb_ops.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

static struct blocked_phone_number_t* _pn = NULL;
static struct dbenum_t* _dbenum = NULL;

/**
 * @brief frees the hash table for blocked phone number.
 */
void block_calls_free(void)
{
    struct blocked_phone_number_t* pn;
    struct blocked_phone_number_t* tmp;

    HASH_ITER(hh, _pn, pn, tmp) {
        HASH_DEL(_pn, pn);
        free(pn);
    }
}

/**
 * @brief rebuild the hash table for blocked phone number.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int block_calls_rebuild(void)
{
    struct dbenumitem_t* item;
    struct dbenumitem_t* item_next;
    struct blocked_phone_number_t* pn;

    const char *num_ptr;
    char phone_number[MAX_NUM_OF_PHONE_NUM + 1];
    char rdb_name[RDB_MAX_NAME_LEN];
    char rdb_number[RDB_MAX_NAME_LEN];
    char sscan_format[RDB_MAX_NAME_LEN];
    int rdb_index;
    int count;

    /* free existing hash items */
    block_calls_free();

    dbenum_enumDbByNames(_dbenum, RDB_BLOCK_CALL_PREFIX);

    item_next = dbenum_findFirst(_dbenum);
    while ((item = item_next) != NULL) {

        item_next = dbenum_findNext(_dbenum);

        /* bypass if prefix is not matching */
        if (strncmp(item->szName, RDB_BLOCK_CALL_PREFIX, strlen_static(RDB_BLOCK_CALL_PREFIX))) {
            continue;
        }

        /* skip if RDB name is in the wrong format */
        snprintf(rdb_name, sizeof(rdb_name), "%s", item->szName + strlen_static(RDB_BLOCK_CALL_PREFIX));
        snprintf(sscan_format, sizeof(sscan_format), "%%d.%%%ds", RDB_MAX_NAME_LEN - 1);
        count = sscanf(rdb_name, sscan_format, &rdb_index, rdb_number);
        if (count != 2) {
            syslog(LOG_ERR, "skip block call number - incorrect RDB name format (rdb=%s)", item->szName);
            continue;
        }
        rdb_number[RDB_MAX_NAME_LEN - 1] = 0;

        /* skip if RDB name is not 'number' */
        if (strcmp(rdb_number, RDB_BLOCK_CALL_NUMBER)) {
            syslog(LOG_DEBUG, "skip, (rdb=%s)", item->szName);
            continue;
        }

        /* get rdb */
        if (ezrdb_get(item->szName, phone_number, sizeof(phone_number)) < 0) {
            syslog(LOG_ERR, "skip block call number - failed to read RDB (rdb='%s',error='%s')", item->szName, strerror(errno));
            continue;
        }

        num_ptr = call_prefix_get_incoming_cid_for_rj11(phone_number);

        /* bypass if RDB is blank */
        if (!*num_ptr) {
            syslog(LOG_INFO, "skip block call number - blank RDB (rdb=%s)", item->szName);
            continue;
        }

        /* allocate blocked phone number */
        pn = malloc(sizeof(struct blocked_phone_number_t));
        if (!pn) {
            syslog(LOG_ERR, "failed to allocate blocked phone number - %s", strerror(errno));
            goto err;
        }

        /* add phone number to hash */
        snprintf(pn->phone_number, sizeof(pn->phone_number), "%s", num_ptr);
        syslog(LOG_DEBUG, "add phone number to block (num=%s)", pn->phone_number);
        HASH_ADD_STR(_pn, phone_number, pn);
    }

    return 0;

err:
    return -1;
}

/**
 * @brief looks up a phone number in the blocked phone hash table.
 *
 * @param num
 *
 * @return TRUE when it finds. Otherwise, FALSE.
 */
int block_calls_is_blocked(const char* num)
{
    struct blocked_phone_number_t* pn = NULL;

    HASH_FIND_STR(_pn, num, pn);

    return pn != NULL;
}

/**
 * @brief finalizes block calls object.
 */
void block_calls_fini(void)
{
    block_calls_free();
    dbenum_destroy(_dbenum);
}

/**
 * @brief initiates block calls object.
 */
void block_calls_init(void)
{
    struct rdb_session* rdb_session;

    /* create dbenum */
    rdb_session = ezrdb_get_session();
    _dbenum = dbenum_create(rdb_session, 0);

    block_calls_rebuild();
}
