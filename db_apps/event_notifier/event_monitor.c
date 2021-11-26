/*
 * The event monitor subscribes to configured event RDB variables
 * and creates a list of events ( also in RDB )
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "daemon.h"
#include "event_rdb_util.h"
#include "event_rdb_names.h"
#include "event_type.h"
#include "event_util.h"

#define APPLICATION_NAME "event_monitor"
#define LOCK_FILE "/var/lock/subsys/"APPLICATION_NAME

/*
 * These are config and state that can be used for debouncing any type of event
 */
typedef struct {
	unsigned lastState : 1;		// the state the input was last seen in (1=high) - i.e. a boolean version of lastVal
	unsigned lastDir : 1;		// the last edge that was acted on (1=low-high) - i.e. =lastState once time period has expired
	unsigned confEdges : 2;		// which edges to send actual notifications for (bit 0 = high-low, bit 1 = low-high)
	long confHighMS;			// how long lastState must be 1 for lastDir to change
	long confLowMS;				// how long lastState must be 0 for lastDir to change
	struct timespec timeEdge;	// the most recent time that lastState changed
} debounce_params_type;

typedef enum {
	SUB_NO	 = 0,			/* do not subscribe */
	SUB_YES,				/* always subscribe */
	SUB_MAYBE,				/* could be either - code is in init() to decide */
} event_subscribe_enum;

/*
 * Params needed for IO events. For analogue inputs confLowThresh & confHighThresh
 * represent a dead-band (hysteresis) to prevent false triggering. When the value is
 * in that range lastState = lastDir so that no timers will start.
 */		//
typedef struct {
	debounce_params_type debounce;
	float confLowThresh;
	float confHighThresh;
} ioConds_type;

typedef struct {
	event_type_enum evtType;
	int instance;
	event_subscribe_enum subscribe;
	debounce_params_type *debounce;
	char lastVal[RDB_VAL_SIZE];
} rdbNameList_type;

typedef struct {
	event_type_enum evtType;
	const char* szPrefix;
	const char* szName;
	int instance;
	event_subscribe_enum subscribe;
	debounce_params_type *debounce;
} rdbNameDef_type;

typedef struct {
	char * rdbPath;
	rdbNameList_type  rdbNameList;
	void * p_event_handler;
	ioConds_type * ioConds;
} evtHandlerList_type;


typedef int (*p_event_handler_type)(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);

typedef struct {
	rdbNameDef_type rdbNameDef;
	p_event_handler_type p_event_handler;
} evtHandlerDef_type;


static evtHandlerList_type *dynEvtHandlerList;
static int ioEvtBase = 0;
static int evtCnt = 0;

static char event_noti_cmd[MAX_EVENT_NOTI_TEXT_LEN];
char rdb_name_buf[RDB_VAR_SIZE];


static int reg_status_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
static int cell_id_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
#ifndef V_SINGLE_NETWORK_y
static int wwan_network_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
#endif
#ifndef V_POWER_SOURCES_none
static int power_src_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
#endif
static int SD_card_status_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
#ifndef V_MULTIPLE_LANWAN_UI_none
static int failover_instance_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
#endif
#ifdef V_HW_PUSH_BUTTON_SETTINGS_y
static int button_settings_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
#endif
#ifdef V_VRRP_y
static int vrrp_mode_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
#endif
static int do_debounce(const evtHandlerList_type *pEvt);
static int io_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal);
static void polling_non_trigger_events(void);

evtHandlerDef_type evtHandlerList[] = {
	{{ REG_STATUS_CHANGED,		"wwan.0", RDB_NETWORKREG_STAT,		0,	SUB_YES}, reg_status_change_evt_handler },
	{{ CELL_ID_CHANGED,			"wwan.0", RDB_NETWORKCELLID_STAT,	0,	SUB_YES}, cell_id_change_evt_handler },
#ifndef V_SINGLE_NETWORK_y
	{{ WWAN_NETWORK_CHANGED,	"wwan.0", RDB_NETWORK_SYSMODE,		0,	SUB_YES}, wwan_network_change_evt_handler },
#endif
#ifndef V_POWER_SOURCES_none
	{{ POWER_SRC_CHANGED,		RDB_SYS_POWER_SOURCE, "",			0,	SUB_YES}, power_src_change_evt_handler },
#endif
#if defined (V_IOBOARD_kudu) || defined (V_IOBOARD_nguni)
	{{ SDCARD_STATUS_CHANGED,	RDB_SDCARD_STAT,	 "",			0,	SUB_YES}, SD_card_status_change_evt_handler },
#endif
#ifndef V_MULTIPLE_LANWAN_UI_none
	{{ FAILOVER_INSTANCE_OCCURED,	RDB_FAILOVER_PRIMARY, "",		0,	SUB_YES}, failover_instance_evt_handler },
#endif
#ifdef V_HW_PUSH_BUTTON_SETTINGS_y
	/*
	 * Currently, only reset button enable/disable status is monitored.
	 * This can be extended to other buttons' settings such as WPS, WAN etc.
	 */
	{{ BUTTON_SETTINGS_CHANGE,	RDB_RESET_DISABLE,	"",				0,	SUB_YES}, button_settings_change_evt_handler },
#endif

#ifdef V_VRRP_y
	{{ VRRP_MODE_CHANGE,	RDB_VRRP_MODE,	"",				0,	SUB_YES}, vrrp_mode_change_evt_handler },
#endif
};

