/*
 * Measurement history writes measured history into RDB.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
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

#include "meas_history.h"
#include "rdb.h"
#include "rdb_names.h"

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*

 4K buffer for history

 It uses RDB driver's 4K block size for a better efficiency. This 4K buffer allows up to 200 records in a worst
 case scenario.

 (log (2^64) + 1 ) * 200 ~= 4K

*/

#define MEAS_HISTORY_RDB_MAX_LEN 4096

const char* history_rdb_names[] = {
    [MEAS_HISTORY_RECORD_RECV_AVG_BPS] = "AverageDownlinkThroughput",
    [MEAS_HISTORY_RECORD_SENT_AVG_BPS] = "AverageUplinkThroughput",
    [MEAS_HISTORY_RECORD_RECV_PEAK_BPS] = "PeakDownlinkThroughput",
    [MEAS_HISTORY_RECORD_SENT_PEAK_BPS] = "PeakUplinkThroughput",
    [MEAS_HISTORY_RECORD_RECV_DURATION] = "AverageDownlinkTimeDuration",
    [MEAS_HISTORY_RECORD_SENT_DURATION] = "AverageUplinkTimeDuration",
    [MEAS_HISTORY_RECORD_RECV_BYTES] = "AverageDownlinkBytesReceived",
    [MEAS_HISTORY_RECORD_SENT_BYTES] = "AverageUplinkBytesSent",
    [MEAS_HISTORY_RECORD_RECV_TOTAL_BYTES] = "TotalDownlinkBytesReceived",
    [MEAS_HISTORY_RECORD_SENT_TOTAL_BYTES] = "TotalUplinkBytesSent",

    [MEAS_HISTORY_RECORD_RTT] = "latency",

    [MEAS_HISTORY_RECORD_IPV4_ADDR] = "IPv4Address",
    [MEAS_HISTORY_RECORD_IPV6_ADDR] = "IPv6Address",
    [MEAS_HISTORY_RECORD_UPTIME] = "UpTime",


    /* MEAS_HISTORY_RECORD_COUNT */
};

/* the measurement item types of per profile history records */
static const enum meas_item_type history_meas_item_types[] = {
    [MEAS_HISTORY_RECORD_RECV_AVG_BPS] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_SENT_AVG_BPS] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_RECV_PEAK_BPS] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_SENT_PEAK_BPS] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_RECV_DURATION] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_SENT_DURATION] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_RECV_BYTES] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_SENT_BYTES] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_RECV_TOTAL_BYTES] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_RECORD_SENT_TOTAL_BYTES] = MEAS_ITEM_TYPE_INT,

    [MEAS_HISTORY_RECORD_RTT] = MEAS_ITEM_TYPE_INT,

    [MEAS_HISTORY_RECORD_IPV4_ADDR] = MEAS_ITEM_TYPE_STRING,
    [MEAS_HISTORY_RECORD_IPV6_ADDR] = MEAS_ITEM_TYPE_STRING,
    [MEAS_HISTORY_RECORD_UPTIME] = MEAS_ITEM_TYPE_INT,


    /* MEAS_HISTORY_RECORD_COUNT */
};

