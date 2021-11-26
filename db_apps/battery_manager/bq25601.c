/**
 * @file bq25601.c
 * @brief battery charger implementation using TI bq25601
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

#include "bq2560x_reg.h"
#include "charger.h"
#include "i2c_charger_common.h"

#define DEV_ADDR 0x6b

static struct i2c_handle handle;

int charger_kick_wdog(void)
{
    CHARGER_UPDATE_BITS(BQ2560X_REG_01,
                        REG01_WDT_RESET_MASK,
                        REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
    return 0;
}

static int set_charge_param(void)
{
    uint8_t val;
    // voltage - VREG 4.208V
    val = (4208 - 3856) / 32;
    CHARGER_UPDATE_BITS(BQ2560X_REG_04, REG04_VREG_MASK,
                        val << REG04_VREG_SHIFT);

    // recharge threshold - VRECHG 200mV
    val = 200 / 200;
    CHARGER_UPDATE_BITS(BQ2560X_REG_04, REG04_VRECHG_MASK,
                        val << REG04_VRECHG_SHIFT);

    // terminate current - ITERM 240mA
    val = 240 / 60;
    CHARGER_UPDATE_BITS(BQ2560X_REG_03, REG03_ITERM_MASK,
                        val << REG03_ITERM_SHIFT);

    // charge current - ICHG 1920mA
    val = 1920 / 60;
    CHARGER_UPDATE_BITS(BQ2560X_REG_02, REG02_ICHG_MASK,
                        val << REG02_ICHG_SHIFT);

    // input current limit - IINDPM 2000mA
    val = 2000 / 100 - 1;
    CHARGER_UPDATE_BITS(BQ2560X_REG_00, REG00_IINLIM_MASK,
                        val << REG00_IINLIM_SHIFT);

    // charge safety timer - 5 hours
    val = REG05_CHG_TIMER_5HOURS;
    CHARGER_UPDATE_BITS(BQ2560X_REG_05, REG05_CHG_TIMER_MASK,
                        val << REG05_CHG_TIMER_SHIFT);

    // 50% percent charge when temperature is low.
    val = REG05_JEITA_ISET_50PCT;
    CHARGER_UPDATE_BITS(BQ2560X_REG_05, REG05_JEITA_ISET_MASK,
                        val << REG05_JEITA_ISET_SHIFT);

    return 0;
}

int enable_charge(void)
{
    int ret;

    // disable HiZ status
    CHARGER_UPDATE_BITS(BQ2560X_REG_00,
                        REG00_ENHIZ_MASK,
                        REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT);
    // enable watchdog and set in default 40s
    CHARGER_UPDATE_BITS(BQ2560X_REG_05,
                        REG05_WDT_MASK,
                        REG05_WDT_40S << REG05_WDT_SHIFT);
    ret = set_charge_param();
    if (ret) {
        BLOG_ERR("Failed to set charge parameters\n");
        return ret;
    }

    CHARGER_UPDATE_BITS(BQ2560X_REG_01,
                        REG01_CHG_CONFIG_MASK,
                        REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT);

    return 0;
}

int disable_charge(void)
{
    CHARGER_UPDATE_BITS(BQ2560X_REG_01,
                        REG01_CHG_CONFIG_MASK,
                        REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT);
    return 0;
}

int charger_get_stats(struct charge_stats * stats)
{
    int ret;
    uint8_t val;

    CHARGER_READ(BQ2560X_REG_08, ret);

    // REG08_VBUS_TYPE_NONE   0
    // REG08_VBUS_TYPE_USB       1
    // REG08_VBUS_TYPE_ADAPTER   3
    // REG08_VBUS_TYPE_OTG 7
    val = (ret & REG08_VBUS_STAT_MASK) >> REG08_VBUS_STAT_SHIFT;
    switch (val) {
    case REG08_VBUS_TYPE_NONE:
        stats->vbus_stat = VBUS_STAT_NONE;
        break;
    case REG08_VBUS_TYPE_USB:
        stats->vbus_stat = VBUS_STAT_USB;
        break;
    case REG08_VBUS_TYPE_ADAPTER:
        stats->vbus_stat = VBUS_STAT_ADAPTER;
        break;
    case REG08_VBUS_TYPE_OTG:
        stats->vbus_stat = VBUS_STAT_OTG;
        break;
    default:
        BLOG_ERR("Illegal vbus stat %d\n", val);
        return -1;
    }

    // REG08_CHRG_STAT_IDLE      0
    // REG08_CHRG_STAT_PRECHG    1
    // REG08_CHRG_STAT_FASTCHG   2
    // REG08_CHRG_STAT_CHGDONE   3
    val = (ret & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;
    switch (val) {
    case REG08_CHRG_STAT_IDLE:
        stats->charge_stat = CHARGE_STAT_IDLE;
        break;
    case REG08_CHRG_STAT_PRECHG:
        stats->charge_stat = CHARGE_STAT_PRECHG;
        break;
    case REG08_CHRG_STAT_FASTCHG:
        stats->charge_stat = CHARGE_STAT_FASTCHG;
        break;
    case REG08_CHRG_STAT_CHGDONE:
        stats->charge_stat = CHARGE_STAT_CHGDONE;
        break;
    default:
        BLOG_ERR("Illegal charge stat %d\n", val);
        return -1;
    }

    val = (ret & REG08_PG_STAT_MASK) >> REG08_PG_STAT_SHIFT;
    stats->power_good = (val == REG08_POWER_GOOD);

    CHARGER_READ(BQ2560X_REG_09, ret);
    val = (ret & REG09_FAULT_NTC_MASK) >> REG09_FAULT_NTC_SHIFT;
    switch (val) {
    case REG09_FAULT_NTC_NORMAL:
        stats->fault_ntc = FAULT_NTC_NORMAL;
        break;
    case REG09_FAULT_NTC_WARM:
        stats->fault_ntc = FAULT_NTC_WARM;
        break;
    case REG09_FAULT_NTC_COOL:
        stats->fault_ntc = FAULT_NTC_COOL;
        break;
    case REG09_FAULT_NTC_COLD:
        stats->fault_ntc = FAULT_NTC_COLD;
        break;
    case REG09_FAULT_NTC_HOT:
        stats->fault_ntc = FAULT_NTC_HOT;
        break;
    default:
        BLOG_ERR("Illegal fault ntc %d\n", val);
        return -1;
    }

    return 0;
}

int is_charger_connected(void)
{
    int ret;
    CHARGER_READ(BQ2560X_REG_0A, ret);
    return (ret & REG0A_VBUS_GD_MASK) >> REG0A_VBUS_GD_SHIFT;
    /*
      Using PG_STAT is better.
      VBUS_GD is set after poor source qualification.
      After VBUS_GD is set and REGN LDO is powered, the IC runs input source
      detection and set IINDPM, PG_STAT and VBUS_STAT. After that, host can
      overwrite these registers, particularly IINDPM.
    */
}

int charger_init(void)
{
    int ret;
    ret = i2c_open(&handle, CHARGER_I2C_BUSNO, DEV_ADDR);
    if (ret < 0) {
        BLOG_ERR("Failed to open i2c: bus=%d, addr=%02x\n", CHARGER_I2C_BUSNO, DEV_ADDR);
        return ret;
    }

    return charger_kick_wdog();
}

void charger_term(void)
{
    i2c_close(&handle);
}

int poweroff(void)
{
    // disable watchdog
    CHARGER_UPDATE_BITS(BQ2560X_REG_05,
                        REG05_WDT_MASK,
                        REG05_WDT_DISABLE << REG05_WDT_SHIFT);
    // disable BATFET
    CHARGER_UPDATE_BITS(BQ2560X_REG_07,
                        REG07_BATFET_DIS_MASK,
                        REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT);
    // enable HiZ
    CHARGER_UPDATE_BITS(BQ2560X_REG_00,
                        REG00_ENHIZ_MASK,
                        REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
    return 0;
}