static void event_notify()
{
	syslog(LOG_DEBUG, "%s: cmd = '%s'", __func__, event_noti_cmd);
	(void) system((const char *)event_noti_cmd);
}

typedef struct {
	const char *orig_name;
	const char *new_name;
} devname_conv_tbl_type;
const devname_conv_tbl_type devname_conv_tbl[] = {
	{ "wwan",	"WWAN"},
	{ "ipsec",	"IPSec"},
	{ "openvpn",	"OpenVPN"},
	{ "pptp", 	"PPTP"},
	{ "gre", 	"GRE"},
	{ "eth", 	"WAN"},
	{ "wlan_sta",	"WLAN_STA"},
	{ NULL, NULL},
};

static const char* conv_to_readable_dev_name(const char *dev_name)
{
	const devname_conv_tbl_type *p = &devname_conv_tbl[0];
	while (p) {
		if (strcmp(p->orig_name, dev_name) == 0)
			return p->new_name;
		p++;
	}
	return NULL;
}

#define MAX_PROFILE_IDX		100
static void polling_non_trigger_events(void)
{
	char val[6][RDB_VAR_SIZE]={{0,}, {0,}, {0,}, {0,}, {0,}, {0,}};
	char *dev_name = &val[0][0], *net_st = &val[1][0], *net_st_last = &val[2][0], *simple_dev_name = &val[3][0];
	char *ip_addr = &val[4][0], *ip_addr_last = &val[5][0], *pSavePtr;
	const char *p, *pToken;
	int i, len;

	for (i = 0; i < MAX_PROFILE_IDX; i++) {
		sprintf((char *)rdb_name_buf, "%s.%d.dev", RDB_LINK_PROFILE, i);
		p = rdb_getVal(rdb_name_buf); if (!p) break; strcpy(dev_name, p);
		syslog(LOG_DEBUG, "%s: idx %d, dev %s", __func__, i, dev_name);

		/* check link status change */
		sprintf((char *)rdb_name_buf, "%s.%d.status", RDB_LINK_PROFILE, i);
		p = rdb_getVal(rdb_name_buf); strcpy(net_st, (p && strlen(p))? p:"down");
		if (strcmp(net_st, "1")==0) strcpy(net_st, "up");
		else if (strcmp(net_st, "0")==0) strcpy(net_st, "down");
		sprintf((char *)rdb_name_buf, "%s.%d.status.last", RDB_LINK_PROFILE, i);
		p = rdb_getVal(rdb_name_buf); strcpy(net_st_last, (p && strlen(p))? p:"down");
		syslog(LOG_DEBUG, "%s: profile %d : dev : %s, link st : %s, link st last: %s", __func__, i, dev_name, net_st, net_st_last);
		/* ignore intermediate status "disconnecting" */
		if (strcmp(net_st_last, net_st) && strcmp(net_st, "disconnecting")) {
			(void) rdb_setVal(rdb_name_buf, net_st);
			pToken = strtok_r(dev_name, ".", &pSavePtr); if (!pToken) continue;
			simple_dev_name = (char *)conv_to_readable_dev_name(pToken);
			if (!simple_dev_name) {
				len = strlen(pToken);
				for (i = 0; i < len; i++, simple_dev_name++, pToken++) {
					*simple_dev_name = toupper(*pToken);
				}
			}
			sprintf((char *)&event_noti_cmd[0], "elogger %d \"Profile %d %s status changed : %s --> %s\"&",
					LINK_STATUS_CHANGED, i, simple_dev_name, net_st_last, net_st);
			event_notify();
		}

		/* check wwan ip change */
		sprintf((char *)rdb_name_buf, "%s.%d.iplocal", RDB_LINK_PROFILE, i);
		p = rdb_getVal(rdb_name_buf); strcpy(ip_addr, (p && strlen(p))? p:"N/A");
		if (strncmp(dev_name, "wwan", 4)) {
			syslog(LOG_DEBUG, "%s: ignore %s device event", __func__, dev_name);
			continue;
		}
		sprintf((char *)rdb_name_buf, "%s.%d.iplocal.last", RDB_LINK_PROFILE, i);
		p = rdb_getVal(rdb_name_buf); strcpy(ip_addr_last, (p && strlen(p))? p:"N/A");
		syslog(LOG_DEBUG, "%s: wwan dev : %s, wwan st : %s, ip addr : %s, ip addr last: %s",
			__func__, dev_name, net_st, ip_addr, ip_addr_last);
		if (strcmp(ip_addr_last, ip_addr)) {
			(void) rdb_setVal(rdb_name_buf, ip_addr);
			sprintf((char *)&event_noti_cmd[0], "elogger %d \"WWAN IP address changed : %s --> %s\"&", WWAN_IP_CHANGED, ip_addr_last, ip_addr);
			event_notify();
		}
	}

#ifdef V_WIFI_backports
#ifdef V_MODCOMMS_y
	p = rdb_getVal("modcomms.rf-mice.1.status");
	if (p && strcmp(p, "ready") == 0) {
#endif

	int wifi_clients=0, wifi_clients_last=0;
	int j;
	int matched;
	char *cmd = &val[0][0], *wlan_mac = &val[1][0], *wlan_mac_reserver = &val[2][0], *mac_reserver_last = &val[3][0];

#if defined(V_WIFI_MBSSID_y)
	#define NUM_APS 5
#else
	#define NUM_APS 1
#endif
	j=0;
	for( i=0; i< NUM_APS; i++ ) {
		sprintf(cmd, "assoc_sta_info.lua wlan%d 2> /dev/null |grep \"Station \"", i);
		FILE *outf = popen(cmd, "r");
		if (outf) {
			/* read connected wlan clients, etc. "Station 00:1d:0f:de:37:7d (on wlan0)" */
			while ( fgets(wlan_mac, RDB_VAR_SIZE-1, outf) ) {
				//syslog(LOG_ERR,"%s: wlan_mac : %s", __func__, wlan_mac);
				if(*(wlan_mac+10)==':' && *(wlan_mac+13)==':' && *(wlan_mac+16)==':' && *(wlan_mac+19)==':' && *(wlan_mac+22)==':') {
					// save connected wlan client to rdb
					*(wlan_mac+25)=0;
					sprintf((char *)rdb_name_buf, "%s.%d",RDB_WLAN_CLIENTLIST, j);
					p = rdb_getVal(rdb_name_buf);
					if ((p && strcmp(p, wlan_mac+8)) || !p) {
						(void) rdb_setVal(rdb_name_buf, wlan_mac+8);
						//syslog(LOG_ERR,"%s: set %s to %s", __func__, rdb_name_buf, wlan_mac+8);
					}
					wifi_clients++;
					j++;
				}
				else {
					syslog(LOG_ERR,"%s mac address format error -- %s", __func__, wlan_mac);
				}
			}
			pclose(outf);
		}
	}

	// WiFi clients number changed
	p = rdb_getVal("wlan.clients_last"); wifi_clients_last = (p && strlen(p)? atoi(p):0);
	if ( wifi_clients != wifi_clients_last) {
		//clients number has changed, may need clean the last_client from rdb list
		if ( wifi_clients < wifi_clients_last) {
			sprintf((char *)rdb_name_buf, "%s.%d",RDB_WLAN_CLIENTLIST, wifi_clients); (void) rdb_setVal(rdb_name_buf, "");
		}
		sprintf(cmd, "%d", wifi_clients); (void) rdb_setVal("wlan.clients_last", cmd);
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"WiFi clients number changed : %d --> %d\"&", WIFI_CLIENTS_CHANGED, wifi_clients_last, wifi_clients);
		event_notify();
	}

	// Wifi reserved MAC client status changed
	for( i=0; ;i++) {
		//get each items from MAC address reservation list
		sprintf((char *)rdb_name_buf, "%s.%d",RDB_WLAN_RESERVATION_MAC, i);
		p = rdb_getVal(rdb_name_buf);
		if(!p || !strlen(p)) break;

		//Check matched clients from the MAC address reservation list
		strcpy(wlan_mac_reserver, p);
		matched=0;
		for( j=0; ;j++) {
			sprintf((char *)rdb_name_buf, "%s.%d",RDB_WLAN_CLIENTLIST, j);
			p = rdb_getVal(rdb_name_buf);
			if(!p || !strlen(p)) break;
			if( strcmp(p, wlan_mac_reserver)==0 ) {
				matched++;
				break;
			}
		}

		// check last matching status
		sprintf((char *)rdb_name_buf, "%s_last.%d",RDB_WLAN_RESERVATION_MAC, i);
		p = rdb_getVal(rdb_name_buf);strcpy(mac_reserver_last, (p && strlen(p))? p:"N/A");
		if(matched && strcmp(mac_reserver_last, "connected")!=0) {
			(void) rdb_setVal(rdb_name_buf, "connected");
			sprintf((char *)&event_noti_cmd[0], "elogger %d \"Monitored WiFi clients status change : %s --> connected (%s)\"&", WIFI_RESERVED_MAC_EVENT, mac_reserver_last, wlan_mac_reserver);
			event_notify();
		}
		else if(!matched && strcmp(mac_reserver_last, "connected")==0) {
			(void) rdb_setVal(rdb_name_buf, "disconnected");
			sprintf((char *)&event_noti_cmd[0], "elogger %d \"Monitored WiFi clients status change : %s --> disconnected (%s)\"&", WIFI_RESERVED_MAC_EVENT, mac_reserver_last, wlan_mac_reserver);
			event_notify();
		}
	}

#ifdef V_MODCOMMS_y
	}
