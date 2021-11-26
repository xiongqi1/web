/**
 * @file max17xxx.c
 * @brief battery monitor implementation using MAX17050 & MAX17201 fuel gauge
 *
 * @note Both chips share a common set of registers with different I2C addresses
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

#include <unistd.h>
#include <math.h>
#include <time.h>
#include "max17050.h"
#include "monitor.h"
#include "i2c_monitor_common.h"
#include "charger.h"
#include "battery_rdb.h"

#ifdef V_BATTERY_MONITOR_max17201
    #include "max172xx.h"
#endif

#if defined(V_BATTERY_MONITOR_max17050) || defined(V_BATTERY_MONITOR_max17201)
    #define DEV_ADDR MAX17050_I2C_ADDR
#else
    #error "V_BATTERY_MONITOR is unknown"
#endif

#ifdef V_BATTERY_MONITOR_max17201
    #define NDEV_ADDR MAX172XX_I2C_NADDR
#endif

#ifdef V_BATTERY_MONITOR_max17050
    #define VALID_TEMP_READING_MIN_VOLTAGE_MV 3100
    #define MONITOR_CHIP_MIN_FULL_BATTERY_CAPACITY (1150 * 2) /* 1150mAh */
    #define MONITOR_CHIP_MAX_FULL_BATTERY_CAPACITY (3300 * 2) /* 3300mAh */
    #define MONITOR_CHIP_NOM_FULL_BATTERY_CAPACITY (2900 * 2) /* 2900mAh */
#else
    #define MONITOR_CHIP_MAX_FULL_BATTERY_CAPACITY (6614 * 2) /* 6614mAh - suggested by Maxim Configuration Wizard */
    #define MONITOR_CHIP_NOM_FULL_BATTERY_CAPACITY (5700 * 2) /* 5700mAh */
#endif

/* Initial configuration - Enable bits related reading temperature */
#define MONITOR_CHIP_CONFIG 0x2210

/* The following values are extracted from the chip after charging and
 * discharging many times. These value may change as we get more ideal
 * values. Only needed for MAX17050 */
#define MONITOR_CHIP_AVG_DQACC 0x291
#define MONITOR_CHIP_AVG_DPACC 0x28DF

// Vempty=3.0V, Vrecovery=3.6V
#define MONITOR_CHIP_V_EMPTY 0x965A

#ifdef V_BATTERY_CHARGER_bq25601
    // align with bq25601 settings
    /*
     * set FullSocThr to 90% so that FullCapRep is updated earlier
     * 3 LSBs should be fixed 101b per datasheet
     */
    #define MONITOR_CHIP_FULL_SOC_THR ((90 << 8) | 5)
    #define MONITOR_CHIP_ICHG_TERM (24000000 / 15625) /* 240mA */
#else
    #define MONITOR_CHIP_FULL_SOC_THR (98 << 8) /* 98% */
    #define MONITOR_CHIP_ICHG_TERM (15000000 / 15625) /* 150mA */
#endif

#ifdef V_BATTERY_MONITOR_max17201
    #define MONITOR_CHIP_PACKCFG ( \
        (MAX172XX_REG_PACKCFG_TEMP_AIN1 << MAX172XX_REG_PACKCFG_TEMP_SHIFT) | \
        (MAX172XX_REG_PACKCFG_VOLT_BATT << MAX172XX_REG_PACKCFG_VOLT_SHIFT) | \
        (1 << MAX172XX_REG_PACKCFG_NCELLS_SHIFT) \
    )

    // set enLCfg, enICT, enVE, enDC (only enLCfg is default)
    #define MONITOR_CHIP_NNVCFG0 0x01b0
    // set enFTh, enCrv, enCTE (enFTh is not default)
    #define MONITOR_CHIP_NNVCFG1 0x2006
    // set enT, enSOC, enMMT, enMMV, enMMC, enVT, enFC, enIAvg (all are default)
    #define MONITOR_CHIP_NNVCFG2 0xff0a
