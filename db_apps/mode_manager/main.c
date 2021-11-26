/*
 * Install Tool Mode Manager
 *
 * This daemon maintains install_tool.mode according to various RDBs from other
 * daemons/services.
 *
 * Please refer to state_machine.xml (draw.io) for mode transition diagram.
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

#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <rdb_ops.h>
#include <time.h>

#include "utils.h"

// The time that the battery mode should remain in the beginging
#define SHOW_BATT_TIME_SEC 30

// daemon start time
static time_t start_time;

#define INSTALL_TOOL_MODE_RDB "install_tool.mode"
// all possible modes
typedef enum {
    IT_MODE_BATTERY,     // battery check mode (disconnected to OWA)
    IT_MODE_CALIBRATION, // calibration mode (disconnected to OWA)
    IT_MODE_NORMAL,      // normal operation mode (connected to OWA)
    IT_MODE_SCAN,        // cell scanning mode (connected to OWA)
    IT_MODE_FLASH_OWA,   // flashing OWA in progress (connected to OWA)
    IT_MODE_PEER,        // peer searching mode (handshaking with OWA)
    IT_MODE_FLASH_SELF,  // flashing Lark in progress (disconnected to OWA)
    IT_MODE_POWEROFF,    // lark start to power off
} it_mode_t;

static it_mode_t it_mode = IT_MODE_BATTERY;

/* all mode transitions are triggered by the following RDBs */
#define OWA_CONNECTED_RDB "owa.connected"
#define RDB_BRIDGE_STATUS_RDB "service.rdb_bridge.connection_status"
#define OWA_ETH_CONNECTED_RDB "owa.eth.connected"
#define COMPASS_CALIB_STATUS_RDB "owa.orien.status"
#define INSTALL_TOOL_UPDATE_RDB "install_tool.update"
#define OWA_UPDATE_RDB "owa.update"
#define SYSTEM_POWEROFF_RDB "service.system.poweroff"
#define DCIN_PRESENT_RDB "system.battery.dcin_present"

#define SELECT_TIMEOUT_MS 2000

#define INIT_RDB_NAMES_LEN 1000
#define RDBV_BUF_LEN 16

/* the following RDBs are watched for changes */
static const char * const rdb_watches[] = {
    OWA_CONNECTED_RDB,
    RDB_BRIDGE_STATUS_RDB,
    OWA_ETH_CONNECTED_RDB,
    COMPASS_CALIB_STATUS_RDB,
    INSTALL_TOOL_UPDATE_RDB,
    OWA_UPDATE_RDB,
    SYSTEM_POWEROFF_RDB,
    DCIN_PRESENT_RDB
};

static struct rdb_session * rdb_s;
static int rdbfd;

volatile static int terminate = 0;

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
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
 * Set install tool mode
 *
 * @param mode The install tool mode value
 * @return 0 on success; or negative error code on failure
 * @note On success, both it_mode global and INSTALL_TOOL_MODE_RDB will be set
 * accordingly to param mode
 */
static int set_mode(it_mode_t mode)
{
    int ret;
    const char * val;

    switch (mode) {
    case IT_MODE_BATTERY:
        val = "battery";
        break;
    case IT_MODE_CALIBRATION:
        val = "calibration";
        break;
    case IT_MODE_NORMAL:
        val = "normal";
        break;
    case IT_MODE_SCAN:
        val = "scan";
        break;
    case IT_MODE_FLASH_OWA:
        val = "flash_owa";
        break;
    case IT_MODE_PEER:
        val = "peer";
        break;
    case IT_MODE_FLASH_SELF:
        val = "flash_self";
        break;
    case IT_MODE_POWEROFF:
        val = "power_off";
        break;
    default:
        return -1;
    }

    ret = rdb_set_string(rdb_s, INSTALL_TOOL_MODE_RDB, val);
    if (!ret) {
        it_mode = mode;
    }
    BLOG_INFO("set mode = %s\n", val);
    return ret;
}

/*
 * initialise mode and rdb watchers
 *
 * @return 0 on success; negative error code on failure
 */