#endif

#else

// @todo Add Ralink/etc support if required.

#endif

#if defined (V_IOBOARD_kudu) || defined (V_IOBOARD_nguni) || defined (V_IOBOARD_falcon) || defined (V_IOBOARD_clarke)
	{

	typedef enum {
		DISCONNECTED = 0,
		CONNECTED  = 1,
		DISABLED = 2,
		NO_STATUS  //Mode changing or undefined
	} status_enum;

	char *otg_cmd = &val[0][0], *otg_resp= &val[1][0];
	static status_enum old_st = NO_STATUS;
	status_enum new_st = DISCONNECTED;
	char * status_str[] = {"disconnected", "connected" , "disabled" , ""};

	#ifdef V_IOBOARD_falcon // to hide intermediate disabled state between connected and disconnected state.
	#define INTERMEDIATE_DELAY 5
	static intermediate_disabled_cnt = 0;
	#endif

#if defined (V_USB_OTG_BUS_) || !defined (USB_OTG_BUS)
	int bus[] = {};
	#define NUM_BUS 0
#else
	int bus[] = {USB_OTG_BUS};
	//#define NUM_BUS sizeof(bus)/sizeof(bus[0])
	// Only first bus is used for USB OTG
	#define NUM_BUS 1
#endif

	for( i=0; i< NUM_BUS; i++ ) {
		sprintf(otg_cmd, "sys -otg st %d 2> /dev/null", bus[i]);
		FILE *outf = popen(otg_cmd, "r");
		if (outf) {
			while ( fgets(otg_resp, RDB_VAR_SIZE-1, outf) ) {
				if (!strcmp(otg_resp, "Device is disconnected\n")) {
					new_st = DISCONNECTED;
				} else if (!strcmp(otg_resp, "Device is connected\n")) {
					new_st = CONNECTED;
				} else if (!strcmp(otg_resp, "USB port is disabled\n")) {
					new_st = DISABLED;
				} else {
					new_st = NO_STATUS; //Mode changing or undefined
				}

				#ifdef V_IOBOARD_falcon // to hide intermediate disabled state between connected and disconnected state.
				if(new_st == DISABLED) {
					intermediate_disabled_cnt++;
					if(intermediate_disabled_cnt > INTERMEDIATE_DELAY) {
						intermediate_disabled_cnt = 0;
					} else {
						new_st = old_st;
					}
				}
				else if(intermediate_disabled_cnt != 0) {
					intermediate_disabled_cnt=0;
				}
				#endif

				if ((old_st!= new_st) && (new_st != NO_STATUS)) {
					if (old_st != NO_STATUS) {
						sprintf((char *)&event_noti_cmd[0], "elogger %d \"USB OTG status changed : %s --> %s\"&", USB_OTG_CONNECTION_EVENT, status_str[old_st], status_str[new_st]);
						event_notify();
					}
					old_st= new_st;
				}
			}
			pclose(outf);
		}
	}
	}
