/*
 * scrm_ntc20_platform.h
 *    NTC20 screen manager platform support.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef __SCRM_NTC20_PLATFORM_H__
#define __SCRM_NTC20_PLATFORM_H__

#include <scrm.h>

#define MAIN_GRID_MAX_ROWS 3
#define MAIN_GRID_ROW_DEFN { 1, 5, 1 }
#define MAIN_GRID_COL_DEFN { 1 }
#define CONTROL_BAR_ROW_DEFN { 1 }
#define CONTROL_BAR_COL_DEFN { 2, 1, 2 }
#define STATUS_BAR_ROW_DEFN { 1 }
#define STATUS_BAR_COL_DEFN { 1, 1, 1 }

#define STATUS_BAR_ROW   0
#define MAIN_CONTENT_ROW 1
#define CONTROL_BAR_ROW  2

#define MAX_NUM_BUTTONS 2

/* Status bar slots */
#define SCRM_NTC20_SBAR_WIFI      0
#define SCRM_NTC20_SBAR_SIM       1
#define SCRM_NTC20_SBAR_BLUETOOTH 2

/* Control bar slots */
#define SCRM_NTC20_CBAR_BTN0    0
#define SCRM_NTC20_CBAR_BATTERY 1
#define SCRM_NTC20_CBAR_BTN1    2

typedef enum scrm_ntc20_battery_id_ {
    SCRM_NTC20_BATT_ID_0_PCENT = 0,
    SCRM_NTC20_BATT_ID_20_PCENT,
    SCRM_NTC20_BATT_ID_40_PCENT,
    SCRM_NTC20_BATT_ID_60_PCENT,
    SCRM_NTC20_BATT_ID_80_PCENT,
    SCRM_NTC20_BATT_ID_100_PCENT,
    SCRM_NTC20_BATT_ID_MAX
} scrm_ntc20_battery_id_t;

#define SCRM_NTC20_IMG_BATT_0_PCENT "battery0.png"
#define SCRM_NTC20_IMG_BATT_20_PCENT "battery1.png"
#define SCRM_NTC20_IMG_BATT_40_PCENT "battery2.png"
#define SCRM_NTC20_IMG_BATT_60_PCENT "battery3.png"
#define SCRM_NTC20_IMG_BATT_80_PCENT "battery4.png"
#define SCRM_NTC20_IMG_BATT_100_PCENT "battery5.png"

typedef enum scrm_ntc20_sim_signal_id_ {
    SCRM_NTC20_SIM_SIG_ID_0_BAR = 0,
    SCRM_NTC20_SIM_SIG_ID_1_BAR,
    SCRM_NTC20_SIM_SIG_ID_2_BAR,
    SCRM_NTC20_SIM_SIG_ID_3_BAR,
    SCRM_NTC20_SIM_SIG_ID_4_BAR,
    SCRM_NTC20_SIM_SIG_ID_5_BAR,
    SCRM_NTC20_SIM_SIG_ID_MAX,
} scrm_ntc20_sim_signal_id_t;

#define SCRM_NTC20_IMG_SIM_SIG_0_BAR "sim_signal0.png"
#define SCRM_NTC20_IMG_SIM_SIG_1_BAR "sim_signal1.png"
#define SCRM_NTC20_IMG_SIM_SIG_2_BAR "sim_signal2.png"
#define SCRM_NTC20_IMG_SIM_SIG_3_BAR "sim_signal3.png"
#define SCRM_NTC20_IMG_SIM_SIG_4_BAR "sim_signal4.png"
#define SCRM_NTC20_IMG_SIM_SIG_5_BAR "sim_signal5.png"

#define RDB_GETNAME_BUF_LEN 256
#define SCRM_NTC20_RDB_BUF_LEN 64

#define RDB_VAR_BATTERY "battery"
#define RDB_VAR_BATTERY_ONLINE RDB_VAR_BATTERY".online"
#define RDB_VAR_BATTERY_PERCENT  RDB_VAR_BATTERY".percent"

#define RDB_VAR_SIM_STATUS "wwan.0.sim.status.status"
#define RDB_VAR_SIM_SIGNAL_STRENGTH "wwan.0.radio.information.signal_strength"
#define RDB_VAL_SIM_STATUS_OK "SIM OK"

#define RDB_VAR_WIFI_PREFIX "wlan.0"
#define RDB_VAR_WIFI_RADIO_VAR RDB_VAR_WIFI_PREFIX".radio"

#define RDB_VAR_WIFI_CLIENT_PREFIX "wlan_sta.0"
#define RDB_VAR_WIFI_CLIENT_RADIO_VAR RDB_VAR_WIFI_CLIENT_PREFIX".radio"

#define RDB_VAR_BLUETOOTH_ENABLE "bluetooth.conf.enable"

#define SCRM_NTC20_POLL_TIME_MSEC 10000

typedef struct scrm_button_ {
    ngt_widget_t *button;
    short key_code;
    scrm_button_callback_t cb;
    void * cb_arg;
} scrm_button_t;

typedef struct scrm_ntc20_screen_ {
    ngt_screen_t *screen;
    ngt_grid_layout_t *top_level_grid;
    unsigned int top_level_row_numbers[MAIN_GRID_MAX_ROWS];
    unsigned int top_level_num_rows;
    ngt_grid_layout_t *status_bar;
    ngt_widget_t *main_content;
    ngt_grid_layout_t *control_bar;
    scrm_button_t *control_buttons;
    unsigned int num_control_buttons;
    void *user_data;
} scrm_ntc20_screen_t;

#endif /* __SCRM_NTC20_PLATFORM_H__ */