static int init(void)
{
    int idx;
    int ret;
    const char * rdbk;

    // record the start time
    time(&start_time);

    // set initial mode
    set_mode(IT_MODE_BATTERY);

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
 * Check if Install Tool is connected to an OWA
 */
static int is_owa_connected(void)
{
    char rdbv[RDBV_BUF_LEN];
    int ret;
    ret = rdb_get_string(rdb_s, OWA_CONNECTED_RDB, rdbv, sizeof(rdbv));
    return !ret && !strcmp(rdbv, "1");
}

/*
 * Check if PoE is connected to OWA
 */
static int is_poe_connected(void)
{
    char rdbv[RDBV_BUF_LEN];
    int ret;
    ret = rdb_get_string(rdb_s, OWA_ETH_CONNECTED_RDB, rdbv, sizeof(rdbv));
    return !ret && !strcmp(rdbv, "1");
}

/*
 * Check if Install Tool is updating (flashing firmware)
 */
static int is_self_updating(void)
{
    char rdbv[RDBV_BUF_LEN];
    int ret;
    ret = rdb_get_string(rdb_s, INSTALL_TOOL_UPDATE_RDB, rdbv, sizeof(rdbv));
    return !ret && !strcmp(rdbv, "start");
}

/*
 * Check if OWA is updating (flashing firmware)
 */
static int is_owa_updating(void)
{
    char rdbv[RDBV_BUF_LEN];
    int ret;
    ret = rdb_get_string(rdb_s, OWA_UPDATE_RDB, rdbv, sizeof(rdbv));
    return !ret && !strcmp(rdbv, "start");
}

/*
 * Check if orientation sensor needs calibration/is calibrating
 */
static int is_calibrating(void)
{
    int status;
    int ret;
    ret = rdb_get_int(rdb_s, COMPASS_CALIB_STATUS_RDB, &status);
    return ret || (status != 0 && status != 2); // 0 or 2 means calibrated
}

/*
 * Check if RDB service is synced
 */
static int rdb_bridge_synced(void)
{
    char rdbv[RDBV_BUF_LEN];
    int ret;
    ret = rdb_get_string(rdb_s, RDB_BRIDGE_STATUS_RDB, rdbv, sizeof(rdbv));
    return !ret && !strcmp(rdbv, "synchronised");
}

/*
 * Check if system starts to power off
 *
 * @param check_forced Whether to check normal poweroff or forced poweroff.
 * 0 - check normal poweroff;
 * 1 - check forced poweroff;
 * otherwise - check any forms of poweroff
 * @return 1 if the specific poweroff form is triggered, otherwise return 0
 */
static int is_going_poweroff(int check_forced)
{
     char rdbv[RDBV_BUF_LEN];
     int ret;
     ret = rdb_get_string(rdb_s, SYSTEM_POWEROFF_RDB, rdbv, sizeof(rdbv));
     if (ret) {
         return 0;
     }
     switch (check_forced) {
     case 0:
         return !strcmp(rdbv, "1");
     case 1:
         return !strcmp(rdbv, "11");
     default:
         return rdbv[0] == '1';
     }
}

/*
 * Check if power adapter is plugged in
 *
 * @return 1 if adapter is plugged in, 0 otherwise
 */
static int is_dcin_present(void)
{
    int status;
    int ret;
    ret = rdb_get_int(rdb_s, DCIN_PRESENT_RDB, &status);
    return !ret && status == 1;
}

/*
 * Helper macro to compare an RDB key with an expected key
 * (name comparison, not value)
 * Evaluate to True if keys are equal or rdbk is NULL; False otherwise
 */
#define CMP_RDB(rdbk, expected) (!rdbk || !strcmp(rdbk, expected))

/*
 * Helper macro to check an RDB variable against an expected name.
 * If rdbk matches expected name or rdbk is NULL, and cond is met,
 * set install tool mode to argument mode and return 1;
 * If rdbk matches expected name but cond is not met, return 0;
 * Otherwise, pass through.
 */
#define CHK_RDB_RET(rdbk, expected, cond, mode) { \
        if (CMP_RDB(rdbk, expected)) { \
            if (cond) { \
                set_mode(mode); \
                return 1; \
            } \
            if (rdbk) { \
                return 0; \
            } \
        } \
    }

/*
 * Helper macro to check an RDB variable against an expected name.
 * If rdbk matches expected name or rdbk is NULL, and cond1 is met,
 * set install tool mode to argument mode1 and return 1;
 * If rdbk matches expected name or rdbk is NULL, and cond2 is met,
 * set install tool mode to argument mode2 and return 1;
 * If rdbk matches expected name but neither cond is met, return 0;
 * Otherwise, pass through.
 */