#endif

	// need to periodically call the debounce funcs as the timer usually expires between events
	for ( i = 0; i < evtCnt; i++  ) {
		evtHandlerList_type * pEvt = &dynEvtHandlerList[i];
		if (pEvt->ioConds) {
			do_debounce(pEvt);
		}
	}
}

const char *reg_con_tbl[] = {
	"Not registered",			/* 0 */
	"Registered to home network",		/* 1 */
	"Not registered",			/* 2 */
	"Not registered",			/* 3 */
	"Not registered",			/* 4 */
	"Registered to roaming network",	/* 5 */
};

static int reg_status_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;
	const char *last_reg = reg_con_tbl[atoi(pNameList->lastVal)];
	const char *curr_reg = reg_con_tbl[atoi(rdbVal)];
	syslog(LOG_DEBUG, "%s: last reg %s(%s)(%p), curr reg %s(%s)(%p)", __func__,
		last_reg, pNameList->lastVal, last_reg, curr_reg, rdbVal, curr_reg);
	if (strcmp(last_reg, curr_reg)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"WWAN registration status changed : %s --> %s\"&", pNameList->evtType, last_reg, curr_reg);
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}

static int cell_id_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;

	/* notify simply when value changes */
	if (strcmp(pNameList->lastVal, rdbVal)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"Cell ID changed : %s --> %s\"&", pNameList->evtType, pNameList->lastVal, rdbVal);
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}

typedef struct {
	const char *mobile_gen;
	const char *network_name;
} network_conv_tbl_type;
const network_conv_tbl_type network_conv_tbl[] = {
	{ "2G", "GSM"},
	{ "2G", "GSM Compact"},
	{ "2G", "GPRS"},
	{ "2G", "EGPRS"},
	{ "3G", "UMTS"},
	{ "3G", "E-UMTS"},
	{ "3G", "HSDPA"},
	{ "3G", "HSPA+"},
	{ "3G", "HSUPA"},
	{ "3G", "HSDPA/HSUPA"},
	{ "3G", "CDMA 1xRTT"},
	{ "3G", "CDMA 1xEVDO Release 0"},
	{ "3G", "CDMA 1xEVDO Release A"},
	{ "4G", "LTE"},
	{ NULL, NULL},
};
static const char* conv_to_mobile_gen(const char *sys_mode)
{
	const network_conv_tbl_type *p = &network_conv_tbl[0];
	while (p->network_name) {
		if (strcmp(p->network_name, sys_mode) == 0)
			return p->mobile_gen;
		p++;
	}
	return "N/A";
}

#ifndef V_SINGLE_NETWORK_y
static int wwan_network_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;
	const char *last_gen = conv_to_mobile_gen(pNameList->lastVal);
	const char *curr_gen = conv_to_mobile_gen(rdbVal);
	syslog(LOG_DEBUG, "%s: last gen %s, curr gen %s", __func__, last_gen, curr_gen);
	if (strcmp(last_gen, curr_gen)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"WWAN network changed : %s(%s) --> %s(%s)\"&",
			pNameList->evtType, last_gen, pNameList->lastVal, curr_gen, rdbVal);
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}
#endif

#ifndef V_POWER_SOURCES_none
static int power_src_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;
	/* notify simply when value changes */
	if (strcmp(pNameList->lastVal, rdbVal)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"Power source changed : %s --> %s\"&", pNameList->evtType, pNameList->lastVal, rdbVal);
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}
#endif

static int SD_card_status_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;

	// check whether NAS package is installed or not, if not simply return
	const char *p;
	char *token = NULL, *saveptr;
	p = rdb_getVal("system.package.installed.nas");
	if (!p || strcmp(p, "1")) {
		syslog(LOG_ERR, "NAS package is not installed, ignore SD card status change event");
		return 0;
	}
	syslog(LOG_ERR, "notify SD card status change event");

	/* notify simply when value changes */
	if (strcmp(pNameList->lastVal, rdbVal)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"SD card status changed: %s --> %s\"&", pNameList->evtType, pNameList->lastVal, rdbVal);
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}

