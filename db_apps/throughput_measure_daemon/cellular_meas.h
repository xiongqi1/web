#ifndef CELLULAR_MEAS_H_14073625052020
#define CELLULAR_MEAS_H_14073625052020
/*
 * Cellular network stats and performance measurements
 *
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "store_bot.h"
#include <stdint.h>

/* at present the max string holds an ipv6 addr */
#define MEAS_ITEM_STRING_LEN 64

/* all possible measurement item types */
enum meas_item_type {
    MEAS_ITEM_TYPE_INT = 0, /* 64-bit integer */
    MEAS_ITEM_TYPE_FLOAT_I, /* float represented as 64-bit integer scaled by 10 */
    MEAS_ITEM_TYPE_FLOAT_II, /* float represented as 64-bit integer scaled by 100 */
    MEAS_ITEM_TYPE_STRING, /* string with max len MEAS_ITEM_STRING_LEN-1 */
};

/* a struct to facilitate parameter/return passing */
struct cellular_meas_record_t {
    enum meas_item_type type;
    union {
        int64_t i_val;
        char s_val[MEAS_ITEM_STRING_LEN];
    } value;
};

/*
 * cellular measurement items
 *
 * Basically, there are two categories of items:
 * 1) metrics whose samples need to be stored to derive the measurement
 * 2) metrics whose measurement is simply the instantaneous value at measurement time
 * In the following, items before CELLULAR_MEAS_COUNT1 are the 1st category;
 * the ones after are the 2nd category
 */
enum cellular_meas_item {
    /* category 1 items */
    CELLULAR_MEAS_GNB_SSBINDEX = 0,
    CELLULAR_MEAS_RSRP,
    CELLULAR_MEAS_SS_RSRP,
    CELLULAR_MEAS_RSRQ,
    CELLULAR_MEAS_SS_RSRQ,
    CELLULAR_MEAS_SINR,
    CELLULAR_MEAS_SS_SINR,
    CELLULAR_MEAS_RSSI,
    CELLULAR_MEAS_NR_RSSI,
    CELLULAR_MEAS_CQI,
    CELLULAR_MEAS_NR_CQI,
    CELLULAR_MEAS_COUNT1,
    /* category 2 items */
    CELLULAR_MEAS_ECGI,
    CELLULAR_MEAS_NCGI,
    CELLULAR_MEAS_ENODEB,
    CELLULAR_MEAS_GNODEB,
    CELLULAR_MEAS_CELLID,
    CELLULAR_MEAS_GNB_CELLID,
    CELLULAR_MEAS_PCI,
    CELLULAR_MEAS_GNB_PCI,
    CELLULAR_MEAS_EARFCN,
    CELLULAR_MEAS_NR_ARFCN,
    CELLULAR_MEAS_COUNT2,
};

/* the method to derive a measurement from samples */
enum cellular_meas_method {
    CELLULAR_MEAS_METHOD_PERCENTILE,
    CELLULAR_MEAS_METHOD_MAJORITY,
    CELLULAR_MEAS_METHOD_SUM,
    CELLULAR_MEAS_METHOD_INSTANTANEOUS,
};

int cellular_meas_init(int len, int percentile);

void cellular_meas_reset(void);

int cellular_meas_sample(void);

void cellular_meas_fini(void);

int cellular_meas_reinit(int len, int percentile);

int cellular_meas_get(enum cellular_meas_item item, struct cellular_meas_record_t* record);

#endif
