#include "cellular_meas.h"
#include "rdb.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <syslog.h>

#define RDB_PREFIX "wwan.0."

/* store bots for all category 1 items (non-instantaneous measurement items) */
static struct store_bot_t bots[CELLULAR_MEAS_COUNT1];

/* this determines how the items are stored in bots and/or represented in memory */
static enum meas_item_type types[] = {
    [CELLULAR_MEAS_GNB_SSBINDEX] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_RSRP] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_SS_RSRP] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_RSRQ] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_SS_RSRQ] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_SINR] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_SS_SINR] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_RSSI] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_NR_RSSI] = MEAS_ITEM_TYPE_FLOAT_I,
    [CELLULAR_MEAS_CQI] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_NR_CQI] = MEAS_ITEM_TYPE_INT,
    /* CELLULAR_MEAS_COUNT1 */
    [CELLULAR_MEAS_ECGI] = MEAS_ITEM_TYPE_STRING,
    [CELLULAR_MEAS_NCGI] = MEAS_ITEM_TYPE_STRING,
    [CELLULAR_MEAS_ENODEB] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_GNODEB] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_CELLID] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_GNB_CELLID] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_PCI] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_GNB_PCI] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_EARFCN] = MEAS_ITEM_TYPE_INT,
    [CELLULAR_MEAS_NR_ARFCN] = MEAS_ITEM_TYPE_INT,
    /* CELLULAR_MEAS_COUNT2 */
};

/* methods by which measurements are derived from samples */
static enum cellular_meas_method methods[] = {
    [CELLULAR_MEAS_GNB_SSBINDEX] = CELLULAR_MEAS_METHOD_MAJORITY,
    [CELLULAR_MEAS_RSRP] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_SS_RSRP] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_RSRQ] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_SS_RSRQ] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_SINR] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_SS_SINR] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_RSSI] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_NR_RSSI] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_CQI] = CELLULAR_MEAS_METHOD_PERCENTILE,
    [CELLULAR_MEAS_NR_CQI] = CELLULAR_MEAS_METHOD_PERCENTILE,
};

/* RDB variables to take samples or measurements from */
static const char* rdbs[] = {
    [CELLULAR_MEAS_GNB_SSBINDEX] = RDB_PREFIX"radio_stack.nr5g.ssb_index",
    [CELLULAR_MEAS_RSRP] = RDB_PREFIX"servcell_info.rsrp",
    [CELLULAR_MEAS_SS_RSRP] = RDB_PREFIX"radio_stack.nr5g.rsrp",
    [CELLULAR_MEAS_RSRQ] = RDB_PREFIX"servcell_info.rsrq",
    [CELLULAR_MEAS_SS_RSRQ] = RDB_PREFIX"radio_stack.nr5g.rsrq",
    [CELLULAR_MEAS_SINR] = RDB_PREFIX"signal.snr",
    [CELLULAR_MEAS_SS_SINR] = RDB_PREFIX"radio_stack.nr5g.snr",
    [CELLULAR_MEAS_RSSI] = RDB_PREFIX"signal.rssi",
#ifdef SA_MODE_SUPPORTED
    [CELLULAR_MEAS_NR_RSSI] = RDB_PREFIX"radio_stack.nr5g.rssi",
#else // if 5G SA is not supported, just use LTE RSSI
    [CELLULAR_MEAS_NR_RSSI] = RDB_PREFIX"signal.rssi",
#endif
    [CELLULAR_MEAS_CQI] = RDB_PREFIX"servcell_info.avg_wide_band_cqi",
    [CELLULAR_MEAS_NR_CQI] =  RDB_PREFIX"radio_stack.nr5g.cqi",
    [CELLULAR_MEAS_ECGI] = RDB_PREFIX"system_network_status.ECGI",
    [CELLULAR_MEAS_NCGI] = RDB_PREFIX"radio_stack.nr5g.cgi",
    [CELLULAR_MEAS_ENODEB] = RDB_PREFIX"system_network_status.eNB_ID",
    [CELLULAR_MEAS_GNODEB] = RDB_PREFIX"radio_stack.nr5g.gNB_ID",
    [CELLULAR_MEAS_CELLID] = RDB_PREFIX"system_network_status.CellID",
    [CELLULAR_MEAS_GNB_CELLID] = RDB_PREFIX"radio_stack.nr5g.CellID",
    [CELLULAR_MEAS_PCI] = RDB_PREFIX"system_network_status.PCID",
    [CELLULAR_MEAS_GNB_PCI] = RDB_PREFIX"radio_stack.nr5g.pci",
    [CELLULAR_MEAS_EARFCN] = RDB_PREFIX"system_network_status.channel",
    [CELLULAR_MEAS_NR_ARFCN] = RDB_PREFIX"radio_stack.nr5g.arfcn",
};

static int cellular_meas_percentile;