const char* global_history_rdb_names[] = {
    [MEAS_HISTORY_GLOBAL_RECORD_TIMESTAMP] = "timestamps",
    [MEAS_HISTORY_GLOBAL_RECORD_ECGI] = "cellular.ecgi",
    [MEAS_HISTORY_GLOBAL_RECORD_NCGI] = "cellular.ncgi",
    [MEAS_HISTORY_GLOBAL_RECORD_ENODEB] = "cellular.enodeb",
    [MEAS_HISTORY_GLOBAL_RECORD_GNODEB] = "cellular.gnodeb",
    [MEAS_HISTORY_GLOBAL_RECORD_CELLID] = "cellular.cellid",
    [MEAS_HISTORY_GLOBAL_RECORD_GNB_CELLID] = "cellular.gnb_cellid",
    [MEAS_HISTORY_GLOBAL_RECORD_PCI] = "cellular.pci",
    [MEAS_HISTORY_GLOBAL_RECORD_GNB_PCI] = "cellular.gnb_pci",
    [MEAS_HISTORY_GLOBAL_RECORD_EARFCN] = "cellular.earfcn",
    [MEAS_HISTORY_GLOBAL_RECORD_NR_ARFCN] = "cellular.nr_arfcn",
    [MEAS_HISTORY_GLOBAL_RECORD_GNB_SSBINDEX] = "cellular.gnb_ssbindex",
    [MEAS_HISTORY_GLOBAL_RECORD_RSRP] = "cellular.rsrp",
    [MEAS_HISTORY_GLOBAL_RECORD_SS_RSRP] = "cellular.ss_rsrp",
    [MEAS_HISTORY_GLOBAL_RECORD_RSRQ] = "cellular.rsrq",
    [MEAS_HISTORY_GLOBAL_RECORD_SS_RSRQ] = "cellular.ss_rsrq",
    [MEAS_HISTORY_GLOBAL_RECORD_SINR] = "cellular.sinr",
    [MEAS_HISTORY_GLOBAL_RECORD_SS_SINR] = "cellular.ss_sinr",
    [MEAS_HISTORY_GLOBAL_RECORD_RSSI] = "cellular.rssi",
    [MEAS_HISTORY_GLOBAL_RECORD_NR_RSSI] = "cellular.nr_rssi",
    [MEAS_HISTORY_GLOBAL_RECORD_CQI] = "cellular.cqi",
    [MEAS_HISTORY_GLOBAL_RECORD_NR_CQI] = "cellular.nr_cqi",
};

/* the measurement item types of global history records */
static const enum meas_item_type global_history_meas_item_types[] = {
    [MEAS_HISTORY_GLOBAL_RECORD_TIMESTAMP] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_ECGI] = MEAS_ITEM_TYPE_STRING,
    [MEAS_HISTORY_GLOBAL_RECORD_NCGI] = MEAS_ITEM_TYPE_STRING,
    [MEAS_HISTORY_GLOBAL_RECORD_ENODEB] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_GNODEB] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_CELLID] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_GNB_CELLID] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_PCI] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_GNB_PCI] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_EARFCN] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_NR_ARFCN] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_GNB_SSBINDEX] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_RSRP] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_SS_RSRP] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_RSRQ] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_SS_RSRQ] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_SINR] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_SS_SINR] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_RSSI] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_NR_RSSI] = MEAS_ITEM_TYPE_FLOAT_I,
    [MEAS_HISTORY_GLOBAL_RECORD_CQI] = MEAS_ITEM_TYPE_INT,
    [MEAS_HISTORY_GLOBAL_RECORD_NR_CQI] = MEAS_ITEM_TYPE_INT,
};

/**
 * @brief Sets measurement RDB to blank.
 *
 * @param mh is a measurement history object.
 */
void meas_history_clean_rdb(struct meas_history_t* mh)
{
    int i;
    const char* var;

    int record_count = mh->global_history ? MEAS_HISTORY_GLOBAL_RECORD_COUNT : MEAS_HISTORY_RECORD_COUNT;

    int rdb_pf_index = mh->profile_no + 1;

    for (i = 0; i < record_count; i++) {
        if (mh->global_history) {
            var = rdb_var_printf(MEA_HISTORY_RDB_RESULT ".%s", global_history_rdb_names[i]);
        } else {
            var = rdb_var_printf(MEA_HISTORY_RDB_RESULT ".%d.%s", rdb_pf_index, history_rdb_names[i]);
        }
        rdb_set_value(var, "", 0);
    }
}

/**
 * @brief Writes comma delimited measurements to RDBs.
 *
 * @param mh is a measurement history object.
 */
