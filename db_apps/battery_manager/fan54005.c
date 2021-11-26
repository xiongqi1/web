/**
 * @file fan54005.c
 * @brief battery charger implementation using Fairchild fan54005
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

#include "fan5401x.h"
#include "charger.h"
#include "i2c_charger_common.h"

#define DEV_ADDR FAN54005_SlaveAddr

static struct i2c_handle handle;

int charger_kick_wdog(void)
{
    CHARGER_UPDATE_BITS(FAN5401x_CTRL0, 0x80, 0x80);
    return 0;
}

/*
 * Set all charging parameters before charging starts
 */
static int set_charge_param(void)
{
    // voltage - 4.34V, 4.24V is too low
    CHARGER_WRITE(FAN5401x_OREG, Voreg_Coder(4340) | 0x10);

    // charge current - 1450mA (this is maximum for this charger ic)
    CHARGER_WRITE(FAN5401x_IBAT, Iocharge_1450mA | Iterm_388mA);

    // input current limit - no limit
    CHARGER_WRITE(FAN5401x_CTRL1, IinLIM_NoLim | Vlowv_3400mV | Iterm_Enabled);

    return 0;
}

int enable_charge(void)
{
    int ret;

    ret = set_charge_param();
    if (ret) {
        BLOG_ERR("Failed to set charge parameters\n");
        return ret;
    }

    CHARGER_UPDATE_BITS(FAN5401x_CTRL1, Charger_Disabled, 0);
    return 0;
}

int disable_charge(void)
{
    CHARGER_UPDATE_BITS(FAN5401x_CTRL1, Charger_Disabled, Charger_Disabled);
    return 0;
}

int charger_get_stats(struct charge_stats * stats)
{
    int ret;
    uint8_t val;
    CHARGER_READ(FAN5401x_CTRL0, ret);

    stats->vbus_stat = VBUS_STAT_UNKNOWN;

    val = (ret >> CHARGER_STAT_BIT_POS) & CHARGER_STAT_BIT_MASK;
    switch (val) {
    case CHARGER_STAT_READY:
        stats->charge_stat = CHARGE_STAT_IDLE;
        break;
    case CHARGER_STAT_CHARGER_IN_PROGRESS:
        stats->charge_stat = CHARGE_STAT_FASTCHG;
        break;
    case CHARGER_STAT_CHARGER_DONE:
        stats->charge_stat = CHARGE_STAT_CHGDONE;
        break;
    case CHARGER_STAT_FAULT:
        stats->charge_stat = CHARGE_STAT_FAULT;
        break;
    default:
        BLOG_ERR("Illegal charge stat %d\n", val);
        return -1;
    }

    ret = is_charger_connected();
    if (ret < 0) {
        BLOG_ERR("Failed to check if charger is connected\n");
        return ret;
    }
    stats->power_good = ret;

    // FAULT_NTC is not available on this chip, assuming normal always.
    stats->fault_ntc = FAULT_NTC_NORMAL;

    return 0;
}

int is_charger_connected(void)
{
    int ret;
    CHARGER_READ(FAN5401x_MON, ret);
    return (ret & MON_VBUS_VALID) >> 1;
}

int charger_init(void)
{
    int ret;
    ret = i2c_open(&handle, CHARGER_I2C_BUSNO, DEV_ADDR);
    if (ret) {
        BLOG_ERR("Failed to open i2c: bus=%d, addr=%02x\n", CHARGER_I2C_BUSNO, DEV_ADDR);
        return ret;
    }

    CHARGER_WRITE(FAN5401x_SAFETY, Isafe_1450mA | Vsafe_VOregMax_4360mV);

    // make input current be controlled by IOCHARGE bits
    CHARGER_UPDATE_BITS(FAN5401x_SPCHARGER, 1 << 5, 0);

    return charger_kick_wdog();
}

void charger_term(void)
{
    i2c_close(&handle);
}

int poweroff(void)
{
    // do nothing. this charger ic does not have power off function
    return 0;
}