#define CHK_RDB_RET2(rdbk, expected, cond1, mode1, cond2, mode2) { \
        if (CMP_RDB(rdbk, expected)) { \
            if (cond1) { \
                set_mode(mode1); \
                return 1; \
            } else if (cond2) { \
                set_mode(mode2); \
                return 1; \
            } \
            if (rdbk) { \
                return 0; \
            } \
        } \
    }



/*
 * Process a single RDB trigger
 *
 * @param rdbk RDB key that is triggered. If NULL is passed in, all relevant
 * RDBs will be checked and mode changed accordingly, useful for polling
 * @retval 1 mode changed
 * @retval 0 mode is not changed
 * @retval -1 error
 */
static int process_single(const char * rdbk)
{
    time_t now;

    if (it_mode != IT_MODE_POWEROFF && CMP_RDB(rdbk, SYSTEM_POWEROFF_RDB) &&
        is_going_poweroff(1)) { // forced an emergency poweroff triggered
        set_mode(IT_MODE_POWEROFF);
        return 1;
    }

    switch (it_mode) {
    case IT_MODE_BATTERY:

        time(&now);
        // mode transition is not allowed in the first SHOW_BATT_TIME_SEC
        if(difftime(now, start_time) < SHOW_BATT_TIME_SEC) {
            // return 1 allows polling to events deferred
            return 1;
        }

        CHK_RDB_RET(rdbk, INSTALL_TOOL_UPDATE_RDB, is_self_updating(),
                    IT_MODE_FLASH_SELF);
        CHK_RDB_RET(rdbk, OWA_CONNECTED_RDB, is_owa_connected(), IT_MODE_PEER);
        CHK_RDB_RET(rdbk, COMPASS_CALIB_STATUS_RDB,
                    is_calibrating() && !is_dcin_present(),
                    IT_MODE_CALIBRATION);
        CHK_RDB_RET(rdbk, SYSTEM_POWEROFF_RDB, is_going_poweroff(0),
                    IT_MODE_POWEROFF);
        return 0;

    case IT_MODE_FLASH_SELF:
        CHK_RDB_RET(rdbk, INSTALL_TOOL_UPDATE_RDB, !is_self_updating(),
                    IT_MODE_BATTERY);
        return 0;

    case IT_MODE_CALIBRATION:
        CHK_RDB_RET(rdbk, COMPASS_CALIB_STATUS_RDB, !is_calibrating(),
                    IT_MODE_BATTERY);
        CHK_RDB_RET(rdbk, SYSTEM_POWEROFF_RDB, is_going_poweroff(0),
                    IT_MODE_POWEROFF);
        CHK_RDB_RET(rdbk, INSTALL_TOOL_UPDATE_RDB, is_self_updating(),
                    IT_MODE_FLASH_SELF);
        CHK_RDB_RET(rdbk, OWA_CONNECTED_RDB, is_owa_connected(), IT_MODE_PEER);
        CHK_RDB_RET(rdbk, DCIN_PRESENT_RDB, is_dcin_present(),
                    IT_MODE_BATTERY);
        return 0;

    case IT_MODE_PEER:
        CHK_RDB_RET(rdbk, RDB_BRIDGE_STATUS_RDB, rdb_bridge_synced(),
                     is_poe_connected() ? IT_MODE_NORMAL : IT_MODE_SCAN);
        CHK_RDB_RET(rdbk, OWA_CONNECTED_RDB, !is_owa_connected(),
                    IT_MODE_BATTERY);
        return 0;

    case IT_MODE_NORMAL:
        CHK_RDB_RET(rdbk, OWA_CONNECTED_RDB, !is_owa_connected(),
                    IT_MODE_BATTERY);
        CHK_RDB_RET(rdbk, OWA_ETH_CONNECTED_RDB, !is_poe_connected(),
                    IT_MODE_SCAN);
        CHK_RDB_RET(rdbk, OWA_UPDATE_RDB, is_owa_updating(), IT_MODE_FLASH_OWA);
        return 0;

    case IT_MODE_SCAN:
        CHK_RDB_RET(rdbk, OWA_CONNECTED_RDB, !is_owa_connected(),
                    IT_MODE_BATTERY);
        CHK_RDB_RET(rdbk, OWA_ETH_CONNECTED_RDB, is_poe_connected(),
                    IT_MODE_NORMAL);
        CHK_RDB_RET(rdbk, OWA_UPDATE_RDB, is_owa_updating(),
                    IT_MODE_FLASH_OWA);
        return 0;

    case IT_MODE_FLASH_OWA:
        CHK_RDB_RET(rdbk, OWA_CONNECTED_RDB, !is_owa_connected(),
                    IT_MODE_BATTERY);
        CHK_RDB_RET(rdbk, OWA_UPDATE_RDB, !is_owa_updating(),
                    is_poe_connected() ? IT_MODE_NORMAL : IT_MODE_SCAN);
        return 0;

    case IT_MODE_POWEROFF:
        return 0;
    }

    BLOG_ERR("Invalid IT mode %d\n", it_mode);
    return -1;
}

