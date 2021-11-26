/*
 * Glue logic for OWA-NIT communications to maintain NIT mode and LEDs
 *
 * This daemon watches and poll a few key RDBs and abstracts them into new RDBs for NIT use
 *
 * Currently, 4 abstracted RDBs are maintained:
 *  eth.connected: if Ethernet/PoE is connected
 *  eth.act: if there is TX/RX activity on Ethernet link
 *  wan.signal.strength: WAN signal strength - RSRP, SINR, ...
 *  wan.link.state: WAN link state - 0, 1, 2, ...
 *
 * The purpose of the abstracted RDBs are to decouple NIT from OWA specific knowledge.
 * e.g. NIT does not need to know it should read hw.switch.port.0.status and parse it
 * to know if Ethernet/PoE is connected.
 * This glue logic acts as a translator between various OWAs and a universal NIT.
 *
 * Currently, the logic is written for Magpie/Titan. But could be extended to other
 * OWAs if required. Build time configuration (V variables) can be used to
 * differentiate variants.
 *
 * TODO: Current implementation has the following issues and needs a full redesign/refactor
 * - Everything is in one file
 * - No decoupling between interface to NIT and particular logic for each OWA
 * - No abstract interface for various OWAs. Using #ifdef in sole one file will soon lead to very long spaghetti code
 * - The main loop does not support more complex usage of other OWAs (in future)
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <rdb_ops.h>
#include "utils.h"

// OWA specific RDBs
#define SWITCH_STATUS_RDB "hw.switch.port.0.status"

#define REG_STAT_RDB "wwan.0.system_network_status.reg_stat"
#define APN_STAT_RDB "link.profile.1.connect_progress"

#define AUTO_MEAS_PREFIX "wwan.0.cell_measurement."
#define MANUAL_MEAS_PREFIX "wwan.0.manual_cell_meas."
#define AUTO_MEAS_QTY_RDB (AUTO_MEAS_PREFIX "qty")
#define MANUAL_MEAS_QTY_RDB (MANUAL_MEAS_PREFIX "qty")
#define MANUAL_MEAS_SEQ_RDB (MANUAL_MEAS_PREFIX "seq")

// abstracted RDBs for NIT use
#define WAN_LINK_STATE_RDB "wan.link.state"
#define ETH_CONNECTED_RDB "eth.connected"
#define ETH_ACT_RDB "eth.act"
#define WAN_SIGNAL_STRENGTH_RDB "wan.signal.strength"

#define BUFSZ 128

// polling period
#define SELECT_TIMEOUT_MS 200

// assumed rsrp when no signal is present
#define NO_SIGNAL_RSRP -150

// manual cell measurements are no longer valid if they are not updated in 10s
#define MIN_MANUAL_REFRESH_TIME_SECS 10

// the following RDBs are watched for changes, while other RDBs are polled
static const char * const rdb_watches[] = {
    REG_STAT_RDB,
    APN_STAT_RDB,
};

static struct rdb_session * rdb_s;

volatile static sig_atomic_t terminate = 0;

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
}

/*
 * initialise rdb watchers
 *
 * @return 0 on success; negative error code on failure
 */
static int init_watchers(void)
{
    int idx;
    int ret;
    const char * rdbk;

    // watch RDBs
    for (idx = 0; idx < ARRAY_SIZE(rdb_watches); idx++) {
        rdbk = rdb_watches[idx];
        ret = rdb_subscribe(rdb_s, rdbk);
        if (ret < 0 && ret != -ENOENT) {
            BLOG_ERR("Failed to subscribe %s\n", rdbk);
            return ret;
        }
    }
    return 0;
}

/*
 * milli-seconds to struct timeval conversion
 *
 * @param ms Milli-second as a long integer
 * @param tv A pointer to struct timeval to store the result
 */
static void ms_to_timeval(long ms, struct timeval * tv)
{
    tv->tv_sec = ms / 1000L;
    tv->tv_usec = (ms % 1000L) * 1000L;
}

/*
 * Set an RDB with an integer value
 *
 * @param s RDB session handle
 * @param szName RDB name
 * @param val The integer value to be set
 * @return 0 on success; negative error code on failure
 */
