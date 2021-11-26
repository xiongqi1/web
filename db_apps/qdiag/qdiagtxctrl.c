/*!
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems Inc.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa System Inc.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY Casa Systems Inc ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

/* local headers */
#include "logcodes.h"

/* qualcomm headers */
#include <diag/diag_lsm.h>
#include <diag/diag_lsm_dci.h>
#include <diag/event_defs.h>

/* standard headers */
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* local headers */
#include "dbenum.h"
#include "def.h"
#include "rdb_ops.h"

#define LOG_MAX_NAME_LEN 20
#define RDB_MAX_VAL_LEN 1024
#define RDB_MAX_NAME_LEN 256
#define SBUCKETS 10
#define FIVE_MIN 300
#define FIVE_INDEX 10
#define ONE_HOUR 3600

// note that TIMESTAMPS is 19 not 20 because the last of the seconds is tracked in the accumulator
#define TIMESTAMPS 19

typedef struct tagLogMsgTimeCost {
    uint16 iLogCode;
    char strMsgName[LOG_MAX_NAME_LEN];
    char strRdbName[RDB_MAX_NAME_LEN];
    int iTimeCost;  // in usec
    uint32 iMsgCnt; // the message number from last enable to disable
} T_LogMsgTimeCost;

/* These are the diag messages we will register for and the worst-case tx time represented by each.
 * Note that the code assumes that the first can only represent a single real transmission
 * but that the remaining have a common header with the transmision count.
 * I.e. ordering is important
 */
static T_LogMsgTimeCost sgatLogMsgTimeCost[] = {
    { 0xb144, "RACH", "wwan.0.txctrl.rach.timecost", 2284, 0 },
    { 0xb139, "PUSCH", "wwan.0.txctrl.pusch.timecost", 1000, 0 },
    { 0xb13c, "PUCCH", "wwan.0.txctrl.pucch.timecost", 1000, 0 },
    { 0xb140, "SRS", "wwan.0.txctrl.srs.timecost", 83, 0 },
};

/* rdb session handle */
static struct rdb_session *rdbSession = NULL;

/* the related rdbs for tx control */
static char *sgstrRdbTxState = "wwan.0.txctrl.tx_state";
static char *sgstrRdbTxStateChangeTime = "wwan.0.txctrl.tx_state_chg.time";
static char *sgstrRdbNextTxTime = "wwan.0.txctrl.next_tx.time";

static char *sgstrRdbTxTimeThresholdPer10s = "wwan.0.txctrl.txtimeper10s"; /* sets sgiTxTimeThresholdPer10s */
static char *sgstrRdbMinOff = "wwan.0.txctrl.min_off";                     /* sets sgimin_off_secs */
static char *sgstrRdbSasTxStatus = "sas.transmit_enabled";

/* keep the tx threshold for 10s in us*/
static int sgiTxTimeThresholdPer10s = 900000; // = 0.9s, but configurable
static int sgimin_off_secs = 180;             // = 3 min, but configurable

/* these must not be used in a different thread */

/* these starting with m_ need to be protected by the mutex that follows */
static int m_tx_accumulator_us = 0;   // the tx time accumulated, subtracts 1s at a time
static int m_accum_us_10s_ago_p1 = 0; // the value tx_accum[10 seconds previously] + 1 second
static int m_larger_window_wait = 0;  // amount of time to wait if the next full second would exceed 5 or 60 min window; 0 won't shutdown tx at all
static int m_tx_off_for = 0;          // set by thread to number of secs to turn off

/* protection between the main thread and the callback thread */
static pthread_mutex_t lock;

/* Qualcomm diag structure - mandatory variables for Qualcomm diag */
struct diag_stat_t {
    /* initiate status */
    int flag_init;
    int flag_reg;
    int flag_sig_reg;
    int flag_proc_reg;
    int flag_logapp;
    int flag_eventapp;

    /* diag constant */
    int channel;
    int noti_signal;
    int data_signal;

    int client_id;
};

static struct diag_stat_t diag_stat; /* Qualcomm diag initiation status */
static int sgiDisableMsgCnt = 0;