#ifndef V_MULTIPLE_LANWAN_UI_none
static int failover_instance_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;
	char value[RDB_VAR_SIZE], last_value[RDB_VAR_SIZE];
	const char *p;
	sprintf((char *)rdb_name_buf, "%s.%s.dev", RDB_LINK_PROFILE, pNameList->lastVal);
	p = rdb_getVal(rdb_name_buf); strcpy(last_value, (p && strlen(p))? p:"N/A");
	sprintf((char *)rdb_name_buf, "%s.%s.dev", RDB_LINK_PROFILE, rdbVal);
	p = rdb_getVal(rdb_name_buf); strcpy(value, (p && strlen(p))? p:"N/A");
	// notify simply when value changes
	if (strcmp(pNameList->lastVal, rdbVal)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"WAN failover instance occured: %s --> %s\"&", pNameList->evtType, last_value, value);
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}
#endif

#ifdef V_HW_PUSH_BUTTON_SETTINGS_y
static int button_settings_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;

	/* notify simply when value changes */
	if (strcmp(pNameList->lastVal, rdbVal)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"Reset button is %s\"&", pNameList->evtType, strcmp(rdbVal, "1") ? "enabled" : "disabled");
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}
#endif

#ifdef V_VRRP_y
static int vrrp_mode_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;

	/* notify simply when value changes */
	if (strcmp(pNameList->lastVal, rdbVal)) {
		sprintf((char *)&event_noti_cmd[0], "elogger %d \"VRRP is in %s mode\"&", pNameList->evtType, strcmp(rdbVal, "3") ?  "backup" : "master");
		event_notify();
		return 1;
	}
	else {
		return 0;
	}
}
#endif

/*
 * This is hopefully generic enough to be used for any event. All that's required is for the handler to
 * set lastState and then call this function and for the debounce field of pNameList to be set
 */
static int do_debounce(const evtHandlerList_type *pEvt)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;
	struct timespec now;
	debounce_params_type *debounce = &pEvt->ioConds->debounce;
	if (!debounce)
		return 0;

	// may have already acted on this
	if (debounce->lastState == debounce->lastDir)
		return 0;

	// not all of our toolchains have the (faster) _COARSE clock. Use it if available.
#ifdef CLOCK_MONOTONIC_COARSE
	clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
#else
	clock_gettime(CLOCK_MONOTONIC, &now);
#endif
	int needms = (debounce->lastState)? debounce->confHighMS : debounce->confLowMS;
	int havems = (now.tv_sec - debounce->timeEdge.tv_sec) * 1000;
	havems += (int) ((now.tv_nsec - debounce->timeEdge.tv_nsec) /1000000);
	syslog(LOG_DEBUG, "%s(Event %d, Instance %d) is %d>=%d?", __func__, pNameList->evtType,
		pNameList->instance, havems, needms);
	if ( havems >= needms) {
		debounce->lastDir = debounce->lastState;
		// if this edge is configured to trigger noti - see struct def
		if ((debounce->confEdges >> debounce->lastState) & 1) {
			if (pNameList->evtType == DIGITAL_IN_CHANGED) {
				sprintf((char *)&event_noti_cmd[0], "elogger %d \"IO pin %d input now %s\"&",
					pNameList->evtType, pNameList->instance, debounce->lastState?"high":"low");
			} else if (pNameList->evtType == DIGITAL_OUT_CHANGED) {
				sprintf((char *)&event_noti_cmd[0], "elogger %d \"IO pin %d output now %s\"&",
					pNameList->evtType, pNameList->instance, debounce->lastState?"high":"low");
			} else {
				sprintf((char *)&event_noti_cmd[0], "elogger %d \"IO pin %d now %s\"&",
					pNameList->evtType, pNameList->instance, debounce->lastState?"high":"low");
			}
			event_notify();
			return 1;
		}
	}
	return 0;
}

/*
 * Handler for IO - this uses debouncing so most RDB updates only set lastState without triggering elogger.
 * rdbVal == NULL on initial call at startup (lastVal is set instead)
 */
static int io_change_evt_handler(const evtHandlerList_type *pEvt, const char *rdbVar, const char *rdbVal)
{
	const rdbNameList_type *pNameList = &pEvt->rdbNameList;
	ioConds_type *cond = pEvt->ioConds;
	if (!cond)
		return 0;
	unsigned state = 0;
	float val = atof((rdbVal != NULL)? rdbVal : pNameList->lastVal);
	if (pNameList->evtType == ANALOGUE_IN_THRESHOLD) {
		if (val >= cond->confHighThresh) {
			state = 1;
/* not needed because of default value:
		} else if (val <= cond->confLowThresh) {
			state = 0
*/
		} else if (val > cond->confLowThresh) {
			// in dead band - go back to previous state
			state = cond->debounce.lastDir;
		}
	} else if (val != 0) {
		// i.e. if digital and high
		state = 1;
	}

	if (state == cond->debounce.lastState) {
		return 0;
	}

	if (rdbVal == NULL) {
		// set initial state to avoid trigger at daemon start
		cond->debounce.lastDir = cond->debounce.lastState = state;
		return 0;
	}

	/*
	 * It's possible that input changed again after the required time but
	 * before the polling sends the notification. In that case it has to be
	 * done now before recording the new state
	 */
	int res = do_debounce(pEvt);

	// now record new state ready for next poll
	cond->debounce.lastState = state;
	// not all of our toolchains have the (faster) _COARSE clock. Use it if available.
#ifdef CLOCK_MONOTONIC_COARSE
	clock_gettime(CLOCK_MONOTONIC_COARSE, &cond->debounce.timeEdge);
#else
	clock_gettime(CLOCK_MONOTONIC, &cond->debounce.timeEdge);
#endif
	return res;
}