#endif

#define GAUGE_FROZEN_TIME_SEC 60
#define MAX_CURRENT_CHANGE_MA_PS 30
#define MIN_LEVEL_STAY_TIME_SEC 20
#define MIN_RESET_INTERVAL_SEC (5*60)

static struct i2c_handle handle;

#ifdef V_BATTERY_MONITOR_max17201
static struct i2c_handle nhandle; // for non-volatile config
#endif

static int i2c_opened = 0;
static int initialised = 0;

int monitor_init(void)
{
    int ret;
#ifdef V_BATTERY_MONITOR_max17201
    int pack_cfg;
#endif
    if (initialised) {
        return 0;
    }

    if (!i2c_opened) {
        ret = i2c_open(&handle, MONITOR_I2C_BUSNO, DEV_ADDR);
        if (ret) {
            BLOG_ERR("Failed to open i2c: bus=%d, addr=%02x\n", MONITOR_I2C_BUSNO, DEV_ADDR);
            return ret;
        }
#ifdef V_BATTERY_MONITOR_max17201
        ret = i2c_open(&nhandle, MONITOR_I2C_BUSNO, NDEV_ADDR);
        if (ret) {
            BLOG_ERR("Failed to open i2c: bus=%d, addr=%02x\n", MONITOR_I2C_BUSNO, NDEV_ADDR);
            i2c_close(&handle);
            return ret;
        }
#endif
        i2c_opened = 1;
    }

    /* Clear status register */
    MONITOR_WRITE(MAX17050_REG_STATUS_ADDR, 0x0);

#ifdef V_BATTERY_MONITOR_max17201
    // Check DesignCap register to tell if the chip lost valid settings
    MONITOR_READ(MAX17050_REG_DESIGN_CAP_ADDR, ret);
    MONITOR_READ(MAX172XX_REG_PACKCFG_ADDR, pack_cfg);
    if (ret != MONITOR_CHIP_NOM_FULL_BATTERY_CAPACITY || pack_cfg != MONITOR_CHIP_PACKCFG) {
        // the chip lost settings (battery removed?), reinitialise
        BLOG_NOTICE("reset fuel gauge settings\n");

        /*
         * MAX172XX has a different register initialisation procedure from MAX17050
         * 1) set non-volatile registers
         * 2) reset fuel gauge operation so that non-volatile register values
         * are loaded into ModelGauge registers and start running.
         *
         * Note: We only write to shadow RAM of non-volatile registers but do
         * not write to the flash (which has a limited number of 7 writes only).
         * This eliminates the factory process to program the chip and also
         * makes further adjustment to the settings easy via firmware upgrade.
         */
        // terminating charging current
        MONITOR_WRITE_N(MAX172XX_REG_NICHG_TERM_ADDR, MONITOR_CHIP_ICHG_TERM);
        // empty battery voltage
        MONITOR_WRITE_N(MAX172XX_REG_NV_EMPTY_ADDR, MONITOR_CHIP_V_EMPTY);
        // nominal full battery capacity
        MONITOR_WRITE_N(MAX172XX_REG_NFULL_CAP_NOM_ADDR, MONITOR_CHIP_MAX_FULL_BATTERY_CAPACITY);
        // full battery capacity
        MONITOR_WRITE_N(MAX172XX_REG_NFULL_CAP_REP_ADDR, MONITOR_CHIP_NOM_FULL_BATTERY_CAPACITY);
        // design battery capacity
        MONITOR_WRITE_N(MAX172XX_REG_NDESIGN_CAP_ADDR, MONITOR_CHIP_NOM_FULL_BATTERY_CAPACITY);
        // voltage measure from VBATT, temperature measure from AIN1
        MONITOR_WRITE_N(MAX172XX_REG_NPACKCFG_ADDR, MONITOR_CHIP_PACKCFG);
        // full state of charge percentage threshold
        MONITOR_WRITE_N(MAX172XX_REG_NFULL_SOC_THR_ADDR, MONITOR_CHIP_FULL_SOC_THR);

        // make sure ModdlGauge m5 registers are restored from non-volatile counterparts on fuel gauge reset
        MONITOR_WRITE_N(MAX172XX_REG_NNVCFG0_ADDR, MONITOR_CHIP_NNVCFG0);
        MONITOR_WRITE_N(MAX172XX_REG_NNVCFG1_ADDR, MONITOR_CHIP_NNVCFG1);
        MONITOR_WRITE_N(MAX172XX_REG_NNVCFG2_ADDR, MONITOR_CHIP_NNVCFG2);

        // reset fuel gauge so that above configs in shadow NV RAM take effects
        MONITOR_WRITE(MAX172XX_REG_CONFIG2_ADDR, 1);
        // reset takes up to POR_TIME(10ms), extra delay to allow readings to stabilize
        sleep(MAX172XX_RESET_DELAY_SEC);
    }
#endif

#ifdef V_BATTERY_MONITOR_max17050
    /* The value of FullCap register is used to check the chip maintains valid information.
     MAX17201 does not need to do this as the chip will automatically set it. */
    MONITOR_READ(MAX17050_REG_FULL_CAP_ADDR, ret);

    if ((ret <= MONITOR_CHIP_MIN_FULL_BATTERY_CAPACITY) ||
        (ret >= MONITOR_CHIP_MAX_FULL_BATTERY_CAPACITY)) {
        /* Set experimental values if FULL CAP information is out of range. */
        MONITOR_WRITE(MAX17050_REG_DESIGN_CAP_ADDR,
                      MONITOR_CHIP_NOM_FULL_BATTERY_CAPACITY);
        MONITOR_WRITE(MAX17050_REG_FULL_CAP_ADDR,
                      MONITOR_CHIP_MIN_FULL_BATTERY_CAPACITY);
        MONITOR_WRITE(MAX17050_REG_DQACC_ADDR, MONITOR_CHIP_AVG_DQACC);
        MONITOR_WRITE(MAX17050_REG_DPACC_ADDR, MONITOR_CHIP_AVG_DPACC);
    }
#endif

    /* Initialize config to enable reading temperature */
    MONITOR_WRITE(MAX17050_REG_CONFIG_ADDR, MONITOR_CHIP_CONFIG);
    MONITOR_READ(MAX17050_REG_CONFIG_ADDR, ret);
    if (MONITOR_CHIP_CONFIG != ret) {
        BLOG_ERR("failed to write to monitor chip\n");
        return -1;
    }

#ifdef V_BATTERY_MONITOR_max17050
    /* Set the fully charged threshold. */
    MONITOR_WRITE(MAX17050_REG_FULL_SOC_THR_ADDR, MONITOR_CHIP_FULL_SOC_THR);
    MONITOR_WRITE(MAX17050_REG_ICHG_TERM_ADDR, MONITOR_CHIP_ICHG_TERM);

    /* Averages voltage every 11.25 secs to synchronise with average current.
       MAX17201 does not need to do this as the default value is enough */
    MONITOR_READ(MAX17050_REG_FILTER_CFG_ADDR, ret);
    uint16_t val = (uint16_t)ret;
    val &= ~MAX17050_FILTER_CFG_VOL_MASK;
    MONITOR_WRITE(MAX17050_REG_FILTER_CFG_ADDR, val);
#endif

    initialised = 1;
    return 0;
}