/*=========================================================================
 Write a value to RDB
 Params:
  rdb : rdb to write.
  val : value to write the rdb variable.
 Return:
  0 = success. Otherwise, failure.
 Note:
  This function creates RDB variable when the value does not exist.
============================================================================*/
int rdbSetValue(const char *rdb, const char *val)
{
    int stat;

    if (!rdbSession)
        return -1;

    /* use blank string if val is NULL */
    if (!val)
        val = "";

    /* set rdb */
    if ((stat = rdb_set_string(rdbSession, rdb, val)) < 0) {
        if (stat == -ENOENT) {
            stat = rdb_create_string(rdbSession, rdb, val, 0, 0);
        }
    }

    /* log error message if it failed */
    if (stat < 0)
        ERR("[RDB] failed to set RDB (rdb='%s',stat=%d,str='%s')", rdb, stat, strerror(errno));
    else
        DEBUG("[RDB] write '%s' ==> [%s]", rdb, val);

    return stat;
}

static void enable_modem_tx(int flag, int iNextTxWaitTime)
{
    // set the rdb, trigger the template to enable/disable the radio transmission
    time_t curr_time = time(NULL);
    struct tm curr_tm = *localtime(&curr_time);
    char strCurrTime[100] = { 0 };
    sprintf(strCurrTime, "%d-%02d-%02d %02d:%02d:%02d\n", curr_tm.tm_year + 1900, curr_tm.tm_mon + 1, curr_tm.tm_mday, curr_tm.tm_hour,
            curr_tm.tm_min, curr_tm.tm_sec);
    rdbSetValue(sgstrRdbTxStateChangeTime, strCurrTime);
    if (flag) {
        NOTICE("qdiagtxctrl:enable the transmission at %s\n", strCurrTime);
        rdbSetValue(sgstrRdbTxState, "1");
    } else {
        NOTICE("qdiagtxctrl:disable the transmission at %s\n", strCurrTime);
        rdbSetValue(sgstrRdbTxState, "0");
        if (iNextTxWaitTime < sgimin_off_secs)
            iNextTxWaitTime = sgimin_off_secs;
        if (m_tx_off_for < iNextTxWaitTime)
            m_tx_off_for = iNextTxWaitTime;
        // indicate the time of next transmission
        curr_tm.tm_sec += m_tx_off_for;
        time_t next_time = mktime(&curr_tm);
        struct tm next_tm = *localtime(&next_time);
        char strNextTxTime[100] = { 0 };
        sprintf(strNextTxTime, "%d-%02d-%02d %02d:%02d:%02d\n", next_tm.tm_year + 1900, next_tm.tm_mon + 1, next_tm.tm_mday, next_tm.tm_hour,
                next_tm.tm_min, next_tm.tm_sec);
        rdbSetValue(sgstrRdbNextTxTime, strNextTxTime);
        NOTICE("qdiagtxctrl:next transmission at %s\n", strNextTxTime);
        NOTICE("qdiagtxctrl: msg number cnt, RACH:%d, PUSCH:%d, PUCCH:%d, SRS:%d \n", sgatLogMsgTimeCost[0].iMsgCnt, sgatLogMsgTimeCost[1].iMsgCnt,
               sgatLogMsgTimeCost[2].iMsgCnt, sgatLogMsgTimeCost[3].iMsgCnt);
    }
}

/* Called once for each diag (QXDM) log message. This could be high-rate so keep it small! */
static void process_dci_log_stream(unsigned char *ptr, int len)
{
    int iLoop = 0;
    uint16 logCode = *(uint16 *)(ptr + 2);
    uint16 dwEntryNum = 1;

    /* disabled the message counting */
    if (sgiDisableMsgCnt != 0)
        return;

    // search for the matching log code
    for (iLoop = sizeof(sgatLogMsgTimeCost) / sizeof(sgatLogMsgTimeCost[0]) - 1; iLoop >= 0; iLoop--) {
        if (logCode == sgatLogMsgTimeCost[iLoop].iLogCode)
            break;
    }
    if (iLoop < 0) {
        ERR("Received diag message we shouldn't have: %x", logCode);
        return;
    }

    // RACH only has one entry, others may be multiple in one message
    if (iLoop > 0) {
        lte_LL1_log_ul_pusch_tx_report_ind_struct *ptPayLoadHead = (lte_LL1_log_ul_pusch_tx_report_ind_struct *)(ptr + 12);
        dwEntryNum = ptPayLoadHead->number_of_records;
    }

    pthread_mutex_lock(&lock);
    m_tx_accumulator_us += sgatLogMsgTimeCost[iLoop].iTimeCost * dwEntryNum;
    sgatLogMsgTimeCost[iLoop].iMsgCnt += dwEntryNum;

    if (m_tx_accumulator_us >= m_accum_us_10s_ago_p1) {
        enable_modem_tx(0, m_larger_window_wait);
        /* setting this var helps rate limit when it takes a short time for the modem
         * to stop transmitting (so we keep getting logs). m_accum_us_10s_ago_p1 will be
         * reset in the main loop anyway but if for some reason the modem doesn't turn
         * off we will try it again every 50ms of transmit)
         */
        m_accum_us_10s_ago_p1 = m_tx_accumulator_us + 50000;
    }
    if (m_larger_window_wait && (m_tx_accumulator_us >= sgiTxTimeThresholdPer10s)) {
        enable_modem_tx(0, m_larger_window_wait);
        m_larger_window_wait = 0; // another rate limit
    }

    pthread_mutex_unlock(&lock);
}

