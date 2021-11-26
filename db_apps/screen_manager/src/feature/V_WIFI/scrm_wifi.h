/*
 * scrm_wifi.h
 *    Wifi Screen UI support.
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
#ifndef __SCRM_WIFI_H__
#define __SCRM_WIFI_H__

#define SCRM_WIFI_MENU_HEADER _("Wireless settings")

#define RDB_WIFI_PREFIX "wlan.0"
#define RDB_WIFI_RADIO_VAR RDB_WIFI_PREFIX".radio"
#define RDB_WIFI_ENABLE_VAR RDB_WIFI_PREFIX".enable"
#define RDB_WIFI_SSID_VAR RDB_WIFI_PREFIX".ssid"
#define RDB_WIFI_AUTH_VAR RDB_WIFI_PREFIX".network_auth"
#define RDB_WIFI_CHANNEL_VAR RDB_WIFI_PREFIX".conf.channel"
#define RDB_WIFI_CUR_CHANNEL_VAR RDB_WIFI_PREFIX".currChan"

#define RDB_WIFI_CLIENT_PREFIX "wlan_sta.0"
#define RDB_WIFI_CLIENT_RADIO_VAR RDB_WIFI_CLIENT_PREFIX".radio"
#define RDB_WIFI_CLIENT_SSID_VAR RDB_WIFI_CLIENT_PREFIX".ap.0.ssid"
#define RDB_WIFI_CLIENT_STATUS_VAR RDB_WIFI_CLIENT_PREFIX".sta.connStatus"
#define RDB_WIFI_CLIENT_IP_VAR RDB_WIFI_CLIENT_PREFIX".ip"

#define SCRM_WIFI_BUF_SIZE 32
#define SCRM_WIFI_LABEL_BUF_SIZE 64

enum {
    SCRM_WIFI_MENU_AP_STATUS = 0,
#ifdef V_WIFI_CLIENT_backports
	SCRM_WIFI_MENU_CLIENT_STATUS,
#endif
    SCRM_WIFI_MENU_AP_ENABLE,
#ifdef V_WIFI_CLIENT_backports
	SCRM_WIFI_MENU_CLIENT_ENABLE,
#endif
};

typedef struct scrm_wifi_menu_item_ {
    const char *label;
    ngt_widget_callback_t cb;
    void *cb_arg;
} scrm_wifi_menu_item_t;

/* wifi ap or client */
typedef enum wifi_func_{
	SCRM_WIFI_AP,
	SCRM_WIFI_CLIENT
} wifi_func_t;

#endif /* __SCRM_WIFI_H__ */