void monitor_term(void)
{
    if (i2c_opened) {
        i2c_close(&handle);
#ifdef V_BATTERY_MONITOR_max17201
        i2c_close(&nhandle);
#endif
    }
    i2c_opened = 0;
    initialised = 0;
}

/*
 * Look up battery charging/discharging level tables by voltage and JEITA mode
 *
 * @param voltage The present voltage reading
 * @param current The present current reading
 * @param mode The present JEITA mode
 * @return The battery level from 0% to 100%
 */

static uint8_t battery_level_lookup(uint16_t voltage, int16_t current, enum fault_ntc mode)
{
    const uint16_t charging_voltages [][21] = {
        { 3218, 3620, 3709, 3758, 3788, 3809, 3827, 3848, 3869, 3896, 3929, 3971,
            4016, 4060, 4104, 4149, 4188, 4199, 4205, 4209, 4216 }, // normal
        { 3135, 3570, 3625, 3669, 3714, 3745, 3766, 3783, 3800, 3819, 3840, 3864,
            3898, 3934, 3973, 4009, 4028, 4035, 4041, 4048, 4055 }, // warm
        { 3500, 3598, 3668, 3720, 3743, 3760, 3777, 3801, 3830, 3858, 3888, 3926,
            3971, 4017, 4061, 4104, 4145, 4191, 4205, 4210, 4212 },  // cool
    };
    const uint16_t discharging_voltages [] = { 3200, 3302, 3362, 3414, 3453, 3484, 3509, 3530, 3552,
        3575, 3603, 3636, 3677, 3725, 3771, 3813, 3860, 3908, 3953, 3996, 4072 };

    const uint16_t* pv;
    uint8_t level = 0;
    static uint8_t last_level = 0xFF;
    static int16_t last_current;
    static time_t mode_change_time, level_change_time;
    int16_t didt;
    int charging =  current > 0;
    uint16_t voltage_p0;

    if(mode != FAULT_NTC_NORMAL && mode != FAULT_NTC_WARM && mode != FAULT_NTC_COOL) {
        mode = FAULT_NTC_NORMAL;
    }
    pv = charging ? charging_voltages[mode] : discharging_voltages;
    voltage_p0 = *pv;

    while (voltage > *pv && level < 100) {
        level += 5;
        pv++;
    }

    // liner estimation
    if(level != 0) {
        float upper = *pv;
        float lower = *(pv-1);

        level -= round(5 * (upper - voltage) / (upper - lower));
        level = level>100 ? 100 : level;
    }

    // It only reports zero percent battery level when the voltage is equal or less than
    // the first entry of the profile table, so that it would turn off the OWA and shortly after
    // it would turn off itself.
    if(level == 0 && voltage > voltage_p0) {
        level = 1;
    }

    if(last_level == 0xFF) {
        // get level reported in the last power cycle
        // set an invalid value in case of accidental turning off.
        last_level = get_persisted_level();
        set_persisted_level(0xFF);
        BLOG_DEBUG("power level of last power cycle: %d", last_level);

        last_level = last_level == 0xFF ? level : last_level;
        last_current = current;

        time(&mode_change_time);
        level_change_time = mode_change_time;
        mode_change_time -= GAUGE_FROZEN_TIME_SEC;
        return last_level;
    }

    didt = current - last_current;
    last_current = current;
    // BLOG_DEBUG("di/dt: %d", didt);
    write_didt(didt);

    if(charging) {
        didt = didt>0 ? didt : -didt;
        if( didt > MAX_CURRENT_CHANGE_MA_PS) {
            time(&mode_change_time);
            BLOG_DEBUG("di/dt locked - 0");
            return last_level;
        }
        else {
            time_t now;
            time(&now);

            if(now - mode_change_time < GAUGE_FROZEN_TIME_SEC) {
                BLOG_DEBUG("di/dt locked - %d", (int) (now - mode_change_time));
                return last_level;
            }
        }
    }

    // BLOG_DEBUG("di/dt not locked");
    if( (charging && level < last_level) || (!charging && level > last_level) ) {
        BLOG_DEBUG("inconsistent level, last: %d, new: %d", last_level, level);
    }
    else if(last_level != level) {
        time_t now;
        time(&now);

        // smooth the change by limiting each change to +/- 1 percent and could
        // not be faster than 1 change per MIN_LEVEL_STAY_TIME_SEC
        if(now - level_change_time > MIN_LEVEL_STAY_TIME_SEC) {
            last_level += level>last_level ? 1 : -1;
            level_change_time = now;
        }
        else {
            BLOG_DEBUG("level change postpond, old: %d, new: %d", last_level, level);
        }
    }

    return last_level;
}