static void process_dci_event_stream(unsigned char *ptr, int len)
{
    // keep it as empty, we do not care about the event here
}

/* Signal handler that handles the change in DCI channel */
static void notify_handler(int signal, siginfo_t *info, void *unused)
{
    (void)unused;

    if (info) {
        int err;
        diag_dci_peripherals list = 0;

        VERBOSE("diag: In %s, signal %d received from kernel, data is: %x\n", __func__, signal, info->si_int);

        if (info->si_int & DIAG_STATUS_OPEN) {
            if (info->si_int & DIAG_CON_MPSS) {
                VERBOSE("diag: DIAG_STATUS_OPEN on DIAG_CON_MPSS\n");
            } else if (info->si_int & DIAG_CON_LPASS) {
                VERBOSE("diag: DIAG_STATUS_OPEN on DIAG_CON_LPASS\n");
            } else {
                VERBOSE("diag: DIAG_STATUS_OPEN on unknown peripheral\n");
            }
        } else if (info->si_int & DIAG_STATUS_CLOSED) {
            if (info->si_int & DIAG_CON_MPSS) {
                VERBOSE("diag: DIAG_STATUS_CLOSED on DIAG_CON_MPSS\n");
            } else if (info->si_int & DIAG_CON_LPASS) {
                VERBOSE("diag: DIAG_STATUS_CLOSED on DIAG_CON_LPASS\n");
            } else {
                VERBOSE("diag: DIAG_STATUS_CLOSED on unknown peripheral\n");
            }
        }
        err = diag_get_dci_support_list_proc(MSM, &list);
        if (err != DIAG_DCI_NO_ERROR) {
            VERBOSE("diag: could not get support list, err: %d\n", err);
        }
        /* This will print out all peripherals supporting DCI */
        if (list & DIAG_CON_MPSS) {
            VERBOSE("diag: Modem supports DCI\n");
        }
        if (list & DIAG_CON_LPASS) {
            VERBOSE("diag: LPASS supports DCI\n");
        }
        if (list & DIAG_CON_WCNSS) {
            VERBOSE("diag: RIVA supports DCI\n");
        }
        if (list & DIAG_CON_APSS) {
            VERBOSE("diag: APSS supports DCI\n");
        }
        if (!list) {
            VERBOSE("diag: No current dci support\n");
        }
    } else {
        VERBOSE("diag: In %s, signal %d received from kernel, but no info value, info: 0x%p\n", __func__, signal, info);
    }
}

/* Signal Handler that will be fired when we receive DCI data */
static void dci_data_handler(int signal)
{
    (void)signal;

    /* Do something here when you receive DCI data. */

    /* This is usually for holding wakelocks when the
      clients are running in Diag Non Real Time mode
      or when they know the Apps processor is in deep
      sleep but they still need to process DCI data.

      Please Note: Wakelocks must be released
      after processing the data in the respective
      response/log/event handler. Failure to do so
      will affect the power consumption of the Apps
      processor.*/

    VERBOSE("got signal");
}