static int rdb_set_int(struct rdb_session *s, const char *szName, int val)
{
    int ret;
    char buf[BUFSZ];
    ret = snprintf(buf, sizeof(buf), "%d", val);
    if (ret < 0) {
        return ret;
    }
    if (ret >= sizeof(buf)) {
        return -EOVERFLOW;
    }
    return rdb_set_string(s, szName, buf);
}

/*
 * Set an RDB with a float value
 *
 * @param s RDB session handle
 * @param szName RDB name
 * @param val The float value to be set
 * @return 0 on success; negative error code on failure
 */
static int rdb_set_float(struct rdb_session *s, const char *szName, float val)
{
    int ret;
    char buf[BUFSZ];
    ret = snprintf(buf, sizeof(buf), "%.1f", val);
    if (ret < 0) {
        return ret;
    }
    if (ret >= sizeof(buf)) {
        return -EOVERFLOW;
    }
    return rdb_set_string(s, szName, buf);
}

/*
 * Update WAN link state RDB by checking LTE registration and APN status RDBs
 *
 * @return 0 on success; negative error code on failure
 */
static int update_wan_link_state(void)
{
    int ret;
    char apn_stat[BUFSZ];
    int reg_stat;

    int link_state;
    static int prev_link_state = -1;

    /*
     * LTE registration status
     *  1 - registered
     *  2 - registering
     *  5 - registered
     *  6 - registered
     *  7 - registered
     *  8 - registered
     *  9 - registered
     *  10 - registered
     *  otherwise - deregistered
     */
    ret = rdb_get_int(rdb_s, REG_STAT_RDB, &reg_stat);
    if (ret) {
        reg_stat = 0;
    }
    BLOG_INFO("reg_stat=%d\n", reg_stat);

    /*
     * APN connection status
     *  established
     *  establishing
     *  otherwise - disconnected
     */
    ret = rdb_get_string(rdb_s, APN_STAT_RDB, apn_stat, sizeof(apn_stat));
    if (ret) {
        apn_stat[0] = '\0';
    }
    BLOG_INFO("apn_stat=%s\n", apn_stat);

    if (reg_stat == 1 || (reg_stat >=5 && reg_stat <= 10)) { // registered
        if (!strcmp(apn_stat, "established")) {
            link_state = 4;
        } else if (!strcmp(apn_stat, "establishing")) {
            link_state = 3;
        } else {
            link_state = 2;
        }
    } else if (reg_stat == 2) { // registering
        link_state = 1;
    } else { // deregistered
        link_state = 0;
    }

    // skip unnecessary rdb set
    if (link_state == prev_link_state) {
        return 0;
    }

    ret = rdb_set_int(rdb_s, WAN_LINK_STATE_RDB, link_state);
    if (!ret) {
        prev_link_state = link_state;
    }

    return ret;
}

/*
 * Update Ethernet/PoE connection status
 *
 * @return 0 on success; negative error code on failure
 */
static int update_eth_connected(void)
{
    int ret;
    char rdbv[BUFSZ];
    static int prev_connected = -1;
    int connected;

    ret = rdb_get_string(rdb_s, SWITCH_STATUS_RDB, rdbv, sizeof(rdbv));
    if (ret) {
        BLOG_ERR("Failed to get RDB %s\n", SWITCH_STATUS_RDB);
        return ret;
    }
    // It is 4-character string, e.g. urhf, where the first letter means u: up d: down
    connected = (rdbv[0] == 'u');

    // skip unnecessary rdb set
    if (connected == prev_connected) {
        return 0;
    }

    BLOG_NOTICE("eth connected: %d\n", connected);
    ret = rdb_set_int(rdb_s, ETH_CONNECTED_RDB, connected);
    if (!ret) {
        prev_connected = connected;
    }

    return ret;
}

/*
 * Update Ethernet link activity status
 *
 * @return 0 on success; negative error code on failure
 */
static int update_eth_act(void)
{
    int ret;
    int act = 0;
    static int prev_act = -1;
    int64_t packets;
    static int64_t prev_packets = -1;
    FILE *fp = fopen("/sys/class/net/eth0/statistics/rx_packets", "r");
    if (fp) {
        if (fscanf(fp, "%"SCNd64, &packets) == 1) {
            if (packets > prev_packets) {
                prev_packets = packets;
                act = 1;
            }
        }
        fclose(fp);
    }

    // skip unnecessary rdb set
    if (act == prev_act) {
        return 0;
    }

    BLOG_INFO("eth rx packets: %"PRId64"\n", packets);
    ret = rdb_set_int(rdb_s, ETH_ACT_RDB, act);
    if (!ret) {
        prev_act = act;
    }

    return ret;
}