/*
 * Process RDB trigger events
 *
 * @param triggered_rdbs A string containing triggered RDBs separated by '&'
 * @return Number of mode changes due to the triggers
 */
static int process_triggers(char * triggered_rdbs)
{
    char * rdbk;
    int ret;
    int cnt = 0;
    rdbk = strtok(triggered_rdbs, "&");
    while (rdbk) {
        ret = process_single(rdbk);
        if (ret < 0) {
            BLOG_ERR("Failed to process triggered RDB %s\n", rdbk);
        } else {
            cnt += ret;
        }
        rdbk = strtok(NULL, "&");
    }
    return cnt;
}

static int main_loop(void)
{
    int ret;
    fd_set fdset;
    struct timeval timeout;
    int need_poll = 1;
    int rdb_names_len = INIT_RDB_NAMES_LEN;

    char * triggered_rdbs = malloc(INIT_RDB_NAMES_LEN);
    if (!triggered_rdbs) {
        return -ENOMEM;
    }

    while (!terminate) {
        if (need_poll) {
            /*
             * When mode changed, poll RDBs to pick up missed RDB triggers,
             * due to previous incompatible mode.
             * e.g. When calibration is needed while OWA is connected, it will
             * not go into Calibration mode (you cannot swing Install Tool with
             * OWA connected). But as soon as OWA is disconnected (changing to
             * Battery mode), we need to transit into Calibration mode
             * immediately.
             */
            BLOG_DEBUG("Poll RDBs ...\n");
            ret = process_single(NULL);
            if (ret == 0) {
                BLOG_DEBUG("No mode change. Turn off polling\n");
                need_poll = 0;
            }
        }

        // wait for rdb triggers
        FD_ZERO(&fdset);
        FD_SET(rdbfd, &fdset);
        ms_to_timeval(SELECT_TIMEOUT_MS, &timeout);
        ret = select(rdbfd + 1, &fdset, NULL, NULL, &timeout);
        if (ret < 0) { // error
            int rval = ret;
            if (errno == EINTR) {
                rval = 0;
            }
            BLOG_ERR("select returned %d, errno: %d\n", ret, errno);
            free(triggered_rdbs);
            return rval;
        } else if (ret > 0) { // rdb triggered
            ret = rdb_getnames_alloc(rdb_s, "", &triggered_rdbs, &rdb_names_len,
                                     TRIGGERED);
            if (ret < 0) {
                BLOG_ERR("Failed to get triggered RDBs, ret:%d\n", ret);
                return ret;
            }
            ret = process_triggers(triggered_rdbs);
            if (ret > 0) {
                BLOG_DEBUG("Mode changed. Turn on polling\n");
                need_poll = 1;
            }
        } else { // timeout
            // poll RDBs to pick up missed RDB trigger for whatever reason
            ret = process_single(NULL);
            if (ret > 0) {
                BLOG_DEBUG("Mode changed. Turn on polling\n");
                need_poll = 1;
            }
        }
    }

    free(triggered_rdbs);
    return 0;
}

int main(void)
{
    int ret;

    openlog("modeman", LOG_CONS, LOG_USER);

    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        return -1;
    }
    rdbfd = rdb_fd(rdb_s);
    if (rdbfd < 0) {
        BLOG_ERR("Failed to get rdb_fd\n");
        ret = -1;
        goto fin;
    }

    ret = init();
    if (ret < 0) {
        BLOG_ERR("Failed to init\n");
        goto fin;
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        ret = -errno;
        BLOG_ERR("Failed to register signal handler\n");
        goto fin;
    }

    ret = main_loop();
    if (ret < 0) {
        BLOG_ERR("main loop terminated unexpectedly\n");
    } else {
        BLOG_NOTICE("main loop exited normally\n");
    }

fin:
    rdb_close(&rdb_s);
    closelog();

    return ret;
}
