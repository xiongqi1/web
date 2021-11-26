/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/times.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "cdcs_syslog.h"

#include "rdb_ops.h"
#include "at/at.h"
#include "daemon.h"
#include "model/model.h"
#include "rdb_names.h"
#include "util/rdb_util.h"
#include "util/scheduled.h"
#include "models/suppl.h"
#include "util/cfg_util.h"
#include "sms/sms.h"
#include "plmn-sel/plmn.h"

#include "featurehash.h"

#ifdef GPS_ON_AT
#include <curl/curl.h>
#endif

#include "logmask.h"

/* Version Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD	1

#define APPLICATION_NAME "simple_at_manager"

/* The user under which to run */
#define RUN_AS_USER "daemon"

#ifdef FORCED_REGISTRATION
// Global variable to handle forced registration
int forced_registration=0;
#endif

static BOOL port_shared = FALSE; // quick and dirty
volatile int running = 1;
volatile BOOL sms_disabled = FALSE;
/* change default SMS mode to PDU for concatenated SMS Tx.
 * Sending concatenated SMS is only possible in PDU mode */
volatile BOOL pdu_mode_sms = TRUE;
volatile BOOL ira_char_supported = FALSE;
volatile BOOL numeric_cmgl_index = FALSE;

const struct {
	const char* szPrefix;
	const char* szName;
	const BOOL share_with_pots_bridge;
	const int subscribe;
	BOOL basic_atmgr_vars;
} _rdbNameList[] = {
	{ RDB_UMTS_SERVICES, "clir.subscriber_status", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES, "clip.subscriber_status", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES, "command.result_name", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".clip", "status", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".clip", "number", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".clir", "status", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".calls", "list", 1 ,0, TRUE },
	{ RDB_UMTS_SERVICES".calls", "event", 1 ,0, TRUE },
	{ RDB_UMTS_SERVICES".call_waiting.status", "", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".call_forwarding.unconditional", "", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".call_forwarding.busy", "", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".call_forwarding.no_reply", "", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".call_forwarding.not_reachable", "", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".call_barring.OI", "", 0 ,0, TRUE },
	{ RDB_UMTS_SERVICES".command", "", 1 ,1, TRUE },
	{ RDB_UMTS_SERVICES".command.last", "", 1 ,0, TRUE },
	{ RDB_UMTS_SERVICES".command.status", "", 1 ,0, TRUE },
	{ RDB_PHONE_SETUP".command", "", 1 ,1, TRUE },
	{ RDB_PHONE_SETUP".command.status", "", 1 ,0, TRUE },
	{ RDB_PHONE_SETUP".command.last", "", 1 ,0, TRUE },
	{ RDB_PHONE_SETUP".command.response", "", 1 ,0, TRUE },
	{ RDB_SIGNALSTRENGTH, "", 0 ,0, FALSE },
	{ RDB_SIGNALSTRENGTH, "bars", 0 ,0, FALSE },
	{ RDB_SIGNALSTRENGTH, "raw", 0 ,0, FALSE },
	{ RDB_NETWORKNAME, "", 0 ,0, TRUE },
	{ RDB_NETWORKATTACHED, "", 0 ,0, TRUE },
 	{ RDB_NETWORKREGISTERED, "", 0 ,0, TRUE },
	{ RDB_NETWORKREG_STAT, "", 0,0, TRUE},
 	{ RDB_NETWORKSIMREADY, "", 0 ,0, TRUE },
	{ RDB_PDP0STAT, "", 0 ,0, FALSE },
	{ RDB_PDP1STAT, "", 0 ,0, FALSE },
	{ RDB_SERVICETYPE, "", 0 ,0, TRUE },
	{ RDB_SERVICETYPE_EXT, "", 0 ,0, TRUE },
	{ RDB_PLMNLIST, "", 0 ,0, TRUE },
	{ RDB_PLMNSTATUS, "", 0 ,0, TRUE },
	{ RDB_PLMNMODE, "", 0 ,0, TRUE },
	{ RDB_MODEL, "", 0 ,0, TRUE },
	{ RDB_IMEI, "", 0 ,0, TRUE },
	{ RDB_SERIAL, "", 0 ,0, TRUE },
	{ RDB_MANUFACTURER, "", 1 ,0, TRUE },
	{ RDB_FIRMWARE_VERSION, "", 0 ,0, TRUE },
	{ RDB_FIRMWARE_VERSION_CID, "", 0 ,0, TRUE },
	{ RDB_HARDWARE_VERSION, "", 0 ,0, FALSE },
	{ RDB_SIM_STATUS, "", 0 ,0, TRUE },
	{ RDB_IMSI".plmn_mcc", "", 0 ,0, TRUE },
	{ RDB_IMSI".plmn_mnc", "", 0 ,0, TRUE },
	{ RDB_IMSI".msin", "", 0 ,0, TRUE },
	{ RDB_CURRENTBAND, "", 0 ,0, TRUE },
	{ RDB_MODULEBANDLIST, "", 0 ,0, TRUE },
	{ RDB_MODULERATLIST, "", 0 ,0, TRUE },
	{ RDB_PDPSTATUS, "", 0 ,0, FALSE },
	{ RDB_BANDPARAM, "", 0 ,0, TRUE },
	{ RDB_BANDPARAM_RAT, "", 0 ,0, TRUE },
	{ RDB_BANDCMMAND, "", 0 ,1, TRUE },
	{ RDB_BANDCFG, "", 1 ,0, TRUE },
 

	{ RDB_PROFILE_CMD, "", 1 ,1, TRUE },
	{ RDB_PROFILE, "msg", 1 ,0, TRUE },
	{ RDB_PROFILE_STAT, "", 1 ,0, TRUE },
	{ RDB_PROFILE_APN, "", 1 ,0, TRUE },
	{ RDB_PROFILE_USER, "", 1 ,0, TRUE },
	{ RDB_PROFILE_PW, "", 1 ,0, TRUE },
	{ RDB_PROFILE_STAT, "", 0 ,0, TRUE },
	{ RDB_PROFILE_UP, "", 0 ,0, TRUE },

	{ RDB_BANDSTATUS, "", 0 ,0, TRUE },
 
	{ RDB_BANDCURSEL, "", 0 ,0, TRUE },
	{ RDB_RATCURSEL, "", 0 ,0, TRUE },
	{ RDB_PLMNCOMMAND, "", 0 ,1, TRUE },
	{ RDB_PLMNCURLIST, "", 0 ,0, TRUE },
	{ RDB_PLMNSTAT, "", 0 ,0, TRUE },
	{ RDB_PLMNSEL, "", 1 ,0, TRUE },
	{ RDB_PLMNSELMODE, "", 0 ,0, TRUE },
	{ RDB_PLMNMNC, "", 0 ,0, TRUE },
	{ RDB_PLMNMCC, "", 0 ,0, TRUE },
	{ RDB_ROAMING, "", 0 ,0, TRUE },
	{ RDB_PLMNCURMODE, "", 0 ,0, TRUE },
	{ RDB_PLMNSYSMODE, "", 0 ,0, TRUE },
	{ RDB_SIMPARAM, ", 0" ,0, TRUE },
	{ RDB_SIMCMMAND, "", 0 ,1, TRUE },
	{ RDB_SIMCMDSTATUS, "", 0 ,0, TRUE },
	{ RDB_SIMMEPCMMAND, "", 0 ,1, TRUE },
 	{ RDB_SIM_MEPLOCK, "", 0 ,0, TRUE },
	{ RDB_SIMPINENABLED, "", 0 ,0, TRUE },
	{ RDB_SIMMCCMNC, "", 0 ,0, TRUE },
	{ RDB_SIMNEWPIN, "", 0 ,0 , TRUE},
	{ RDB_SIMPUK, "", 0 ,0, TRUE },
	{ RDB_SIMRETRIES, "", 0 ,0, TRUE },
#if defined(PLATFORM_BOVINE) || defined(PLATFORM_ANTELOPE)
	{ RDB_SIMRESULTOFUO, "", 0 ,0, TRUE },
#endif
	{ RDB_SIMPUKREMAIN, "", 0 ,0, TRUE },
	{ RDB_SIMCPINC, "", 0 ,0, TRUE },
	{ RDB_AUTOPIN_TRIED, "", 0 ,0, TRUE },

	{ RDB_SMS_VOICEMAIL_STATUS, "", 1 ,0, TRUE },
	{ RDB_SMS_STATUS, "", 1 ,0, TRUE },
	{ RDB_SMS_STATUS2, "", 1 ,0, TRUE },
	{ RDB_SMS_STATUS3, "", 0 ,0, TRUE },
	{ RDB_SMS_CMD_ID, "", 0 ,0, TRUE },
	{ RDB_SMS_CMD_TO, "", 0 ,0, TRUE },
	{ RDB_SMS_CMD_ST, "", 0 ,0, TRUE },
	{ RDB_SMS_CMD, "", 0 ,1, TRUE },
	{ RDB_SMS_CMD_MSG, "", 0 ,0, TRUE },
	{ RDB_SMS_RD_DST, "", 0 ,0, TRUE },
	{ RDB_SMS_RD_SMSC, "", 0 ,0, TRUE },
	{ RDB_SMS_RD_TIME, "", 0 ,0, TRUE },
	{ RDB_SMS_RD_RXTIME, "", 0 ,0, TRUE },
	{ RDB_SMS_RD_MSG, "", 0 ,0, TRUE },
	{ RDB_SMS_SMSC_ADDR, "", 1 ,0, TRUE },
	{ RDB_SMS_TX_CONCAT_EN, "", 0 ,0, TRUE },

	{ RDB_SIM_DATA_MBN, "", 0 ,0, TRUE },
	{ RDB_SIM_DATA_MBDN, "", 0 ,0, TRUE },
	{ RDB_SIM_DATA_MSISDN, "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".simICCID", "", 0 ,0, TRUE},
#ifndef PLATFORM_AVIAN
	{ RDB_MODULE_BOOT_VERSION, "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".PSCs0", "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".LAC", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".RAC", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".CellID", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".ChannelNumber", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".RSCPs0" , "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".ECIOs0", "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".ECN0s0", "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".ECN0s1", "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".ECN0_valid", "", 1 ,0, FALSE },
	{ RDB_NETWORK_STATUS".PRIID_REV", "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".PRIID_PN", "", 0 ,0, FALSE },
	{ RDB_NETWORK_STATUS".RRC", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".RAT", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".CGATT", "", 0 ,0, TRUE },
	{ RDB_RX_LEVEL"0", "", 0 ,0, TRUE },
	{ RDB_RX_LEVEL"1", "", 0 ,0, TRUE },
#ifndef V_CELL_NW_cdma
	{ RDB_RSCP"0", "", 0 ,0, TRUE },
	{ RDB_RSCP"1", "", 0 ,0, TRUE },
#endif

 	/* LTE information */
	{ RDB_RSRP0, "", 0 ,0, TRUE },
	{ RDB_RSRP1, "", 0 ,0, TRUE },
	{ RDB_RSRQ, "", 0 ,0, TRUE },
	{ RDB_TAC, "", 0 ,0, TRUE },
	{ RDB_PSC, "", 0 ,0, TRUE },

	{ RDB_ECIO"0", "", 0 ,0, TRUE },
	{ RDB_ECIO"1", "", 0 ,0, TRUE },
	{ RDB_HSUCAT, "", 0 ,0, TRUE },
	{ RDB_HSDCAT, "", 0 ,0, TRUE },
	{ RDB_MODULETEMPERATURE, "", 0 ,0, TRUE },
	{ RDB_MODULEVOLTAGE, "", 0 ,0, TRUE },
#endif
	{ RDB_NETWORK_STATUS".hint", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".hint.encoded", "", 0 ,0, TRUE },
	{ RDB_NETWORK_STATUS".CGATT", "", 0 ,0, TRUE },

	// ipw ums
	{ RDB_UMS".command", "", 0 ,1, TRUE },
	{ RDB_UMS".status", "", 0 ,0, TRUE },
	{ RDB_UMS".msg", "", 0 ,0, TRUE },
	{ RDB_UMS".enable", "", 0 ,0, TRUE },
	{ RDB_UMS".ipaddr", "", 0 ,0, TRUE },
	{ RDB_UMS".name", "", 0 ,0, TRUE },
	{ RDB_UMS".port", "", 0 ,0, TRUE },
	{ RDB_UMS".timeout", "", 0 ,0, TRUE },

	// lstatus
	{ RDB_LSTATUS".command", "", 0 ,1, TRUE },
	{ RDB_LSTATUS".msg", "", 0 ,0, TRUE },
	{ RDB_LSTATUS".status", "", 0 ,0, TRUE },

	// ipw frequency selection
	{ RDB_FREQ, "command", 0 ,1, TRUE },
	{ RDB_FREQ, "status", 0 ,0, TRUE },
	{ RDB_FREQ, "msg", 0 ,0, TRUE },
	{ RDB_FREQ, "band", 0 ,0, TRUE },

	// ipw status
	{ RDB_NETWORK, "areacode", 0 ,0, TRUE },
	{ RDB_NETWORK, "cellid", 0 ,0, TRUE },
	{ RDB_NETWORK, "txpwr", 0 ,0, TRUE },
	{ RDB_NETWORK, "lstatus_er7", 0 ,0, TRUE },
	{ RDB_NETWORK, "lstatus_r99", 0 ,0, TRUE },
	{ RDB_PLMNID, "", 0,0, TRUE },
	{ RDB_WANFREQ, "", 0,0, TRUE },
	{ RDB_SIGNALQUALITY, "", 0,0, TRUE },
	{ RDB_CHIPRATE, "", 0,0, TRUE },
	{ RDB_TOFFSET, "", 0,0, TRUE },
	{ RDB_REGSTATUS, "", 0,0, TRUE },

	{ RDB_HEART_BEAT, "", 0,0, TRUE },
	{ RDB_HEART_BEAT2, "", 0,0, TRUE },

	{ RDB_PRL_VERSION, "", 0,0, TRUE },
	{ RDB_MDN, "", 0,0, TRUE },
	{ RDB_MSID, "", 0,0, TRUE },
	{ RDB_NAI, "", 0,0, TRUE },
	{ RDB_AVAIL_DATA_NETWORK, "", 0,0, TRUE },
	{ RDB_CONNECTION_STATUS, "", 0,0, TRUE },
	{ RDB_MODULE_ACTIVATED, "", 1,0, TRUE },

	{ RDB_CDMA_ROAMPREFERENCE, "", 0,0, TRUE },

	{ RDB_CDMA_SERVICEOPTION, "", 0,0 , TRUE },
	{ RDB_CDMA_SLOTCYCLEIDX, "", 0,0 , TRUE },
	{ RDB_CDMA_BANDCLASS, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTTCHANNEL, "", 0,0 , TRUE },
 	{ RDB_CDMA_1XEVDOCHANNEL, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTTPN, "", 0,0 , TRUE },
	{ RDB_CDMA_1XEVDOPN, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTT_RX_LEVEL, "", 0,0 , TRUE },
	{ RDB_CDMA_1XEVDO_RX_LEVEL, "", 0,0 , TRUE },
 
	{ RDB_CDMA_SYSTEMID, "", 0,0 , TRUE },
	{ RDB_CDMA_NETWORKID, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTTASET, "", 0,0 , TRUE },
	{ RDB_CDMA_EVDOASET, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTTCSET, "", 0,0 , TRUE },
	{ RDB_CDMA_EVDOCSET, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTTNSET, "", 0,0 , TRUE },
	{ RDB_CDMA_EVDONSET, "", 0,0 , TRUE },
	{ RDB_CDMA_DOMINANTPN, "", 0,0 , TRUE },
	{ RDB_CDMA_EVDODRCREQ, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTTRSSI, "", 0,0 , TRUE },
	{ RDB_CDMA_EVDORSSI, "", 0,0 , TRUE },
	{ RDB_ECIOS0, "", 0,0 , TRUE },
	{ RDB_CDMA_1XRTTPER, "", 0,0 , TRUE },
	{ RDB_CDMA_EVDOPER, "", 0,0 , TRUE },
	{ RDB_CDMA_TXADJ, "", 0,0 , TRUE },

	{ RDB_CDMA_MIPCMMAND, "", 0 ,1 , TRUE },
	{ RDB_CDMA_MIPCMDSTATUS, "", 0 ,0 , TRUE },
	{ RDB_CDMA_MIPCMDGETDATA, "", 0 ,0 , TRUE },
	{ RDB_CDMA_MIPCMDSETDATA, "", 0 ,0 , TRUE },
	{ RDB_SPRINT_MIPMSL, "", 0 ,0 , TRUE },
	{ RDB_SPRINT_CUR_MIP, "", 0 ,0 , TRUE },
	{ RDB_SPRINT_MIP_MODE, "", 0 ,0 , TRUE },

	{ RDB_CDMA_ADPARACMMAND, "", 0 ,1 , TRUE },
	{ RDB_CDMA_ADPARASTATUS, "", 0 ,0 , TRUE },
	{ RDB_CDMA_ADPARAERRCODE, "", 0 ,0 , TRUE },
	{ RDB_CDMA_ADPARABUFFER, "", 0 ,0 , TRUE },
	{ RDB_SPRINT_ADPARAMSL, "", 0 ,0 , TRUE },

	{ RDB_MODULE_RESETCMD, "", 0 ,1, TRUE },
	{ RDB_MODULE_RESETCMDSTATUS, "", 0 ,0, TRUE },

	{ RDB_MODULE_CONFCMD, "", 0 ,1, TRUE },
	{ RDB_MODULE_CONFSTATUS, "", 0 ,0, TRUE },
	{ RDB_MODULE_CONFCMDPARAM, "", 0 ,0, TRUE },
	{ RDB_MODULE_CONFSTATUSPARAM, "", 0 ,0, TRUE },

	{ RDB_NETIF_IPADDR, "", 0 ,0, TRUE },
	{ RDB_NETIF_GWADDR, "", 0 ,0, TRUE },
	{ RDB_NETIF_PDNSADDR, "", 0 ,0, TRUE },
	{ RDB_NETIF_SDNSADDR, "", 0 ,0, TRUE },

#ifdef USSD_SUPPORT
// USSD interface command
	{ RDB_USSD_CMD, "", 0 ,1, TRUE },
	{ RDB_USSD_CMD_STATUS, "", 0 ,0, TRUE },
	{ RDB_USSD_CMD_RESULT, "", 0 ,0, TRUE },
	{ RDB_USSD_MESSAGE, "", 0 ,0, TRUE },
#endif	/* USSD_SUPPORT */

	// dtmf key command
	{ RDB_DTMF_CMD, "", 1 ,1, TRUE },

#ifdef MHS_ENABLED
 	// MHS
	{"mhs","command",0,1,TRUE},
	{"mhs","hswlanphymode",0,0,FALSE},
	{"mhs","hswlanssid",0,0,FALSE},
	{"mhs","hswlanhiddenssid",0,0,FALSE},
	{"mhs","hswlanchan",0,0,FALSE},
	{"mhs","hswlanip",0,0,FALSE},
	{"mhs","hswlanmac",0,0,FALSE},
	{"mhs","hswlanencrip",0,0,FALSE},
	{"mhs","hswlanpassphr",0,0,FALSE},
	{"mhs","hswlanprotect",0,0,FALSE},
	{"mhs","hsrtrdhcpena",0,0,FALSE},
	{"mhs","hsrtriplow",0,0,FALSE},
	{"mhs","hsrtriphi",0,0,FALSE},
	{"mhs","hsrtrnetmask",0,0,FALSE},
	{"mhs","hsrtrpridns",0,0,FALSE},
	{"mhs","hsrtrsecdns",0,0,FALSE},
	{"mhs","hscustlastwifichan",0,0,FALSE},

	{"mhs","chargingonlymode",0,0,FALSE},
	{"mhs","wwan",0,0,FALSE},
	{"mhs","wps",0,0,FALSE},
#endif /* MHS_ENABLED */

	// Phone Module related
	{"module_temperature", "", 1, 0, TRUE},
	{RDB_DATE_AND_TIME, "", 0, 0, FALSE},
	{RDB_QXDM_COMMAND,"",0,1,TRUE},
	{RDB_QXDM_STAT,"",0,0,TRUE},
 
	{ RDB_NWCTRLCOMMAND, "", 0 ,1, TRUE },
	{ RDB_NWCTRLSTATUS, "", 0 ,0, TRUE },
	{ RDB_NWCTRLVALID, "", 0 ,0, TRUE },
 
 
#if defined(V_CELL_NW_cdma) || defined(MODULE_MC7354) || defined(MODULE_MC7304)
	{RDB_CDMA_OTASP_CMD,"",0,1,TRUE},	/* otasp command - otasp */
	{RDB_CDMA_OTASP_XX,"",0,0,TRUE},	/* otasp command param - subscriber */
	{RDB_CDMA_OTASP_STAT,"",1,0,TRUE},	/* otasp command - status */
	{RDB_CDMA_OTASP_PROGRESS,"",1,0,TRUE},	/* provisioning in progress - 1:programming, 0:idle */
	{RDB_CDMA_OTASP_SPC,"",1,0,TRUE},	/* spc code */

	{RDB_CDMA_OTASP_MDN,"",1,0,TRUE},	/* mdn to change */
	{RDB_CDMA_OTASP_MSID,"",1,0,TRUE},	/* msid to change */
	{RDB_CDMA_OTASP_NAI,"",1,0,TRUE},	/* nav to change */
#endif
 
#ifdef V_CELL_NW_cdma
	{RDB_CDMA_PREFSET_CMD ,"",0,1,TRUE},	/* pref mode set command - set, get */
	{RDB_CDMA_PREFSET_NETWORK_MODE,"",0,0,TRUE},
	{RDB_CDMA_PREFSET_ROAM_MODE,"",0,0,TRUE},
	{RDB_CDMA_PREFSET_STAT,"",0,0,TRUE},
#endif

	#if defined(MODULE_MC7354) || defined(MODULE_MC7304) 
 	/* MC7304 and MC7354 can do WCDMA and CDMA together */
	{RDB_CDMA_PREFSET_MIPINFO,"",0,0,TRUE},
	{RDB_PRLVER,"",0,0,TRUE},
	{RDB_MEID,"",0,0,TRUE},
	{RDB_ESN,"",0,0,TRUE},
	{RDB_PRIID_CARRIER,"",0,0,TRUE},
	{RDB_PRIID_CONFIG,"",0,0,TRUE},
	#endif

#ifdef GPS_ON_AT
    /* GPS command & variables */
	{RDB_GPS_PREFIX, RDB_GPS_CMD, 0, 1, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_CMD_TIMEOUT, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_DATE, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_TIME, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_LONG, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_LONG_DIR, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_LATI, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_LATI_DIR, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_GEOID, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_LOCUNCP, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_LOCUNCA, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_LOCUNCANGLE, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_HEPE, 0, 0, TRUE},
	{RDB_GPS_PREFIX, RDB_GPS_VAR_3D_FIX, 0, 0, TRUE},
 
	/* gpsOne parameters */
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_CAP,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_EN,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_URLS,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_UPDATE_NOW,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_UPDATED,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_PERIOD,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_RETRY_DELAY,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_MAX_RETRY_CNT,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_RETRIED,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_GNSS_TIME,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_VALID_TIME,1,0,TRUE},
	{RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME,1,0,TRUE},

#endif
#ifdef GPS
	{RDB_GPS_INITIALIZED, "", 1, 0, TRUE},
#endif
	{RDB_BAND_INITIALIZED,"", 1, 0, TRUE},

#ifdef HAS_VOICE_DEVICE
	{ RDB_POTS_CMD, "", 1 ,1, TRUE },
#endif

	{ RDB_SYSLOG_PREFIX, RDB_SYSLOG_MASK, 1 ,1, TRUE },

	{ RDB_CRIT_TEMP_STATUS, "", 1 ,0, TRUE },

	{ RDB_CPOL_PREF_PLMNLIST, "", 0 ,0, FALSE },
#ifdef FORCED_REGISTRATION
	{ "vf.force.registration", "", 0, 1, TRUE },
#endif
	{ 0,},
};

////////////////////////////////////////////////////////////////////////////////
void update_debug_level()
{
	char buf[128];
	int error_level;

	if(rdb_get_single(rdb_name("debug", ""), buf, sizeof(buf))>=0)
	{
		error_level=atoi(buf);
		setlogmask(LOG_UPTO(error_level + LOG_INFO));
	  	log_db[LOGMASK_AT].loglevel = (error_level + LOG_INFO);
		SYSLOG_ERR("error level changed to %d", error_level);
	}
	else
	{
		SYSLOG_INFO("no debugging database variable exists");
	}
}
////////////////////////////////////////////////////////////////////////////////
static void sig_handler(int signum)
{
	pid_t	pid;
	int stat;

	/* The rdb_library and driver have a bug that means that subscribing to
	variables always enables notfication via SIGHUP. So we don't whinge
	on SIGHUP. */
	if (signum != SIGHUP)
		SYSLOG_INFO("caught signal %d", signum);

	switch (signum)
	{
		case SIGHUP:
			_updateTestMode = !0;
			break;

		case SIGUSR1:
			update_debug_level();
			break;

		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			running = 0;
			break;

		case SIGCHLD:
			if ( (pid = waitpid(-1, &stat, WNOHANG)) > 0 )
				SYSLOG_INFO("Child %d terminated\n", pid);
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////
static void on_at_notification(const char* notification)
{
	if (model_notify(notification) != 0)
	{
		/* notification messages from PX8 have OK - ignore */
		if(!strcmp(notification,"OK"))
			return;
		
		SYSLOG_ERR("failed - %s",notification);
	}
}

////////////////////////////////////////////////////////////////////////////////
static int handle_at_event(void)
{
	while (1)
	{
		const char* r = at_readline();
		if (r == NULL)
			break;

		on_at_notification(r);
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int handle_rdb_event(const char* prefix, const char* name)
{
	int ret;
	char value[ RDB_COMMAND_MAX_LEN ];
	int ignored;
    char eventName[256];
    char lastName[256];

	struct name_value_t args[2];
	//rdb_database_lock(0);   // need to lock database, as otherwise we get spurious RDB events, if another process sets the command inside of the database lock
	ret = rdb_get_single(rdb_name(prefix, name), value, RDB_COMMAND_MAX_LEN);
	//rdb_database_unlock();
	if (ret != 0)
	{
		SYSLOG_ERR("failed to get '%s' (%s)", rdb_name(prefix, name), strerror(errno));
		return -1;
	}
	/* dtmf key command needs to be null to clear digits position */
	if (strcmp(value, "") == 0 && strcmp(name, RDB_DTMF_CMD) != 0)
	{
		return 0;
	}

    if (strcmp(prefix, RDB_GPS_PREFIX) == 0 || strcmp(prefix, RDB_SYSLOG_PREFIX) == 0) {
    	strcpy(eventName, rdb_name(prefix, name));
    } else {
    	if(!*name || *name=='.')
    		snprintf(eventName,sizeof(eventName),"%s%s",prefix,name);
    	else
    		snprintf(eventName,sizeof(eventName),"%s.%s",prefix,name);
    	eventName[sizeof(eventName)-1]=0;
    }

	args[0].name = eventName;
	args[0].value = value;
	args[1].name = NULL; // zero-terminator
	args[1].value = NULL; // zero-terminator
	if ((ret = model_run_command(args,&ignored)) != 0)
	{
		SYSLOG_ERR("'%s' '%s' failed", args[0].name, args[0].value);
	}
	sprintf(lastName,"%s.last",rdb_name(prefix, name));
	rdb_set_single(lastName, value);
	/* do not clear dtmf key buffer to keep polling */
	if (!ignored && (strcmp(eventName, RDB_DTMF_CMD) != 0))
		rdb_set_single(rdb_name(prefix, name), "");
	return ret;
}
////////////////////////////////////////////////////////////////////////////////
char last_dtmf_keys[AT_RESPONSE_MAX_SIZE] = {0,};
int dtmf_key_idx = 0;
extern BOOL empty_call_list;
int polling_dtmf_rdb_event(void)
{
	int ret;
	char value[ RDB_COMMAND_MAX_LEN ];
	struct name_value_t args[2];

	if (empty_call_list)
		return 0;

	empty_call_list = TRUE;		/* mark empty until next clcc command execution */

	ret = rdb_get_single(rdb_name(RDB_DTMF_CMD, ""), value, RDB_COMMAND_MAX_LEN);
	//SYSLOG_ERR("------------ polling dtmf keys : '%s' '%s'", rdb_name(RDB_DTMF_CMD, ""), value);
	if (ret != 0)
	{
		SYSLOG_ERR("failed to get '%s.%s' (%s)", wwan_prefix, RDB_DTMF_CMD, strerror(errno));
		return -1;
	}
	if (strcmp(last_dtmf_keys, value) == 0 && dtmf_key_idx == strlen(value))
	{
		//SYSLOG_ERR("same as previous command value. skip : %s", last_dtmf_keys);
		return 0;
	}

	SYSLOG_ERR("detect DTMF command (polling): '%s'", value);
	args[0].name = RDB_DTMF_CMD;
	args[0].value = value;
	args[1].name = NULL; // zero-terminator
	args[1].value = NULL; // zero-terminator
	if ((ret = model_run_command(args,0)) != 0)
	{
		SYSLOG_ERR("'%s' '%s' failed", args[0].name, args[0].value);
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////
static int handle_shared_port(BOOL force)
{
	static char last_value = '0';
	char value[64];

	int fPrevStat;
	int fCurStat;

	fPrevStat = at_get_fd() >= 0;

	// if force or not shared port
	if ( force || (!port_shared && at_get_fd() < 0) )
		return at_open();


	static int fOpenNow=0;
	static clock_t clkOpenNow;
	struct tms tmsbuf;
	clock_t clkNow=times(&tmsbuf);

	if(!fOpenNow)
	{
		clkOpenNow=clkNow;
	}
	else
	{
		clock_t clkPerSec=sysconf(_SC_CLK_TCK);

		if( (clkNow-clkOpenNow)/clkPerSec > 10 )
		{
			at_open();
			fOpenNow=0;
		}
	}


	//rdb_database_lock(0);   // need to lock database, as otherwise we get spurious RDB events, if another process sets the command inside of the database lock

	if (rdb_get_single(rdb_name("module.lock", ""), value, sizeof(value)) == 0 && *value)
	{
		if (last_value != value[0])
		{
			if (value[0] == '1')
			{
				at_close();
				fOpenNow=0;
			}
			else
			{
				//at_open();
				fOpenNow=1;
			}
			last_value = value[0];
			rdb_set_single(rdb_name("module.lock_result", ""), "1");
		}
	}
/*
	else
	{
		at_open();
	}
*/
	//rdb_database_unlock();


	fCurStat = at_get_fd() >= 0;

	if (!fPrevStat && fCurStat)
	{
		SYSLOG_INFO("reinitializing module");
		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");
	}

	return at_get_fd();
}

////////////////////////////////////////////////////////////////////////////////
static int init_shared_port(void)
{
	if (port_shared)
	{
		if (rdb_update_single(rdb_name("module.lock", ""), "", CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to update to '%s' - %s!", "module.lock", strerror(errno));
			return -1;
		}

		if (rdb_update_single(rdb_name("module.lock_result", ""), "", CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to update to '%s'!", "module.lock_result");
			return -1;
		}

		if (rdb_subscribe_variable(rdb_name("module.lock", "")) != 0)
		{
			SYSLOG_ERR("failed to subscribe to '%s'!", "module.lock");
			return -1;
		}
	}
	return handle_shared_port(TRUE);
}

////////////////////////////////////////////////////////////////////////////////
static int handle_rdb_events(void)
{
	int succ;

	const typeof(_rdbNameList[0])* pNameList=_rdbNameList;

	while(pNameList->szPrefix)
	{
		/* skip if the variable is updated by other port manager such as cnsmgr */
		if (!is_enabled_feature(FEATUREHASH_CMD_ALLCNS) && !pNameList->basic_atmgr_vars) {
			pNameList++;
			continue;
		}

		if(pNameList->subscribe)
		{
			if( (succ=handle_rdb_event(pNameList->szPrefix, pNameList->szName))!=0 )
				break;
		}

		pNameList++;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int create_rdb_variables(int fCreate)
{

	const typeof(_rdbNameList[0])* pNameList=_rdbNameList;

	while(pNameList->szPrefix) {
		const char* szVariable=rdb_name(pNameList->szPrefix, pNameList->szName);
		char s[128];

		/* skip if the variable is updated by other port manager such as cnsmgr */
		if (!is_enabled_feature(FEATUREHASH_CMD_ALLCNS) && !pNameList->basic_atmgr_vars) {
			pNameList++;
			continue;
		}

		if(fCreate)
		{
			if (rdb_get_single(szVariable, s, 128) != 0)
			{
				SYSLOG_DEBUG("create '%s'", szVariable);
				if(rdb_create_variable(szVariable, "", CREATE, ALL_PERM, 0, 0)<0)
				{
					SYSLOG_ERR("failed to create '%s'", szVariable);
				}
			}
			else
			{
				//SYSLOG_ERR("rdb var '%s' already exists", szVariable);

				// clear the previous command or values
				if(pNameList->share_with_pots_bridge == 0) {
					//SYSLOG_INFO("clear rdb var '%s'", szVariable);
					rdb_set_single(szVariable, "");
				}
			}
		}
		else if (pNameList->share_with_pots_bridge == 0)
		{
			/* Do not delete rdb variables which are registered to rdb_manager as triggering
			 * variables. Currently rdb manager is not smart to deal with this case!!!
			 */
			//if(rdb_delete_variable(szVariable)<0)
			//	SYSLOG_ERR("failed to delete '%s'", szVariable);

			rdb_set_single(szVariable, "");
		}

		pNameList++;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int init_rdb(void)
{
	if ((rdb_open_db()) < 0)
	{
		SYSLOG_ERR("failed to open RDB!");
		return -1;
	}

	create_rdb_variables(1);

	rdb_close_db();
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int subscribe(void)
{
	const typeof(_rdbNameList[0])* pNameList=_rdbNameList;

	while(pNameList->szPrefix)
	{
		/* skip if the variable is updated by other port manager such as cnsmgr */
		if (!is_enabled_feature(FEATUREHASH_CMD_ALLCNS) && !pNameList->basic_atmgr_vars) {
			pNameList++;
			continue;
		}

		if(pNameList->subscribe)
		{
			const char* szVariable=rdb_name(pNameList->szPrefix, pNameList->szName);

			SYSLOG_DEBUG("subscribing %s",szVariable);
			if (rdb_subscribe_variable(szVariable) != 0)
				SYSLOG_ERR("failed to subscribe to '%s'!", szVariable);
		}

		pNameList++;
	}

	handle_rdb_events();

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int isPIDRunning(pid_t pid)
{
	char achProcPID[128];
	char cbFN;
	struct stat statProc;

	cbFN = sprintf(achProcPID, "/proc/%d", pid);
	return stat(achProcPID, &statProc) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
static void ensure_singleton(int instance)
{
	char achPID[128];
	char lockfile[128];
	int fd;
	int cbRead;

	pid_t pid;
	int cbPID;

	sprintf(lockfile, "/var/lock/subsys/"APPLICATION_NAME"%-d", instance);

	fd = open(lockfile, O_RDWR | O_CREAT | O_EXCL, 0640);
	if (fd < 0)
	{
		if (errno == EEXIST)
		{
			fd = open(lockfile, O_RDONLY);

			// get PID from lock
			cbRead = read(fd, achPID, sizeof(achPID));
			if (cbRead > 0)
				achPID[cbRead] = 0;
			else
				achPID[0] = 0;

			pid = atoi(achPID);
			if (!pid || !isPIDRunning(pid))
			{
				SYSLOG_ERR("deleting the lockfile - %s", lockfile);

				close(fd);
				unlink(lockfile);

				ensure_singleton(instance);
				return;
			}
		}

		SYSLOG_ERR("another instance of %s already running (because creating lock file %s failed: %s)", APPLICATION_NAME, lockfile, strerror(errno));
		exit(EXIT_FAILURE);
	}

	pid = getpid();
	cbPID = sprintf(achPID, "%d\n", pid);

	write(fd, achPID, cbPID);
	close(fd);
}

////////////////////////////////////////////////////////////////////////////////
static void release_singleton(void)
{
	unlink("/var/lock/subsys/"APPLICATION_NAME);
}

////////////////////////////////////////////////////////////////////////////////
static void release_resources(void)
{
	release_singleton();
}

////////////////////////////////////////////////////////////////////////////////
static void shutdown_at_manager(void)
{
	SYSLOG_DEBUG("shutting down...");

	#ifdef V_MANUAL_ROAMING_vdfglobal
	fini_plmn();
	#endif

	create_rdb_variables(0);
    rdb_set_single("atmgr.status", "");

	rdb_close_db();
	at_close();
	release_resources();

	// fini supplimentary services
	supplFini();
}

////////////////////////////////////////////////////////////////////////////////
static int wait_for_port(const char* port, int timeout_sec)
{
	struct stat port_stat;
	SYSLOG_DEBUG("waiting for port '%s' for %d seconds...", port, timeout_sec);
	while (stat(port, &port_stat) < 0)
	{
		if (timeout_sec-- <= 0)
		{
			SYSLOG_ERR("waiting for port %s timed out (%s)", port, strerror(errno));
			return -1;
		}
		sleep(1);
	}
	SYSLOG_DEBUG("waiting for port '%s' done", port);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int simpleExec(const char* szCmd,int fFork)
{
	#define __countOf(x)	(sizeof(x)/sizeof((x)[0]))

	char* szDupCmd=strdup(szCmd);

	char* szToken;
	int iParam;

	char* achExecParam[256];

	// build exec param
	for(iParam=0,szToken=strtok(szDupCmd," "); iParam<__countOf(achExecParam) && szToken; iParam++,szToken=strtok(NULL," "))
		achExecParam[iParam]=szToken;
	achExecParam[iParam]=NULL;


	pid_t pid=0;

	if(fFork)
	{
		// fork
		pid=fork();
		if(pid<0)
			goto error;
	}

	// spawn if child or not forked
	if(!pid)
	{

		/* do not inherit file descriptor to child processor */
                if( at_get_fd() >= 0 )
                  fcntl(at_get_fd(), F_SETFD, FD_CLOEXEC);

		execv(achExecParam[0],achExecParam);
		exit(-1);
	}

	if(szDupCmd)
		free(szDupCmd);

	return 0;

error:
	if(szDupCmd)
		free(szDupCmd);

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
extern int read_smstools_inbox_path(void);
extern void read_smstools_encoding_scheme(void);
BOOL at_manager_initialized = FALSE;
static int init_at_manager(int instance, const char* port, const char* model,int be_daemon)
{
	int retry = 0;

	/* check singleton */
	SYSLOG_INFO("starting...");
	ensure_singleton(instance);

	/* create database variables */
	while (init_rdb() != 0)
	{
		SYSLOG_ERR("retry init RDB : %d", retry);
		retry++;
		if (retry >= 5)
		{
			SYSLOG_ERR("failed to init RDB!");
			goto cleanup;
		}
	}

	/* handle signals and daemonize */
	SYSLOG_DEBUG("setting signal handlers...");
	signal(SIGHUP, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGCHLD, sig_handler);

	if (be_daemon)
	{
		daemonize("/var/lock/subsys/" APPLICATION_NAME, "");
		SYSLOG_DEBUG("setting signal handlers in daemon...");
		signal(SIGHUP, sig_handler);
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
		signal(SIGUSR1, sig_handler);
		signal(SIGCHLD, sig_handler);;
	}

	/* wait until the port appears */
	if (wait_for_port(port, 60) != 0)
		goto cleanup;

	/* initialize AT */
	SYSLOG_DEBUG("initializing AT port...");
	if (at_init(port, on_at_notification) != 0)
		goto cleanup;

	/* open database */
	if (rdb_open_db() < 0)
	{
		SYSLOG_ERR("failed to re-open RDB!");
		goto cleanup;
	}

	// change debug level
	update_debug_level();
	(void)update_loglevel_via_logmask();

	/* init supplimentary services */
	supplInit();

	/* initialize share port database variables */
	SYSLOG_DEBUG("initializing phone module...");
	if (init_shared_port() < 0)
		goto cleanup;

	/* initialize schedule */
	scheduled_init();

	/* reopen database - why? */
	rdb_close_db();
	SYSLOG_INFO("AT manager initialized: AT port: '%s' (%s); instance: '%s'", port, port_shared ? "shared" : "not shared", wwan_prefix);
	SYSLOG_DEBUG("subscribing...");
	if (rdb_open_db() < 0)
	{
		SYSLOG_ERR("failed to re-open RDB!");
		goto cleanup;
	}

	/* find the module handler */
	SYSLOG_DEBUG("getting information from phone module...");
	if (model_init(model) != 0)
	{
		SYSLOG_ERR("failed to initialize model!");
		goto cleanup;
	}

	/* subscribe */
	if (subscribe() != 0)
	{
		SYSLOG_ERR("failed to subscribe to RDB variables!");
		goto cleanup;
	}

	if (!sms_disabled) {
		/* read sms tools config file information */
		if (read_smstools_inbox_path() != 0)
		{
			SYSLOG_ERR("failed to read smstools path!");
			goto cleanup;
		}

		/* read encoding scheme */
		read_smstools_encoding_scheme();
	}

	#ifdef V_MANUAL_ROAMING_vdfglobal
	// use error level to print always
	syslog(LOG_ERR,"[roaming] start custom roaming algorithm");

	// This is a work-around to avoid a conflict between Vodafone Manual roaming algorithm and selectband.template that is for applying backuped band-setting.
	if (1) {
		char buf[128];
		int cnt, hits=0;
		if(rdb_get_single(rdb_name("currentband.backup_config", ""), buf, sizeof(buf))>=0) {
			if (strlen(buf) > 0) {
				suspend_connection_mgr();
				for(cnt=0; cnt < 40 && hits < 2 ; cnt++) {
					rdb_get_single(rdb_name(RDB_BANDCMMAND, ""), buf, sizeof(buf));
					if (strlen(buf)>0) {
						hits++;
						handle_rdb_events();
					}
					sleep(1);
				}
			}
		}
	}

	// init plmn
	init_plmn();
	
	// quick and dirty - perform band selection perform before roaming otherwise the first PDP conneciton will fails
	extern void initialize_band_selection_mode(const struct model_status_info_t* new_status);
	extern void reset_connection_mgr_suspend_timer();

	struct model_status_info_t new_status;
	memcpy(&new_status,&new_status,sizeof(new_status));
	initialize_band_selection_mode(&new_status);

	reset_connection_mgr_suspend_timer();
	// do vodafone required algorithm
	do_manual_roaming_if_necessary();
	#endif
	
	model_init_schedule();
	
	/* set ready flag in database */
	if (rdb_update_single("atmgr.status", "ready", CREATE, ALL_PERM, 0, 0) != 0)
	{
		SYSLOG_ERR("failed to write ready flag!");
		goto cleanup;
	}

	at_manager_initialized = TRUE;

#if defined(PLATFORM_AVIAN)
#elif defined(PLATFORM_BOVINE)
#elif defined(PLATFORM_ANTELOPE)
#elif defined(PLATFORM_PLATYPUS2)

	#ifdef HAS_VOICE_DEVICE
	// Launching pots_bridge via the rdb variable takes 7 seconds at startup because Platypus2 has a slow shell interpretor
	// and rdb template manage is busy walking through all the templates during the startup period.
	// I know we want to be beautiful but unfurtunately, we have to save this 7 seconds for the factory!
	SYSLOG_ERR("directly launching pots_bridge.sh...");
	simpleExec("/sbin/pots_bridge.sh",1);
	#endif

#elif defined(PLATFORM_PLATYPUS)

	#warning 'TODO: This code is wrong. Please migrate to a suitable V_* variable'

    #if defined(BOARD_3g18wn) || \
        defined(BOARD_3g17wn) || \
        defined(BOARD_3gt1wn) || \
        defined(BOARD_3gt1wn2) || \
        defined(BOARD_3g34wn) || \
        defined(BOARD_testbox) || \
        defined(BOARD_3g8wv) || \
        defined(BOARD_3g36wv) || \
        defined(BOARD_3g39w) || \
        defined(BOARD_3g38wv)|| \
        defined(BOARD_3g38wv2)|| \
        defined(BOARD_3g46)
    #elif defined(BOARD_3g23wn) || \
        defined(BOARD_3g23wnl)
            simpleExec("/sbin/pots_bridge.sh",1);
    #else
    #error PLATYPUS BOARD_XXXX not specified - grep "platypus variant selection" to add a new variant
    #endif
#elif defined(PLATFORM_SERPENT)
#else
	#error Unknown Platform
#endif

	SYSLOG_INFO("AT manager initialized");
	return 0;

cleanup:
	release_resources();
    rdb_set_single("atmgr.status", "");
	exit(1);
}

////////////////////////////////////////////////////////////////////////////////
static int main_loop(void)
{
	int fAtClosed;

	while (running)
	{
		/* handle port sharing */
		handle_shared_port(FALSE);

		/* initialize loop variables */
		fd_set fdr;
		int selected;
		int at_fd = at_get_fd();
		int rdb_fd = rdb_get_fd();
		int nfds = 1 + (at_fd > rdb_fd ? at_fd : rdb_fd);
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

		FD_ZERO(&fdr);

		/* put AT port into fd read set */
		fAtClosed = at_get_fd() < 0;
		if (!fAtClosed)
			FD_SET(at_fd, &fdr);

		/* put database into fd read set */
		FD_SET(rdb_fd, &fdr);

		/* select */
		selected = select(nfds, &fdr, NULL, NULL, &timeout);

		if (!running)
		{
			SYSLOG_ERR("running flag off");
			break;
		}
		else if (selected < 0)
		{

			// if system call
			if (errno == EINTR)
			{
				SYSLOG_ERR("system call detected");
				continue;
			}

			SYSLOG_ERR("select() punk - error#%d(str%s)",errno,strerror(errno));
			break;
		}
		else if (selected > 0)
		{
			/* process AT port read */
			if (!fAtClosed && FD_ISSET(at_fd, &fdr))
			{
				SYSLOG_DEBUG("got event on AT port");

				if (handle_at_event() != 0)
					SYSLOG_ERR("handling AT event failed");
			}

			/* process rdb port read */
			if (FD_ISSET(rdb_fd, &fdr))
			{
				SYSLOG_DEBUG("got RDB event");

				if (handle_rdb_events() != 0)
					SYSLOG_ERR("handling RDB event failed");
			}
		}

		/* give schedule execution */
		scheduled_fire();
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static void usage(void)
{
	fprintf(stderr, "\nUsage: simple_at_manager [options]\n");
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-d don't detach from controlling terminal (don't daemonise)\n");
	fprintf(stderr, "\t-i instance\n");
	fprintf(stderr, "\t-p AT-command port\n");
	fprintf(stderr, "\t-s port shared with PPP\n");
	fprintf(stderr, "\t-v increase verbosity\n");
	fprintf(stderr, "\t-V version information\n");
	fprintf(stderr, "\t-l specify last USB port\n");
	fprintf(stderr, "\t-m specify phone module name - Sierra, Qualcomm, IPW and etc\n");
	fprintf(stderr, "\t-x launch without SMS feature\n");

	fprintf(stderr, "\n");

	fprintf(stderr,
		"\n * feature enable or disable options\n"
		"\t-t enable features (to work together with other managers)\n"
		"\t-f disable features (to work together with other managers)\n"
		"\n"
	);

	exit(-1);
}
/* get the instance number from program name
 simple_at_manager -> return 0
 simple_at_manager10 -> return 10
*/
static int get_instance(const char *argv)
{
	const char *p= &argv[strlen(argv)-1];
	while(p > argv && *p >='0'&& *p <='9' ) p--;
	return atoi(p+1);
}
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	const char *port = NULL;
	int	ret = 0;
	int	verbosity = 0;
	int	be_daemon = 1;
	int	instance = 0;
	char* last_port = NULL;
	char* model = NULL;

	#ifdef GPS_ON_AT
	/* init. curl */
	curl_global_init(CURL_GLOBAL_ALL);
	#endif

	if( init_feature()<0 ) {
		fprintf(stderr,"failed to initialize feature hash\n");
		exit(-1);
	}
	/*could get instance from  program name*/
	instance = get_instance(argv[0]);

	/* pharse command line */
	while ((ret = getopt(argc, argv, "dvVhi:m:p:sl:x?t:f:")) != EOF)
	{
		switch (ret)
		{
			case 't':
			case 'f':
// 				printf("optarg=%s\n",optarg);
				if( add_feature(optarg,ret=='t')<0 ) {
					fprintf(stdout,"failed to add feature - feature=%s,opt=%c\n",optarg,ret);
					exit(-1);
				}
				break;

			case 'd':
				be_daemon = 0;
				break;
			case 'v':
				++verbosity ;
				break;
			case 'V':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0], VER_MJ, VER_MN, VER_BLD);
				break;
			case 'i':
				instance = atoi(optarg);
				break;
			case 'p':
				port = optarg;
				break;
			case 'm':
				model = optarg;
				break;
			case 'l':
				last_port = optarg;
				break;
			case 's':
				port_shared = TRUE;
				break;
			case 'x':
				sms_disabled = TRUE;
				SYSLOG_ERR("launching without SMS features...");
				break;
			case 'h':
			case '?':
				usage();
				break;
		}
	}

	/* check incorrect parameters */
	if (!port)
		usage();

	// by default, enable all feature if no option is specified
	if(is_enabled_feature(FEATUREHASH_ALL)<0) {
		if( add_feature(FEATUREHASH_ALL,1)<0 ) {
			fprintf(stdout,"failed to enable all feature by default\n");
			exit(-1);
		}
	}


	/* initialize logging	*/
	sprintf(wwan_prefix, "wwan.%-d", instance);
	sprintf(gps_prefix, "%s.%-d", RDB_GPS_PREFIX, instance);
	openlog(APPLICATION_NAME, LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5);
	setlogmask(LOG_UPTO(verbosity + LOG_INFO));
	log_db[LOGMASK_AT].loglevel = (verbosity + LOG_INFO);

	SYSLOG_ERR("log-level-check - SYSLOG_ERR");
	SYSLOG_INFO("log-level-check - SYSLOG_INFO");
	SYSLOG_DEBUG("log-level-check - SYSLOG_DEBUG");

	/* disable sms */
	if(!is_enabled_feature(FEATUREHASH_CMD_SMS)) {
		SYSLOG_INFO("sms feature disabled by command line (-f sms)");
		sms_disabled=TRUE;
	}
#define XSTR(x) #x
#define STR(x) XSTR(x)

/* give predefined delay for routers using 3G USB dongle or for Sequance VZ20Q module */
#ifdef V_PORT_OPEN_DELAY
	
	/* waiting until the last port appears */
	SYSLOG_ERR("waiting %s seconds...", STR(V_PORT_OPEN_DELAY));
	sleep(atoi(STR(V_PORT_OPEN_DELAY)));
#endif

	if (last_port)
	{
		SYSLOG_INFO("waiting last port appeared for 30 seconds...");
		wait_for_port(last_port, 30);
	}
#ifdef V_PORT_OPEN_DELAY
	SYSLOG_ERR("waiting %s seconds...", STR(V_PORT_OPEN_DELAY));
	sleep(atoi(STR(V_PORT_OPEN_DELAY)));
#endif

	/* initialize at manager */
	if ((ret = init_at_manager(instance, port, model, be_daemon)) != 0)
	{
		SYSLOG_ERR("fatal: failed to initialize");
		exit(ret);
	}

	/* main loop */
	ret = main_loop();

	/* shutdown at manager */
	shutdown_at_manager();

	fini_feature();

	/* finialize logging */
	SYSLOG_INFO("exit (%s)", ret == 0 ? "normal" : "failure");
	closelog();

	exit(ret);
}

