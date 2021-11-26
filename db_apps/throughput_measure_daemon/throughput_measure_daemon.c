/*
 * Throughput measurement daemon
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

#include "proc_net_dev.h"
#include "link_profile_meas.h"
#include "tick_clock.h"
#include "meas_bot.h"
#include "rdb.h"
#include "meas_history.h"
#include "rdb_names.h"

#include <syslog.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#ifdef MDEBUG
#include <mcheck.h>
#endif

static int sig_term_caught = 0; /* true when signal got caught to terminate */

/**
 * @brief Handles signals and set termination flag.
 *
 * @param sig is a signal number.
 */
void sighandler(int sig)
{
    sig_term_caught = 1;
}

struct rdb_to_c_var_t {
    const char* rdb;
    int* var;
};

/**
 * @brief Reads rdb variables into memory variables.
 *
 * @param rdb_to_c_var is a mapping table from RDB to memroy variables.
 */
void read_rdb_settings(struct rdb_to_c_var_t* rdb_to_c_var, int* pf_enable)
{
    int val;
    const char* val_str;

    int i;
    int pf_rdb_idx;


    syslog(LOG_DEBUG, "* RDB settings");
    while (rdb_to_c_var->rdb) {

        val_str = rdb_get_value(rdb_to_c_var->rdb);
        if (val_str && *val_str) {
            val = atoi(val_str);
            *rdb_to_c_var->var = val;

            syslog(LOG_DEBUG, "RDB config [%s] ==> %d", rdb_to_c_var->rdb, val);
        } else {
            syslog(LOG_DEBUG, "RDB config %s not available", rdb_to_c_var->rdb);
        }

        rdb_to_c_var++;
    }

    /* read profile activation flags */
    for (i = 0; i < LINK_PROFILE_COUNT; i++) {
        pf_rdb_idx = i + 1;
        pf_enable[i] = atoi(rdb_get_printf("performance.measurement.%d.Enable", pf_rdb_idx));
    }

}

/*
 * get a cellular measurement and record into a meas_history_global_entry
 *
 * @param mh_item The global history item to record into
 * @param cm_item The cellular measurement item to read from
 * @param hent A pointer to the global history entry
 * @param enable Whether cellular measument is enabled or not
 */
static void record_cellular_stat(enum meas_history_global_record_item mh_item,
                                 enum cellular_meas_item cm_item,
                                 struct meas_history_global_entry_t* hent,
                                 int enable)
{
    struct cellular_meas_record_t record;
    if (!enable) {
        hent->valid[mh_item] = 0;
        return;
    }
    if ((hent->valid[mh_item] = cellular_meas_get(cm_item, &record))) {
        const int sz = (record.type == MEAS_ITEM_TYPE_STRING ?
                        MEAS_ITEM_STRING_LEN : sizeof(int64_t));
        memcpy(hent->record[mh_item], &record.value, sz);
    }
}

/*
 * get and record an IP address into a string buffer
 *
 * @param ipv6 Whether to get IPv4 (0) or IPv6 (1) address
 * @param profile The profile number to get address from
 * @param record A pointer to the buffer where result will be written to
 * @return 1 if the result is valid; 0 if invalid
 */
static int record_ip_addr(int ipv6, int profile, char * record)
{
    const char *rdb, *addr, *status;
    rdb = rdb_var_printf(LINK_POLICY_PREFIX".%d.%s", profile,
                         ipv6 ? "status_ipv6" : "status_ipv4");
    status = rdb_get_value(rdb);
    if (!status || strcmp(status, "up")) {
        return 0;
    }
    rdb = rdb_var_printf(LINK_POLICY_PREFIX".%d.%s", profile, ipv6 ? "ipv6_ipaddr" : "iplocal");
    addr = rdb_get_value(rdb);
    if (!addr) {
        return 0;
    }
    strncpy(record, addr, MEAS_ITEM_STRING_LEN);
    return record[MEAS_ITEM_STRING_LEN - 1] ? 0 : 1;
}