/*
 * Get the max RSRP from a cell measurement result set
 *
 * @param qty The number of cells included in the measurement set
 * @param prefix The RDB prefix for the measurement result set
 * @return The max RSRP value. On error, NO_SIGNAL_RSRP is returned
 */
static float get_max_rsrp(int qty, const char * prefix)
{
    int ret;
    char rdb_name[MAX_NAME_LENGTH];
    char rdbv[BUFSZ];
    float max_rsrp = NO_SIGNAL_RSRP;
    int i;
    char type;
    int channel, cell_id;
    float ss, sq;
    for (i = 0; i < qty; i++) {
        ret = snprintf(rdb_name, sizeof(rdb_name), "%s%d", prefix, i);
        if (ret < 0 || ret >= sizeof(rdb_name)) {
            BLOG_ERR("Failed to build rdb_name %s%d\n", prefix, i);
            break;
        }
        ret = rdb_get_string(rdb_s, rdb_name, rdbv, sizeof(rdbv));
        if (ret) {
            BLOG_ERR("Failed to ger rdb %s\n", rdb_name);
            break;
        }
        ret = sscanf(rdbv, "%c,%d,%d,%f,%f", &type, &channel, &cell_id, &ss, &sq);
        if (ret != 5) {
            BLOG_ERR("Invalid measurement %s=%s\n", rdb_name, rdbv);
            continue;
        }
        if (ss > max_rsrp) {
            max_rsrp = ss;
        }
    }
    return max_rsrp;
}

/*
 * Update WAN signal strength RDB from auto/manual cell scan measurements
 *
 * @return 0 on success; negative error code on failure
 */
static int update_wan_signal_strength(void)
{
    int ret;
    int auto_meas_qty;
    int manual_meas_qty;
    int manual_meas_seq;

    time_t now;

    static int manual_meas_last_seq = -1;
    static time_t manual_meas_last_ts;

    float auto_rsrp, manual_rsrp, max_rsrp;
    static float prev_max_rsrp = NO_SIGNAL_RSRP - 1;

    now = time(NULL);

    ret = rdb_get_int(rdb_s, AUTO_MEAS_QTY_RDB, &auto_meas_qty);
    if (ret) {
        auto_meas_qty = 0;
    }
    ret = rdb_get_int(rdb_s, MANUAL_MEAS_QTY_RDB, &manual_meas_qty);
    if (ret) {
        manual_meas_qty = 0;
    }
    ret = rdb_get_int(rdb_s, MANUAL_MEAS_SEQ_RDB, &manual_meas_seq);
    if (ret) {
        manual_meas_seq = -1;
    }

    if (manual_meas_last_seq != manual_meas_seq) {
        manual_meas_last_seq = manual_meas_seq;
        manual_meas_last_ts = now;
    }

    if (difftime(now, manual_meas_last_ts) > MIN_MANUAL_REFRESH_TIME_SECS) {
        // manual cell measurements are outdated, ignore them
        manual_meas_qty = 0;
    }

    auto_rsrp = get_max_rsrp(auto_meas_qty, AUTO_MEAS_PREFIX);
    BLOG_INFO("auto_rsrp=%.1f\n", auto_rsrp);
    manual_rsrp = get_max_rsrp(manual_meas_qty, MANUAL_MEAS_PREFIX);
    BLOG_INFO("manual_rsrp=%.1f\n", manual_rsrp);
    max_rsrp = manual_rsrp == NO_SIGNAL_RSRP ? auto_rsrp : manual_rsrp;

    // skip unnecessary rdb set
    if (fabs(max_rsrp - prev_max_rsrp) < 0.05) {
        return 0;
    }

    ret = rdb_set_float(rdb_s, WAN_SIGNAL_STRENGTH_RDB, max_rsrp);
    if (!ret) {
        prev_max_rsrp = max_rsrp;
    }

    return ret;
}

