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


#ifndef RDB_NAMES_H_
#define RDB_NAMES_H_

// general
#define RDB_COMMAND_MAX_LEN 256

// generic variables
#define RDB_UMTS_SERVICES "umts.services"
#define RDB_SIGNALSTRENGTH "radio.information.signal_strength"
#define RDB_RX_LEVEL "radio.information.rx_level"
#define RDB_RSCP "radio.information.rscp"
#define RDB_ECIO "radio.information.ecio"
#define RDB_HSDCAT "radio.information.hsdcat"
#define RDB_HSUCAT "radio.information.hsucat"
#define RDB_MODULETEMPERATURE "radio.temperature"
#define RDB_MODULEVOLTAGE "radio.voltage"
#define RDB_NETWORKNAME "system_network_status.network"


/* LTE network information */

/* LTE  signal strength */
#define RDB_RSRP0 "signal.0.rsrp"
#define RDB_RSRP1 "signal.1.rsrp"
/* LTE noise level */
#define RDB_RSRQ "signal.rsrq"
/* LTE  location information */
#define RDB_TAC "radio.information.tac"
#define RDB_PSC "system_network_status.PSCs0"


#define RDB_NETWORKATTACHED "system_network_status.attached"
#define RDB_NETWORKREGISTERED "system_network_status.registered"
#define RDB_NETWORKSIMREADY "system_network_status.simready"
#define RDB_NETWORKREG_STAT "system_network_status.reg_stat"

#define RDB_PDP0STAT "system_network_status.pdp0_stat"
#define RDB_PDP1STAT "system_network_status.pdp1_stat"
#define	RDB_SERVICETYPE "system_network_status.service_type"
#define RDB_NETWORK	"system_network_status"

#define	RDB_PDPSTATUS	"system_network_status.pdp_status"
#define	RDB_CURRENTBAND "system_network_status.current_band"
#define	RDB_MODULEBANDLIST "module_band_list"
#define RDB_MODULERATLIST "module_rat_list"
#define	RDB_PLMNLIST "system_network_plmn_list"
#define	RDB_PLMNSTATUS "system_network_plmn_status"
#define	RDB_PLMNMODE "system_network_plmn_selectionMode"
#ifdef V_CELL_NW_cdma
	#define	RDB_IMEI "meid"
#else
	#define	RDB_IMEI "imei"
#endif

#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
#define	RDB_MEID "meid"
#define RDB_ESN "esn"
#define RDB_PRIID_CARRIER "priid_carrier"
#define RDB_PRIID_CONFIG "priid_config"
#endif

#define RDB_SERIAL "serial"
#define	RDB_MODEL "model"
#define	RDB_MANUFACTURER "manufacture"
#define	RDB_FIRMWARE_VERSION "firmware_version"
#define	RDB_FIRMWARE_VERSION_CID "firmware_version_cid"
#define	RDB_HARDWARE_VERSION "hardware_version"
#define RDB_SIM_STATUS "sim.status.status"
#define RDB_IMSI "imsi"
#define RDB_DATE_AND_TIME "modem_date_and_time"

// sim varaibles
#define RDB_SIM_DATA_MBN	"sim.data.mbn"
#define RDB_SIM_DATA_MBDN	"sim.data.mbdn"
#define RDB_SIM_DATA_MSISDN "sim.data.msisdn"
#define RDB_SIM_DATA_ADN    "sim.data.adn"

#define	RDB_BANDPARAM "currentband.cmd.param.band"
#define	RDB_BANDPARAM_RAT "currentband.cmd.param.rat"
#define	RDB_BANDCURSEL "currentband.current_selband"
#define RDB_RATCURSEL "currentband.current_selrat"
#define	RDB_BANDCMMAND "currentband.cmd.command"
#define	RDB_BANDSTATUS "currentband.cmd.status"
#define	RDB_BANDCFG "currentband.config"

#define RDB_NWCTRLCOMMAND "networkctrl.cmd.command"
#define RDB_NWCTRLSTATUS "networkctrl.cmd.status"
#define RDB_NWCTRLVALID	"networkctrl.cmd.valid"

