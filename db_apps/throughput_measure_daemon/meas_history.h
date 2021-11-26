#ifndef MEAS_HISTORY_H_03052018
#define MEAS_HISTORY_H_03052018

/*!
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

#include "cellular_meas.h"
#include <stdint.h>

/* global history record items */
enum meas_history_global_record_item {
    MEAS_HISTORY_GLOBAL_RECORD_TIMESTAMP = 0,

    MEAS_HISTORY_GLOBAL_RECORD_ECGI,
    MEAS_HISTORY_GLOBAL_RECORD_NCGI,
    MEAS_HISTORY_GLOBAL_RECORD_ENODEB,
    MEAS_HISTORY_GLOBAL_RECORD_GNODEB,
    MEAS_HISTORY_GLOBAL_RECORD_CELLID,
    MEAS_HISTORY_GLOBAL_RECORD_GNB_CELLID,
    MEAS_HISTORY_GLOBAL_RECORD_PCI,
    MEAS_HISTORY_GLOBAL_RECORD_GNB_PCI,
    MEAS_HISTORY_GLOBAL_RECORD_EARFCN,
    MEAS_HISTORY_GLOBAL_RECORD_NR_ARFCN,
    MEAS_HISTORY_GLOBAL_RECORD_GNB_SSBINDEX,

    MEAS_HISTORY_GLOBAL_RECORD_RSRP,
    MEAS_HISTORY_GLOBAL_RECORD_SS_RSRP,
    MEAS_HISTORY_GLOBAL_RECORD_RSRQ,
    MEAS_HISTORY_GLOBAL_RECORD_SS_RSRQ,
    MEAS_HISTORY_GLOBAL_RECORD_SINR,
    MEAS_HISTORY_GLOBAL_RECORD_SS_SINR,
    MEAS_HISTORY_GLOBAL_RECORD_RSSI,
    MEAS_HISTORY_GLOBAL_RECORD_NR_RSSI,
    MEAS_HISTORY_GLOBAL_RECORD_CQI,
    MEAS_HISTORY_GLOBAL_RECORD_NR_CQI,

    MEAS_HISTORY_GLOBAL_RECORD_COUNT,
};

/* per profile history record items */
enum meas_history_record_item {
    MEAS_HISTORY_RECORD_RECV_AVG_BPS = 0,
    MEAS_HISTORY_RECORD_SENT_AVG_BPS,
    MEAS_HISTORY_RECORD_RECV_PEAK_BPS,
    MEAS_HISTORY_RECORD_SENT_PEAK_BPS,
    MEAS_HISTORY_RECORD_RECV_DURATION,
    MEAS_HISTORY_RECORD_SENT_DURATION,
    MEAS_HISTORY_RECORD_RECV_BYTES,
    MEAS_HISTORY_RECORD_SENT_BYTES,
    MEAS_HISTORY_RECORD_RECV_TOTAL_BYTES,
    MEAS_HISTORY_RECORD_SENT_TOTAL_BYTES,

    MEAS_HISTORY_RECORD_RTT,

    MEAS_HISTORY_RECORD_IPV4_ADDR,
    MEAS_HISTORY_RECORD_IPV6_ADDR,
    MEAS_HISTORY_RECORD_UPTIME,

    MEAS_HISTORY_RECORD_COUNT,
};

/* global history entry */
struct meas_history_global_entry_t {
    int valid[MEAS_HISTORY_GLOBAL_RECORD_COUNT];
    void* record[MEAS_HISTORY_GLOBAL_RECORD_COUNT];
};

/* per profile history entry */
struct meas_history_entry_t {
    int valid[MEAS_HISTORY_RECORD_COUNT];
    void* record[MEAS_HISTORY_RECORD_COUNT];
};

struct meas_history_t {
    int global_history;
    int profile_no;

    /* point to an array of meas_history_entry_t or meas_history_global_entry_t array */
    void * entry;

    int head;
    int tail;

    int len;

    int fetch;
};

void meas_history_clean_rdb(struct meas_history_t* mh);
void meas_history_flush_to_rdb(struct meas_history_t *mh);
void *meas_history_fetch_first(struct meas_history_t *mh);
void *meas_history_fetch_next(struct meas_history_t *mh);
void meas_history_add(struct meas_history_t *mh, void *hent);
void meas_history_fini(struct meas_history_t *mh);
int meas_history_reinit(struct meas_history_t *mh, int meas_history_count);
int meas_history_init(struct meas_history_t* mh, int profile_no, int meas_history_count, int global_history);
int meas_history_entry_init(struct meas_history_entry_t* hent);
void meas_history_entry_fini(struct meas_history_entry_t* hent);
int meas_history_global_entry_init(struct meas_history_global_entry_t* hent);
void meas_history_global_entry_fini(struct meas_history_global_entry_t* hent);

#endif