int monitor_read_voltage(uint16_t * voltage)
{
    int ret;
    MONITOR_READ(MAX17050_REG_AVERAGE_V_CELL_ADDR, ret);
    *voltage = (ret >> 3) * 625 / 1000;
    return 0;
}

int monitor_read_current(int16_t * current)
{
    int ret;
    MONITOR_READ(MAX17050_REG_AVERAGE_CURRENT_ADDR, ret);
    *current = (int16_t)ret * (int)15625 / 100000;
    return 0;
}

int monitor_read_capacity(uint16_t * capacity, uint8_t * percentage)
{
    int ret;
    MONITOR_READ(MAX17050_REG_REM_CAP_REP_ADDR, ret);
    *capacity = ret / 2;
    MONITOR_READ(MAX17050_REG_SOC_REP_ADDR, ret);
    *percentage = ret >> 8;
    if (*percentage > 100) {
        *percentage = 100;
    }
    return 0;
}

int monitor_read_full_capacity(uint16_t * capacity)
{
    int ret;
#ifdef V_BATTERY_MONITOR_max17050
    MONITOR_READ(MAX17050_REG_FULL_CAP_ADDR, ret);
#else
    // MAX172XX uses a new register for full capacity report
    MONITOR_READ(MAX172XX_REG_FULL_CAP_REP_ADDR, ret);
#endif
    *capacity = ret / 2;
    return 0;
}