static void _diag_fini()
{
    struct sigaction notify_action;
    struct sigaction dci_data_action;

    if (diag_stat.flag_eventapp) {
        INFO("disable event");
        diag_disable_all_events(diag_stat.client_id);
    }

    if (diag_stat.flag_logapp) {
        INFO("disable log");
        diag_disable_all_logs(diag_stat.client_id);
    }

    /* This delay is essentially required as Modem could take time for its own finalization */
    sleep(5);

    if (diag_stat.flag_sig_reg) {
        INFO("deregister data signal");
        diag_deregister_dci_signal_data(diag_stat.client_id);
    }

    if (diag_stat.flag_reg) {
        INFO("release dci client");
        diag_release_dci_client(&diag_stat.client_id);
    }

    if (diag_stat.flag_init) {
        INFO("deinit diag lsm");
        Diag_LSM_DeInit();
    }

    INFO("restore notify signal handler");
    sigemptyset(&notify_action.sa_mask);
    dci_data_action.sa_handler = SIG_DFL;
    sigaction(diag_stat.noti_signal, &notify_action, NULL);

    INFO("restore data signal handler");
    sigemptyset(&dci_data_action.sa_mask);
    dci_data_action.sa_handler = SIG_DFL;
    sigaction(diag_stat.data_signal, &dci_data_action, NULL);
}

static int _diag_init()
{
    int res;
    diag_dci_peripherals list = DIAG_CON_MPSS | DIAG_CON_APSS | DIAG_CON_LPASS;
    struct sigaction notify_action;
    struct sigaction dci_data_action;
    uint16 *log_codes_array = NULL;
    int logCodeNum = 0;
    int index = 0;

    /* init diag */
    INFO("initiate diag struct \n");
    memset(&diag_stat, 0, sizeof(diag_stat));
    diag_stat.channel = MSM;
    diag_stat.noti_signal = SIGCONT;
    diag_stat.data_signal = SIGRTMIN + 15;

    INFO("initiate notify signal handler \n");
    sigemptyset(&notify_action.sa_mask);
    notify_action.sa_sigaction = notify_handler;
    notify_action.sa_flags = SA_SIGINFO;
    sigaction(diag_stat.noti_signal, &notify_action, NULL);

    INFO("initiate data signal handler \n");
    sigemptyset(&dci_data_action.sa_mask);
    dci_data_action.sa_handler = dci_data_handler;
    dci_data_action.sa_flags = 0;
    sigaction(diag_stat.data_signal, &dci_data_action, NULL);

    INFO("initiate libdiag \n");
    if (!Diag_LSM_Init(NULL)) {
        ERR("failed to initiate libdiag \n");
        goto err;
    }
    diag_stat.flag_init = 1;

    INFO("register with DCI \n");
    res = diag_register_dci_client(&diag_stat.client_id, &list, diag_stat.channel, &diag_stat.noti_signal);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to register with DCI (res=%d,errno=%d,str='%s') \n", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_reg = 1;
    INFO("DCI registered (client_id=%d) \n", diag_stat.client_id);

    INFO("register signal");
    res = diag_register_dci_signal_data(diag_stat.client_id, diag_stat.data_signal);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to register signal (err=%d,errno=%d,str='%s') \n", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_sig_reg = 1;

    /* Getting supported Peripherals list*/
    int err;
    static int channel = MSM;

    INFO("* DCI Status on Processors: \n");
    err = diag_get_dci_support_list_proc(channel, &list);
    if (err != DIAG_DCI_NO_ERROR) {
        ERR("could not get support list, err: %d, errno: %d \n", err, errno);
        goto err;
    }
    INFO("MPSS:  %s\n", (list & DIAG_CON_MPSS) ? "UP" : "DOWN");
    INFO("LPASS: %s\n", (list & DIAG_CON_LPASS) ? "UP" : "DOWN");

    INFO("WCNSS: %s\n", (list & DIAG_CON_WCNSS) ? "UP" : "DOWN");
    INFO("APSS:  %s\n", (list & DIAG_CON_APSS) ? "UP" : "DOWN");

    INFO("register stream proc\n");
    res = diag_register_dci_stream_proc(diag_stat.client_id, process_dci_log_stream, process_dci_event_stream);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to register stream proc (err=%d,errno=%d,str='%s')\n", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_proc_reg = 1;

    // register the log message codes
    logCodeNum = sizeof(sgatLogMsgTimeCost) / sizeof(sgatLogMsgTimeCost[0]);
    log_codes_array = (uint16 *)malloc(sizeof(uint16) * logCodeNum);
    for (index = 0; index < logCodeNum; index++) {
        log_codes_array[index] = sgatLogMsgTimeCost[index].iLogCode;
    }
    res = diag_log_stream_config(diag_stat.client_id, ENABLE, log_codes_array, logCodeNum);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to enable log stream proc (err=%d,errno=%d,str='%s') \n", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_logapp = 1;

    return 0;
err:
    _diag_fini();
    return -1;
}