/*
 * Process a single RDB trigger
 *
 * @param rdbk RDB key that is triggered.
 * @return 0 on success; negative error code on failure
 */
static int process_single(const char * rdbk)
{
    int ret;
    if (!strcmp(rdbk, REG_STAT_RDB) || !strcmp(rdbk, APN_STAT_RDB)) {
        ret = update_wan_link_state();
        if (ret) {
            BLOG_ERR("Failed to update WAN link state (%d): triggered %s\n", ret, rdbk);
        }
    } else {
        BLOG_ERR("Unknown RDB trigger: %s\n", rdbk);
        ret = -1;
    }
    return ret;
}

/*
 * Process RDB trigger events
 *
 * @param triggered_rdbs A string containing triggered RDBs separated by '&'
 * @return 0 on success; negative error code on failure
 */
static int process_triggers(char * triggered_rdbs)
{
    char * rdbk;
    int ret;

    rdbk = strtok(triggered_rdbs, "&");
    while (rdbk) {
        ret = process_single(rdbk);
        if (ret < 0) {
            BLOG_ERR("Failed to process triggered RDB: %s (%d)\n", rdbk, ret);
            return ret;
        }
        rdbk = strtok(NULL, "&");
    }
    return 0;
}

/*
 * Perform one polling
 */
static void process_poll(void)
{
    int ret;
    ret = update_eth_connected();
    if (ret) {
        BLOG_ERR("Failed to update %s\n", ETH_CONNECTED_RDB);
    }
    ret = update_eth_act();
    if (ret) {
        BLOG_ERR("Failed to update %s\n", ETH_ACT_RDB);
    }
    ret = update_wan_signal_strength();
    if (ret) {
        BLOG_ERR("Failed to update %s\n", WAN_SIGNAL_STRENGTH_RDB);
    }
}

/*
 * The main loop
 *
 * @return 0 on success; negative error code on failure
 */
static int main_loop(void)
{
    int ret;
    int rdbfd;
    fd_set fdset;
    struct timeval timeout;

    int name_len;
    char name_buf[MAX_NAME_LENGTH];

    rdbfd = rdb_fd(rdb_s);
    if (rdbfd < 0) {
        BLOG_ERR("Failed to get rdb fd\n");
        return -1;
    }

    while (!terminate) {
        FD_ZERO(&fdset);
        FD_SET(rdbfd, &fdset);
        ms_to_timeval(SELECT_TIMEOUT_MS, &timeout);
        ret = select(rdbfd + 1, &fdset, NULL, NULL, &timeout);

        if (ret < 0) { // error
            int tmp = errno;
            BLOG_ERR("select returned %d, errno: %d\n", ret, tmp);
            if (tmp == EINTR) { // interrupted by signal
                BLOG_NOTICE("Exiting on signal\n");
                break;
            }
            return ret;
        }

        if (ret > 0) { // RDB triggered
            if (!FD_ISSET(rdbfd, &fdset)) {
                // this should not happen for now
                continue;
            }
            name_len = sizeof(name_buf) - 1;
            ret = rdb_getnames(rdb_s, "", name_buf, &name_len, TRIGGERED);
            if (ret) {
                BLOG_ERR("Failed to get triggered RDB names (%d)\n", ret);
                return ret;
            }
            ret = process_triggers(name_buf);
            if (ret) {
                BLOG_ERR("Failed to process RDB triggers: %s (%d)\n", name_buf, ret);
            }
        } else { // it is time to poll
            process_poll();
        }
    }

    return 0;
}

/*
 * The main entry
 *
 * @return 0 on success; negative error code on failure
 */
int main(void)
{
    int ret;

    openlog("owa_nit_glue", LOG_CONS, LOG_USER);

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        ret = -errno;
        BLOG_ERR("Failed to register signal handler\n");
        goto fin_log;
    }

    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        ret = -1;
        goto fin_log;
    }

    ret = init_watchers();
    if (ret < 0) {
        BLOG_ERR("Failed to init rdb watchers\n");
        goto fin_rdb;
    }

    ret = main_loop();
    if (ret < 0) {
        BLOG_ERR("main loop terminated unexpectedly\n");
    } else {
        BLOG_NOTICE("main loop exited normally\n");
    }

fin_rdb:
    rdb_close(&rdb_s);

fin_log:
    closelog();

    return ret;
}