#define RDB_PLMNCOMMAND "PLMN_command_state"
#define RDB_PLMNCURLIST	"PLMN_list"
#define RDB_PLMNSTAT		"PLMN.cmd.status"
#define RDB_PLMNSEL			"PLMN_select"
#define RDB_PLMNMNC			"system_network_status.MNC"
#define RDB_PLMNMCC			"system_network_status.MCC"
#define RDB_PLMNSELMODE	"PLMN_selectionMode"

#define RDB_ROAMING			"system_network_status.roaming"
#define RDB_PLMNCURMODE	"system_network_status.manual_mode"
#define RDB_PLMNSYSMODE	"system_network_status.system_mode"

#define RDB_SMS_VOICEMAIL_STATUS		"sms.received_message.vmstatus"
#define RDB_SMS_STATUS					"sms.received_message.status"
#define RDB_SMS_STATUS2					"sms.message.status"
#define RDB_SMS_STATUS3					"sms.storage.status"
#define RDB_SMS_CMD_ID					"sms.cmd.param.message_id"
#define RDB_SMS_CMD_TO					"sms.cmd.param.to"
#define RDB_SMS_CMD_ST					"sms.cmd.status"
#define RDB_SMS_CMD_TX_ST				"sms.cmd.send.status"
#define RDB_SMS_CMD						"sms.cmd.command"
#define RDB_SMS_CMD_MSG					"sms.cmd.param.message"
#define RDB_SMS_CMD_FNAME				"sms.cmd.param.filename"
#define RDB_SMS_RD_SMSC					"sms.read.dstscno"
#define RDB_SMS_RD_DST					"sms.read.dstno"
#define RDB_SMS_RD_TIME					"sms.read.time_stamp"
#define RDB_SMS_RD_RXTIME				"sms.read.receivedtime"
#define RDB_SMS_RD_MSG					"sms.read.message"
#define RDB_SMS_SMSC_ADDR				"sms.smsc_addr"
#define RDB_SMS_TX_CONCAT_EN			"sms.txconcat_en"

#define	RDB_SIMPARAM "sim.cmd.param.pin"
#define	RDB_SIMCMMAND "sim.cmd.command"
#define	RDB_SIMMEPCMMAND "sim.cmd.mepcommand"
#define RDB_SIMCMDSTATUS "sim.cmd.status"
#define RDB_SIM_MEPLOCK "meplock.status"
#define RDB_SIMPINENABLED "sim.status.pin_enabled"
#define RDB_SIMNEWPIN "sim.cmd.param.newpin"
#define RDB_SIMMEP "sim.cmd.param.mep"
#define RDB_SIMMCCMNC "sim.cmd.param.mccmnc"
#define RDB_SIMPUK "sim.cmd.param.puk"
#define RDB_SIMRETRIES "sim.status.retries_remaining"
#define RDB_SIMAUTOPIN "sim.autopin"
#define RDB_SIMPIN "sim.pin"
#define RDB_SIMRESULTOFUO "sim.status.result_of_user_operation"
#define RDB_SIMPUKREMAIN "sim.status.retries_puk_remaining"
#define RDB_SIMCPINC "sim.cpinc_supported"
#define RDB_AUTOPIN_TRIED "sim.autopin.tried"

#define RDB_MODULE_BOOT_VERSION "boot_version"
#define RDB_NETWORK_STATUS "system_network_status"

// sierra phone module-specific variables
#define RDB_PHONE_SETUP "phone.setup"
#define RDB_PHONE_SETUP_CMD_SET_PROFILE "set profile"

#define RDB_PROFILE				"profile.cmd"
#define RDB_PROFILE_CMD		"profile.cmd.command"
#define RDB_PROFILE_ID		"profile.cmd.param.profile_id"
#define RDB_PROFILE_APN		"profile.cmd.param.apn"
#define RDB_PROFILE_USER	"profile.cmd.param.user"
#define RDB_PROFILE_PW		"profile.cmd.param.password"
#define RDB_PROFILE_AUTH	"profile.cmd.param.auth_type"
#define RDB_PROFILE_STAT	"profile.cmd.status"
#define RDB_PROFILE_UP		"session.0.status"