static void process_triggered_event(const char *rdbVar, const char *rdbVal)
{
	int i;
	for ( i = 0; i < evtCnt; i++  ) {
		evtHandlerList_type * pEvt = &dynEvtHandlerList[i];
		const char* szVariable=pEvt->rdbPath;
		/* find only triggering variables in the table */
		if (pEvt->rdbNameList.subscribe == SUB_NO || strcmp(szVariable, rdbVar)) {
			continue;
		}
		syslog(LOG_DEBUG, "found triggered '%s' in the table, invoke event handler", rdbVar);

		p_event_handler_type pfn = (p_event_handler_type)pEvt->p_event_handler;
		pfn(pEvt, rdbVar, rdbVal);
		(void) strcpy((char *)&pEvt->rdbNameList.lastVal[0], rdbVal);
		return;
	}
	syslog(LOG_ERR, "failed to find triggered '%s' in event handler table, throw away", rdbVar);
}

static int process_triggered_rdb(void)
{
	char names[RDB_TRIGGER_NAME_BUF];
	char value[RDB_VAR_SIZE];
	int names_len;
	char *rdbVar, *rdbVal=&value[0];
	char *token, *saveptr;

	names_len=sizeof(names);
	if( rdb_getnames(rdb,"",names,&names_len,TRIGGERED)<0 ) {
		syslog(LOG_ERR,"rdb_get_names() failed - %s",strerror(errno));
		goto err;
	}
	names[names_len]=0;

	syslog(LOG_DEBUG,"---------------------------------------------------------------");
	syslog(LOG_DEBUG,"rdbvar notify list - %s",names);
	token=NULL;
	while( (token=strtok_r(!token?names:NULL,"&",&saveptr))!=NULL ) {
		rdbVar=token;
		strcpy(rdbVal, rdb_getVal(rdbVar));
		process_triggered_event(rdbVar,rdbVal);
	}
	return 0;
err:
	return -1;
}

int rdbfd=-1;
int running=1;
int verbosity=0;
int poll_due=0;

static void alarm_handler(int signum)
{
	poll_due = 1;
}

typedef void (*pInstanceDataFn)(ioConds_type *pIO, const char *data);

void set_confLowMS(ioConds_type *pIO, const char *data) {
	int ms = atoi(data);
	pIO->debounce.confLowMS = ms <= 500 ? 500: ms;
}
void set_confHighMS(ioConds_type *pIO, const char *data) {
	int ms = atoi(data);
	pIO->debounce.confHighMS = ms <= 500 ? 500: ms;
}
void set_confEdges(ioConds_type *pIO, const char *data) {
	pIO->debounce.confEdges = atoi(data);
}
void set_confHighThresh(ioConds_type *pIO, const char *data) {
	pIO->confHighThresh = atof(data);
}
void set_confLowThresh(ioConds_type *pIO, const char *data) {
	pIO->confLowThresh = atof(data);
}

static void applyInstanceData( const char * prefix, const char * suffix, ioConds_type **pIoConds, int maxtok, pInstanceDataFn fn ) {
	char * varval = varval = rdb_getVal(rdb_name(prefix, suffix));
	char *tok = NULL, *saveptr;
	tok=(varval)?strtok_r(varval,";",&saveptr):NULL;
	int tokens;
	for( tokens = 0; tokens < maxtok && tok!=NULL; tokens++ ) {
		if (pIoConds[tokens] != NULL) {
			fn(pIoConds[tokens], tok);
		}
		tok=strtok_r(NULL,";",&saveptr);
	}
}
/*
 * This function theoretically applies to all event types, though right now only IO needs it.
 * Load params from service.eventnoti.conf.type.<num>.*
 * It tries to only read the ones it needs, based on event type.
 * Some may be arrays separated by ; - these correspond to the order
 * in service.eventnoti.conf.type.<num>.instances, though exactly
 * what an "instance" actually means also depends on event type.
 */
static void load_event_params(event_type_enum event)
{
	char *tok = NULL, *saveptr;
	char prefix[RDB_VARIABLE_NAME_MAX_LEN];
	int max_io = evtCnt - ioEvtBase;
	if ( 0 == max_io )
		return;

	sprintf(prefix, "%s.%d", RDB_EVENT_NOTI_TYPE_PREPIX, event);
	char *varval = rdb_getVal(rdb_name(prefix, "instances"));
	if (!varval)
		return;

	ioConds_type **pIoConds = (ioConds_type **)calloc( max_io, sizeof(ioConds_type *));
	if (!pIoConds)
		return;

	int tokens = 0;
	while( tokens < max_io && (tok=strtok_r(!tok?varval:NULL,";",&saveptr))!=NULL ) {
		int ioPin = atoi(tok);
		if (ioPin > 0 && ioPin <= max_io) {
			/* only proceed if this event handler's event type matches the provided one */
			evtHandlerList_type *pEvtHandler = &dynEvtHandlerList[ioEvtBase+ioPin-1];
			if (pEvtHandler && pEvtHandler->rdbNameList.evtType == event) {
				pIoConds[tokens] = pEvtHandler->ioConds;
			}
		}
		/*
		 * if the number is invalid we still need to keep a spot so the matching
		 * entry in the other options is ignored (actually it's treated as 0 for now)
		 */
		tokens++;
	}
	int maxtok = tokens;

	applyInstanceData( prefix,"debounce.highms", pIoConds, maxtok, set_confHighMS );
	applyInstanceData( prefix, "debounce.lowms", pIoConds, maxtok, set_confLowMS );
	applyInstanceData( prefix, "debounce.edges", pIoConds, maxtok, set_confEdges );

	if (event == ANALOGUE_IN_THRESHOLD) {
		applyInstanceData( prefix,"thresh.highthresh", pIoConds, maxtok, set_confHighThresh );
		applyInstanceData( prefix, "thresh.lowthresh", pIoConds, maxtok, set_confLowThresh );
	}
	free(pIoConds);
}