void meas_history_flush_to_rdb(struct meas_history_t* mh)
{
    const char* var;
    static char val[MEAS_HISTORY_RDB_MAX_LEN];

    char* p;
    int p_idx;

    void* hent;
    int valid;
    void* record;
    enum meas_item_type type;
    int i;
    int first;

    int rdb_pf_index = mh->profile_no + 1;

    int record_count = mh->global_history ? MEAS_HISTORY_GLOBAL_RECORD_COUNT : MEAS_HISTORY_RECORD_COUNT;

    for (i = 0; i < record_count; i++) {

        /*
        ///////////////////////////////////////////////
        build rdb value ( comma delimited value )
        ///////////////////////////////////////////////
        */

        /* prepare loop */
        hent = meas_history_fetch_first(mh);
        p = val;
        p_idx = 0;
        first = 1;

        /* update rdb */
        if (mh->global_history) {
            var = rdb_var_printf(MEA_HISTORY_RDB_RESULT ".%s", global_history_rdb_names[i]);
            type = global_history_meas_item_types[i];
        } else {
            var = rdb_var_printf(MEA_HISTORY_RDB_RESULT ".%d.%s", rdb_pf_index, history_rdb_names[i]);
            type = history_meas_item_types[i];
        }

        while (hent) {
            if (mh->global_history) {
                valid = ((struct meas_history_global_entry_t *)hent)->valid[i];
                record = ((struct meas_history_global_entry_t *)hent)->record[i];
            } else {
                valid = ((struct meas_history_entry_t *)hent)->valid[i];
                record = ((struct meas_history_entry_t *)hent)->record[i];
            }

            /* concatenate records */
            if (valid) {
                switch(type) {
                case MEAS_ITEM_TYPE_INT:
                    p_idx += snprintf(p + p_idx, sizeof(val) - p_idx, "%s%lld", first ? "" : ",", *(int64_t *)record);
                    break;
                case MEAS_ITEM_TYPE_FLOAT_I:
                    p_idx += snprintf(p + p_idx, sizeof(val) - p_idx, "%s%.1f", first ? "" : ",", *(int64_t *)record / 10.0f);
                    break;
                case MEAS_ITEM_TYPE_FLOAT_II:
                    p_idx += snprintf(p + p_idx, sizeof(val) - p_idx, "%s%.2f", first ? "" : ",", *(int64_t *)record / 100.0f);
                    break;
                case MEAS_ITEM_TYPE_STRING:
                    p_idx += snprintf(p + p_idx, sizeof(val) - p_idx, "%s%s", first ? "" : ",", (const char *)record);
                    break;
                default:
                    assert(0);
                }
            } else if (!first) {
                p_idx += snprintf(p + p_idx, sizeof(val) - p_idx, ",");
            }

            /* break if it is out of space */
            if (!(p_idx < sizeof(val))) {
                syslog(LOG_ERR, "overflow detected in building RDB value (%s)", var);
                break;
            }

            /* fetch next entry */
            hent = meas_history_fetch_next(mh);
            first = 0;
        }

        rdb_set_value(var, val, 0);
    }
}


/**
 * @brief Find first or next history entity (record) collection.
 *
 * @param mh is a measurement history object.
 * @param next selects next or first. 0 is to search the first record. 1 is to search any following record.
 *
 * @return history entity (record) collection.
 */
static void* meas_history_fetch_first_or_next(struct meas_history_t* mh, int next)
{
    void* hent = NULL;
    int fetch = mh->fetch;

    /* start from head if next flag is not set */
    if (!next) {
        fetch = mh->head;
    }

    /* end if fetch is at the end */
    if (fetch == mh->tail) {
        goto fini;
    }

    /* get entry */
    if (mh->global_history) {
        hent = &((struct meas_history_global_entry_t *)mh->entry)[fetch];
    } else {
        hent = &((struct meas_history_entry_t *)mh->entry)[fetch];
    }

    /* update fetch */
    mh->fetch = (fetch + 1) % mh->len;

fini:
    return hent;
}

/**
 * @brief Find first entity (record) collection.
 *
 * @param mh is a measurement history object.
 *
 * @return first history entity (record) collection.
 */
void* meas_history_fetch_first(struct meas_history_t* mh)
{
    return meas_history_fetch_first_or_next(mh, 0);
}

/**
 * @brief Find next entity (record) collection.
 *
 * @param mh is a measurement history object.
 *
 * @return next history entity (record) collection.
 */