// ipw module specific variables
#define RDB_FREQ						"freq.cmd"
#define RDB_UMS							"ums.cmd"
#define RDB_LSTATUS					"lstatus.cmd"
#define RDB_PLMNID					"system_network_status.MNCMCC"
#define	RDB_SERVICETYPE_EXT	"system_network_status.service_type_ext"
#define	RDB_WANFREQ					"system_network_status.wanfreq"
#define RDB_SIGNALQUALITY		"system_network_status.signal_quality"

#define RDB_CHIPRATE				"system_network_status.chiprate"
#define RDB_TOFFSET				"system_network_status.toffset"
#define RDB_REGSTATUS				"system_network_status.regstatus"
#define RDB_HEART_BEAT	"heart_beat"
#define RDB_HEART_BEAT2	"heart_beat.simple_at_manager"

// Fusion module specific variables (Module information)
#define RDB_PRL_VERSION   "module_info.cdma.prlversion"
#define RDB_MDN   "module_info.cdma.MDN"
#define RDB_MSID   "module_info.cdma.MSID"
#define RDB_NAI   "module_info.cdma.NAI"
#define RDB_AVAIL_DATA_NETWORK   "module_info.cdma.ADN"
#define RDB_CONNECTION_STATUS   "module_info.cdma.connectionstatus"
#define RDB_MODULE_ACTIVATED   "module_info.cdma.activated"

// Fusion module specific variables (Roaming functionality)
#define RDB_CDMA_ROAMPREFERENCE   "system_network_status.roampreference"

// Fusion module specific variables (RF Information (##Debug))
#define RDB_CDMA_SERVICEOPTION   "system_network_status.serviceoption"
#define RDB_CDMA_SLOTCYCLEIDX	"system_network_status.slotcycleidx"
#define RDB_CDMA_BANDCLASS   "system_network_status.bandclass"

#define RDB_CDMA_1XRTTCHANNEL	"system_network_status.1xrttchannel"
#define RDB_CDMA_1XEVDOCHANNEL	"system_network_status.1xevdochannel"
#define RDB_CDMA_1XRTTPN	"system_network_status.1xrttpn"
#define RDB_CDMA_1XEVDOPN	"system_network_status.1xevdopn"

#define RDB_CDMA_1XRTT_RX_LEVEL "radio.information.1xrtt_rx_level"
#define RDB_CDMA_1XEVDO_RX_LEVEL "radio.information.1xevdo_rx_level"



#define RDB_CDMA_SYSTEMID	"system_network_status.SID"
#define RDB_CDMA_NETWORKID	"system_network_status.NID"
#define RDB_CDMA_1XRTTASET    "system_network_status.1xrttaset"
#define RDB_CDMA_EVDOASET    "system_network_status.evdoaset"
#define RDB_CDMA_1XRTTCSET    "system_network_status.1xrttcset"
#define RDB_CDMA_EVDOCSET    "system_network_status.evdocset"
#define RDB_CDMA_1XRTTNSET    "system_network_status.1xrttnset"
#define RDB_CDMA_EVDONSET    "system_network_status.evdonset"
#define RDB_CDMA_DOMINANTPN   "system_network_status.dominantPN"
#define RDB_CDMA_EVDODRCREQ	"system_network_status.evdodrcreq"
#define RDB_CDMA_1XRTTRSSI	"system_network_status.1xrttrssi"
#define RDB_CDMA_EVDORSSI	"system_network_status.evdorssi"
#define RDB_ECIOS0	"system_network_status.ECIOs0"
#define RDB_CDMA_1XRTTPER	   "system_network_status.1xrttper"
#define RDB_CDMA_EVDOPER	"system_network_status.evdoper"
#define RDB_CDMA_TXADJ	   "system_network_status.txadj"

