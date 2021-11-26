/**
 * @file charger.h
 * @brief common definitions for battery charger IC
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CHARGER_H_09000011032019
#define CHARGER_H_09000011032019

#include <stdint.h>

enum charge_stat {
    CHARGE_STAT_UNKNOWN,
    CHARGE_STAT_IDLE,
    CHARGE_STAT_PRECHG,
    CHARGE_STAT_FASTCHG,
    CHARGE_STAT_CHGDONE,
    CHARGE_STAT_FAULT,
};

enum vbus_stat {
    VBUS_STAT_UNKNOWN,
    VBUS_STAT_NONE,
    VBUS_STAT_USB,
    VBUS_STAT_ADAPTER,
    VBUS_STAT_OTG,
};

enum fault_ntc {
    FAULT_NTC_NORMAL,
    FAULT_NTC_WARM,
    FAULT_NTC_COOL,
    FAULT_NTC_COLD,
    FAULT_NTC_HOT
};

struct charge_stats {
    enum vbus_stat vbus_stat;
    enum charge_stat charge_stat;
    uint8_t power_good;
    enum fault_ntc fault_ntc;
};

/*
 * Kick the charger watchdog to keep it in host mode
 *
 * @return 0 on success; negative error code on failure
 */
int charger_kick_wdog(void);

/*
 * initialise charger IC chip
 *
 * @return 0 on success; negative error code on error
 */
int charger_init(void);

/*
 * terminate/cleanup charger IC resource
 */
void charger_term(void);

/*
 * check if charger is connected
 *
 * @return 1 if charger is connected; 0 otherwise
 */
int is_charger_connected(void);

/*
 * read all battery charging stats from charger IC
 *
 * @param stats A charge_stats struct to store the reading
 * @return 0 on success; negative error code on failure
 */
int charger_get_stats(struct charge_stats * stats);

/*
 * enable charging
 *
 * @return 0 on success; negative error code on failure
 */
int enable_charge(void);

/*
 * disable charging
 *
 * @return 0 on success; negative error code on failure
 */
int disable_charge(void);

/*
 * power off the system
 *
 * @return 0 on success; negative error code on failure
 */
int poweroff(void);

#endif // CHARGER_H_09000011032019
