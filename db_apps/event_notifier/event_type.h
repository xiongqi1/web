/*!
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */
#ifndef EVENT_TYPE_H_20140617
#define EVENT_TYPE_H_20140617

typedef enum {
	DUMMY	 = 0,			/* index 0 is used for a placeholder of common destination fields */
	POWER_UP = 1,			/* send when this appl start automatically, only once */
	REBOOT = 2,			/* send by external event notifier */
	LINK_STATUS_CHANGED = 3,	/* wwan, IPSec, OpenVPN, PPTP, GRE etcs,. */
	WWAN_IP_CHANGED = 4,
	REG_STATUS_CHANGED = 5,		/* not reg. <--> home <--> roaming */
	CELL_ID_CHANGED = 6,
	WWAN_NETWORK_CHANGED = 7,	/* 2G <--> 3G <--> LTE */
	ETH_DEV_NUM_CHANGED = 8,
	POWER_SRC_CHANGED = 9,		/* DCJack <--> PoE */
	LOGIN_STATUS = 10,		/* send by external event notifier when WEBUI, SSH, TELNET login status */
	SDCARD_STATUS_CHANGED = 11,	/* inserted <--> removed */
	FAILOVER_INSTANCE_OCCURED = 12,	/* interface x <--> interface y  */
	WIFI_CLIENTS_CHANGED = 13,	/* connected WiFi clients */
	WIFI_RESERVED_MAC_EVENT = 14,	/* connected <--> disconnected (mac address from MAC reserve list) */
	DIGITAL_IN_CHANGED = 15,	/* Pin status change */
	ANALOGUE_IN_THRESHOLD = 16,	/* Pin voltage crosses configured threshold for time */
	DIGITAL_OUT_CHANGED = 17,	/* Pin status change */
	WDS_STATUS_CHANGED = 18,	/* Wi-Fi Bridge (WDS) status change */
	VPN_STATUS_CHANGED = 19,	/* IPSec/OpenVPN/PPTP/GRE tunnel status change */
	FOTA_DOTA_STATUS = 20,		/* FOTA/DOTA status */
	BUTTON_SETTINGS_CHANGE = 21,		/* Button settings change */
	VRRP_MODE_CHANGE = 22,		/* VRRP daemon mode change (Master|BackUp) */
	USB_OTG_CONNECTION_EVENT = 23,	/* USB OTG connection/disconnection event */
	GPS_GEOFENCE_STATUS_CHANGED = 24,	/* GPS GEOFENCE status changed */
	/* new events should be added here (before NO_EVENT element) */
	NO_EVENT			/* not a event */
} event_type_enum;

#define MAX_EVENT_NUM	NO_EVENT

#define MAX_EVENT_NOTI_TEXT_LEN		2048
/* this array of strings must match event_type_enum */
const char *event_name_list[] = {
	"DUMMY",
	"POWER UP",
	"REBOOT",
	"LINK STATUS CHANGE",
	"WWAN IP CHANGE",
	"REG. STATUS CHANGE",
	"CELL ID CHANGE",
	"WWAN NETWORK CHANGE",
	"ETH. DEV. NUM. CHANGE",
	"POWER SRC. CHANGE",
	"LOGIN STATUS",
	"SDCARD STATUS CHANGE",
	"FAILOVER INSTANCE OCCURED",
	"WIFI CLIENTS CHANGED",
	"WIFI RESERVED MAC EVENT",
	"DIGITAL IO CHANGED",
	"ANALOGUE IN THRESHOLD",
	"DIGITAL OUT CHANGED",
	"WDS STATUS CHANGED",
	"VPN STATUS CHANGED",
	"FOTA/DOTA STATUS",
	"BUTTON SETTINGS CHANGE",
	"NO EVENT"
};

#endif  /* EVENT_TYPE_H_20140617 */