// Fusion module specific variables (Mobile IP Information and configuration (##DATA))
#define RDB_CDMA_MIPCMMAND     "cdmamip.cmd.command"
#define RDB_CDMA_MIPCMDSTATUS "cdmamip.cmd.status"
#define RDB_CDMA_MIPCMDGETDATA "cdmamip.cmd.getdata"
#define RDB_CDMA_MIPCMDSETDATA "cdmamip.cmd.setdata"
#define RDB_SPRINT_MIPMSL            "cdmamip.mslcode"
#define RDB_SPRINT_CUR_MIP "cdmamip.ip"
#define RDB_SPRINT_MIP_MODE "cdmamip.mode"

// Fusion module specific variables (Advanced Parameters (##DATA#))
#define RDB_CDMA_ADPARACMMAND   "advancedpara.cmd.command"
#define RDB_CDMA_ADPARASTATUS   "advancedpara.cmd.status"
#define RDB_CDMA_ADPARAERRCODE   "advancedpara.cmd.errorcode"
#define RDB_CDMA_ADPARABUFFER   "advancedpara.cmd.buffer"
#define RDB_SPRINT_ADPARAMSL            "advancedpara.mslcode"

// Fusion module specific variables (Reverse Logistics (##RTN))
#define RDB_MODULE_RESETCMD "module_reset.cmd.command"
#define RDB_MODULE_RESETCMDSTATUS   "module_reset.cmd.status"

// Fusion module specific variables (Module configuration setting)
#define RDB_MODULE_CONFCMD "moduleconfig.cmd.command"
#define RDB_MODULE_CONFSTATUS "moduleconfig.cmd.status"
#define RDB_MODULE_CONFCMDPARAM "moduleconfig.cmd.cmdparam"
#define RDB_MODULE_CONFSTATUSPARAM "moduleconfig.cmd.statusparam"

// Option Wireless module specific variables
#define RDB_NETIF_IPADDR   "network_interface.ipaddress"
#define RDB_NETIF_GWADDR   "network_interface.gwaddress"
#define RDB_NETIF_PDNSADDR   "network_interface.pdnsaddress"
#define RDB_NETIF_SDNSADDR   "network_interface.sdnsaddress"

#ifdef USSD_SUPPORT
// USSD interface command
#define RDB_USSD_CMD   			"ussd.command"
#define RDB_USSD_CMD_STATUS   	"ussd.command.status"
#define RDB_USSD_CMD_RESULT   	"ussd.command.result"
#define RDB_USSD_MESSAGE   		"ussd.messages"
#endif	/* USSD_SUPPORT */

/* dtmf key command */
#define RDB_DTMF_CMD			RDB_UMTS_SERVICES".dtmfkeys"

#define RDB_IF_NAME			"if"

/* GPS command & variables */
#define RDB_GPS_PREFIX   	    "sensors.gps"
#define RDB_GPS_CMD   	        "cmd.command"
#define RDB_GPS_CMD_STATUS      "cmd.status"
#define RDB_GPS_CMD_ERRCODE     "cmd.errcode"
#define RDB_GPS_CMD_TIMEOUT     "cmd.timeout"

#define RDB_GPS_GPSONE_CAP				"gpsone.cap"
/* gpsOne enable or disbale */
#define RDB_GPS_GPSONE_EN				"gpsone.enable"
/* gpsOne semicolon-delimited URLs */
#define RDB_GPS_GPSONE_URLS				"gpsone.urls"
/* gpsOne manual update trigger */
#define RDB_GPS_GPSONE_UPDATE_NOW			"gpsone.update_now"
#define RDB_GPS_GPSONE_UPDATED				"gpsone.updated"
/* gpsOne automatic update parameters - update delay (seconds) */
#define RDB_GPS_GPSONE_AUTO_UPDATE_RETRY_DELAY		"gpsone.auto_update.retry_delay"
/* gpsOne automatic update parameters - update period (hours) */
#define RDB_GPS_GPSONE_AUTO_UPDATE_PERIOD		"gpsone.auto_update.update_period"
#define RDB_GPS_GPSONE_AUTO_UPDATE_MAX_RETRY_CNT	"gpsone.auto_update.max_retry_count"
#define RDB_GPS_GPSONE_AUTO_UPDATE_RETRIED		"gpsone.auto_update.retried"
/* gpsOne info - week */
#define RDB_GPS_GPSONE_XTRA_INFO_GNSS_TIME		"gpsone.xtra.info.gnss_time"
/* gpsOne info - extra duration */
#define RDB_GPS_GPSONE_XTRA_INFO_VALID_TIME		"gpsone.xtra.info.valid_time"
/* gpsOne info - update time */
#define RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME		"gpsone.xtra.updated_time"

