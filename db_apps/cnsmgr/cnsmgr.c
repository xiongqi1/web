
#include "cnsmgr.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>

#define _GNU_SOURCE
#include <string.h>

#include "base.h"
#include "globalconfig.h"

#include "voicecall_constants.h"
#include "smsdef.h"
#include "sierracns.h"
#include "smsrecv.h"
#include "smssend.h"
#include "databasedriver.h"
#include "signormterm.h"
#include "daemon.h"
#include "modulemonitor.h"
#include "tickcount.h"

#include "featurehash.h"

#include <errno.h>

//#define SIM_DEBUG

char *strsignal(int sig);

static sierracns* _pCns = NULL;
static databasedriver* _pDb = NULL;
static smsrecv* _pSmsRecv = NULL;
static smssend* _pSmsSend = NULL;

typedef enum
{
	database_reserved_msg_error = 0,
	database_reserved_msg_busy,
	database_reserved_msg_done,
	database_reserved_msg_unknown,
} database_reserved_msg;

// local function defines
BOOL dbGetStr(int nObjId, int iIdx, char* pValue, int* pValueLen);
BOOL cnsVeriChvCode(int iChvType, const char* szChv, const char* szUnbChv);
BOOL cnsGetOperator(void);
BOOL dbSetStr(int nObjId, int iIdx, char* pValue);
BOOL dbSetInt(int nObjId, int iIdx, int nVal);
BOOL dbSetStat(int nObjId, int iIdx, database_reserved_msg resMsg, char* lpszDetail);
BOOL cnsSingleLocFix(int nFixType, int nAccuracy,int nTimeOut);

// command-based database callback
BOOL dbFifoCmdSms(char* pValue, int cbValue, char** ppNewValue);
BOOL dbCmdSms(char* pValue, int cbValue, char** ppNewValue);
BOOL dbCmdSimCardPin(char* pValue, int cbValue, char** ppNewValue);
BOOL dbCmdCurBand(char* pValue, int cbValue, char** ppNewValue);
BOOL dbCmdVoiceCall(char* pValue, int cbValue, char** ppNewValue);
BOOL dbCmdProfile(char* pValue, int cbValue, char** ppNewValue);
#ifdef GPS_ENABLED
BOOL dbCmdGPS( char* pValue, int cbValue, char** ppNewValue );
#endif
BOOL dbCmdSetProvider(char* pValue, int cbValue, char** ppNewValue);

// non-command-based database callback
BOOL cnsmgr_callbackOnDatabaseSelAdoProfile(char* pValue, int cbValue, char** ppNewValue);
BOOL cnsmgr_callbackOnDatabaseOverDialDigits(char* pValue, int cbValue, char** ppNewValue);
BOOL cnsgetSmscAddress(void);

#define CNSMGR_MAX_SUBITEM 20

// module firmware version for BCD error workaround - K2_0_7_30AP
static char _modFirmwareVersoin[128]={0,};
static char _wwanPrefix[16]={0,};
#ifdef GPS_ENABLED
static char _agpsPrefix[16]={0,};

// gps mode
#define GPS_MODE_DISABLE	0
#define GPS_MODE_ENABLE		1
#define GPS_MODE_AGPS		2

// gps default values
#define GPS_DEFAULT_TIMEOUT			255		// 255 seconds
#define GPS_DEFAULT_ACCURACY		(30)	// 30 meters
#define GPS_DEFAULT_FIX_COUNT		(-1)	// continue
#define GPS_DEFAULT_FIX_RATE		(1)		// every second

// gps configuration
#ifndef GPS_ON_AT
static int _gpsCurrentMode=0;									// current gps mode
static int _gpsInTracking=0;									// is gps in tracking?
static int _gpsTimeout=GPS_DEFAULT_TIMEOUT;		// timeout for agps
static int _gpsError=0;												// is error in agps?
static int _gpsAGPS=0;												//
#endif
#endif /* GPS_ENABLED */

// pre-define local functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsReadUnreadSms(void);
BOOL cnsReadReadSms(void);
BOOL cnsmgr_createOrDeleteKeys(BOOL fCreate);
BOOL cnsSendSmsFromQueue(void);
void cnsmgr_onModuleFault(BOOL fModuleStat);
BOOL cnsStartTracking(int nFixType, int nAccuracy,int nTimeOut,int nFixCount,int nFixRate);

BOOL has_modem_emulator = FALSE;
int active_pdp_session=0;

static int simcard_pin_enabled = 0;
static int simcard_schedule_enable_pin = -1;
static char simcard_pin[CNSMGR_MAX_VALUE_LENGTH]={0,};

/* synchronize operation mode with RDB variable and module internal mode after config restoring. */
typedef enum {
	NONE 		= -1,
	SELECT_PLMN 	= 0,
	WAITING_RESP 	= 1,
	SYNCHRONIZED 	= 2,
} op_mode_sync_state_type;
op_mode_sync_state_type op_mode_sync_state = -1;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// string convert define functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* cnsmgr_convertToStr(int iIdx, const char* strTbl[], int nTblCnt)
{
	static char szHex[16];

	if( (0<=iIdx) && (iIdx < nTblCnt))
		return strTbl[iIdx];

	sprintf(szHex,"%x",iIdx);
	return szHex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "strconverttable.h"

// _databaseKeyInfo
static struct
{
	int nObjId;
	int nObjIx;

	BOOL fFifo;

	int cCount;

	const char* szKey;
	BOOL (*lpfnHandler)(char* pValue, int cbValue, char** ppNewValue);

	const char* szPrefix;

	BOOL basic_cns_vars;
} _databaseKeyInfo[] =
{
	// ## need to tidy up ### audio profile - get
	{SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 0, FALSE, 	1,					"voicecall.audioprofile.get.profile", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 1, FALSE,	1,					"voicecall.audioprofile.get.earpiece", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 2, FALSE,	1,					"voicecall.audioprofile.get.microphone", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 3, FALSE,	1,					"voicecall.audioprofile.get.audiovolume", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 0, FALSE, 1,				"voicecall.dtmf.digits", cnsmgr_callbackOnDatabaseOverDialDigits,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 1, FALSE, 1,				"voicecall.dtmf.onduration", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 2, FALSE, 1,				"voicecall.dtmf.offduration", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 3, FALSE, 1,				"voicecall.dtmf.earpiece", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 4, FALSE, 1,				"voicecall.dtmf.status", NULL,NULL, FALSE},

	{SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS, 0, FALSE, 1,		"sms.received_message.status", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS, 1, FALSE, 1,		"sms.message.status", NULL,NULL, FALSE},


	// fifo - sms
	{CNSMGR_DB_FIFO_SMS, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,						"sms.fifo.command", dbFifoCmdSms,NULL, FALSE},

	// connection profile
	{CNSMGR_DB_PROFILE, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,							"profile.cmd.command", dbCmdProfile,NULL, FALSE},
	{CNSMGR_DB_PROFILE, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,						"profile.cmd.status", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 2, FALSE, 1,											"profile.cmd.param.profile_id", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 3, FALSE, 1,											"profile.cmd.param.header_compression", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 4, FALSE, 1,											"profile.cmd.param.data_compression", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 5, FALSE, 1,											"profile.cmd.param.pdp_address", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 6, FALSE, 1,											"profile.cmd.param.apn", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 7, FALSE, 1,											"profile.cmd.param.auth_type", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 8, FALSE, 1,											"profile.cmd.param.user", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 9, FALSE, 1,											"profile.cmd.param.password", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 10, FALSE, 1,											"profile.cmd.param.label", NULL,NULL, FALSE},
	{CNSMGR_DB_PROFILE, 11, FALSE, 1,											"profile.cmd.param.pdp_type", NULL,NULL, FALSE},

	// sms
	{ SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, 0, FALSE, 1,	"sms.smsc_addr", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_SMS, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,							"sms.cmd.command", dbCmdSms,NULL, FALSE},
	{CNSMGR_DB_KEY_SMS, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,						"sms.cmd.status", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_SMS, 2, FALSE, 1,											"sms.cmd.param.to", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_SMS, 3, FALSE, 1,											"sms.cmd.param.message", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_SMS, 4, FALSE, 1,											"sms.cmd.param.message_id", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_SMS, 5, FALSE, 1,											"sms.cmd.send.status", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_SMS, 6, FALSE, 1,											"sms.cmd.param.filename", NULL,NULL, FALSE},

	{SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, 0, FALSE, 1,					"sms.cmd.status", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, 1, FALSE, 1,					"sms.cmd.param.message_id", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, 2, FALSE, 1,					"sms.read.time_stamp", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, 3, FALSE, 1,					"sms.read.message", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, 4, FALSE, 1,					"sms.read.dstno", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, 5, FALSE, 1,					"sms.read.dstscno", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, 6, FALSE, 1,					"sms.read.receivedtime", NULL,NULL, FALSE},

	{SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, 0, FALSE, 1,					"sms.cmd.status", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, 1, FALSE, 1,					"sms.cmd.param.message_id", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, 2, FALSE, 1,					"sms.read.time_stamp", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, 3, FALSE, 1,					"sms.read.message", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, 4, FALSE, 1,					"sms.read.dstno", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, 5, FALSE, 1,					"sms.read.dstscno", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, 6, FALSE, 1,					"sms.read.receivedtime", NULL,NULL, FALSE},

	// status & information
	{SIERRACNS_PACKET_OBJ_HEARTBEAT, 0, FALSE, 1,								"heart_beat", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_HEARTBEAT, 1, FALSE, 1,								"heart_beat.cnsmgr", NULL,NULL, TRUE},
 
	{SIERRACNS_PACKET_OBJ_RETURN_MODEM_MODEL, 0, FALSE, 1,						"model", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_RETURN_BOOT_VERSION, 0, FALSE, 1,						"boot_version", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_HARDWARE_VERSION, 0, FALSE, 1,					"hardware_version", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_BUILD_DATE, 0, FALSE, 1,				"firmware_build_date", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_IMEI, 0, FALSE, 1,								"imei", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_RETURN_RADIO_TEMPERATURE, 0, FALSE, 1,				"radio.temperature", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_RETURN_RADIO_VOLTAGE, 0, FALSE, 1,					"radio.voltage", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_VERSION, 0, FALSE, 1,					"firmware_version", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_RETURN_MODEM_DATE_AND_TIME, 0, FALSE, 1,				"modem_date_and_time", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_REPORT_RADIO_INFORMATION, 0, FALSE, 1,				"radio.information.signal_strength", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_REPORT_RADIO_INFORMATION, 1, FALSE, 1,				"radio.information.bit_error_rate", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_ICCID, 0, FALSE, 1,							"system_network_status.simICCID", NULL,NULL, TRUE},
	// PRI
	{SIERRACNS_PACKET_OBJ_PRI, 0, FALSE, 1,										"system_network_status.PRIID_REV", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_PRI, 1, FALSE, 1,										"system_network_status.PRIID_PN", NULL,NULL, TRUE},
	// RSCP
	{SIERRACNS_PACKET_OBJ_RSCP, 0, FALSE, 16,									"system_network_status.RSCPs%d", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RSCP, 1, FALSE, 16,									"system_network_status.ECIOs%d", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RSCP, 2, FALSE, 16,									"system_network_status.PSCs%d", NULL,NULL, TRUE},


	{SIERRACNS_PACKET_OBJ_REPORT_NETWORK_STATUS, 0, FALSE, 1,			"system_network_status.reg_stat", NULL,NULL, TRUE},
	
	// system network status
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 0, FALSE, 1,			"system_network_status.modem_status", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 1, FALSE, 1,			"system_network_status.service_status", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 2, FALSE, 1,			"system_network_status.service_type", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 3, FALSE, 1,			"system_network_status.system_mode", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 4, FALSE, 1,			"system_network_status.current_band", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 5, FALSE, 1,			"system_network_status.roaming_status", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 6, FALSE, 1,			"system_network_status.manual_mode", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 7, FALSE, 1,			"system_network_status.country", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 8, FALSE, 1,			"system_network_status.network", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 9, FALSE, 1,			"system_network_status.MCC", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 10, FALSE, 1,			"system_network_status.MNC", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 11, FALSE, 1,			"system_network_status.LAC", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 12, FALSE, 1,			"system_network_status.RAC", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 13, FALSE, 1,			"system_network_status.CellID", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 14, FALSE, 1,			"system_network_status.ChannelNumber", NULL,NULL, FALSE},
	/* add for compatibility with simple_at_manager */
	{SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS, 15, FALSE, 1,			"system_network_status.roaming", NULL,NULL, FALSE},


	{SIERRACNS_PACKET_OBJ_AVAILABLE_SERVICE_DETAIL, 0, FALSE, 1,				"system_network_status.attached", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_AVAILABLE_SERVICE_DETAIL, 1, FALSE, 1,				"system_network_status.pdp0_stat", NULL,NULL, TRUE},

	// cns manager version
	{CNSMGR_DB_KEY_CNS_VERSION,	0, FALSE, 1,									"binary_info.name", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_CNS_VERSION,	1, FALSE, 1,									"binary_info.version", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_CNS_VERSION,	2, FALSE, 1,									"binary_info.heart_beat_status", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_CNS_VERSION,	3, FALSE, 1,									"binary_info.heart_beat_fail_count", NULL,NULL, TRUE},

	// sim status
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, 0, FALSE, 1,						"sim.status.status", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, 1, FALSE, 1,						"sim.status.user_operation_requeted", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, 2, FALSE, 1,						"sim.status.previous_user_operation", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, 3, FALSE, 1,						"sim.status.result_of_user_operation", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, 4, FALSE, 1,						"sim.status.retry_information_type", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, 5, FALSE, 1,						"sim.status.retries_remaining", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, 6, FALSE, 1,						"sim.status.retries_puk_remaining", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION, 0, FALSE, 1,				"sim.status.pin_enabled", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_SIMCARDPIN, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,					"sim.cmd.command", dbCmdSimCardPin,NULL, TRUE},
	{CNSMGR_DB_KEY_SIMCARDPIN, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,					"sim.cmd.status", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_SIMCARDPIN, 3, FALSE, 1,										"sim.cmd.param.pin", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_SIMCARDPIN, 4, FALSE, 1,										"sim.cmd.param.newpin", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_SIMCARDPIN, 5, FALSE, 1,										"sim.cmd.param.puk", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_SIMCARDPIN, 6, FALSE, 1,										"sim.cmd.param.mep", NULL,NULL, TRUE},
	{CNSMGR_DB_KEY_SIMCARDPIN, 7, FALSE, 1,										"sim.cpinc_supported", NULL,NULL, FALSE},

	// service provider
	{SIERRACNS_PACKET_OBJ_SERVICE_PROVIDER_NAME, 0, FALSE, 1,					"service_provider_name", NULL,NULL, TRUE},

	// IMSI
	{SIERRACNS_PACKET_OBJ_RETURN_IMSI, 0, FALSE, 1,								"imsi.plmn_mcc",NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_RETURN_IMSI, 1, FALSE, 1,								"imsi.plmn_mnc",NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_RETURN_IMSI, 2, FALSE, 1,								"imsi.msin",NULL,NULL, FALSE},

	// set provider
	{CNSMGR_DB_PLMN, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,							"PLMN_command_state", dbCmdSetProvider,NULL, FALSE},
	{CNSMGR_DB_PLMN, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,							"PLMN.cmd.status", NULL,NULL, FALSE},
	{CNSMGR_DB_PLMN, 2, FALSE, 1,												"PLMN_selectionMode", NULL,NULL, FALSE},
	{CNSMGR_DB_PLMN, 3, FALSE, 0,												"PLMN_select", NULL,NULL, FALSE},
	{CNSMGR_DB_PLMN, 4, FALSE, 1,												"PLMN_list", NULL,NULL, FALSE},
	{CNSMGR_DB_PLMN, 5, FALSE, 1,												"PLMN_ManualSelAvail", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS,0,FALSE,1,						"PLMN_list_available", NULL,NULL, TRUE},


	// set current band
	{CNSMGR_DB_KEY_CURRENTBAND, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,					"currentband.cmd.command", dbCmdCurBand,NULL, FALSE},
	{CNSMGR_DB_KEY_CURRENTBAND, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,				"currentband.cmd.status", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_CURRENTBAND, 2, FALSE, 0,									"currentband.cmd.param.band", NULL,NULL, FALSE},

	{SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND, 0, FALSE, 1,						"currentband.current_band", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,	"currentband.cmd.status", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND, 2, FALSE, 1,						"currentband.current_selband", NULL,NULL, FALSE},

	// packet session status
	{SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION, 0, FALSE, 16,					"session.%d.status", NULL,NULL, TRUE},
	{SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION, 1, FALSE, 16,					"session.%d.reason_code", NULL,NULL, TRUE},

	{SIERRACNS_PACKET_OBJ_RETURN_IP_ADDERSS,0, FALSE, 16,						"session.%d.ipaddress", NULL,NULL, TRUE},

	// voice call - status
	{SIERRACNS_PACKET_OBJ_REPORT_CALL_PROGRESS, 0, FALSE, 1,					"voicecall.status.callid", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_REPORT_CALL_PROGRESS, 1, FALSE, 1,					"voicecall.status.progress", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_MANAGE_MISSED_VOICE_CALLS_COUNT, 0, FALSE, 1,			"voicecall.status.missed_calls", NULL,NULL, FALSE},

	// voice call - command
	{CNSMGR_DB_KEY_VOICECALL, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,					"voicecall.cmd.command", dbCmdVoiceCall,NULL, FALSE},
	{CNSMGR_DB_KEY_VOICECALL, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,					"voicecall.cmd.status", NULL,NULL, FALSE},

	// voice call - command parameter
	{CNSMGR_DB_KEY_VOICECALL, 3, FALSE, 1,										"voicecall.cmd.param.callid", NULL,NULL, FALSE},
	{CNSMGR_DB_KEY_VOICECALL, 4, FALSE, 1,										"voicecall.cmd.param.action", NULL,NULL, FALSE},
	{SIERRACNS_PACKET_OBJ_INITIATE_VOICE_CALL,	0, FALSE, 1,					"voicecall.cmd.param.number", NULL,NULL, FALSE},

	// gps command
#ifdef GPS_ENABLED
#ifndef GPS_ON_AT
	{ SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSGMR_DB_SUBKEY_CMD, FALSE, 1,	"cmd.command", dbCmdGPS ,_agpsPrefix, TRUE},
	{ SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT, FALSE, 1,	"cmd.status", NULL ,_agpsPrefix, TRUE},
	{ SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 2, FALSE, 1,					"status", NULL ,_agpsPrefix, TRUE},
	{ SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 3, FALSE, 1,					"cmd.timeout", NULL ,_agpsPrefix, TRUE},
	{ SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4, FALSE, 1,					"cmd.errcode", NULL ,_agpsPrefix, TRUE},
#endif
#endif
};