/* get the configuration time cost for each kind of message */
void readTimeCost()
{
    int stat = 0;
    int index = 0;
    int iValueLen = RDB_MAX_VAL_LEN + 1;

    /*read the rdb values */
    char strValue[RDB_MAX_VAL_LEN + 1];
    for (index = 0; index < sizeof(sgatLogMsgTimeCost) / sizeof(T_LogMsgTimeCost); index++) {
        memset(strValue, 0, RDB_MAX_VAL_LEN + 1);
        iValueLen = RDB_MAX_VAL_LEN + 1;
        if ((stat = rdb_get(rdbSession, sgatLogMsgTimeCost[index].strRdbName, strValue, &iValueLen)) < 0) {
            ERR("[RDB] failed to get RDB (rdb='%s',stat=%d,str='%s')", sgatLogMsgTimeCost[index].strRdbName, stat, strerror(errno));
        } else {
            sgatLogMsgTimeCost[index].iTimeCost = atoll(strValue);
            INFO("get the rdb:%s, its value=%d", sgatLogMsgTimeCost[index].strRdbName, sgatLogMsgTimeCost[index].iTimeCost);
        }
    }
}

void check_signal(int signum)
{
    switch (signum) {
        case SIGTERM:
        case SIGKILL:
        case SIGINT: {
            NOTICE("qdiagtxctrl got signal (%d)", signum);
            _diag_fini();
            exit(0);
        } break;

        case SIGHUP: {
            // nothing todo at moment
        } break;

        case SIGUSR1: {
            // nothing todo at moment
        } break;

        default: {
            // nothing todo at moment
        } break;
    }
}

static void terminate()
{
    _diag_fini();
}

/*
 * check if SAS transmission is allowed if IA is configuring
 * @return 1 if TX is allowed or if configuration is on-going
 */
static int isTxAllowedOrIAConfig(int *txAllowed)
{
    char strValue[RDB_MAX_VAL_LEN + 1] = { 0 };
    int iTransmitAllowed = 0;

    /* transmission is allowed if sas.transmit_enabled is 1 */
    int iValueLen = sizeof(strValue);
    int stat = rdb_get(rdbSession, sgstrRdbSasTxStatus, strValue, &iValueLen);
    if (stat < 0) {
        ERR("failed to get RDB (rdb='%s',stat=%d,str='%s')", sgstrRdbSasTxStatus, stat, strerror(errno));
        return iTransmitAllowed;
    }

    iTransmitAllowed = atoll(strValue);
    if (txAllowed) {
      *txAllowed = iTransmitAllowed;
    }

    if (iTransmitAllowed != 1) {
        // Check if installation assistant configuring is on-going
        int iNitConnected = 0;
        iValueLen = sizeof(strValue);
        stat = rdb_get(rdbSession, "nit.connected", strValue, &iValueLen);
        if (stat >= 0) {
            iNitConnected = atoll(strValue);
        }
        if (iNitConnected == 1) {
            iValueLen = sizeof(strValue);
            stat = rdb_get(rdbSession, "installation.state", strValue, &iValueLen);
            if (stat >= 0) {
                return strcmp(strValue, "normal_operation") != 0;
            }
        }
    }

    return iTransmitAllowed;
}