/**
 * @brief Periodically samples measurement values to RDB variables for TR069.
 *
 * @param argc is a command line option count.
 * @param argv[] are command line options.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int main(int argc, char* argv[])
{
#ifdef MDEBUG
    mtrace();
#endif

    struct timeval tv;
    int stat;

    /* RDB settings */
    int sample_sec;
    int meas_min;
    int recv_idle_kibps;
    int sent_idle_kibps;
    int hist_count;
    int cellular_enable;
    int cellular_percentile;

    /* interval timers */
    struct tick_clock_t sample_tm;
    struct tick_clock_t meas_tm;

    /* measurement history */
    struct meas_history_t mhist_global;
    struct meas_history_t mhist[LINK_PROFILE_COUNT];
    /* measurement bot */
    struct meas_bot_t meas_bot_recv[LINK_PROFILE_COUNT];
    struct meas_bot_t meas_bot_sent[LINK_PROFILE_COUNT];
    /* link.profile measurement */
    struct link_profile_meas_t*  lpm;

    /* measurement of uptime for each profile */
    struct store_bot_t store_bot_uptime[LINK_PROFILE_COUNT];

    /* duration between samples */
    time_diff_ms_t duration;
    time_diff_ms_t duration_meas;
    int i;

    int meas_expired;
    int sample_expired;

    uint32_t recv_avg_bps;
    uint32_t sent_avg_bps;

    uint32_t recv_peak_bps;
    uint32_t sent_peak_bps;

    int rdb_pf_idx;

    struct meas_history_entry_t hent;
    struct meas_history_global_entry_t hent_global;

    int recv_valid;
    int sent_valid;

    int retcode = -1;

    int h;
    fd_set readfds;
    const char* rdb_rtt;
    const char* rdb_rtt_val;

    int pf_enable[LINK_PROFILE_COUNT] = {0,};

    int sample_val;

    /*
    ///////////////////////////////////////////////
    initiate early objects
    ///////////////////////////////////////////////
    */

    /* catch signals */
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    rdb_init();

    /*
    ///////////////////////////////////////////////
    read rdb settings
    ///////////////////////////////////////////////
    */

    /* instantaneous sampling interval 3 second */
    sample_sec = 3;
    /* measurement interval 60 seconds */
    meas_min = 1;
    /* idle kibps 50 kibps */
    recv_idle_kibps = 50;
    sent_idle_kibps = 20;
    /* history count 25 */
    hist_count = 25;
    /* disable cellular stats */
    cellular_enable = 0;
    /* default percentile for cellular meas */
    cellular_percentile = 90;
    int rtt;


    struct rdb_to_c_var_t rdb_to_c_var[] = {
        {MEAS_CONFIG_RDB_PREFIX ".MeasurementPeriod", &meas_min},
        {MEAS_CONFIG_RDB_PREFIX ".InstantaneousSamplingPeriod", &sample_sec},
        {MEAS_CONFIG_RDB_PREFIX ".NumberOfMeasureRecords", &hist_count},
        {MEAS_CONFIG_RDB_PREFIX ".IdleTrafficLimitDownlink", &recv_idle_kibps},
        {MEAS_CONFIG_RDB_PREFIX ".IdleTrafficLimitUplink", &sent_idle_kibps},
        {MEAS_CONFIG_RDB_PREFIX ".cellular.enable", &cellular_enable},
        {MEAS_CONFIG_RDB_PREFIX ".cellular.percentile", &cellular_percentile},
        {0, 0},
    };

    /* update rdb settings */
    read_rdb_settings(rdb_to_c_var, pf_enable);
    rdb_subscribe_own(MEAS_CONFIG_RDB_TRIGGER, 0);

    /*
    ///////////////////////////////////////////////
    initiate local objects
    ///////////////////////////////////////////////
    */

    syslog(LOG_DEBUG, "* startup settings");
    syslog(LOG_DEBUG, "history count = %d", hist_count);
    syslog(LOG_DEBUG, "recv idle KiBPS = %d KiBPS", recv_idle_kibps);
    syslog(LOG_DEBUG, "sent idle KiBPS = %d KiBPS", sent_idle_kibps);
    syslog(LOG_DEBUG, "sample period = %d sec", sample_sec);
    syslog(LOG_DEBUG, "measurement period = %d min", meas_min);
    syslog(LOG_DEBUG, "cellular meas enable = %d", cellular_enable);
    syslog(LOG_DEBUG, "cellular meas percentile = %d", cellular_percentile);

    /* initialise history entry */
    meas_history_entry_init(&hent);
    meas_history_global_entry_init(&hent_global);

    /* initiate history and measure bot */
    meas_history_init(&mhist_global, -1, hist_count, 1);
    for (i = 0; i < LINK_PROFILE_COUNT; i++) {
        meas_history_init(&mhist[i], i, hist_count, 0);

        meas_bot_init(&meas_bot_recv[i], recv_idle_kibps * 1024);
        meas_bot_init(&meas_bot_sent[i], sent_idle_kibps * 1024);

        store_bot_init(&store_bot_uptime[i], meas_min * 60 / sample_sec + 1, STAT_TYPE_INT);
    }

    /* initiate clocks */
    tick_clock_init(&sample_tm, sample_sec * 1000);
    tick_clock_init(&meas_tm, meas_min * 60 * 1000);

    /* initiate link_profile_meas */
    link_profile_meas_collection_init();

    /* reset measurement bot */
    for (i = 0; i < LINK_PROFILE_COUNT; i++) {
        meas_bot_reset(&meas_bot_recv[i]);
        meas_bot_reset(&meas_bot_sent[i]);
    }

    /* initialise cellular meas */
    cellular_meas_init(meas_min * 60 / sample_sec + 1, cellular_percentile);

    /*
    ///////////////////////////////////////////////
    prepare for select loop
    ///////////////////////////////////////////////
    */

    /* update && get clock */
    tick_clock_update();

    /* read measurement */
    link_profile_meas_collection_read(&duration);
    for (i = 0; i < LINK_PROFILE_COUNT; i++) {
        lpm = link_profile_meas_collection_get(i);
        if (pf_enable[i]) {
            link_profile_meas_read(lpm);
        }
    }

    /* start clocks */
    tick_clock_do_trigger(&sample_tm);
    tick_clock_do_trigger(&meas_tm);

    /* get rdb handle */
    h = rdb_get_handle();

    /*
    ///////////////////////////////////////////////
    loop to sample
    ///////////////////////////////////////////////
    */

    while (!sig_term_caught) {

        /* update && get clock */
        tick_clock_update();

        /* get expiry stats */
        sample_expired = tick_clock_is_expired(&sample_tm);
        meas_expired = tick_clock_is_expired(&meas_tm);

        /* if instantaneous sampling interval or measurement is expired */
        if (sample_expired || meas_expired) {

            /* get duration between samples */
            duration = tick_clock_get_duration(&sample_tm);
            duration_meas = tick_clock_get_duration(&meas_tm);

            syslog(LOG_DEBUG, "sample timer expired (sample=%d,meas=%d,sample_duration=%d,meas_duration=%d)",
                   sample_expired, meas_expired, duration, duration_meas);

            /* read measurement */
            link_profile_meas_collection_read(&duration);

            /* feed diff into bots */
            for (i = 0; i < LINK_PROFILE_COUNT; i++) {
                rdb_pf_idx = i + 1;
                lpm = link_profile_meas_collection_get(i);

                if (!pf_enable[i]) {
                    syslog(LOG_DEBUG, "feed to link.profile.[%d] disabled", rdb_pf_idx);
                    continue;
                }

                /* read link.profile stats */
                link_profile_meas_read(lpm);

                if (lpm->if_running) {

                    /* get and reset rtt */
                    rdb_rtt = rdb_var_printf(MEA_HISTORY_RDB_RESULT ".%d.instantaneous_passive_rtt", rdb_pf_idx);
                    rdb_rtt_val = rdb_get_value(rdb_rtt);
                    rtt = rdb_rtt_val ? atoi(rdb_rtt_val) : 0;

                    /* reset rdb */
                    if (rdb_rtt_val) {
                        rdb_set_value(rdb_rtt, "", 0);
                    }

                    /* feed rtt if rtt is available */
                    if (rtt) {
                        meas_bot_feed_rtt(&meas_bot_sent[i], rtt);
                    }

                    /* feed diff into each bot */
                    meas_bot_feed(&meas_bot_recv[i], duration, lpm->diff_recv_bytes);
                    meas_bot_feed(&meas_bot_sent[i], duration, lpm->diff_sent_bytes);

                    /* assume UP for the full sample interval */
                    sample_val = sample_sec;
                    store_bot_feed(&store_bot_uptime[i], &sample_val);

                    /* get validation flags */
                    recv_valid = meas_bot_is_bps_valid(&meas_bot_recv[i]);
                    sent_valid = meas_bot_is_bps_valid(&meas_bot_sent[i]);

                    /* get averages and peaks */
                    recv_avg_bps = meas_bot_get_avg_bps(&meas_bot_recv[i]);
                    sent_avg_bps = meas_bot_get_avg_bps(&meas_bot_sent[i]);
                    recv_peak_bps = meas_bot_recv[i].peak_bps;
                    sent_peak_bps = meas_bot_sent[i].peak_bps;

                    /* log sampled traffic */
                    syslog(LOG_DEBUG, "feed to link.profile.[%d] up - recv (valid=%d,rdiff=%llu bytes,rdur=%d msec,rbytes=%llu bytes,ravg=%d bps, rpeak=%d bps)",
                           rdb_pf_idx,
                           recv_valid, lpm->diff_recv_bytes, meas_bot_recv[i].duration_for_avg, meas_bot_recv[i].diff_for_avg, recv_avg_bps, recv_peak_bps);
                    syslog(LOG_DEBUG, "feed to link.profile.[%d] up - sent (valid=%d,sdiff=%llu bytes,sdur=%d msec,sbytes=%llu bytes,savg=%d bps, speak=%d bps)",
                           rdb_pf_idx,
                           sent_valid, lpm->diff_sent_bytes, meas_bot_sent[i].duration_for_avg, meas_bot_sent[i].diff_for_avg, sent_avg_bps, sent_peak_bps);
                    syslog(LOG_DEBUG, "feed to link.profile.[%d] up - rtt (valid=%d,cnt=%d,cur=%d,avg=%d)",
                           rdb_pf_idx,
                           meas_bot_sent[i].rtt_valid, meas_bot_sent[i].rtt_count, rtt, meas_bot_sent[i].rtt);
                } else {
                    syslog(LOG_DEBUG, "feed to link.profile.[%d] down", rdb_pf_idx);
                }
            }

            /* collect cellular stats samples */
            if (cellular_enable) {
                cellular_meas_sample();
            }

            /* collect measurement */
            if (meas_expired) {

                for (i = 0; i < LINK_PROFILE_COUNT; i++) {

                    /* clean RDB if the profile is not enabled*/
                    if (!pf_enable[i]) {
                        syslog(LOG_DEBUG, "measurement timer expired skip #%d", i);
                        meas_history_clean_rdb(&mhist[i]);
                        continue;
                    }

                    /* get averages and peaks */
                    recv_avg_bps = meas_bot_get_avg_bps(&meas_bot_recv[i]);
                    sent_avg_bps = meas_bot_get_avg_bps(&meas_bot_sent[i]);
                    recv_peak_bps = meas_bot_recv[i].peak_bps;
                    sent_peak_bps = meas_bot_sent[i].peak_bps;

                    /* log recv and sent throughput information */
                    syslog(LOG_DEBUG, "measurement timer expired recv #%d (rdur=%d msec,rbytes=%llu bytes,ravg=%d bps,rpeak=%d bps",
                           i, meas_bot_recv[i].duration_for_avg, meas_bot_recv[i].diff_for_avg, recv_avg_bps, recv_peak_bps);
                    syslog(LOG_DEBUG, "measurement timer expired sent #%d (sdur=%d msec,sbytes=%llu bytes,sent_avg=%d bps,sent_peak=%d bps)",
                           i, meas_bot_sent[i].duration_for_avg, meas_bot_sent[i].diff_for_avg, sent_avg_bps, sent_peak_bps);
                    syslog(LOG_DEBUG, "measurement timer expired rtt #%d (valid=%d,rtt_count=%d,avg_rtt=%d)",
                           i, meas_bot_sent[i].rtt_valid, meas_bot_sent[i].rtt_count, meas_bot_sent[i].rtt);

                    /* get validation flags */
                    recv_valid = meas_bot_is_bps_valid(&meas_bot_recv[i]);
                    sent_valid = meas_bot_is_bps_valid(&meas_bot_sent[i]);

                    /*
                    ///////////////////////////////////////////////
                    add history record
                    ///////////////////////////////////////////////
                    */

                    /* build valid table */
                    hent.valid[MEAS_HISTORY_RECORD_RECV_AVG_BPS] = recv_valid;
                    hent.valid[MEAS_HISTORY_RECORD_SENT_AVG_BPS] = sent_valid;
                    hent.valid[MEAS_HISTORY_RECORD_RECV_PEAK_BPS] = recv_valid;
                    hent.valid[MEAS_HISTORY_RECORD_SENT_PEAK_BPS] = sent_valid;
                    hent.valid[MEAS_HISTORY_RECORD_RECV_DURATION] = recv_valid;
                    hent.valid[MEAS_HISTORY_RECORD_SENT_DURATION] = sent_valid;
                    hent.valid[MEAS_HISTORY_RECORD_RECV_BYTES] = recv_valid;
                    hent.valid[MEAS_HISTORY_RECORD_SENT_BYTES] = sent_valid;
                    hent.valid[MEAS_HISTORY_RECORD_RECV_TOTAL_BYTES] = recv_valid;
                    hent.valid[MEAS_HISTORY_RECORD_SENT_TOTAL_BYTES] = sent_valid;

                    /* build record */
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_RECV_AVG_BPS] = recv_avg_bps;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_SENT_AVG_BPS] = sent_avg_bps;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_RECV_PEAK_BPS] = recv_peak_bps;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_SENT_PEAK_BPS] = sent_peak_bps;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_RECV_DURATION] = meas_bot_recv[i].duration_for_avg;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_SENT_DURATION] = meas_bot_sent[i].duration_for_avg;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_RECV_BYTES] = meas_bot_recv[i].diff_for_avg;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_SENT_BYTES] = meas_bot_sent[i].diff_for_avg;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_RECV_TOTAL_BYTES] = meas_bot_recv[i].bytes;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_SENT_TOTAL_BYTES] = meas_bot_sent[i].bytes;

                    /* set proper RTT validation flag and value */
                    hent.valid[MEAS_HISTORY_RECORD_RTT] = meas_bot_sent[i].rtt_valid;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_RTT] = meas_bot_sent[i].rtt;

                    hent.valid[MEAS_HISTORY_RECORD_IPV4_ADDR] = record_ip_addr(0, i + 1, (char *)hent.record[MEAS_HISTORY_RECORD_IPV4_ADDR]);
                    hent.valid[MEAS_HISTORY_RECORD_IPV6_ADDR] = record_ip_addr(1, i + 1, (char *)hent.record[MEAS_HISTORY_RECORD_IPV6_ADDR]);
                    hent.valid[MEAS_HISTORY_RECORD_UPTIME] = 1;
                    *(int64_t *)hent.record[MEAS_HISTORY_RECORD_UPTIME] = *(int64_t *)store_bot_get_by_sum(&store_bot_uptime[i]);

                    /* add entry to history */
                    meas_history_add(&mhist[i], &hent);

                    /* flush to rdb */
                    meas_history_flush_to_rdb(&mhist[i]);
                }

                /* set timestamp */
                hent_global.valid[MEAS_HISTORY_GLOBAL_RECORD_TIMESTAMP] = 1;
                *(int64_t *)hent_global.record[MEAS_HISTORY_GLOBAL_RECORD_TIMESTAMP] = tick_clock_get_time();

                /* add cellular stats measurement records */
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_ECGI, CELLULAR_MEAS_ECGI, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_NCGI, CELLULAR_MEAS_NCGI, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_ENODEB, CELLULAR_MEAS_ENODEB, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_GNODEB, CELLULAR_MEAS_GNODEB, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_CELLID, CELLULAR_MEAS_CELLID, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_GNB_CELLID, CELLULAR_MEAS_GNB_CELLID, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_PCI, CELLULAR_MEAS_PCI, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_GNB_PCI, CELLULAR_MEAS_GNB_PCI, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_EARFCN, CELLULAR_MEAS_EARFCN, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_NR_ARFCN, CELLULAR_MEAS_NR_ARFCN, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_GNB_SSBINDEX, CELLULAR_MEAS_GNB_SSBINDEX, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_RSRP, CELLULAR_MEAS_RSRP, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_SS_RSRP, CELLULAR_MEAS_SS_RSRP, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_RSRQ, CELLULAR_MEAS_RSRQ, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_SS_RSRQ, CELLULAR_MEAS_SS_RSRQ, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_SINR, CELLULAR_MEAS_SINR, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_SS_SINR, CELLULAR_MEAS_SS_SINR, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_RSSI, CELLULAR_MEAS_RSSI, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_NR_RSSI, CELLULAR_MEAS_NR_RSSI, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_CQI, CELLULAR_MEAS_CQI, &hent_global, cellular_enable);
                record_cellular_stat(MEAS_HISTORY_GLOBAL_RECORD_NR_CQI, CELLULAR_MEAS_NR_CQI, &hent_global, cellular_enable);

                /* add entry to history */
                meas_history_add(&mhist_global, &hent_global);

                /* flush to global rdb */
                meas_history_flush_to_rdb(&mhist_global);
            }


            /* mark as triggered */
            if (sample_expired) {
                tick_clock_do_trigger(&sample_tm);
            }

            /*
            ///////////////////////////////////////////////
            finish a measurement period
            ///////////////////////////////////////////////
            */

            if (meas_expired) {

                for (i = 0; i < LINK_PROFILE_COUNT; i++) {
                    /* reset measurement bot */
                    meas_bot_reset(&meas_bot_recv[i]);
                    meas_bot_reset(&meas_bot_sent[i]);

                    store_bot_reset(&store_bot_uptime[i]);
                }

                cellular_meas_reset();

                /* mark tick clock as triggered */
                tick_clock_do_trigger(&meas_tm);
            }
        }

        /* set poll interval */
        tv.tv_sec = 0;
        tv.tv_usec = sample_sec * 1000000;

        /* set read fds */
        FD_ZERO(&readfds);
        FD_SET(h, &readfds);

        /* select - wait for interval */
        stat = select(h + 1, &readfds, 0, 0, &tv);

        /* handle errors and signals */
        if (stat < 0) {
            if (errno == EINTR) {
                syslog(LOG_NOTICE, "signal caught, retry to select");
            } else {
                syslog(LOG_NOTICE, "failed to select - %s", strerror(errno));
                break;
            }
        } else if (FD_ISSET(h, &readfds)) {

            /* reset trigger */
            rdb_get_value(MEAS_CONFIG_RDB_TRIGGER);

            syslog(LOG_DEBUG, "rdb trigger detected. re-initiate settings");
            read_rdb_settings(rdb_to_c_var, pf_enable);

            /* re-initiate history and measure bot */
            meas_history_reinit(&mhist_global, hist_count);

            for (i = 0; i < LINK_PROFILE_COUNT; i++) {
                meas_history_reinit(&mhist[i], hist_count);

                meas_bot_init(&meas_bot_recv[i], recv_idle_kibps * 1024);
                meas_bot_init(&meas_bot_sent[i], sent_idle_kibps * 1024);

                store_bot_reinit(&store_bot_uptime[i], meas_min * 60 / sample_sec + 1, STAT_TYPE_INT);
            }

            cellular_meas_reinit(meas_min * 60 / sample_sec + 1, cellular_percentile);

            /* initiate clocks */
            tick_clock_reinit(&sample_tm, sample_sec * 1000);
            tick_clock_reinit(&meas_tm, meas_min * 60 * 1000);

            /* reset measurement bot */
            for (i = 0; i < LINK_PROFILE_COUNT; i++) {
                meas_bot_reset(&meas_bot_recv[i]);
                meas_bot_reset(&meas_bot_sent[i]);
            }

            /* start clocks */
            tick_clock_do_trigger(&sample_tm);
            tick_clock_do_trigger(&meas_tm);
        }


    }


    /* set return code */
    retcode = 0;

    /* finish clocks */
    tick_clock_fini(&sample_tm);
    tick_clock_fini(&meas_tm);

    /* finish measure bot and measurement history */
    for (i = 0; i < LINK_PROFILE_COUNT; i++) {
        meas_bot_fini(&meas_bot_recv[i]);
        meas_bot_fini(&meas_bot_sent[i]);

        store_bot_fini(&store_bot_uptime[i]);

        meas_history_fini(&mhist[i]);
    }
    meas_history_fini(&mhist_global);

    meas_history_global_entry_fini(&hent_global);
    meas_history_entry_fini(&hent);

    /* finish link_profile_measure */
    link_profile_meas_collection_fini();

    /* finish cellular measurement */
    cellular_meas_fini();

    /* finish rdb */
    rdb_fini();

    return retcode;
}