#define RDB_GPS_VAR_DATE        "assisted.date"
#define RDB_GPS_VAR_TIME        "assisted.time"
#define RDB_GPS_VAR_LONG        "assisted.longitude"
#define RDB_GPS_VAR_LONG_DIR    "assisted.longitude_direction"
#define RDB_GPS_VAR_LATI        "assisted.latitude"
#define RDB_GPS_VAR_LATI_DIR    "assisted.latitude_direction"
#define RDB_GPS_VAR_GEOID       "assisted.height_of_geoid"
#define RDB_GPS_VAR_LOCUNCP     "assisted.LocUncP"
#define RDB_GPS_VAR_LOCUNCA     "assisted.LocUncA"
#define RDB_GPS_VAR_LOCUNCANGLE "assisted.LocUncAngle"
#define RDB_GPS_VAR_HEPE        "assisted.HEPE"
#define RDB_GPS_VAR_3D_FIX      "assisted.3d_fix"

#define RDB_GPS_INITIALIZED     "gps_initialized"	/* wwan.0.gps_initialized */

#define RDB_BAND_INITIALIZED    "band_initialized"

#define RDB_QXDM_SPC		"qxdm.cmd.spc"
#define RDB_QXDM_STAT		"qxdm.cmd.status"
#define RDB_QXDM_COMMAND	"qxdm.cmd.command"

#define RDB_CDMA_OTASP_STAT	"cdma.otasp.stat"
#define RDB_CDMA_OTASP_CMD	"cdma.otasp.cmd"
#define RDB_CDMA_OTASP_XX	"cdma.otasp.xx"
#define RDB_CDMA_OTASP_PROGRESS	"cdma.otasp.progress"

#define RDB_CDMA_OTASP_MDN	"cdma.otasp.mdn"
#define RDB_CDMA_OTASP_MSID	"cdma.otasp.msid"
#define RDB_CDMA_OTASP_NAI	"cdma.otasp.nai"


#define RDB_CDMA_OTASP_SPC	"cdma.otasp.spc"

#define RDB_CDMA_PREFSET_CMD		"cdma.prefset.cmd"
#define RDB_CDMA_PREFSET_NETWORK_MODE	"cdma.prefset.network_mode"
#define RDB_CDMA_PREFSET_ROAM_MODE	"cdma.prefset.roam_mode"
#define RDB_CDMA_PREFSET_STAT		"cdma.perfset.stat"
#define RDB_CDMA_PREFSET_MIPINFO		"cdma.perfset.mipinfo"
#define RDB_CDMA_PREFSET_INIT		"cdma.prefset.initialized"

#define RDB_PRLVER			"prlver"

/* pots bridge enable command */
#define RDB_POTS_CMD			"potsbridge_disabled"

/* Voice echo cancellation */
#define RDB_ECHO_CANCELLATION		"voicecall.echocancelled"

#define RDB_SYSLOG_PREFIX		"service.syslog.option"
#define RDB_SYSLOG_MASK			"mask"

/* Cinterion PHS8-P critical temperature status */
#define RDB_CRIT_TEMP_STATUS	"crit_temp_status"

#define	RDB_CPOL_PREF_PLMNLIST "cpol_pref_plmnlist"
#endif /* RDB_NAMES_H_ */