/* special CNS packet IDs not defined in the table above. */
static const struct
{
	int nObjId;
	BOOL basic_cns_vars;
} _databaseKeyInfoSpecial[] =
{
	{SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, 				FALSE},
	{SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM, 							FALSE},
	{SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK, 						FALSE},
	{SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST, 									FALSE },
	{SIERRACNS_PACKET_OBJ_SELECT_PLMN, 											FALSE },
	{SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_RESULT, 							TRUE },
	{SIERRACNS_PACKET_OBJ_REPORT_POSITION_DET_FAILURE, 							TRUE },
	{SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_COMPLETE, 						TRUE },
	{SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_ERROR, 							TRUE },
	{SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX, 								TRUE },
	{SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 								TRUE },
	{SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION, 							TRUE },
	{SIERRACNS_PACKET_OBJ_MANAGE_PROFILE, 										TRUE },
	{SIERRACNS_PACKET_OBJ_WRITE_PROFILE, 										TRUE },
	{SIERRACNS_PACKET_OBJ_READ_PROFILE, 										TRUE },
	{SIERRACNS_PACKET_OBJ_RETURN_PROFILE_SUMMARY, 								TRUE },
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct
{
	// received - response of get SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS
	BOOL fRecvSmsCfgDetail;

	// SMS configuration
	smsconfigurationdetail smsConfigurationDetail;

	BOOL fRecvReadSmsMsg;

} _runtimeStat;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL cnsmgr_is_BasicCnsPacket(unsigned short nObjId, int nObjIx)
{
	int iInfo;
	int iKey;
	const __pointTypeOf(_databaseKeyInfo[0]) pKeyInfo;
	const __pointTypeOf(_databaseKeyInfoSpecial[0]) pKeyInfoSpecial;


	if (is_enabled_feature(FEATUREHASH_CMD_ALLAT))
		return TRUE;

	// check all database keys
	__forEach(iInfo, pKeyInfo, _databaseKeyInfo)
	{
		for (iKey = 0;iKey < pKeyInfo->cCount;iKey++)
		{
			if (pKeyInfo->nObjId == nObjId &&
				(nObjIx == -1 || (nObjIx >= 0 && pKeyInfo->nObjIx == nObjIx)) &&
				pKeyInfo->basic_cns_vars)
				return TRUE;
		}
	}
	// check all special database keys
	__forEach(iInfo, pKeyInfoSpecial, _databaseKeyInfoSpecial)
	{
		if (pKeyInfoSpecial->nObjId == nObjId && pKeyInfoSpecial->basic_cns_vars)
			return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* If the module support AT+CPINC command to read remaining PIN/PUK count value,
 * skip all SIM related command in cnsmgr
 * return : 0 : not support
 * 			1 : support
 * 			-1 : unknown yet */
static int is_cpinc_command_supporting(void)
{
	char* pMsg = __alloc(BSIZE_16);
	int cbMsg = BSIZE_16;
	if (is_enabled_feature(FEATUREHASH_CMD_ALLAT)) {
		return 0;
	}
	if (dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 7, pMsg, &cbMsg)) {
		if (!strcmp(pMsg, "1")) {
			return 1;
		} else if (!strcmp(pMsg, "0")) {
			return 0;
		}
	}
	return -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void cnsmgr_turn_off_BasicCnsFlag(int nObjId, int nObjIx)
{
	int iInfo;
	int iKey;
	__pointTypeOf(_databaseKeyInfo[0]) pKeyInfo;

	// check all database keys
	__forEach(iInfo, pKeyInfo, _databaseKeyInfo)
	{
		for (iKey = 0;iKey < pKeyInfo->cCount;iKey++)
		{
			if (pKeyInfo->nObjId == nObjId) {
				pKeyInfo->basic_cns_vars = FALSE;
				#ifdef SIM_DEBUG
				syslog(LOG_ERR, "turn off BasicCnsFlag: %x : %d", pKeyInfo->nObjId, pKeyInfo->nObjIx);
				#endif
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void __ucs2StrToStr(const void* pUcs2Str,char* achBuf)
{
	const BYTE* pLen=(const BYTE*)pUcs2Str;
	const char* pStr=(const char*)(pUcs2Str+2);

	int cbLen=*pLen;

	while(cbLen--)
	{
		*achBuf++=*pStr;
		pStr+=2;
	}

	*achBuf=0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void __pasStrToStr(const void* pPasStr,char* achBuf)
{
	const BYTE* pLen=(const BYTE*)pPasStr;
	const char* pStr=(const char*)(pLen+1);

	memcpy(achBuf,pStr,*pLen);
	achBuf[*pLen]=0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void __str2pasStr(const char* achBuf,void* pPasStr,int cbPasStr)
{
	BYTE* pLen=(BYTE*)pPasStr;
	char* pStr=(char*)(pLen+1);

	int cbLen=strlen(achBuf);
	if(cbLen>cbPasStr)
		cbLen=cbPasStr;

	*pLen=cbLen;
	memcpy(pStr,achBuf,cbLen);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cnsmgr_fini(void)
{
	syslog(LOG_DEBUG, "enter");

	// delete database names
	if (!cnsmgr_createOrDeleteKeys(FALSE))
		syslog(LOG_ERR, "failed to delete one of database keys");

	// destroy database
	databasedriver_destroy(_pDb);

	if (!_globalConfig.fSmsDisabled) {
		// destroy sms parser
		smsrecv_destroy(_pSmsRecv);
		// destroy sms trans
		smssend_destroy(_pSmsSend);
	}

	_pDb = NULL;
	_pSmsRecv = NULL;
	_pSmsSend = NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
char* dbGetKeyPrefix(int nObjId, int nObjIx)
{
	int iInfo;

	const __pointTypeOf(_databaseKeyInfo[0]) pKeyInfo;

	__forEach(iInfo, pKeyInfo, _databaseKeyInfo)
	{
		if ((pKeyInfo->nObjId == nObjId) && (pKeyInfo->nObjIx == nObjIx))
			return (char*)pKeyInfo->szPrefix;
	}

	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
char* dbGetKey(int nObjId, int nObjIx)
{
	int iInfo;

	const __pointTypeOf(_databaseKeyInfo[0]) pKeyInfo;

	__forEach(iInfo, pKeyInfo, _databaseKeyInfo)
	{
		if ((pKeyInfo->nObjId == nObjId) && (pKeyInfo->nObjIx == nObjIx))
			return (char*)pKeyInfo->szKey;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void str_replace(char *st, const char *orig, const char *repl)
{
    static char buffer[256];
    char *ch;
    if (strlen(st) > 256) {
        syslog(LOG_ERR, "too long string to run this replace command");
        return;
    }
    if (!(ch = strstr(st, orig)))
        return;
    strncpy(buffer, st, ch-st);
    buffer[ch-st] = 0;
    sprintf(buffer+(ch-st), "%s%s", repl, ch+strlen(orig));
    (void) strcpy(st, buffer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct cnsprofile_t
{
	BYTE bProfileId;
	BYTE bProfileInfoType;
	BYTE bProfileValid;
	BYTE bPdpType;
	BYTE bHdrCompression;
	BYTE bDataCompression;

	BYTE bPdpAddrLen;
	BYTE pdpAddr[16];

	BYTE bApnLen;
	BYTE apn[100];
	BYTE bPdpInitType;
	BYTE bPrimProfileId;
	BYTE bAuthType;
	BYTE szUserName[33];
	BYTE szPassword[33];
	BYTE szLabel[31];
	BYTE bAutoContextActivationMode;
	BYTE bProfileWriteProtect;
	BYTE bPromptPassword;
	BYTE bAutoLaunchApp;
	WORD bPdpLingerTimer;
	BYTE bSoftwareOpt;
	BYTE bReserved[14];
} __packedStruct;

struct cnsprofile_manage_tlv_t {
	BYTE bObjType;
	BYTE bObjLen;
	BYTE bObjVal[0];
} __packedStruct tlv[0];


struct cnsprofile_manage_t
{
	WORD wVer;
	BYTE bProfileID;
	BYTE bNumberOfTLV;

	struct cnsprofile_manage_tlv_t tlv[0];

} __packedStruct;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct
{
	char* szBand;
	UINT64 l64Code;
	BOOL	valid;
} bandConvTbl[] =
{
	{"GSM 450",						0x0000000000010000ULL, 0},
	{"GSM 480",						0x0000000000020000ULL, 0},
	{"GSM 750",						0x0000000000040000ULL, 0},
	{"GSM 850",						0x0000000000080000ULL, 0},

	{"GSM RGSM 900",				0x0000000000100000ULL, 0},
	{"GSM PCS 1900",				0x0000000000200000ULL, 0},
	{"GSM DCS 1800",				0x0000000000400000ULL, 0},
	{"GSM EGSM 900",				0x0000000000800000ULL, 0},

	{"GSM PGSM 900",				0x0000000001000000ULL, 0},
	{"GSM 850/PCS",					0x0000000000280000ULL, 0},

	{"WCDMA II PCS 1900",			0x0000000200000000ULL, 0},
	{"WCDMA III 1700",				0x0000000400000000ULL, 0},
	{"WCDMA IV 1700",				0x0000000800000000ULL, 0},

	{"WCDMA VI 800",				0x0000002000000000ULL, 0}, //8790 | 8795
	{"WCDMA VII 2600",				0x0000004000000000ULL, 0},
	{"WCDMA VIII 900",				0x0000008000000000ULL, 0}, //8792 | 8795
	{"WCDMA IX 1700",				0x0000010000000000ULL, 0},
	{"WCDMA NA",					0x0000001200000000ULL, 0},
	{"WCDMA/GSM EU",				0x0000000101c00000ULL, 0},
	{"WCDMA/GSM NA",				0x0000001200280000ULL, 0},
	{"WCDMA Australia",				0x0000001100000000ULL, 0},
	{"WCDMA Australia/GSM EU",		0x0000001101c00000ULL, 0},
	{"WCDMA Japan",					0x0000002100000000ULL, 0},

	{"WCDMA All",					0x0000003300000000ULL, 0},	// WCDMA All - WCDMA Australia + WCDMA I IMT 2000 (EU)
	{"WCDMA All",					0x0000003300000000ULL, 0},	// WCDMA All - 800/850/1900/2100 (MC8790)
	{"WCDMA ALL",					0x0000008300000000ULL, 0},  // All 900/2100/1900 (MC8792)
	{"WCDMA ALL",					0x000000b300000000ULL, 0},  // All 800/850/900/1900/2100 (MC8795)

	// from locking phone module
	{"Autoband",					0xffffffffffffffffULL, 1},
	{"WCDMA 2100",					0x0000000100000000ULL, 0},	// WCDMA 2100
	{"GSM 900/1800",				0x0000000001c00000ULL, 0},	// GSM 900/1800
	{"2G",							0x0000000001e80000ULL, 0},	// 2G GSM all bands

	// the extention bands for unlocked phone module
	{"UMTS 850Mhz Only",			0x0000001000000000ULL, 0,},	// UMTS 850Mhz Only WCDMA V 850
	{"UMTS 850Mhz,2G",				0x0000001001c00000ULL, 0,},	// UMTS 850Mhz,2G - WCDMA Australia/GSM EU sub WCDMA I IMT 2000 (EU)

	{"WCDMA 900/2100",				0x0000008100000000ULL, 0},
	{"WCDMA 900/2100 GSM 900/1800",	0x0000008101c00000ULL, 0},

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int _fTermSigDetected = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 __htonl64(UINT64 ul64Host)
{
	UINT64STRUC* pHost = (UINT64STRUC*) & ul64Host;
	UINT64STRUC ul64Network;

	ul64Network.dw32hi = htonl(pHost->dw32lo);
	ul64Network.dw32lo = htonl(pHost->dw32hi);

	return ul64Network.u64;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 __ntohl64(UINT64 ul64Network)
{
	UINT64STRUC* pNetwork = (UINT64STRUC*) & ul64Network;
	UINT64STRUC ul64Host;

	ul64Host.dw32hi = ntohl(pNetwork->dw32lo);
	ul64Host.dw32lo = ntohl(pNetwork->dw32hi);

	return ul64Host.u64;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_convertBandToStr(UINT64 ul64BandCode, char* szBand)
{
	__pointTypeOf(bandConvTbl[0]) pConvTbl;
	int i;

	__forEach(i, pConvTbl, bandConvTbl)
	{
		if (pConvTbl->l64Code != ul64BandCode)
			continue;
		pConvTbl->valid=TRUE;
		strcpy(szBand, pConvTbl->szBand);
		return TRUE;
	}

	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_convertStrToBand(char* szBand, UINT64* pul64BandCode)
{
	__pointTypeOf(bandConvTbl[0]) pConvTbl;
	int i;

	__forEach(i, pConvTbl, bandConvTbl)
	{
		if (strcasecmp(pConvTbl->szBand, szBand))
			continue;
		if(!pConvTbl->valid)
			continue;
		*pul64BandCode = pConvTbl->l64Code;
		return TRUE;
	}

	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
char* cnsmgr_convertSysBandToStr(WORD wBand)
{
	static char band_name[128];

	static const char* _bandNameList[] =
	{
		"No service",
		"GSM 850",
		"GSM 900",
		"GSM 1800",
		"GSM 1900",
	};

	static const char* _bandNameList101[] =
	{
		"WCDMA 2100",
		"WCDMA 1900",
		"WCDMA 850",
		"WCDMA 800",
		"WCDMA 1800",
		"WCDMA 1700 (US)",
		"WCDMA 2600",
		"WCDMA 900",
		"WCDMA 1700 (Japan)",
	};

	if (wBand < __countOf(_bandNameList))
		return (char*)_bandNameList[wBand];

	else if (00101 <= wBand && wBand < 0x0101 + __countOf(_bandNameList101))
		return (char*)_bandNameList101[wBand-0x0101];

	snprintf(band_name,sizeof(band_name),"unknown (0x%04x)",wBand);
	return band_name;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//char* cnsmgr_convertBandToStr(unsigned char bBand)
//{
//	static const char* _bandNameList28[] =
//	{
//		"GSM 450",
//		"GSM 480",
//		"GSM 750",
//		"GSM 850",
//		"GSM E-GSM 900",
//		"GSM P-GSM 900",
//		"GSM R-GSM 900",
//		"GSM DCS 1800",
//		"GSM PCS 1900"
//	};
//
//	static const char* _bandNameList50[] =
//	{
//		"WCDMA I IMT 2000",
//		"WCDMA II PCS 1900",
//		"WCDMA III DCS 1800",
//		"WCDMA IV 1700",
//		"WCDMA V 850",
//		"WCDMA VI 800",
//		"WCDMA VII 2600",
//		"WCDMA VIII 900",
//		"WCDMA IX 1700 (US)",
//		"WCDMA 2600",
//		"WCDMA 900",
//		"WCDMA 1700 (Japan)",
//	};
//
//	if (0x28 <= bBand && bBand < 0x28 + __countOf(_bandNameList28))
//		return (char*)_bandNameList28[bBand-0x28];
//	else if (0x50 <= bBand && bBand < 0x50 + __countOf(_bandNameList50))
//		return (char*)_bandNameList50[bBand-0x50];
//
//	return NULL;
//}
//

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_callbackOnCnsNotEnResp(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, sierracns_stat cnsStat)
{
	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbWrite(const char* szKey, const char* szValue,const char* szPrefix)
{
	//syslog(LOG_DEBUG, "##db## write %s=%s", szKey, szValue);

	if (!databasedriver_cmdSetSingle(_pDb, szKey, (char*)szValue, FALSE, FALSE, szPrefix))
	{
		if(!databasedriver_cmdSetSingle(_pDb, szKey, (char*)szValue, TRUE, FALSE, szPrefix))
			syslog(LOG_ERR, "write failed - key=%s, value=%s, error=%d", szKey, szValue, -errno);
		return FALSE;
	}

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbWriteInt(const char* szKey, int nValue, const char* szPrefix)
{
	char achBuf[CNSMGR_MAX_VALUE_LENGTH];

	sprintf(achBuf,"%d",nValue);

	return dbWrite(szKey,achBuf, szPrefix);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
long double fabsl(long double val)
{
	return val<0?-val:val;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
long double __round3(long double x)
{
	return (long double)((long long)(x*1000+0.5))/1000;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* convDegToNMEA(long double dblDeg,int nDegDigit)
{
	static char achDegMinSec[128];
	char achMin[128];
	char achFmt[32];

	int nDeg;
	int nMin;
	long double dblMin;


	if(dblDeg<0)
		dblDeg=-dblDeg;

	// get deg
	nDeg=(int)dblDeg;
	dblDeg-=nDeg;

	// get min
	dblMin=dblDeg*60;
	nMin=(int)dblMin;

	// get min below point
	dblMin-=nMin;

	sprintf(achMin,"%Lg",__round3(dblMin));

	sprintf(achFmt,"%%0%dd%%02d%%s",nDegDigit);
	sprintf(achDegMinSec,achFmt,nDeg,nMin,achMin+1);

	return achDegMinSec;
}

static time_t convert_utc_to_local(struct tm* tm_utc)
{
	time_t utc;
	
	tzset();
	
	// make utc
	utc=timegm(tm_utc);
	
	return utc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_callbackOnCnsGetResp(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, sierracns_stat cnsStat)
{
	BOOL fProcessed = FALSE;

	if (!cnsmgr_is_BasicCnsPacket(wObjId, -1))
		return TRUE;

	if (_globalConfig.fSmsDisabled &&
	   (wObjId == SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM ||
	    wObjId == SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK ||
	    wObjId == SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS))
    {
		return TRUE;
	}


	switch (wObjId)
	{
#ifdef GPS_ENABLED
#ifdef GPS_ON_AT
        /* Even if GPS is running on AT command base, location fix result and notification are
         * still coming out from CNS port. Silently ignore these GPS packets. */
		case SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_RESULT:
		case SIERRACNS_PACKET_OBJ_REPORT_POSITION_DET_FAILURE:
		case SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_ERROR:
		case SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_COMPLETE:
            fProcessed = TRUE;
            break;
#else   /* GPS_ON_AT */
		case SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_RESULT:
		{
			struct
			{
				uint16_t wVer;
				int32_t nLatitude;
				int32_t nLongitude;
				uint32_t dwTimestamp;
				uint16_t wLocUncertAngle;
				uint16_t wLocUncertA;
				uint16_t wLocUncertP;
				uint16_t wFixType;
				uint16_t wHeightInfoInc;
				int16_t nHeight;
				uint16_t wLocUncertVert;
				uint16_t wVelInfoInc;
				uint16_t wHeading;
				uint16_t wVelHorz;
				int16_t nVelVert;
				uint16_t wUncertA;
				uint16_t wUncertP;
				uint16_t wUncertV;
				uint16_t wHEPE;
				uint8_t bNoOfSat;
				uint8_t bPosMode;
				uint8_t bPosSrc;
			} __packedStruct* pLocResult = pParam;

			#ifdef GPS_PKT_DEBUG
			syslog(LOG_ERR, "<--SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_RESULT");
			#endif

			if(_gpsCurrentMode==GPS_MODE_AGPS)
			{
				const double locUncertTbl[]={0.5,0.75,1,1.5,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288};

				long double dblLatitude;
				long double dblLongitude;

				char achBuf[CNSMGR_MAX_VALUE_LENGTH];

				/* check date & time field validity first because MC8704
				   sends this field with 0 always */
                if (ntohl(pLocResult->dwTimestamp) == 0) {
        			syslog(LOG_ERR, "Date & Time in location fix result is zero, ignore this packet!!");
    				// set status - error
    				if (!dbSetStr(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4, "invalid date/time"))
    					syslog(LOG_ERR, "failed to write %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4));
    				_gpsError=1;
        			fProcessed = TRUE;
    				break;
                }

				dblLatitude=(long double)(int)ntohl(pLocResult->nLatitude)*360/(2<<25);

				// set latitude degree
				dbWrite("assisted.latitude",convDegToNMEA(dblLatitude,2),_agpsPrefix);
				// set latitude direction
				if(dblLatitude==0)
					dbWrite("assisted.latitude_direction","",_agpsPrefix);
				else if (dblLatitude<0)
					dbWrite("assisted.latitude_direction","S",_agpsPrefix);
				else
					dbWrite("assisted.latitude_direction","N",_agpsPrefix);


				// set longitude
				dblLongitude=(long double)(int)ntohl(pLocResult->nLongitude)*360/(2<<25);
				dbWrite("assisted.longitude",convDegToNMEA(dblLongitude,3),_agpsPrefix);
				if(dblLongitude==0)
					dbWrite("assisted.longitude_direction","",_agpsPrefix);
				else if (dblLongitude<0)
					dbWrite("assisted.longitude_direction","W",_agpsPrefix);
				else
					dbWrite("assisted.longitude_direction","E",_agpsPrefix);

				// set height
				if(pLocResult->wHeightInfoInc)
				{
					dbWriteInt("assisted.height_of_geoid",ntohs(pLocResult->nHeight),_agpsPrefix);
				}
				else
				{
					dbWrite("assisted.height_of_geoid","",_agpsPrefix);
				}

				// set time
				{
					struct tm tmGPS;
					struct tm* pTmGPS;
					time_t timeGPS;

					memset(&tmGPS,0,sizeof(tmGPS));
					tmGPS.tm_year=1980-1900;
					tmGPS.tm_mon=0;
					tmGPS.tm_mday=6;
					tmGPS.tm_hour=0;
					tmGPS.tm_min=0;
					tmGPS.tm_sec+=ntohl(pLocResult->dwTimestamp);
					timeGPS=convert_utc_to_local(&tmGPS);

					// convert time_t to tm again to get year, mon, day, hour, min and sec
					pTmGPS=gmtime(&timeGPS);

					// write date
					sprintf(achBuf,"%02d%02d%02d",pTmGPS->tm_mday,pTmGPS->tm_mon+1,(pTmGPS->tm_year+1900)%100);
					dbWrite("assisted.date",achBuf,_agpsPrefix);
					// write time
					sprintf(achBuf,"%02d%02d%02d",pTmGPS->tm_hour,pTmGPS->tm_min,pTmGPS->tm_sec);
					dbWrite("assisted.time",achBuf,_agpsPrefix);
				}

				// set LocUncAngle
				sprintf(achBuf,"%Lg",(long double)__round3(ntohs(pLocResult->wLocUncertAngle)*5.625));
				dbWrite("assisted.LocUncAngle",achBuf,_agpsPrefix);

				// set location uncertity A&P
				{
					int wLocUncertA=ntohs(pLocResult->wLocUncertA);
					int wLocUncertP=ntohs(pLocResult->wLocUncertP);

					int cTbl=sizeof(locUncertTbl)/sizeof(locUncertTbl[0]);

					if(cTbl==wLocUncertA)
						sprintf(achBuf,"> %g",locUncertTbl[cTbl-1]);
					else if (wLocUncertA>cTbl)
						sprintf(achBuf,"Not computable");
					else
						sprintf(achBuf,"%g",locUncertTbl[wLocUncertA]);

					dbWrite("assisted.LocUncA",achBuf,_agpsPrefix);

					if(cTbl==wLocUncertP)
						sprintf(achBuf,"> %g",locUncertTbl[cTbl-1]);
					else if (wLocUncertP>cTbl)
						sprintf(achBuf,"Not computable");
					else
						sprintf(achBuf,"%g",locUncertTbl[wLocUncertP]);

					dbWrite("assisted.LocUncP",achBuf,_agpsPrefix);
				}

				// set fix type
				if(!pLocResult->wFixType)
					dbWrite("assisted.3d_fix","2",_agpsPrefix);
				else
					dbWrite("assisted.3d_fix","3",_agpsPrefix);

				// set HEPE
				sprintf(achBuf,"%g",ntohs(pLocResult->wHEPE)*0.1);
				dbWrite("assisted.HEPE",achBuf,_agpsPrefix);

/*
				currently we don't need the following variables

				dbWriteInt("assisted.raw.wLocUncertVert",ntohs(pLocResult->wLocUncertVert),_agpsPrefix);
				dbWriteInt("assisted.raw.wVelInfoInc",ntohs(pLocResult->wVelInfoInc),_agpsPrefix);
				dbWriteInt("assisted.raw.wHeading",ntohs(pLocResult->wHeading),_agpsPrefix);
				dbWriteInt("assisted.raw.wVelHorz",ntohs(pLocResult->wVelHorz),_agpsPrefix);
				dbWriteInt("assisted.raw.nVelVert",(int)ntohs(pLocResult->nVelVert),_agpsPrefix);
				dbWriteInt("assisted.raw.wUncertA",ntohs(pLocResult->wUncertA),_agpsPrefix);
				dbWriteInt("assisted.raw.wUncertP",ntohs(pLocResult->wUncertP),_agpsPrefix);
				dbWriteInt("assisted.raw.wUncertV",ntohs(pLocResult->wUncertV),_agpsPrefix);
				dbWriteInt("assisted.raw.bNoOfSat",pLocResult->bNoOfSat,_agpsPrefix);
				dbWriteInt("assisted.raw.bPosMode",pLocResult->bPosMode,_agpsPrefix);
				dbWriteInt("assisted.raw.bPosSrc",pLocResult->bPosSrc,_agpsPrefix);
*/
			}

			fProcessed = TRUE;
			break;
		}

		case SIERRACNS_PACKET_OBJ_REPORT_POSITION_DET_FAILURE:
		{
			#ifdef GPS_PKT_DEBUG
			syslog(LOG_ERR, "<--SIERRACNS_PACKET_OBJ_REPORT_POSITION_DET_FAILURE");
			#endif
			if(_gpsCurrentMode==GPS_MODE_AGPS)
			{
				// set status - error
				if (!dbSetStr(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4, "PD failure - timeout"))
					syslog(LOG_ERR, "failed to write %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4));

				_gpsError=1;
			}

			fProcessed = TRUE;
			break;
		}


		case SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_ERROR:
		{
			struct {
				uint16_t wVer;
				uint16_t wErrCode;
			} __packedStruct* pFixError=pParam;

			#ifdef GPS_PKT_DEBUG
			syslog(LOG_ERR, "<--SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_ERROR, error 0x%04x", pFixError->wErrCode);
			#endif
			if(_gpsCurrentMode==GPS_MODE_AGPS && _gpsAGPS)
			{
				uint16_t wErrCode=ntohs(pFixError->wErrCode);

				char achBuf[CNSMGR_MAX_VALUE_LENGTH];

				if(wErrCode<0x1000)
					sprintf(achBuf,"FIX error (%s)",(char*)STRCONVERT_FUNCTION_CALL(LocationFixError, wErrCode));
				else
					sprintf(achBuf,"FIX error (%s)",(char*)STRCONVERT_FUNCTION_CALL(LocationFixError2, wErrCode));

				// set status - error
				if (!dbSetStr(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4, achBuf))
					syslog(LOG_ERR, "failed to write %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4));

				_gpsError=pFixError->wErrCode;
			} else {
				_gpsError = 0;
			}

			fProcessed = TRUE;
			break;
		}

		case SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_COMPLETE:
		{
			/* SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_COMPLETE comes after SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION
			 * and SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX as well so it is needed to separate two cases.
			 */
			static int gps_initiated = 0;
			#ifdef GPS_PKT_DEBUG
			syslog(LOG_ERR, "<--SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_COMPLETE");
			syslog(LOG_ERR, "<--_gpsCurrentMode = %d, gps_initiated = %d", _gpsCurrentMode, gps_initiated);
			#endif

			/* When received REPORT_LOCATION_FIX_COMPLETE for gps disable command, always return result immediatly */
			if(_gpsCurrentMode==GPS_MODE_DISABLE)
			{
				// set status - done
				if (!dbSetStat(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
					syslog(LOG_ERR, "failed to write to %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT));
			}

			/* When received REPORT_LOCATION_FIX_COMPLETE for gps enable command
			 * and previoulsy gps tracking has not been started, start now.
			 */
			//else if (_gpsCurrentMode==GPS_MODE_ENABLE && gps_initiated != GPS_MODE_ENABLE)
			else if (_gpsCurrentMode==GPS_MODE_ENABLE)
			{
				// hard-coded : 255 second timeout, continue, 30 meters
				cnsStartTracking(1,GPS_DEFAULT_ACCURACY,GPS_DEFAULT_TIMEOUT,GPS_DEFAULT_FIX_COUNT,GPS_DEFAULT_FIX_RATE);
    			gps_initiated = GPS_MODE_ENABLE;
			}

			else if (_gpsCurrentMode==GPS_MODE_AGPS)
			{
				/* When received REPORT_LOCATION_FIX_COMPLETE for agps command
				 * and previoulsy location fix has been started, this is the last stage
				 * of agps process so now start standalone gps tracking here.
				 */
        		if (gps_initiated==GPS_MODE_AGPS)
        		{
					// set status - done
					if (!dbSetStat(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT, _gpsError?database_reserved_msg_error:database_reserved_msg_done, NULL))
						syslog(LOG_ERR, "dbSetStat(done) failed - 0x%04x", SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION);

					if(_gpsInTracking)
					{
						// hard-coded : 255 second timeout, continue, 30 meters
						cnsStartTracking(1,GPS_DEFAULT_ACCURACY,GPS_DEFAULT_TIMEOUT,GPS_DEFAULT_FIX_COUNT,GPS_DEFAULT_FIX_RATE);
            			gps_initiated = GPS_MODE_ENABLE;
					}
        		}

				/* When received REPORT_LOCATION_FIX_COMPLETE for agps command
				 * and previoulsy location fix has not been started, location fix
				 * should begin here.
				 */
        		else
        		{
    				// hard-coded : 30 second
    				cnsSingleLocFix(3,-1,_gpsTimeout);
        			gps_initiated = GPS_MODE_AGPS;
        		}
			}

			fProcessed = TRUE;
			break;
		}
#endif  /* GPS_ON_AT */
#endif /* GPS_ENABLED */

		case SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM:
		{
			char* pStat = (char*)pParam;
			syslog(LOG_DEBUG, "got CNS_COPY_MO_SMS_MESSAGE_TO_SIM noti - %s, pStat = 0x%02x, %s",
					STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat), pStat[0], STRCONVERT_FUNCTION_CALL(SmsStatMsg, pStat[0]));
			cnsSendSmsFromQueue();

			if (cnsStat == sierracns_stat_success) {
				if (pStat[0] != 0x00) {
					if (!dbSetStat(CNSMGR_DB_KEY_SMS, 5, database_reserved_msg_error, (char *)STRCONVERT_FUNCTION_CALL(SmsStatMsg, pStat[0])))
						syslog(LOG_ERR, "dbSetStat(error) failed");
				}
			} else {
				if (!dbSetStat(CNSMGR_DB_KEY_SMS, 5, database_reserved_msg_error, (char *)STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat)))
					syslog(LOG_ERR, "dbSetStat(error) failed");
			}

			fProcessed = TRUE;
			break;
		}

		case SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK:
		{
			char* pStat = (char*)pParam;
			syslog(LOG_DEBUG, "got CNS_COPY_MO_SMS_MESSAGE_TO_NETWORK noti - %s, pStat = 0x%02x, %s",
					STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat), pStat[0], STRCONVERT_FUNCTION_CALL(SmsStatMsg, pStat[0]));
			cnsSendSmsFromQueue();

			/* For SMS Tx, tx result should be set after receiving positive response
			 * for SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK command else tx error should be set.
			 * Tx result should be set separately to avoid conflict with incoming SMS
			 * */
			if (cnsStat == sierracns_stat_success) {
				if (pStat[0] == 0x00) {
					if (!dbSetStat(CNSMGR_DB_KEY_SMS, 5, database_reserved_msg_done, "send"))
						syslog(LOG_ERR, "dbSetStat(done) failed");
				} else {
					if (!dbSetStat(CNSMGR_DB_KEY_SMS, 5, database_reserved_msg_error, (char *)STRCONVERT_FUNCTION_CALL(SmsStatMsg, pStat[0])))
						syslog(LOG_ERR, "dbSetStat(error) failed");
				}
			} else {
				if (!dbSetStat(CNSMGR_DB_KEY_SMS, 5, database_reserved_msg_error, (char *)STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat)))
					syslog(LOG_ERR, "dbSetStat(error) failed");
			}

			fProcessed = TRUE;
			break;
		}

		case SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS:
		{
			smsconfigurationdetail* pConfDetail = (smsconfigurationdetail*)pParam;
			char smsc_addr[32], tmpaddr[32];
			if (!dbSetStr(SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, 0, ""))
				syslog(LOG_ERR, "failed to write %s",dbGetKey(SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, 0));
				syslog(LOG_INFO, "_runtimeStat.smsConfigurationDetail.bServCenAddrPresent = %d",_runtimeStat.smsConfigurationDetail.bServCenAddrPresent);
				syslog(LOG_INFO, "pConfDetail->bServCenAddrPresent = %d",pConfDetail->bServCenAddrPresent);
				syslog(LOG_INFO, "pConfDetail->servCenAddrInfo.bLenOfAddrPhoneNo = %d",pConfDetail->servCenAddrInfo.bLenOfAddrPhoneNo);
			if (cnsStat == sierracns_stat_success && sizeof(_runtimeStat.smsConfigurationDetail) == cbParam) {
				memcpy(&_runtimeStat.smsConfigurationDetail, pConfDetail, sizeof(_runtimeStat.smsConfigurationDetail));
				/* copy smsc addr to rdb variable */
				if (_runtimeStat.smsConfigurationDetail.bServCenAddrPresent) {
					(void) memset(smsc_addr, 0x00, 32);
					(void) memset(tmpaddr, 0x00, 32);
					cnsConvSmscAddressField((char *)&_runtimeStat.smsConfigurationDetail.servCenAddrInfo.achAddrPhoneNo[0],
						_runtimeStat.smsConfigurationDetail.servCenAddrInfo.bLenOfAddrPhoneNo, (unsigned char *)&tmpaddr);
					sprintf(smsc_addr, "%s%s", (_runtimeStat.smsConfigurationDetail.servCenAddrInfo.bAddrType == 1)? "+":"", tmpaddr);
					syslog(LOG_ERR, "smsc_addr = %s",smsc_addr);
					if (!dbSetStr(SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, 0, smsc_addr))
						syslog(LOG_ERR, "failed to write %s",dbGetKey(SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, 0));
					_runtimeStat.fRecvSmsCfgDetail = TRUE;
				}
			} else {
				__zeroObj(&_runtimeStat.smsConfigurationDetail);
			}

			fProcessed = TRUE;
			break;
		}
	}

	return fProcessed;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_updateDatabase(char* ppValueList[], int cValueCnt, int nObjId, int iKeyIndexer)
{
	int iValue;
	__pointTypeOf(ppValueList[0]) ppValue;

	char* szRawKey;
	char achKeyBuf[MAX_NAME_LENGTH];
	char* szKey = achKeyBuf;

	// write database entry
	for (iValue = 0, ppValue = &ppValueList[iValue];iValue < cValueCnt;ppValue = &ppValueList[++iValue])
	{
		if (__isNotAssigned(*ppValue))
			continue;

		// get database key
		if (__isNotAssigned(szRawKey = 	dbGetKey(nObjId, iValue)))
		{
			syslog(LOG_INFO, "unknown wObjId received - database key not found (nObjId=%04x, iIdx=%d)", nObjId, iValue);
			continue;
		}

		if (!cnsmgr_is_BasicCnsPacket(nObjId, iValue))
			continue;

		// cook the key if needed
		if (iKeyIndexer < 0)
			szKey = szRawKey;
		else
			sprintf(szKey, szRawKey, iKeyIndexer);

		char* szPrefix=dbGetKeyPrefix(nObjId,iValue);

		if (!dbWrite(szKey, *ppValue, szPrefix))
			return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsStopTracking()
{
	struct
	{
		uint16_t version;
		uint16_t session_type;
	} __packedStruct stop_packet =
	{
		  .version = htons( 0x0001 )
		, .session_type = htons( 0x0000 )
	};

	return sierracns_write( _pCns, SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION, SIERRACNS_PACKET_OP_SET, &stop_packet, sizeof( stop_packet ) );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsSingleLocFix(int nFixType, int nAccuracy,int nTimeOut)
{
	struct
	{
		uint16_t version;
		uint16_t location_fix_type;
		uint16_t satellite_acquisition_timeout;
		uint32_t accuracy;
	} __packedStruct initLocFix;

	initLocFix.version=htons( 0x0001 );
	initLocFix.location_fix_type = htons( (uint16_t)nFixType );
	initLocFix.satellite_acquisition_timeout = htons( (uint16_t)(nTimeOut<255?nTimeOut:255) );
	initLocFix.accuracy = htonl( nAccuracy<0?0xFFFFFFF0u:nAccuracy );

	return sierracns_write( _pCns, SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX, SIERRACNS_PACKET_OP_SET, &initLocFix, sizeof( initLocFix ) );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_callbackOnCnsSetResp(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, sierracns_stat cnsStat)
{
	const char* szError = STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_error);

	char* ppValueList[CNSMGR_MAX_SUBITEM] = {NULL, };
	int cbMaxLen = SIERRACNS_PACKET_PARAMMAXLEN + sizeof(szError) + 1;

	int iValue;
	__pointTypeOf(ppValueList[0]) ppValue;

	int iKeyIndexer = -1;

	if (!cnsmgr_is_BasicCnsPacket(wObjId, -1))
		goto success;

	if (_globalConfig.fSmsDisabled &&
	   (wObjId == SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM ||
	    wObjId == SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK ||
	    wObjId == SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS))
    {
		return TRUE;
	}

	// switch for database operation
	switch (wObjId)
	{
#ifdef GPS_ENABLED
#ifndef GPS_ON_AT
		case SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX:
		{
			struct
			{
				uint16_t version;
				uint16_t error;
			} __packedStruct* pLocFix = pParam;

			#ifdef GPS_PKT_DEBUG
			syslog(LOG_ERR, "<--SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX, error 0x%04x", pLocFix->error);
			#endif

			if(pLocFix->error)
			{
				char achBuf[CNSMGR_MAX_VALUE_LENGTH];
				sprintf(achBuf,"PD error (%s)",(char*)STRCONVERT_FUNCTION_CALL(PDError, ntohs(pLocFix->error)));

				// error code
				if (!dbSetStr(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4, achBuf))
					syslog(LOG_ERR, "failed to write to %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4));

				// set status - error
				if (!dbSetStat(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, NULL))
					syslog(LOG_ERR, "failed to write to %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT));

			}
        	_gpsError=pLocFix->error;

			break;
		}

		case SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION:
		{
			struct
			{
				uint16_t version;
				uint16_t error;
			} __packedStruct* pTrackingSession = pParam;

			#ifdef GPS_PKT_DEBUG
			syslog(LOG_ERR, "<--SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION");
			#endif

			if(_gpsCurrentMode==GPS_MODE_ENABLE)
			{
				char achBuf[CNSMGR_MAX_VALUE_LENGTH];

				/* Do not treat as error for PD error 13 "Session already active" */
				if(pTrackingSession->error && strcmp((char*)STRCONVERT_FUNCTION_CALL(PDError, ntohs(pTrackingSession->error)), "Session already active") != 0)
				{
					sprintf(achBuf,"PD error (%s)",(char*)STRCONVERT_FUNCTION_CALL(PDError, ntohs(pTrackingSession->error)));

					// error code
					if (!dbSetStr(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4, achBuf))
						syslog(LOG_ERR, "failed to write to %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4));

					// set status - error
					if (!dbSetStat(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, NULL))
						syslog(LOG_ERR, "failed to write to %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT));
				}
				else
				{
					pTrackingSession->error = 0;
					// set status - done
					if (!dbSetStat(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
						syslog(LOG_ERR, "failed to write to %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT));
				}
			}
        	_gpsError=pTrackingSession->error;
			break;
		}

		case SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION:
		{
			struct
			{
				uint16_t version;
				uint16_t error;
			} __packedStruct* pLocFix = pParam;
			#ifdef GPS_PKT_DEBUG
			syslog(LOG_ERR, "<--SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION, _gpsCurrentMode = %d", _gpsCurrentMode);
			syslog(LOG_ERR, "   error 0x%04x %s", pLocFix->error, (char*)STRCONVERT_FUNCTION_CALL(PDError, ntohs(pLocFix->error)));
			#endif
            /* if the error code is 0x0c00 (no active session), there is no more packet for ending session so
             * start tracking or single location fix here.
             */
			if (pLocFix->error == 0x0c00)
			{
    			if(_gpsCurrentMode==GPS_MODE_DISABLE)
    			{
    					// set status - done
    					if (!dbSetStat(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
    						syslog(LOG_ERR, "failed to write to %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, CNSMGR_DB_SUBKEY_STAT));
    			}
    			else if (_gpsCurrentMode==GPS_MODE_ENABLE)
    			{
    				// hard-coded : 255 second timeout, continue, 30 meters
    				cnsStartTracking(1,GPS_DEFAULT_ACCURACY,GPS_DEFAULT_TIMEOUT,GPS_DEFAULT_FIX_COUNT,GPS_DEFAULT_FIX_RATE);
    			}
    			else if (_gpsCurrentMode==GPS_MODE_AGPS)
    			{
    				// hard-coded : 30 second
    				cnsSingleLocFix(3,-1,_gpsTimeout);
    			}
    		}
        	_gpsError=pLocFix->error;
			break;
		}
#endif  /* GPS_ON_AT */
#endif /* GPS_ENABLED */

		case SIERRACNS_PACKET_OBJ_SELECT_PLMN:
		{
			const char* szStatMsg=STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat);
			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_SELECT_PLMN setr - %s", szStatMsg);

			if(cnsStat!=sierracns_stat_success)
			{
				syslog(LOG_ERR, "SIERRACNS_PACKET_OBJ_SELECT_PLMN setr failed - %s", szStatMsg);

				dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"-1");
				dbSetStr(CNSMGR_DB_PLMN,CNSMGR_DB_SUBKEY_STAT,(char*)szStatMsg);
				if (op_mode_sync_state == WAITING_RESP) {
					op_mode_sync_state = SELECT_PLMN;	// retry if error
				}
			} else if (op_mode_sync_state == WAITING_RESP) {
				op_mode_sync_state = SYNCHRONIZED;		// finish if success
			}
			break;
		}

		case SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST:
		{
			const char* szStatMsg=STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat);
			syslog(LOG_INFO, "##cns## got SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST setr - %s", szStatMsg);

			if(cnsStat!=sierracns_stat_success)
			{
				syslog(LOG_ERR, "SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST setr failed - %s", szStatMsg);
				dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"-1");
				dbSetStr(CNSMGR_DB_PLMN,CNSMGR_DB_SUBKEY_STAT,(char*)szStatMsg);
			}

			break;
		}

		case SIERRACNS_PACKET_OBJ_RETURN_IP_ADDERSS:
		{
			struct
			{
				BYTE bProifleId;
				BYTE bLenOfIp;
				BYTE ipAddress[16];
			} __packedStruct *pRetIpAddress = pParam;

			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_RETURN_IP_ADDERSS setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));

			iKeyIndexer=pRetIpAddress->bProifleId-1;

			__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
			sprintf(ppValueList[0],"%d.%d.%d.%d",pRetIpAddress->ipAddress[0],pRetIpAddress->ipAddress[1],pRetIpAddress->ipAddress[2],pRetIpAddress->ipAddress[3]);

			break;
		}

		case SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION:
		{
			struct
			{
				BYTE bProifleId;
				BYTE bActStat;
				BYTE bStatCode;
				char szErr[33];
			} __packedStruct* pManPckSession = pParam;

			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));
			syslog(LOG_INFO, "bProifleId=%d, bActStat=%d, bStatCode=%d, err=%s", pManPckSession->bProifleId, pManPckSession->bActStat, pManPckSession->bStatCode, pManPckSession->szErr );

			if(cnsStat == sierracns_stat_timeout && pManPckSession && cbParam==sizeof(*pManPckSession))
			{
				iKeyIndexer=pManPckSession->bProifleId-1;

				__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
				strcpy(ppValueList[1], "0x10000");
			}
			break;
		}

		case SIERRACNS_PACKET_OBJ_MANAGE_PROFILE:
		{
			struct
			{
				WORD wVer;
				BYTE bReqStat;
			} __packedStruct* pMngProfile = pParam;

			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_MANAGE_PROFILE setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));

			syslog(LOG_INFO, "version = %d", ntohs(pMngProfile->wVer));

			switch(pMngProfile->bReqStat) {
				case 0:
					syslog(LOG_INFO, "request success");
					break;

				case 1:
					syslog(LOG_ERR, "UE does not support extended username/password string length");
					break;

				case 0xff:
					syslog(LOG_ERR, "General failure");
					break;

				default:
					syslog(LOG_ERR, "unknown error - %d",pMngProfile->bReqStat);
					break;
			}

			break;
		}

		case SIERRACNS_PACKET_OBJ_WRITE_PROFILE:
		{
			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_WRITE_PROFILE setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));

			//struct
			//{
			//	BYTE bProfileId;
			//	BYTE bReqAction;
			//} __packedStruct* pActiveSession=pParam;

			//syslog(LOG_DEBUG, "profile id = %d", pActiveSession->bProfileId);
			break;
		}

		case SIERRACNS_PACKET_OBJ_READ_PROFILE:
		{
			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_READ_PROFILE setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));

			struct cnsprofile_t* pProfile=(struct cnsprofile_t*)pParam;

			char achBuf[256]={0,};

			syslog(LOG_INFO, "profile id = %d", pProfile->bProfileId);

			strncpy(achBuf,(char*)pProfile->szUserName,sizeof(pProfile->szUserName));
			achBuf[sizeof(pProfile->szUserName)]=0;
			syslog(LOG_INFO, "user name = %s", achBuf);

			strncpy(achBuf,(char*)pProfile->szPassword,sizeof(pProfile->szPassword));
			achBuf[sizeof(pProfile->szPassword)]=0;
			syslog(LOG_INFO, "password  = %s", achBuf);

			strncpy(achBuf,(char*)pProfile->apn,pProfile->bApnLen);
			achBuf[pProfile->bApnLen]=0;
			syslog(LOG_INFO, "apn = %s", achBuf);

			break;
		}

		case SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS:
		{
			char* lpszStatus;

			__goToErrorIfFalse(ppValueList[5] = __alloc(cbMaxLen));

			if (cnsStat == sierracns_stat_failure || cnsStat == sierracns_stat_timeout)
				lpszStatus = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_error);
			else
				lpszStatus = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_done);

			strcpy(ppValueList[5], lpszStatus);
			break;
		}

		case SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND:
		{
			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));

			__goToErrorIfFalse(ppValueList[CNSMGR_DB_SUBKEY_STAT] = __alloc(cbMaxLen));

			switch (cnsStat)
			{
				case sierracns_stat_timeout:
				{
					char* pErr = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_error);

					sprintf(ppValueList[CNSMGR_DB_SUBKEY_STAT], "%s timeout", pErr);
					break;
				}

				case sierracns_stat_failure:
				{
					char* pErr = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_error);

					strcpy(ppValueList[CNSMGR_DB_SUBKEY_STAT], pErr);
					strcat(ppValueList[CNSMGR_DB_SUBKEY_STAT], " ");

					char* pMsg = strlen(ppValueList[CNSMGR_DB_SUBKEY_STAT]) + ppValueList[CNSMGR_DB_SUBKEY_STAT];
					utils_strSNCpy(pMsg, pParam, cbParam);

					break;
				}

				case sierracns_stat_success:
				{
					char* pErr = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_done);

					strcpy(ppValueList[CNSMGR_DB_SUBKEY_STAT], pErr);
					break;
				}

			}

			break;
		}

		case SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE:
		{
			struct
			{
				unsigned short wObjVer;
				unsigned char bResult;
			} __packedStruct *pSelAdoProfile = pParam;

			__goToErrorIfFalse(ppValueList[15] = __alloc(cbMaxLen));

			char* lpszStatus;

			if (cnsStat == sierracns_stat_failure || cnsStat == sierracns_stat_timeout || pSelAdoProfile->bResult != 0)
				lpszStatus = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_error);
			else
				lpszStatus = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_done);

			strcpy(ppValueList[15], lpszStatus);
			break;
		}

		case SIERRACNS_PACKET_OBJ_REPORT_CALL_PROGRESS:
		{
			printf("response - set SIERRACNS_PACKET_OBJ_REPORT_CALL_PROGRESS\n");
			break;
		}

		case SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM:
		{
			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));
			break;
		}

		case SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK:
		{
			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));
			break;
		}

		case SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS:
		{
			syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS setr - %s", STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat));
			__goToErrorIfFalse(cnsgetSmscAddress());
			break;
		}

		default: // discard silently
			syslog(LOG_DEBUG, "no handler for message Id 0x%02X; discarded", wObjId ); // TODO: remove
			break;
	}

	// update database
	cnsmgr_updateDatabase(ppValueList, __countOf(ppValueList), wObjId, iKeyIndexer);

success:
	// free value
	__forEach(iValue, ppValue, ppValueList)
	{
		__free(*ppValue);
	}

	return TRUE;

error:
	// free value
	__forEach(iValue, ppValue, ppValueList)
	{
		__free(*ppValue);
	}

	return FALSE;
}


void setLED( int csq )
{
#define CSQ_LED_RED 11
#define CSQ_LED_GREEN 8

	if(csq == 0)
	{
		//syslog(LOG_DEBUG, "csq = 0\n");
		system( "gpio l 11 1 0 0 0 0 >/dev/null 2>/dev/null" ); //RED off
		system( "gpio l 8 1 0 0 0 0 >/dev/null 2>/dev/null" );	//GREEN off
	}
	else if(csq >= -86 )
	{
		//syslog(LOG_DEBUG, "csq = %d (strong)\n", csq);
		system( "gpio l 11 1 0 0 0 0 >/dev/null 2>/dev/null" ); //RED off
		system( "gpio l 8 1 1 0 1 15 >/dev/null 2>/dev/null" );	//GREEN on
	}
	else if(csq >= -101 )
	{
		//syslog(LOG_DEBUG, "csq = %d (medium)\n", csq);
		system( "gpio l 11 1 1 0 1 15 >/dev/null 2>/dev/null" ); //RED on
		system( "gpio l 8 1 1 0 1 15 >/dev/null 2>/dev/null" );	 //GREEN on

	}
	else if(csq >= -110 )
	{
		//syslog(LOG_DEBUG, "csq = %d (poor)\n", csq);
		system( "gpio l 11 1 1 0 1 15>/dev/null 2>/dev/null" ); //RED on
		system( "gpio l 8 1 0 0 0 0 >/dev/null 2>/dev/null" );	//GREEN off
	}
}


char* _audioMuteTbl[] = {"Unmuted", "Muted"};

int unlockOnDbConfig()
{
	static char achPinTried[256]={0,};

	char achPin[CNSMGR_MAX_VALUE_LENGTH];
	char achAutoPin[1024];

	int cbValue;

	// get auto pin
	cbValue=sizeof(achAutoPin);
	if(!databasedriver_cmdGetSingle(_pDb,"sim.autopin",achAutoPin,&cbValue,NULL))
	{
		syslog(LOG_CRIT, "failed to get sim.autopin");
		return -1;
	}

	// bypass if auto-pin not set
	if(!atol(achAutoPin)) {
		return 0;
	}

	// get read pin
	cbValue=sizeof(achPin);
	if (!databasedriver_cmdGetSingle(_pDb,"sim.pin",achPin,&cbValue,NULL))
	{
		syslog(LOG_CRIT, "cannot read SIM pin from database");
		return -1;
	}

	if(strlen(achPin) && strcmp(achPinTried,achPin))
	{
		if(!cnsVeriChvCode(0x0001, achPin, NULL))
		{
			syslog(LOG_CRIT, "SIM pin incorrect");
		}
		else
		{
			syslog(LOG_INFO, "SIM pin enabled and unlocked by the pin - %s",achPin);
		}
	}

	// store the pin already we tried not to try again
	strcpy(achPinTried,achPin);

	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsGetOperatorSelMode(void)
{
	return sierracns_writeGet(_pCns, SIERRACNS_PACKET_OBJ_SELECT_PLMN);
}

int IsValidMNC(int nMCC,int nMNC)
{
	struct
	{
		int nMCC;
		int nMNC;
	} mncLenInfo[]=
	{
		{505,1},
		{505,2},
		{505,3},
		{505,4},
		{505,5},
		{505,6},
		{505,8},
		{505,9},
		{505,12},
		{505,13},
		{505,14},
		{505,15},
		{505,16},
		{505,21},
		{505,24},
		{505,38},
		{505,71},
		{505,72},
		{505,88},
		{505,90},
		{505,99},
		{0,0}
	};

	int i=0;

	while(mncLenInfo[i].nMCC && mncLenInfo[i].nMNC)
	{
		if(mncLenInfo[i].nMCC == nMCC && mncLenInfo[i].nMNC == nMNC)
			return 1;
		i++;
	}

	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_callbackOnCnsGetRespDatabase(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, sierracns_stat cnsStat)
{
	const char* szError = STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_error);
	const char* szDone = STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_done);

	char* ppValueList[CNSMGR_MAX_SUBITEM] = {NULL, };
	int cbMaxLen = SIERRACNS_PACKET_PARAMMAXLEN + sizeof(szError) + 1;

	int iKeyIndexer = -1;

	int iValue;
	__pointTypeOf(ppValueList[0]) ppValue;

	time_t tmNow = time(NULL);

	static int fSimLocked=0;

	if (!cnsmgr_is_BasicCnsPacket(wObjId, -1))
		goto success;

	if (_globalConfig.fSmsDisabled &&
	   (wObjId == SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE ||
	    wObjId == SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE))
    {
		goto success;
	}

	// switch for database operation
	switch (cnsStat)
	{
		case sierracns_stat_timeout:
		{
			switch (wObjId)
			{
				case SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST:
				{
					syslog(LOG_INFO, "got SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST setr - timeout");

					dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"-1");
					dbSetStr(CNSMGR_DB_PLMN,CNSMGR_DB_SUBKEY_STAT,(char*)szError);
					break;
				}

				default:
				{
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%s %s", szError, "TIMEOUT");
					break;
				}
			}
			break;
		}

		case sierracns_stat_failure:
		{
			switch (wObjId)
			{
				case SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST:
				{
					syslog(LOG_INFO, "##cns## got SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST getr - failure");

					dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"-1");
					dbSetStr(CNSMGR_DB_PLMN,CNSMGR_DB_SUBKEY_STAT,(char*)szError);
					break;
				}

				default:
				{
					char achParam[SIERRACNS_PACKET_PARAMMAXLEN];

					utils_strSNCpy(achParam, pParam, cbParam);
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%s %s", szError, achParam);
					break;
				}
			}
			break;
		}

		case sierracns_stat_success:
		{
			// get value to write
			switch (wObjId)
			{
				case SIERRACNS_PACKET_OBJ_SELECT_PLMN:
				{
					if(bOpType==SIERRACNS_PACKET_OP_GET_RESPONSE)
					{
						struct
						{
							unsigned char bCurPLMNMode;
						} __packedStruct* pSelPLMN=pParam;

						syslog(LOG_INFO, "##cns## got SIERRACNS_PACKET_OBJ_SELECT_PLMN (OP_GET_RESPONSE)");
						
						dbSetStr(CNSMGR_DB_PLMN,2,pSelPLMN->bCurPLMNMode?"Manual":"Automatic");
					}
					else if(bOpType==SIERRACNS_PACKET_OP_NOTIFICATION)
					{
						struct
						{
							unsigned char bResultOfReq;
						} __packedStruct* pSelPLMN=pParam;
						
						syslog(LOG_INFO, "##cns## got SIERRACNS_PACKET_OBJ_SELECT_PLMN (OP_NOTIFICATION)");

						if(pSelPLMN->bResultOfReq)
						{
							const char* szStatMsg=STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat);

							dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"-1");
							dbSetStr(CNSMGR_DB_PLMN,CNSMGR_DB_SUBKEY_STAT,(char*)szStatMsg);
							if (op_mode_sync_state == WAITING_RESP) {
								op_mode_sync_state = SELECT_PLMN;	// retry if fail
							}
						}
						else
						{
							cnsGetOperatorSelMode();
							dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"7");
							dbSetStr(CNSMGR_DB_PLMN,CNSMGR_DB_SUBKEY_STAT,"[DONE]");
							if (op_mode_sync_state == WAITING_RESP) {
								op_mode_sync_state = SYNCHRONIZED;	// finish if success
							}
						}
					}

					break;
				}

				case SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS:
				{
					struct
					{
						unsigned char bServiceAvail;
					} __packedStruct* pReadiness=pParam;
					
					syslog(LOG_INFO, "##cns## got SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS setr - GET_RESPONSE");

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%d", pReadiness->bServiceAvail);
					dbSetStr(SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS, 0, ppValueList[0]);
					break;
				}

				case SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST:
				{
					static char achPLMNList[CNSMGR_MAX_VALUE_LENGTH];

					if(bOpType==SIERRACNS_PACKET_OP_GET_RESPONSE)
					{
						syslog(LOG_INFO, "##cns## got SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST setr - GET_RESPONSE");

						struct
						{
							unsigned char bLenOfFullPLMNName;
							unsigned char PLMNFullName[62];

							unsigned char bLenOfAbbPLMNName;
							unsigned char PLMNAbbName[26];

							unsigned short wMCC;
							unsigned short wMNC;

							unsigned char bPLMNStat;
							unsigned char bNetworkType;

							unsigned char bMorePLMNsFlag;
						} __packedStruct* pPLMNList = pParam;


						int fAdd=achPLMNList[0];
						char achItem[CNSMGR_MAX_VALUE_LENGTH];
						char achPLMNName[sizeof(pPLMNList->PLMNFullName)];

						// get PLMN name
						__ucs2StrToStr(&pPLMNList->bLenOfFullPLMNName,achPLMNName);
						// get item
						sprintf(achItem,"%s%s,%u,%u,%u,%u",fAdd?"&":"",achPLMNName,ntohs(pPLMNList->wMCC),ntohs(pPLMNList->wMNC),pPLMNList->bPLMNStat,pPLMNList->bNetworkType);
						
						syslog(LOG_INFO,"##cns## achItem=%s",achItem);
						
						// append PLMN
						if(CNSMGR_MAX_VALUE_LENGTH-(strlen(achPLMNList)+strlen(achItem))>0)
							strcat(achPLMNList,achItem);

						if(pPLMNList->bMorePLMNsFlag)
						{
							syslog(LOG_INFO,"##cmd## send SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST #1");
							cnsGetOperator();
						}
						else
						{
							cnsGetOperatorSelMode();
							dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"4");
							dbSetStr(CNSMGR_DB_PLMN,CNSMGR_DB_SUBKEY_STAT,"[done]");
							dbSetStr(CNSMGR_DB_PLMN,4,achPLMNList);
							if (op_mode_sync_state == WAITING_RESP) {
								op_mode_sync_state = SELECT_PLMN;	// ready to select PLMN after reading PLMN list
							}
						}
					}
					else if(bOpType==SIERRACNS_PACKET_OP_NOTIFICATION)
					{
						syslog(LOG_INFO, "##cns## got SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST setr - OP_NOTIFICATION");

						struct
						{
							unsigned char bManualAvail;
						} __packedStruct* pPLMNAvail = pParam;


						dbSetInt(CNSMGR_DB_PLMN,5,pPLMNAvail->bManualAvail);

						// clear previous PLMN list
						achPLMNList[0]=0;
						
						if(!pPLMNAvail->bManualAvail) {
							syslog(LOG_INFO,"##cns## no PLMNs are available");
						}
							
						cnsGetOperator();
						
					}
					else
					{
						syslog(LOG_ERR, "unknown response received - 0x%02x",bOpType);
					}

					break;
				}

				case SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE:
				{
					struct
					{
						unsigned short wObjVer;
						unsigned short wCurProfile;
						unsigned char bEarpiece;
						unsigned char bMicrophone;
						unsigned char bAudioVolume;
					} __packedStruct *pSelAdoProfile = pParam;

					// get current audio profile
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%d", ntohs(pSelAdoProfile->wCurProfile));

					if (pSelAdoProfile->wObjVer == 2)
					{
						const char* audioMuteTbl[__countOf(_audioMuteTbl)+1];

						memcpy(audioMuteTbl, _audioMuteTbl, sizeof(_audioMuteTbl));
						audioMuteTbl[__countOf(_audioMuteTbl)-1] = szError;

						// get earpiece status
						__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
						strcpy(ppValueList[1], audioMuteTbl[__getRanged(pSelAdoProfile->bEarpiece,__countOf(_audioMuteTbl))]);

						// get microphone status
						__goToErrorIfFalse(ppValueList[2] = __alloc(cbMaxLen));
						strcpy(ppValueList[1], audioMuteTbl[__getRanged(pSelAdoProfile->bMicrophone,__countOf(_audioMuteTbl))]);

						// get audio volume
						__goToErrorIfFalse(ppValueList[3] = __alloc(cbMaxLen));
						sprintf(ppValueList[3], "%d", pSelAdoProfile->bAudioVolume);
					}

					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_PROFILE_SUMMARY:
				{
					struct
					{
						BYTE bMaxProfile;
						BYTE bMaxTFT;
						BYTE bDefUserProfile;
						BYTE profileStat[16];
					} __packedStruct *pProfileSummary = pParam;

					syslog(LOG_INFO, "max=%d, maxtft=%d, def=%d", pProfileSummary->bMaxProfile, pProfileSummary->bMaxTFT, pProfileSummary->bDefUserProfile);

					int i;
					for(i=0;i<16;i++)
						syslog(LOG_INFO, "stat%d=%d", i, pProfileSummary->profileStat[i]);


					break;
				}

				case SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION:
				{
					struct
					{
						BYTE bProifleId;
						BYTE bActStat;
						BYTE __notImplemented[22];
						WORD wReasonCode;
					} __packedStruct *pPckSession = pParam;


					// bypass if invalid profile id
					if (!(1 <= pPckSession->bProifleId && pPckSession->bProifleId < 16))
						break;

					// use key indexer
					iKeyIndexer = pPckSession->bProifleId-1;

					// act stat
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%d", pPckSession->bActStat);
					active_pdp_session=pPckSession->bActStat;

					// reason code
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					sprintf(ppValueList[1], "0x%04x", ntohs(pPckSession->wReasonCode));

					break;
				}

				case SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND:
				{
					struct
					{
						WORD wVersion;
						long long l64BandGroup;
						BYTE bDevSpecBandGroupNumber;
						UINT64 l64SupportedGroups[0];
					} __packedStruct *pSetCurBand = pParam;

					__goToErrorIfFalse(ppValueList[0] = __alloc(MAX_BAND_LIST_LENGTH));
					__goToErrorIfFalse(ppValueList[2] = __alloc(cbMaxLen));

					char* pSelBand=__alloc(cbMaxLen);
					if (!pSelBand) {
						syslog(LOG_ERR, "memory alloc error for %d bytes", cbMaxLen);
						__goToError();
					}

					// get band selection
					UINT64 ul64SelBand = __ntohl64(pSetCurBand->l64BandGroup);
					if (!cnsmgr_convertBandToStr(ul64SelBand, pSelBand))
						sprintf(pSelBand,"0x%016llx",ul64SelBand);

					strcpy(ppValueList[2],pSelBand);


					int iGroup;
					char* pBand=__alloc(cbMaxLen);
					if (!pBand) {
						syslog(LOG_ERR, "memory alloc error for %d bytes", cbMaxLen);
						__free(pSelBand);
						__goToError();
					}
					for (iGroup = 0; iGroup < pSetCurBand->bDevSpecBandGroupNumber; iGroup++)
					{

						UINT64 ul64Band = __ntohl64(pSetCurBand->l64SupportedGroups[iGroup]);
						if (!cnsmgr_convertBandToStr(ul64Band, pBand))
							sprintf(pBand,"0x%016llx",ul64Band);

						strcat(ppValueList[0], pBand);
						strcat(ppValueList[0], ";");

					}
					/* delete last ';' to prevent empty drop menu in web menu */
					memset(ppValueList[0]+strlen(ppValueList[0])-1, 0,1);
					__free(pBand);
					__free(pSelBand);
					break;
				}

				case SIERRACNS_PACKET_OBJ_REPORT_CALL_PROGRESS:
				{
					struct
					{
						unsigned short wCallId;
						unsigned char bCallProgress;
						unsigned char bError;
						unsigned char bPhoneNoInfoValid;
						unsigned char bPresentIndi;
						unsigned char bScrindi;
						unsigned char bInternationalNoFlag;
						unsigned char bPhoneNoLen;
						unsigned char phoneNo[20];
						unsigned char bCallType;
						unsigned char __reservied[163];
					} __packedStruct *pCallProg = pParam;

					const char* callProgTbl[] =
					{
						RDB_VOICECALL_PROGRESS_FAILED,
						RDB_VOICECALL_PROGRESS_CONNECTING,
						RDB_VOICECALL_PROGRESS_QUEUED,
						RDB_VOICECALL_PROGRESS_WAITING,
						RDB_VOICECALL_PROGRESS_INCOMING,
						RDB_VOICECALL_PROGRESS_RINGING,
						RDB_VOICECALL_PROGRESS_CONNECTED,
						RDB_VOICECALL_PROGRESS_DISCONNECTING,
						RDB_VOICECALL_PROGRESS_DISCONNECTED,
						RDB_VOICECALL_PROGRESS_FORWARD_CALL_INCOMING
					};

					const char* szPrgMsg;

					// call progress
					{
						// get progress message
						szPrgMsg = cnsmgr_convertToStr(pCallProg->bCallProgress, callProgTbl, __countOf(callProgTbl));

						// if unknown return
						if (!szPrgMsg)
							szPrgMsg = STRCONVERT_FUNCTION_CALL(DatabaseMsg, database_reserved_msg_unknown);

						// strcpy
						__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
						strcpy(ppValueList[1], szPrgMsg);
					}

					// call id
					{
						__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
						sprintf(ppValueList[0], "%d", ntohs(pCallProg->wCallId));
					}

					break;
				}

				case SIERRACNS_PACKET_OBJ_MANAGE_MISSED_VOICE_CALLS_COUNT:
				{
					struct
					{
						unsigned char bMissedVoiceCallCount;
					} __packedStruct *pMissedVoiceCallCount = pParam;

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%d", pMissedVoiceCallCount->bMissedVoiceCallCount);

					break;
				}

				case SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS:
				{
					struct
					{
						unsigned char bRecCnt;
						unsigned char bUnreadCnt;
						unsigned char bMemExFlag;
					} __packedStruct *pMsgStat = pParam;

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "Total:%d read:x unread:%d sent:x unsent:x, full:%s",
							pMsgStat->bRecCnt, pMsgStat->bUnreadCnt, __getStringBoolean(pMsgStat->bMemExFlag));
					strcpy(ppValueList[1], ppValueList[0]);

					/* support SMS tools : if there is any unread message, call sms_handler script to read unread message and store */
					//if (pMsgStat->bUnreadCnt > 0)
					//{
					//	system("/usr/bin/sms_handler.sh rx &");
					//}

					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_VERSION:
				{
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					__goToErrorIfFalse(utils_strTrimCpy(ppValueList[0], (char*)pParam, cbParam));

					strcpy(_modFirmwareVersoin,(char*)pParam);
					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_BUILD_DATE:
				case SIERRACNS_PACKET_OBJ_RETURN_HARDWARE_VERSION:
				case SIERRACNS_PACKET_OBJ_RETURN_BOOT_VERSION:
				case SIERRACNS_PACKET_OBJ_RETURN_IMEI:
				{
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					__goToErrorIfFalse(utils_strTrimCpy(ppValueList[0], (char*)pParam, cbParam));
					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_ICCID:
				{
					struct
					{
						BYTE achCCID[10];

					} __packedStruct* pRetICCID = pParam;

					BYTE* achCCID=pRetICCID->achCCID;

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0],"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",achCCID[0],achCCID[1],achCCID[2],achCCID[3],achCCID[4],achCCID[5],achCCID[6],achCCID[7],achCCID[8],achCCID[9]);
					break;
				}

				case SIERRACNS_PACKET_OBJ_AVAILABLE_SERVICE_DETAIL:
				{
					struct
					{
						WORD wObjVer;
						BYTE bDispServIcon;
						BYTE bServTypeAvail;
						BYTE bGPRSAtt;
						BYTE bPckSessionAct;
						BYTE bReserved[239];

					} __packedStruct* pServDetail = pParam;

					// get network attached
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					if(pServDetail->bServTypeAvail)
						sprintf(ppValueList[0],"1");
					else
						sprintf(ppValueList[0],"0");

					// get pdp status
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					if(pServDetail->bPckSessionAct)
						sprintf(ppValueList[1],"up");
					else
						sprintf(ppValueList[1],"down");

					break;
				}

				case SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS:
				{
					struct
					{
						WORD wObjVer;
						WORD wModemStat;
						WORD wServStat;
						WORD wServErr;
						BYTE bServType;
						BYTE bSysMode;
						WORD wCurBand;
						BYTE bRoamStat;
						BYTE bManualMode;
						BYTE bCtryStrLen;
						char achCtryStr[6];
						BYTE bNetworkStrLen;
						char achNetworkStr[16];
						WORD wMCC;
						WORD wMNC;
						WORD wLAC;
						WORD wRAC;
						WORD wCellID;
						WORD wChNumber;
						WORD wPriScrCode;
						BYTE bMNCFmt;
						BYTE bPLMNServErr;
						DWORD dCellID;
					} __packedStruct* pSysNetworkStat = pParam;

					// modem status
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					strcpy(ppValueList[0],  STRCONVERT_FUNCTION_CALL(ModemStat, ntohs(pSysNetworkStat->wModemStat)));

					// service status
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					strcpy(ppValueList[1],  STRCONVERT_FUNCTION_CALL(SysServStat, ntohs(pSysNetworkStat->wServStat)));

					// service type
					__goToErrorIfFalse(ppValueList[3] = __alloc(cbMaxLen));
					strcpy(ppValueList[3],  STRCONVERT_FUNCTION_CALL(ServType, pSysNetworkStat->bServType));

					// system mode
					__goToErrorIfFalse(ppValueList[2] = __alloc(cbMaxLen));
					strcpy(ppValueList[2],  STRCONVERT_FUNCTION_CALL(SysMode, pSysNetworkStat->bSysMode));

					// current band
					__goToErrorIfFalse(ppValueList[4] = __alloc(cbMaxLen));
					strcpy(ppValueList[4],  cnsmgr_convertSysBandToStr(ntohs(pSysNetworkStat->wCurBand)));

					// roaming status
					__goToErrorIfFalse(ppValueList[5] = __alloc(cbMaxLen));
					strcpy(ppValueList[5],  STRCONVERT_FUNCTION_CALL(RoamStat, pSysNetworkStat->bRoamStat));

					/* add for compatibility with simple_at_manager */
					__goToErrorIfFalse(ppValueList[15] = __alloc(cbMaxLen));
					strcpy(ppValueList[15],  STRCONVERT_FUNCTION_CALL(RoamStat2, pSysNetworkStat->bRoamStat));

					// manual mode
					__goToErrorIfFalse(ppValueList[6] = __alloc(cbMaxLen));
					strcpy(ppValueList[6],  STRCONVERT_FUNCTION_CALL(SysManualMode, pSysNetworkStat->bManualMode));

					// contry string
					__goToErrorIfFalse(ppValueList[7] = __alloc(cbMaxLen));
					utils_strConv2Bytes(ppValueList[7], pSysNetworkStat->achCtryStr, pSysNetworkStat->bCtryStrLen);

					// network string
					__goToErrorIfFalse(ppValueList[8] = __alloc(cbMaxLen));
					utils_strConv2Bytes(ppValueList[8], pSysNetworkStat->achNetworkStr, pSysNetworkStat->bNetworkStrLen);
                    /* replace "Telstra Mobile" to "Telstra" */
                    str_replace(&ppValueList[8][0], "Telstra Mobile", "Telstra");
                    /* replace "3Telstra" to "Telstra" */
                    str_replace(&ppValueList[8][0], "3Telstra", "Telstra");

					// MCC
					__goToErrorIfFalse(ppValueList[9] = __alloc(cbMaxLen));
					sprintf(ppValueList[9], "%d", ntohs(pSysNetworkStat->wMCC));
					// MNC
					__goToErrorIfFalse(ppValueList[10] = __alloc(cbMaxLen));
					sprintf(ppValueList[10], "%d", ntohs(pSysNetworkStat->wMNC));
					// LAC
					__goToErrorIfFalse(ppValueList[11] = __alloc(cbMaxLen));
					sprintf(ppValueList[11], "%d", ntohs(pSysNetworkStat->wLAC));
					// RAC
					__goToErrorIfFalse(ppValueList[12] = __alloc(cbMaxLen));
					sprintf(ppValueList[12], "%d", ntohs(pSysNetworkStat->wRAC));
					// Cell ID
					__goToErrorIfFalse(ppValueList[13] = __alloc(cbMaxLen));
//syslog(LOG_ERR, "pSysNetworkStat->wObjVer=%u\n",pSysNetworkStat->wObjVer);
					if(pSysNetworkStat->wObjVer==0x03)
					  sprintf(ppValueList[13], "%d", ntohs(pSysNetworkStat->dCellID));
					else
					  sprintf(ppValueList[13], "%d", ntohs(pSysNetworkStat->wCellID));
					// Cnannel Number
					__goToErrorIfFalse(ppValueList[14] = __alloc(cbMaxLen));
					sprintf(ppValueList[14], "%d", ntohs(pSysNetworkStat->wChNumber));
					break;
				}


				case SIERRACNS_PACKET_OBJ_REPORT_NETWORK_STATUS:
				{
					struct
					{
						WORD wServStat;
						BYTE bServType;
						BYTE bRoamStat;
						BYTE bNetworkSelMode;
						BYTE bCtryStrLen;
						char achCtryStr[4];
						BYTE bNetworkStrLen;
						char achNetworkStr[8];
						WORD wMCC;
						WORD wMNC;
						WORD wLAC;
						WORD wCellID;
					} __packedStruct* pNetworkStat=pParam;
					
					BYTE bServType;
					BYTE bRoamStat;
					WORD wServStat;
					int reg_stat;
					
					/*
						+CREG?
						
						0 not registered, MT is not currently searching an operator to register to
						1 registered, home network
						2 not registered, but MT is currently trying to attach or searching an operator to register to
						3 registration denied
						4 unknown
						5 registered, roaming
					*/
					
					/*
						service status
						
						• 0x0000=Normal
						• 0x0001=Emergency only
						• 0x0002=No service
						• 0x0003=Access difficulty
						• 0x0004=Forbidden PLMN
						• 0x0005=Location area is forbidden
						• 0x0006=National roaming is forbidden
						• 0x0007=Illegal mobile station
						• 0x0008=Illegal mobile equipment
						• 0x0009=IMSI unknown in HLR
						• 0x000A=Authentication failure
						• 0x000B=GPRS failed
						• 0x000C–0xFFFF=Reserved					
					*/

					/* convert to the result of +CREG? */

					bServType=pNetworkStat->bServType;
					wServStat=ntohs(pNetworkStat->wServStat);
					bRoamStat=pNetworkStat->bRoamStat;
					
					//syslog(LOG_ERR,"bServType=%d,wServStat=%d,bRoamStat=%d",bServType,wServStat,bRoamStat);
					
					reg_stat=4;
					if(bServType<=0x02) {
						if(!bRoamStat)
							reg_stat=1;
						else
							reg_stat=5;
					}
					else {
						if((0x0003<=wServStat && wServStat<=0x000B) || wServStat==0x0001 || wServStat==0x0000)
							reg_stat=0;
						else if(wServStat>=0x000c)
							reg_stat=4;
						else
							reg_stat=2;
						/*
							0 not registered, MT is not currently searching an operator to register to
							4 unknown
							2 not registered, but MT is currently trying to attach or searching an operator to register to
							3 registration denied
						*/							
					}
					
					
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					snprintf(ppValueList[0],cbMaxLen,"%d",reg_stat);


					#if 0
					// service status
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					strcpy(ppValueList[0],  STRCONVERT_FUNCTION_CALL(ServStat, ntohs(pNetworkStat->wServStat)));

					// service type
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					strcpy(ppValueList[1], STRCONVERT_FUNCTION_CALL(ServType, pNetworkStat->bServType));

					// roaming status
					__goToErrorIfFalse(ppValueList[2] = __alloc(cbMaxLen));
					strcpy(ppValueList[2], STRCONVERT_FUNCTION_CALL(RoamStat, pNetworkStat->bRoamStat));
				
					// network selection mode
					__goToErrorIfFalse(ppValueList[3] = __alloc(cbMaxLen));
					strcpy(ppValueList[3], STRCONVERT_FUNCTION_CALL(NetworkSelMode, pNetworkStat->bNetworkSelMode));

					// contry string
					__goToErrorIfFalse(ppValueList[4] = __alloc(cbMaxLen));
					utils_strSNCpy(ppValueList[4],pNetworkStat->achCtryStr,pNetworkStat->bCtryStrLen);

					// network string
					__goToErrorIfFalse(ppValueList[5] = __alloc(cbMaxLen));
					utils_strSNCpy(ppValueList[5],pNetworkStat->achNetworkStr,pNetworkStat->bNetworkStrLen);

					// MCC
					__goToErrorIfFalse(ppValueList[6] = __alloc(cbMaxLen));
					sprintf(ppValueList[6],"%d",ntohs(pNetworkStat->wMCC));
					// MNC
					__goToErrorIfFalse(ppValueList[7] = __alloc(cbMaxLen));
					sprintf(ppValueList[7],"%d",ntohs(pNetworkStat->wMNC));
					// LAC
					__goToErrorIfFalse(ppValueList[8] = __alloc(cbMaxLen));
					sprintf(ppValueList[8],"%d",ntohs(pNetworkStat->wLAC));
					// Cell ID
					__goToErrorIfFalse(ppValueList[9] = __alloc(cbMaxLen));
					sprintf(ppValueList[9],"%d",ntohs(pNetworkStat->wCellID));
					#endif

					break;
				}

				case SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE:
				case SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE:
				{
					struct
					{
						smsenvelope smsEnv;
						smsheader smsHdr;
					} __packedStruct *pSmsParam = pParam;

					// empty if the first token
					if (smsrecv_isFirstToken(_pSmsRecv, &pSmsParam->smsEnv))
						smsrecv_emptyList(_pSmsRecv);

					// add the token
					if (!smsrecv_addSmsToken(_pSmsRecv, &pSmsParam->smsEnv))
					{
						syslog(LOG_ERR, "sms allocated failure");
						__goToError();
					}

					// get more sms tokens
					if (!smsrecv_isCompleted(_pSmsRecv))
					{
						if (wObjId == SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE)
							cnsReadUnreadSms();
						else
							cnsReadReadSms();

						break;
					}

					int nMsgId;

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));

					// get msg id
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					if (!smsrecv_getMsgId(_pSmsRecv, &nMsgId))
					{
						sprintf(ppValueList[0], "%s %s", szError, "failed to get message id");
						break;
					}
					sprintf(ppValueList[1], "%d", nMsgId);

					// get time stamp
					__goToErrorIfFalse(ppValueList[2] = __alloc(cbMaxLen));
					if (!smsrecv_getTimeStamp(_pSmsRecv, ppValueList[2], cbMaxLen))
					{
						sprintf(ppValueList[0], "%s %s", szError, "failed to get time stamp");
						break;
					}

					// get msg body
					int cbMsgLen = smsrecv_getTotalMsgLen(_pSmsRecv);
					__goToErrorIfFalse(ppValueList[3] = __alloc(MAX_UTF8_BUF_SIZE));
				    SET_SMS_LOG_MASK_TO_DEBUG_LEVEL
				    (void) memset(ppValueList[3], 0x00, MAX_UTF8_BUF_SIZE);
					if (!smsrecv_getMsgBody(_pSmsRecv, ppValueList[3], cbMsgLen))
					{
						sprintf(ppValueList[0], "%s %s", szError, "failed to get message body");
    				    SET_SMS_LOG_MASK_TO_ERROR_LEVEL
						break;
					}
					syslog(LOG_DEBUG, "msg body: %s", ppValueList[3]);
				    SET_SMS_LOG_MASK_TO_ERROR_LEVEL

					// get destination source phone number
					__goToErrorIfFalse(ppValueList[4] = __alloc(cbMaxLen));
					if (!smsrecv_get_dst_phone_no(_pSmsRecv, ppValueList[4], cbMsgLen,
						 sms_destination_source_phone_number))
					{
						sprintf(ppValueList[0], "%s %s", szError, "failed to get dest src no");
						break;
					}

					// get destination service center phone number
					__goToErrorIfFalse(ppValueList[5] = __alloc(cbMaxLen));
					if (!smsrecv_get_dst_phone_no(_pSmsRecv, ppValueList[5], cbMsgLen,
						 sms_destination_service_center_phone_number))
					{
						sprintf(ppValueList[0], "%s %s", szError, "failed to get dest sc no");
						break;
					}

					// get received time
					__goToErrorIfFalse(ppValueList[6] = __alloc(cbMaxLen));
					if (!smsrecv_get_dst_recv_time(_pSmsRecv, ppValueList[6], cbMaxLen))
					{
						sprintf(ppValueList[0], "%s %s", szError, "failed to get received time");
						break;
					}

					// success
					strcpy(ppValueList[0], szDone);

					break;
				}

				case SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION:
				{
					struct
					{
						unsigned char bChvEnStat;
					} __packedStruct* pStat=pParam;

					#ifdef SIM_DEBUG
					syslog(LOG_ERR,"<-- SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION");
					#endif
					if(pStat->bChvEnStat>1)
						pStat->bChvEnStat=0;

					/* update simcard pin enable status */
					simcard_pin_enabled=pStat->bChvEnStat?1:0;
					
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					strcpy(ppValueList[0], STRCONVERT_FUNCTION_CALL(EnableStat, pStat->bChvEnStat));

					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS:
				{
					struct
					{
						unsigned char bSimStatus;
						unsigned char bUserOpReq;
						unsigned char bPrevUserOp;
						unsigned char bResPrevUserOp;
						unsigned char bRetryInfo;
						unsigned char bRetryInfoType;
						unsigned char bRetryRemaining;
					} __packedStruct *pSimStat = pParam;


					
					/* do scheduled simcard action */
					if(simcard_schedule_enable_pin>=0) {
						int pin_en=simcard_schedule_enable_pin;

						syslog(LOG_DEBUG,"schedule detected, start SIM operation (succ=%d,en=%d,pin=%s)",pSimStat->bResPrevUserOp,simcard_schedule_enable_pin,simcard_pin);
						
						simcard_schedule_enable_pin=-1;
						
						/* if sim operation succeeded */
						if(!pSimStat->bResPrevUserOp) {
							cnsEnChv1(pin_en, simcard_pin);
							break;
						}
						
					}

					fSimLocked=pSimStat->bSimStatus==0x05;
					int fSimPuked=pSimStat->bSimStatus==0x06;

					#ifdef SIM_DEBUG
					syslog(LOG_ERR,"<-- SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS");
					#endif
					if(fSimLocked)
						syslog(LOG_INFO,"SIM lock detected - unlocking flag set");

					// set sim status
					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					if(fSimLocked)
					{
						char* szMsg;
						if(pSimStat->bPrevUserOp==0x04 && pSimStat->bResPrevUserOp!=0x00)
							szMsg="incorrect SIM PIN";
						else
							szMsg="SIM locked";

						sprintf(ppValueList[0],"%s - Remaining count : %d",szMsg, pSimStat->bRetryRemaining);
					}
					else
					{
						strcpy(ppValueList[0], STRCONVERT_FUNCTION_CALL(SimStat, pSimStat->bSimStatus));
					}

					// get user operation requested
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					strcpy(ppValueList[1], STRCONVERT_FUNCTION_CALL(SimUsrOpReq, pSimStat->bUserOpReq));

					// set previous user operation
					__goToErrorIfFalse(ppValueList[2] = __alloc(cbMaxLen));
					strcpy(ppValueList[2], STRCONVERT_FUNCTION_CALL(PrevUsrOpToStr, pSimStat->bPrevUserOp));

					// set result of previous operation
					__goToErrorIfFalse(ppValueList[3] = __alloc(cbMaxLen));
					strcpy(ppValueList[3], STRCONVERT_FUNCTION_CALL(ResPrevUsrOp, pSimStat->bResPrevUserOp));

					// get retry information type
					int nRetryInfoType = pSimStat->bRetryInfoType;
					if (pSimStat->bRetryInfo != 0x01)
						nRetryInfoType = 0x04;

					__goToErrorIfFalse(ppValueList[4] = __alloc(cbMaxLen));
					strcpy(ppValueList[4], STRCONVERT_FUNCTION_CALL(RetryInfoType, nRetryInfoType));

					// get retries remain
					if(fSimPuked)
					{
						__goToErrorIfFalse(ppValueList[5] = __alloc(cbMaxLen));
						sprintf(ppValueList[5], "%d", 0);
						__goToErrorIfFalse(ppValueList[6] = __alloc(cbMaxLen));
						sprintf(ppValueList[6], "%d", pSimStat->bRetryRemaining);
					}
					else
					{
						__goToErrorIfFalse(ppValueList[5] = __alloc(cbMaxLen));
						sprintf(ppValueList[5], "%d", pSimStat->bRetryRemaining);
						__goToErrorIfFalse(ppValueList[6] = __alloc(cbMaxLen));
						sprintf(ppValueList[6], "%d", 10);
					}

					#ifdef SIM_DEBUG
					syslog(LOG_ERR,"------------------------------------------------------------");
					syslog(LOG_ERR,"SIM status          :  %s", STRCONVERT_FUNCTION_CALL(SimStat, pSimStat->bSimStatus));
					syslog(LOG_ERR,"SIM user op req     :  %s", STRCONVERT_FUNCTION_CALL(SimUsrOpReq, pSimStat->bUserOpReq));
					syslog(LOG_ERR,"SIM prev user op:   :  %s", STRCONVERT_FUNCTION_CALL(PrevUsrOpToStr, pSimStat->bPrevUserOp));
					syslog(LOG_ERR,"SIM prev user op res:  %s", STRCONVERT_FUNCTION_CALL(ResPrevUsrOp, pSimStat->bResPrevUserOp));
					syslog(LOG_ERR,"SIM retry info      :  %d", pSimStat->bRetryInfo);
					syslog(LOG_ERR,"SIM retry info type :  %s", STRCONVERT_FUNCTION_CALL(RetryInfoType, nRetryInfoType));
					syslog(LOG_ERR,"SIM retry remaining :  %d", pSimStat->bRetryRemaining);
					syslog(LOG_ERR,"------------------------------------------------------------");
					#endif

					static int renewGetTable[]={
						SIERRACNS_PACKET_OBJ_SERVICE_PROVIDER_NAME,
						SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION
					};

					// initiate CnS packets
					int iObj;
					const int* pObjId;

					// write single get objects
					__forEach(iObj, pObjId, renewGetTable)
					{
						sierracns_writeGet(_pCns, *pObjId);
					}

					/* ----------------------------------------------------------------------------------
					 * unlock SIM only when it is really needed
					 * ----------------------------------------------------------------------------------
					 * Added more condition to do auto unlocking operation because of PIN retry count
					 * consumtion error. It caused by WEBUI modification which leads users turn on
					 * autopin option unconsciously when the user enters PIN code. Previous bug flow is as below;
					 * on 1st try with incorrect PIN: remaining count-- but autopin is 0 so skip unlocking
					 * on 2nd try with incorrect PIN: remaining count-- and autopin is 1 so do unlocking,
					 * 								  remaining count-- again then turn into PUK state.
					 * Allow auto unlocking only when there is no previous PIN operation and previous
					 * operation is successful not to waist retry count after previous SIM operation failed.
					 * Add more condition : Do not try unlocking if SIM operation is done by other manager.
					 *                      All SIM operations may already be blocked but for safety.
					 * If any problem in this logic, please contact Kwonhee
					 */
					if(fSimLocked && pSimStat->bPrevUserOp == 0 && pSimStat->bResPrevUserOp == 0 &&
					    is_cpinc_command_supporting() == 0)
					{
						syslog(LOG_ERR,"SIM lock detected - trying to unlock");
						unlockOnDbConfig();
					}
					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_MODEM_DATE_AND_TIME:
				{
					struct
					{
						unsigned short wObjVer;
						unsigned short wYear;
						unsigned short wMonth;
						unsigned short wDay;
						unsigned short wDayOfWeek;
						unsigned short wHour;
						unsigned short wMin;
						unsigned short wSec;
						unsigned short wTimeZoneOffset;
						unsigned char bDaylightSaving;
					} __packedStruct *pModemDateTime = pParam;

					struct tm tmUTC;
					time_t curDiff;
					time_t time_network;

					// get network time - assume network time is utc
					tmUTC.tm_year = ntohs(pModemDateTime->wYear) - 1900;
					tmUTC.tm_mon = ntohs(pModemDateTime->wMonth) - 1;
					tmUTC.tm_mday = ntohs(pModemDateTime->wDay);
					tmUTC.tm_hour = ntohs(pModemDateTime->wHour);
					tmUTC.tm_min = ntohs(pModemDateTime->wMin);
					tmUTC.tm_sec = ntohs(pModemDateTime->wSec);
					time_network=convert_utc_to_local(&tmUTC);

					// get system time
					curDiff = time_network - tmNow;

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%+ldsec", (long int)(curDiff));

					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_RADIO_TEMPERATURE:
				{
					struct
					{
						unsigned short wObjVer;
						unsigned short wTempState;
						unsigned short wCurTemp;
						unsigned char achReserved[14];
					} __packedStruct *pRadioTemp = pParam;

					static int prevTemp = 0;
					int curTemp = ntohs(pRadioTemp->wCurTemp);

					if (prevTemp != curTemp)
					{
						__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
						sprintf(ppValueList[0], "%d", prevTemp = curTemp);
					}

					break;
				}

				case SIERRACNS_PACKET_OBJ_REPORT_RADIO_INFORMATION:
				{
					struct
					{
						unsigned short wSigLevel;
						unsigned short wAveBitErr;
					} __packedStruct *pRadioInfo = pParam;

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%d dBm", -110 + ntohs(pRadioInfo->wSigLevel));
					setLED( -110 + ntohs(pRadioInfo->wSigLevel) );
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					sprintf(ppValueList[1], "%.02f%%",  18.1 / (128 >> ntohs(pRadioInfo->wAveBitErr)));

					break;
				}

				case SIERRACNS_PACKET_OBJ_HEARTBEAT:
				{
					static long long int lBeatenCnt = 0;

					__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
					sprintf(ppValueList[0], "%lld", ++lBeatenCnt);
					__goToErrorIfFalse(ppValueList[1] = __alloc(cbMaxLen));
					sprintf(ppValueList[1], "%lld", lBeatenCnt);
					
					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_RADIO_VOLTAGE:
				{
					struct
					{
						unsigned short wObjVer;
						unsigned short wVoltStat;
						unsigned short wCurVolt;
						unsigned char achReserved[14];
					} __packedStruct *pRadioVolt = pParam;

					static int prevVolt = 0;
					int curVolt = ntohs(pRadioVolt->wCurVolt);

					if (prevVolt != curVolt)
					{
						__goToErrorIfFalse(ppValueList[0] = __alloc(cbMaxLen));
						sprintf(ppValueList[0], "%dmV", prevVolt = curVolt);
					}

					break;
				}

				//case SIERRACNS_PACKET_OBJ_REPORT_CURRENT_BAND:
				//{
				//	struct
				//	{
				//		unsigned short wObjVer;
				//		unsigned char bBandInUse;
				//		unsigned char bRadioType;
				//		unsigned char bCurBand;
				//	} __packedStruct *pCurBand = pParam;

				//	char* szBand;


				//	if (__isNotAssigned(szBand = cnsmgr_convertBandToStr(pCurBand->bCurBand)))
				//		szBand = (char*)STRCONVERT_FUNCTION_CALL(DatabaseMsg,database_reserved_msg_unknown);

				//	__goToErrorIfFalse(ppValueList[0] = __alloc(strlen(szBand) + 1));
				//	strcpy(ppValueList[0], szBand);

				//	break;
				//}

				//case SIERRACNS_PACKET_OBJ_MANAGE_RADIO_POWER:
				//{
				//	struct
				//	{
				//		WORD wObjVer;
				//		WORD wPowerMode;
				//		WORD wReason;
				//		BYTE bReserved[14];
				//	} __packedStruct *pRadioPower=pParam;

				//	static WORD wPrevPowerMode=-1;
				//	WORD wPowerMode=ntohs(pRadioPower->wPowerMode);
				//	WORD wObjVer=ntohs(pRadioPower->wObjVer);

				//	syslog(LOG_INFO,"version =%d, radio power = %d",wObjVer,wPowerMode);

				//	if(wPrevPowerMode == wPowerMode)
				//		break;
				//	wPrevPowerMode=wPowerMode;

				//	__goToErrorIfFalse(ppValueList[0] = __alloc(CNSMGR_MAX_VALUE_LENGTH));
				//	sprintf(ppValueList[0],"%d",wPowerMode);

				//	// set power
				//	if(wPowerMode==0)
				//	{
				//		static struct
				//		{
				//			BYTE bModemStat;
				//		} __packedStruct radioPower;

				//		radioPower.bModemStat=1;

				//		syslog(LOG_NOTICE, "set radio-power mode");
				//		if(!sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_DISABLE_MODEM, SIERRACNS_PACKET_OP_SET, &radioPower, sizeof(radioPower)))
				//			syslog(LOG_NOTICE, "error");
				//		else
				//			syslog(LOG_NOTICE, "ok");
				//	}

				//	break;

				//}

				case SIERRACNS_PACKET_OBJ_RETURN_IMSI:
				{
					int nMCC;
					int nMNC;

					struct
					{
						WORD wPLMN_MCC;
						WORD wLMN_MNC;
						BYTE bMSIN;
						char achMSIN[10];
					} __packedStruct *pIMSI=pParam;
					__goToErrorIfFalse(ppValueList[0] = __alloc(CNSMGR_MAX_VALUE_LENGTH));
					__goToErrorIfFalse(ppValueList[1] = __alloc(CNSMGR_MAX_VALUE_LENGTH));
					__goToErrorIfFalse(ppValueList[2] = __alloc(CNSMGR_MAX_VALUE_LENGTH));

					// work-around for module-bug  - todo: check the next firmware to make sure this happens only to K2_0_7_30AP
					if(strstr(_modFirmwareVersoin,"K2_0_7_30AP"))
					{
						sprintf(ppValueList[0],"%d",ntohs(pIMSI->wPLMN_MCC));
						sprintf(ppValueList[1],"%d",ntohs(pIMSI->wLMN_MNC));
					}
					else
					{
						sprintf(ppValueList[0],"%x",ntohs(pIMSI->wPLMN_MCC));
						sprintf(ppValueList[1],"%x",ntohs(pIMSI->wLMN_MNC));
					}

					// work-around for invalid MCC length SIM
					nMCC=atoi(ppValueList[0]);
					nMNC=atoi(ppValueList[1]);
					if(nMCC && nMNC)
					{
						int nCorrectedMNC=nMNC/10;

						if((nMCC==505) && !IsValidMNC(nMCC,nMNC) && IsValidMNC(nMCC,nCorrectedMNC))
						{
							if( nMNC%10 == (pIMSI->achMSIN[0]-'0') )
								sprintf(ppValueList[1],"%d",nCorrectedMNC);
						}
					}

					//utils_strSNCpy(ppValueList[2],pIMSI->achMSIN,pIMSI->bMSIN);
					nMNC=atoi(ppValueList[1]);
					sprintf(ppValueList[2],"%d%d", nMCC, nMNC);
					(void)strncat(ppValueList[2],pIMSI->achMSIN,pIMSI->bMSIN); 
					break;
				}

				case SIERRACNS_PACKET_OBJ_SERVICE_PROVIDER_NAME:
				{
					struct
					{
						BYTE cbServProvider;
						char achServProvider[16];
					} __packedStruct *pServProName = pParam;

					__goToErrorIfFalse(ppValueList[0] = __alloc(CNSMGR_MAX_VALUE_LENGTH));
					utils_strSNCpy(ppValueList[0], pServProName->achServProvider, pServProName->cbServProvider);

					break;
				}

				case SIERRACNS_PACKET_OBJ_RSCP:
				{
					#define RSCP_MAX_CELLINFO 6
					struct
					{
						unsigned short wObjVer;

						unsigned short wNumOfCells;

						struct
						{
							unsigned short wPSC;
							unsigned short wRSCP;
							unsigned short wECIO;
						} cellInfo[RSCP_MAX_CELLINFO];

					} __packedStruct *pRSCP = pParam;

					int i;

					char achKey[CNSMGR_MAX_VALUE_LENGTH];
					char achValue[CNSMGR_MAX_VALUE_LENGTH];
					const char* pPrekey;
					int cCells=ntohs(pRSCP->wNumOfCells);

					for(i=0;i<RSCP_MAX_CELLINFO;i++)
					{
						// RSCP
						pPrekey=dbGetKey(wObjId, 0);
						sprintf(achKey,pPrekey,i);

						if(i<cCells)
							sprintf(achValue,"%d",ntohs(pRSCP->cellInfo[i].wRSCP));
						else
							achValue[0]=0;

						dbWrite(achKey,achValue,NULL);

						// ECIO: according to Sierra manuals
						// (refer to Return RSCP and Ec/io section),
						// Ec/io is returned in half-decibel increments.
						// Failing to adjust for this results in the router displaying
						// a worse than real signal-to-interference (Ec/io) ratio.
						// So, convert it to decibels by dividing by 2
						pPrekey=dbGetKey(wObjId, 1);
						sprintf(achKey,pPrekey,i);
						if(i<cCells)
							sprintf(achValue,"%d",ntohs(pRSCP->cellInfo[i].wECIO)/2);
						else
							achValue[0]=0;

						dbWrite(achKey,achValue,NULL);

						// PSC
						pPrekey=dbGetKey(wObjId, 2);
						sprintf(achKey,pPrekey,i);
						if(i<cCells)
							sprintf(achValue,"%d",ntohs(pRSCP->cellInfo[i].wPSC));
						else
							achValue[0]=0;

						dbWrite(achKey,achValue,NULL);
					}

					break;
				}

				case SIERRACNS_PACKET_OBJ_PRI:
				{
					struct
					{
						unsigned short wObjVer;
						unsigned short wMajVer;
						unsigned short wMinVer;
						unsigned short wReservered[2];
						unsigned int dwSKU;
					} __packedStruct *pPRI = pParam;

					__goToErrorIfFalse(ppValueList[0] = __alloc(CNSMGR_MAX_VALUE_LENGTH));
					sprintf(ppValueList[0],"%d.%d",ntohs(pPRI->wMajVer),ntohs(pPRI->wMinVer));
					__goToErrorIfFalse(ppValueList[1] = __alloc(CNSMGR_MAX_VALUE_LENGTH));
					sprintf(ppValueList[1],"%u",ntohl(pPRI->dwSKU));

					break;
				}

				case SIERRACNS_PACKET_OBJ_RETURN_MODEM_MODEL:
				{
					struct
					{
						unsigned char bModemType;
					} __packedStruct *pModemModel = pParam;

					char* szModemModel;

					__goToErrorIfFalse(ppValueList[0] = __alloc(CNSMGR_MAX_VALUE_LENGTH));

					if (__isNotAssigned(szModemModel = (char*)STRCONVERT_FUNCTION_CALL(ModemModel, pModemModel->bModemType)))
						sprintf(ppValueList[0],"[0x%02x]",pModemModel->bModemType);
					else
						strcpy(ppValueList[0],szModemModel);

					break;
				}

				default:
					__goToError();
			}

			break;
		}
	}

	// update database
	cnsmgr_updateDatabase(ppValueList, __countOf(ppValueList), wObjId, iKeyIndexer);

success:
	// free value
	__forEach(iValue, ppValue, ppValueList)
	{
		__free(*ppValue);
	}

	return TRUE;

error:

	// free value
	__forEach(iValue, ppValue, ppValueList)
	{
		__free(*ppValue);
	}

	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cnsmgr_callbackOnCns(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, sierracns_stat cnsStat)
{
	BOOL fError = FALSE;
	unsigned char bNakedOpType = bOpType & ~0x80;

	const char* szCnsStat = STRCONVERT_FUNCTION_CALL(StatMsg, cnsStat);
	const char* szOpType = STRCONVERT_FUNCTION_CALL(OpType, bNakedOpType);

#ifdef SMS_DEBUG
	if (wObjId == SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS ||
		wObjId == SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM			||
		wObjId == SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK		||
		wObjId == SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS	||
		wObjId == SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE				||
		wObjId == SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE					||
		wObjId == SIERRACNS_PACKET_OBJ_DELETE_MO_SMS_MESSAGE )
		syslog(LOG_DEBUG, "##cns## recv id=0x%04x,type=%02x(%s), p=0x%04x, p_cnt=%d, stat=%s", wObjId, bOpType, szOpType, (unsigned)pParam, cbParam, szCnsStat);
#else
	syslog(LOG_DEBUG, "##cns## recv id=0x%04x,type=%02x(%s), p=0x%04x, p_cnt=%d, stat=%s", wObjId, bOpType, szOpType, (unsigned)pParam, cbParam, szCnsStat);
#endif

	if (_globalConfig.fSmsDisabled &&
	   (wObjId == SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS ||
		wObjId == SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM			||
		wObjId == SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK		||
		wObjId == SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS	||
		wObjId == SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE				||
		wObjId == SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE					||
		wObjId == SIERRACNS_PACKET_OBJ_DELETE_MO_SMS_MESSAGE ))
    {
		return;
	}

	switch (bNakedOpType)
	{
		case SIERRACNS_PACKET_OP_NOTIFY_ENABLE_RESP:
			fError = !cnsmgr_callbackOnCnsNotEnResp(pCns, wObjId, bNakedOpType, pParam, cbParam, cnsStat);
			break;

		case SIERRACNS_PACKET_OP_SET_RESPONSE:
			fError = !cnsmgr_callbackOnCnsSetResp(pCns, wObjId, bNakedOpType, pParam, cbParam, cnsStat);
			break;

		case SIERRACNS_PACKET_OP_GET_RESPONSE:
		case SIERRACNS_PACKET_OP_NOTIFICATION:
			if (!cnsmgr_callbackOnCnsGetResp(pCns, wObjId, bNakedOpType, pParam, cbParam, cnsStat))
				fError = !cnsmgr_callbackOnCnsGetRespDatabase(pCns, wObjId, bNakedOpType, pParam, cbParam, cnsStat);
			break;
	}

	if (fError)
		syslog(LOG_ERR, "failed to process - wObj=0x%04x, bOpType=0x%02x", wObjId, bOpType);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	unsigned char bLen;
	char achChv[8];
} __packedStruct CHVCODE;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cnsConvChvToStr(char* lpDst, CHVCODE* pChv)
{
	utils_strSNCpy(lpDst, pChv->achChv, pChv->bLen);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsConvStrToChv(CHVCODE* pChv, const char* lpSrc)
{
	int cbSrc = lpSrc ? strlen(lpSrc) : 0;
	if (cbSrc > sizeof(pChv->achChv))
		return FALSE;

	pChv->bLen = cbSrc;

	if (cbSrc)
		memcpy(pChv->achChv, lpSrc, cbSrc);

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsStartTracking(int nFixType, int nAccuracy,int nTimeOut,int nFixCount,int nFixRate)
{
	struct
	{
		uint16_t version;
		uint16_t location_fix_type;
		uint16_t satellite_acquisition_timeout;
		uint32_t accuracy;
		uint16_t location_fix_count;
		uint32_t location_fix_rate;
	} __packedStruct start_packet;

	start_packet.version=htons( 0x0001 );
	start_packet.location_fix_type = htons( (uint16_t)nFixType );
	start_packet.satellite_acquisition_timeout = htons( (uint16_t)(nTimeOut<255?nTimeOut:255) );
	start_packet.accuracy = htonl( nAccuracy<0?0xFFFFFFF0u:nAccuracy );
	start_packet.location_fix_count = htons( nFixCount<0?1000:nFixCount );
	start_packet.location_fix_rate = htonl( nFixRate );

	return sierracns_write( _pCns, SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, SIERRACNS_PACKET_OP_SET, &start_packet, sizeof( start_packet ) );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsDelSms(BYTE bRefNo)
{
	struct
	{
		BYTE bRefNoToDel;
	} __packedStruct strucDelMOMsg;

	__zeroObj(&strucDelMOMsg);

	strucDelMOMsg.bRefNoToDel = bRefNo;
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_DELETE_MO_SMS_MESSAGE, SIERRACNS_PACKET_OP_SET, &strucDelMOMsg, sizeof(strucDelMOMsg));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsReadUnreadSms(void)
{
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE, SIERRACNS_PACKET_OP_GET, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsReadReadSms(void)
{
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE, SIERRACNS_PACKET_OP_GET, NULL, 0);
}

	///////////////////////
	//// copy to NETWORK //
	///////////////////////
	//_runtimeStat.fRecvSmsMsgToNetwork = FALSE;
	//if (!sierracns_writeSet(_pCns, SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK))
	//{
	//	syslog(LOG_ERR, "failed to copy SMS messages to Network");
	//	__goToError();
	//}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsReadReceivedSmsStatus(void)
{
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS, SIERRACNS_PACKET_OP_GET, NULL, 0);
}

	///////////////////////
	//// copy to NETWORK //
	///////////////////////
	//_runtimeStat.fRecvSmsMsgToNetwork = FALSE;
	//if (!sierracns_writeSet(_pCns, SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK))
	//{
	//	syslog(LOG_ERR, "failed to copy SMS messages to Network");
	//	__goToError();
	//}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsSendSmsFromQueue(void)
{
	char achOut[SIERRACNS_PACKET_PARAMMAXLEN];

	// bypass if empty
	__goToErrorIfFalse(!smssend_isEmpty(_pSmsSend));

	// copy sms message to network if all done
	if(smssend_isDoneMsg(_pSmsSend))
	{
		if (!sierracns_writeSet(_pCns, SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK))
			syslog(LOG_ERR, "failed to copy SMS messages to Network");

		// waste if done packet
		smssend_wasteMsg(_pSmsSend);
	}
	else
	{
		// copy sms configuration detail to send packet structure
		if (_runtimeStat.fRecvSmsCfgDetail)
			smssend_setSmsConfig(_pSmsSend, &_runtimeStat.smsConfigurationDetail);

		// get sms packet
		int cbRead=smssend_getMsgPacket(_pSmsSend,achOut,sizeof(achOut));

		// write sms packet
		if(!sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM, SIERRACNS_PACKET_OP_SET, achOut, cbRead))
			syslog(LOG_ERR, "failed to copy SMS messages to SIM");
	}

	return TRUE;

error:
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsSendSms(char* pDstPhoneNo, char* szMsg, int msSimTimeOut, int msNetworkTimeOut, int encode_type, int cbMsg)
{
	BOOL fTrigger=smssend_isEmpty(_pSmsSend);

	if(!smssend_addMsg(_pSmsSend,pDstPhoneNo, szMsg, encode_type, cbMsg))
	{
		syslog(LOG_ERR, "failed to add a new sms message into queue");
		__goToError();
	}

	if(fTrigger)
		cnsSendSmsFromQueue();

	return TRUE;

error:
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsVeriChvCode(int iChvType, const char* szChv, const char* szUnbChv)
{
	struct
	{
		WORD wChvType;
		CHVCODE chvCode;
		CHVCODE unbChvCode;
	} strucVerichvCode;

	__zeroObj(&strucVerichvCode);

	// get type
	strucVerichvCode.wChvType = htons(iChvType);

	// get chv
	if (!cnsConvStrToChv(&strucVerichvCode.chvCode, szChv))
		return FALSE;

	// get unblocking chv
	if (!cnsConvStrToChv(&strucVerichvCode.unbChvCode, szUnbChv))
		return FALSE;

	#ifdef SIM_DEBUG
	syslog(LOG_ERR, "--> SIERRACNS_PACKET_OBJ_VERIFY_CHV_CODE");
	#endif
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_VERIFY_CHV_CODE, SIERRACNS_PACKET_OP_SET, &strucVerichvCode, sizeof(strucVerichvCode));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sendMEPUnlockCode(const char* szMEP)
{
	struct
	{
		unsigned char bLen;
		char achMEP[12];
	} __packedStruct structMEPUnlockCode;
	int code_len = 0;

	__zeroObj(&structMEPUnlockCode);

	// get length of MEP Unlock Code
	code_len = szMEP ? strlen(szMEP) : 0;
	if ((code_len <= 0) || (code_len > sizeof(structMEPUnlockCode.achMEP)))
		return FALSE;

	structMEPUnlockCode.bLen = htons(code_len);

	// get MEP Unlock Code
	memcpy(structMEPUnlockCode.achMEP, szMEP, code_len);

	#ifdef SIM_DEBUG
	syslog(LOG_ERR, "--> SIERRACNS_PACKET_OBJ_SEND_MEP_UNLOCK_CODE");
	#endif
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_SEND_MEP_UNLOCK_CODE, SIERRACNS_PACKET_OP_SET, &structMEPUnlockCode, sizeof(structMEPUnlockCode));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsChgChvCodes(int iChvType, const char* szOldPin, const char* szNewPin)
{
	struct
	{
		WORD wChvType;
		CHVCODE oldChv;
		CHVCODE newChv;
	} __packedStruct strucChgChvCodes;

	__zeroObj(&strucChgChvCodes);

	// get CHV type
	strucChgChvCodes.wChvType = htons(iChvType);

	// get old pin
	if (!cnsConvStrToChv(&strucChgChvCodes.oldChv, szOldPin))
		return FALSE;

	// get new pin
	if (!cnsConvStrToChv(&strucChgChvCodes.newChv, szNewPin))
		return FALSE;

	#ifdef SIM_DEBUG
	syslog(LOG_ERR, "--> SIERRACNS_PACKET_OBJ_CHANGE_CHV_CODES");
	#endif
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_CHANGE_CHV_CODES, SIERRACNS_PACKET_OP_SET, &strucChgChvCodes, sizeof(strucChgChvCodes));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsGetEnChv1(void)
{
	#ifdef SIM_DEBUG
	syslog(LOG_ERR, "--> SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION");
	#endif
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION, SIERRACNS_PACKET_OP_GET, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsEnChv1(BOOL fEnable, const char *szPin)
{
	struct
	{
		BYTE bReqChv1EnStatus;
		CHVCODE chvCode;
	} __packedStruct strucEnChv1Veri;

	__zeroObj(&strucEnChv1Veri);

	// get version
	strucEnChv1Veri.bReqChv1EnStatus = fEnable ? 0x01 : 0x00;
	// get chv
	if (!cnsConvStrToChv(&strucEnChv1Veri.chvCode, szPin))
		return FALSE;

	#ifdef SIM_DEBUG
	syslog(LOG_ERR, "--> SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION");
	#endif
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION, SIERRACNS_PACKET_OP_SET, &strucEnChv1Veri, sizeof(strucEnChv1Veri));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsSetCurBand(long long l64Band)
{
	struct
	{
		unsigned short wVersion;

		UINT64 l64BandGroup;
	} __packedStruct strucSetCurrentBand;

	__zeroObj(&strucSetCurrentBand);

	strucSetCurrentBand.wVersion = htons(0x0001);
	strucSetCurrentBand.l64BandGroup = __htonl64(l64Band);

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND, SIERRACNS_PACKET_OP_SET, &strucSetCurrentBand, sizeof(strucSetCurrentBand));
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsSetOperator(unsigned char bSelMode,unsigned short wMCC,unsigned short wMNC)
{
	struct {
		unsigned char bSelMode;
		unsigned short wMCC;
		unsigned short wMNC;
	} __packedStruct selPLMN;

	selPLMN.bSelMode=bSelMode;
	selPLMN.wMCC=htons(wMCC);
	selPLMN.wMNC=htons(wMNC);

	syslog(LOG_INFO,"##cmd## send SIERRACNS_PACKET_OBJ_SELECT_PLMN set");
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_SELECT_PLMN, SIERRACNS_PACKET_OP_SET, &selPLMN, sizeof(selPLMN));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsGetOperator(void)
{
	syslog(LOG_INFO,"##cmd## send SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST get");

	return sierracns_writeGet(_pCns, SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsScanOperators(void)
{
	syslog(LOG_INFO,"##cmd## send SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST set");
	
	return sierracns_writeSet(_pCns, SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsGetCurBand(void)
{
	return sierracns_writeGet(_pCns, SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsVcAccept(unsigned char bCallAction, unsigned short wCallId)
{
	struct
	{
		unsigned short wVerOfPck;
		unsigned short wCallId;

		unsigned char bCallAction;

		unsigned char __reserved[15];

	} __packedStruct callCtrlForUmts;

	// init.
	__zeroObj(&callCtrlForUmts);

	callCtrlForUmts.wVerOfPck = htons(0x0001);
	callCtrlForUmts.wCallId = htons(wCallId);
	callCtrlForUmts.bCallAction = bCallAction;

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_CALL_CONTROL_FOR_UMTS, SIERRACNS_PACKET_OP_SET, &callCtrlForUmts, sizeof(callCtrlForUmts));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsVcSelAdoProfile(int iProfile, BOOL fEarpiece, BOOL fMicrophone, int nVolumeLevel, int fGenerator)
{
	struct
	{
		unsigned short wObjVer;
		unsigned short wCurProfile;
		unsigned char bEarpiece;
		unsigned char bMicrophone;
		unsigned char bAudioGenerator;
		unsigned char bAudioVolume;
	} __packedStruct selAdoProfile;

	__zeroObj(&selAdoProfile);

	selAdoProfile.wObjVer = 2;
	selAdoProfile.wCurProfile = iProfile;
	selAdoProfile.bEarpiece = fEarpiece ? 0x01 : 0x00;
	selAdoProfile.bMicrophone = fMicrophone ? 0x01 : 0x00;
	selAdoProfile.bAudioVolume = nVolumeLevel;
	selAdoProfile.bAudioGenerator = fGenerator ? 0x01 : 0x00;

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, SIERRACNS_PACKET_OP_SET, &selAdoProfile, sizeof(selAdoProfile));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int cnsVcDialConv(const char* pDial,unsigned char* pDst,int cbDst)
{
	unsigned char* p=pDst;

	int ch;
	while( (ch = *pDial++) !=0 && cbDst--)
	{
		char chDial;

		switch(ch)
		{
			case '*':	chDial='A'; break;
			case '#':	chDial='B'; break;
			case ',':	chDial='C'; break;
			case '?':	chDial='D'; break;
			default:	chDial=ch;
		}

		*p++=chDial;
	}

	return p-pDst;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsVcDTMF(const char* pDial, int nOnDuration, int nOffDuration, BOOL fSendTonesToEarpiece)
{
	struct
	{
		unsigned char bNumberOfDigits;
		unsigned char achOverDialDigits[20];
		unsigned short wToneOnDuraton;
		unsigned short wToneOffDuration;
		unsigned char bSendTonesToEarpiece;
	} __packedStruct sendDtmfOverdialDigits;

	int nPhoneNumberLen;

	__zeroObj(&sendDtmfOverdialDigits);

	// error if too big
	if (strlen(pDial) >= __countOf(sendDtmfOverdialDigits.achOverDialDigits))
		return FALSE;

	// encode phone number
	if ((nPhoneNumberLen = cnsVcDialConv(pDial, sendDtmfOverdialDigits.achOverDialDigits, sizeof(sendDtmfOverdialDigits.achOverDialDigits))) <= 0)
		return FALSE;

	sendDtmfOverdialDigits.bNumberOfDigits = (unsigned char)nPhoneNumberLen;
	sendDtmfOverdialDigits.wToneOnDuraton = htons((unsigned short)nOnDuration);
	sendDtmfOverdialDigits.wToneOffDuration = htons((unsigned short)nOffDuration);
	sendDtmfOverdialDigits.bSendTonesToEarpiece = fSendTonesToEarpiece ? 0x01 : 0x00;

	// write
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, SIERRACNS_PACKET_OP_SET, &sendDtmfOverdialDigits, sizeof(sendDtmfOverdialDigits));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsVcDial(const char* pDial)
{
	struct
	{
		unsigned char bCallLineId;
		unsigned char bInternationalId;
		unsigned char bPhoneNumberLen;
		unsigned char bNumberBeingCalled[20];
	} __packedStruct initVoiceCall;

	BOOL fInternational;
	int nPhoneNumberLen;

	__zeroObj(&initVoiceCall);


	// encode phone number
	if ((nPhoneNumberLen = cnsConvPhoneNumber(pDial, initVoiceCall.bNumberBeingCalled, sizeof(initVoiceCall.bNumberBeingCalled), &fInternational)) <= 0)
		return FALSE;

	// set phone number length
	initVoiceCall.bPhoneNumberLen = (unsigned char)nPhoneNumberLen;
	// set international flag
	initVoiceCall.bInternationalId = fInternational ? 0x01 : 0x00;

	// write
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_INITIATE_VOICE_CALL, SIERRACNS_PACKET_OP_SET, &initVoiceCall, sizeof(initVoiceCall));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void test_cnsmgr_dial(void)
{
	if (!cnsVcDial("+61-413-237-592"))
		syslog(LOG_INFO, "cnsVcDial failed");
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbGetLength(int nObjId, int iIdx, int* pValueLen)
{
	const char* szKey;
	const char* szPrefix;

	// get database key
	if (__isNotAssigned(szKey = dbGetKey(nObjId, iIdx)))
		return FALSE;

	szPrefix = dbGetKeyPrefix(nObjId, iIdx);

	// get value
	if (!databasedriver_cmdGetLength(_pDb, szKey, pValueLen,szPrefix))
		return FALSE;

	syslog(LOG_INFO, "length %s=%d", szKey, *pValueLen);

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbGetStr(int nObjId, int iIdx, char* pValue, int* pValueLen)
{
	const char* szKey;
	const char* szPrefix;

	// get database key
	if (__isNotAssigned(szKey = dbGetKey(nObjId, iIdx)))
		return FALSE;

	szPrefix = dbGetKeyPrefix(nObjId, iIdx);

	// get value
	if (!databasedriver_cmdGetSingle(_pDb, szKey, pValue, pValueLen,szPrefix))
		return FALSE;

	//syslog(LOG_INFO, "##db## read %s=%s", szKey, pValue);

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbGetBool(int nObjId, int iIdx, BOOL* pValue)
{
	char achValue[256];
	int cbValue = sizeof(achValue);

	// get value
	if (!dbGetStr(nObjId, iIdx, achValue, &cbValue))
		return FALSE;

	if (!strcasecmp(achValue, "true"))
		*pValue = TRUE;
	else
		*pValue = FALSE;

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbGetInt(int nObjId, int iIdx, int* pValue)
{
	char achValue[256];
	int cbValue = sizeof(achValue);

	// get value
	if (!dbGetStr(nObjId, iIdx, achValue, &cbValue))
		return FALSE;

	if (!strlen(achValue))
		return FALSE;

	*pValue = atoi(achValue);

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbSetStr(int nObjId, int iIdx, char* pValue)
{
	const char* szKey;
	const char* szPrefix;

	// get database key
	if (__isNotAssigned(szKey = dbGetKey(nObjId, iIdx)))
		return FALSE;

	szPrefix = dbGetKeyPrefix(nObjId, iIdx);

	// get value
	if (!dbWrite(szKey, pValue,szPrefix))
		return FALSE;

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbSetInt(int nObjId, int iIdx, int nVal)
{
	char achBuf[CNSMGR_MAX_VALUE_LENGTH];

	sprintf(achBuf,"%d",nVal);
	return dbSetStr(nObjId,iIdx,achBuf);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbSetStat(int nObjId, int iIdx, database_reserved_msg resMsg, char* lpszDetail)
{
	char achOut[CNSMGR_MAX_VALUE_LENGTH];

	// get error message
	if (lpszDetail && strlen(lpszDetail))
		sprintf(achOut, "%s %s", STRCONVERT_FUNCTION_CALL(DatabaseMsg, resMsg), lpszDetail);
	else
		strcpy(achOut, STRCONVERT_FUNCTION_CALL(DatabaseMsg, resMsg));

	// set status - error
	if (!dbSetStr(nObjId, iIdx, achOut))
	{
		syslog(LOG_ERR, "dbSetStat() failed");
		return FALSE;
	}

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int dbGetCmdErrMsg(char* pCmdList[], char* pBuf)
{
	sprintf(pBuf, "use only - ");

	char* pDst;
	pDst = pBuf + strlen(pBuf);

	int i = 0;

	while (*pCmdList)
	{
		if (i++)
			strcat(pDst, " ");

		strcat(pDst, *pCmdList++);
	}

	return i;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int dbGetCmdIdx(char* pCmdList[], char* szCmd)
{
	int i = 0;

	while (*pCmdList)
	{
		if (!strcasecmp(*pCmdList, szCmd))
			return i;

		i++;
		pCmdList++;
	}

	return -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbFifoCmdSms(char* pValue, int cbValue, char** ppNewValue)
{
	*ppNewValue = NULL;
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsGetReturnIpAddress(int iProfileId)
{
	struct
	{
		BYTE bProfileId;
	} __packedStruct strucReturnIpAddress;

	strucReturnIpAddress.bProfileId=(BYTE)iProfileId+1;

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_RETURN_IP_ADDERSS, SIERRACNS_PACKET_OP_SET, &strucReturnIpAddress, sizeof(strucReturnIpAddress));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsSetActiveProfile(int iProfileId,int fActive)
{
	struct
	{
		BYTE bProfileId;
		BYTE bReqAction;
	} __packedStruct strucActiveSession;

	strucActiveSession.bProfileId=(BYTE)iProfileId+1;
	strucActiveSession.bReqAction=fActive?1:0;

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION, SIERRACNS_PACKET_OP_SET, &strucActiveSession, sizeof(strucActiveSession));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsReadProfile(int iProfileId,int nInfoType,int nTftFilterId)
{
	struct
	{
		BYTE bProfileId;
		BYTE bProfileInfoType;
		BYTE bTftFilterId;
	} __packedStruct strucReadProfile;

	strucReadProfile.bProfileId=(BYTE)iProfileId+1;
	strucReadProfile.bProfileInfoType=(BYTE)nInfoType;
	strucReadProfile.bTftFilterId=(BYTE)nTftFilterId;

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_READ_PROFILE, SIERRACNS_PACKET_OP_SET, &strucReadProfile, sizeof(strucReadProfile));
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsWriteProfileManage(int iProfileId,const char* userName,const char* pass)
{
	struct cnsprofile_manage_tlv_t* tlv;
	struct cnsprofile_manage_t* manage;
	char* val;

	int userLen;
	int passLen;

	int tlvCnt;
	int len;

	char buf[sizeof(struct cnsprofile_manage_t)+sizeof(struct cnsprofile_manage_tlv_t)*4+ 1+128+ 1+128];

	userLen=strlen(userName);
	passLen=strlen(pass);

	syslog(LOG_INFO,"using exteneded user name and password cns message");

	tlvCnt=0;

	if(userLen>=127 || passLen>=127) {
		syslog(LOG_ERR,"user name or password too long - userLen=%d,passLen=%d",userLen,passLen);
		return 0;
	}


	// check user name
	if(userLen>32)
		tlvCnt++;

	// check password
	if(passLen>32)
		tlvCnt++;

	// check length
	if(!tlvCnt) {
		syslog(LOG_INFO,"user name and password shorter than 32 - user=%d,pass=%d",userLen,passLen);
		return 1;
	}

	syslog(LOG_INFO,"tlv count = %d, userLen=%d,passLen=%d",tlvCnt,userLen,passLen);

	// clear buf
	syslog(LOG_INFO,"maximum request length = %d",sizeof(buf));
	memset(buf,0,sizeof(buf));

	// set header
	manage=(struct cnsprofile_manage_t*)buf;
	syslog(LOG_INFO,"building profile manage head - %p, profile=%d",manage,iProfileId);
	manage->wVer=htons(1);
	manage->bProfileID=(BYTE)(iProfileId+1);
	manage->bNumberOfTLV=(BYTE)tlvCnt;

	tlv=manage->tlv;

	// set tlv - user name
	if(userLen>32) {
/*
		syslog(LOG_INFO,"building user name length TLV - %p",tlv);

		 // max user name length
		tlv->bObjType=0x02;
		tlv->bObjLen=1;
		val=(char*)tlv->bObjVal;
		*val=userLen+1;
		tlv=(struct cnsprofile_manage_tlv_t*)( (char*)tlv+sizeof(*tlv)+1 );
*/
		syslog(LOG_INFO,"building user name TLV - %p",tlv);

		// user name
		tlv->bObjType=0x00;
		tlv->bObjLen=(BYTE)(userLen+1);
		val=(char*)tlv->bObjVal;
		strcpy(val,userName);
		tlv=(struct cnsprofile_manage_tlv_t*)( (char*)tlv+sizeof(*tlv)+userLen+1 );
	};

	// set tlv - password
	if(passLen>32) {
/*
		syslog(LOG_INFO,"building password length TLV - %p",tlv);

		 // max pass length
		tlv->bObjType=0x03;
		tlv->bObjLen=1;
		val=(char*)tlv->bObjVal;
		*val=passLen+1;
		tlv=(struct cnsprofile_manage_tlv_t*)( (char*)tlv+sizeof(*tlv)+1 );
*/
		syslog(LOG_INFO,"building password TLV - %p",tlv);

		// password
		tlv->bObjType=0x01;
		tlv->bObjLen=(BYTE)(passLen+1);
		val=(char*)tlv->bObjVal;
		strcpy(val,pass);
		tlv=(struct cnsprofile_manage_tlv_t*)( (char*)tlv+sizeof(*tlv)+passLen+1 );
	};

	len=(int)((size_t)tlv-(size_t)manage);

	syslog(LOG_INFO,"current tlv = %p,len=%d",tlv,len);

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_MANAGE_PROFILE, SIERRACNS_PACKET_OP_SET, manage, len);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsWriteProfile(int iProfileId,int iPdpType,int fHdrComp,int fDataComp,const char* szPdpAddr,const char* szApn,int iAuthType,const char* szUser,const char* szPw,const char* szLabel)
{
	struct cnsprofile_t strucProfile;

	memset(&strucProfile,0,sizeof(strucProfile));

	strucProfile.bProfileId=iProfileId+1;
	strucProfile.bProfileInfoType=0;	// basic
	strucProfile.bProfileValid=1;	// valid
	strucProfile.bPdpType=(BYTE)iPdpType;
	strucProfile.bHdrCompression=(BYTE)fHdrComp;
	strucProfile.bDataCompression=(BYTE)fDataComp;

	__str2pasStr(szPdpAddr,&strucProfile.bPdpAddrLen,sizeof(strucProfile.pdpAddr));
	__str2pasStr(szApn,&strucProfile.bApnLen,sizeof(strucProfile.apn));

	strucProfile.bPdpInitType=0;	// primary mobile initiated
	strucProfile.bPrimProfileId=0;
	strucProfile.bAuthType=(BYTE)iAuthType;
	utils_strDNCpy((char*)strucProfile.szUserName,sizeof(strucProfile.szUserName),szUser);
	utils_strDNCpy((char*)strucProfile.szPassword,sizeof(strucProfile.szPassword),szPw);
	utils_strDNCpy((char*)strucProfile.szLabel,sizeof(strucProfile.szLabel),szLabel);

	strucProfile.bAutoContextActivationMode=0;	// connect on request
	strucProfile.bProfileWriteProtect=0;	// no write-protection
	strucProfile.bPromptPassword=0;	// use the password field - not prompting
	strucProfile.bAutoLaunchApp=0;
	strucProfile.bPdpLingerTimer=0;
	strucProfile.bSoftwareOpt=0;

	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_WRITE_PROFILE, SIERRACNS_PACKET_OP_SET, &strucProfile, sizeof(strucProfile));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbCmdProfile(char* pValue, int cbValue, char** ppNewValue)
{
	char achErr[CNSMGR_MAX_VALUE_LENGTH] = "";
	int nObjId=CNSMGR_DB_PROFILE;

	//// set status - busy
	//if (!dbSetStat(nObjId, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_busy, NULL))
	//	syslog(LOG_ERR, "dbSetStat(busy) failed");

	syslog(LOG_INFO, "##profile## got profile cmd - %s",pValue);

	char* pszCmds[] = {"read", "write", "activate", "deactivate", "getip", NULL};
	int iCmd = dbGetCmdIdx(pszCmds, pValue);
	switch (iCmd)
	{
		case 4:
		{
			// get profile id
			int nProfileId;
			if (!dbGetInt(nObjId, 2, &nProfileId))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(nObjId, 2));
				__goToError();
			}

			// set active profile
			__goToErrorIfFalse(cnsGetReturnIpAddress(nProfileId));
			break;
		}

		case 2:
		case 3:
		{
			int fActive=iCmd==2;
			int stat_chg;

			stat_chg=(!fActive && active_pdp_session) || (fActive && !active_pdp_session);

			syslog(LOG_INFO, "##profile## got activate/deactivate");

			// get profile id
			int nProfileId;
			if (!dbGetInt(nObjId, 2, &nProfileId))
			{
				syslog(LOG_ERR, "failed to read wwan.0.profile.cmd.param.profile_id");

				sprintf(achErr, "cannot read [%s]", dbGetKey(nObjId, 2));
				__goToError();
			}

			// deactive first if any previous connection
			if(fActive && stat_chg) {
				syslog(LOG_INFO, "##profile## deactivate previous connection");
				cnsSetActiveProfile(nProfileId,0);
			}

			// set active profile
			if(fActive)
				syslog(LOG_INFO, "##profile## activate connection");
			else
				syslog(LOG_INFO, "##profile## deactivate connection");

			if(stat_chg) {
				__goToErrorIfFalse(cnsSetActiveProfile(nProfileId,fActive));
			}
			else {
				dbWriteInt("session.0.status", fActive?1:0,NULL);
			}

			break;
		}

		case 0: // read
		{
			syslog(LOG_INFO, "##profile## got read");

			// get profile id
			int nProfileId;
			if (!dbGetInt(nObjId, 2, &nProfileId))
			{
				syslog(LOG_ERR, "failed to read wwan.0.profile.cmd.param.profile_id");

				sprintf(achErr, "cannot read [%s]", dbGetKey(nObjId, 2));
				__goToError();
			}

			syslog(LOG_INFO, "##profile## send cns profile read");

			// read basic profile
			__goToErrorIfFalse(cnsReadProfile(nProfileId,0x00,0x00));
			break;
		}

		case 1: // write
		{
			// get profile id
			int iProfileId;
			if (!dbGetInt(nObjId, 2, &iProfileId))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(nObjId, 2));
				__goToError();
			}

			// get header compression
			int fHdrComp;
			if (!dbGetInt(nObjId, 3, &fHdrComp))
				fHdrComp=0;

			// get data compression
			int fDataComp;
			if (!dbGetInt(nObjId, 4, &fDataComp))
				fDataComp=0;

			// get pdp address
			char szPdpAddr[CNSMGR_MAX_VALUE_LENGTH];
			int cbPdpAddr=sizeof(szPdpAddr);
			if (!dbGetStr(nObjId, 5, szPdpAddr, &cbPdpAddr))
				szPdpAddr[0]=0;

			// get apn
			char szApn[CNSMGR_MAX_VALUE_LENGTH];
			int cbApn=sizeof(szApn);
			if (!dbGetStr(nObjId, 6, szApn, &cbApn))
				szApn[0]=0;

			// get authentication type
			char szAuthType[CNSMGR_MAX_VALUE_LENGTH];
			int cbAuthType=sizeof(szAuthType);
			if(!dbGetStr(nObjId, 7, szAuthType, &cbAuthType))
				szAuthType[0]=0;

			// convert to numeric
			int iAuthType;
			if (!strcasecmp(szAuthType,"pap"))
				iAuthType=1;
			else if (!strcasecmp(szAuthType,"chap"))
				iAuthType=2;
			else
				iAuthType=0;

			// get user
			char szUser[CNSMGR_MAX_VALUE_LENGTH];
			int cbUser=sizeof(szUser);
			if (!dbGetStr(nObjId, 8, szUser,&cbUser))
				szUser[0]=0;

			// get password
			char szPw[CNSMGR_MAX_VALUE_LENGTH];
			int cbPw=sizeof(szPw);
			if (!dbGetStr(nObjId, 9, szPw,&cbPw))
				szPw[0]=0;

			// get label
			char szLabel[CNSMGR_MAX_VALUE_LENGTH];
			int cbLabel=sizeof(szLabel);
			if (!dbGetStr(nObjId, 10, szLabel,&cbLabel))
				szLabel[0]=0;

			// get pdp type
			char szPdpType[CNSMGR_MAX_VALUE_LENGTH];
			int cbPdpType=sizeof(szPdpType);
			if (!dbGetStr(nObjId, 11, szPdpType,&cbPdpType))
				szPdpType[0]=0;

			int iPdpType;
			if(!strcasecmp(szPdpType,"ipv6"))
				iPdpType=2;
			else if (!strcasecmp(szPdpType,"ppp"))
				iPdpType=1;
			else
				iPdpType=0;

			__goToErrorIfFalse(cnsWriteProfile(iProfileId,iPdpType,fHdrComp,fDataComp,szPdpAddr,szApn,iAuthType,szUser,szPw,szLabel));

			if(!cnsWriteProfileManage(iProfileId,szUser,szPw)) {
				syslog(LOG_ERR,"failed to use extended user name and password");
				goto error;
			}
			break;
		}

		default:
		{
			dbGetCmdErrMsg(pszCmds, achErr);
			__goToError();
		}

	}

	// set status - done
	if (!dbSetStat(nObjId, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
		syslog(LOG_ERR, "dbSetStat(done) failed");

//success:
	*ppNewValue = NULL;
	return TRUE;

error:
	if (!dbSetStat(nObjId, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
		syslog(LOG_ERR, "failed to write status key");

	*ppNewValue = NULL;
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL checkSimStatus(BOOL log)
{
	char achSimStatus[CNSMGR_MAX_VALUE_LENGTH];
	int cbValue = CNSMGR_MAX_VALUE_LENGTH;

	if(!databasedriver_cmdGetSingle(_pDb,"sim.status.status",achSimStatus,&cbValue,NULL))
	{
	    if (log)
    		syslog(LOG_ERR, "failed to get sim.status.status");
		return FALSE;
	}
	if(strcmp(achSimStatus,"SIM OK"))
	{
	    if (log)
		    syslog(LOG_ERR, "SIM is not ready");
		return FALSE;
	}
    if (log)
    	syslog(LOG_INFO, "SIM is ready");
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL checkSimInserted(BOOL log)
{
	char achSimStatus[CNSMGR_MAX_VALUE_LENGTH];
	int cbValue = CNSMGR_MAX_VALUE_LENGTH;

	if(!databasedriver_cmdGetSingle(_pDb,"sim.status.status",achSimStatus,&cbValue,NULL))
	{
	    if (log)
    		syslog(LOG_ERR, "failed to get sim.status.status");
		return TRUE;
	}
	if(strcmp(achSimStatus,"SIM not inserted") == 0 || strcmp(achSimStatus,"SIM removed") == 0)
	{
	    if (log)
		    syslog(LOG_ERR, "SIM is not inserted");
		return FALSE;
	}
    if (log)
    	syslog(LOG_INFO, "SIM is inserted");
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// initialize band selection mode after factory reset
#define RDB_BAND_SEL_MODE_INIT		"band_selection_mode_initialized"
void cnsmgr_initBandSelMode(void)
{
	static int band_sel_initialized = 0;
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
#else
	char rd_data[10] = {0, };
#endif

	if (!checkSimStatus(FALSE) && !checkSimInserted(FALSE))
	    return;

	if (band_sel_initialized)
	    return;

    /* When the SIM status changes to 'READY' there is race condition with simple_at_manager
       to initialize band selection mode. But MC8801 seems to be not supporting band selection
       command via cns as it always fails to initialze band selection mode here.
       Therefore let simple_at_manager initialize band selection mode. */
	if (!is_enabled_feature(FEATUREHASH_CMD_ALLAT))
        return;

#if defined(PLATFORM_PLATYPUS)
	nvram_init(RT2860_NVRAM);
	rd_data = nvram_bufget(RT2860_NVRAM, RDB_BAND_SEL_MODE_INIT);
	if (!rd_data)
		goto error;
#else
	if (rdb_get_single(RDB_BAND_SEL_MODE_INIT, rd_data, 10) != 0)
		goto error;
#endif

	if (strlen(rd_data) == 0)
		goto error;
	if (strcmp(rd_data, "0") == 0) {
		UINT64 ul64Band=(UINT64)0xffffffffffffffffULL;	/* auto band */
		int nSelMode=0;		/* 0 for auto band */
		int nMCC=0;			/* 0 for auto band */
		int nMNC=0;			/* 0 for auto band */
		__goToErrorIfFalse(cnsSetCurBand(ul64Band));
		__goToErrorIfFalse(cnsSetOperator((unsigned char)nSelMode,(unsigned short)nMCC,(unsigned short)nMNC));

		// set current band to auto
		//if (!dbSetStr(CNSMGR_DB_KEY_CURRENTBAND, 0, "set"))
		//	syslog(LOG_ERR, "failed to write %s",dbGetKey(CNSMGR_DB_KEY_CURRENTBAND, 0));
		//if (!dbSetStr(CNSMGR_DB_KEY_CURRENTBAND, 2, "Autoband"))
		//	syslog(LOG_ERR, "failed to write %s",dbGetKey(CNSMGR_DB_KEY_CURRENTBAND, 2));

		// set PLMN mode to auto
		//if (!dbSetStr(CNSMGR_DB_PLMN, 0, "5"))
		//	syslog(LOG_ERR, "failed to write %s",dbGetKey(CNSMGR_DB_PLMN, 0));
		//if (!dbSetStr(CNSMGR_DB_PLMN, 3, "0,0,0"))
		//	syslog(LOG_ERR, "failed to write %s",dbGetKey(CNSMGR_DB_PLMN, 3));

#if defined(PLATFORM_PLATYPUS)
		nvram_set(RT2860_NVRAM, RDB_BAND_SEL_MODE_INIT, "1");
#else
		rdb_set_single(RDB_BAND_SEL_MODE_INIT, "1");
#endif
    	band_sel_initialized = 1;
		syslog(LOG_ERR, "================================================");
		syslog(LOG_ERR, "Band and PLMN are initialized to Automatic mode!");
		syslog(LOG_ERR, "================================================");
	} else if (strcmp(rd_data, "1") == 0) {
        band_sel_initialized = 1;
        goto success;
	}
	sleep(1);
	__goToErrorIfFalse(cnsGetCurBand());
	sleep(1);
	__goToErrorIfFalse(cnsGetOperator());
success:
error:
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
	return;
}

///////////////////////////////////////////////////////////////////////////////
/* Once changed operation mode M2M module keeps it in NV memory over router reboot or
 * factory reset, config restore. This function is implemented in order to restore to 
 * saved value in configuration file.
 * After launching cnsmgr, try to syncronize operation mode only once.
 * Before selecting PLMN list, reading PLMN list should be proceeded.
 */ 
void sync_operation_mode(void)
{
	int saved_service_type = 0, saved_selmode = 1;
	char sysmode[64], svctype[64], plmnsel[64];
	int cLen, nMCC=0, nMNC=0;
	static int interval = 0;
	static int init = 1;

	if (init) {
		if (interval++ < 100) {
			return;
		} else {
			init =0;
			interval = 0;
		}
	} else {
		if (interval++ < 20) {
			return;
		} else {
			interval = 0;
		}
	}
	
	syslog(LOG_DEBUG,"[PLMN SYNC] -----------------------------------------------");
	if (op_mode_sync_state == SYNCHRONIZED) {
		syslog(LOG_DEBUG,"[PLMN SYNC] already synchronized, return");
		return;
	} else if (op_mode_sync_state == WAITING_RESP) {
		syslog(LOG_DEBUG,"[PLMN SYNC] waiting for response, return");
		return;
	}
	
	if (!checkSimStatus(FALSE) || !checkSimInserted(FALSE)) {
		syslog(LOG_DEBUG,"[PLMN SYNC] SIM is not ready, return");
	    return;
	}
	
	cLen=sizeof(sysmode);
	if(!databasedriver_cmdGetSingle(_pDb,"system_network_status.system_mode",sysmode,&cLen,NULL))
	{
		syslog(LOG_DEBUG,"[PLMN SYNC] failed to get system mode, return");
		return;
	}
	if(strcmp(sysmode,"Invalid service") == 0)
	{
		syslog(LOG_DEBUG,"[PLMN SYNC] invalid system mode, return");
		return;
	}
	cLen=sizeof(svctype);
	if(!databasedriver_cmdGetSingle(_pDb,"system_network_status.service_type",svctype,&cLen,NULL))
	{
		syslog(LOG_DEBUG,"[PLMN SYNC] failed to get service type, return");
		return;
	}
	if(strcmp(svctype,"None") == 0)
	{
		syslog(LOG_DEBUG,"[PLMN SYNC] invalid service state, return");
		return;
	}
	
    /* When the SIM status changes to 'READY' there is race condition with simple_at_manager
       to initialize band selection mode. But MC8801 seems to be not supporting band selection
       command via cns as it always fails to initialze band selection mode here.
       Therefore let simple_at_manager initialize band selection mode. */
	if (!is_enabled_feature(FEATUREHASH_CMD_ALLAT)) {
		syslog(LOG_DEBUG,"[PLMN SYNC] Other AT manager is running, return");
        return;
    }

	cLen=sizeof(plmnsel);
	if (!dbGetStr(CNSMGR_DB_PLMN, 3, plmnsel, &cLen)) {
		syslog(LOG_DEBUG,"[PLMN SYNC] cannot read [%s]", dbGetKey(CNSMGR_DB_PLMN, 3));
	    return;
	}

	if (strcmp(plmnsel, "0") == 0) {
		saved_selmode = 0;	// automatic mode
	} else {
		sscanf(plmnsel,"%d,%d,%d",&saved_service_type,&nMCC,&nMNC);
		if (nMCC <= 0 || nMNC <= 0) {
			syslog(LOG_DEBUG,"[PLMN SYNC] invalid [%s] value", dbGetKey(CNSMGR_DB_PLMN, 3));
		    return;
		}
	}
	
	syslog(LOG_INFO,"[PLMN SYNC] synchronise operation mode to saved PLMN_selMode '%s', saved svctype %d",
		(saved_selmode? "Manual":"Automatic"), saved_service_type);
	if (op_mode_sync_state == NONE) {
		__goToErrorIfFalse(sierracns_writeGet(_pCns, SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS));
		__goToErrorIfFalse(cnsScanOperators());
	} else {
		syslog(LOG_INFO,"[PLMN SYNC] select network (mode=%d,mcc=%d,mnc=%d)",saved_service_type,nMCC,nMNC);
		__goToErrorIfFalse(cnsSetOperator((unsigned char)saved_service_type,(unsigned short)nMCC,(unsigned short)nMNC));
	}		
	op_mode_sync_state = WAITING_RESP;
error:
	return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* check if msg body contains extended chars to be sent with UCS2 mode */
#define RDB_VAR_SMS_SP  "wwan.0.sms.has_special_chars"
int check_extended_char(char *pMsg, int cbMsg)
{
    char tempstr[10];
    if (rdb_get_single(RDB_VAR_SMS_SP, tempstr, 10) != 0) {
        syslog(LOG_ERR, "failed to read %s", RDB_VAR_SMS_SP);
        return -1;
    }
    if (strcmp(tempstr, "1") == 0)
        return 1;
    return 0;
}
///////////////////////////////////////////////////////////////////////////////
#define RDB_SMS_ENCODING_SCHEME		"smstools.conf.coding_scheme"
int smstools_encoding_scheme = CNSMGR_ENCODE_GSM_7;
void read_smstools_encoding_scheme(void)
{
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
	nvram_close(RT2860_NVRAM);
	nvram_init(RT2860_NVRAM);
	rd_data = nvram_bufget(RT2860_NVRAM, RDB_SMS_ENCODING_SCHEME);
	if (!rd_data)
		goto ret;
#else
	char rd_data[10] = {0, };
	if (rdb_get_single(RDB_SMS_ENCODING_SCHEME, rd_data, 10) != 0)
		goto ret;
#endif
	if (strlen(rd_data) == 0)
		goto ret;
	if (strcmp(rd_data, "UCS2") == 0)
		smstools_encoding_scheme = CNSMGR_ENCODE_UCS_2;
	else
		smstools_encoding_scheme = CNSMGR_ENCODE_GSM_7;
ret:
	syslog(LOG_ERR, "SMS encoding scheme %s, rd_data '%s'",
			(smstools_encoding_scheme == CNSMGR_ENCODE_GSM_7)? "GSM7":"UCS2", rd_data);
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL cnsSetSmscAddress(char *NewAddr)
{
	smsconfigurationdetail newSmsConfigurationDetail;
	BOOL fInternational = FALSE, tmp;
	unsigned char smsc_addr[32], tmpaddr[32];
	int cbLen;
	syslog(LOG_ERR, "cnsSetSmscAddress()");
	(void) memcpy((char *)&newSmsConfigurationDetail, (char *)&_runtimeStat.smsConfigurationDetail, sizeof(smsconfigurationdetail));
	(void) memset(smsc_addr, 0x00, 32);
	(void) memset(tmpaddr, 0x00, 32);
	if (NewAddr[0] == '+' || (NewAddr[0] == '0' && NewAddr[1] == '0') || strlen(NewAddr) > 10) {
		fInternational = TRUE;
		if (NewAddr[0] == '+') {
			strcpy((char *)&tmpaddr[0], (char *)(NewAddr+1));
		} else {
			strcpy((char *)&tmpaddr[0], NewAddr);
		}
	} else {
		strcpy((char *)&tmpaddr[0], NewAddr);
	}
	cbLen = cnsConvPhoneNumber((const char *)&tmpaddr[0], (unsigned char *)&smsc_addr[0], strlen((char*)tmpaddr), &tmp);

	newSmsConfigurationDetail.bServCenAddrPresent = 0x01;
	newSmsConfigurationDetail.servCenAddrInfo.bLenOfAddrPhoneNo = cbLen;
	newSmsConfigurationDetail.servCenAddrInfo.bAddrType = (fInternational? 0x01:0x02);
	newSmsConfigurationDetail.servCenAddrInfo.bAddrNoPlan = 0x01;
	(void) memset(newSmsConfigurationDetail.servCenAddrInfo.achAddrPhoneNo, 0x00, sizeof(newSmsConfigurationDetail.servCenAddrInfo.achAddrPhoneNo));
	(void) memcpy(newSmsConfigurationDetail.servCenAddrInfo.achAddrPhoneNo, smsc_addr, cbLen);
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, SIERRACNS_PACKET_OP_SET, &newSmsConfigurationDetail, sizeof(newSmsConfigurationDetail));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsgetSmscAddress(void)
{
	return sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, SIERRACNS_PACKET_OP_GET, NULL, 0);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define RDB_VAR_SMS_ROUTING_OPTION  "smstools.conf.mo_service"
BOOL dbCmdSms(char* pValue, int cbValue, char** ppNewValue)
{
	char achErr[CNSMGR_MAX_VALUE_LENGTH] = "";
	char achPhoneNo[CNSMGR_MAX_VALUE_LENGTH];
	int cbhPhoneNo = sizeof(achPhoneNo);
	int iCmd;
	char tempstr[BSIZE_256];
	int routing_option;
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
#endif

	iCmd=-1;

	if (_globalConfig.fSmsDisabled)
    {
		syslog(LOG_ERR, "SMS disabled");
		__goToError();
	}

    SET_SMS_LOG_MASK_TO_DEBUG_LEVEL

	// set status - busy
	if (!dbSetStat(CNSMGR_DB_KEY_SIMCARDPIN, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_busy, NULL))
		syslog(LOG_ERR, "dbSetStat(busy) failed");

	char* pszCmds[] = {"send", "readread", "readunread", "delete", "senddiag", "setsmsc", NULL};
	iCmd = dbGetCmdIdx(pszCmds, pValue);

	switch (iCmd)
	{
		case 3:
		{
			int nMsgId;

			// get msg id
			if (!dbGetInt(CNSMGR_DB_KEY_SMS, 4, &nMsgId))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SMS, 4));
				__goToError();
			}

			// delete
			__goToErrorIfFalse(cnsDelSms((BYTE)nMsgId));
			__goToErrorIfFalse(cnsReadReceivedSmsStatus());
			break;
		}

		case 1:
			__goToErrorIfFalse(cnsReadReadSms());
			__goToErrorIfFalse(cnsReadReceivedSmsStatus());
			goto success;
			break;

		case 2:
			__goToErrorIfFalse(cnsReadUnreadSms());
			__goToErrorIfFalse(cnsReadReceivedSmsStatus());
			goto success;
			break;

		case 0:
		case 4:
		{
			if (!checkSimStatus(TRUE))
			{
				sprintf(achErr, "SIM status error");
				__goToError();
			}

			// read phone number
			if (!dbGetStr(CNSMGR_DB_KEY_SMS, 2, achPhoneNo, &cbhPhoneNo))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SMS, 2));
				__goToError();
			}

			char* pMsg = NULL;
			char* pEncodeMsg = NULL;
			int cbMsg = BSIZE_256, cbEncodeMsg = 0, rd_cnt;
			int encode_type;
			struct stat sb;
			int fp;

			read_smstools_encoding_scheme();

			/* Do not use rdb variable as a message body container
			 * because if the message is extremely large RDB manager
			 * becomes panic and eventually it will cause kernel crash.
			 * rdb_set wwan.0.sms.cmd.param.message "${TEXT}" 2>/dev/null */
			#if (0)
			// get message length
			dbGetLength(CNSMGR_DB_KEY_SMS, 3, &cbMsg);
			#endif

			/* read message from file */
			if (!dbGetStr(CNSMGR_DB_KEY_SMS, 6, tempstr, &cbMsg))
			{
				syslog(LOG_ERR, "cannot read message file name[%s]", dbGetKey(CNSMGR_DB_KEY_SMS, 6));
				__goToError();
			}
			sprintf(tempstr, "%s.raw", tempstr);
			if (stat(tempstr, &sb) == -1) {
				syslog(LOG_ERR, "failed to stat of %s", tempstr);
				__goToError();
		    }
			syslog(LOG_ERR, "message file size = %lld", (long long)sb.st_size);
			/* Caculated maximum message length which can be sent within one concatenated message
			 * defined in Sierra Wireless CNS guide is 61350.
			 * 150 bytes in first CNS packet and 240 bytes in each of following 245 CNS packet */
			#define MAX_LENGTH_OF_CONCATENATED_MSG	(150+255*240)
			if (sb.st_size > MAX_LENGTH_OF_CONCATENATED_MSG) {
				syslog(LOG_ERR, "msg length is over possible concatenated message limit %d", MAX_LENGTH_OF_CONCATENATED_MSG);
				__goToError();
			}
			fp = open(tempstr, O_RDONLY);
			if (fp < 0) {
				syslog(LOG_ERR, "failed to open msg file %s", tempstr);
				__goToError();
			}
			cbMsg = sb.st_size;
			/* allocate buffer */
			if (cbMsg != 0) {
				pMsg = __alloc(cbMsg);
				if (!pMsg) {
					syslog(LOG_ERR, "failed to allocate raw tx msg memory for %d bytes", cbMsg);
					__goToError();
				}
			}
			rd_cnt = read(fp, pMsg, cbMsg);
			if (rd_cnt <= cbMsg) {
				cbMsg = rd_cnt;
			}
			pMsg[cbMsg] = 0;
			//syslog(LOG_ERR, "successfully read %d bytes", cbMsg);
			//syslog(LOG_ERR, "pMsg: %s", pMsg);
			//printMsgBody(pMsg, cbMsg);
			
			/* Do not use rdb variable as a message body container
			 * because if the message is extremely large RDB manager
			 * becomes panic and eventually it will cause kernel crash.
			 * rdb_set wwan.0.sms.cmd.param.message "${TEXT}" 2>/dev/null */
			#if (0)
			if (!dbGetStr(CNSMGR_DB_KEY_SMS, 3, pMsg, &cbMsg))
			{
				sprintf(achErr, "cannot read [%s] - cbMsg=%d", dbGetKey(CNSMGR_DB_KEY_SMS, 3),cbMsg);
				__free(pMsg);
				__goToError();
			}
			#endif

			syslog(LOG_ERR, "msg len %d, text = %s", strlen(pMsg), pMsg);

			_runtimeStat.smsConfigurationDetail.dcsInfo.bDcsDataType = 0;

			/* set SMS routing option */
#if defined(PLATFORM_PLATYPUS)
			nvram_close(RT2860_NVRAM);
			nvram_init(RT2860_NVRAM);
			rd_data = nvram_bufget(RT2860_NVRAM, RDB_VAR_SMS_ROUTING_OPTION);
			if (rd_data) {
				strcpy(tempstr, rd_data);
#else
			if (rdb_get_single(RDB_VAR_SMS_ROUTING_OPTION, tempstr, BSIZE_256) == 0) {
#endif
				routing_option = atoi(tempstr);
				if (routing_option < 0 || routing_option > 3) {
					syslog(LOG_ERR, "invalid routing option value %d, use current setting", routing_option);
				} else {
					_runtimeStat.smsConfigurationDetail.bSmsRoutingOpt = routing_option;
					syslog(LOG_ERR, "set SMS routing option to %s",
						routing_option==0? "Packet-switched":routing_option==1? "Circuit-switched":
						routing_option==2? "Packet-switched preferred":"Circuit-switched preferred");
				}
			} else {
				syslog(LOG_ERR, "failed to read %s, use current setting", RDB_VAR_SMS_ROUTING_OPTION);
			}
#if defined(PLATFORM_PLATYPUS)
			nvram_strfree(rd_data);
			nvram_close(RT2860_NVRAM);
#endif

			/* convert ascii data to GSM-7 bit or UCS-2 according to SIM DCS data type or DB config. */
			pEncodeMsg = __alloc(cbMsg*2);
			if (!pEncodeMsg) {
				syslog(LOG_ERR, "failed to allocate encoded msg memory for %d bytes", cbMsg*2);
				__free(pMsg);
				__goToError();
			}
			/* If send extended chars via Sierra modem with GSM 7bit encoding, they are broken.
			 * Instead of GSM 7bit encoding, use UCS-2 encoding.
			 */
			if (check_extended_char(pMsg, cbMsg) || smstools_encoding_scheme == CNSMGR_ENCODE_UCS_2)
			{
			    encode_type = CNSMGR_ENCODE_UCS_2;
			}
			else
			{
			    encode_type = _runtimeStat.smsConfigurationDetail.dcsInfo.bDcsDataType;
			}
			cbEncodeMsg = smssend_encodeMsg(pMsg, pEncodeMsg, cbMsg, encode_type);
			if (cbEncodeMsg < 0) {
				syslog(LOG_ERR, "encode memery allocation error");
				__free(pMsg);
				__free(pEncodeMsg);
				__goToError();
			}
			__goToErrorIfFalse(cnsSendSms(achPhoneNo, pEncodeMsg, CNSMGR_TIMEOUT_NOTIFICATION, CNSMGR_TIMEOUT_NOTIFICATION, encode_type, cbEncodeMsg));
			__free(pMsg);
			__free(pEncodeMsg);
			break;
		}

		case 5:
		{
			// read new SMSC address
			if (!dbGetStr(CNSMGR_DB_KEY_SMS, 2, achPhoneNo, &cbhPhoneNo))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SMS, 2));
				__goToError();
			}
			__goToErrorIfFalse(cnsSetSmscAddress((char *)&achPhoneNo[0]));

			break;
		}

		default:
		{
			dbGetCmdErrMsg(pszCmds, achErr);
			__goToError();
		}

	}


success:
	// set status - done
	if (!dbSetStat(CNSMGR_DB_KEY_SIMCARDPIN, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
		syslog(LOG_ERR, "dbSetStat(done) failed");
	/* For SMS Tx, tx result should be set after receiving positive response
	 * for SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK command else tx error should be set.
	 */
	if (iCmd != 0)
		if (!dbSetStat(CNSMGR_DB_KEY_SMS, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, pszCmds[iCmd]))
			syslog(LOG_ERR, "dbSetStat(done) failed");

	*ppNewValue = NULL;
    //SET_SMS_LOG_MASK_TO_ERROR_LEVEL
	return TRUE;

error:
	if (!dbSetStat(CNSMGR_DB_KEY_SIMCARDPIN, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
		syslog(LOG_ERR, "failed to write status key");
	if (iCmd != 0)
	{
		if (!dbSetStat(CNSMGR_DB_KEY_SMS, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
			syslog(LOG_ERR, "failed to write status key");
	}
	else
	{
		/* For SMS Tx, tx result should be set separately to avoid conflict with incoming SMS */
		if (!dbSetStat(CNSMGR_DB_KEY_SMS, 5, database_reserved_msg_error, achErr))
			syslog(LOG_ERR, "failed to write status key");
	}

	*ppNewValue = NULL;
    //SET_SMS_LOG_MASK_TO_ERROR_LEVEL
    return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbCmdSimCardPin(char* pValue, int cbValue, char** ppNewValue)
{
	char achErr[CNSMGR_MAX_VALUE_LENGTH] = "";

	/* Ignore SIM command if SIM operation is done by other manager.
	 * All SIM operations may already be blocked but for safety. */
	if(is_cpinc_command_supporting() == 1) {
		*ppNewValue = NULL;
		return TRUE;
	}

	// set status - busy
	if (!dbSetStat(CNSMGR_DB_KEY_SIMCARDPIN, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_busy, NULL))
		syslog(LOG_ERR, "dbSetStat(busy) failed");

	char* pszCmds[] = {"enablepin", "disablepin", "changepin", "verifypin", "verifypuk", "check", "unlockmep", NULL};
	int iCmd = dbGetCmdIdx(pszCmds, pValue);

	switch (iCmd)
	{
		case 6:
		{
			char mepCode[CNSMGR_MAX_VALUE_LENGTH];
			int cbmepCode = sizeof(mepCode);

			// read newpin
			if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 6, mepCode, &cbmepCode))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 6));
				__goToError();
			}

			__goToErrorIfFalse(sendMEPUnlockCode(mepCode));

			break;
		}

		case 5:
		{
			if(!cnsGetEnChv1())
			{
				sprintf(achErr, "CnS communication failure");
				__goToError();
			}

			break;
		}

		case 4:
		{
			char achNewPin[CNSMGR_MAX_VALUE_LENGTH];
			int cbNewPin = sizeof(achNewPin);

			char achPuk[CNSMGR_MAX_VALUE_LENGTH];
			int cbPuk = sizeof(achPuk);

			// read newpin
			if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 4, achNewPin, &cbNewPin))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 4));
				__goToError();
			}

			// read puk
			if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 5, achPuk, &cbPuk))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 5));
				__goToError();
			}

			__goToErrorIfFalse(cnsVeriChvCode(0x0003, achNewPin, achPuk));

			// change SIM status to "SIM BUSY"
			// workaround for those SIM cards that take longer than 15 seconds - BIP 1024 Telstra memory SIM card and some Vodafone SIM cards
			if(cnsmgr_is_BasicCnsPacket(SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS,-1)) {
				char* ppValueList[]={"SIM BUSY",0};

				syslog(LOG_INFO,"##cns## change SIM card status to SIM BUSY");
				cnsmgr_updateDatabase(ppValueList, __countOf(ppValueList), SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, -1);
			}

			break;
		}

		case 3:
		{
			char achPin[CNSMGR_MAX_VALUE_LENGTH];
			int cbPin = sizeof(achPin);

			// get read pin
			if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 3, achPin, &cbPin))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 3));
				__goToError();
			}

			__goToErrorIfFalse(cnsVeriChvCode(0x0001, achPin, NULL));

			// change SIM status to "SIM BUSY"
			// workaround for those SIM cards that take longer than 15 seconds - BIP 1024 Telstra memory SIM card and some Vodafone SIM cards
			if(cnsmgr_is_BasicCnsPacket(SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS,-1)) {
				char* ppValueList[]={"SIM BUSY",0};

				syslog(LOG_INFO,"##cns## change SIM card status to SIM BUSY");
				cnsmgr_updateDatabase(ppValueList, __countOf(ppValueList), SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS, -1);
			}

			break;
		}

		case 0:
		case 1:
		{
			char achPin[CNSMGR_MAX_VALUE_LENGTH];
			int cbPin = sizeof(achPin);
			BOOL fEn = iCmd == 0;


			// get read pin
			if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 3, achPin, &cbPin))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 3));
				__goToError();
			}

			/* do sim pin operation twice if it is already in the same status */
			if((simcard_pin_enabled && fEn) || (!simcard_pin_enabled && !fEn)) {
				
				/* schedule the following simcard operation */
				strcpy(simcard_pin,achPin);
				simcard_schedule_enable_pin = fEn;

				/* start */
				__goToErrorIfFalse(cnsEnChv1(!fEn, achPin));
			}
			else {
				__goToErrorIfFalse(cnsEnChv1(fEn, achPin));
			}
			break;
		}

		case 2:
		{
			char achOld[CNSMGR_MAX_VALUE_LENGTH];
			int cbOld = sizeof(achOld);
			char achNew[CNSMGR_MAX_VALUE_LENGTH];
			int cbNew = sizeof(achNew);

			// get read pin
			if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 3, achOld, &cbOld))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 3));
				__goToError();
			}
			// get read new pin
			if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 4, achNew, &cbNew))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 4));
				__goToError();
			}

			__goToErrorIfFalse(cnsChgChvCodes(1, achOld, achNew));

			break;
		}

		default:
		{
			dbGetCmdErrMsg(pszCmds, achErr);
			__goToError();
		}
	}

	// set status - done
	if (!dbSetStat(CNSMGR_DB_KEY_SIMCARDPIN, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
		syslog(LOG_ERR, "dbSetStat(done) failed");


	*ppNewValue = NULL;
	return TRUE;

error:
	if (!dbSetStat(CNSMGR_DB_KEY_SIMCARDPIN, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
		syslog(LOG_ERR, "failed to write status key");

	*ppNewValue = NULL;
	return TRUE;

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbCmdSetProvider(char* pValue, int cbValue, char** ppNewValue)
{
	char achErr[CNSMGR_MAX_VALUE_LENGTH] = "";

	// set status - busy
	if (!dbSetStat(CNSMGR_DB_KEY_CURRENTBAND, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_busy, NULL))
		syslog(LOG_ERR, "dbSetStat(busy) failed");

	char* pszCmds[] = {"1", "5", "-1", NULL};
	int iCmd = dbGetCmdIdx(pszCmds, pValue);

	switch (iCmd)
	{
		// scan
		case 0:
		{
			syslog(LOG_INFO,"##cmd## start network scan command(%s) ",pszCmds[iCmd]);
			
			syslog(LOG_INFO,"##cmd## send SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS get");
			__goToErrorIfFalseLog(
				sierracns_writeGet(_pCns, SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS),
				"##cmd## failed in sierracns_writeGet()"
			);
			
			__goToErrorIfFalseLog(
				cnsScanOperators(),
				"##cmd## failed in cnsScanOperators()"
			);
			break;
		}

		// select
		case 1:
		{
			int cLen;
			
			syslog(LOG_INFO,"##cmd## start network select command(%s) ",pszCmds[iCmd]);

			// get selection mode from database
			char achSelMode[CNSMGR_MAX_VALUE_LENGTH];
			cLen=sizeof(achSelMode);
			if (!dbGetStr(CNSMGR_DB_PLMN, 3, achSelMode, &cLen))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_PLMN, 3));
				__goToError();
			}

			int nSelMode=0;
			int nMCC=0;
			int nMNC=0;

			sscanf(achSelMode,"%d,%d,%d",&nSelMode,&nMCC,&nMNC);

			syslog(LOG_INFO,"##cmd## select network (mode=%d,mcc=%d,mnc=%d)",nSelMode,nMCC,nMNC);
			
			__goToErrorIfFalse(cnsSetOperator((unsigned char)nSelMode,(unsigned short)nMCC,(unsigned short)nMNC));
			goto success;
			break;
		}

		case 2:
		{
			return FALSE;
			break;
		}

		default:
		{
			dbGetCmdErrMsg(pszCmds, achErr);
			__goToError();
		}
	}

	// set status - done
	if (!dbSetStat(CNSMGR_DB_KEY_CURRENTBAND, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
		syslog(LOG_ERR, "dbSetStat(done) failed");

success:
	*ppNewValue = NULL;
	return FALSE;

error:
	if (!dbSetStat(CNSMGR_DB_KEY_CURRENTBAND, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
		syslog(LOG_ERR, "failed to write status key");

	*ppNewValue = NULL;
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbCmdCurBand(char* pValue, int cbValue, char** ppNewValue)
{
	char achErr[CNSMGR_MAX_VALUE_LENGTH] = "";

	// set status - busy
	if (!dbSetStat(CNSMGR_DB_KEY_CURRENTBAND, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_busy, NULL))
		syslog(LOG_ERR, "dbSetStat(busy) failed");

	char* pszCmds[] = {"get", "set", NULL};
	int iCmd = dbGetCmdIdx(pszCmds, pValue);

	switch (iCmd)
	{
		case 0:
		{
			__goToErrorIfFalse(cnsGetCurBand());
			break;
		}

		case 1:
		{
			char achBand[CNSMGR_MAX_VALUE_LENGTH];
			int cbBand;
			UINT64 ul64Band;

			// get read band from database
			if (!dbGetStr(CNSMGR_DB_KEY_CURRENTBAND, 2, achBand, &cbBand))
			{
				sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_CURRENTBAND, 2));
				__goToError();
			}

			// convert to code
			if (!cnsmgr_convertStrToBand(achBand, &ul64Band))
			{
				sprintf(achErr, "The band(%s:%s) is not supported", dbGetKey(CNSMGR_DB_KEY_CURRENTBAND, 2), achBand);
				__goToError();
			}

			__goToErrorIfFalse(cnsSetCurBand(ul64Band));

			goto success;
			break;
		}

		default:
		{
			dbGetCmdErrMsg(pszCmds, achErr);
			__goToError();
		}
	}

	// set status - done
	if (!dbSetStat(CNSMGR_DB_KEY_CURRENTBAND, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
		syslog(LOG_ERR, "dbSetStat(done) failed");

success:
	*ppNewValue = NULL;
	return TRUE;

error:
	if (!dbSetStat(CNSMGR_DB_KEY_CURRENTBAND, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
		syslog(LOG_ERR, "failed to write status key");

	*ppNewValue = NULL;
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL dbCmdVoiceCall(char* pValue, int cbValue, char** ppNewValue)
{
	char achDial[CNSMGR_MAX_VALUE_LENGTH];
	int cbDial = sizeof(achDial);

	char achErr[CNSMGR_MAX_VALUE_LENGTH] = "";

	// set status - busy
	if (!dbSetStat(CNSMGR_DB_KEY_VOICECALL, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_busy, NULL))
		syslog(LOG_ERR, "dbSetStat(busy) failed");

	char* pszCmds[] = { RDB_VOICECALL_CMD_DIAL, RDB_VOICECALL_CMD_HANGUP, RDB_VOICECALL_CMD_PICKUP, NULL};
	int iCmd = dbGetCmdIdx(pszCmds, pValue);

	switch (iCmd)
	{
		case 0:
		{
			// get phone number
			if (!dbGetStr(SIERRACNS_PACKET_OBJ_INITIATE_VOICE_CALL, 0, achDial, &cbDial))
			{
				sprintf(achErr, "failed to read key#%s", dbGetKey(SIERRACNS_PACKET_OBJ_INITIATE_VOICE_CALL, 0));
				__goToError();
			}

			if (!cbDial || !strlen(achDial))
			{
				sprintf(achErr, "key#%s required", dbGetKey(SIERRACNS_PACKET_OBJ_INITIATE_VOICE_CALL, 0));
				__goToError();
			}

			// do dial
			__goToErrorIfFalse(cnsVcDial(achDial));
			break;
		}

		case 1:
		case 2:
		{
			int nCallId;
			int nAction;

			// get call id
			if (!dbGetInt(CNSMGR_DB_KEY_VOICECALL, 3, &nCallId))
			{
				sprintf(achErr, "failed to read key#%s", dbGetKey(CNSMGR_DB_KEY_VOICECALL, 3));
				__goToError();
			}

			// get action
			if (!dbGetInt(CNSMGR_DB_KEY_VOICECALL, 4, &nAction))
			{
				sprintf(achErr, "failed to read key#%s", dbGetKey(CNSMGR_DB_KEY_VOICECALL, 4));
				__goToError();
			}

			__goToErrorIfFalse(cnsVcAccept((unsigned char)nAction, (unsigned short)nCallId));
			break;
		}

		default:
		{
			dbGetCmdErrMsg(pszCmds, achErr);
			__goToError();
		}
	}

	// set status - busy
	if (!dbSetStat(CNSMGR_DB_KEY_VOICECALL, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_done, NULL))
		syslog(LOG_ERR, "dbSetStat(done) failed");

	*ppNewValue = NULL;
	return TRUE;

error:
	if (!dbSetStat(CNSMGR_DB_KEY_VOICECALL, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
		syslog(LOG_ERR, "failed to write status key");

	*ppNewValue = NULL;
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef GPS_ENABLED
#ifndef GPS_ON_AT
BOOL dbCmdGPS( char* pValue, int cbValue, char** ppNewValue )
{
	char achErr[CNSMGR_MAX_VALUE_LENGTH] = "";
	int nObjId=SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION;
	char* pszCmds[] = {"disable", "enable", "agps", NULL};
	int iCmd = dbGetCmdIdx(pszCmds, pValue);

	// init gps variables
	_gpsAGPS=_gpsError=0;
	_gpsCurrentMode=iCmd;

	// clear error string
	if (!dbSetStr(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4, ""))
		syslog(LOG_ERR, "failed to write %s",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 4));

	switch (iCmd)
	{
		// disable or enable gps tracking
		case 0:
		case 1:
		{
			_gpsInTracking=iCmd==1;

			cnsStopTracking();
			break;
		}

		case 2:
		{
			// get timeout
			if(!dbGetInt(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION,3,&_gpsTimeout))
			{
				syslog(LOG_ERR, "no timeout specified in %s. default(%d) used",dbGetKey(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION, 3),GPS_DEFAULT_TIMEOUT);
				_gpsTimeout=GPS_DEFAULT_TIMEOUT;
			}

			cnsStopTracking();
			break;
		}

		default:
		{
			dbGetCmdErrMsg(pszCmds, achErr);
			__goToError();
		}
	}


	*ppNewValue = NULL;
	return TRUE;

error:
	if (!dbSetStat(nObjId, CNSMGR_DB_SUBKEY_STAT, database_reserved_msg_error, achErr))
		syslog(LOG_ERR, "failed to write status key");

	*ppNewValue = NULL;
	return TRUE;
}
#endif  /* GPS_ON_AT */
#endif /* GPS_ENABLED */

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_callbackOnDatabaseOverDialDigits(char* pValue, int cbValue, char** ppNewValue)
{
	char achDigit[256];
	int cbLen;

	cbLen = sizeof(achDigit);
	if (!dbGetStr(SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 0, achDigit, &cbLen))
	{
		syslog(LOG_ERR, "dbGetStr() failed");
		__goToError();
	}

	int nOnDuration;
	if (!dbGetInt(SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 1, &nOnDuration))
	{
		syslog(LOG_ERR, "dbGetStr() failed");
		__goToError();
	}

	int nOffDuration;
	if (!dbGetInt(SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 2, &nOffDuration))
	{
		syslog(LOG_ERR, "dbGetInt() failed");
		__goToError();
	}

	BOOL fEarPiece;
	if (!dbGetInt(SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS, 3, &fEarPiece))
	{
		syslog(LOG_ERR, "dbGetInt() failed");
		__goToError();
	}

	if (!cnsVcDTMF(achDigit, nOnDuration, nOffDuration, fEarPiece))
	{
		syslog(LOG_ERR, "cnsVcDTMF() failed");
		__goToError();
	}

	*ppNewValue = NULL;

	return TRUE;

error:
	*ppNewValue = NULL;

	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_callbackOnDatabaseSelAdoProfile(char* pValue, int cbValue, char** ppNewValue)
{
	char achProfile[256];
	char achEarpiece[256];
	char achMicrophone[256];
	char achAudioVolume[256];
	char achAudioGen[256];

	int nProfile;
	BOOL fEarpiece;
	BOOL fMicrophone;
	int nAudioVolumn;
	BOOL fAudioGen;

	int cbLen;

	// get profile
	cbLen = sizeof(achProfile);
	if (!dbGetStr(SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 10, achProfile, &cbLen))
	{
		syslog(LOG_ERR, "database reading failure - SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE #5");
		__goToError();
	}

	// get earpiece
	cbLen = sizeof(achEarpiece);
	if (!dbGetStr(SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 11, achEarpiece, &cbLen))
	{
		syslog(LOG_ERR, "database reading failure - SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE #6");
		__goToError();
	}

	// get microphone
	cbLen = sizeof(achMicrophone);
	if (!dbGetStr(SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 12, achMicrophone, &cbLen))
	{
		syslog(LOG_ERR, "database reading failure - SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE #7");
		__goToError();
	}

	// get volume
	cbLen = sizeof(achAudioVolume);
	if (!dbGetStr(SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 13, achAudioVolume, &cbLen))
	{
		syslog(LOG_ERR, "database reading failure - SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE #8");
		__goToError();
	}

	// get audio gen
	cbLen = sizeof(achAudioGen);
	if (!dbGetStr(SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE, 14, achAudioGen, &cbLen))
	{
		syslog(LOG_ERR, "database reading failure - SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE #9");
		__goToError();
	}

	nProfile = atol(achProfile);
	fEarpiece = strcasecmp(_audioMuteTbl[0], achEarpiece) ? FALSE : TRUE;
	fMicrophone = strcasecmp(_audioMuteTbl[0], achMicrophone) ? FALSE : TRUE;
	nAudioVolumn = atol(achAudioVolume);
	fAudioGen = strcasecmp(_audioMuteTbl[0], achMicrophone) ? FALSE : TRUE;

	// call cns
	if (!cnsVcSelAdoProfile(nProfile, fEarpiece, fMicrophone, nAudioVolumn, fAudioGen))
		syslog(LOG_ERR, "cnsVcSelAdoProfile() failed");

	*ppNewValue = NULL;
	return TRUE;

error:
	*ppNewValue = NULL;
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cnsmgr_callbackOnDatabaseNotify(databasedriver* pDb)
{
	static char szValue[1024];
	int cbValue;

	char* pNew = NULL;

	const __pointTypeOf(_databaseKeyInfo[0]) pKeyInfo;
	int iIdx;

	syslog(LOG_DEBUG, "got database event");

	__forEach(iIdx, pKeyInfo, _databaseKeyInfo)
	{
		// bypass if invalid handler
		if (!pKeyInfo->lpfnHandler)
			continue;

	        /* skip common variables which are managed by cnsmgr and other prime at manager */
        	if (!is_enabled_feature(FEATUREHASH_CMD_ALLAT) && !pKeyInfo->basic_cns_vars)
	                continue;

		//syslog(LOG_ERR, "db read : %s %d", pKeyInfo->szKey, pKeyInfo->basic_cns_vars);

		// get a corresponding key
		cbValue = sizeof(szValue);
		if (!dbGetStr(pKeyInfo->nObjId, pKeyInfo->nObjIx, szValue, &cbValue))
		{
			syslog(LOG_ERR, "dbGetStr() failed - 0x%04x:%d", pKeyInfo->nObjId, pKeyInfo->nObjIx);
			continue;
		}

		// bypass if no length
		if (!cbValue || !strlen(szValue))
			continue;

		syslog(LOG_INFO, "##event## calling db handler - nObjId=0x%08x, nObjIx=%d, szValue=%s", (unsigned int)pKeyInfo->nObjId, pKeyInfo->nObjIx, szValue);

		// run handler
		if (pKeyInfo->lpfnHandler(szValue, cbValue, &pNew))
		{
			if (!dbSetStr(pKeyInfo->nObjId, pKeyInfo->nObjIx, pNew))
				syslog(LOG_ERR, "dbSetStr() failed - 0x%04x", pKeyInfo->nObjId);
		}
	}

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_createOrDeleteKeys(BOOL fCreate)
{
	int iInfo;
	int iKey;
	const __pointTypeOf(_databaseKeyInfo[0]) pKeyInfo;

	char achKey[MAX_NAME_LENGTH];

	BOOL fSucc = TRUE;
	BOOL fInSucc;

	// display all database keys
	__forEach(iInfo, pKeyInfo, _databaseKeyInfo)
	{
		for (iKey = 0;iKey < pKeyInfo->cCount;iKey++)
		{
			const char* szPrefix;

			/* skip common variables which are managed by cnsmgr and other prime at manager */
			if (!is_enabled_feature(FEATUREHASH_CMD_ALLAT) && !pKeyInfo->basic_cns_vars)
				continue;

			snprintf(achKey, sizeof(achKey), pKeyInfo->szKey, iKey);
			achKey[sizeof(achKey)-1] = 0;

			szPrefix=pKeyInfo->szPrefix;

			if (fCreate)
			{
				fInSucc = databasedriver_cmdSetSingle(_pDb, achKey, NULL, TRUE, pKeyInfo->fFifo,szPrefix);
				if (!fInSucc)
					fInSucc = databasedriver_cmdSetSingle(_pDb, achKey, NULL, FALSE, FALSE,szPrefix);

				//syslog(LOG_INFO, "##db## create - %s, FIFO:%d", achKey, pKeyInfo->fFifo);
			}
			else
			{
				fInSucc = databasedriver_cmdSetSingle(_pDb, achKey, "", FALSE, FALSE,szPrefix);
				//fInSucc = databasedriver_cmdDelSingle(_pDb, achKey);

				//syslog(LOG_INFO, "##db## delete %s", achKey);
			}

			if (!fInSucc)
				syslog(LOG_ERR, "failed to create database key - %s, error=%d", achKey, -errno );


			fSucc = fSucc && fInSucc;
		}
	}

	return fSucc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cnsmgr_finiSierraCns(void)
{
	__bypassIfNull(_pCns);

	// destroy cns
	sierracns_closeDev(_pCns);
	sierracns_destroy(_pCns);

	_pCns=NULL;

	syslog(LOG_INFO, "cns port is closed");
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_initSierraCns(void)
{
	///////////////
	// sierracns //
	///////////////

	// create cns
	if (__isNotAssigned(_pCns = sierracns_create()))
	{
		syslog(LOG_ERR, "sierracns_create() failed");
		__goToError();
	}

	// initiate cns
	sierracns_setCnsHandler(_pCns, cnsmgr_callbackOnCns);
	if (!sierracns_openDev(_pCns, _globalConfig.szCnsDevName))
	{
		static int fOpenFailed=0;

		if(!fOpenFailed)
			syslog(LOG_INFO, "CnS port(%s) doesn't exist yet - waiting for the port.", _globalConfig.szCnsDevName);
		fOpenFailed=1;

		__goToError();
	}

	{
		static struct
		{
			BYTE bModemStat;
		} __packedStruct disableModem;

		disableModem.bModemStat=1;

		syslog(LOG_NOTICE, "set radio-power mode");
		sierracns_write(_pCns, SIERRACNS_PACKET_OBJ_DISABLE_MODEM, SIERRACNS_PACKET_OP_SET, &disableModem, sizeof(disableModem));

		// sending packets that the windows driver sends

		sleep(1);

		syslog(LOG_NOTICE, "set 1083");
		sierracns_write(_pCns, 0x1083, SIERRACNS_PACKET_OP_SET, NULL, 0);
		sleep(1);

		syslog(LOG_NOTICE, "set 1071");
		unsigned char chStartUp=0;
		sierracns_write(_pCns, 0x1071, SIERRACNS_PACKET_OP_SET, &chStartUp, sizeof(chStartUp));
		sleep(1);

		syslog(LOG_NOTICE, "set default profile");
		unsigned char chDefProfile=1;
		sierracns_write(_pCns, 0x7001, SIERRACNS_PACKET_OP_SET, &chDefProfile, sizeof(chDefProfile));
		sleep(1);

/*
		syslog(LOG_NOTICE, "set default 100f");

		struct
		{
			unsigned char bSelMode;
			unsigned short wMCC;
			unsigned short wMNC;
		} __packedStruct strucPLMN;

		strucPLMN.bSelMode=0;
		strucPLMN.wMCC=htons(0);
		strucPLMN.wMNC=htons(0);

		sierracns_write(_pCns, 0x100f, SIERRACNS_PACKET_OP_SET, &strucPLMN, sizeof(strucPLMN));
		sleep(1);
*/

	}


	// initiate CnS packets
	int iObj;
	const int* pObjId;

	/* If the module support AT+CPINC command to read remaining PIN/PUK count value,
	 * skip all SIM related command in cnsmgr */
	syslog(LOG_ERR, "check whether supporting AT+CPINC or not for 30 seconds");
	{
		const static int simObjTable[] =
		{
			SIERRACNS_PACKET_OBJ_RETURN_ICCID,
			SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS,
			SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION,
			CNSMGR_DB_KEY_SIMCARDPIN
		};
		int iRetry = 30;
		while(iRetry-- > 0 && is_cpinc_command_supporting() == -1) {
			sleep(1);
		}
		if (is_cpinc_command_supporting() == 1)
		{
			syslog(LOG_ERR, "supporting AT+CPINC, disable SIM function in cnsmgr");
			__forEach(iObj, pObjId, simObjTable)
			{
				cnsmgr_turn_off_BasicCnsFlag(*pObjId, -1);
			}
		} else if (iRetry == 0){
			syslog(LOG_ERR, "timeout for checking +CPINC command support from simple_at_manager");
		} else {
			syslog(LOG_ERR, "not supporting AT+CPINC, enable SIM function in cnsmgr");
		}
	}

	/* moved from cnsmgr_init() to subscribe rdb trigger after checking 
	   module supports +CPINC command. */
	// subscribe database keys & start notification
	int iinfo;

	const __pointTypeOf(_databaseKeyInfo[0]) pKeyInfo;

	__forEach(iinfo, pKeyInfo, _databaseKeyInfo)
	{
		if (__isAssigned(pKeyInfo->lpfnHandler) &&
			(is_enabled_feature(FEATUREHASH_CMD_ALLAT) || (!is_enabled_feature(FEATUREHASH_CMD_ALLAT) && pKeyInfo->basic_cns_vars)))
		{
			//syslog(LOG_ERR, "databasedriver_cmdSetNotify() - szKey=%s", pKeyInfo->szKey);
			if (!databasedriver_cmdSetNotify(_pDb, pKeyInfo->szKey, pKeyInfo->szPrefix))
				syslog(LOG_ERR, "databasedriver_cmdSetNotify() failed - szKey=%s", pKeyInfo->szKey);
		}
	}

	const static int getObjTable[] =
	{
		SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION,
		SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_VERSION,
		SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_BUILD_DATE,
		SIERRACNS_PACKET_OBJ_RETURN_HARDWARE_VERSION,
		SIERRACNS_PACKET_OBJ_RETURN_BOOT_VERSION,
		SIERRACNS_PACKET_OBJ_RETURN_MODEM_MODEL,
		SIERRACNS_PACKET_OBJ_RETURN_IMEI,
		SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS,
		SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE,
		SIERRACNS_PACKET_OBJ_SERVICE_PROVIDER_NAME,
		SIERRACNS_PACKET_OBJ_PRI,
		SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND,
		SIERRACNS_PACKET_OBJ_SELECT_PLMN,
		SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS,
	};

	// write single get objects
	__forEach(iObj, pObjId, getObjTable)
	{
		/* do not read modem model name if has modem_emulator */
		if(has_modem_emulator && *pObjId == SIERRACNS_PACKET_OBJ_RETURN_MODEM_MODEL) {
			syslog(LOG_ERR, "do not read modem model name, leave it for modem_emulator");
			continue;
		}
		if (cnsmgr_is_BasicCnsPacket(*pObjId, -1))
			sierracns_writeGet(_pCns, *pObjId);
	}

	const static int notifyObjTable[] =
	{
		SIERRACNS_PACKET_OBJ_HEARTBEAT,
		SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS,
		SIERRACNS_PACKET_OBJ_REPORT_RADIO_INFORMATION,
		//SIERRACNS_PACKET_OBJ_REPORT_CURRENT_BAND,
		SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS,
		SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM,
		SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK,
		SIERRACNS_PACKET_OBJ_REPORT_CALL_PROGRESS,
		//SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE,
		SIERRACNS_PACKET_OBJ_MANAGE_MISSED_VOICE_CALLS_COUNT,
		SIERRACNS_PACKET_OBJ_REPORT_NETWORK_STATUS,
		SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS,
		SIERRACNS_PACKET_OBJ_AVAILABLE_SERVICE_DETAIL,
		SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION,
		SIERRACNS_PACKET_OBJ_RSCP,
		SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS,
		SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST,
		SIERRACNS_PACKET_OBJ_SELECT_PLMN,

		SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_RESULT,
		SIERRACNS_PACKET_OBJ_REPORT_POSITION_DET_FAILURE,
		SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_COMPLETE,
		SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_ERROR
	};

	// register notifications
	__forEach(iObj, pObjId, notifyObjTable)
	{
		BOOL ret;
		if (cnsmgr_is_BasicCnsPacket(*pObjId, -1)) {
			ret = sierracns_writeNotifyEnable(_pCns, *pObjId);
			syslog(LOG_INFO, "sierracns_writeNotifyEnable(%d, 0x%x) = %d", iObj, *pObjId, ret);
		}
	}


	const static int permObjTable[] =
	{
		SIERRACNS_PACKET_OBJ_RETURN_RADIO_TEMPERATURE,
		SIERRACNS_PACKET_OBJ_RETURN_RADIO_VOLTAGE,
		SIERRACNS_PACKET_OBJ_RETURN_MODEM_DATE_AND_TIME,
		//SIERRACNS_PACKET_OBJ_RETURN_PROFILE_SUMMARY,
		SIERRACNS_PACKET_OBJ_RETURN_IMSI,
		//SIERRACNS_PACKET_OBJ_MANAGE_RADIO_POWER
		SIERRACNS_PACKET_OBJ_RETURN_ICCID
	};

	// register permanant CnS gets
	__forEach(iObj, pObjId, permObjTable)
	{
		BOOL fStopIfError=TRUE;

		fStopIfError=*pObjId != SIERRACNS_PACKET_OBJ_RETURN_IMSI;
		if (cnsmgr_is_BasicCnsPacket(*pObjId, -1))
			sierracns_addPermGet(_pCns, *pObjId, 5, fStopIfError);
	}
	//dbSetStr(CNSMGR_DB_PLMN,CNSGMR_DB_SUBKEY_CMD,"5");

	//dbSetStr(CNSMGR_DB_KEY_CURRENTBAND,CNSGMR_DB_SUBKEY_CMD,"set");
	syslog(LOG_NOTICE, "sierrancns module initialized");

	return TRUE;

error:
	cnsmgr_finiSierraCns();
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_init(void)
{
	////////////////////
	// runtime status //
	////////////////////

	// init runtime status
	__zeroObj(&_runtimeStat);


	/////////////////////
	// database driver //
	/////////////////////

	// create database driver
	if (__isNotAssigned(_pDb = databasedriver_create()))
	{
		syslog(LOG_ERR, "databasedriver_create() failed");
		__goToError();
	}

	// set database prefix
	sprintf(_wwanPrefix, CNSMGR_DB_KEY_PREFIX, _globalConfig.iInstance);
#ifdef GPS_ENABLED
	sprintf(_agpsPrefix, CNSMGR_DB_AGPS_PREFIX, _globalConfig.iInstance);
#endif
	databasedriver_setKeyPrefix(_pDb, _wwanPrefix);

	syslog(LOG_INFO, "select key prefix - %s", _wwanPrefix);

	// set notify handler
	if (!databasedirver_setNotifyHandler(_pDb, cnsmgr_callbackOnDatabaseNotify))
	{
		syslog(LOG_ERR, "databasedirver_setNotifyHandler() failed");
		__goToError();
	}

	// start database session
	if (!databasedriver_openSession(_pDb))
	{
		syslog(LOG_ERR, "failed to open database");
		__goToError();
	}

	// create database keys
	if (!cnsmgr_createOrDeleteKeys(TRUE))
		syslog(LOG_ERR, "Failed to make one of database keys. It might be left-over from the last execution");

	// create version field
	BOOL fName = dbSetStr(CNSMGR_DB_KEY_CNS_VERSION, 0, "CnS manager");
	BOOL fVersion = dbSetStr(CNSMGR_DB_KEY_CNS_VERSION, 1, CNS_MANAGER_VERSION);
	if (!fName || !fVersion)
	{
		if (!_globalConfig.fDaemon)
		{
			printf("Failed to create a version key. It may be caused by abnormal termination. You may need to reload database driver as following:\n");
			printf("\n\t# rmmod cdcs_DD.ko\n");
			printf("\t# insmod cdcs_DD.ko\n\n");
		}

		syslog(LOG_ERR, "failed to create version keys");
	}


	/////////
	// SMS //
	/////////

	// create sms parser
	if (is_enabled_feature(FEATUREHASH_CMD_ALLAT) && !_globalConfig.fSmsDisabled)
	{
		syslog(LOG_ERR, "run cnsmgr with SMS function!");
		if (__isNotAssigned(_pSmsRecv = smsrecv_create()))
		{
			syslog(LOG_ERR, "failed to create smsrecv object");
			__goToError();
		}

		if (__isNotAssigned(_pSmsSend = smssend_create()))
		{
			syslog(LOG_ERR, "failed to create smssend object");
			__goToError();
		}
	}
	else
	{
		syslog(LOG_ERR, "run cnsmgr without SMS function!");
		/* turn off fSmsDisabled for the case SMS is disabled by application argument */
		_globalConfig.fSmsDisabled = 1;
	}

	char szValue[10];
	/* do not read modem model name if has modem_emulator */
	if(rdb_get_single("confv250.enable",szValue,10) == 0 && strcmp(szValue, "1") == 0) {
		has_modem_emulator = TRUE;
	}

	syslog(LOG_ERR, "----------------------------------------------------------------------");
	return TRUE;

error:
	return FALSE;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum
{
	cnsportstatus_preparing,
	cnsportstatus_stopped,
	cnsportstatus_running,
} cnsportstatus;

static cnsportstatus _statInDb=cnsportstatus_preparing;


STRCONVERT_TABLE_BEGIN(CnsPortStatus)
{
	"preparing",
	"stopped",
	"running"
}
STRCONVERT_TABLE_END(CnsPortStatus)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setCnsPortStatInDb(cnsportstatus curCnsPortStat)
{
	if(curCnsPortStat==_statInDb)
		return;

	const char* szStat=STRCONVERT_FUNCTION_CALL(CnsPortStatus, curCnsPortStat);

	if(!dbSetStr(CNSMGR_DB_KEY_CNS_VERSION, 2, (char*)szStat))
		syslog(LOG_ERR,"database write error - %s",dbGetKey(CNSMGR_DB_KEY_CNS_VERSION, 2));

	_statInDb=curCnsPortStat;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void read_smsc_address( void )
{
	static int retry_cnt = 0;

	/* do not try to read SMSC address when SMS feature is disabled */
	if (_globalConfig.fSmsDisabled) {
		return;
	}

	/* return if already has smsc address or exceeded retry count */
	if (_runtimeStat.fRecvSmsCfgDetail || retry_cnt >= 20) {
		return;
	}

	/* return if SIM card is not ready because Sierra modem does not give smsc adderss
	 * when SIM card is not ready */
	if (!checkSimStatus(FALSE)) {
		return;
	}

	syslog(LOG_ERR, "resend SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS, retry_cnt = %d", retry_cnt);
	(void) cnsgetSmscAddress();
	retry_cnt++;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL cnsmgr_loop(void)
{
	fd_set fdR;
	fd_set fdW;

	sierrahip* pHip=NULL;
	int hHip=-1;

	int cbCnsErrorCnt=0;

	BOOL fInit=TRUE;

	// get handles
	int hDb = databasedriver_getHandle(_pDb);

	while (TRUE)
	{
		// open if not opened
		if(!__isAssigned(_pCns))
		{
			modulemonitor_resetTick();

			syslog(LOG_NOTICE, "opening cns port");
			if(!cnsmgr_initSierraCns())
			{
				syslog(LOG_INFO, "failed to open cns port - sleeping 1 sec");
				sleep(1);
			}

			if(!fInit)
			{
				cbCnsErrorCnt++;
				dbSetInt(CNSMGR_DB_KEY_CNS_VERSION,3,cbCnsErrorCnt);
			}

			fInit=FALSE;
		}

		struct timeval tv = {1, 0};

		//////////////////
		// build fd set //
		//////////////////

		// setup fd set
		FD_ZERO(&fdR);
		FD_ZERO(&fdW);

		if(__isAssigned(_pCns))
		{
			pHip = _pCns->pHip;
			hHip = sierrahip_getHandle(pHip);
			FD_SET(hHip, &fdR);

			// set write-flag if there is any data to send
			if (!sierrahip_isWriteBufEmpty(pHip))
				FD_SET(hHip, &fdW);
		}

		FD_SET(hDb, &fdR);

		int nFd;

		if(hHip<0)
			nFd = hDb + 1;
		else
			nFd = __max(hHip, hDb) + 1;

		////////////
		// select //
		////////////

		// select
		int nSel = select(nFd, &fdR, &fdW, NULL, &tv);

		// signaled
		if (_fTermSigDetected)
		{
			syslog(LOG_ERR, "delayed signal detected");
			__goToError();
		}

		// byebye for unknown return
		if (nSel < 0)
		{
			// if system call
			if (errno == EINTR)
			{
				syslog(LOG_ERR,"system call detected");
				continue;
			}

			syslog(LOG_ERR, "abnormal termination from the main select");
			__goToError();
		}

		////////////////////
		// process select //
		////////////////////

		// call corresponding handles if signaled
		if (nSel)
		{
			if(!(hHip<0))
			{
				// read for hip
				if (FD_ISSET(hHip, &fdR))
				{
					modulemonitor_beatIt();
					if(sierrahip_onRead(pHip)==0)
					{
						syslog(LOG_ERR, "module disconnection detected!!");

						sierracns_destroy(_pCns);
						_pCns=NULL;

						syslog(LOG_ERR, "CnS port is closed");
						//continue;
						break;
					}

					// set module running status to run
					setCnsPortStatInDb(cnsportstatus_running);
				}

				// write for hip
				if (FD_ISSET(hHip, &fdW))
					sierrahip_onWrite(pHip);
			}

			// read for database
			if (FD_ISSET(hDb, &fdR))
				databasedriver_onRead(_pDb);
		}

		// call tick whatever it is
		if(_pCns) {
			// initialize band selection mode after factory reset
			cnsmgr_initBandSelMode();
			sierracns_onTick(_pCns);
		}

		// close cns port if heartbeat stops
		if(__isNotAssigned(_pCns) && !modulemonitor_isBeating())
		{
			syslog(LOG_ERR, "cns heartbeat failure detected. reinitializing cns port");
			cnsmgr_finiSierraCns();

			setCnsPortStatInDb(cnsportstatus_stopped);
		}

		/* check SMS rx spool folder */
		polling_rx_sms_event();

		// synchronize operation mode after restoring configuration.
		sync_operation_mode();
		
		/* check and read smsc address every seconds */
		if (nSel == 0) {
			read_smsc_address();
		}
	}

	cnsmgr_finiSierraCns();
	return TRUE;

error:
	cnsmgr_finiSierraCns();
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
void sigDummyHandler(int iSig)
{
	const char* szSig = strsignal(iSig);

	switch (iSig)
	{
		case SIGTERM:
			fprintf(stderr, "%s detected\n", szSig);

			_fTermSigDetected = 1;
			break;

		default:
			fprintf(stderr, "%s ignored\n", szSig);
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////
static int waitForPort(const char* szPort, int ms)
{
	struct stat portStat;

	tick tckS = getTickCountMS();
	int res = -1;

	while (res < 0)
	{
		if (_fTermSigDetected)
			break;

		tick tckC = getTickCountMS();

		if (tckC > tckS + ms)
			break;

		res = stat(szPort, &portStat);
	}

	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	////////////////////
	// initialization //
	////////////////////
	initTickCount();

	if( init_feature()<0 ) {
		fprintf(stderr,"failed to initialize feature hash\n");
		exit(-1);
	}

	// initiate configuration
	__goToErrorIfFalse(globalconfig_init(argc, argv));

	// initiate syslog
	syslog_init(CNS_DAEMON_NAME, _globalConfig.iDbgPrior,!_globalConfig.fDaemon);

	// ignore hangup signal
	signormterm_ignoreSig(SIGHUP);

	// ignore otherwise getting killed - platypus platform sends SIGHUP instread of SIGUSR1
	signal(SIGHUP, sigDummyHandler);
	signal(SIGINT, sigDummyHandler);
	signal(SIGTERM, sigDummyHandler);

	/////////////////
	// cnsmgr loop //
	/////////////////

	// wait until last port appears
	if(_globalConfig.szLastDevName)
	{
		syslog(LOG_INFO,"waiting for last port - %s",_globalConfig.szLastDevName);
		if(waitForPort(_globalConfig.szLastDevName,10000)<0)
			syslog(LOG_INFO,"last doesn't appear - timeout");
	}

	// wait until cns port appears
	if(_globalConfig.szCnsDevName)
	{
		syslog(LOG_INFO,"waiting for cns port - %s",_globalConfig.szCnsDevName);
		if(waitForPort(_globalConfig.szCnsDevName,10000)<0)
			syslog(LOG_INFO,"cnsport doesn't appear - timeout");
	}

	// initiate cnsmgr and go into select loop
	BOOL fSucc;

	// daemonize the process
	if (_globalConfig.fDaemon)
	{
		char* lockFileName = "/var/lock/subsys/" CNS_DAEMON_NAME;

		daemon_init(lockFileName, CNS_RUN_AS_USER);
		syslog(LOG_INFO, "daemonized");
	}

	if (__isTrue(fSucc = cnsmgr_init()))
	{

		/* set ready flag in database to block some functions launch until cnsmgr is ready */
		if (rdb_update_single("cnsmgr.status", "ready", CREATE, ALL_PERM, 0, 0) != 0)
		{
			syslog(LOG_ERR, "failed to write ready flag!");
		} else {
			cnsmgr_loop();
		}
	}
	else
	{
		syslog(LOG_ERR, "cnsmgr_init() failed");
	}

	if (rdb_set_single("cnsmgr.status", "") != 0) {
		syslog(LOG_ERR, "failed to write ready flag!");
	}

	// finish cnsmgr
	cnsmgr_fini();

	///////////////////
	// finialization //
	///////////////////

	// finish daemon
	if (_globalConfig.fDaemon)
		daemon_fini();

	fini_feature();

	// finish syslog
	syslog_fini();

	return fSucc;

error:
	return -1;
}