int monitor_read_temperature(int8_t * temperature)
{
    int ret;
    MONITOR_READ(MAX17050_REG_TEMPERATURE_ADDR, ret);
    *temperature = (int16_t)ret >> 8;
    return 0;
}

int monitor_get_stats(struct monitor_stats * stats, enum fault_ntc mode)
{
    int ret;
    time_t now;
    static time_t last_reset_time = 0;

    uint16_t voltage;
    uint16_t capacity;
    uint8_t percentage;
    int8_t temperature;

    ret = monitor_init();
    if (ret < 0) {
        return ret;
    }

    ret = monitor_read_voltage(&voltage);
    if (ret < 0) {
        return ret;
    }
    stats->voltage = voltage;

    int16_t current;
    ret = monitor_read_current(&current);
    if (ret < 0) {
        return ret;
    }
    stats->current = current;

    ret = monitor_read_capacity(&capacity, &percentage);
    if (ret < 0) {
        return ret;
    }
    stats->capacity = capacity;
    stats->percentage = battery_level_lookup(voltage, current, mode);

    ret = monitor_read_full_capacity(&capacity);
    if (ret < 0) {
        return ret;
    }
    stats->full_capacity = capacity;

    ret = monitor_read_temperature(&temperature);
    if (ret < 0) {
        return ret;
    }
    stats->temperature = temperature;

#ifdef V_BATTERY_MONITOR_max17050
    stats->temp_valid = voltage > VALID_TEMP_READING_MIN_VOLTAGE_MV;
#endif

#ifdef V_BATTERY_MONITOR_max17201
    MONITOR_READ(MAX172XX_REG_PACKCFG_ADDR, ret);
    stats->temp_valid = MONITOR_CHIP_PACKCFG == ret;

    time(&now);
    if(last_reset_time == 0) {
        last_reset_time = now - MIN_RESET_INTERVAL_SEC;
    }

    if(!stats->temp_valid && now > (last_reset_time + MIN_RESET_INTERVAL_SEC)) {
        last_reset_time = now;
        BLOG_NOTICE("fuel gauge full reset\n");

        // hardware reset
        MONITOR_WRITE(MAX172XX_REG_COMMAND_ADDR, 0x0F);
        usleep(MAX172XX_POR_TIME_MS * 1000);

        // at next main loop, when this function is called again, write the
        // configuration and then a FG reset will follow
        initialised = 0;
    }
#endif

    return 0;
}