void* meas_history_fetch_next(struct meas_history_t* mh)
{
    return meas_history_fetch_first_or_next(mh, 1);
}

/**
 * @brief Add history entity (record) collection to history.
 *
 * @param mh is a measurement history object.
 * @param hent history entity
 */
void meas_history_add(struct meas_history_t* mh, void* hent)
{
    int tail = mh->tail;
    int i;
    int sz;

    /* advance tail */
    mh->tail = (mh->tail + 1) % mh->len;

    /* advance head if required */
    if (mh->tail == mh->head) {
        mh->head = (mh->head + 1) % mh->len;
    }

    /* store entry */
    if (mh->global_history) {
        struct meas_history_global_entry_t * hdest =
            &((struct meas_history_global_entry_t *)mh->entry)[tail];
        struct meas_history_global_entry_t * hsrc =
            (struct meas_history_global_entry_t *)hent;
        for (i = 0; i < MEAS_HISTORY_GLOBAL_RECORD_COUNT; i++) {
            if (!(hdest->valid[i] = hsrc->valid[i])) {
                continue;
            }
            sz = (global_history_meas_item_types[i] == MEAS_ITEM_TYPE_STRING ?
                  MEAS_ITEM_STRING_LEN : sizeof(int64_t));
            memcpy(hdest->record[i], hsrc->record[i], sz);
        }
    } else {
        struct meas_history_entry_t * hdest =
            &((struct meas_history_entry_t *)mh->entry)[tail];
        struct meas_history_entry_t * hsrc = (struct meas_history_entry_t *)hent;
        for (i = 0; i < MEAS_HISTORY_RECORD_COUNT; i++) {
            if (!(hdest->valid[i] = hsrc->valid[i])) {
                continue;
            }
            sz = (history_meas_item_types[i] == MEAS_ITEM_TYPE_STRING ?
                  MEAS_ITEM_STRING_LEN : sizeof(int64_t));
            memcpy(hdest->record[i], hsrc->record[i], sz);
        }
    }
}

/**
 * @brief Finalize a measurement history object to destroy.
 *
 * @param mh is a measurement history object.
 */
void meas_history_fini(struct meas_history_t* mh)
{
    int i, j;
    if (mh->global_history) {
        struct meas_history_global_entry_t* hent = (struct meas_history_global_entry_t *)mh->entry;
        for (i = 0; i < mh->len; i++) {
            for (j = 0; j < MEAS_HISTORY_GLOBAL_RECORD_COUNT; j++) {
                free(hent->record[j]);
            }
            hent++;
        }
    } else {
        struct meas_history_entry_t* hent = (struct meas_history_entry_t *)mh->entry;
        for (i = 0; i < mh->len; i++) {
            for (j = 0; j < MEAS_HISTORY_RECORD_COUNT; j++) {
                free(hent->record[j]);
            }
            hent++;
        }
    }
    mh->len = 0;
    /* free entry buffer */
    free(mh->entry);
    mh->entry = NULL;
}

/**
 * @brief Re-initiate a measurement history object to re-size the length of history.
 *
 * @param mh is a measurement history object.
 * @param meas_history_count is a new history length.
 *
 * @return 0 when succeeds. Otherwise, -1.
 */