// Initialize a run time event object from the definition object
static void initDynEvt( evtHandlerList_type *pEvt, evtHandlerDef_type *pEvtDef ) {
	pEvt->rdbPath = strdup( rdb_name(pEvtDef->rdbNameDef.szPrefix, pEvtDef->rdbNameDef.szName) );
	syslog(LOG_DEBUG, "Init %s ", pEvt->rdbPath );
	pEvt->rdbNameList.evtType=pEvtDef->rdbNameDef.evtType;
	pEvt->rdbNameList.instance=pEvtDef->rdbNameDef.instance;
	pEvt->rdbNameList.subscribe=pEvtDef->rdbNameDef.subscribe;
	pEvt->rdbNameList.debounce=pEvtDef->rdbNameDef.debounce;
	pEvt->p_event_handler=pEvtDef->p_event_handler;
	pEvt->ioConds = 0;
}

// Scan the XAUX RDB values adding them to our events ( resizing array as we go )
static void add_IO(size_t allocSize) {
	int i;
	for ( i = 1; i < 99; i++  ) { // 99 is a safe guard, loop will normally exit due to lack of RDBs
		char rdbName[128];
		evtHandlerDef_type evtHandler;
		sprintf(rdbName, RDB_IO_PREFIX ".xaux%d.mode",  i );
		const char *p=rdb_getVal(rdbName);
		if (!p)
			break;
		syslog(LOG_DEBUG, "XAUX %s %s", rdbName, p);
		if ( 0 == strcmp(p, "digital_output")) {
			sprintf(rdbName, "xaux%d.d_out",  i );
			evtHandler.rdbNameDef.evtType = DIGITAL_OUT_CHANGED;
		}
		else if ( 0 == strcmp(p, "analogue_input")) {
			sprintf(rdbName, "xaux%d.adc",  i );
			evtHandler.rdbNameDef.evtType = ANALOGUE_IN_THRESHOLD;
		}
		else if ( 0 == strcmp(p, "virtual_digital_input")||( 0 == strcmp(p, "digital_input")) ) {
			sprintf(rdbName, "xaux%d.d_in",  i );
			evtHandler.rdbNameDef.evtType = DIGITAL_IN_CHANGED;
		}
		else
			continue;
		evtHandler.p_event_handler =  io_change_evt_handler;
		evtHandler.rdbNameDef.szPrefix = RDB_IO_PREFIX;
		evtHandler.rdbNameDef.szName = rdbName;
		evtHandler.rdbNameDef.instance = i;
		evtHandler.rdbNameDef.subscribe = SUB_YES;
		allocSize += sizeof(evtHandlerList_type);
		evtHandlerList_type * bigger_dynEvtHandlerList = (evtHandlerList_type *)realloc(dynEvtHandlerList, allocSize);
		if (!bigger_dynEvtHandlerList) {
			syslog(LOG_ERR,"Could not allocate more memory for dynEvtHandlerList");
			break;
		}
		dynEvtHandlerList = bigger_dynEvtHandlerList;
		initDynEvt( &dynEvtHandlerList[evtCnt], &evtHandler);
		dynEvtHandlerList[evtCnt].ioConds =  (ioConds_type *)calloc( 1, sizeof(ioConds_type));
		evtCnt++;
	}
	/* events come too fast to read IO params each time */
	load_event_params(DIGITAL_IN_CHANGED);
	load_event_params(ANALOGUE_IN_THRESHOLD);
	load_event_params(DIGITAL_OUT_CHANGED);
}