int main(int argc, char *argv[])
{
    int iValueLen = RDB_MAX_VAL_LEN + 1;
    char strValue[RDB_MAX_VAL_LEN + 1] = { 0 };
    int stat = 0;
    int i;

    int sgaiTxTimeBucket[SBUCKETS] = { 0 }; // record the tx time of the last 10 time slots
    int sgiCurrTimeSlot = 0;                // index into that where next item is saved
    int tx_acc_last_bucket = 0;             // the value of previous at time it was last put in a bucket; can be negative
    int sum_of_buckets = 0;
    uint32 second_ts[TIMESTAMPS] = { 0 }; // record the clock at each time 1s of tx was used
    uint32 current_time = 1;
    uint32 tx_off_until = 0;
    int proposed_wait;
    int iCheckSasRdbTimes = 0;

#ifdef USE_TIMERFD
    struct timespec tv;
#endif

    INFO("initiate RDB");
    if (rdb_open(NULL, &rdbSession) < 0) {
        ERR("cannot open Netcomm RDB (errno=%d,str='%s')", errno, strerror(errno));
        return -1;
    }

    /* read the rdb config to get the threshold for 10s */
    if ((stat = rdb_get(rdbSession, sgstrRdbTxTimeThresholdPer10s, strValue, &iValueLen)) < 0) {
        ERR("failed to get RDB (rdb='%s',stat=%d,str='%s')", sgstrRdbTxTimeThresholdPer10s, stat, strerror(errno));
    } else {
        sgiTxTimeThresholdPer10s = atoll(strValue);
        INFO("get the rdb:%s, its value=%d", sgstrRdbTxTimeThresholdPer10s, sgiTxTimeThresholdPer10s);
    }

    /* read the rdb config to get the threshold for 10s */
    iValueLen = RDB_MAX_VAL_LEN + 1;
    if ((stat = rdb_get(rdbSession, sgstrRdbMinOff, strValue, &iValueLen)) < 0) {
        ERR("failed to get RDB (rdb='%s',stat=%d,str='%s')", sgstrRdbMinOff, stat, strerror(errno));
    } else {
        sgimin_off_secs = atoll(strValue);
        INFO("get the rdb:%s, its value=%d", sgstrRdbMinOff, sgimin_off_secs);
    }

    /* read the rdb to get the time cost for related burst */
    readTimeCost();

    atexit(terminate);
    if (signal(SIGINT, check_signal) == SIG_ERR)
        ERR("qdiagtxctrl, can't catch SIGINT");

    if (signal(SIGTERM, check_signal) == SIG_ERR)
        ERR("qdiagtxctrl, can't catch SIGTERM");

    INFO("initiate diag \n");
    if (_diag_init() < 0) {
        ERR("qdaigtxctrl, failed to initiate diag \n");
        return -1;
    }

    if (pthread_mutex_init(&lock, NULL) != 0) {
        ERR("failed to init the mutex \n");
        return -1;
    }

    NOTICE("Initialised with threshold %f and min off = %d", sgiTxTimeThresholdPer10s / 1.0E6, sgimin_off_secs);
    m_accum_us_10s_ago_p1 = sgiTxTimeThresholdPer10s;
    enable_modem_tx(1, 0);

    /* read the sas tx status. In case the tx ctrl was started before sas client and the rdb is not ready,
     * we can give it a try after waiting some time. Here we suppose that 20 seconds should be enough.
     */
    iValueLen = RDB_MAX_VAL_LEN + 1;
    while (rdb_get(rdbSession, sgstrRdbSasTxStatus, strValue, &iValueLen) < 0) {
        if (++iCheckSasRdbTimes == 10) {
            ERR("failed to read rdb:%s after 20s\n", sgstrRdbSasTxStatus);
            return -1;
        }
        iValueLen = RDB_MAX_VAL_LEN + 1;
        sleep(2);
    }

    while (1) {
#ifdef USE_TIMERFD
        /* It may be better to replace the integer counter with a proper clock in the future,
         * to deal better with potential skew or unexpected signals.
         * This is how it could be done; timerfd_create()/read() is an alternative to sleep();
         * Reason it's not default: not tested and would need caution around time_t overflow
         */
        if (clock_gettime(CLOCK_MONOTONIC, &tv))
            bail;
        current_time = tv.tv_sec;
#else
        sleep(1);
        current_time++;
#endif

        // disable message counting when SAS transmission is allowed or the unit is being configured
        int txAllowed = 0;
        sgiDisableMsgCnt = isTxAllowedOrIAConfig(&txAllowed);
        static int lastDisableMsgCnt = -1;
        if (sgiDisableMsgCnt != lastDisableMsgCnt) {
            NOTICE("txctrl sec:%u, %sable message counting", current_time, sgiDisableMsgCnt ? "Dis" : "En");
            lastDisableMsgCnt = sgiDisableMsgCnt;
        }
        if (sgiDisableMsgCnt) {
            if (txAllowed && tx_off_until != 0) {
              NOTICE("txctrl sec:%u, OffUntil:%u, SAS TX -> allowed.", current_time, tx_off_until);
              enable_modem_tx(1, 0);
              tx_off_until = 0;
            }
            continue;
        }

        // protects m_* vars from diag thread
        pthread_mutex_lock(&lock);

        // have we used a whole second since last time we checked?
        if (m_tx_accumulator_us > sgiTxTimeThresholdPer10s) {
            // shift them all down and save the current time
            for (i = 0; i < TIMESTAMPS - 1; i++) {
                second_ts[i] = second_ts[i + 1];
                INFO("TS %d = %d", i, second_ts[i]);
            }
            second_ts[TIMESTAMPS - 1] = current_time;

            // have to change these two by the same amount so that the difference is the same
            m_tx_accumulator_us -= sgiTxTimeThresholdPer10s;
            tx_acc_last_bucket -= sgiTxTimeThresholdPer10s;
        }

        proposed_wait = 0;
        /* Note: the entry used for the condition is one less than the entry used
         * for the wait calculation. This gives hysteresis.
         * The largest window is always position 0 and determined by TIMESTAMPS
         */
        if (second_ts[0] && ((current_time - second_ts[0]) < ONE_HOUR)) {
            i = second_ts[1] + ONE_HOUR - current_time;
            if (i > proposed_wait)
                proposed_wait = i;
            if (i > m_larger_window_wait)
                NOTICE("Hour window is nearly full");
        }
        if (second_ts[FIVE_INDEX] && ((current_time - second_ts[FIVE_INDEX]) < FIVE_MIN)) {
            i = second_ts[FIVE_INDEX + 1] + FIVE_MIN - current_time;
            if (i > proposed_wait)
                proposed_wait = i;
            if (i > m_larger_window_wait)
                NOTICE("5-min window is nearly full");
        }
        if (proposed_wait < m_larger_window_wait - 1) {
            NOTICE("larger_window_wait %d -> %d", m_larger_window_wait, proposed_wait);
        }
        m_larger_window_wait = proposed_wait;

        /* the bucket is used to avoid the following situation:
            Transmitted for 0.8s at the end of the previous 10s time slot and 0.8s at the start of the next 10s time slot,
            although in each 10s slot, the tx time less 1s, but it break the rule: transmission less then 1s at any 10s.
            The array of the buckets traced all the tx time length during the last 10s;
            Each bucket kept the tx time length of the 1s time slot.
        */
        sum_of_buckets -= sgaiTxTimeBucket[sgiCurrTimeSlot];
        sgaiTxTimeBucket[sgiCurrTimeSlot] = m_tx_accumulator_us - tx_acc_last_bucket;
        tx_acc_last_bucket = m_tx_accumulator_us;
        sum_of_buckets += sgaiTxTimeBucket[sgiCurrTimeSlot];
        sgiCurrTimeSlot = (sgiCurrTimeSlot + 1) % 10;

        /* m_tx_accumulator_us drops 1s at a time above so we have to "pretend" that it is monotonic
         * by subtracting the amount currently in the buckets. By adding the threshold to that we
         * can compare directly against the accumulator in process_dci_log_stream().
         * We skip summing the array by keeping sum_of_buckets up to date as the array changes
         */
        m_accum_us_10s_ago_p1 = m_tx_accumulator_us - sum_of_buckets + sgiTxTimeThresholdPer10s;

        if (m_tx_off_for && m_tx_off_for + current_time > tx_off_until) {
            tx_off_until = m_tx_off_for + current_time;
            m_tx_off_for = 0;
        }

        pthread_mutex_unlock(&lock);

        INFO("Second %u: OffUntil %u. Accum %dus. Buckets %.3fs. NextWait %d", current_time, tx_off_until, tx_acc_last_bucket, sum_of_buckets / 1.0E6,
             m_larger_window_wait);
        if (tx_off_until && tx_off_until <= current_time) {
            /* restart the transmission */
            enable_modem_tx(1, 0);
            tx_off_until = 0;
        }
    }

    if (rdbSession)
        rdb_close(&rdbSession);

    pthread_mutex_destroy(&lock);
    return 0;
}