int meas_history_reinit(struct meas_history_t* mh, int meas_history_count)
{
    int i, j;
    int q_len = meas_history_count + 1;
    int sz = (mh->global_history ? sizeof(struct meas_history_global_entry_t) :
              sizeof(struct meas_history_entry_t));

    meas_history_fini(mh);

    mh->entry = calloc(q_len, sz);
    if (!mh->entry) {
        syslog(LOG_ERR, "failed to allocate history entry buffer - %s", strerror(errno));
        goto err;
    }

    /* initiate members */
    mh->head = 0;
    mh->tail = 0;
    mh->fetch = 0;
    mh->len = q_len;

    if (mh->global_history) {
        struct meas_history_global_entry_t* hent = (struct meas_history_global_entry_t *)mh->entry;
        for (i = 0; i < q_len; i++) {
            for (j = 0; j < MEAS_HISTORY_GLOBAL_RECORD_COUNT; j++) {
                sz = (global_history_meas_item_types[j] == MEAS_ITEM_TYPE_STRING ?
                      MEAS_ITEM_STRING_LEN : sizeof(int64_t));
                if (!(hent->record[j] = malloc(sz))) {
                    goto err2;
                }
            }
            hent++;
        }
    } else {
        struct meas_history_entry_t* hent = (struct meas_history_entry_t *)mh->entry;
        for (i = 0; i < q_len; i++) {
            for (j = 0; j < MEAS_HISTORY_RECORD_COUNT; j++) {
                sz = (history_meas_item_types[j] == MEAS_ITEM_TYPE_STRING ?
                      MEAS_ITEM_STRING_LEN : sizeof(int64_t));
                if (!(hent->record[j] = malloc(sz))) {
                    goto err2;
                }
            }
            hent++;
        }
    }

    return 0;

err2:
    meas_history_fini(mh);
err:
    return -1;
}

/**
 * @brief Initiate a measurement history to use
 *
 * @param mh is a measurement history object.
 * @param profile_no is a profile index number.
 * @param meas_history_count is total history entity count.
 * @param global_history describes level of history object. (0=per-PDN history, 1=global history)
 *
 * @return 0 when succeeds. Otherwise, -1.
 */
int meas_history_init(struct meas_history_t* mh, int profile_no, int meas_history_count, int global_history)
{
    /* initiate members */
    mh->profile_no = profile_no;
    mh->entry = NULL;
    mh->global_history = global_history;
    mh->len = 0;

    return meas_history_reinit(mh, meas_history_count);
}

/*
 * initialise a per profile history entry
 *
 * @param hent A pointer to the history entry to be initialised
 * @return 0 on success; -1 on failure
 */
int meas_history_entry_init(struct meas_history_entry_t* hent)
{
    int i;
    int sz;
    for (i = 0; i < MEAS_HISTORY_RECORD_COUNT; i++) {
        sz = (history_meas_item_types[i] == MEAS_ITEM_TYPE_STRING ?
              MEAS_ITEM_STRING_LEN : sizeof(int64_t));
        if (!(hent->record[i] = malloc(sz))) {
            for (i--; i >= 0; i--) { /* free already allocated mem */
                free(hent->record[i]);
                hent->record[i] = NULL;
            }
            return -1;
        }
    }
    return 0;
}

/*
 * release resource for a per profile history entry
 *
 * @param hent A pointer to the history entry of interest
 */
void meas_history_entry_fini(struct meas_history_entry_t* hent)
{
    int i;
    for (i = 0; i < MEAS_HISTORY_RECORD_COUNT; i++) {
        free(hent->record[i]);
        hent->record[i] = NULL;
    }
}

/*
 * initialise a global history entry
 *
 * @param hent A pointer to the history entry to be initialised
 * @return 0 on success; -1 on failure
 */
int meas_history_global_entry_init(struct meas_history_global_entry_t* hent)
{
    int i;
    int sz;
    for (i = 0; i < MEAS_HISTORY_GLOBAL_RECORD_COUNT; i++) {
        sz = (global_history_meas_item_types[i] == MEAS_ITEM_TYPE_STRING ?
              MEAS_ITEM_STRING_LEN : sizeof(int64_t));
        if (!(hent->record[i] = malloc(sz))) {
            for (i--; i >= 0; i--) { /* free already allocated mem */
                free(hent->record[i]);
                hent->record[i] = NULL;
            }
            return -1;
        }
    }
    return 0;
}

/*
 * release resource for a global history entry
 *
 * @param hent A pointer to the history entry of interest
 */
void meas_history_global_entry_fini(struct meas_history_global_entry_t* hent)
{
    int i;
    for (i = 0; i < MEAS_HISTORY_GLOBAL_RECORD_COUNT; i++) {
        free(hent->record[i]);
        hent->record[i] = NULL;
    }
}