/*
 * initialise this module
 *
 * @param len The maximum number of samples to be stored within a measurement period
 * @param percentile The percentile value to be used when percentile method is
 * to be used; otherwise, this param is not used.
 * @return 0 on success; -1 on falure
 */
int cellular_meas_init(int len, int percentile)
{
    int i;
    cellular_meas_percentile = percentile;
    for (i = 0; i < CELLULAR_MEAS_COUNT1; i++) {
        /* at the moment, we only need 32-bit integers */
        if (store_bot_init(&bots[i], len, STAT_TYPE_INT)) {
            return -1;
        }
    }
    return 0;
}

/*
 * reset this module to prepare for the next measurement period
 */
void cellular_meas_reset(void)
{
    int i;
    for (i = 0; i < CELLULAR_MEAS_COUNT1; i++) {
        store_bot_reset(&bots[i]);
    }
}

/*
 * read an integer or float value from an RDB and convert into an integer representation
 *
 * @param rdb The RDB name
 * @param type The measurement item type:
 *   MEAS_ITEM_TYPE_INT, MEAS_ITEM_TYPE_FLOAT_I or MEAS_ITEM_TYPE_FLOAT_II
 * @param result A pointer to an integer to store the result
 * @return 0 on success; -1 on failure
 */
static int read_rdb_to_int(const char* rdb, enum meas_item_type type, int* result)
{
    char* endptr;
    const char* val = rdb_get_value(rdb);
    if (!val) {
        return -1;
    }

    if (type == MEAS_ITEM_TYPE_INT) {
        long lval = strtol(val, &endptr, 10);
        if (endptr == val) { /* failed to convert */
            return -1;
        }
        *result = (int)lval;
    } else if (type == MEAS_ITEM_TYPE_FLOAT_I || type == MEAS_ITEM_TYPE_FLOAT_II) {
        float fval = strtof(val, &endptr);
        if (endptr == val) { /* failed to convert */
            return -1;
        }
        if (type == MEAS_ITEM_TYPE_FLOAT_I) {
            fval *= 10;
        } else {
            fval *= 100;
        }
        *result = (int)(fval > 0 ? fval + 0.5 : fval - 0.5);
    } else {
        assert(0);
    }
    return 0;
}

/*
 * take a sample from various sources and store it
 *
 * @return 0 on success; -1 on failure
 */
int cellular_meas_sample(void)
{
    int i;
    int sample;
    for (i = 0; i < CELLULAR_MEAS_COUNT1; i++) {
        if (read_rdb_to_int(rdbs[i], types[i], &sample)) {
            continue; /* skip invalid samples */
        }
        if (store_bot_feed(&bots[i], &sample)) {
            return -1;
        }
    }
    return 0;
}

/*
 * release resource of this module
 */
void cellular_meas_fini(void)
{
    int i;
    for (i = 0; i < CELLULAR_MEAS_COUNT1; i++) {
        store_bot_fini(&bots[i]);
    }
}

/*
 * reinitialise this module for new parameters
 *
 * @see cellular_meas_init
 */
int cellular_meas_reinit(int len, int percentile)
{
    cellular_meas_fini();
    return cellular_meas_init(len, percentile);
}

/*
 * get a measurement record by deriving from all samples so far obtained
 *
 * @param item
 * @param record
 * @return 1 if the measurement record is valid; 0 if invalid
 */
int cellular_meas_get(enum cellular_meas_item item, struct cellular_meas_record_t* record)
{
    void* res = NULL;
    record->type = types[item];
    if (item < CELLULAR_MEAS_COUNT1) {
        /* get from store_bot for category 1 items */
        switch(methods[item]) {
        case CELLULAR_MEAS_METHOD_PERCENTILE:
            res = store_bot_get_by_percentile(&bots[item], cellular_meas_percentile);
            break;
        case CELLULAR_MEAS_METHOD_MAJORITY:
            res = store_bot_get_by_majority(&bots[item]);
            break;
        default:
            assert(0);
        }
        if (res) {
            record->value.i_val = *(int *)res;
            return 1;
        } else {
            return 0;
        }
    } else {
        /* get instantaneous value for category 2 items */
        const char* rdb = rdbs[item];
        if (types[item] == MEAS_ITEM_TYPE_STRING) {
            const char* val = rdb_get_value(rdb);
            if (val && val[0]) {
                strncpy(record->value.s_val, val, MEAS_ITEM_STRING_LEN);
                if (record->value.s_val[MEAS_ITEM_STRING_LEN - 1]) { // overflow
                    return 0;
                } else {
                    return 1;
                }
            } else {
                return 0;
            }
        } else {
            int val;
            if (read_rdb_to_int(rdb, types[item], &val)) {
                return 0;
            } else {
                record->value.i_val = val;
                return 1;
            }
        }
    }
}