static int init(int be_daemon)
{
	size_t allocSize;
	int i;

	if (be_daemon) {
		daemon_init(APPLICATION_NAME, NULL, 0, (verbosity? LOG_DEBUG:LOG_INFO));
		syslog(LOG_INFO, "daemonized");
	} else {
		openlog(APPLICATION_NAME ,LOG_PID | LOG_PERROR, LOG_LOCAL5);
		setlogmask(LOG_UPTO(verbosity? LOG_DEBUG:LOG_INFO));
	}

	#ifdef LOG_LEVEL_CHECK
	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"loglevel check");
	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"syslog LOG_ERR");
	syslog(LOG_INFO,"syslog LOG_INFO");
	syslog(LOG_DEBUG,"syslog LOG_DEBUG");
	#endif

	syslog(LOG_INFO, "initializing...");
	if (be_daemon) {
		// this call never returns if not a daemon
		ensure_singleton(APPLICATION_NAME, LOCK_FILE);
	}

	/* signal handler set */
	signal(SIGHUP, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGCHLD, sig_handler);
	signal(SIGALRM, alarm_handler);

	if(rdb_init()<0) {
		syslog(LOG_ERR,"rdb_init() failed");
		goto err;
	}

	rdbfd=rdb_fd(rdb);

	// Transfer the static definitions to the runtime structure
	evtCnt = sizeof(evtHandlerList)/sizeof(evtHandlerList[0]);
	allocSize = evtCnt * sizeof(evtHandlerList_type);
	dynEvtHandlerList = (evtHandlerList_type *)malloc(allocSize);
	if (!dynEvtHandlerList ) {
		syslog(LOG_ERR,"Could not allocate memory for dynEvtHandlerList");
		return -1;
	}
	for ( i = 0; i < evtCnt; i++  ) {
		initDynEvt( &dynEvtHandlerList[i], &evtHandlerList[i]);
	}

	ioEvtBase = evtCnt;
	add_IO(allocSize);

	for ( i = 0; i < evtCnt; i++  ) {
		evtHandlerList_type * pEvt = &dynEvtHandlerList[i];
		const char *rdbVar = pEvt->rdbPath;
		const char *p=rdb_getVal(rdbVar);
		if (!p) {
			syslog(LOG_DEBUG, "create '%s'", rdbVar);
			if (rdb_create_string(rdb,rdbVar,"",0,0)<0) {
				syslog(LOG_ERR, "failed to create '%s'", rdbVar);
				continue;
			}
		} else {
			syslog(LOG_DEBUG, "already exist '%s'", rdbVar);
		}

		if (pEvt->rdbNameList.subscribe) {
			if( rdb_subscribe(rdb,rdbVar)<0 ) {
				syslog(LOG_ERR,"failed to subscribe rdb variable(%s) - %s",rdbVar,strerror(errno));
			} else {
				syslog(LOG_DEBUG, "subscribed '%s'", rdbVar);
			}
		}
		(void) strcpy(pEvt->rdbNameList.lastVal, rdb_getVal(rdbVar));

		syslog(LOG_DEBUG, "call trigger function for initial set");
		p_event_handler_type pfn = (p_event_handler_type)pEvt->p_event_handler;
		if (pEvt->rdbNameList.subscribe == SUB_YES) {
			char value[RDB_VAR_SIZE];
			strcpy(value, rdb_getVal(rdbVar));
			pfn(pEvt, rdbVar, value);
		}
		else if (pEvt->rdbNameList.subscribe == SUB_MAYBE) {
			pfn(pEvt, NULL, NULL);
		}
	}

	syslog(LOG_INFO, "initialized...");
	return 0;
err:
	if (be_daemon) {
		release_resources(LOCK_FILE);
	}
	return -1;
}

static int print_usage()
{
	fprintf(stderr,
		"Usage: "APPLICATION_NAME" [-v] [-V]\n"

		"\n"
		"Options:\n"

		"\t-v verbose mode (debugging log output)\n"
		"\t-V Display version information\n"
		"\t-d don't detach from controlling terminal (don't daemonise)\n"
		"\n"
	);

	return 0;
}

int main(int argc,char* argv[])
{
	fd_set rfds;
	struct itimerval itv;
	int retval = 0, ret, be_daemon = 1;

	/* Parse Options */
	while ((ret = getopt(argc, argv, "vVdh")) != EOF) {
		switch (ret) {
			case 'v': verbosity=1; break;
			case 'V': fprintf(stderr, "%s: build date / %s %s\n",APPLICATION_NAME, __TIME__,__DATE__); return 0;
			case 'd': be_daemon = 0; break;
			case 'h':
			case '?': print_usage(argv); return 2;
		}
	}

	running = init(be_daemon) >= 0;

	/* set up a recurring SIGALRM */
	itv.it_value.tv_sec = itv.it_interval.tv_sec = 2;
	itv.it_value.tv_usec = itv.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &itv, NULL);

	/* select loop */
	while(running) {
		FD_ZERO(&rfds);
		FD_SET(rdbfd, &rfds);

		/*
		 * There is a race here with the signal handler, though it has no impact because
		 * it's extremely unlikely that the time between 2 select() calls would exceed
		 * the 2-second alarm interval and even if it did the next alarm will trip
		 * it anyway - we just miss one poll.
		 * If that ever changes pselect() or timerfd_create() might be a better option.
		 */
		retval = select(rdbfd+1, &rfds, NULL, NULL, NULL);
		/* call again if interrupted by signal, or exit if error condition */
		if ((retval<0) && (errno!=EINTR)) {
			syslog(LOG_ERR,"select() failed with %d - %s", retval, strerror(errno));
			goto err;
		}
		/* Set by SIGALRM handler every 2 seconds */
		if (poll_due) {
			poll_due = 0;
			polling_non_trigger_events();
		}

		/* If select() errno==EINTR - i.e. ignore signals */
		if (retval <= 0)
			continue;

		/* give the execution to rdbnoti if rdb triggered */
		if(FD_ISSET(rdbfd,&rfds)) {
			process_triggered_rdb();
		}
	}

	if ( dynEvtHandlerList ) {
		int i;
		for ( i = 0; i < evtCnt;i++) {
			evtHandlerList_type * pEvt = &dynEvtHandlerList[i];
			if ( pEvt->ioConds )
				free( pEvt->ioConds );
			if ( pEvt->rdbPath )
				free( pEvt->rdbPath );
		}
		free(dynEvtHandlerList);
	}

	/* finish daemon */
	fini(LOCK_FILE);
	return 0;
err:
	return -1;
}
