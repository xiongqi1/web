/**
 * @file monitor.h
 * @brief common definitions for fuel gauge
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

#ifndef MONITOR_H_12410011032019
#define MONITOR_H_12410011032019

#include <stdint.h>
#include "charger.h"

struct monitor_stats {
    uint16_t voltage;
    int16_t current;
    uint16_t capacity;
    uint8_t percentage;
    uint16_t full_capacity;
    int8_t temperature;
    uint8_t temp_valid;
};

/*
 * initialise monitor IC chip
 *
 * @return 0 on success; negative error code on failure
 */
int monitor_init(void);

/*
 * terminate/cleanup monitor IC resource
 */
void monitor_term(void);

/*
 * read battery voltage from monitor
 *
 * @param voltage A uint16 pointer to store the voltage reading (mV)
 * @return 0 on success; negative error code on failure
 */
int monitor_read_voltage(uint16_t * voltage);

/*
 * read battery current from monitor
 *
 * @param voltage An int16 pointer to store the current reading (mA).
 * A positive value means current flows into the battery (charging), while
 * a negative value means current flows out of the battery (discharging).
 * @return 0 on success; negative error code on failure
 */
int monitor_read_current(int16_t * current);

/*
 * read battery capacity from monitor
 *
 * @param capacity A uint16 pointer to store the absolute capacity reading (mAh)
 # @param percentage A uint8 pointer to store the percentage capacity reading (0-100)
 * @return 0 on success; negative error code on failure
 */
int monitor_read_capacity(uint16_t * capacity, uint8_t * percentage);

/*
 * read battery full capacity from monitor
 *
 * @param capacity A uint16 pointer to store the absolute capacity reading (mAh)
 * @return 0 on success; negative error code on failure
 */
int monitor_read_full_capacity(uint16_t * capacity);

/*
 * read battery temperature from monitor
 *
 * @param temperature An int8 pointer to store the temperature reading (degree Celcius)
 * @return 0 on success; negative error code on failure
 */
int monitor_read_temperature(int8_t * temperature);

/*
 * read all battery stats from monitor
 *
 * @param stats A monitor_stats structto store the reading
 * @mode JEITA mode currently the charger is operating in
 * @return 0 on success; negative error code on failure
 */
int monitor_get_stats(struct monitor_stats * stats, enum fault_ntc mode);

#endif // MONITOR_H_12410011032019
