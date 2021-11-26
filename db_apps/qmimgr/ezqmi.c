#include "g.h"

#include "svcver.h"
#include "bandsel.h"
#include "custom/custom.h"
#include "ezqmi_uim.h"
#include "qmimux.h"

struct rdb_session* _s = NULL;

/* TODO: separate GPS, profile, SIM card, PLMN and band selection into seperate files (example: ezqmi_sms.c and qmi_cell_info )*/

/* TODO: change all QMI request functions to use "_qmi_easy_req_*" - ex. _qmi_gps_start_tracking() */

// callbacks
////////////////////////////////////////////////////////////////////////////////
void atmgr_callback_on_noti(struct port_t* port, const char* noti);
void atmgr_callback_on_common(struct port_t* port, unsigned short tran_id, struct strqueue_t* resultq, int timeout);

#ifdef MODULE_PRI_BASED_OPERATION
extern int vzw_pripub;
//extern int isSynchronized;
extern int count;
int synchronizeAPN(void);
#endif

// Local variables
////////////////////////////////////////////////////////////////////////////////
static int loop_running = 1;
static int cell_info_timer = 0;

// Command line options
////////////////////////////////////////////////////////////////////////////////
const char* at_port;
const char* qmi_port;
int verbosity;
int verbosity_cmdline;
int instance;
const char* last_port;
int be_daemon;
int sms_disabled;

char wwan_prefix[WWAN_PREFIX_LENGTH];
int wwan_prefix_len = 0;

// Local objects
////////////////////////////////////////////////////////////////////////////////
static struct qmiuniclient_t* uni = NULL;
struct funcschedule_t* sched = NULL;
static struct dbenum_t* dbenum = NULL;
static struct port_t* port = NULL;


// Hash resources
////////////////////////////////////////////////////////////////////////////////
static struct dbhash_t* atcmds = NULL;
static struct dbhash_t* dbvars = NULL;
static struct dbhash_t* dbcmds = NULL;
static struct dbhash_t* mccmncs = NULL;

// String resoruces
////////////////////////////////////////////////////////////////////////////////
static struct resourcetree_t* res_qmi_strings;

// String resources - res_qmi_error_codes
////////////////////////////////////////////////////////////////////////////////

static struct resourcetree_element_t res_qmi_strings_elements[] = {
	// QMI_DMS_UIM_GET_PIN_STATUS QMI_DMS_UIM_GET_PIN_STATUS_RESP_TYPE_PIN1
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, 0), "PIN not initialized"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, 1), "PIN enabled, not verified"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, 2), "PIN enabled, verified"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, 3), "PIN disabled"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, 4), "PIN blocked"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, 5), "PIN permanently blocked"},

	// QMI_DMS_UIM_GET_PIN_STATUS QMI_DMS_UIM_GET_PIN_STATUS_RESP_TYPE_PIN2
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, (1 << 16) | 0), "SIM BUSY"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, (1 << 16) | 1), "SIM PIN"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, (1 << 16) | 2), "SIM OK"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, (1 << 16) | 3), "SIM OK"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, (1 << 16) | 4), "SIM PUK"},
	{RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, (1 << 16) | 5), "SIM PUK"},

	// QMI_NAS_GET_SERVING_SYSTEM - QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x00), "None (no service)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x01), "CDMA2000 1X"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x02), "CDMA2000 HRPD (1xEV-DO)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x03), "AMPS"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x04), "GSM"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x05), "UMTS"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x08), "LTE"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, 0x09), "TD-SCDMA"},

	// QMI_NAS_GET_RF_BAND_INFO - QMI_NAS_GET_RF_BAND_INFO_RESP_TYPE
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 0), "CDMA 800-MHz (BC 0)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 1), "CDMA 1900-MHz (BC 1)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 2), "CDMA TACS (BC 2)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 3), "CDMA JTACS (BC 3)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 4), "CDMA Korean PCS (BC 4)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 5), "CDMA 450-MHz (BC 5)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 6), "CDMA 2-GHz IMT2000 (BC 6)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 7), "CDMA Upper 700-MHz (BC 7)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 8), "CDMA 1800-MHz (BC 8)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 9), "CDMA 900-MHz (BC 9)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 10), "CDMA Secondary 800 MHz (BC 10)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 11), "CDMA 400 MHz European PAMR (BC 11)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 12), "CDMA 800 MHz PAMR (BC 12)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 13), "CDMA 2.5 GHz IMT-2000 Extension (BC 13)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 14), "CDMA US PCS 1.9GHz (BC 14)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 15), "CDMA AWS (BC 15)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 16), "CDMA US 2.5GHz (BC 16)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 17), "CDMA US 2.5GHz Forward Link Only (BC 17)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 18), "CDMA 700 MHz Public Safety (BC 18)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 19), "CDMA Lower 700-MHz (BC 19)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 20), "CDMA L-Band (BC 20)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 21), "CDMA S-Band (BC 21)"},

	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 40), "GSM 450"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 41), "GSM 480"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 42), "GSM 750"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 43), "GSM 850"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 44), "GSM 900 (Extended)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 45), "GSM 900 (Primary)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 46), "GSM 900 (Railways)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 47), "GSM 1800"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 48), "GSM 1900"},

	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 80), "WCDMA 2100"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 81), "WCDMA PCS 1900"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 82), "WCDMA DCS 1800"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 83), "WCDMA 1700 (US)"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 84), "WCDMA 850"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 85), "WCDMA 800"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 86), "WCDMA 2600"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 87), "WCDMA 900"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 88), "WCDMA 1700 (Japan)"},

	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 120), "LTE Band 1 - 2100MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 121), "LTE Band 2 - 1900MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 122), "LTE Band 3 - 1800MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 123), "LTE Band 4 - 1700MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 124), "LTE Band 5 - 850MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 125), "LTE Band 6 - 800MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 126), "LTE Band 7 - 2600MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 127), "LTE Band 8 - 900MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 128), "LTE Band 9 - 1700MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 129), "LTE Band 10 - 1700MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 130), "LTE Band 11 - 1500MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 131), "LTE Band 12 - 700MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 132), "LTE Band 13 - 700MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 133), "LTE Band 14 - 700MHz"},

	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 134), "LTE Band 17 - 700MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 143), "LTE Band 18 - 800MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 144), "LTE Band 19 - 800MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 145), "LTE Band 20 - 800MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 146), "LTE Band 21 - 1500MHz"},

	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 152), "LTE Band 23 - 2000MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 147), "LTE Band 24 - 1600MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 148), "LTE Band 25 - 1900MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 153), "LTE Band 26 - 850MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 158), "LTE Band 28 - 700MHz"},
	//{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 159), "LTE Band 29 - 700MHz"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 154), "LTE Band 32 - 1500MHz"},

	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 135), "LTE Band 33 - TDD 2100"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 136), "LTE Band 34 - TDD 2100"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 137), "LTE Band 35 - TDD 1900"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 138), "LTE Band 36 - TDD 1900"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 139), "LTE Band 37 - TDD 1900"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 140), "LTE Band 38 - TDD 2600"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 141), "LTE Band 39 - TDD 1900"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 142), "LTE Band 40 - TDD 2300"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 149), "LTE Band 41 - TDD 2500"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 150), "LTE Band 42 - TDD 3500"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 151), "LTE Band 43 - TDD 3700"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 155), "LTE Band 125"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 156), "LTE Band 126"},
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 157), "LTE Band 127"},

	// Value returned by Quanta LTE band 40 module
	{RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, 159), "LTE E-UTRA Operating Band 40"},

	// QMI_WDS_START_NETWORK_INTERFACE
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_UNSPECIFIED), "Reason unspecified"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CLIENT_END), "Client ended the call"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_NO_SRV), "Phone has no service"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_FADE), "Call has ended abnormally"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_REL_NORMAL), "Received release from BS – no reason given"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_ACC_IN_PROG), "Access attempt already in progress; SD2.0 only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_ACC_FAIL), "Access failure for reason other than the above"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_REDIR_OR_HANDOFF), "Call rejected because of redirection or handoff"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CLOSE_IN_PROGRESS), "Call failed because close is in progress"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_AUTH_FAILED), "Authentication failed"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CDMA_LOCK), "Phone is CDMA-locked until power cycle"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_INTERCEPT), "Received intercept from BS – origination only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_REORDER), "Received reorder from BS – origination only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_REL_SO_REJ), "Received release from BS – SO reject"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_INCOM_CALL), "Received incoming call from BS"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_ALERT_STOP), "Received alert stop from BS – incoming only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_ACTIVATION), "Received end activation – OTASP call only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_MAX_ACCESS_PROBE), "Max access probes transmitted"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CCS_NOT_SUPPORTED_BY_BS), "Concurrent service is not supported by base station"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_NO_RESPONSE_FROM_BS), "No response received from base station"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_REJECTED_BY_BS), "Call rejected by the base station; CDMA only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_INCOMPATIBLE), "Concurrent services requested were not compatible; CDMA only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_ALREADY_IN_TC), "Corresponds to IN_TC ALREADY_IN_TC"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_USER_CALL_ORIG_DURING_GPS), "Used if CM is ending a GPS call in favor of a user call"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_USER_CALL_ORIG_DURING_SMS), "Used if CM is ending a SMS call in favor of a user call"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_NO_CDMA_SRV), "CDMA only; phone has no service"},

	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CONF_FAILED), "Call origination request failed; WCDMA/GSM only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_INCOM_REJ), "Client rejected the incoming call; WCDMA/GSM only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_NO_GW_SRV), "GWM/WCDMA only; phone has no service"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_NETWORK_END), "Network ended the call, look in cc_cause; WCDMA/GSM only"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_LLC_SNDCP_FAILURE), "LLC or SNDCP failure"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_INSUFFICIENT_RESOURCES), "Insufficient resources"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_OPTION_TEMP_OOO), "Service option temporarily out of order"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_NSAPI_ALREADY_USED), "NSAPI already used"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_REGULAR_DEACTIVATION), "Regular PDP context deactivation"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_NETWORK_FAILURE), "Network failure"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_UMTS_), "REATTACH_REQ Reactivation requested"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_PROTOCOL_ERROR), "Protocol error, unspecified"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_OPERATOR_DETERMINED_BARRING), "Operator-determined barring"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_UNKNOWN_APN), "Unknown or missing Access Point Name"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_UNKNOWN_PDP), "Unknown PDP address or PDP type"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_GGSN_REJECT), "Activation rejected by GGSN"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_ACTIVATION_REJECT), "Activation rejected, unspecified"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_OPTION_NOT_SUPPORTED), "Service option not supported"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_OPTION_UNSUBSCRIBED), "Requested service option not subscribed"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_QOS_NOT_ACCEPTED), "QoS not accepted"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_TFT_SEMANTIC_ERROR), "Semantic error in the TFT operation"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_TFT_SYNTAX_ERROR), "Syntactical error in the TFT operation"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_UNKNOWN_PDP_CONTEXT), "Unknown PDP context"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_FILTER_SEMANTIC_ERROR), "Semantic errors in packet filter(s)"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_FILTER_SYNTAX_ERROR), "Syntactical error in packet filter(s)"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_PDP_WITHOUT_ACTIVE_TFT), "PDP context without TFT already activated"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_INVALID_TRANSACTION_ID), "Invalid transaction identifier value"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_MESSAGE_INCORRECT_SEMANTIC), "Semantically incorrect message"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_INVALID_MANDATORY_INFO), "Invalid mandatory information"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_MESSAGE_TYPE_UNSUPPORTED), "Message type non-existent or not implemented"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_MSG_TYPE_NONCOMPATIBLE_STATE), "Message not compatible with state"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_UNKNOWN_INFO_ELEMENT), "Information element nonexistent or not implemented"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CONDITIONAL_IE_ERROR), "Conditional IE error"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE), "Message not compatible with protocol state"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_APN_TYPE_CONFLICT), "APN restriction value incompatible with active PDP context"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CD_GEN_OR_BUSY), "Abort connection setup due to the reception of a ConnectionDeny msg with deny code = general or network busy"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CD_BILL_OR_AUTH), "Abort connection setup due to the reception of a ConnectionDeny msg with deny code = billing or authentication failure"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_CHG_HDR), "Change HDR system due to redirection or PRL not preferred"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_EXIT_HDR), "Exit HDR due to redirection or PRL not preferred"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_HDR_NO_SESSION), "No HDR session"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_HDR_ORIG_DURING_GPS_FIX), "Used if CM is ending a HDR call orig in favor of GPS fix"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_HDR_CS_TIMEOUT), "Connection setup timeout"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_END_REASON_HDR_RELEASED_BY_CM), "BY_CM CM released HDR call so 1X call can continue"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_LLC_SNDCP_FAILURE), "#25 LLC SNDCP failure"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_INSUFFICIENT_RESOURCES), "#26 Insufficient resources"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_UNKNOWN_APN), "#27 Unknown APN"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_UNKNOWN_PDP), "#28 Unknown PDP"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_AUTH_FAILED), "#29 Auth failed"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_GGSN_REJECT), "#30 GGSN reject"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_ACTIVATION_REJECT), "#31 Activation reject"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_OPTION_NOT_SUPPORTED), "#32 Option not supported"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_OPTION_UNSUBSCRIBED), "#33 Option unsubscribed"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_OPTION_TEMP_OOO), "#34 Option temp 000"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_NSAPI_ALREADY_USED), "#35 NSAPI already used"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_REGULAR_DEACTIVATION), "#36 Regular deactivation"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_QOS_NOT_ACCEPTED), "#37 QOS not accepted"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_NETWORK_FAILURE), "#38 Network failure"},
	{RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, QMI_WDS_CALL_VERBOSE_END_REASON_UMTS_REACTIVATION_REQ), "#39 UMTS reactivation req"},
	/* QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT */
	/*
			• 0 – Position engine
			• 1 – GNSS background scan
			• 2 – Injected clock inconsistency
			• 3 – GPS subframe misalignment
			• 4 – Decoded time inconsistency
			• 5 – Code consistency error
			• 6 – Soft reset caused by an integer millisecond (INTMS) error
			• 7 – Soft reset caused by an RF failure
	*/
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 0), "Position engine"},
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 1), "GNSS background scan"},
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 2), "Injected clock inconsistency"},
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 3), "GPS subframe misalignment"},
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 4), "Decoded time inconsistency"},
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 5), "Code consistency error"},
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 6), "Soft reset caused by an integer millisecond (INTMS) error"},
	{RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, 7), "Soft reset caused by an RF failure"},

	/* Cinterion PHS8-P return codes */

	// QMIEX - res_qmi_error_codes
	////////////////////////////////////////////////////////////////////////////////
#define QMIEX (QMIUNICLIENT_SERVICE_CLIENT+0)
#define QMIERR 1
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NONE), "QMI_ERR_NONE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_MALFORMED_MSG), "QMI_ERR_MALFORMED_MSG"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NO_MEMORY), "QMI_ERR_NO_MEMORY"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INTERNAL), "QMI_ERR_INTERNAL"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_ABORTED), "QMI_ERR_ABORTED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_CLIENT_IDS_EXHAUSTED), "QMI_ERR_CLIENT_IDS_EXHAUSTED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_UNABORTABLE_TRANSACTION), "QMI_ERR_UNABORTABLE_TRANSACTION"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_CLIENT_ID), "QMI_ERR_INVALID_CLIENT_ID"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_HANDLE), "QMI_ERR_INVALID_HANDLE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_PROFILE), "QMI_ERR_INVALID_PROFILE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_PINID), "QMI_ERR_INVALID_PINID"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INCORRECT_PIN), "QMI_ERR_INCORRECT_PIN"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NO_NETWORK_FOUND), "QMI_ERR_NO_NETWORK_FOUND"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_CALL_FAILED), "QMI_ERR_CALL_FAILED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_OUT_OF_CALL), "QMI_ERR_OUT_OF_CALL"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NOT_PROVISIONED), "QMI_ERR_NOT_PROVISIONED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_MISSING_ARG), "QMI_ERR_MISSING_ARG"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_ARG_TOO_LONG), "QMI_ERR_ARG_TOO_LONG"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_TX_ID), "QMI_ERR_INVALID_TX_ID"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_DEVICE_IN_USE), "QMI_ERR_DEVICE_IN_USE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_OP_NETWORK_UNSUPPORTED), "QMI_ERR_OP_NETWORK_UNSUPPORTED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_OP_DEVICE_UNSUPPORTED), "QMI_ERR_OP_DEVICE_UNSUPPORTED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NO_EFFECT), "QMI_ERR_NO_EFFECT"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NO_FREE_PROFILE), "QMI_ERR_NO_FREE_PROFILE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_PDP_TYPE), "QMI_ERR_INVALID_PDP_TYPE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_TECH_PREF), "QMI_ERR_INVALID_TECH_PREF"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_PROFILE_TYPE), "QMI_ERR_INVALID_PROFILE_TYPE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_SERVICE_TYPE), "QMI_ERR_INVALID_SERVICE_TYPE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_REGISTER_ACTION), "QMI_ERR_INVALID_REGISTER_ACTION"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_PS_ATTACH_ACTION), "QMI_ERR_INVALID_PS_ATTACH_ACTION"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_AUTHENTICATION_FAILED), "QMI_ERR_AUTHENTICATION_FAILED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_PIN_BLOCKED), "QMI_ERR_PIN_BLOCKED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_PIN_PERM_BLOCKED), "QMI_ERR_PIN_PERM_BLOCKED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_UIM_NOT_INITIALIZED), "QMI_ERR_UIM_NOT_INITIALIZED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_MAX_QOS_REQUESTS_IN_USE), "QMI_ERR_MAX_QOS_REQUESTS_IN_USE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INCORRECT_FLOW_FILTER), "QMI_ERR_INCORRECT_FLOW_FILTER"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NETWORK_QOS_UNAWARE), "QMI_ERR_NETWORK_QOS_UNAWARE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_QOS_ID), "QMI_ERR_INVALID_QOS_ID"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_NUM_QOS_IDS), "QMI_ERR_NUM_QOS_IDS"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_FLOW_SUSPENDED), "QMI_ERR_FLOW_SUSPENDED"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_DATA_FORMAT), "QMI_ERR_INVALID_DATA_FORMAT"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_ARG), "QMI_ERR_INVALID_ARG"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_TRANSITION), "QMI_ERR_INVALID_TRANSITION"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INVALID_QMI_CMD), "QMI_ERR_INVALID_QMI_CMD"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_INFO_UNAVAILABLE), "QMI_ERR_INFO_UNAVAILABLE"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_EXTENDED_INTERNAL), "QMI_ERR_EXTENDED_INTERNAL"},
	{RES_STR_KEY(QMIEX, QMIERR, QMI_ERR_BUNDLING_NOT_SUPPORTED), "QMI_ERR_BUNDLING_NOT_SUPPORTED"},

	// ATCMD - HAUWEI SYSCFG
	////////////////////////////////////////////////////////////////////////////////
#define ATCMD	(QMIUNICLIENT_SERVICE_CLIENT+1)
#define AT_SYSCFG1 1
#define AT_SYSCFG2 2
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00080000), "GSM 850"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00000080), "GSM DCS systems"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00000100), "Extended GSM 900"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00000200), "Primary GSM 900"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00100000), "Railway GSM 900"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00200000), "GSM PCS"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00400000), "WCDMA IMT 2000"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x00800000), "WCDMA_II_PCS_1900"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG1, 0x04000000), "WCDMA_V_850"},
	{RES_STR_KEY(ATCMD, AT_SYSCFG2, 0x00020000), "WCDMA_VIII_900"}, // 0x0002000000000000
};


// Hash resources - atcmdvar
////////////////////////////////////////////////////////////////////////////////
#define AT_COMMAND_VAR_CIMI   0x0001
#define AT_COMMAND_VAR_HINT   0x0002
#define AT_COMMAND_VAR_CPIN   0x0003

static const struct dbhash_element_t atcmd_res[] = {
	{"AT+CIMI",			AT_COMMAND_VAR_CIMI},
	{"AT+CRSM=176,28486,0,0,17",	AT_COMMAND_VAR_HINT},
	{"AT+CPIN?",			AT_COMMAND_VAR_CPIN},
};


// Hash resources - dbvars
////////////////////////////////////////////////////////////////////////////////

enum {
	/* wwan.x. prefix RDB commands */
	DB_COMMAND_VAR_BASE = 0x1000,
	DB_COMMAND_VAR_SIM,
	DB_COMMAND_VAR_PROFILE,
	DB_COMMAND_VAR_DEBUG_LEVEL,
	DB_COMMAND_VAR_FAST_STATS,
	DB_COMMAND_VAR_PLMN,
	DB_COMMAND_VAR_BAND,
	DB_COMMAND_VAR_CELL_INFO_TIMER,
	DB_COMMAND_VAR_SMS,
	DB_COMMAND_VAR_PRIID_CARRIER,	/* status from AT command */
	DB_COMMAND_VAR_OMADM_PROGRESS,	/* status from AT command */
	DB_COMMAND_VAR_SIM_STATUS,	/* status from AT command or from qmimgr itself */
	DB_COMMAND_VAR_PRI_PROFILE,

#ifdef QMI_VOICE_y
	DB_COMMAND_VAR_VOICE_CTRL,
	DB_COMMAND_VAR_VOICE_NOTI,
#endif

	/* RDB commands without prefix */
	DB_COMMAND_WITH_NO_PREFIX = (1 << 30),
	DB_COMMAND_VAR_GPS,
};


static const struct dbhash_element_t dbcmdvar_res[] = {
	{"debug",			DB_COMMAND_VAR_DEBUG_LEVEL},
	{"fast_stats",			DB_COMMAND_VAR_FAST_STATS},
	{"sim.cmd.command",		DB_COMMAND_VAR_SIM},
	{"profile.cmd.command",		DB_COMMAND_VAR_PROFILE},
	{"PLMN_command_state",		DB_COMMAND_VAR_PLMN},
	{"currentband.cmd.command",	DB_COMMAND_VAR_BAND},

	{"cellinfo.timer",		DB_COMMAND_VAR_CELL_INFO_TIMER},
	{"sensors.gps.0.cmd.command",	DB_COMMAND_VAR_GPS},

	{"sms.cmd.command",		DB_COMMAND_VAR_SMS},

	{"priid_carrier",		DB_COMMAND_VAR_PRIID_CARRIER},
	{"pri_profile.command",		DB_COMMAND_VAR_PRI_PROFILE},
	{"cdma.otasp.progress",		DB_COMMAND_VAR_OMADM_PROGRESS},

	{"sim.status.status",		DB_COMMAND_VAR_SIM_STATUS},

#ifdef QMI_VOICE_y
	{"voice.command.ctrl",		 DB_COMMAND_VAR_VOICE_CTRL},
	{"voice.command.noti",		 DB_COMMAND_VAR_VOICE_NOTI},
#endif

};



static const struct dbhash_element_t dbcmdval_res[] = {
	// network profile
	{"clean", DB_COMMAND_VAL_CLEAN},
	{"read", DB_COMMAND_VAL_READ},
	{"write", DB_COMMAND_VAL_WRITE},
	{"delete", DB_COMMAND_VAL_DELETE},
	{"activate", DB_COMMAND_VAL_ACTIVATE},
	{"deactivate", DB_COMMAND_VAL_DEACTIVATE},
	{"getdef", DB_COMMAND_VAL_GETDEF},
	{"setdef", DB_COMMAND_VAL_SETDEF},
	// sim pin
	{"enable", DB_COMMAND_VAL_ENABLE},
	{"disable", DB_COMMAND_VAL_DISABLE},
	{"enablepin", DB_COMMAND_VAL_ENABLEPIN},
	{"disablepin", DB_COMMAND_VAL_DISABLEPIN},
	{"changepin", DB_COMMAND_VAL_CHANGEPIN},
	{"verifypin", DB_COMMAND_VAL_VERIFYPIN},
	{"verifypuk", DB_COMMAND_VAL_VERIFYPUK},
	{"check", DB_COMMAND_VAL_CHECK},
	// plmn
	{"1", DB_COMMAND_VAL_1},
	{"5", DB_COMMAND_VAL_5},
	{"-1", DB_COMMAND_VAL_NEGATIVE_1},

	// band
	{"set", DB_COMMAND_VAL_SET},
	{"get", DB_COMMAND_VAL_GET},

	// gps commands - enable, disable and agps
	{"agps", DB_COMMAND_VAL_AGPS},

	/* sms commands */
	{"senddiag", DB_COMMAND_VAL_SENDDIAG},
	{"send", DB_COMMAND_VAL_SEND},
	{"readall", DB_COMMAND_VAL_READALL},
	{"delall", DB_COMMAND_VAL_DELALL},
	{"setsmsc", DB_COMMAND_VAL_SETSMSC},

	/* pri profile command */
	{"update", DB_COMMAND_VAL_UPDATE},
};


// Database variables to reset at shutdown - TODO: dynamically build this list from _set_str_db()
////////////////////////////////////////////////////////////////////////////////
const char* dbresetvals[] = {
	"system_network_status.pdp0_stat",
	"sim.data.msisdn",
	"firmware_version",
	"manufacture",
	"model",
	"imei",
	"sim.status.status",
	"sim.status.status0",
	"sim.status.status1",
	"system_network_status.attached",
	"system_network_status.system_mode",
	"system_network_status.service_type",
	"system_network_status.system_mode0",
	"system_network_status.service_type0",
	"system_network_status.system_mode1",
	"system_network_status.service_type1",
	"system_network_status.roaming",
	"system_network_status.MCC",
	"system_network_status.MNC",
	"system_network_status.network",
	"system_network_status.current_band",
	"system_network_status.current_band0",
	"system_network_status.current_band1",
	"radio.information.signal_strength",
	"radio.information.signal_strength_rsrq",
	"sim.cmd.param.verify_left",
	"sim.cmd.param.unblock_left",
	"imsi.msin",
	"system_network_status.simICCID",
	"imsi.plmn_mcc",
	"imsi.plmn_mnc",
	"imsi.msin",
	"system_network_status.hint",
	"system_network_status.hint.encoded",
	"module_band_list,"
	"currentband.current_band",
	"heart_beat",
	"link_profile_ready",
};

// Hash resources - MCC MNC
////////////////////////////////////////////////////////////////////////////////

static const struct dbhash_element_t mccmnc_res[] = {
	{ "001 01", 1 },
	{ "200 053", 2 },
	{ "200 054", 3 },
	{ "200 066", 4 },
	{ "202 01", 5 },
	{ "202 05", 6 },
	{ "202 09", 7 },
	{ "202 10", 8 },
	{ "204 01", 9 },
	{ "204 02", 10 },
	{ "204 04", 11 },
	{ "204 05", 12 },
	{ "204 06", 13 },
	{ "204 07", 14 },
	{ "204 08", 15 },
	{ "204 09", 16 },
	{ "204 10", 17 },
	{ "204 12", 18 },
	{ "204 14", 19 },
	{ "204 16", 20 },
	{ "204 20", 21 },
	{ "204 21", 22 },
	{ "204 23", 23 },
	{ "204 69", 24 },
	{ "206 01", 25 },
	{ "206 05", 26 },
	{ "206 10", 27 },
	{ "206 20", 28 },
	{ "208 00", 29 },
	{ "208 01", 30 },
	{ "208 02", 31 },
	{ "208 05", 32 },
	{ "208 06", 33 },
	{ "208 07", 34 },
	{ "208 10", 35 },
	{ "208 11", 36 },
	{ "208 13", 37 },
	{ "208 15", 38 },
	{ "208 20", 39 },
	{ "208 21", 40 },
	{ "208 22", 41 },
	{ "208 88", 42 },
	{ "212 01", 43 },
	{ "213 03", 44 },
	{ "214 01", 45 },
	{ "214 03", 46 },
	{ "214 04", 47 },
	{ "214 05", 48 },
	{ "214 06", 49 },
	{ "214 07", 50 },
	{ "214 08", 51 },
	{ "214 09", 52 },
	{ "214 15", 53 },
	{ "214 16", 54 },
	{ "214 17", 55 },
	{ "214 18", 56 },
	{ "214 19", 57 },
	{ "214 20", 58 },
	{ "214 21", 59 },
	{ "214 22", 60 },
	{ "214 23", 61 },
	{ "214 24", 62 },
	{ "214 25", 63 },
	{ "216 01", 64 },
	{ "216 30", 65 },
	{ "216 70", 66 },
	{ "218 03", 67 },
	{ "218 05", 68 },
	{ "218 90", 69 },
	{ "219 01", 70 },
	{ "219 02", 71 },
	{ "219 10", 72 },
	{ "220 01", 73 },
	{ "220 02", 74 },
	{ "220 03", 75 },
	{ "220 05", 76 },
	{ "222 01", 77 },
	{ "222 02", 78 },
	{ "222 10", 79 },
	{ "222 30", 80 },
	{ "222 77", 81 },
	{ "222 88", 82 },
	{ "222 98", 83 },
	{ "222 99", 84 },
	{ "225 ?", 85 },
	{ "226 01", 86 },
	{ "226 02", 87 },
	{ "226 03", 88 },
	{ "226 04", 89 },
	{ "226 05", 90 },
	{ "226 06", 91 },
	{ "226 10", 92 },
	{ "228 01", 93 },
	{ "228 02", 94 },
	{ "228 03", 95 },
	{ "228 05", 96 },
	{ "228 06", 97 },
	{ "228 07", 98 },
	{ "228 08", 99 },
	{ "228 50", 100 },
	{ "228 51", 101 },
	{ "230 01", 102 },
	{ "230 02", 103 },
	{ "230 03", 104 },
	{ "230 04", 105 },
	{ "230 98", 106 },
	{ "230 99", 107 },
	{ "231 01", 108 },
	{ "231 02", 109 },
	{ "231 03", 110 },
	{ "231 04", 111 },
	{ "231 05", 112 },
	{ "231 06", 113 },
	{ "231 99", 114 },
	{ "232 01", 115 },
	{ "232 03", 116 },
	{ "232 05", 117 },
	{ "232 07", 118 },
	{ "232 09", 119 },
	{ "232 10", 120 },
	{ "232 11", 121 },
	{ "232 12", 122 },
	{ "232 14", 123 },
	{ "232 15", 124 },
	{ "232 91", 125 },
	{ "234 ?", 126 },
	{ "234 00", 127 },
	{ "234 01", 128 },
	{ "234 02", 129 },
	{ "234 03", 130 },
	{ "234 04", 131 },
	{ "234 07", 132 },
	{ "234 08", 133 },
	{ "234 09", 134 },
	{ "234 10", 135 },
	{ "234 11", 136 },
	{ "234 12", 137 },
	{ "234 14", 138 },
	{ "234 15", 139 },
	{ "234 16", 140 },
	{ "234 18", 141 },
	{ "234 19", 142 },
	{ "234 20", 143 },
	{ "234 22", 144 },
	{ "234 25", 145 },
	{ "234 30", 146 },
	{ "234 31", 147 },
	{ "234 32", 148 },
	{ "234 33", 149 },
	{ "234 34", 150 },
	{ "234 50", 151 },
	{ "234 55", 152 },
	{ "234 58", 153 },
	{ "234 75", 154 },
	{ "234 77", 155 },
	{ "234 78", 156 },
	{ "238 01", 157 },
	{ "238 02", 158 },
	{ "238 03", 159 },
	{ "238 05", 160 },
	{ "238 06", 161 },
	{ "238 07", 162 },
	{ "238 09", 163 },
	{ "238 10", 164 },
	{ "238 11", 165 },
	{ "238 12", 166 },
	{ "238 20", 167 },
	{ "238 30", 168 },
	{ "238 40", 169 },
	{ "238 77", 170 },
	{ "240 01", 171 },
	{ "240 02", 172 },
	{ "240 03", 173 },
	{ "240 04", 174 },
	{ "240 05", 175 },
	{ "240 06", 176 },
	{ "240 07", 177 },
	{ "240 08", 178 },
	{ "240 09", 179 },
	{ "240 10", 180 },
	{ "240 11", 181 },
	{ "240 12", 182 },
	{ "240 13", 183 },
	{ "240 14", 184 },
	{ "240 15", 185 },
	{ "240 16", 186 },
	{ "240 17", 187 },
	{ "240 20", 188 },
	{ "240 21", 189 },
	{ "240 25", 190 },
	{ "240 26", 191 },
	{ "242 01", 192 },
	{ "242 02", 193 },
	{ "242 03", 194 },
	{ "242 04", 195 },
	{ "242 05", 196 },
	{ "242 06", 197 },
	{ "242 07", 198 },
	{ "242 08", 199 },
	{ "242 09", 200 },
	{ "242 11", 201 },
	{ "242 20", 202 },
	{ "242 23", 203 },
	{ "244 03", 204 },
	{ "244 05", 205 },
	{ "244 07", 206 },
	{ "244 08", 207 },
	{ "244 10", 208 },
	{ "244 11", 209 },
	{ "244 12", 210 },
	{ "244 14", 211 },
	{ "244 15", 212 },
	{ "244 21", 213 },
	{ "244 29", 214 },
	{ "244 91", 215 },
	{ "246 01", 216 },
	{ "246 02", 217 },
	{ "246 03", 218 },
	{ "246 06", 219 },
	{ "247 01", 220 },
	{ "247 02", 221 },
	{ "247 03", 222 },
	{ "247 05", 223 },
	{ "247 06", 224 },
	{ "247 07", 225 },
	{ "247 08", 226 },
	{ "247 09", 227 },
	{ "248 01", 228 },
	{ "248 02", 229 },
	{ "248 03", 230 },
	{ "248 04", 231 },
	{ "248 05", 232 },
	{ "248 06", 233 },
	{ "250 ?", 234 },
	{ "250 01", 235 },
	{ "250 02", 236 },
	{ "250 03", 237 },
	{ "250 04", 238 },
	{ "250 05", 239 },
	{ "250 06", 240 },
	{ "250 07", 241 },
	{ "250 09", 242 },
	{ "250 10", 243 },
	{ "250 11", 244 },
	{ "250 12", 245 },
	{ "250 13", 246 },
	{ "250 15", 247 },
	{ "250 16", 248 },
	{ "250 17", 249 },
	{ "250 19", 250 },
	{ "250 20", 251 },
	{ "250 23", 252 },
	{ "250 28", 253 },
	{ "250 30", 254 },
	{ "250 35", 255 },
	{ "250 38", 256 },
	{ "250 39", 257 },
	{ "250 44", 258 },
	{ "250 92", 259 },
	{ "250 93", 260 },
	{ "250 99", 261 },
	{ "255 01", 262 },
	{ "255 02", 263 },
	{ "255 03", 264 },
	{ "255 04", 265 },
	{ "255 05", 266 },
	{ "255 06", 267 },
	{ "255 07", 268 },
	{ "255 21", 269 },
	{ "255 23", 270 },
	{ "257 01", 271 },
	{ "257 02", 272 },
	{ "257 03", 273 },
	{ "257 04", 274 },
	{ "259 01", 275 },
	{ "259 02", 276 },
	{ "259 03", 277 },
	{ "259 04", 278 },
	{ "259 05", 279 },
	{ "260 01", 280 },
	{ "260 02", 281 },
	{ "260 03", 282 },
	{ "260 04", 283 },
	{ "260 05", 284 },
	{ "260 06", 285 },
	{ "260 07", 286 },
	{ "260 08", 287 },
	{ "260 09", 288 },
	{ "260 10", 289 },
	{ "260 11", 290 },
	{ "260 12", 291 },
	{ "260 15", 292 },
	{ "260 16", 293 },
	{ "260 17", 294 },
	{ "262 01", 295 },
	{ "262 02", 296 },
	{ "262 03", 297 },
	{ "262 04", 298 },
	{ "262 05", 299 },
	{ "262 06", 300 },
	{ "262 07", 301 },
	{ "262 08", 302 },
	{ "262 09", 303 },
	{ "262 10", 304 },
	{ "262 11", 305 },
	{ "262 12", 306 },
	{ "262 13", 307 },
	{ "262 14", 308 },
	{ "262 15", 309 },
	{ "262 16", 310 },
	{ "262 42", 311 },
	{ "262 43", 312 },
	{ "262 60", 313 },
	{ "262 76", 314 },
	{ "262 77", 315 },
	{ "262 901", 316 },
	{ "262 92", 317 },
	{ "266 01", 318 },
	{ "266 06", 319 },
	{ "268 01", 320 },
	{ "268 03", 321 },
	{ "268 06", 322 },
	{ "268 21", 323 },
	{ "270 01", 324 },
	{ "270 77", 325 },
	{ "270 99", 326 },
	{ "272 01", 327 },
	{ "272 02", 328 },
	{ "272 03", 329 },
	{ "272 04", 330 },
	{ "272 05", 331 },
	{ "272 07", 332 },
	{ "272 09", 333 },
	{ "272 11", 334 },
	{ "274 01", 335 },
	{ "274 02", 336 },
	{ "274 03", 337 },
	{ "274 04", 338 },
	{ "274 06", 339 },
	{ "274 07", 340 },
	{ "274 08", 341 },
	{ "274 11", 342 },
	{ "274 12", 343 },
	{ "276 01", 344 },
	{ "276 02", 345 },
	{ "276 03", 346 },
	{ "276 04", 347 },
	{ "278 01", 348 },
	{ "278 21", 349 },
	{ "278 77", 350 },
	{ "280 01", 351 },
	{ "280 10", 352 },
	{ "282 01", 353 },
	{ "282 02", 354 },
	{ "282 03", 355 },
	{ "282 04", 356 },
	{ "282 05", 357 },
	{ "283 01", 358 },
	{ "283 05", 359 },
	{ "283 10", 360 },
	{ "284 01", 361 },
	{ "284 03", 362 },
	{ "284 04", 363 },
	{ "284 05", 364 },
	{ "286 01", 365 },
	{ "286 02", 366 },
	{ "286 03", 367 },
	{ "286 04", 368 },
	{ "288 01", 369 },
	{ "288 02", 370 },
	{ "289 67", 371 },
	{ "289 88", 372 },
	{ "290 01", 373 },
	{ "292 01", 374 },
	{ "293 40", 375 },
	{ "293 41", 376 },
	{ "293 64", 377 },
	{ "293 70", 378 },
	{ "294 01", 379 },
	{ "294 02", 380 },
	{ "294 03", 381 },
	{ "295 01", 382 },
	{ "295 02", 383 },
	{ "295 05", 384 },
	{ "295 77", 385 },
	{ "297 01", 386 },
	{ "297 02", 387 },
	{ "297 03", 388 },
	{ "297 04", 389 },
	{ "302 220", 390 },
	{ "302 221", 391 },
	{ "302 290", 392 },
	{ "302 320", 393 },
	{ "302 350", 394 },
	{ "302 360", 395 },
	{ "302 361", 396 },
	{ "302 370", 397 },
	{ "302 380", 398 },
	{ "302 490", 399 },
	{ "302 500", 400 },
	{ "302 510", 401 },
	{ "302 610", 402 },
	{ "302 620", 403 },
	{ "302 640", 404 },
	{ "302 651", 405 },
	{ "302 652", 406 },
	{ "302 653", 407 },
	{ "302 655", 408 },
	{ "302 656", 409 },
	{ "302 657", 410 },
	{ "302 660", 411 },
	{ "302 680", 412 },
	{ "302 701", 413 },
	{ "302 702", 414 },
	{ "302 703", 415 },
	{ "302 710", 416 },
	{ "302 720", 417 },
	{ "302 780", 418 },
	{ "302 880", 419 },
	{ "308 01", 420 },
	{ "310 000", 421 },
	{ "310 004", 422 },
	{ "310 005", 423 },
	{ "310 010", 424 },
	{ "310 012", 425 },
	{ "310 013", 426 },
	{ "310 014", 427 },
	{ "310 016", 428 },
	{ "310 017", 429 },
	{ "310 020", 430 },
	{ "310 026", 431 },
	{ "310 030", 432 },
	{ "310 032", 433 },
	{ "310 033", 434 },
	{ "310 034", 435 },
	{ "310 040", 436 },
	{ "310 046", 437 },
	{ "310 060", 438 },
	{ "310 070", 439 },
	{ "310 080", 440 },
	{ "310 090", 441 },
	{ "310 100", 442 },
	{ "310 110", 443 },
	{ "310 120", 444 },
	{ "310 140", 445 },
	{ "310 150", 446 },
	{ "310 160", 447 },
	{ "310 170", 448 },
	{ "310 180", 449 },
	{ "310 190", 450 },
	{ "310 200", 451 },
	{ "310 210", 452 },
	{ "310 220", 453 },
	{ "310 230", 454 },
	{ "310 240", 455 },
	{ "310 250", 456 },
	{ "310 260", 457 },
	{ "310 270", 458 },
	{ "310 280", 459 },
	{ "310 290", 460 },
	{ "310 300", 461 },
	{ "310 310", 462 },
	{ "310 311", 463 },
	{ "310 320", 464 },
	{ "310 330", 465 },
	{ "310 340", 466 },
	{ "310 350", 467 },
	{ "310 370", 468 },
	{ "310 380", 469 },
	{ "310 390", 470 },
	{ "310 400", 471 },
	{ "310 410", 472 },
	{ "310 420", 473 },
	{ "310 430", 474 },
	{ "310 440", 475 },
	{ "310 450", 476 },
	{ "310 460", 477 },
	{ "310 470", 478 },
	{ "310 480", 479 },
	{ "310 490", 480 },
	{ "310 500", 481 },
	{ "310 510", 482 },
	{ "310 520", 483 },
	{ "310 530", 484 },
	{ "310 540", 485 },
	{ "310 560", 486 },
	{ "310 570", 487 },
	{ "310 580", 488 },
	{ "310 59", 489 },
	{ "310 590", 490 },
	{ "310 610", 491 },
	{ "310 620", 492 },
	{ "310 630", 493 },
	{ "310 640", 494 },
	{ "310 650", 495 },
	{ "310 660", 496 },
	{ "310 670", 497 },
	{ "310 680", 498 },
	{ "310 690", 499 },
	{ "310 730", 500 },
	{ "310 740", 501 },
	{ "310 760", 502 },
	{ "310 770", 503 },
	{ "310 780", 504 },
	{ "310 790", 505 },
	{ "310 800", 506 },
	{ "310 830", 507 },
	{ "310 840", 508 },
	{ "310 850", 509 },
	{ "310 870", 510 },
	{ "310 880", 511 },
	{ "310 890", 512 },
	{ "310 900", 513 },
	{ "310 910", 514 },
	{ "310 940", 515 },
	{ "310 950", 516 },
	{ "310 960", 517 },
	{ "310 970", 518 },
	{ "310 980", 519 },
	{ "310 990", 520 },
	{ "311 ?", 521 },
	{ "311 000", 522 },
	{ "311 010", 523 },
	{ "311 020", 524 },
	{ "311 030", 525 },
	{ "311 040", 526 },
	{ "311 050", 527 },
	{ "311 060", 528 },
	{ "311 070", 529 },
	{ "311 080", 530 },
	{ "311 090", 531 },
	{ "311 100", 532 },
	{ "311 110", 533 },
	{ "311 120", 534 },
	{ "311 130", 535 },
	{ "311 140", 536 },
	{ "311 150", 537 },
	{ "311 160", 538 },
	{ "311 170", 539 },
	{ "311 180", 540 },
	{ "311 190", 541 },
	{ "311 210", 542 },
	{ "311 250", 543 },
	{ "311 330", 544 },
	{ "311 480", 545 },
	{ "316 010", 546 },
	{ "316 011", 547 },
	{ "330 110", 548 },
	{ "334 01", 549 },
	{ "334 02", 550 },
	{ "334 03", 551 },
	{ "334 04", 552 },
	{ "334 050", 553 },
	{ "338 020", 554 },
	{ "338 05", 555 },
	{ "338 050", 556 },
	{ "338 070", 557 },
	{ "338 180", 558 },
	{ "340 01", 559 },
	{ "340 02", 560 },
	{ "340 03", 561 },
	{ "340 08", 562 },
	{ "340 20", 563 },
	{ "342 600", 564 },
	{ "342 750", 565 },
	{ "342 820", 566 },
	{ "344 030", 567 },
	{ "344 920", 568 },
	{ "346 140", 569 },
	{ "348 170", 570 },
	{ "348 570", 571 },
	{ "348 770", 572 },
	{ "350 01", 573 },
	{ "350 02", 574 },
	{ "352 030", 575 },
	{ "352 110", 576 },
	{ "356 ?", 577 },
	{ "356 050", 578 },
	{ "356 110", 579 },
	{ "358 050", 580 },
	{ "358 110", 581 },
	{ "360 070", 582 },
	{ "360 100", 583 },
	{ "360 110", 584 },
	{ "362 ?", 585 },
	{ "362 51", 586 },
	{ "362 69", 587 },
	{ "362 91", 588 },
	{ "362 94", 589 },
	{ "362 95", 590 },
	{ "363 01", 591 },
	{ "363 02", 592 },
	{ "364 390", 593 },
	{ "365 010", 594 },
	{ "365 840", 595 },
	{ "366 020", 596 },
	{ "366 110", 597 },
	{ "368 01", 598 },
	{ "370 01", 599 },
	{ "370 02", 600 },
	{ "370 03", 601 },
	{ "370 04", 602 },
	{ "372 01", 603 },
	{ "372 02", 604 },
	{ "374 12", 605 },
	{ "374 130", 606 },
	{ "376 350", 607 },
	{ "376 352", 608 },
	{ "? 40", 609 },
	{ "400 01", 610 },
	{ "400 02", 611 },
	{ "400 03", 612 },
	{ "400 04", 613 },
	{ "401 01", 614 },
	{ "401 02", 615 },
	{ "401 07", 616 },
	{ "401 08", 617 },
	{ "401 77", 618 },
	{ "402 11", 619 },
	{ "402 77", 620 },
	{ "404 01", 621 },
	{ "404 02", 622 },
	{ "404 03", 623 },
	{ "404 04", 624 },
	{ "404 05", 625 },
	{ "404 07", 626 },
	{ "404 09", 627 },
	{ "404 10", 628 },
	{ "404 11", 629 },
	{ "404 12", 630 },
	{ "404 13", 631 },
	{ "404 14", 632 },
	{ "404 15", 633 },
	{ "404 17", 634 },
	{ "404 19", 635 },
	{ "404 20", 636 },
	{ "404 21", 637 },
	{ "404 22", 638 },
	{ "404 24", 639 },
	{ "404 25", 640 },
	{ "404 27", 641 },
	{ "404 28", 642 },
	{ "404 29", 643 },
	{ "404 30", 644 },
	{ "404 31", 645 },
	{ "404 34", 646 },
	{ "404 36", 647 },
	{ "404 37", 648 },
	{ "404 38", 649 },
	{ "404 41", 650 },
	{ "404 42", 651 },
	{ "404 44", 652 },
	{ "404 45", 653 },
	{ "404 48", 654 },
	{ "404 49", 655 },
	{ "404 51", 656 },
	{ "404 52", 657 },
	{ "404 53", 658 },
	{ "404 54", 659 },
	{ "404 55", 660 },
	{ "404 56", 661 },
	{ "404 57", 662 },
	{ "404 58", 663 },
	{ "404 59", 664 },
	{ "404 60", 665 },
	{ "404 62", 666 },
	{ "404 64", 667 },
	{ "404 66", 668 },
	{ "404 67", 669 },
	{ "404 68", 670 },
	{ "404 69", 671 },
	{ "404 72", 672 },
	{ "404 74", 673 },
	{ "404 76", 674 },
	{ "404 78", 675 },
	{ "404 80", 676 },
	{ "404 81", 677 },
	{ "404 82", 678 },
	{ "404 83", 679 },
	{ "404 84", 680 },
	{ "404 85", 681 },
	{ "404 86", 682 },
	{ "404 87", 683 },
	{ "404 88", 684 },
	{ "404 89", 685 },
	{ "404 90", 686 },
	{ "404 91", 687 },
	{ "404 92", 688 },
	{ "404 927", 689 },
	{ "404 93", 690 },
	{ "404 96", 691 },
	{ "405 01", 692 },
	{ "405 025", 693 },
	{ "405 026", 694 },
	{ "405 027", 695 },
	{ "405 029", 696 },
	{ "405 03", 697 },
	{ "405 030", 698 },
	{ "405 031", 699 },
	{ "405 032", 700 },
	{ "405 033", 701 },
	{ "405 034", 702 },
	{ "405 035", 703 },
	{ "405 036", 704 },
	{ "405 037", 705 },
	{ "405 038", 706 },
	{ "405 039", 707 },
	{ "405 04", 708 },
	{ "405 041", 709 },
	{ "405 042", 710 },
	{ "405 043", 711 },
	{ "405 044", 712 },
	{ "405 045", 713 },
	{ "405 046", 714 },
	{ "405 047", 715 },
	{ "405 05", 716 },
	{ "405 10", 717 },
	{ "405 13", 718 },
	{ "405 51", 719 },
	{ "405 52", 720 },
	{ "405 54", 721 },
	{ "405 56", 722 },
	{ "405 66", 723 },
	{ "405 70", 724 },
	{ "405 750", 725 },
	{ "405 751", 726 },
	{ "405 752", 727 },
	{ "405 753", 728 },
	{ "405 754", 729 },
	{ "405 755", 730 },
	{ "405 756", 731 },
	{ "405 799", 732 },
	{ "405 800", 733 },
	{ "405 801", 734 },
	{ "405 802", 735 },
	{ "405 803", 736 },
	{ "405 804", 737 },
	{ "405 805", 738 },
	{ "405 806", 739 },
	{ "405 807", 740 },
	{ "405 808", 741 },
	{ "405 809", 742 },
	{ "405 810", 743 },
	{ "405 811", 744 },
	{ "405 812", 745 },
	{ "405 818", 746 },
	{ "405 819", 747 },
	{ "405 820", 748 },
	{ "405 821", 749 },
	{ "405 822", 750 },
	{ "405 824", 751 },
	{ "405 827", 752 },
	{ "405 834", 753 },
	{ "405 844", 754 },
	{ "405 845", 755 },
	{ "405 848", 756 },
	{ "405 850", 757 },
	{ "405 855", 758 },
	{ "405 864", 759 },
	{ "405 865", 760 },
	{ "405 875", 761 },
	{ "405 880", 762 },
	{ "405 881", 763 },
	{ "405 912", 764 },
	{ "405 913", 765 },
	{ "405 914", 766 },
	{ "405 917", 767 },
	{ "405 929", 768 },
	{ "410 01", 769 },
	{ "410 03", 770 },
	{ "410 04", 771 },
	{ "410 06", 772 },
	{ "410 07", 773 },
	{ "412 01", 774 },
	{ "412 20", 775 },
	{ "412 40", 776 },
	{ "412 50", 777 },
	{ "413 01", 778 },
	{ "413 02", 779 },
	{ "413 03", 780 },
	{ "413 05", 781 },
	{ "413 08", 782 },
	{ "414 01", 783 },
	{ "415 01", 784 },
	{ "415 03", 785 },
	{ "415 05", 786 },
	{ "416 01", 787 },
	{ "416 02", 788 },
	{ "416 03", 789 },
	{ "416 77", 790 },
	{ "417 01", 791 },
	{ "417 02", 792 },
	{ "418 ?", 793 },
	{ "418 05", 794 },
	{ "418 08", 795 },
	{ "418 20", 796 },
	{ "418 30", 797 },
	{ "418 40", 798 },
	{ "419 02", 799 },
	{ "419 03", 800 },
	{ "419 04", 801 },
	{ "420 01", 802 },
	{ "420 03", 803 },
	{ "420 04", 804 },
	{ "421 01", 805 },
	{ "421 02", 806 },
	{ "421 03", 807 },
	{ "421 04", 808 },
	{ "422 02", 809 },
	{ "422 03", 810 },
	{ "424 02", 811 },
	{ "424 03", 812 },
	{ "425 01", 813 },
	{ "425 02", 814 },
	{ "425 03", 815 },
	{ "425 05", 816 },
	{ "425 06", 817 },
	{ "425 77", 818 },
	{ "426 01", 819 },
	{ "426 02", 820 },
	{ "426 04", 821 },
	{ "427 01", 822 },
	{ "427 02", 823 },
	{ "428 88", 824 },
	{ "428 91", 825 },
	{ "428 98", 826 },
	{ "428 99", 827 },
	{ "429 01", 828 },
	{ "429 02", 829 },
	{ "429 03", 830 },
	{ "429 04", 831 },
	{ "432 11", 832 },
	{ "432 14", 833 },
	{ "432 19", 834 },
	{ "432 32", 835 },
	{ "432 35", 836 },
	{ "432 70", 837 },
	{ "434 01", 838 },
	{ "434 02", 839 },
	{ "434 04", 840 },
	{ "434 05", 841 },
	{ "434 06", 842 },
	{ "434 07", 843 },
	{ "436 01", 844 },
	{ "436 02", 845 },
	{ "436 03", 846 },
	{ "436 04", 847 },
	{ "436 05", 848 },
	{ "436 12", 849 },
	{ "437 01", 850 },
	{ "437 03", 851 },
	{ "437 05", 852 },
	{ "437 09", 853 },
	{ "438 01", 854 },
	{ "438 02", 855 },
	{ "440 00", 856 },
	{ "440 01", 857 },
	{ "440 02", 858 },
	{ "440 03", 859 },
	{ "440 04", 860 },
	{ "440 06", 861 },
	{ "440 07", 862 },
	{ "440 08", 863 },
	{ "440 09", 864 },
	{ "440 10", 865 },
	{ "440 11", 866 },
	{ "440 12", 867 },
	{ "440 13", 868 },
	{ "440 14", 869 },
	{ "440 15", 870 },
	{ "440 16", 871 },
	{ "440 17", 872 },
	{ "440 18", 873 },
	{ "440 19", 874 },
	{ "440 20", 875 },
	{ "440 21", 876 },
	{ "440 22", 877 },
	{ "440 23", 878 },
	{ "440 24", 879 },
	{ "440 25", 880 },
	{ "440 26", 881 },
	{ "440 27", 882 },
	{ "440 28", 883 },
	{ "440 29", 884 },
	{ "440 30", 885 },
	{ "440 31", 886 },
	{ "440 32", 887 },
	{ "440 33", 888 },
	{ "440 34", 889 },
	{ "440 35", 890 },
	{ "440 36", 891 },
	{ "440 37", 892 },
	{ "440 38", 893 },
	{ "440 39", 894 },
	{ "440 40", 895 },
	{ "440 41", 896 },
	{ "440 42", 897 },
	{ "440 43", 898 },
	{ "440 44", 899 },
	{ "440 45", 900 },
	{ "440 46", 901 },
	{ "440 47", 902 },
	{ "440 48", 903 },
	{ "440 49", 904 },
	{ "440 50", 905 },
	{ "440 51", 906 },
	{ "440 52", 907 },
	{ "440 53", 908 },
	{ "440 54", 909 },
	{ "440 55", 910 },
	{ "440 56", 911 },
	{ "440 58", 912 },
	{ "440 60", 913 },
	{ "440 61", 914 },
	{ "440 62", 915 },
	{ "440 63", 916 },
	{ "440 64", 917 },
	{ "440 65", 918 },
	{ "440 66", 919 },
	{ "440 67", 920 },
	{ "440 68", 921 },
	{ "440 69", 922 },
	{ "440 70", 923 },
	{ "440 71", 924 },
	{ "440 72", 925 },
	{ "440 73", 926 },
	{ "440 74", 927 },
	{ "440 75", 928 },
	{ "440 76", 929 },
	{ "440 77", 930 },
	{ "440 78", 931 },
	{ "440 79", 932 },
	{ "440 80", 933 },
	{ "440 81", 934 },
	{ "440 82", 935 },
	{ "440 83", 936 },
	{ "440 84", 937 },
	{ "440 85", 938 },
	{ "440 86", 939 },
	{ "440 87", 940 },
	{ "440 88", 941 },
	{ "440 89", 942 },
	{ "440 90", 943 },
	{ "440 92", 944 },
	{ "440 93", 945 },
	{ "440 94", 946 },
	{ "440 95", 947 },
	{ "440 96", 948 },
	{ "440 97", 949 },
	{ "440 98", 950 },
	{ "440 99", 951 },
	{ "450 02", 952 },
	{ "450 03", 953 },
	{ "450 04", 954 },
	{ "450 05", 955 },
	{ "450 06", 956 },
	{ "450 08", 957 },
	{ "452 01", 958 },
	{ "452 02", 959 },
	{ "452 03", 960 },
	{ "452 04", 961 },
	{ "452 05", 962 },
	{ "452 06", 963 },
	{ "452 07", 964 },
	{ "452 08", 965 },
	{ "454 00", 966 },
	{ "454 01", 967 },
	{ "454 02", 968 },
	{ "454 03", 969 },
	{ "454 04", 970 },
	{ "454 05", 971 },
	{ "454 06", 972 },
	{ "454 07", 973 },
	{ "454 08", 974 },
	{ "454 09", 975 },
	{ "454 10", 976 },
	{ "454 11", 977 },
	{ "454 12", 978 },
	{ "454 14", 979 },
	{ "454 15", 980 },
	{ "454 16", 981 },
	{ "454 17", 982 },
	{ "454 18", 983 },
	{ "454 19", 984 },
	{ "454 29", 985 },
	{ "455 00", 986 },
	{ "455 01", 987 },
	{ "455 02", 988 },
	{ "455 03", 989 },
	{ "455 04", 990 },
	{ "455 05", 991 },
	{ "456 01", 992 },
	{ "456 02", 993 },
	{ "456 03", 994 },
	{ "456 04", 995 },
	{ "456 05", 996 },
	{ "456 06", 997 },
	{ "456 08", 998 },
	{ "456 09", 999 },
	{ "456 11", 1000 },
	{ "456 18", 1001 },
	{ "457 01", 1002 },
	{ "457 02", 1003 },
	{ "457 03", 1004 },
	{ "457 08", 1005 },
	{ "460 00", 1006 },
	{ "460 01", 1007 },
	{ "460 02", 1008 },
	{ "460 03", 1009 },
	{ "460 05", 1010 },
	{ "460 06", 1011 },
	{ "460 07", 1012 },
	{ "460 20", 1013 },
	{ "466 01", 1014 },
	{ "466 05", 1015 },
	{ "466 06", 1016 },
	{ "466 11", 1017 },
	{ "466 88", 1018 },
	{ "466 89", 1019 },
	{ "466 92", 1020 },
	{ "466 93", 1021 },
	{ "466 97", 1022 },
	{ "466 99", 1023 },
	{ "467 192", 1024 },
	{ "467 193", 1025 },
	{ "470 01", 1026 },
	{ "470 02", 1027 },
	{ "470 03", 1028 },
	{ "470 04", 1029 },
	{ "470 05", 1030 },
	{ "470 06", 1031 },
	{ "470 07", 1032 },
	{ "472 01", 1033 },
	{ "472 02", 1034 },
	{ "502 01", 1035 },
	{ "502 10", 1036 },
	{ "502 11", 1037 },
	{ "502 12", 1038 },
	{ "502 13", 1039 },
	{ "502 14", 1040 },
	{ "502 150", 1041 },
	{ "502 151", 1042 },
	{ "502 152", 1043 },
	{ "502 16", 1044 },
	{ "502 17", 1045 },
	{ "502 18", 1046 },
	{ "502 19", 1047 },
	{ "502 20", 1048 },
	{ "505 01", 1049 },
	{ "505 02", 1050 },
	{ "505 03", 1051 },
	{ "505 04", 1052 },
	{ "505 05", 1053 },
	{ "505 06", 1054 },
	{ "505 08", 1055 },
	{ "505 09", 1056 },
	{ "505 12", 1057 },
	{ "505 13", 1058 },
	{ "505 14", 1059 },
	{ "505 15", 1060 },
	{ "505 16", 1061 },
	{ "505 21", 1062 },
	{ "505 24", 1063 },
	{ "505 38", 1064 },
	{ "505 71", 1065 },
	{ "505 72", 1066 },
	{ "505 88", 1067 },
	{ "505 90", 1068 },
	{ "505 99", 1069 },
	{ "510 00", 1070 },
	{ "510 01", 1071 },
	{ "510 03", 1072 },
	{ "510 07", 1073 },
	{ "510 08", 1074 },
	{ "510 09", 1075 },
	{ "510 10", 1076 },
	{ "510 11", 1077 },
	{ "510 20", 1078 },
	{ "510 21", 1079 },
	{ "510 27", 1080 },
	{ "510 28", 1081 },
	{ "510 89", 1082 },
	{ "510 99", 1083 },
	{ "514 02", 1084 },
	{ "515 01", 1085 },
	{ "515 02", 1086 },
	{ "515 03", 1087 },
	{ "515 05", 1088 },
	{ "515 11", 1089 },
	{ "515 18", 1090 },
	{ "515 88", 1091 },
	{ "520 ?", 1092 },
	{ "520 00", 1093 },
	{ "520 01", 1094 },
	{ "520 02", 1095 },
	{ "520 10", 1096 },
	{ "520 15", 1097 },
	{ "520 18", 1098 },
	{ "520 23", 1099 },
	{ "520 88", 1100 },
	{ "520 99", 1101 },
	{ "525 01", 1102 },
	{ "525 02", 1103 },
	{ "525 03", 1104 },
	{ "525 05", 1105 },
	{ "525 12", 1106 },
	{ "528 01", 1107 },
	{ "528 02", 1108 },
	{ "528 11", 1109 },
	{ "530 00", 1110 },
	{ "530 01", 1111 },
	{ "530 02", 1112 },
	{ "530 03", 1113 },
	{ "530 04", 1114 },
	{ "530 05", 1115 },
	{ "530 24", 1116 },
	{ "536 02", 1117 },
	{ "537 01", 1118 },
	{ "537 03", 1119 },
	{ "539 01", 1120 },
	{ "539 43", 1121 },
	{ "539 88", 1122 },
	{ "540 ?", 1123 },
	{ "541 01", 1124 },
	{ "542 01", 1125 },
	{ "542 02", 1126 },
	{ "545 09", 1127 },
	{ "546 01", 1128 },
	{ "547 20", 1129 },
	{ "548 01", 1130 },
	{ "549 01", 1131 },
	{ "549 27", 1132 },
	{ "552 01", 1133 },
	{ "552 80", 1134 },
	{ "553 01", 1135 },
	{ "555 01", 1136 },
	{ "602 01", 1137 },
	{ "602 02", 1138 },
	{ "602 03", 1139 },
	{ "603 01", 1140 },
	{ "603 02", 1141 },
	{ "603 03", 1142 },
	{ "604 00", 1143 },
	{ "604 01", 1144 },
	{ "604 02", 1145 },
	{ "605 01", 1146 },
	{ "605 02", 1147 },
	{ "605 03", 1148 },
	{ "606 00", 1149 },
	{ "606 01", 1150 },
	{ "606 02", 1151 },
	{ "606 03", 1152 },
	{ "606 06", 1153 },
	{ "607 01", 1154 },
	{ "607 02", 1155 },
	{ "607 03", 1156 },
	{ "608 01", 1157 },
	{ "608 02", 1158 },
	{ "608 03", 1159 },
	{ "609 01", 1160 },
	{ "609 02", 1161 },
	{ "609 10", 1162 },
	{ "610 01", 1163 },
	{ "610 02", 1164 },
	{ "611 01", 1165 },
	{ "611 02", 1166 },
	{ "611 03", 1167 },
	{ "611 04", 1168 },
	{ "611 05", 1169 },
	{ "612 01", 1170 },
	{ "612 02", 1171 },
	{ "612 03", 1172 },
	{ "612 04", 1173 },
	{ "612 05", 1174 },
	{ "612 06", 1175 },
	{ "613 01", 1176 },
	{ "613 02", 1177 },
	{ "613 03", 1178 },
	{ "614 01", 1179 },
	{ "614 02", 1180 },
	{ "614 03", 1181 },
	{ "614 04", 1182 },
	{ "615 01", 1183 },
	{ "615 03", 1184 },
	{ "616 01", 1185 },
	{ "616 02", 1186 },
	{ "616 03", 1187 },
	{ "616 04", 1188 },
	{ "616 05", 1189 },
	{ "617 01", 1190 },
	{ "617 02", 1191 },
	{ "617 10", 1192 },
	{ "618 01", 1193 },
	{ "618 02", 1194 },
	{ "618 04", 1195 },
	{ "618 07", 1196 },
	{ "618 20", 1197 },
	{ "619 ?", 1198 },
	{ "619 01", 1199 },
	{ "619 02", 1200 },
	{ "619 03", 1201 },
	{ "619 04", 1202 },
	{ "619 05", 1203 },
	{ "619 25", 1204 },
	{ "620 01", 1205 },
	{ "620 02", 1206 },
	{ "620 03", 1207 },
	{ "620 04", 1208 },
	{ "620 06", 1209 },
	{ "621 20", 1210 },
	{ "621 30", 1211 },
	{ "621 40", 1212 },
	{ "621 50", 1213 },
	{ "621 60", 1214 },
	{ "622 01", 1215 },
	{ "622 02", 1216 },
	{ "622 03", 1217 },
	{ "622 04", 1218 },
	{ "623 01", 1219 },
	{ "623 02", 1220 },
	{ "623 03", 1221 },
	{ "623 04", 1222 },
	{ "624 01", 1223 },
	{ "624 02", 1224 },
	{ "625 01", 1225 },
	{ "625 02", 1226 },
	{ "626 01", 1227 },
	{ "627 01", 1228 },
	{ "627 03", 1229 },
	{ "628 01", 1230 },
	{ "628 02", 1231 },
	{ "628 03", 1232 },
	{ "628 04", 1233 },
	{ "629 01", 1234 },
	{ "629 07", 1235 },
	{ "629 10", 1236 },
	{ "630 ?", 1237 },
	{ "630 01", 1238 },
	{ "630 02", 1239 },
	{ "630 04", 1240 },
	{ "630 05", 1241 },
	{ "630 86", 1242 },
	{ "630 89", 1243 },
	{ "631 02", 1244 },
	{ "632 02", 1245 },
	{ "632 03", 1246 },
	{ "633 01", 1247 },
	{ "633 02", 1248 },
	{ "633 10", 1249 },
	{ "634 ?", 1250 },
	{ "634 01", 1251 },
	{ "634 02", 1252 },
	{ "634 05", 1253 },
	{ "634 07", 1254 },
	{ "635 10", 1255 },
	{ "635 12", 1256 },
	{ "635 13", 1257 },
	{ "636 01", 1258 },
	{ "637 01", 1259 },
	{ "637 04", 1260 },
	{ "637 10", 1261 },
	{ "637 25", 1262 },
	{ "637 30", 1263 },
	{ "637 82", 1264 },
	{ "638 01", 1265 },
	{ "639 02", 1266 },
	{ "639 03", 1267 },
	{ "639 05", 1268 },
	{ "639 07", 1269 },
	{ "640 02", 1270 },
	{ "640 03", 1271 },
	{ "640 04", 1272 },
	{ "640 05", 1273 },
	{ "640 06", 1274 },
	{ "640 07", 1275 },
	{ "640 08", 1276 },
	{ "640 09", 1277 },
	{ "640 11", 1278 },
	{ "641 01", 1279 },
	{ "641 10", 1280 },
	{ "641 11", 1281 },
	{ "641 14", 1282 },
	{ "641 22", 1283 },
	{ "642 01", 1284 },
	{ "642 02", 1285 },
	{ "642 03", 1286 },
	{ "642 07", 1287 },
	{ "642 08", 1288 },
	{ "642 82", 1289 },
	{ "643 01", 1290 },
	{ "643 04", 1291 },
	{ "645 01", 1292 },
	{ "645 02", 1293 },
	{ "645 03", 1294 },
	{ "646 01", 1295 },
	{ "646 02", 1296 },
	{ "646 03", 1297 },
	{ "646 04", 1298 },
	{ "647 00", 1299 },
	{ "647 02", 1300 },
	{ "647 10", 1301 },
	{ "648 01", 1302 },
	{ "648 03", 1303 },
	{ "648 04", 1304 },
	{ "649 01", 1305 },
	{ "649 02", 1306 },
	{ "649 03", 1307 },
	{ "650 01", 1308 },
	{ "650 10", 1309 },
	{ "651 01", 1310 },
	{ "651 02", 1311 },
	{ "652 01", 1312 },
	{ "652 02", 1313 },
	{ "652 04", 1314 },
	{ "653 10", 1315 },
	{ "654 01", 1316 },
	{ "655 01", 1317 },
	{ "655 02", 1318 },
	{ "655 06", 1319 },
	{ "655 07", 1320 },
	{ "655 10", 1321 },
	{ "655 11", 1322 },
	{ "655 13", 1323 },
	{ "655 21", 1324 },
	{ "655 30", 1325 },
	{ "655 31", 1326 },
	{ "655 32", 1327 },
	{ "655 33", 1328 },
	{ "657 01", 1329 },
	{ "702 67", 1330 },
	{ "702 99", 1331 },
	{ "704 ?", 1332 },
	{ "704 01", 1333 },
	{ "704 02", 1334 },
	{ "704 03", 1335 },
	{ "706 01", 1336 },
	{ "706 02", 1337 },
	{ "706 03", 1338 },
	{ "706 04", 1339 },
	{ "706 11", 1340 },
	{ "708 01", 1341 },
	{ "708 02", 1342 },
	{ "708 30", 1343 },
	{ "710 21", 1344 },
	{ "710 30", 1345 },
	{ "710 73", 1346 },
	{ "712 01", 1347 },
	{ "712 02", 1348 },
	{ "712 03", 1349 },
	{ "714 01", 1350 },
	{ "714 02", 1351 },
	{ "714 03", 1352 },
	{ "714 04", 1353 },
	{ "716 06", 1354 },
	{ "716 07", 1355 },
	{ "716 10", 1356 },
	{ "722 010", 1357 },
	{ "722 020", 1358 },
	{ "722 070", 1359 },
	{ "722 310", 1360 },
	{ "722 320", 1361 },
	{ "722 330", 1362 },
	{ "722 34", 1363 },
	{ "722 341", 1364 },
	{ "722 350", 1365 },
	{ "722 36", 1366 },
	{ "724 00", 1367 },
	{ "724 02", 1368 },
	{ "724 03", 1369 },
	{ "724 04", 1370 },
	{ "724 05", 1371 },
	{ "724 06", 1372 },
	{ "724 07", 1373 },
	{ "724 08", 1374 },
	{ "724 10", 1375 },
	{ "724 11", 1376 },
	{ "724 15", 1377 },
	{ "724 16", 1378 },
	{ "724 23", 1379 },
	{ "724 24", 1380 },
	{ "724 31", 1381 },
	{ "724 32", 1382 },
	{ "724 33", 1383 },
	{ "724 34", 1384 },
	{ "724 37", 1385 },
	{ "730 01", 1386 },
	{ "730 02", 1387 },
	{ "730 03", 1388 },
	{ "730 04", 1389 },
	{ "730 08", 1390 },
	{ "730 09", 1391 },
	{ "730 10", 1392 },
	{ "730 99", 1393 },
	{ "732 001", 1394 },
	{ "732 002", 1395 },
	{ "732 101", 1396 },
	{ "732 102", 1397 },
	{ "732 103", 1398 },
	{ "732 111", 1399 },
	{ "732 123", 1400 },
	{ "734 01", 1401 },
	{ "734 02", 1402 },
	{ "734 03", 1403 },
	{ "734 04", 1404 },
	{ "734 06", 1405 },
	{ "736 01", 1406 },
	{ "736 02", 1407 },
	{ "736 03", 1408 },
	{ "738 01", 1409 },
	{ "738 02", 1410 },
	{ "740 00", 1411 },
	{ "740 01", 1412 },
	{ "740 02", 1413 },
	{ "744 01", 1414 },
	{ "744 02", 1415 },
	{ "744 04", 1416 },
	{ "744 05", 1417 },
	{ "744 06", 1418 },
	{ "746 02", 1419 },
	{ "746 03", 1420 },
	{ "746 04", 1421 },
	{ "748 00", 1422 },
	{ "748 01", 1423 },
	{ "748 07", 1424 },
	{ "748 10", 1425 },
	{ "901 01", 1426 },
	{ "901 02", 1427 },
	{ "901 03", 1428 },
	{ "901 04", 1429 },
	{ "901 05", 1430 },
	{ "901 06", 1431 },
	{ "901 07", 1432 },
	{ "901 08", 1433 },
	{ "901 09", 1434 },
	{ "901 10", 1435 },
	{ "901 11", 1436 },
	{ "901 12", 1437 },
	{ "901 13", 1438 },
	{ "901 14", 1439 },
	{ "901 15", 1440 },
	{ "901 16", 1441 },
	{ "901 17", 1442 },
	{ "901 18", 1443 },
	{ "901 19", 1444 },
	{ "901 21", 1445 },
	{ "901 23", 1446 },
	{ "901 24", 1447 },
	{ "901 26", 1448 },
	{ "901 29", 1449 },
};


// information for qmi protocol state manchine
////////////////////////////////////////////////////////////////////////////////
static int _gps_stat_agps = 0;
int _simcard_pin_enabled = 0;
int registered_network = 0;

#define _dbstruct_str_member(name)	char name[QMIMGR_MAX_DB_VARIABLE_LENGTH]; int valid_##name
#define _dbstruct_uint_member(name)	unsigned int name; int valid_##name

#define _dbstruct_set_str_member(x,m,valid,val,len) { \
		(x)->valid_##m=(valid)!=0; \
		if(valid) \
			__strncpy((x)->m,val,len); \
		else \
			(x)->m[0]=0; \
	} while(0)

#define _dbstruct_get_str_member(x,m) \
	(((x)->valid_##m)?(x->m):"")

#define _dbstruct_set_int_member(x,m,valid,val) { \
		(x)->valid_##m=(valid)!=0; \
		if((x)->valid_##m) \
			(x)->m=val; \
	} while(0)

#define _dbstruct_is_valid_member(x,m) ((x)->valid_##m)

#define _dbstruct_read_str_member_from_db(x,m,db_var)	{ \
		const char* db_val; \
		db_val=_get_str_db(db_var,""); \
		(x)->valid_##m=db_val[0]!=0; \
		if((x)->valid_##m) \
			__strncpy((x)->m,db_val,sizeof((x)->m)); \
	} while(0)

#define _dbstruct_read_int_member_from_db(x,m,db_var) { \
		const char* db_val; \
		db_val=_get_str_db(db_var,""); \
		(x)->valid_##m=db_val[0]!=0; \
		if((x)->valid_##m) \
			(x)->m=atoi(db_val); \
	} while(0)

#define _dbstruct_write_str_member_to_db(x,m,db_var) {\
		if((x)->valid_##m) \
			_set_str_db(db_var,(x)->m,sizeof((x)->m)); \
		else \
			_set_str_db(db_var,"",-1); \
	} while(0)

#define _dbstruct_write_int_member_to_db(x,m,db_var) {\
		if((x)->valid_##m) \
			_set_int_db(db_var,(x)->m,NULL); \
		else \
			_set_str_db(db_var,"",-1); \
	} while(0)



struct db_struct_profile_tft {
	_dbstruct_uint_member(filter_id);
	_dbstruct_uint_member(eval_id);
	_dbstruct_str_member(ip_version);

	_dbstruct_str_member(source_ip);
	_dbstruct_uint_member(source_ip_mask);
	_dbstruct_uint_member(next_header);

	_dbstruct_uint_member(dest_port_range_start);
	_dbstruct_uint_member(dest_port_range_end);

	_dbstruct_uint_member(src_port_range_start);
	_dbstruct_uint_member(src_port_range_end);

	_dbstruct_uint_member(ipsec_spi);

	_dbstruct_uint_member(tos_mask);
	_dbstruct_uint_member(flow_label);
};

struct db_struct_profile {
	_dbstruct_str_member(result_rdb);

	_dbstruct_str_member(apn_name);
	_dbstruct_str_member(user);
	_dbstruct_str_member(pass);
	_dbstruct_str_member(auth);
	_dbstruct_str_member(pdp_type);
	_dbstruct_str_member(auth_preference);
	_dbstruct_str_member(ipv4_addr_pref);

	int valid_tft_id[2];
	struct db_struct_profile_tft tft_id[2];

	_dbstruct_uint_member(pdp_context_number);
	_dbstruct_uint_member(pdp_context_sec_flag);
	_dbstruct_uint_member(pdp_context_primary_id);
	_dbstruct_str_member(addr_allocation_preference);

	_dbstruct_uint_member(qci);
	_dbstruct_uint_member(g_dl_bit_rate);
	_dbstruct_uint_member(max_dl_bit_rate);
	_dbstruct_uint_member(g_ul_bit_rate);
	_dbstruct_uint_member(max_ul_bit_rate);
};

// local functions
////////////////////////////////////////////////////////////////////////////////
int _qmi_get_profile(int profile_index, int profile_type, char* profile_name, struct db_struct_profile* profile);


int wait_until_ready(const char* port, int timeout_sec)
{
	struct stat port_stat;

	clock_t now;
	clock_t start;
	clock_t period;

	start = _get_current_sec();

	while(1) {
		if(stat(port, &port_stat) >= 0)
			return 0;
		if(!loop_running) {
			SYSLOG(LOG_OPERATION, "process term detected");
			return -1;
		}

		// get now and period from start
		now = _get_current_sec();
		period = now - start;

		if(period >= timeout_sec)
			break;

		sleep(1);

		SYSLOG(LOG_OPERATION, "waiting until %s is ready #%d/%d", port, (int)period, timeout_sec);
	}

	return -1;
}

const char* _get_not_prefix_dbvar(const char* dbvar)
{
	if(strncmp(dbvar, wwan_prefix, wwan_prefix_len))
		return dbvar;

	return dbvar + wwan_prefix_len;
}

const char* _get_prefix_dbvar(const char* dbvar)
{
	static char buf[QMIMGR_MAX_DB_VARIABLE_LENGTH];

	snprintf(buf, sizeof(buf), "%s%s", wwan_prefix, dbvar);

	return buf;
}

const struct qmitlv_t* _get_tlv(struct qmimsg_t* msg, unsigned char t, int min_len) {
	struct qmitlv_t* tlv;

	// get TLV
	tlv = qmimsg_get_tlv(msg, t);
	if(!tlv) {
		SYSLOG(LOG_COMM, "TLV type(0x%02x) not found - msg_id=0x%04x, tlv_count=%d", t, msg->msg_id, msg->tlv_count);
		goto err;
	}

	// check validation
	if((tlv->l < min_len) || (min_len && !tlv->v)) {
		SYSLOG(LOG_ERROR, "TLV type(0x%02x) invalid - msg_id=0x%04x, required min size=%d,tlv length=%d, v=0x%08x", t, msg->msg_id, min_len, tlv->l, (unsigned int)tlv->v);
		goto err;
	}

	return tlv;

err:
	return NULL;
}

const char* _get_indexed_str(const char* str, int idx, const char* suffix)
{
	static char buf[QMIMGR_MAX_DB_VALUE_LENGTH];

	if(suffix)
		snprintf(buf, sizeof(buf), "%s%d%s", str, idx, suffix);
	else
		snprintf(buf, sizeof(buf), "%s%d", str, idx);

	return buf;
}

const char* _get_unknown_code(unsigned int code, const char* suffix)
{
	static char buf[QMIMGR_MAX_DB_BIGVALUE_LENGTH];

	if(suffix)
		snprintf(buf, sizeof(buf), "UNKNOWN(0x%08x) - %s", code, suffix);
	else
		snprintf(buf, sizeof(buf), "UNKNOWN(0x%08x)", code);

	return buf;
}

const char* _get_str_db_ex(const char* dbvar, const char* defval, int prefix)
{
	const char* prefix_dbvar;
	static char buf[QMIMGR_MAX_DB_VALUE_LENGTH];

	int len;

	if(prefix)
		prefix_dbvar = _get_prefix_dbvar(dbvar);
	else
		prefix_dbvar = dbvar;

	len = sizeof(buf);
	if(rdb_get(_s, prefix_dbvar, buf, &len) < 0) {
		SYSLOG(LOG_DB, "failed to get db variable(%s) - %s(%d)", prefix_dbvar, strerror(errno), errno);
		goto err;
	}

	SYSLOG(LOG_DB, "###db### read var=%s, val=%s", prefix_dbvar, buf);

	return buf;

err:
	return defval;
}

const char* _get_str_db(const char* dbvar, const char* defval)
{
	return _get_str_db_ex(dbvar, defval, 1);
}

int _get_int_db_ex(const char* dbvar, int defval, int prefix)
{
	const char* dbval;

	dbval = _get_str_db_ex(dbvar, NULL, prefix);
	if(!dbval || !dbval[0])
		return defval;

	return atoi(dbval);
}

int _get_int_db(const char* dbvar, int defval)
{
	return _get_int_db_ex(dbvar, defval, 1);
}


int _set_str_db_ex(const char* dbvar, const char* dbval, int dbval_len, int prefix)
{
	const char* prefix_dbvar;
	char buf[QMIMGR_MAX_DB_BIGVALUE_LENGTH];
	int buf_len;

	if(prefix)
		prefix_dbvar = _get_prefix_dbvar(dbvar);
	else
		prefix_dbvar = dbvar;

	// get buffer length
	if(dbval_len < 0) {
		buf_len = sizeof(buf);
	} else {
		dbval_len++;
		buf_len = (dbval_len < sizeof(buf)) ? dbval_len : sizeof(buf);
	}

	// strcpy
	__strncpy(buf, dbval, buf_len);

	if(rdb_set_string(_s, prefix_dbvar, buf) < 0) {

		if(errno != ENOENT) {
			SYSLOG(LOG_ERROR, "failed to set db variable(%s) val=%s - %s(%d)", prefix_dbvar, buf, strerror(errno), errno);
			goto err;
		}

		if(rdb_create_string(_s, prefix_dbvar, buf, CREATE, ALL_PERM) < 0) {
			SYSLOG(LOG_ERROR, "failed to create db variable(%s) val=%s - %s(%d)", prefix_dbvar, buf, strerror(errno), errno);
			goto err;
		}
	}

	SYSLOG(LOG_DB, "###db### write var=%s, val=%s", prefix_dbvar, buf);
	return 0;

err:
	return -1;
}

int _set_str_db_with_no_prefix(const char* dbvar, const char* dbval, int dbval_len)
{
	return _set_str_db_ex(dbvar, dbval, dbval_len, 0);
}

const char* _get_fmt_dbvar_ex(int prefix, const char* fmt, const char* defval, ...)
{
	char buf[EZQMI_DB_VAL_MAX_LEN];
	va_list ap;

	va_start(ap, defval);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return _get_str_db_ex(buf, defval, prefix);
}

int _set_fmt_dbvar_ex(int prefix, const char* fmt, const char* dbval, ...)
{
	char buf[EZQMI_DB_VAL_MAX_LEN];
	va_list ap;

	va_start(ap, dbval);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return _set_str_db_ex(buf, dbval, -1, prefix);
}

int _set_fmt_db_ex(const char* dbvar, const char* fmt, ...)
{
	char buf[EZQMI_DB_VAL_MAX_LEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return _set_str_db_ex(dbvar, buf, -1, 1);
}

int _set_str_db(const char* dbvar, const char* dbval, int dbval_len)
{
	return _set_str_db_ex(dbvar, dbval, dbval_len, 1);
}

int _set_reset_db(const char* dbvar)
{
	return _set_str_db(dbvar, "", -1);
}

#define DEFINE_SET_TYPE_DB_ACCESS_FUNC(type,format) \
	int _set_##type##_db_ex(const char* dbvar,type dbval,const char* suffix,int prefix) \
	{ \
		char dbval_str[QMIMGR_MAX_DB_VARIABLE_LENGTH]; \
	\
		if(suffix) \
			snprintf(dbval_str,sizeof(dbval_str),format "%s",dbval,suffix); \
		else \
			snprintf(dbval_str,sizeof(dbval_str),format,dbval); \
	\
		return _set_str_db_ex(dbvar,dbval_str,-1,prefix); \
	} \
	\
	int _set_##type##_db(const char* dbvar,type dbval,const char* suffix) \
	{ \
		return _set_##type##_db_ex(dbvar,dbval,suffix,1); \
	}


DEFINE_SET_TYPE_DB_ACCESS_FUNC(int, "%d")	/*  _set_int_db_ex() _set_int_db() */
DEFINE_SET_TYPE_DB_ACCESS_FUNC(float, "%f")	/*  _set_float_db_ex() _set_float_db() */

int _set_idx_db(const char* dbvar, unsigned long long idx)
{
	const char* dbval;
	unsigned int code;

	code = (unsigned int)(idx & 0xffffffff);

	SYSLOG(LOG_DEBUG, "setting database variable - var=%s, idx=0x%016llx, code=%d", dbvar, idx, code);

	dbval = resourcetree_lookup(res_qmi_strings, idx);
	if(!dbval)
		dbval = _get_unknown_code(code, 0);

	return _set_str_db(dbvar, dbval, -1);
}

int qmimgr_init()
{
	int i;
	const char* dbvar;

	pid_t pid;
	pid_t sid;

	// openlog
	openlog("qmimgr", LOG_PID | (!be_daemon ? LOG_PERROR : 0), LOG_USER);
	setlogmask(LOG_UPTO(LOG_DEBUG));

	if(be_daemon) {
		pid = fork();
		if(pid < 0) {
			SYSLOG(LOG_ERROR, "failed to fork - %s(%d)", strerror(errno), errno);
			goto err;
		} else if(pid > 0) {
			SYSLOG(LOG_OPERATION, "child forked - pid-%d", pid);
			exit(EXIT_SUCCESS);
		}

		SYSLOG(LOG_OPERATION, "daemonized");

		// change file mode mask
		umask(0);

		// create a new group id
		sid = setsid();
		if(sid < 0) {
			SYSLOG(LOG_ERROR, "failed to set a group id - %s(%d)", strerror(errno), errno);
			exit(EXIT_FAILURE);
		}

		// change to the root directory - not to lock the directory
		if((chdir("/")) < 0) {
			SYSLOG(LOG_ERROR, "failed to change directory - %s(%d)", strerror(errno), errno);
			exit(EXIT_FAILURE);
		}

		// say goodbye to all standard files
		SYSLOG(LOG_OPERATION, "closing standard input, output and error");

		int i;

		// close all files
		for(i = getdtablesize(); i >= 0; --i)
			close(i);

		int fd;

		fd = open("/dev/null", O_RDWR);
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);

		SYSLOG(LOG_OPERATION, "finished daemonizing");
	}

	SYSLOG(LOG_DEBUG, "* DS profile extened error codes");
	SYSLOG(LOG_DEBUG, "1 DS_PROFILE_REG_RESULT_FAIL General Failure");
	SYSLOG(LOG_DEBUG, "2 DS_PROFILE_REG_RESULT_ERR_INVAL_HNDL The request contains an invalid profile handle.");
	SYSLOG(LOG_DEBUG, "3 DS_PROFILE_REG_RESULT_ERR_INVAL_OP An invalid operation was requested.");
	SYSLOG(LOG_DEBUG, "4 DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_TYPE The request contains an invalid technology type.");
	SYSLOG(LOG_DEBUG, "5 DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_NUM The request contains an invalid profile number.");
	SYSLOG(LOG_DEBUG, "6 DS_PROFILE_REG_RESULT_ERR_INVAL_IDENT The request contains an invalid profile identifier.");
	SYSLOG(LOG_DEBUG, "7 DS_PROFILE_REG_RESULT_ERR_INVAL The request contains an invalid argument other than profile number and profile identifier received");
	SYSLOG(LOG_DEBUG, "8 DS_PROFILE_REG_RESULT_ERR_LIB_NOT_INITED Profile registry has not been initialized yet");
	SYSLOG(LOG_DEBUG, "9 DS_PROFILE_REG_RESULT_ERR_LEN_INVALID The request contains a parameter with invalid length.");
	SYSLOG(LOG_DEBUG, "10 DS_PROFILE_REG_RESULT_LIST_END End of the profile list was reached while searching for the requested profile.");
	SYSLOG(LOG_DEBUG, "11 DS_PROFILE_REG_RESULT_ERR_INVAL_SUBS_ID The request contains an invalid subscrition identifier.");
	SYSLOG(LOG_DEBUG, "12 DS_PROFILE_REG_INVAL_PROFILE_FAMILY The request contains an invalid profile family.");
	SYSLOG(LOG_DEBUG, "1001 DS_PROFILE_REG_3GPP_INVAL_PROFILE_FAMILY The request contains an invalid 3GPP profile family.");
	SYSLOG(LOG_DEBUG, "1002 DS_PROFILE_REG_3GPP_ACCESS_ERR An error was encountered while accessing the 3GPP profiles.");
	SYSLOG(LOG_DEBUG, "1003 DS_PROFILE_REG_3GPP_CONTEXT_NOT_DEFINED The given 3GPP profile doesn’t have a valid context.");

	enter_singleton();

	// create database
	SYSLOG(LOG_OPERATION, "opening database");
	if(rdb_open(NULL, &_s) < 0) {
		SYSLOG(LOG_ERROR, "failed to open database - %s", strerror(errno));
		goto err;
	}

	// deal with debug database variable
	if(verbosity_cmdline) {
		SYSLOG(LOG_OPERATION, "changing log level to %d by command line", verbosity);
		_set_int_db("debug", verbosity, NULL);

		setlogmask(LOG_UPTO(verbosity + LOG_INFO));
	} else {
		verbosity = _get_int_db("debug", 0);
		_set_int_db("debug", verbosity, NULL);

		SYSLOG(LOG_OPERATION, "changing log level to %d by wwan.0.debug", verbosity);
		setlogmask(LOG_UPTO(verbosity + LOG_INFO));
	}

	// set signal handler
	SYSLOG(LOG_DEBUG, "setting signal handlers...");
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGCHLD, sig_handler);
	signal(SIGALRM, sig_handler);

	// logging test
	SYSLOG(LOG_0, "loglevel check - LOG_0(%d)", LOG_0);
	SYSLOG(LOG_1, "loglevel check - LOG_1(%d)", LOG_1);
	SYSLOG(LOG_ERROR, "loglevel check - LOG_ERROR(%d) / error", LOG_ERROR);
	SYSLOG(LOG_OPERATION, "loglevel check - LOG_OPERATION(%d) / normal", LOG_OPERATION);
	SYSLOG(LOG_DB, "loglevel check - LOG_DB(%d) / db", LOG_DB);
	SYSLOG(LOG_COMM, "loglevel check - LOG_COMM(%d) / QMI", LOG_COMM);
	SYSLOG(LOG_DUMP, "loglevel check - LOG_DUMP(%d) / QMI dump", LOG_DUMP);
	SYSLOG(LOG_DEBUG, "loglevel check - LOG_DEBUG(%d) / ALL", LOG_DEBUG);

#ifdef QMI_VOICE_y
	SYSLOG(LOG_OPERATION, "###voice### init. qmivoice");
	if(init_qmivoice() < 0) {
		SYSLOG(LOG_ERROR, "failed to init voice");
		goto err;
	}
#endif

	// create db resources - database variables
	SYSLOG(LOG_OPERATION, "creating database variable hash");
	dbvars = dbhash_create(dbcmdvar_res, __countof(dbcmdvar_res));
	if(!dbvars) {
		SYSLOG(LOG_ERROR, "failed to create db name resource - dbvars");
		goto err;
	}

	// create db resources - database variables
	SYSLOG(LOG_OPERATION, "creating at command hash");
	atcmds = dbhash_create(atcmd_res, __countof(atcmd_res));
	if(!atcmds) {
		SYSLOG(LOG_ERROR, "failed to create at command hash - atcmds");
		goto err;
	}


	// create db resources - database commands
	SYSLOG(LOG_OPERATION, "creating database command hash");
	dbcmds = dbhash_create(dbcmdval_res, __countof(dbcmdval_res));
	if(!dbcmds) {
		SYSLOG(LOG_ERROR, "failed to create db name resource - dbcmds");
		goto err;
	}

	// create mccmncs
	SYSLOG(LOG_OPERATION, "creating mccmnc hash");
	mccmncs = dbhash_create(mccmnc_res, __countof(mccmnc_res));
	if(!mccmncs) {
		SYSLOG(LOG_ERROR, "failed to create mccmnc hash");
		goto err;
	}

	// create string resources
	SYSLOG(LOG_OPERATION, "creating res_qmi_strings resource");
	res_qmi_strings = resourcetree_create(res_qmi_strings_elements, __countof(res_qmi_strings_elements));
	if(!res_qmi_strings) {
		SYSLOG(LOG_ERROR, "failed to create string resource - res_qmi_strings");
		goto err;
	}

	// wait for last port
	if(last_port) {
		SYSLOG(LOG_OPERATION, "waiting for the last port %s", last_port);
		if(wait_until_ready(last_port, QMIMGR_LASTPORT_WAIT_SEC) < 0) {
			SYSLOG(LOG_ERROR, "last port %s not ready", last_port);
		}
	} else {
		SYSLOG(LOG_OPERATION, "skipping last port - option not given");
	}

	// wait for qcqmi
	SYSLOG(LOG_OPERATION, "waiting for qmi port %s", qmi_port);
	if (
#ifdef CONFIG_LINUX_QMI_DRIVER
      !is_quectel_qmi_proxy(qmi_port) && 
#endif
      wait_until_ready(qmi_port, QMIMGR_PORT_WAIT_SEC) < 0) {
		SYSLOG(LOG_ERROR, "qmi port (%s) not ready", qmi_port);
		goto err2;
	}

	// wait for at port
	if(at_port) {
		SYSLOG(LOG_OPERATION, "waiting for at port %s", at_port);
		if(wait_until_ready(at_port, QMIMGR_PORT_WAIT_SEC) < 0) {
			SYSLOG(LOG_ERROR, "at port (%s) not ready", at_port);
			goto err2;
		}
	} else {
		SYSLOG(LOG_OPERATION, "skipping AT port - option not given");
	}

#ifndef DEBUG
#ifdef CONFIG_INIT_DELAY
	SYSLOG(LOG_OPERATION, "apply stabilization period (%d sec)", CONFIG_INIT_DELAY);
	sleep(CONFIG_INIT_DELAY);
	if(!loop_running) {
		SYSLOG(LOG_ERROR, "got termination request");
		goto err;
	}
#endif
#endif

	// create universal client
	SYSLOG(LOG_OPERATION, "creating qmi universal client");
	uni = qmiuniclient_create(qmi_port, instance);
	if(!uni) {
		SYSLOG(LOG_ERROR, "failed to create universal qmi client");
		goto err;
	}

	// create atport
	SYSLOG(LOG_OPERATION, "creating atport");
	port = port_create();
	if(!port) {
		SYSLOG(LOG_ERROR, "failed to create atport");
		goto err;
	}

	if(at_port) {
		// open atport
		if(port_open(port, at_port) < 0) {
			SYSLOG(LOG_ERROR, "failed to open atport");
			goto err;
		}
	}

	// register notifications
	port_register_callbacks(port, atmgr_callback_on_noti, atmgr_callback_on_common);

	// create schedule
	SYSLOG(LOG_OPERATION, "creating function scheduler");
	sched = funcschedule_create();
#ifdef MODULE_PRI_BASED_OPERATION
	/* check if Verizon Network */
	int ret;
	if(vzw_pripub) {
	    SYSLOG(LOG_OPERATION,"[qmimgr_init] calling synchronizeAPN() function at the start of qmimgr...");
	    /* synchronize router APN from module APN */
	    ret = synchronizeAPN();
	    switch(ret) {
		case 0:
			SYSLOG(LOG_ERR,"[qmimgr_init] APN synchronization success");
			break;
		case -2:
			SYSLOG(LOG_ERR,"[qmimgr_init] Module Default Profile APN and Router Default link profile APN matches...so no need to sync");
			break;
		case -1:
			SYSLOG(LOG_ERR,"[qmimgr_init] module profile could not be read...hence no question of sync");
			break;
		default:
			break;
	    }
	}
#endif
	// subscribe database
	SYSLOG(LOG_OPERATION, "subscribing database variables");
	for(i = 0; i < __countof(dbcmdvar_res); i++) {

		if(dbcmdvar_res[i].idx & DB_COMMAND_WITH_NO_PREFIX)
			dbvar = dbcmdvar_res[i].str;
		else
			dbvar = _get_prefix_dbvar(dbcmdvar_res[i].str);

		if(rdb_create_string(_s, dbvar, "", CREATE, ALL_PERM) < 0) {
			if(errno != EEXIST) {
				SYSLOG(LOG_ERROR, "failed to create database variable(%s) - %s", dbvar, strerror(errno));
			} else {
				SYSLOG(LOG_OPERATION, "database variable(%s) already exists", dbvar);
			}
		}

		if(rdb_subscribe(_s, dbvar) < 0) {
			SYSLOG(LOG_ERROR, "failed to subscribe database variable(%s) - %s", dbvar, strerror(errno));
		}
	}

	// create database enumerator
	SYSLOG(LOG_OPERATION, "subscribing database enumerator");
	dbenum = dbenum_create(_s, TRIGGERED);

	return 0;

err:
	return -1;

err2:
#ifdef PLATFORM_PLATYPUS
	SYSLOG(LOG_ERROR, "power-cycling module - workaround for module malfunctions");
	system("reboot_module.sh&");
#endif
	return -1;


}

void qmimgr_fini(int termbysignal)
{
	int i;

	// reset database variables
	SYSLOG(LOG_OPERATION, "reseting database variables");
	for(i = 0; i < __countof(dbresetvals); i++)
		_set_reset_db(dbresetvals[i]);

	SYSLOG(LOG_OPERATION, "destroying db enumerator");
	dbenum_destroy(dbenum);

	// destroy schedule
	SYSLOG(LOG_OPERATION, "destroying scheduler");
	funcschedule_destroy(sched);

	// destroy port
	port_close(port);
	port_destroy(port);

	// do not close it if the driver is dead
	if(termbysignal) {
		// destroy universal client
		SYSLOG(LOG_OPERATION, "destroying qmiuniclient");
		qmiuniclient_destroy(uni);
	}

	// destroy string resources
	SYSLOG(LOG_OPERATION, "destroying resources");
	resourcetree_destroy(res_qmi_strings);

	// destroy db resources
	SYSLOG(LOG_OPERATION, "destroying db resources");
	dbhash_destroy(mccmncs);
	dbhash_destroy(dbvars);
	dbhash_destroy(atcmds);
	dbhash_destroy(dbcmds);

#ifdef QMI_VOICE_y
	SYSLOG(LOG_OPERATION, "###voice### finishing qmivoice");
	fini_qmivoice();
#endif

	SYSLOG(LOG_OPERATION, "closing database");
	rdb_close(&_s);

	SYSLOG(LOG_OPERATION, "releasing singleton");
	release_singleton();

	SYSLOG(LOG_OPERATION, "closing log");
	closelog();
}

int qmimgr_callback_on_preprocess(struct qmiuniclient_t* uni, unsigned char msg_type, unsigned short tran_id, struct qmimsg_t* msg, unsigned short* result, unsigned short* error, unsigned short* ext_error, int* noti)
{
	unsigned short qmi_result;
	unsigned short qmi_error;
	int qmi_noti;

	// dump debug information
	int i;
	struct qmitlv_t* tlv;

	qmi_result = 0;
	qmi_error = 0;
	qmi_noti = 0;


	// dump
	SYSLOG(LOG_DUMP, "msg_type=0x%02x tran_id=0x%04x, msg_id=0x%04x, tlv_count=%d", msg_type, tran_id, msg->msg_id, msg->tlv_count);
	for(i = 0; i < msg->tlv_count; i++) {

		tlv = msg->tlvs[i];

		SYSLOG(LOG_DUMP, "TLV %d type=0x%02x,len=%d", i, tlv->t, tlv->l);
		_dump(LOG_DUMP, __FUNCTION__, tlv->v, tlv->l);
	}

	// preprocess msg by msg_type
	switch(msg_type) {
	case QMI_MSGTYPE_REQ:
		SYSLOG(LOG_ERROR, "got QMI_MSGTYPE_REQ - wierd!!");
		goto err;


	case QMI_MSGTYPE_RESP: {
		const struct qmitlv_t* tlv;
		const char* error_msg;
		unsigned short resp_ext_code = 0;

		struct qmi_resp_result* resp;
		unsigned short* resp2;

		SYSLOG(LOG_COMM, "got QMI_MSGTYPE_RESP");

		tlv = _get_tlv(msg, QMI_RESP_RESULT_CODE_TYPE, sizeof(*resp));
		if(!tlv)
			break;

		// get result TLV
		resp = (struct qmi_resp_result*)tlv->v;
		qmi_result = read16_from_little_endian(resp->qmi_result);
		qmi_error = read16_from_little_endian(resp->qmi_error);

		error_msg = resourcetree_lookup(res_qmi_strings, RES_STR_KEY(QMIEX, QMIERR, qmi_error));
		if(!error_msg)
			error_msg = "unknown";

		/* extended error code */
		tlv = _get_tlv(msg, QMI_RESP_RESULT_EXTCODE_TYPE, sizeof(*resp2));
		if(tlv) {
			resp2 = (unsigned short*)tlv->v;
			resp_ext_code = *resp2;
		}

		if(ext_error)
			*ext_error = resp_ext_code;

		SYSLOG(LOG_COMM, "###qmimsg### tran_id=%d,msg_id=0x%04x,qmi_result=0x%04x,qmi_error=%s(0x%04x),qmi_ext_error(0x%04x)", tran_id, msg->msg_id, qmi_result, error_msg, qmi_error, resp_ext_code);
		break;
	}

	case QMI_MSGTYPE_INDI:
		SYSLOG(LOG_OPERATION, "got QMI_MSGTYPE_INDI");

		qmi_noti = 1;
		break;

	default:
		SYSLOG(LOG_OPERATION, "got incorrect msg type = 0x%02x", msg_type);
		goto err;
	}

	if(result)
		*result = qmi_result;
	if(error)
		*error = qmi_error;

	if(noti)
		*noti = qmi_noti;

	return 0;

err:
	return -1;
}

// handle QMI call back for IPv6 client
void qmimgr_callback_on_ipv6(unsigned char msg_type, struct qmimsg_t* msg,
		unsigned short qmi_result, unsigned short qmi_error, int noti)
{
	static int profile_index = -1; // will be set when IPv6 pdp up, re-used when pdp down
	static const char* rdb_vars[] = {
		"link.profile.%d.ipv6_ipaddr",
		"link.profile.%d.ipv6_dns1",
		"link.profile.%d.ipv6_dns2",
		"link.profile.%d.ipv6_gw",
	};

	unsigned char tlv_types[] = {
		QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6ADDR,
		QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6DNS1,
		QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6DNS2,
		QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6GATEWAY
	};

	int i;
	const struct qmitlv_t* tlv;
	switch(msg->msg_id) {
	case QMI_WDS_GET_RUNTIME_SETTINGS: {
		// read profile id first
		tlv = _get_tlv(msg, QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_PROFID, 2);
		if (tlv) {
			unsigned char* values = (unsigned char*) tlv->v;
			profile_index = values[1];
			SYSLOG(LOG_DEBUG,"profile type:%u, index:%u", values[0], profile_index);
		}

		if (profile_index < 0) {
			SYSLOG(LOG_ERR, "Failed to get profile index from module");
			break;
		}

		// IPv6 PDP up, save IP, dns, gateway addresses to rdb variables
		for(i = 0; i < __countof(tlv_types); i++) {
			char rdb[QMIMGR_MAX_DB_VARIABLE_LENGTH];
			snprintf(rdb, sizeof(rdb), rdb_vars[i], profile_index);
			static const int ipv6addr_len = 16; // IPv6 address is 16 bytes long
			tlv = _get_tlv(msg, tlv_types[i], ipv6addr_len);
			if (tlv) {
				char buf[QMIMGR_MAX_DB_VALUE_LENGTH];
				unsigned char* ip = (unsigned char*) tlv->v;
				snprintf(buf, sizeof(buf),
					"%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
					ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8],
					ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15]);
				_set_str_db(rdb, buf, -1);
			} else {
				_set_reset_db(rdb);
			}
		}

		_set_fmt_dbvar_ex(1, "system_network_status.pdp%d_stat", "up", instance%10);
		break;
	}

	case QMI_WDS_GET_PKT_SRVC_STATUS: {
		unsigned char ipfamily = 0;
		tlv = _get_tlv(msg, QMI_WDS_GET_PKT_SRVC_STATUS_RESP_IP_FAMILY, sizeof(ipfamily));
		if (!tlv) {
			SYSLOG(LOG_ERR, "Failed to get IP family of the data connection");
			break;
		}

		ipfamily = *((unsigned char*)tlv->v);

		// ignore non-IPv6 data call
		if (ipfamily != IPV6) {
			break;
		}

		int pstat = 0;
		if(noti) {
			struct qmi_wds_get_pkt_srvc_status_indi_pss* pss;

			SYSLOG(LOG_DEBUG, "got QMI_WDS_GET_PKT_SRVC_STATUS_IND");
			tlv = _get_tlv(msg, QMI_WDS_GET_PKT_SRVC_STATUS_RESP_TYPE, sizeof(*pss));
			if(tlv) {
				pss = (struct qmi_wds_get_pkt_srvc_status_indi_pss*)tlv->v;
				pstat = (pss->conn_stat == QMI_WDS_PKT_DATA_CONNECTED) && (pss->reconn_req == 0);
			}
		} else {
			unsigned char* conn_stat;

			SYSLOG(LOG_DEBUG, "got QMI_WDS_GET_PKT_SRVC_STATUS");
			tlv = _get_tlv(msg, QMI_WDS_GET_PKT_SRVC_STATUS_RESP_TYPE, sizeof(*conn_stat));
			if(tlv) {
				conn_stat = (unsigned char*)tlv->v;
				pstat = (*conn_stat == QMI_WDS_PKT_DATA_CONNECTED);
			}
		}

		if (pstat) {
			// PDP up, trigger a read of IPv6 address that LTE network assigned to our pdp
			struct qmimsg_t* msg = qmimsg_create();
			if (!msg) {
				SYSLOG(LOG_ERR,"Failed to create a msg to read IPv6 addr");
				break;
			}
			qmimsg_set_msg_id(msg, QMI_WDS_GET_RUNTIME_SETTINGS);
			qmimsg_clear_tlv(msg);
			unsigned int mask = QMI_WDS_GET_RUNTIME_SETTINGS_MASK_IP_ADDR|
				QMI_WDS_GET_RUNTIME_SETTINGS_MASK_DNS_ADDR|
				QMI_WDS_GET_RUNTIME_SETTINGS_MASK_GW_ADDR|
				QMI_WDS_GET_RUNTIME_SETTINGS_MASK_PROFILE_ID;
			if (qmimsg_add_tlv(msg, QMI_WDS_GET_RUNTIME_SETTINGS_REQ_TYPE, sizeof(mask), &mask) < 0){
				SYSLOG(LOG_ERR,"Failed to add TLV to read IPv6 addr");
				qmimsg_destroy(msg);
				break;
			}
			qmiuniclient_write(uni, QMIIPV6, msg, NULL);
			qmimsg_destroy(msg);
		} else if (profile_index >= 0) {
			// PDP down and index is valid, clear ipv6 rdb variables
			for(i = 0; i < __countof(rdb_vars); i++) {
				char rdb[QMIMGR_MAX_DB_VARIABLE_LENGTH];
				snprintf(rdb, sizeof(rdb), rdb_vars[i], profile_index);
				_set_reset_db(rdb);
			}
			_set_fmt_dbvar_ex(1, "system_network_status.pdp%d_stat", "down", instance%10);
		}
		break;
	}
	}
}

void qmimgr_callback_on_wds(unsigned char msg_type, struct qmimsg_t* msg,
		unsigned short qmi_result, unsigned short qmi_error, int noti)
{
	const struct qmitlv_t* tlv;

	switch(msg->msg_id) {
	case QMI_WDS_GET_RUNTIME_SETTINGS: {
		int i;
		unsigned int addr;
		char buf[QMIMGR_MAX_DB_VALUE_LENGTH];

		SYSLOG(LOG_OPERATION, "got QMI_WDS_GET_RUNTIME_SETTINGS_REQ_TYPE");

		unsigned char tlv_types[] = {
			QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_DNS1,
			QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_DNS2,
			QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPADDR,
			QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_GATEWAY,
			QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_NETMASK
		};

		const char* tlv_db_names[] = {
			"profile.cmd.param.dns1",
			"profile.cmd.param.dns2",
			"profile.cmd.param.ipaddr",
			"profile.cmd.param.gw",
			"profile.cmd.param.netmask"
		};

		// set information
		for(i = 0; i < __countof(tlv_types); i++) {
			tlv = _get_tlv(msg, tlv_types[i], sizeof(addr));

			if(tlv) {
				addr = read32_from_little_endian(tlv->v);

				snprintf(buf, sizeof(buf), "%d.%d.%d.%d", (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, (addr >> 0) & 0xff);
				_set_str_db(tlv_db_names[i], buf, -1);

			} else {
				_set_reset_db(tlv_db_names[i]);
			}
		}
		break;
	}

	case QMI_WDS_GET_PKT_SRVC_STATUS: {
		int pstat = -1;

		if(!is_enabled_feature(FEATUREHASH_CMD_CONNECT)) {
			SYSLOG(LOG_COMM, "got QMI_WDS_GET_PKT_SRVC_STATUS - feature not enabled (%s)", FEATUREHASH_UNSPECIFIED);
			break;
		}

		if(noti) {
			struct qmi_wds_get_pkt_srvc_status_indi_pss* pss;

			SYSLOG(LOG_COMM, "got QMI_WDS_GET_PKT_SRVC_STATUS_IND");

			/* QMI_WDS_GET_PKT_SRVC_STATUS_RESP_TYPE */
			tlv = _get_tlv(msg, QMI_WDS_GET_PKT_SRVC_STATUS_RESP_TYPE, sizeof(*pss));
			if(tlv) {
				pss = (struct qmi_wds_get_pkt_srvc_status_indi_pss*)tlv->v;
				pstat = (pss->conn_stat == QMI_WDS_PKT_DATA_CONNECTED) && (pss->reconn_req == 0);
			}
		} else {
			unsigned char* conn_stat;

			SYSLOG(LOG_COMM, "got QMI_WDS_GET_PKT_SRVC_STATUS");

			/* QMI_WDS_GET_PKT_SRVC_STATUS_RESP_TYPE */
			tlv = _get_tlv(msg, QMI_WDS_GET_PKT_SRVC_STATUS_RESP_TYPE, sizeof(*conn_stat));
			if(tlv) {
				conn_stat = (unsigned char*)tlv->v;
				pstat = (*conn_stat == QMI_WDS_PKT_DATA_CONNECTED);
			}
		}


		/* set rdb  for eacho sub_if*/
		if(pstat < 0) {
			_set_fmt_dbvar_ex(1, "system_network_status.pdp%d_stat", "", instance%10);
		} else {
			_set_fmt_dbvar_ex(1, "system_network_status.pdp%d_stat", pstat ? "up" : "down", instance%10);
		}

		break;
	}
	}
}

int _get_mcc_mnc(const char* imsi, int imsi_len, char* ret_mcc, char* ret_mnc)
{
	char mcc[4];
	char mnc[4];
	char mccmnc[3 + 1 + 3 + 1];
	int mccmnc32_idx;
	int mccmnc33_idx;
	int mccmnc_len;

	// check minimum length of MCC and MNC
	if(imsi_len < 3 + 3)
		goto err;

	// mcc(3) + mnc(2)
	__strncpy(mcc, imsi, 3 + 1);
	__strncpy(mnc, imsi + 3, 2 + 1);
	snprintf(mccmnc, sizeof(mccmnc), "%s %s", mcc, mnc);
	mccmnc32_idx = dbhash_lookup(mccmncs, mccmnc);

	// mcc(3) + mnc(3)
	__strncpy(mcc, imsi, 3 + 1);
	__strncpy(mnc, imsi + 3, 3 + 1);
	snprintf(mccmnc, sizeof(mccmnc), "%s %s", mcc, mnc);
	mccmnc33_idx = dbhash_lookup(mccmncs, mccmnc);

	if(mccmnc32_idx >= 0 && mccmnc33_idx >= 0) {
		SYSLOG(LOG_ERROR, "ambiguous - mccmnc32_idx=%d,mccmnc33_idx=%d (guessing mccmnc length is 5)", mccmnc32_idx, mccmnc33_idx);
		mccmnc_len = 5;
	} else if(mccmnc32_idx < 0 && mccmnc33_idx < 0) {
		SYSLOG(LOG_ERROR, "mccmnc not in database (guessing mccmnc length is 5)");
		mccmnc_len = 5;
	} else if(mccmnc32_idx > 0) {
		mccmnc_len = 5;
	} else {
		mccmnc_len = 6;
	}

	__strncpy(ret_mcc, imsi, 3 + 1);

	if(mccmnc_len == 6)
		__strncpy(ret_mnc, imsi + 3, 3 + 1);
	else
		__strncpy(ret_mnc, imsi + 3, 2 + 1);

	return 0;
err:
	return -1;
}

/*
 * Update rdb variables module_band_list and currentband.current_band based on
 * band masks: mask and lte_mask.
 * Params:
 *  mask: band bit mask for GSM/WCDMA
 *  lte_mask: band bit mask for LTE
 * Return:
 *  0 for success; -1 for failure.
 * On failure, both rdb variables will be cleared.
 */
static int
_qmi_update_band_list_by_masks(const struct qmi_band_bit_mask * mask,
                               const struct qmi_band_bit_mask * lte_mask)
{
    char band_list[QMIMGR_MAX_DB_BIGVALUE_LENGTH];
    char current_band[QMIMGR_MAX_DB_BIGVALUE_LENGTH];

    if(_qmi_build_band_list_from_masks(mask, lte_mask,
                                       band_list, __countof(band_list),
                                       current_band, __countof(current_band))) {
        _set_reset_db("module_band_list");
        _set_reset_db("currentband.current_band");
        return -1;
    }

    _set_str_db("module_band_list", band_list, -1);
    _set_str_db("currentband.current_band", current_band, -1);
    return 0;
}

/*
 * Band capability bit masks.
 * They are updated when reading supported bands from the module.
 * When selecting bands, we should never select a band beyond the capability.
 */
static struct qmi_band_bit_mask _g_band_cap, _g_lte_band_cap;

void qmimgr_callback_on_dms(unsigned char msg_type, struct qmimsg_t* msg, unsigned short qmi_result, unsigned short qmi_error, int noti)
{
	const struct qmitlv_t* tlv;

	switch(msg->msg_id) {
	case QMI_DMS_UIM_GET_ICCID:
		if(!is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
			SYSLOG(LOG_COMM, "got QMI_DMS_UIM_GET_ICCID - feature not enabled (%s)", FEATUREHASH_UNSPECIFIED);
			break;
		}

		SYSLOG(LOG_OPERATION, "got QMI_DMS_UIM_GET_ICCID");
		tlv = _get_tlv(msg, QMI_DMS_UIM_GET_ICCID_REQ_TYPE, 1);
		if(tlv) {
			_set_str_db("system_network_status.simICCID", tlv->v, tlv->l);
		} else {
			_set_reset_db("system_network_status.simICCID");
		}
		break;

	case QMI_DMS_GET_IMSI: {
		char mcc[4];
		char mnc[4];
		int mccmnc_len;
		int msin_len;
		char imsi[QMIMGR_MAX_DB_VALUE_LENGTH];

		SYSLOG(LOG_OPERATION, "got QMI_DMS_GET_IMSI");


		tlv = _get_tlv(msg, QMI_DMS_GET_IMSI_RESP_TYPE, 3 + 3);
		if(!tlv)
			goto qmi_dms_get_imsi_err;

		__strncpy(imsi, tlv->v, tlv->l + 1);

		if(_get_mcc_mnc(imsi, strlen(imsi), mcc, mnc) < 0) {
			SYSLOG(LOG_ERROR, "incorret format of IMSI - %s", imsi);
			goto qmi_dms_get_imsi_err;
		}

		mccmnc_len = strlen(mcc) + strlen(mnc);

		SYSLOG(LOG_OPERATION, "mccmnc_len=%d", mccmnc_len);

		if(!atoi(mcc) || !atoi(mnc)) {
			SYSLOG(LOG_ERROR, "incorret mcc(%s) or mnc(%s) - %s", imsi, mcc, mnc);
			goto qmi_dms_get_imsi_err;
		}

		_set_str_db("imsi.plmn_mcc", mcc, sizeof(mcc));
		_set_str_db("imsi.plmn_mnc", mnc, sizeof(mnc));

		msin_len = tlv->l - mccmnc_len;
		_set_str_db("imsi.msin", imsi + mccmnc_len, msin_len);
		break;

qmi_dms_get_imsi_err:
		_set_reset_db("imsi.plmn_mcc");
		_set_reset_db("imsi.plmn_mnc");
		_set_reset_db("imsi.msin");
		break;
	}

	case QMI_DMS_GET_MSISDN: {
		if(!is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
			SYSLOG(LOG_COMM, "got QMI_DMS_GET_MSISDN - feature not enabled (%s)", FEATUREHASH_UNSPECIFIED);
			break;
		}

		SYSLOG(LOG_OPERATION, "got QMI_DMS_GET_MSISDN");

		tlv = _get_tlv(msg, QMI_DMS_GET_MSISDN_RESP_TYPE, 1);
		if(tlv) {
			const char* resp = (const char*)tlv->v;
			_set_str_db("sim.data.msisdn", resp, tlv->l);
		} else {
			_set_reset_db("sim.data.msisdn");
		}

		break;
	}

	case QMI_DMS_GET_DEVICE_REV_ID: {

		SYSLOG(LOG_OPERATION, "got QMI_DMS_GET_DEVICE_REV_ID");

		tlv = _get_tlv(msg, QMI_DMS_GET_DEVICE_REV_ID_RESP_TYPE, 1);
		if(tlv) {
			const char* resp = (const char*)tlv->v;
			_set_str_db("firmware_version", resp, tlv->l);
		} else {
			_set_reset_db("firmware_version");
		}

		break;
	}

	case QMI_DMS_GET_DEVICE_MFR: {
		SYSLOG(LOG_OPERATION, "got QMI_DMS_GET_DEVICE_MFR");

		tlv = _get_tlv(msg, QMI_DMS_GET_DEVICE_MFR_RESP_TYPE, 1);
		if(tlv) {
			const char* resp = (const char*)tlv->v;
			_set_str_db("manufacture", resp, tlv->l);
		} else {
			_set_reset_db("manufacture");
		}

		break;
	}

	case QMI_DMS_GET_DEVICE_MODEL_ID: {
		SYSLOG(LOG_OPERATION, "got QMI_DMS_GET_DEVICE_MODEL_ID");

		tlv = _get_tlv(msg, QMI_DMS_GET_DEVICE_MODEL_ID_RESP_TYPE, 1);
		if(tlv) {
			const char* resp = (const char*)tlv->v;
			_set_str_db("model", resp, tlv->l);
		} else {
			_set_reset_db("model");
		}

		break;
	}

	case QMI_DMS_GET_BAND_CAPABILITY: {
		SYSLOG(LOG_COMM, "got QMI_DMS_GET_BAND_CAPABILITY");
		const struct qmi_band_bit_mask * resp = NULL, * resp_lte = NULL;
		tlv = _get_tlv(msg,
		               QMI_DMS_GET_BAND_CAPABILITY_RESP_TYPE_BAND,
		               sizeof(*resp));
		if(tlv) {
			resp = (const struct qmi_band_bit_mask *)tlv->v;
			SYSLOG(LOG_INFO, "Band Cap = %llx", resp->band);
			_g_band_cap = *resp;
		} else {
			SYSLOG(LOG_ERROR, "Failed to get band capability");
		}
		tlv = _get_tlv(msg,
		               QMI_DMS_GET_BAND_CAPABILITY_RESP_TYPE_LTEBAND,
		               sizeof(*resp_lte));
		if(tlv) {
			resp_lte = (const struct qmi_band_bit_mask*)tlv->v;
			SYSLOG(LOG_INFO, "LTE Band Cap = %llx", resp_lte->band);
			_g_lte_band_cap = *resp_lte;
		} else { // it is not an error if LTE is not supported
			SYSLOG(LOG_INFO, "Failed to get lte band capability");
		}

		// limit to custom bands
		custom_band_limit(&_g_band_cap, &_g_lte_band_cap);

		_qmi_update_band_list_by_masks(resp, resp_lte);
		break;
	}

	case QMI_DMS_GET_DEVICE_SERIAL_NUMBERS: {
		const char* resp;

		SYSLOG(LOG_OPERATION, "got QMI_DMS_GET_DEVICE_SERIAL_NUMBERS");

		// get TLV - QMI_DMS_GET_DEVICE_SERIAL_NUMBERS_RESP_TYPE_IMEI
		tlv = _get_tlv(msg, QMI_DMS_GET_DEVICE_SERIAL_NUMBERS_RESP_TYPE_IMEI, 1);
		if(tlv) {
			resp = (const char*)tlv->v;
			_set_str_db("imei", resp, __min(15, tlv->l));
		} else {
			_set_reset_db("imei");
		}


		break;
	}

	case QMI_DMS_GET_ACTIVATED_STATE: {
		char* resp;

		SYSLOG(LOG_DEBUG, "[oma-dm] got QMI_DMS_GET_ACTIVATED_STATE");

		tlv = _get_tlv(msg, QMI_DMS_GET_ACTIVATED_STATE_RESP_TYPE, sizeof(*resp));
		if(tlv) {
			resp = (char*)tlv->v;

			_set_fmt_db_ex("omadm_activated", "%d", *resp ? 1 : 0);
		} else
			_set_reset_db("omadm_activated");

		break;
	}


	case QMI_DMS_UIM_GET_PIN_STATUS: {
		int i;
		unsigned char t[] = {QMI_DMS_UIM_GET_PIN_STATUS_RESP_TYPE_PIN1, QMI_DMS_UIM_GET_PIN_STATUS_RESP_TYPE_PIN2};

		SYSLOG(LOG_COMM, "got QMI_DMS_UIM_GET_PIN_STATUS");

		for(i = 0; i < __countof(t); i++) {

			// get TLV - QMI_DMS_UIM_GET_PIN_STATUS_RESP_TYPE_PIN1
			tlv = _get_tlv(msg, t[i], sizeof(struct qmi_dms_uim_get_pin_status_resp));
			if(tlv) {
				struct qmi_dms_uim_get_pin_status_resp* resp = (struct qmi_dms_uim_get_pin_status_resp*)tlv->v;
				const char* pin_enabled;
				const char* dbvar;

				if((resp->status == 1) || (resp->status == 2))
					pin_enabled = "Enabled";
				else if(resp->status == 3)
					pin_enabled = "Disabled";
				else
					pin_enabled = "";

				// backward compatibabity
				if(i == 0) {
					_set_idx_db("sim.status.status", RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, (1 << 16) | resp->status));
					_set_str_db("sim.status.pin_enabled", pin_enabled, -1);

					/* store sim card status */
					if((resp->status == 1) || (resp->status == 2))
						_simcard_pin_enabled = 1;
					else
						_simcard_pin_enabled = 0;

					_set_int_db("sim.cmd.param.verify_left", resp->verify_retries_left, NULL);
					_set_int_db("sim.cmd.param.unblock_left", resp->unblock_retries_left, NULL);

					/* AT port manager - support incorrect port manager RDB variables */
					_set_int_db("sim.status.retries_remaining", resp->verify_retries_left, NULL);
					_set_int_db("sim.status.retries_puk_remaining", resp->unblock_retries_left, NULL);

				}

				// get qmi sim status
				dbvar = _get_indexed_str("sim.status.status", i, NULL);
				_set_idx_db(dbvar, RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, resp->status));

				// set pin_enabled
				dbvar = _get_indexed_str("sim.status.pin_enabled", i, NULL);
				_set_str_db(dbvar, pin_enabled, -1);

				// set verify left
				dbvar = _get_indexed_str("sim.cmd.param.verify_left", i, NULL);
				_set_int_db(dbvar, resp->verify_retries_left, NULL);

				// set unblock left
				dbvar = _get_indexed_str("sim.cmd.param.unblock_left", i, NULL);
				_set_int_db(dbvar, resp->unblock_retries_left, NULL);

			} else {
				if(i == 0) {
					_set_str_db("sim.status.status", "SIM not inserted", -1);
					_set_reset_db("sim.status.pin_enabled");
					_set_reset_db("sim.cmd.param.verify_left");
					_set_reset_db("sim.cmd.param.unblock_left");
				}

				_set_reset_db(_get_indexed_str("sim.status.status", i, NULL));
				_set_reset_db(_get_indexed_str("sim.status.pin_enabled", i, NULL));
				_set_reset_db(_get_indexed_str("sim.cmd.param.verify_left", i, NULL));
				_set_reset_db(_get_indexed_str("sim.cmd.param.unblock_left", i, NULL));
			}
		}

		break;
	}
	}
}

const char* convert_decimal_degree_to_degree_minute(double decimal, int latitude, char* dir, int dir_len)
{
	int degree;

	static char degree_result[128];

	/* get direction */
	if(latitude)
		snprintf(dir, dir_len, "%s", decimal >= 0 ? "N" : "S");
	else
		snprintf(dir, dir_len, "%s", decimal >= 0 ? "E" : "W");

	/* remove sign */
	if(decimal < 0)
		decimal = -decimal;

	/* degree */
	degree = (int)decimal;

	/* minute */
	decimal = (decimal - degree) * 60;

	snprintf(degree_result, sizeof(degree_result), latitude ? "%02d%09.6f" : "%03d%09.6f", degree, decimal);

	return degree_result;
}

#ifdef V_SMS_QMI_MODE_y
void qmimgr_callback_on_wms(unsigned char msg_type, struct qmimsg_t* msg, unsigned short qmi_result, unsigned short qmi_error, int noti)
{
	const struct qmitlv_t* tlv;

	switch(msg->msg_id) {
	case QMI_WMS_SERVICE_READY_IND: {
		char *resp_ind;
		int* resp_ready_status;

		SYSLOG(LOG_OPERATION, "###sms### got QMI_WMS_SERVICE_READY_IND");

		/* get transport layer info */
		tlv = _get_tlv(msg, QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_REG, sizeof(*resp_ind));
		if(tlv) {
			resp_ind = (char*)(tlv->v);
			SYSLOG(LOG_DEBUG, "###sms### QMI_WMS_SERVICE_READY_IND - ind=%d", *resp_ind);
		}

		/* get transport layer info */
		tlv = _get_tlv(msg, QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_RSTAT, sizeof(*resp_ready_status));
		if(tlv) {
			resp_ready_status = (int*)(tlv->v);
			SYSLOG(LOG_DEBUG, "###sms### QMI_WMS_SERVICE_READY_IND - resp_ready_status=%d", *resp_ready_status);

			_qmi_sms_update_sms_type(resp_ready_status);
		}

		break;
	}

	case QMI_WMS_SET_EVENT_REPORT: {
		struct qmi_wms_set_event_report_resp_mt_msg* mt_msg = NULL;
		struct qmi_wms_set_event_report_resp_msg_mode* msg_mode = NULL;
		struct qmi_wms_set_event_report_resp_sms_on_ims* sms_on_ims = NULL;

		SYSLOG(LOG_OPERATION, "###sms### got QMI_WMS_SET_EVENT_REPORT");

		/* collect tlv */

		tlv = _get_tlv(msg, QMI_WMS_SET_EVENT_REPORT_RESP_TYPE_MT_MSG, sizeof(*mt_msg));
		if(tlv) {
			mt_msg = tlv->v;

			SYSLOG(LOG_DEBUG, "###sms### mt_msg->storage_type=%d", mt_msg->storage_type);
			SYSLOG(LOG_DEBUG, "###sms### mt_msg->storage_index=%d", mt_msg->storage_index);
		}

		tlv = _get_tlv(msg, QMI_WMS_SET_EVENT_REPORT_RESP_TYPE_MSG_MODE, sizeof(*msg_mode));
		if(tlv) {
			msg_mode = tlv->v;
			SYSLOG(LOG_DEBUG, "###sms### msg_mode->message_mode=%d", msg_mode->message_mode);
		}

		tlv = _get_tlv(msg, QMI_WMS_SET_EVENT_REPORT_RESP_TYPE_SMS_ON_IMS, sizeof(*sms_on_ims));
		if(tlv) {
			sms_on_ims = tlv->v;
			SYSLOG(LOG_DEBUG, "###sms### msg_mode->sms_on_ims=%d", sms_on_ims->sms_on_ims);
		}

		/* schedule sms */
		qmimgr_schedule_sms_readall();

		break;
	}

	default:
		SYSLOG(LOG_COMM, "unknown WMS msg(0x%04x) detected", msg->msg_id);
		break;
	}
}
#endif

void qmimgr_callback_on_pds(unsigned char msg_type, struct qmimsg_t* msg, unsigned short qmi_result, unsigned short qmi_error, int noti)
{
	const struct qmitlv_t* tlv;

	switch(msg->msg_id) {
	case QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT: {
		struct qmi_pds_set_gnss_engine_error_recovery_report_resp* resp;
		const char* reset_type;

		SYSLOG(LOG_OPERATION, "got QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT");

		tlv = _get_tlv(msg, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT_RESP_TYPE, sizeof(*resp));
		if(tlv) {
			resp = tlv->v;
			reset_type = resourcetree_lookup(res_qmi_strings, RES_STR_KEY(QMIPDS, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT, resp->reset_type));

			SYSLOG(LOG_OPERATION, "###gps### error reocvery (reset_type=%s)", reset_type);
			SYSLOG(LOG_OPERATION, "###gps### error reocvery (assist_delete_mask=0x%04x)", resp->assist_delete_mask);

			SYSLOG(LOG_OPERATION, "###gps### assist_delete_mask info %s", "• 0x00000001 – Clock information");
			SYSLOG(LOG_OPERATION, "###gps### assist_delete_mask info %s", "• 0x00000002 – Position information");
			SYSLOG(LOG_OPERATION, "###gps### assist_delete_mask info %s", "• 0x00000004 – SV directions");
			SYSLOG(LOG_OPERATION, "###gps### assist_delete_mask info %s", "• 0x00000008 – SV steering");
		}
		break;
	}

	case QMI_PDS_GPS_READY: {
		SYSLOG(LOG_OPERATION, "got QMI_PDS_GPS_READY");
		break;
	}

	case QMI_PDS_GET_GPS_SERVICE_STATE: {
		SYSLOG(LOG_OPERATION, "got QMI_PDS_GET_GPS_SERVICE_STATE");
		tlv = _get_tlv(msg, QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE, 1);
		if(tlv) {
			struct qmi_pds_get_gps_service_state_resp* resp;
			resp = (struct qmi_pds_get_gps_service_state_resp*)tlv->v;

			SYSLOG(LOG_OPERATION, "gps_service_state=%d,tracking_session_state=%d", resp->gps_service_state, resp->tracking_session_state);

			/* manual polling of gps not used */
#if 0
			if(!resp->gps_service_state) {
				SYSLOG(LOG_ERROR, "###gps### gps service disabled");
			} else {
				SYSLOG(LOG_ERROR, "###gps### gps service enabled");

				switch(resp->tracking_session_state) {
				case QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE_TSS_INACTIVE:
					SYSLOG(LOG_OPERATION, "###gps### stop determining (qmi tracking inactive)");
					_qmi_gps_determine(-1);
					break;

				case QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE_TSS_ACTIVE:
					SYSLOG(LOG_OPERATION, "###gps### qmi tracking active");
					break;

				default:
					SYSLOG(LOG_ERROR, "###gps### qmi unknown tracking status");
					break;
				}
			}
#endif
		}

		break;
	}

	case QMI_PDS_SET_EVENT_REPORT: {
		const struct qmi_pds_set_event_report_resp_position_session_status* pss = NULL;
		const struct qmi_pds_set_event_report_resp_parsed_position_data* ppd = NULL;
		const struct qmi_pds_set_event_report_resp_position_source* ps = NULL;
		const struct qmi_pds_set_event_report_resp_type_satellite_information* si = NULL;

		SYSLOG(LOG_OPERATION, "got QMI_PDS_SET_EVENT_REPORT");

		SYSLOG(LOG_OPERATION, "###qmimux### QMI_PDS_SET_EVENT_REPORT info.");

		/* get session staus */
		tlv = _get_tlv(msg, QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_POSITION_SESSION_STATUS, sizeof(*pss));
		if(tlv) {
			pss = tlv->v;
			SYSLOG(LOG_OPERATION, "###qmimux### session status=%d", pss->position_session_status);
		}

		/* get position data  */
		tlv = _get_tlv(msg, QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_PARSED_POSITION_DATA, sizeof(*ppd));
		if(tlv) {
			char dir[16];

			ppd = tlv->v;


			SYSLOG(LOG_OPERATION, "###gps### valid_mask=0x%08x", ppd->valid_mask);


			/* TODO: use instance instead of hard-code zero */
			if(ppd->valid_mask & 0x00000010) {
				SYSLOG(LOG_OPERATION, "###gps### latitude=%f", ppd->latitude);

				_set_str_db_ex("sensors.gps.0.assisted.latitude", convert_decimal_degree_to_degree_minute(ppd->latitude, 1, dir, sizeof(dir)), -1, 0);
				_set_str_db_ex("sensors.gps.0.assisted.latitude_direction", dir, -1, 0);
			}

			if(ppd->valid_mask & 0x00000020) {
				SYSLOG(LOG_OPERATION, "###gps### longitude=%f", ppd->longitude);

				_set_str_db_ex("sensors.gps.0.assisted.longitude", convert_decimal_degree_to_degree_minute(ppd->longitude, 0, dir, sizeof(dir)), -1, 0);
				_set_str_db_ex("sensors.gps.0.assisted.longitude_direction", dir, -1, 0);
			}

			if(ppd->valid_mask & 0x00000040) {
				_set_float_db_ex("sensors.gps.0.assisted.altitude", ppd->altitude_wrt_ellipsoid, NULL, 0);
				/* height_of_geoid not supported */
				_set_int_db_ex("sensors.gps.0.assisted.height_of_geoid", 0, NULL, 0);
			}

			if(ppd->valid_mask & 0x00000002) {
				SYSLOG(LOG_OPERATION, "###gps### timestamp_utc=%llu", ppd->timestamp_utc);
				struct tm tm_gps;
				time_t timestamp_utc_sec;

				char date_str[32];
				char time_str[32];

				/* convert ms to sec */
				timestamp_utc_sec = (time_t)(ppd->timestamp_utc / 1000ULL);

				/* get Y,M,D,H,M and S */
				gmtime_r(&timestamp_utc_sec, &tm_gps);

				snprintf(date_str, sizeof(date_str), "%02d%02d%02d", tm_gps.tm_mday, tm_gps.tm_mon + 1, (tm_gps.tm_year + 1900) % 100);
				snprintf(time_str, sizeof(time_str), "%02d%02d%02d", tm_gps.tm_hour, tm_gps.tm_min, tm_gps.tm_sec);

				_set_str_db_ex("sensors.gps.0.assisted.date", date_str, -1, 0);
				_set_str_db_ex("sensors.gps.0.assisted.time", time_str, -1, 0);
			}

			//if(ppd->valid_mask&0x00040000) {
			SYSLOG(LOG_OPERATION, "###gps### altitude_wrt_ellipsoid=%f", ppd->altitude_wrt_ellipsoid);
			SYSLOG(LOG_OPERATION, "###gps### altitude_wrt_sea_level=%f", ppd->altitude_wrt_sea_level);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_speed=%f", ppd->horizontal_speed);
			SYSLOG(LOG_OPERATION, "###gps### vertical_speed=%f", ppd->vertical_speed);
			SYSLOG(LOG_OPERATION, "###gps### heading=%f", ppd->heading);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_unc_circular=%f", ppd->horizontal_unc_circular);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_unc_ellipse_semi_major=%f", ppd->horizontal_unc_ellipse_semi_major);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_unc_ellipse_semi_minor=%f", ppd->horizontal_unc_ellipse_semi_minor);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_unc_ellipse_orient_azimuth=%f", ppd->horizontal_unc_ellipse_orient_azimuth);
			SYSLOG(LOG_OPERATION, "###gps### vertical_unc=%f", ppd->vertical_unc);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_vel_unc=%f", ppd->horizontal_vel_unc);
			SYSLOG(LOG_OPERATION, "###gps### vertical_vel_unc=%f", ppd->vertical_vel_unc);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_confidence=%d", ppd->horizontal_confidence);
			SYSLOG(LOG_OPERATION, "###gps### position_dop=%f", ppd->position_dop);
			SYSLOG(LOG_OPERATION, "###gps### horizontal_dop=%f", ppd->horizontal_dop);
			SYSLOG(LOG_OPERATION, "###gps### vertical_dop=%f", ppd->vertical_dop);
			SYSLOG(LOG_OPERATION, "###gps### position_op_mode=%d", ppd->position_op_mode);
			//}

			/*
			_set_float_db_ex("sensors.gps.0.assisted.LocUncP",0,NULL,0);
			_set_float_db_ex("sensors.gps.0.assisted.LocUncA",0,NULL,0);
			_set_float_db_ex("sensors.gps.0.assisted.LocUncAngle",0,NULL,0);
			_set_float_db_ex("sensors.gps.0.assisted.HEPE",0,NULL,0);
			_set_float_db_ex("sensors.gps.0.assisted.3d_fix",0,NULL,0);
			*/

		}

		/* get position source  */
		tlv = _get_tlv(msg, QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_POSITION_SOURCE, sizeof(*ps));
		if(tlv) {
			ps = tlv->v;
			SYSLOG(LOG_OPERATION, "###qmimux### position source=%d", ps->pds_position_source);
		}

		/* get satellite information */
		tlv = _get_tlv(msg, QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_SATELLITE_INFORMATION, sizeof(*si));
		if(tlv) {
			si = tlv->v;

			if(si->svns_len)
				SYSLOG(LOG_OPERATION, "###qmimux### svn_system=%d", si->e[0].svn_system);
		}

		SYSLOG(LOG_OPERATION, "done QMI_PDS_SET_EVENT_REPORT");

		break;
	}

	default:
		SYSLOG(LOG_COMM, "unknown PDS msg(0x%04x) detected", msg->msg_id);
		break;
	}
}

static int _qmi_band_set(const char* band);

void qmimgr_callback_on_nas(unsigned char msg_type, struct qmimsg_t* msg, unsigned short qmi_result, unsigned short qmi_error, int noti)
{
	const struct qmitlv_t* tlv;

	switch(msg->msg_id) {

#if 0
	case QMI_NAS_GET_OPERATOR_NAME_DATA: {
		//const char* resp;

		SYSLOG(LOG_OPERATION, "got QMI_NAS_GET_OPERATOR_NAME_DATA");

		tlv = _get_tlv(msg, QMI_NAS_GET_OPERATOR_NAME_DATA_RESP_TYPE_PLMN_NETWORK_NAME, 1);
		if(tlv) {
			_dump("QMI_NAS_GET_OPERATOR_NAME_DATA", tlv->v, tlv->l);
		}

		break;
	}
#endif


	case QMI_NAS_GET_SERVING_SYSTEM: {

		if(!is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
			SYSLOG(LOG_COMM, "got QMI_NAS_GET_SERVING_SYSTEM - feature not enabled (%s)", FEATUREHASH_CMD_SERVING_SYS);
			break;
		}

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_SERVING_SYSTEM");

		int reg_stat = 0xff, roaming_stat = 0xff;

#if defined (MODULE_MC7304)
		static  int last_valid_roaming_stat = 1; //default is deactive
#endif

		// QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE
		{
			struct qmi_nas_get_serving_system_resp* resp;
			int i;
			const char* dbvar;
			int if_num;

			// get resp
			tlv = _get_tlv(msg, QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE, sizeof(struct qmi_nas_get_serving_system_resp));
			if(tlv)
				resp = (struct qmi_nas_get_serving_system_resp*)tlv->v;
			else
				resp = NULL;

			// get interface 0
			if(resp) {
				unsigned char radio_if = resp->radio_if[0];
				if_num = resp->in_use_radio_if_list_num;

				switch(resp->ps_attach_stat) {

				case 1:
				case 2:
					_set_int_db("system_network_status.attached", (resp->ps_attach_stat == 1) ? 1 : 0, NULL);
					break;

				default:
					_set_reset_db("system_network_status.attached");
					break;


				}
				_set_idx_db("system_network_status.system_mode", RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, radio_if));
				_set_idx_db("system_network_status.service_type", RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, radio_if));

				//_set_int_db("system_network_status.reg_stat",resp->registration_state,NULL);
				reg_stat = resp->registration_state;

				/* store current service type */
				SYSLOG(LOG_DEBUG, "###sms### QMI_NAS_GET_SERVING_SYSTEM - set sms type (%d)", resp->registered_network);
#ifdef V_SMS_QMI_MODE_y

				_qmi_sms_init_sms_type(resp->registered_network);
				_qmi_sms_update_sms_type_based_on_firmware();
#endif
			} else {
				if_num = 0;

				_set_reset_db("system_network_status.attached");
				_set_reset_db("system_network_status.system_mode");
				_set_reset_db("system_network_status.service_type");
				//_set_reset_db("system_network_status.reg_stat");
			}

			// get all interfaces
			for(i = 0; i < QMI_MAX_INTERFACE; i++) {
				dbvar = _get_indexed_str("system_network_status.system_mode", i, NULL);
				if(i < if_num) {
					unsigned char radio_if = resp->radio_if[i];
					_set_idx_db(dbvar, RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, radio_if));
				} else {
					_set_reset_db(dbvar);
				}

				dbvar = _get_indexed_str("system_network_status.service_type", i, NULL);
				if(i < if_num) {
					unsigned char radio_if = resp->radio_if[i];
					_set_idx_db(dbvar, RES_STR_KEY(QMINAS, QMI_NAS_GET_SERVING_SYSTEM, radio_if));
				} else {
					_set_reset_db(dbvar);
				}
			}

		}

		// QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_ROAMING_INDICATOR
		tlv = _get_tlv(msg, QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_ROAMING_INDICATOR, sizeof(struct qmi_nas_get_serving_system_resp_type_roaming_indicator));
		if(tlv) {
			struct qmi_nas_get_serving_system_resp_type_roaming_indicator* resp = (struct qmi_nas_get_serving_system_resp_type_roaming_indicator*)tlv->v;
			_set_str_db("system_network_status.roaming", !resp->roaming_indicator ? "active" : "deactive", -1);
			roaming_stat = resp->roaming_indicator;
		} else {
			_set_reset_db("system_network_status.roaming");
		}

#if defined (MODULE_MC7304)
		if(reg_stat == 1 && roaming_stat != 0xff)
			last_valid_roaming_stat = roaming_stat;
#endif

		if(reg_stat == 0xFF) {
			_set_reset_db("system_network_status.reg_stat");
		} else {
			if(reg_stat == 1 && roaming_stat == 0) {   // in roaming
				_set_int_db("system_network_status.reg_stat", 5, NULL);
			}
#if defined (MODULE_MC7304)
			else if(reg_stat == 1 && roaming_stat == 0xff) {
				if(!last_valid_roaming_stat) { //IN roaming
					_set_int_db("system_network_status.reg_stat", 5, NULL);
				} else {
					_set_int_db("system_network_status.reg_stat", 1, NULL);
				}
			}
#endif
			else {
				_set_int_db("system_network_status.reg_stat", reg_stat, NULL);
			}
		}

		// QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_CURRENT_PLMN
                static int plmn_network_updated = 0; // only clear rdb if we're the one who has updated it
		tlv = _get_tlv(msg, QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_CURRENT_PLMN, sizeof(struct qmi_nas_get_serving_system_resp_type_current_plmn));
		if(tlv) {
			struct qmi_nas_get_serving_system_resp_type_current_plmn* resp = (struct qmi_nas_get_serving_system_resp_type_current_plmn*)tlv->v;

			_set_int_db("system_network_status.MCC", resp->mobile_country_code, NULL);
			_set_int_db("system_network_status.MNC", resp->mobile_network_code, NULL);
			if (is_printable(resp->network_description, resp->network_description_length)){
				_set_str_db("system_network_status.network", resp->network_description, resp->network_description_length);
                                plmn_network_updated = 1;
			}
		} else {
			_set_reset_db("system_network_status.MCC");
			_set_reset_db("system_network_status.MNC");
                        if (plmn_network_updated) {
			        _set_reset_db("system_network_status.network");
                                plmn_network_updated = 0;
                        }
		}

		// QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_CDMA_SYSTEM_ID
		tlv = _get_tlv(msg, QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_CDMA_SYSTEM_ID, sizeof(struct qmi_nas_get_serving_system_resp_type_cdma_system_id));
		if(tlv) {
			struct qmi_nas_get_serving_system_resp_type_cdma_system_id* resp = (struct qmi_nas_get_serving_system_resp_type_cdma_system_id*)tlv->v;

			_set_int_db("system_network_status.SID", resp->system_id, NULL);
			_set_int_db("system_network_status.NID", resp->network_id, NULL);
		} else {
			_set_reset_db("system_network_status.SID");
			_set_reset_db("system_network_status.NID");
		}

		// QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_UMTS_PSC
		{
			unsigned short * resp_umts_psc = NULL;
			unsigned short new_psc = 0, old_psc = 0;
			const char* str_old_psc;
			int empty_string = 0;

			str_old_psc = _get_str_db("system_network_status.PSCs0", "");

			if(!str_old_psc[0])
				empty_string = 1;
			else
				old_psc = _get_int_db("system_network_status.PSCs0", 0);

			tlv = _get_tlv(msg, QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_UMTS_PSC, sizeof(*resp_umts_psc));
			if(tlv) {
				resp_umts_psc = (unsigned short *)tlv->v;
				new_psc = *resp_umts_psc;

				if(empty_string == 1 || old_psc != new_psc) {
					_set_int_db("system_network_status.PSCs0", new_psc, NULL);
				}
			}
			else {
				if(empty_string == 0) // To avoid unnecessary rdb set.
					_set_reset_db("system_network_status.PSCs0");
			}
		}
		break;
	}

	case QMI_NAS_GET_RF_BAND_INFO: {
		struct qmi_nas_get_rf_band_info_resp* resp;
		int num_instances;
		const char* dbvar;
		unsigned short active_band;
		unsigned short active_channel;
		int i;

		if(!is_enabled_feature(FEATUREHASH_CMD_BANDSTAT)) {
			SYSLOG(LOG_COMM, "got QMI_NAS_GET_RF_BAND_INFO - feature not enabled (%s)", FEATUREHASH_UNSPECIFIED);
			break;
		}

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_RF_BAND_INFO");

		tlv = _get_tlv(msg, QMI_NAS_GET_RF_BAND_INFO_RESP_TYPE, sizeof(struct qmi_nas_get_rf_band_info_resp));
		if(tlv) {
			resp = (struct qmi_nas_get_rf_band_info_resp*)tlv->v;
			num_instances = resp->num_instances;
		} else {
			resp = NULL;
			num_instances = 0;
		}

		/* if any instance exists */
		if(num_instances > 0) {
			active_band = resp->interfaces[0].active_band;
			active_channel = resp->interfaces[0].active_channel;

			SYSLOG(LOG_COMM, "active band = %d / channel = %d", active_band, active_channel);

			_set_idx_db("system_network_status.current_band", RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, active_band));
			_set_int_db("system_network_status.ChannelNumber", active_channel, NULL);
		} else {
			_set_reset_db("system_network_status.current_band");
			_set_reset_db("system_network_status.ChannelNumber");
		}

		for(i = 0; i < QMI_MAX_INTERFACE; i++) {

			if(i < num_instances) {
				active_band = resp->interfaces[i].active_band;
				active_channel = resp->interfaces[i].active_channel;

				SYSLOG(LOG_COMM, "interface = %d / active band = %d / channel = %d", i, active_band, active_channel);

				dbvar = _get_indexed_str("system_network_status.current_band", i, NULL);
				_set_idx_db(dbvar, RES_STR_KEY(QMINAS, QMI_NAS_GET_RF_BAND_INFO, active_band));

				dbvar = _get_indexed_str("system_network_status.channel", i, NULL);
				_set_int_db(dbvar, active_channel, NULL);

			} else {
				dbvar = _get_indexed_str("system_network_status.current_band", i, NULL);
				_set_reset_db(dbvar);

				dbvar = _get_indexed_str("system_network_status.ChannelNumber", i, NULL);
				_set_reset_db(dbvar);
			}
		}

		break;
	}

	case QMI_NAS_GET_SYS_INFO: {

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_SYS_INFO");

		{ //RAC(Routing Area Code)
			unsigned char * resp_wcdma_rac = NULL, * resp_gsm_rac = NULL;
			unsigned char new_rac = 0, old_rac = 0;
			const char* str_old_rac;
			int empty_string = 0;

			str_old_rac = _get_str_db("system_network_status.RAC", "");

			if(!str_old_rac[0])
				empty_string = 1;
			else
				old_rac = _get_int_db("system_network_status.RAC", 0);

			tlv = _get_tlv(msg, QMI_NAS_GET_SYS_INFO_RESP_TYPE_GSM_RAC, sizeof(*resp_gsm_rac));
			if(tlv)
				resp_gsm_rac = (unsigned char *)tlv->v;

			tlv = _get_tlv(msg, QMI_NAS_GET_SYS_INFO_RESP_TYPE_WCDMA_RAC, sizeof(*resp_wcdma_rac));
			if(tlv)
				resp_wcdma_rac = (unsigned char *)tlv->v;

			if(resp_gsm_rac != NULL)
				new_rac = *resp_gsm_rac;
			if(resp_wcdma_rac != NULL)
				new_rac = *resp_wcdma_rac;

			if(resp_gsm_rac == NULL && resp_wcdma_rac == NULL) {
				if(empty_string == 0) // To avoid unnecessary rdb set.
					_set_reset_db("system_network_status.RAC");
			}
			else if(empty_string == 1 || old_rac != new_rac) {
				_set_int_db("system_network_status.RAC", (int) new_rac, NULL);
			}
		} //RAC(Routing Area Code)
		break;
	}

	case QMI_NAS_GET_SIG_INFO:
	case QMI_NAS_SIG_INFO_IND: {

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_SIG_INFO");

		if(!is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH)) {
			SYSLOG(LOG_COMM, "got QMI_NAS_GET_SIG_INFO - feature not enabled (%s)", FEATUREHASH_CMD_SIGSTRENGTH);
			break;
		}

		int sigstrength;
		int new_lte_rsrp = 0, old_lte_rsrp = 0;
		int new_lte_rsrq = 0, old_lte_rsrq = 0;
		int new_wcdma_rscp = 0, old_wcdma_rscp = 0;
		int new_rssi = 0, old_rssi = 0;
		int new_ecio = 0, old_ecio = 0;

		struct qmi_nas_get_sig_info_resp_cdma* resp_cdma = NULL;
		struct qmi_nas_get_sig_info_resp_hdr* resp_hdr = NULL;
		struct qmi_nas_get_sig_info_resp_gsm* resp_gsm = NULL;
		struct qmi_nas_get_sig_info_resp_wcdma* resp_wcdma = NULL;
		struct qmi_nas_get_sig_info_resp_lte* resp_lte = NULL;
		struct qmi_nas_get_sig_info_resp_tdscdma* resp_tdscdma = NULL;
		struct qmi_nas_get_sig_info_resp_tdscdma_ext* resp_tdscdma_ext = NULL;

		struct tlv_to_struc_ptr_table_t {
			int tlv;
			int tlv_len;
			void* tlv_ptr;
		};

		struct tlv_to_struc_ptr_table_t tlv_to_struc_ptr_table[] = {
			{QMI_NAS_GET_SIG_INFO_RESP_TYPE_CDMA, sizeof(*resp_cdma), &resp_cdma},
			{QMI_NAS_GET_SIG_INFO_RESP_TYPE_HDR, sizeof(*resp_hdr), &resp_hdr},
			{QMI_NAS_GET_SIG_INFO_RESP_TYPE_GSM, sizeof(*resp_gsm), &resp_gsm},
			{QMI_NAS_GET_SIG_INFO_RESP_TYPE_WCDMA, sizeof(*resp_wcdma), &resp_wcdma},
			{QMI_NAS_GET_SIG_INFO_RESP_TYPE_LTE, sizeof(*resp_lte), &resp_lte},
			{QMI_NAS_GET_SIG_INFO_RESP_TYPE_TDSCDMA, sizeof(*resp_tdscdma), &resp_tdscdma},
			{QMI_NAS_GET_SIG_INFO_RESP_TYPE_TDSCDMA_EXT, sizeof(*resp_tdscdma_ext), &resp_tdscdma_ext},
			{0, 0, NULL}
		};


		struct tlv_to_struc_ptr_table_t* p;

		/* fill up tlv structures */
		p = tlv_to_struc_ptr_table;

		while(p->tlv) {
			tlv = _get_tlv(msg, p->tlv, p->tlv_len);
			if(tlv) {
				SYSLOG(LOG_COMM, "###sigstrength### got tlv(0x%02x) from QMI_NAS_GET_SIG_INFO", p->tlv);
				*(void**)p->tlv_ptr = tlv->v;
			}

			p++;
		}

		sigstrength = 0;

		old_lte_rsrp = _get_int_db("signal.0.rsrp", 0);
		old_lte_rsrq = _get_int_db("signal.rsrq", 0);
		old_wcdma_rscp = _get_int_db("radio.information.rscp0", 0);
		old_rssi = _get_int_db("radio.information.rssi", 0);
		old_ecio = _get_int_db("radio.information.ecio0", 0);

		if(resp_cdma) {
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (cdma) - rssi=%d", resp_cdma->rssi);
			sigstrength = resp_cdma->rssi;
			new_rssi = resp_cdma->rssi;
			new_ecio = (-resp_cdma->ecio) / 2 + 0.5;
		}

		if(resp_hdr) {
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (hdr) - rssi=%d", resp_hdr->rssi);
			sigstrength = resp_hdr->rssi;
			new_rssi = resp_hdr->rssi;
			new_ecio = (-resp_hdr->ecio) / 2 + 0.5;
		}

		if(resp_gsm) {
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (gsm) - rssi=%d", resp_gsm->gsm_sig_info);
			sigstrength = resp_gsm->gsm_sig_info;
			new_rssi = resp_gsm->gsm_sig_info;
		}

		if(resp_wcdma) {
			int ecio = (-resp_wcdma->ecio) / 2 + 0.5;
			int rssi = resp_wcdma->rssi;
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (wcdma) - rssi=%d", resp_wcdma->rssi);
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (wcdma) - ecio=%d", ecio);
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (wcdma) - rscp=%d", rssi + ecio);
			//rscp is used for signal strength in 3G, instead of rssi.
			new_wcdma_rscp = sigstrength = rssi + ecio;
			new_rssi = resp_wcdma->rssi;
			new_ecio = (-resp_wcdma->ecio) / 2 + 0.5;
		}

		if(resp_tdscdma) {
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (tdscdma) - rscp=%d", resp_tdscdma->rscp);
			sigstrength = resp_tdscdma->rscp;
		}

		if(resp_tdscdma_ext) {
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (tdscdma_ext) - rssi=%d", (int)resp_tdscdma_ext->rssi);
			sigstrength = resp_tdscdma_ext->rssi;
			new_ecio = (-resp_tdscdma_ext->ecio) / 2 + 0.5;
		}

		if(resp_lte) {
			SYSLOG(LOG_COMM, "###sigstrength### QMI_NAS_GET_SIG_INFO (lte) - rsrp=%d", resp_lte->rsrp);

			/*
				As LTE network hands over based on RSRP/RSRQ, we are using RSRP as signal strength.
				RSRP as signal strengh is also requirement of Telstra
			*/

			sigstrength = resp_lte->rsrp;

			new_lte_rsrp = resp_lte->rsrp;
			new_lte_rsrq = resp_lte->rsrq;
			new_rssi = resp_lte->rssi;
		}

		SYSLOG(LOG_COMM, "###sigstrength### sigstrength=%d", sigstrength);

		if(sigstrength)
			_set_int_db("radio.information.signal_strength", sigstrength, "dBm");
		else
			_set_reset_db("radio.information.signal_strength");

		if(old_lte_rsrp != new_lte_rsrp) {
			if(new_lte_rsrp == 0)
				_set_reset_db("signal.0.rsrp");
			else
				_set_int_db("signal.0.rsrp", new_lte_rsrp, NULL);

		}
		if(old_lte_rsrq != new_lte_rsrq) {
			if(new_lte_rsrq == 0)
				_set_reset_db("signal.rsrq");
			else
				_set_int_db("signal.rsrq", new_lte_rsrq, NULL);

		}
		if(old_wcdma_rscp != new_wcdma_rscp) {
			if(new_wcdma_rscp == 0)
				_set_reset_db("radio.information.rscp0");
			else
				_set_int_db("radio.information.rscp0", new_wcdma_rscp, NULL);

		}
		if(old_rssi != new_rssi) {
			if(new_rssi == 0)
				_set_reset_db("radio.information.rssi");
			else
				_set_int_db("radio.information.rssi", new_rssi, NULL);
		}
		if(old_ecio != new_ecio) {
			if(new_ecio == 0)
				_set_reset_db("radio.information.ecio0");
			else
				_set_int_db("radio.information.ecio0", new_ecio, NULL);
		}
		break;
	}

	case QMI_NAS_GET_SIGNAL_STRENGTH: {
		struct qmi_nas_get_signal_strength_resp_sig_strength* resp_sig_strength;

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_SIGNAL_STRENGTH");

		if(!is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH)) {
			SYSLOG(LOG_COMM, "got QMI_NAS_GET_SIGNAL_STRENGTH - feature not enabled (%s)", FEATUREHASH_UNSPECIFIED);
			break;
		}

		// get TLV - QMI_NAS_GET_SIGNAL_STRENGTH_RESP_TYPE_SIG_STRENGTH
		tlv = _get_tlv(msg, QMI_NAS_GET_SIGNAL_STRENGTH_RESP_TYPE_SIG_STRENGTH, sizeof(*resp_sig_strength));
		if(tlv) {

			/*
				Received signal strength in dBm
					For CDMA and UMTS, forward link pilot Ec
					For GSM, the received signal strength
					For LTE, this indicates the total received wideband power observed by UE

				radio_if 1 Radio interface technology of the signal being measured
					0x00 – None (no service)
					0x01 – cdma2000 1X
					0x02 – cdma2000 HRPD (1xEV-DO)
					0x03 – AMPS
					0x04 – GSM
					0x05 – UMTS
					0x08 – LTE
			*/
			// get qmi sim status
			resp_sig_strength = (struct qmi_nas_get_signal_strength_resp_sig_strength*)tlv->v;

			SYSLOG(LOG_COMM, "result of QMI_NAS_GET_SIGNAL_STRENGTH - tlv->sig_strength=%d,radio_if=%d", resp_sig_strength->sig_strength, resp_sig_strength->radio_if);
			_set_int_db("radio.information.signal_strength", resp_sig_strength->sig_strength, "dBm");
		}

		break;
	}

	case QMI_NAS_GET_CELL_LOCATION_INFO: {

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_CELL_LOCATION_INFO");

		/*
		 * This is non-trivial, it's in a separate file.
		 * this file is already too big.
		 */
		process_qmi_cell_info(msg);
		break;
	}

	case QMI_NAS_GET_CDMA_POSITION_INFO: {
		struct qmi_nas_get_cdma_position_info_resp_type* resp;
		int i;

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_CDMA_POSITION_INFO");

		/* get position info */
		tlv = _get_tlv(msg, QMI_NAS_GET_CDMA_POSITION_INFO_RESP_TYPE, sizeof(*resp));
		if(tlv) {
			int pilot;

			resp = (struct qmi_nas_get_cdma_position_info_resp_type*)tlv->v;

			/* search current pilot */
			for(i = 0, pilot = -1; i < resp->bs_len && pilot < 0 ; i++) {
				/*
					Pilot information type. Values:
						• 0x00 – NAS_CDMA_PILOT_CURR_ACT_PLT – Current active pilot information
						• 0x01 – NAS_CDMA_PILOT_NEIGHBOR_PLT – Neighbor pilot information
				*/
				if(resp->bs[i].pilot_type == 0)
					pilot = i;
			}

			if(!(pilot < 0)) {
				_set_int_db("system_network_status.pn", resp->bs[pilot].pilot_pn, NULL);
			} else {
				_set_reset_db("system_network_status.pn");
			}

		}

		break;
	}

	case QMI_NAS_NETWORK_REJECT_IND: {
		struct qmi_nas_network_reject_ind* resp;

		SYSLOG(LOG_COMM, "got QMI_NAS_NETWORK_REJECT_IND");

		/* get network registration cause */
		tlv = _get_tlv(msg, QMI_NAS_NETWORK_REJECT_IND_TYPE_REGISTRATION_REJECTION_CAUSE, sizeof(*resp));
		if(tlv) {
			resp = (struct qmi_nas_network_reject_ind*)tlv->v;
			if (resp) {
				_set_int_db("system_network_status.rej_cause", resp->rej_cause, NULL);
			}
			else {
				SYSLOG(LOG_WARNING, "rej_cause is not received");
			}
		}
		else {
			SYSLOG(LOG_WARNING, "Registration Rejection Cause TLV is not received");
		}

		break;
	}

	case QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO: {
		struct qmi_nas_get_3gpp2_subscription_info_resp_type_channel* resp_ch;
		struct qmi_nas_get_3gpp2_subscription_info_resp_type_mdn* resp_mdn;

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO");

		/* bypass if the feature is not enabled */
		if(!is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
			SYSLOG(LOG_COMM, "got QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO - feature not enabled (%s)", FEATUREHASH_CMD_SERVING_SYS);
			break;
		}

		/* get channels */
		tlv = _get_tlv(msg, QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_RESP_TYPE_CHANNEL, sizeof(*resp_ch));
		if(tlv) {
			resp_ch = (struct qmi_nas_get_3gpp2_subscription_info_resp_type_channel*)tlv->v;

			/* TODO: set channel RDB variables */
		}

		/* get mdn */
		tlv = _get_tlv(msg, QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_RESP_TYPE_MDN, sizeof(*resp_mdn));
		if(tlv) {
			resp_mdn = (struct qmi_nas_get_3gpp2_subscription_info_resp_type_mdn*)tlv->v;

			_set_str_db("module_info.cdma.MDN", resp_mdn->mdn, resp_mdn->mdn_len);
		} else {
			_set_reset_db("module_info.cdma.MDN");
		}

		break;
	}

	case QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE: {
		const unsigned short * resp_mode = NULL;
		const struct qmi_band_bit_mask * resp_band = NULL,
		                               * resp_lte_band = NULL;
		char cur_band[QMIMGR_MAX_DB_BIGVALUE_LENGTH];
		int ret;
		const char * band;

		SYSLOG(LOG_COMM, "got QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE");
		tlv = _get_tlv(msg,
		           QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE_RESP_TYPE_MODE_PREF,
		           sizeof(*resp_mode));
		if(tlv) {
			resp_mode = (const unsigned short *)tlv->v;
			SYSLOG(LOG_DEBUG, "mode_pref=%02x", *resp_mode);
		} else {
			SYSLOG(LOG_WARNING, "mode_pref tlv not received");
		}
		tlv = _get_tlv(msg,
		           QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE_RESP_TYPE_BAND_PREF,
		           sizeof(*resp_band));
		if(tlv) {
			resp_band = (const struct qmi_band_bit_mask *)tlv->v;
			SYSLOG(LOG_DEBUG, "band_pref=%llx", resp_band->band);
		} else {
			SYSLOG(LOG_WARNING, "band_pref tlv not received");
		}
		tlv = _get_tlv(msg,
		       QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE_RESP_TYPE_LTE_BAND_PREF,
		       sizeof(*resp_lte_band));
		if(tlv) {
			resp_lte_band = (const struct qmi_band_bit_mask *)tlv->v;
			SYSLOG(LOG_DEBUG, "lte_band_pref=%llx", resp_lte_band->band);
		} else {
			SYSLOG(LOG_WARNING, "lte_band_pref tlv not received");
		}

		ret = _qmi_mode_band_to_curband(resp_mode, resp_band, resp_lte_band,
		                                cur_band, __countof(cur_band));
		if(ret >= 0) {
			SYSLOG(LOG_DEBUG, "writing rdb current_selband=%s", cur_band);
			_set_str_db("currentband.current_selband", cur_band, -1);
			if(ret) {
				// current band selection is not allowed by custom band setting
				// force it back to allowed range
				band = custom_band_qmi[custom_band_qmi_len-1].name;
				SYSLOG(LOG_NOTICE, "enforce band selection to %s", band);
				_qmi_band_set(band);
			}
		} else {
			_set_reset_db("currentband.current_selband");
		}
		break;
	}

	default:
		SYSLOG(LOG_COMM, "unknown NAS msg(0x%04x) detected", msg->msg_id);
		break;
	}
}

void atmgr_callback_on_noti(struct port_t* port, const char* noti)
{
	SYSLOG(LOG_OPERATION, "noti - %s", noti);
}

char* _get_at_result(struct strqueue_t* resultq, int line_count, int result_idx, const char* prefix)
{
	const char* cmd;
	const char* result_line;
	struct strqueue_element_t* el;

	char* result;

	if(!resultq) {
		SYSLOG(LOG_ERROR, "result not specified - prefix=%s", prefix);
		goto err;
	}

	// get cmd
	el = strqueue_walk_to(resultq, 0);
	if(!el) {
		SYSLOG(LOG_ERROR, "result echo command missing - prefix=%s", prefix);
		goto err;
	}

	cmd = el->str;

	// get result
	el = strqueue_walk_to(resultq, result_idx);
	if(!el) {
		SYSLOG(LOG_ERROR, "result missing in at command result - cmd=%s,prefix=%s", cmd, prefix);
		goto err;
	}

	result_line = el->str;

	// check result line count
	if(resultq->active_count < line_count) {
		SYSLOG(LOG_ERROR, "at command result count not matched - cmd=%s,line_count=%d,active_count=%d", cmd, line_count, resultq->active_count);
		goto err;
	}

	// get result
	result = strstr(result_line, prefix);
	if(!result) {
		SYSLOG(LOG_ERROR, "at command result prefix not found - cmd=%s,prefix=%s", cmd, prefix);
		goto err;
	}

	result += strlen(prefix);

	return result;

err:
	return NULL;
}

void atmgr_callback_on_common(struct port_t* port, unsigned short tran_id, struct strqueue_t* resultq, int timeout)
{
	struct strqueue_element_t* el;
	int atcmd_idx;

	struct strqueue_element_t* els[STRQUEUE_MAX_STR];
	int i;

	const char* cmd;

	SYSLOG(LOG_OPERATION, "at result tran_id=%d", tran_id);

	// get total elements count
	SYSLOG(LOG_OPERATION, "total result line = %d", resultq->active_count);

	// get echo
	el = strqueue_walk_first(resultq);
	if(!el) {
		SYSLOG(LOG_ERROR, "empty result");
		return;
	}

	// walk through elements
	i = 0;
	while(el) {
		els[i++] = el;

		SYSLOG(LOG_OPERATION, "at result - %s", el->str);
		el = strqueue_walk_next(resultq);
	}


	// get at command idx
	cmd = els[0]->str;
	atcmd_idx = dbhash_lookup(atcmds, cmd);

	switch(atcmd_idx) {
	case AT_COMMAND_VAR_CPIN: {

		const char* cpin;
		const char* pin_stat;
		/*
			AT+CPIN?
			+CPIN: READY

			OK
		*/

		SYSLOG(LOG_OPERATION, "got AT_COMMAND_VAR_CPIN");

		if(resultq->active_count != 4) {
			SYSLOG(LOG_ERROR, "incorrect result for AT_COMMAND_VAR_CPIN");
			goto at_command_var_cpin_err;
		}

		cpin = els[1]->str;
		pin_stat = strstr(cpin, "+CPIN: ");
		if(!pin_stat) {
			SYSLOG(LOG_ERROR, "+CPIN not found in result");
			goto at_command_var_cpin_err;
		}

		pin_stat += strlen("+CPIN: ");
		if(!strcmp(pin_stat, "READY"))
			pin_stat = "SIM OK";

		_set_str_db("sim.status.status", pin_stat, -1);
		break;

at_command_var_cpin_err:
		_set_reset_db("sim.status.status");
		break;
	}

	case AT_COMMAND_VAR_HINT: {
		char* crsm;
		char* pos1;
		char* pos2;
		int i;
		int j;
		int ascii;

		char value[QMIMGR_MAX_DB_VALUE_LENGTH];
		char encoded_hint[QMIMGR_MAX_DB_VALUE_LENGTH];

		SYSLOG(LOG_OPERATION, "got AT_COMMAND_VAR_HINT");

		/*
			AT+CRSM=176,28486,0,0,17
			+CRSM: 144,0,"0254656C73747261FFFFFFFFFFFFFFFFFF"

			OK
		*/

		if(resultq->active_count != 4) {
			SYSLOG(LOG_ERROR, "incorrect result for AT_COMMAND_VAR_HINT");
			goto at_command_var_cimi_err;
		}

		crsm = els[1]->str;

		// get result prefix
		pos1 = strstr(crsm, "+CRSM:");
		if(!pos1) {
			SYSLOG(LOG_ERROR, "CRSM not found in the result");
			goto at_command_var_hint_err;
		}

		// get start of encoding
		pos2 = strstr(pos1, ",\"");
		if(!pos2 || strlen(pos2) <= 5) {
			SYSLOG(LOG_ERROR, "start of encoding not found");
			goto at_command_var_hint_err;
		}

		// get end of encoding
		pos1 = strchr(pos2 + 2, '\"');
		if(!pos1) {
			SYSLOG(LOG_ERROR, "end of encoding not found");
			goto at_command_var_hint_err;
		}

		*encoded_hint = 0;
		*pos1 = 0;
		pos1 = pos2 + 2;
		for(i = 0, j = 0; i < strlen(pos2) - 2; i += 2) {
			if(*pos1 == 'F' || *pos1 == 'f') break;
			ascii = (__hex(*(pos1 + i)) << 4) + __hex(*(pos1 + i + 1));
			if(i == 0 && !isgraph(ascii)) continue;
			if((j == 0 && ascii == '<') || (ascii == 0xff)) break; //<No file on SIM>
			sprintf(encoded_hint + strlen(encoded_hint), "%%%02x", ascii);
			if(ascii == '\r' || ascii == '\n')
				value[j++] = ' ';
			else
				value[j++] = ascii;
		}
		value[j] = 0;

		// set results
		_set_str_db("system_network_status.hint", value, sizeof(value));
		_set_str_db("system_network_status.hint.encoded", encoded_hint, sizeof(encoded_hint));

		break;

at_command_var_hint_err:
		_set_reset_db("system_network_status.hint");
		_set_reset_db("system_network_status.hint.encoded");
		break;
	}

	case AT_COMMAND_VAR_CIMI: {
		char mcc[4];
		char mnc[4];
		const char* imsi;
		int mccmnc_len;

		SYSLOG(LOG_OPERATION, "got AT_COMMAND_VAR_CIMI");

		/*
			at+CIMI
			505013434043185

			OK
		*/

		if(resultq->active_count != 4) {
			SYSLOG(LOG_ERROR, "incorrect result for at+CIMI");
			goto at_command_var_cimi_err;
		}

		imsi = els[1]->str;

		SYSLOG(LOG_OPERATION, "imsi = %s", imsi);

		if(_get_mcc_mnc(imsi, strlen(imsi), mcc, mnc) < 0) {
			SYSLOG(LOG_ERROR, "incorret format of IMSI - %s", imsi);
			goto at_command_var_cimi_err;
		}


		// set mnc
		_set_str_db("imsi.plmn_mcc", mcc, sizeof(mcc));
		_set_str_db("imsi.plmn_mnc", mnc, sizeof(mnc));

		// set msin
		mccmnc_len = strlen(mcc) + strlen(mnc);
		_set_str_db("imsi.msin", imsi + mccmnc_len, -1);
		break;

at_command_var_cimi_err:
		_set_reset_db("imsi.plmn_mcc");
		_set_reset_db("imsi.plmn_mnc");
		_set_reset_db("imsi.msin");

		break;
	}

	default:
		SYSLOG(LOG_ERROR, "unknown at command - %s", cmd);
		break;
	}
}

void qmimgr_callback_on_common(struct qmiuniclient_t* uni, int serv_id, unsigned char msg_type, unsigned short tran_id, struct qmimsg_t* msg)
{

	unsigned short qmi_result;
	unsigned short qmi_error;
	int qmi_noti;

	// preprocess
	if(qmimgr_callback_on_preprocess(uni, msg_type, tran_id, msg, &qmi_result, &qmi_error, NULL, &qmi_noti) < 0) {
		SYSLOG(LOG_ERROR, "invalid msg detected");
		goto err;
	}

	// call each handler
	switch(serv_id) {
	case QMICTL:
		SYSLOG(LOG_DEBUG, "got QMICTL - no default handler");
		break;

	case QMIIPV6:
		SYSLOG(LOG_DEBUG, "got QMIIPV6");
		qmimgr_callback_on_ipv6(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;

	case QMIWDS:
		SYSLOG(LOG_DEBUG, "got QMIWDS");
		qmimgr_callback_on_wds(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;

	case QMIDMS:
		SYSLOG(LOG_DEBUG, "got QMIDMS");
		qmimgr_callback_on_dms(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;
	case QMINAS:
		SYSLOG(LOG_DEBUG, "got QMINAS");
		qmimgr_callback_on_nas(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;

	case QMIPDS:
		SYSLOG(LOG_DEBUG, "got QMIPDS");
		qmimgr_callback_on_pds(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;

#ifdef V_SMS_QMI_MODE_y
	case QMIWMS:
		SYSLOG(LOG_DEBUG, "got QMIWMS");
		qmimgr_callback_on_wms(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;
#endif

#ifdef QMI_VOICE_y
	case QMIVOICE:
		SYSLOG(LOG_DEBUG, "got QMIVOICE");
		qmimgr_callback_on_voice(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;
#endif

	case QMIUIM:
		SYSLOG(LOG_DEBUG, "got QMIUIM");
		qmimgr_callback_on_uim(msg_type, msg, qmi_result, qmi_error, qmi_noti, tran_id);
		break;

	case QMILOC:
		SYSLOG(LOG_DEBUG, "got QMILOC");
		qmimgr_callback_on_loc(msg_type, msg, qmi_result, qmi_error, qmi_noti);
		break;
	default:
		SYSLOG(LOG_ERROR, "unknown service id (%d) detected", serv_id);
		break;
	}

err:
	__noop;
}

void qmimgr_callback_on_schedule_generic_at(struct funcschedule_element_t* element)
{

#if 0
	unsigned short tran_id;

	///////////// implemented but not used /////////////
	// cpin
	SYSLOG(LOG_DEBUG, "request AT+CPIN?");
	_request_at("AT+CPIN?", QMIMGR_AT_RESP_TIMEOUT, &tran_id);
#endif

	// scheduling
	SYSLOG(LOG_DEBUG, "adding AT generic schedule - %d sec", QMIMGR_GENERIC_SCHEDULE_PERIOD);
	funcschedule_add(sched, 0, QMIMGR_GENERIC_SCHEDULE_PERIOD, qmimgr_callback_on_schedule_generic_at, NULL);
}

void qmimgr_callback_on_schedule_sim_card_at(struct funcschedule_element_t* element)
{
	unsigned short tran_id;
	const char* sim_stat;

	sim_stat = _get_str_db("sim.status.status", "");
	if(strcmp(sim_stat, "SIM OK")) {
		SYSLOG(LOG_DEBUG, "SIM not ready - sim_stat=%s", sim_stat);
		goto fini;
	}

	// request MCC and MNC
	if(!*_get_str_db("imsi.msin", "")) {
		SYSLOG(LOG_DEBUG, "request AT+CIMI");
		// cimi
		_request_at("AT+CIMI", QMIMGR_AT_RESP_TIMEOUT, &tran_id);
	}

	if(!*_get_str_db("system_network_status.hint", "")) {
		SYSLOG(LOG_DEBUG, "AT+CRSM=176,28486,0,0,17");
		// hint
		_request_at("AT+CRSM=176,28486,0,0,17", QMIMGR_AT_RESP_TIMEOUT, &tran_id);
	}

fini:
	// scheduling
	SYSLOG(LOG_DEBUG, "adding sim card AT schedule - %d sec", QMIMGR_GENERIC_SCHEDULE_PERIOD);
	funcschedule_add(sched, 0, QMIMGR_GENERIC_SCHEDULE_PERIOD, qmimgr_callback_on_schedule_sim_card_at, NULL);
}

int _qmi_dms_get_iccid(int wait)
{
	struct qmimsg_t * msg;
	unsigned short tran_id;

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to allocate msg - QMI_DMS_UIM_GET_ICCID");
		goto err;
	}

	// QMI_DMS_UIM_GET_ICCID - Sierra specific CCID (MCC and MCC)
	if(_request_qmi(msg, QMIDMS, QMI_DMS_UIM_GET_ICCID, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - QMI_DMS_UIM_GET_ICCID");
		goto err;
	}

	if(wait && _wait_qmi_until(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
    }

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_dms_get_imsi(int wait)
{
	struct qmimsg_t * msg;
	unsigned short tran_id;

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to allocate msg - QMI_DMS_GET_IMSI");
		goto err;
	}

	// QMI_DMS_GET_IMSI
	if(_request_qmi(msg, QMIDMS, QMI_DMS_GET_IMSI, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - QMI_DMS_GET_IMSI");
		goto err;
	}

	if(wait && _wait_qmi_until(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
    }

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

void qmimgr_callback_on_schedule_sim_card_qmi(struct funcschedule_element_t* element)
{
	struct qmimsg_t* msg;
	unsigned short tran_id;
	const char* sim_stat;

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to allocate msg for scheduled msgs");
		goto fini;
	}

	SYSLOG(LOG_DEBUG, "schedule triggered");

	sim_stat = _get_str_db("sim.status.status", "");
	if(strcmp(sim_stat, "SIM OK")) {
		SYSLOG(LOG_DEBUG, "SIM not ready - sim_stat=%s", sim_stat);
		goto fini;
	}

	if(!*_get_str_db("imsi.msin", "")) {
		SYSLOG(LOG_DEBUG, "request get_imsi");
		_qmi_get_imsi(0);
	}

	if(!*_get_str_db("system_network_status.simICCID", "")) {
		SYSLOG(LOG_DEBUG, "request get_iccid");
		_qmi_get_iccid(0);
	}

	if(!*_get_str_db("sim.data.msisdn", "")) {
		SYSLOG(LOG_DEBUG, "request QMI_DMS_GET_MSISDN");

		// DMS - QMI_DMS_GET_MSISDN request
		_request_qmi(msg, QMIDMS, QMI_DMS_GET_MSISDN, &tran_id);
	}

fini:
	qmimsg_destroy(msg);

	// scheduling
	SYSLOG(LOG_DEBUG, "adding sim card schedule - %d sec", QMIMGR_GENERIC_SCHEDULE_PERIOD);
	funcschedule_add(sched, 0, QMIMGR_GENERIC_SCHEDULE_PERIOD, qmimgr_callback_on_schedule_sim_card_qmi, NULL);
}

void update_heartbeat(int force)
{
	static unsigned long long beat = 0;
	char buff[QMIMGR_MAX_DB_VALUE_LENGTH];

	static clock_t heart_beat_tick = 0;
	clock_t cur;
	int expired;

	/* get expiry flag */
	cur = _get_current_sec();
	expired = !((cur - heart_beat_tick) < QMIMGR_GENERIC_SCHEDULE_PERIOD);

	/* only update under the condition */
	if(!heart_beat_tick || force || expired) {
		beat++;
		snprintf(buff, sizeof(buff), "%lld", beat);

		// heart beat for qmimgr
		_set_str_db("heart_beat.qmimgr", buff, -1);
		if(is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
			// heart beat - essential for port managers
			_set_str_db("heart_beat", buff, -1);
		}

		heart_beat_tick = cur;
	}
}

int _g_fast_stats_mode = 0;

void qmimgr_callback_on_schedule_generic_qmi(struct funcschedule_element_t* element)
{
	struct qmimsg_t* msg;
	unsigned short tran_id;

	SYSLOG(LOG_DEBUG, "generic schedule triggered");

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to allocate msg for scheduled msgs");
		goto fini;
	}

	update_heartbeat(1);

	if(is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
#if 0
		//////////// implemented but not used ////////////

		// QMI_WDS_GET_RUNTIME_SETTINGS_REQ_TYPE - get current profile configuration
		{
			struct qmi_wds_get_runtime_settings_req req;

			req.profile_name = 1;
			req.ip_address = 1;
			req.gateway_info = 1;
			req.dns_address = 1;
			write32_to_little_endian(req.requested_settings, req.requested_settings);

			_request_qmi_tlv(msg, QMIWDS, QMI_WDS_GET_RUNTIME_SETTINGS, &tran_id, QMI_WDS_GET_RUNTIME_SETTINGS_REQ_TYPE, sizeof(req), &req);
		}


		//////////// implemented but not supported by any module so far ////////////

		// NAS - QMI_NAS_GET_OPERATOR_NAME_DATA
		_request_qmi(msg, QMINAS, QMI_NAS_GET_OPERATOR_NAME_DATA, &tran_id);

#endif


	}

	if(is_enabled_feature(FEATUREHASH_CMD_SIMCARD)) {
		//////////// sim card ////////////

		// pin status and sim status
		_qmi_check_pin_status(0);
	}

	_request_qmi(msg, QMINAS, QMI_NAS_GET_SYS_INFO, &tran_id);

	if(is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH)) {
		//////////// network or air ////////////

#if 0
		// QMI_NAS_GET_SIGNAL_STRENGTH - signal strength - TODO: this command is obsolete, use QMI_NAS_GET_SIG_INFO
		{
			struct qmi_nas_get_signal_strength_req req;

			req.request_mask = QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE_RSSI;

			write16_to_little_endian(req.request_mask, req.request_mask);
			_request_qmi_tlv(msg, QMINAS, QMI_NAS_GET_SIGNAL_STRENGTH, &tran_id, QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE, sizeof(req), &req);
		}
#endif
		if(!_g_fast_stats_mode) { // fast stats mode relies on indication
			_request_qmi(msg, QMINAS, QMI_NAS_GET_SIG_INFO, &tran_id);
		}
	}

	if(is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		// QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE - current service
		_request_qmi(msg, QMINAS, QMI_NAS_GET_SERVING_SYSTEM, &tran_id);

		/* CDMA subscribe information - SID, NID and MDN */
		{
			/* clear msg */
			qmimsg_clear_tlv(msg);

			/* add NAM id */
			unsigned char nam_id;
			nam_id = 0xff;
			qmimsg_add_tlv(msg, QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_REQ_TYPE, sizeof(nam_id), &nam_id);

			/* add 3GPPS mask */

			/*
				• 0x01 - QMI_NAS_GET_3GPP2_SUBS_INFO_NAM_NAME – NAM Name
				• 0x02 - QMI_NAS_GET_3GPP2_SUBS_INFO_DIR_NUM – Directory Number
				• 0x04 - QMI_NAS_GET_3GPP2_SUBS_INFO_HOME_SID_IND – Home SID/NID
				• 0x08 - QMI_NAS_GET_3GPP2_SUBS_INFO_MIN_BASED_IMSI – MIN-based IMSI
				• 0x10 - QMI_NAS_GET_3GPP2_SUBS_INFO_TRUE_IMSI – True IMSI
				• 0x20 - QMI_NAS_GET_3GPP2_SUBS_INFO_CDMA_CHANNEL – CDMA Channel
				• 0x40 - QMI_NAS_GET_3GPP2_SUBS_INFO_MDN – Mobile Directory Number
				All other bits are reserved for future
			*/

			unsigned int mask;
			mask = 0x20 | 0x40;
			qmimsg_add_tlv(msg, QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_REQ_TYPE_3GPP2_MASK, sizeof(mask), &mask);

			_request_qmi_tlv_ex(msg, QMINAS, QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO, &tran_id, 0, 0, NULL, 0);
		}

		/* CDMA position info - Pilot PN */
		_request_qmi(msg, QMINAS, QMI_NAS_GET_CDMA_POSITION_INFO, &tran_id);

		/* request OMA-DM activation */
		_request_qmi(msg, QMIDMS, QMI_DMS_GET_ACTIVATED_STATE, &tran_id);
	}

	if(is_enabled_feature(FEATUREHASH_CMD_BANDSTAT)) {
		// QMI_NAS_GET_RF_BAND_INFO - current band information
		_request_qmi(msg, QMINAS, QMI_NAS_GET_RF_BAND_INFO, &tran_id);
	}

	if(is_enabled_feature(FEATUREHASH_CMD_BANDSEL)) {
		// QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE - get current selected band
		_request_qmi(msg, QMINAS, QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE, &tran_id);
	}

fini:
	qmimsg_destroy(msg);

	// scheduling
	SYSLOG(LOG_DEBUG, "adding generic schedule - %d sec", QMIMGR_GENERIC_SCHEDULE_PERIOD);
	funcschedule_add(sched, 0, QMIMGR_GENERIC_SCHEDULE_PERIOD, qmimgr_callback_on_schedule_generic_qmi, NULL);
}

void qmimgr_callback_on_schedule_cell_info(struct funcschedule_element_t* element)
{
	struct qmimsg_t* msg;
	unsigned short tran_id;

	SYSLOG(LOG_DEBUG, "cell info schedule triggered");

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_OPERATION, "failed to allocate msg for scheduled msgs");
		goto fini;
	}

	// QMI_NAS_GET_CELL_LOCATION_INFO - cell info
	_request_qmi(msg, QMINAS, QMI_NAS_GET_CELL_LOCATION_INFO, &tran_id);

fini:
	qmimsg_destroy(msg);

	// re-schedule
	if(cell_info_timer) {
		SYSLOG(LOG_DEBUG, "adding cell info schedule - %d sec", cell_info_timer);
		funcschedule_add(sched, 0, cell_info_timer, qmimgr_callback_on_schedule_cell_info, NULL);
	}
}

// select which quectel QMAP interface to be used by this instance of qmimgr
static int quectel_bind_mux_data_port() {
	int rc = -1;
	struct qmimsg_t* msg = qmimsg_create();

	if (!msg) {
		SYSLOG(LOG_ERROR, "%s() failed to allocate msg\n", __func__);
		goto err;
	}

	qmimsg_clear_tlv(msg);
	qmimsg_set_msg_id(msg, QMI_WDS_BIND_MUX_DATA_PORT_REQ);

	// peripheral end point type
	unsigned long long endpoint = (0x4ULL << 32) | 0x2ULL;
	if (qmimsg_add_tlv(msg, 0x10, sizeof(endpoint), &endpoint) < 0) {
		SYSLOG(LOG_ERROR, "add TLV peripheral end point type failed");
		goto err;
	}

	// mux id base 0x81 is in driver code qmi_wwan.c
	unsigned char mux_id = 0x81 + instance;
	if (qmimsg_add_tlv(msg, 0x11, sizeof(mux_id), &mux_id) < 0) {
		SYSLOG(LOG_ERROR, "add TLV mux id failed");
		goto err;
	}

	// client type
	unsigned int client_type = 1;
	if (qmimsg_add_tlv(msg, 0x13, sizeof(client_type), &client_type) < 0) {
		SYSLOG(LOG_ERROR, "add TLV client type failed");
		goto err;
	}

	unsigned short trans_id;
	if (qmiuniclient_write(uni, QMIWDS, msg, &trans_id) < 0) {
		SYSLOG(LOG_ERROR, "%s() failed to queue\n", __func__);
		goto err;
	}

	unsigned short result, error;
	struct qmimsg_t*
		rmsg = _wait_qmi_response(QMIWDS, QMIMGR_MIDLONG_RESP_TIMEOUT, trans_id, &result, &error, NULL);
	if (!rmsg) {
		SYSLOG(LOG_ERROR, "ERROR: timeout waiting for QMIWDS_BIND_MUX_DATA_PORT_RESP");
		goto err;
	}

	if (result != 0) {
		SYSLOG(LOG_ERROR, "failed to bind mux, error=0x%x", error);
		goto err;
	}

	rc = 0;

err:
	qmimsg_destroy(msg);
	return rc;
}

#ifdef CONFIG_LINUX_QMI_DRIVER
int _qmi_set_data_format()
{
	struct qmi_easy_req_t er;
	int rc;

	unsigned char data_format;
	unsigned short link_prot;

	rc = _qmi_easy_req_init(&er, QMICTL, QMI_CTL_SET_DATA_FORMAT);
	if(rc < 0)
		goto fini;

//#define QOS_MODE
//#define DATA_MODE_RP

#ifdef QOS_MODE
	data_format = 1;
#else
	data_format = 0;
#endif

#ifdef DATA_MODE_RP
	link_prot = 2;
#else
	link_prot = 1;
#endif

	qmimsg_add_tlv(er.msg, QMI_CTL_SET_DATA_FORMAT_REQ, sizeof(data_format), &data_format);
	qmimsg_add_tlv(er.msg, QMI_CTL_SET_DATA_FORMAT_REQ_LINK_PROTO, sizeof(link_prot), &link_prot);

	/* init. tlv */
	rc = _qmi_easy_req_do_ex(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT, 1, 0);
	if(rc < 0)
		goto fini;

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_set_set_sync()
{
	struct qmi_easy_req_t er;
	int rc;

	rc = _qmi_easy_req_init(&er, QMICTL, QMI_CTL_SET_SYNC);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do_ex(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT, 1, 0);
	if(rc < 0)
		goto fini;

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_set_ready()
{
	struct qmi_easy_req_t er;
	int rc;

	rc = _qmi_easy_req_init(&er, QMICTL, QMI_CTL_SET_READY);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do_ex(&er, 0, 0, NULL, QMIMGR_MIDLONG_RESP_TIMEOUT, 1, 0);
	if(rc < 0)
		goto fini;

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}
int _qmi_get_client_id(int service_id, int* client_id)
{
	unsigned short tran_id;
	unsigned short qmi_result;

	struct qmi_ctl_get_client_id_req_service_type req;
	struct qmi_ctl_get_client_id_resp_client_id* resp;

	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;

	const struct qmitlv_t* tlv;

	SYSLOG(LOG_OPERATION, "###qmimux### get client id");

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to create a msg");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### requesting QMI_WDS_SET_DEFAULT_PROFILE_NUM");

	// build request
	req.qmi_svc_type = (unsigned char)service_id;

	// request QMI_WDS_SET_DEFAULT_PROFILE_NUM
	if(_request_qmi_tlv(msg, QMICTL, QMI_CTL_GET_CLIENT_ID, &tran_id, QMI_CTL_GET_CLIENT_ID_REQ_TYPE_SERVICE_TYPE, sizeof(req), &req) < 0) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to request for QMI_CTL_GET_CLIENT_ID");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### waiting for response - QMI_CTL_GET_CLIENT_ID");

	// wait for response
	rmsg = _wait_qmi_response_ex(QMICTL, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL, 0);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "###qmimux### qmi response timeout (or qmi failure result)");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "check result of QMI_CTL_GET_CLIENT_ID");

	// get QMI_WDS_GET_PROFILE_LIST_RESP_TYPE
	tlv = _get_tlv(rmsg, QMI_CTL_GET_CLIENT_ID_RESP_TYPE_CLIENT_ID, sizeof(*resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR, "QMI_CTL_GET_CLIENT_ID_RESP_TYPE_CLIENT_ID not found in QMI_CTL_GET_CLIENT_ID");
		goto err;
	}

	/* get resp and client id */
	resp = (struct qmi_ctl_get_client_id_resp_client_id*)tlv->v;
	if(resp->qmi_svc_type != service_id) {
		SYSLOG(LOG_ERROR, "service id not matching in QMI_CTL_GET_CLIENT_ID_RESP_TYPE_CLIENT_ID (req=%d,resp=%d)", service_id, resp->qmi_svc_type);
		goto err;
	}

	/* return client id */
	*client_id = resp->client_id;

	qmimsg_destroy(msg);

	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}
#endif

int _qmi_setdef(int profile_type, int profile_family, int profile_index)
{
	unsigned short tran_id;
	unsigned short qmi_result;

	struct qmi_wds_set_default_profile_num_req req;

	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;

	SYSLOG(LOG_OPERATION, "setting default profile");

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "requesting QMI_WDS_SET_DEFAULT_PROFILE_NUM");

	// build request
	req.profile_type = profile_type;
	req.profile_family = profile_family;
	req.profile_index = profile_index;

	// request QMI_WDS_SET_DEFAULT_PROFILE_NUM
	if(_request_qmi_tlv(msg, QMIWDS, QMI_WDS_SET_DEFAULT_PROFILE_NUM, &tran_id, QMI_WDS_GET_DEFAULT_PROFILE_NUM_REQ_TYPE, sizeof(req), &req) < 0) {
		SYSLOG(LOG_ERROR, "failed to request for QMI_WDS_SET_DEFAULT_PROFILE_NUM");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "waiting for response - QMI_WDS_SET_DEFAULT_PROFILE_NUM");

	// wait for response
	rmsg = _wait_qmi_response(QMIWDS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "QMI_WDS_SET_DEFAULT_PROFILE_NUM success");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_getdef(int profile_type, int profile_family)
{
	unsigned short tran_id;
	unsigned short qmi_result;

	struct qmi_wds_get_default_profile_num_req req;

	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;

	unsigned char* profile_index;

	// get TLV
	const struct qmitlv_t* tlv;

	SYSLOG(LOG_OPERATION, "getting default profile");

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "requesting QMI_WDS_GET_DEFAULT_PROFILE_NUM");

	// build request
	req.profile_type = profile_type;
	req.profile_family = profile_family;

	// request QMI_WDS_GET_DEFAULT_PROFILE_NUM
	if(_request_qmi_tlv(msg, QMIWDS, QMI_WDS_GET_DEFAULT_PROFILE_NUM, &tran_id, QMI_WDS_GET_DEFAULT_PROFILE_NUM_REQ_TYPE, sizeof(req), &req) < 0) {
		SYSLOG(LOG_ERROR, "failed to request for QMI_WDS_GET_DEFAULT_PROFILE_NUM");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "waiting for response - QMI_WDS_GET_DEFAULT_PROFILE_NUM");

	// wait for response
	rmsg = _wait_qmi_response(QMIWDS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "checking result - QMI_WDS_GET_DEFAULT_PROFILE_NUM");

	// get QMI_WDS_GET_PROFILE_LIST_RESP_TYPE
	tlv = _get_tlv(rmsg, QMI_WDS_GET_DEFAULT_PROFILE_NUM_RESP_TYPE, sizeof(*profile_index));
	if(!tlv) {
		SYSLOG(LOG_ERROR, "QMI_WDS_GET_DEFAULT_PROFILE_NUM_RESP_TYPE not found in QMI_WDS_GET_DEFAULT_PROFILE_NUM");
		goto err;
	}

	profile_index = (unsigned char*)tlv->v;

	SYSLOG(LOG_OPERATION, "QMI_WDS_GET_DEFAULT_PROFILE_NUM success");

	qmimsg_destroy(msg);
	return *profile_index;

err:
	qmimsg_destroy(msg);
	return -1;
}

typedef int (*_qmi_walk_thru_profiles_callback_t)(int profile_idx, const char* profile, const struct db_struct_profile* db_profile, void* ref);

int _qmi_walk_thru_profiles(_qmi_walk_thru_profiles_callback_t cb, void* ref)
{
	struct qmi_easy_req_t er;
	const struct qmitlv_t* tlv;
	int rc;
	int len;
	int i;

	struct qmi_wds_get_profile_list_resp* resp;
	struct qmi_wds_get_profile_list_resp* resp_local;
	struct qmi_wds_get_profile_list_resp_sub* resp_sub;
	int profile_name_length;
	struct qmi_wds_get_profile_list_resp_sub* resp_sub_upper_bounce;

	struct db_struct_profile db_profile;
	char profile[QMIMGR_MAX_DB_VALUE_LENGTH];

	/* init qmi req */
	rc = _qmi_easy_req_init(&er, QMIWDS, QMI_WDS_GET_PROFILE_LIST);
	if(rc < 0)
		goto fini;

	/* send tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc < 0)
		goto fini;

	tlv = _get_tlv(er.rmsg, QMI_WDS_GET_PROFILE_LIST_RESP_TYPE, sizeof(*resp));
	if(!tlv)
		goto err;

	resp = tlv->v;
	SYSLOG(LOG_ERR, "[oma-dm] total %d profile(s) found", resp->num_instances);

	/* check validation of length  */
	len = sizeof(*resp) + sizeof(*resp_sub) * resp->num_instances;
	if(tlv->l < len) {
		SYSLOG(LOG_ERROR, "[oma-dm] incorrect length of TLV found (l=%d,len=%d,num=%d)", tlv->l, len, resp->num_instances);
		goto err;
	}

	/* backup resp */
	resp_local = alloca(tlv->l);
	memcpy(resp_local, resp, tlv->l);

	/* get upper bounce limit */
	resp_sub_upper_bounce = (struct qmi_wds_get_profile_list_resp_sub*)((char*)resp_local + tlv->l);

	/* walk through profiles */
	resp_sub = resp_local->profile;
	for(i = 0; i < resp_local->num_instances; i++) {
		// paranoid check
		if(resp_sub >= resp_sub_upper_bounce) {
			SYSLOG(LOG_ERROR, "[oma-dm] profile packet is broken!!");
			goto fini;
		}

		profile_name_length = resp_sub->profile_name_length;

		if(_qmi_get_profile(resp_sub->profile_index, 0, profile, &db_profile) < 0) {
			SYSLOG(LOG_ERROR, "[oma-dm] failed to read profile (i=%d,idx=%d)", i, resp_sub->profile_index);
			goto err;
		}

		/* call callback function */
		if(cb(resp_sub->profile_index, profile, &db_profile, ref) < 0)
			break;

		/* get next resp sub */
		resp_sub = (struct qmi_wds_get_profile_list_resp_sub*)((char*)resp_sub + profile_name_length + sizeof(struct qmi_wds_get_profile_list_resp_sub));
	}

fini:
	_qmi_easy_req_fini(&er);
	return rc;

err:
	rc = -1;
	goto fini;
}

int _qmi_find_profile(const char* name, int empty_slot, int skip_first)
{
	// get TLV
	const struct qmitlv_t* tlv;
	struct qmi_wds_get_profile_list_resp* resp;
	struct qmi_wds_get_profile_list_resp_sub* resp_sub;
	struct qmi_wds_get_profile_list_resp_sub* resp_sub_upper_bounce;

	int i;
	int profile_name_length;
	char *profile_name;
	int name_len;

	unsigned short tran_id;
	unsigned short qmi_result;

	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;

	char profile[QMIMGR_MAX_DB_VALUE_LENGTH];

	int ret;

	ret = -1;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto fini;
	}

	// request QMI_WDS_GET_PROFILE_LIST
	if(_request_qmi(msg, QMIWDS, QMI_WDS_GET_PROFILE_LIST, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to request for QMI_WDS_GET_PROFILE_LIST");
		goto fini;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMIWDS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto fini;
	}

	SYSLOG(LOG_OPERATION, "QMI_WDS_GET_PROFILE_LIST success");

	// get QMI_WDS_GET_PROFILE_LIST_RESP_TYPE
	tlv = _get_tlv(rmsg, QMI_WDS_GET_PROFILE_LIST_RESP_TYPE, sizeof(*resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR, "QMI_WDS_GET_PROFILE_LIST_RESP_TYPE not found in QMI_WDS_GET_PROFILE_LIST");
		goto fini;
	}

	// get upper bounce limit
	resp_sub_upper_bounce = (struct qmi_wds_get_profile_list_resp_sub*)((char*)tlv->v + tlv->l);

	// search profile name
	resp = (struct qmi_wds_get_profile_list_resp*)tlv->v;
	resp_sub = (struct qmi_wds_get_profile_list_resp_sub*)(resp + 1);

	name_len = strlen(name);

	SYSLOG(LOG_OPERATION, "total qmi profile = %d", resp->num_instances);

	for(i = 0; (i < resp->num_instances) && (ret < 0); i++) {
		// paranoid check
		if(resp_sub >= resp_sub_upper_bounce) {
			SYSLOG(LOG_ERROR, "profile packet is broken!!");
			goto fini;
		}

		profile_name_length = resp_sub->profile_name_length;
		profile_name = resp_sub->profile_name;

		if(!profile_name_length) {
			SYSLOG(LOG_ERROR, "zero length profile name found - document does not allow");
		} else {
			__strncpy(profile, profile_name, (int)profile_name_length + 1);

			SYSLOG(LOG_OPERATION, "qmi profile %d - %s(idx:%d)", i, profile, resp_sub->profile_index);

			// search for exact profile
			if(!empty_slot) {
				if(!strcmp(name, profile_name)) {
					SYSLOG(LOG_OPERATION, "profile name matched - %s(%d)", profile, resp_sub->profile_index);
					ret = resp_sub->profile_index;
				}
			}
			// search for a profile that does not have name prefix
			else {
				if((i != 0 || !skip_first) && strncmp(name, profile_name, name_len)) {
					SYSLOG(LOG_OPERATION, "empty profile found - %s(%d)", profile, resp_sub->profile_index);
					ret = resp_sub->profile_index;
				}
			}
		}

		resp_sub = (struct qmi_wds_get_profile_list_resp_sub*)((char*)resp_sub + profile_name_length + sizeof(struct qmi_wds_get_profile_list_resp_sub));
	}

fini:
	qmimsg_destroy(msg);
	return ret;
}

int _qmi_delete_profile(unsigned char profile_index)
{
	struct qmi_wds_profile_index_reqresp_t req;
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short tran_id;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	req.profile_type = 0;
	req.profile_index = profile_index;

	unsigned short qmi_result;

	// request QMI_WDS_DELETE_PROFILE
	if(_request_qmi_tlv(msg, QMIWDS, QMI_WDS_DELETE_PROFILE, &tran_id, QMI_WDS_DELETE_PROFILE_REQ_TYPE, sizeof(req), &req) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed");
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMIWDS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "QMI_WDS_DELETE_PROFILE success");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_get_profile(int profile_index, int profile_type, char* profile_name, struct db_struct_profile* profile)
{
	struct qmi_wds_profile_index_reqresp_t req;
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short tran_id;

	const struct qmitlv_t* tlv;

	char buf[QMIMGR_MAX_DB_VALUE_LENGTH];

	struct in_addr* source_ip;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	req.profile_type = profile_type;
	req.profile_index = profile_index;

	unsigned short qmi_result;

	// request QMI_WDS_GET_PROFILE_SETTINGS
	if(_request_qmi_tlv(msg, QMIWDS, QMI_WDS_GET_PROFILE_SETTINGS, &tran_id, QMI_WDS_GET_PROFILE_SETTINGS_REQ_TYPE, sizeof(req), &req) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed");
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMIWDS, QMIMGR_MIDLONG_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "QMI_WDS_GET_PROFILE_SETTINGS success");

	// extracting TLVs - profile name
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_NAME, 0);
	if(tlv) {
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_NAME");
		__strncpy(profile_name, tlv->v, __min(QMIMGR_MAX_DB_VALUE_LENGTH, tlv->l + 1));
	} else {
		profile_name[0] = 0;
	}

	// extracating TLV - pdp_type
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_PDP_TYPE, 1);
	if(tlv) {
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_PDP_TYPE");

		switch(*(unsigned char*)tlv->v) {
		case 0:
			sprintf(buf, "PDP-IP");
			break;

		case 1:
			sprintf(buf, "PDP-PPP");
			break;

		case 2:
			sprintf(buf, "PDP-IPV6");
			break;

		case 3:
			sprintf(buf, "PDP-IPV4V6");
			break;

		default:
			sprintf(buf, "UNKNOWN(%d)", *(unsigned char*)tlv->v);
			break;
		}

		_dbstruct_set_str_member(profile, pdp_type, 1, buf, QMIMGR_MAX_DB_VALUE_LENGTH);
	}

	// extracating TLV - pdp_type (3GPP2)
	tlv = _get_tlv(rmsg, QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_PDN_TYPE, 1);
	if(tlv) {
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_PDP_TYPE");

		switch(*(unsigned char*)tlv->v) {
		case 0:
			sprintf(buf, "PDP-IP");
			break;

		case 1:
			sprintf(buf, "PDP-IPV6");
			break;

		case 2:
			sprintf(buf, "PDP-IPV4V6");
			break;

		default:
			sprintf(buf, "UNKNOWN(%d)", *(unsigned char*)tlv->v);
			break;
		}

		_dbstruct_set_str_member(profile, pdp_type, 1, buf, QMIMGR_MAX_DB_VALUE_LENGTH);
	}


	// extracting TLV - apn name
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_APN_NAME, 0);
	// extracting TLV - apn name (3GPP2)
	if(!tlv)
		tlv = _get_tlv(rmsg, QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_APN_NAME, 0);

	_dbstruct_set_str_member(profile, apn_name, tlv != NULL, tlv->v, __min(QMIMGR_MAX_DB_VALUE_LENGTH, tlv->l + 1));
	if(tlv)
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_APN_NAME");

	// extracting TLV - user
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_USER, 0);
	// extracting TLV - user (3GPP2)
	if(!tlv)
		tlv = _get_tlv(rmsg, QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_USER, 0);

	_dbstruct_set_str_member(profile, user, tlv != NULL && tlv->l > 0, tlv->v, __min(QMIMGR_MAX_DB_VALUE_LENGTH, tlv->l + 1));
	if(tlv)
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_USER");

	// extracting TLV - pass
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_PASS, 0);
	// extracting TLV - pass (3GPP2)
	if(!tlv)
		tlv = _get_tlv(rmsg, QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_PASS, 0);

	_dbstruct_set_str_member(profile, pass, tlv != NULL, tlv->v, __min(QMIMGR_MAX_DB_VALUE_LENGTH, tlv->l + 1));
	if(tlv)
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_PASS");


	// extracting TLV - auth
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_AUTH, 0);
	// extracting TLV - auth (3GPP2)
	if(!tlv)
		tlv = _get_tlv(rmsg, QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_AUTH, 0);

	if(tlv) {
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_AUTH");

		switch(*(unsigned char*)tlv->v) {
		case 0:
		case 4:
			/* TODO:
				3GPP2 seems to use protocol type (#4) as NONE authentication instead of 0. NONE authentication is not documented in QMI
				Although, this assumption works for MC7354 SWI9X15C_05.05.58.01.
			*/

			sprintf(buf, "NONE");
			break;

		case(QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_PAP):
			sprintf(buf, "PAP");
			break;

		case(QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_CHAP):
			sprintf(buf, "CHAP");
			break;

		case(QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_PAP|QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_CHAP):
			sprintf(buf, "PAP|CHAP");
			break;

		default:
			sprintf(buf, "UNKNOWN(%d)", *(unsigned char*)tlv->v);
			break;
		}

		_dbstruct_set_str_member(profile, auth_preference, 1, buf, QMIMGR_MAX_DB_VALUE_LENGTH);
	}

	// extracting TLV -  ipv4 address preference
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_IPV4_ADDR_PREF, 1);

	_dbstruct_set_str_member(profile, ipv4_addr_pref, tlv != NULL, tlv->v, __min(QMIMGR_MAX_DB_VALUE_LENGTH, tlv->l + 1));
	if(tlv) {
		struct in_addr prefIp;
		struct qmi_wds_profile_reqresp_ipv4_addr_pref * ipv4_pref;
		in_addr_t* ipv4_addr_pref;

		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_IPV4_ADDR_PREF");
		ipv4_pref = tlv->v;
		ipv4_addr_pref = (in_addr_t*)ipv4_pref->ipv4_addr_pref;
		prefIp.s_addr = htonl(*ipv4_addr_pref);
		_dbstruct_set_str_member(profile, ipv4_addr_pref, 1, inet_ntoa(*(struct in_addr*)(&prefIp)), QMIMGR_MAX_DB_VALUE_LENGTH);
	}

	// extracting TLV - TFTs
	int i;
	struct qmi_wds_profile_reqresp_tft_id* tft;
	char ip_version[QMIMGR_MAX_DB_VALUE_LENGTH];

	for(i = 0; i < __countof(profile->tft_id); i++) {

		tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_TFT_ID1 + i, sizeof(*tft));
		if(!tlv)
			continue;

		tft = (struct qmi_wds_profile_reqresp_tft_id*)tlv->v;

		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_TFT_ID%d", i);

		// filter_id
		_dbstruct_set_int_member(&profile->tft_id[i], filter_id, 1, tft->filter_id);
		// eval_id
		_dbstruct_set_int_member(&profile->tft_id[i], eval_id, 1, tft->eval_id);

		// ip_version
		switch(tft->ip_version) {
		case 4:
			sprintf(ip_version, "IPv4");
			break;

		case 6:
			sprintf(ip_version, "IPv6");
			break;

		default:
			sprintf(ip_version, "UNKNOWN(%d)", tft->ip_version);
			break;
		}
		_dbstruct_set_str_member(&profile->tft_id[i], ip_version, 1, ip_version, QMIMGR_MAX_DB_VALUE_LENGTH);

		// source_ip
		source_ip = (struct in_addr*)tft->source_ip;
		_dbstruct_set_str_member(&profile->tft_id[i], source_ip, 1, inet_ntoa(*source_ip), QMIMGR_MAX_DB_VALUE_LENGTH);
		// source_ip_mask
		_dbstruct_set_int_member(&profile->tft_id[i], source_ip_mask, 1, tft->source_ip_mask);
		// next_header
		_dbstruct_set_int_member(&profile->tft_id[i], next_header, 1, tft->next_header);
		// dest_port_range_start
		_dbstruct_set_int_member(&profile->tft_id[i], dest_port_range_start, 1, read16_from_little_endian(tft->dest_port_range_start));
		// dest_port_range_end
		_dbstruct_set_int_member(&profile->tft_id[i], dest_port_range_end, 1, read16_from_little_endian(tft->dest_port_range_end));
		// src_port_range_start
		_dbstruct_set_int_member(&profile->tft_id[i], src_port_range_start, 1, read16_from_little_endian(tft->src_port_range_start));
		// src_port_range_end
		_dbstruct_set_int_member(&profile->tft_id[i], src_port_range_end, 1, read16_from_little_endian(tft->src_port_range_end));
		// ipsec_spi
		_dbstruct_set_int_member(&profile->tft_id[i], ipsec_spi, 1, read32_from_little_endian(tft->ipsec_spi));
		// tos_mask
		_dbstruct_set_int_member(&profile->tft_id[i], tos_mask, 1, read16_from_little_endian(tft->tos_mask));
		// flow_label
		_dbstruct_set_int_member(&profile->tft_id[i], flow_label, 1, read32_from_little_endian(tft->flow_label));
	}

	// extracting TLV - pdp_context_number
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_PDP_CONTEXT_NUMBER, 1);
	_dbstruct_set_int_member(profile, pdp_context_number, tlv != NULL, *(unsigned char*)tlv->v);
	if(tlv)
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_PDP_CONTEXT_NUMBER");

	SYSLOG(LOG_DEBUG, "reading TLV - QMI_WDS_PROFILE_REQRESP_TYPE_SECONDARY_FLAG");

	// extracting TLV - pdp_context_sec_flag
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_SECONDARY_FLAG, 1);
	_dbstruct_set_int_member(profile, pdp_context_sec_flag, tlv != NULL, *(unsigned char*)tlv->v);
	if(tlv)
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_SECONDARY_FLAG");

	// extracting TLV - pdp_context_primary_id
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_PRIMARY_ID, 1);
	_dbstruct_set_int_member(profile, pdp_context_primary_id, tlv != NULL, *(unsigned char*)tlv->v);
	if(tlv)
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_PRIMARY_ID");

	// extracting TLV - addr_allocation_preference
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_ADDR_ALLOCATION_PREFERENCE, 1);
	if(tlv) {

		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_ADDR_ALLOCATION_PREFERENCE");

		switch(*(unsigned char*)tlv->v) {
		case 0:
			sprintf(buf, "NAS");
			break;

		case 1:
			sprintf(buf, "DHCP");
			break;

		default:
			sprintf(buf, "UNKNOWN(%d)", *(unsigned char*)tlv->v);
			break;
		}

		_dbstruct_set_str_member(profile, addr_allocation_preference, 1, buf, QMIMGR_MAX_DB_VALUE_LENGTH);
	}

	struct qmi_wds_profile_reqresp_type_qci* qci;

	// extracting TLV - qci
	tlv = _get_tlv(rmsg, QMI_WDS_PROFILE_REQRESP_TYPE_QCI, sizeof(*qci));
	if(tlv) {
		SYSLOG(LOG_DEBUG, "tlv detected - got QMI_WDS_PROFILE_REQRESP_TYPE_QCI");

		qci = (struct qmi_wds_profile_reqresp_type_qci*)tlv->v;

		_dbstruct_set_int_member(profile, qci, 1, qci->qci);
		_dbstruct_set_int_member(profile, g_dl_bit_rate, 1, read32_from_little_endian(qci->g_dl_bit_rate));
		_dbstruct_set_int_member(profile, max_dl_bit_rate, 1, read32_from_little_endian(qci->max_dl_bit_rate));
		_dbstruct_set_int_member(profile, g_ul_bit_rate, 1, read32_from_little_endian(qci->g_ul_bit_rate));
		_dbstruct_set_int_member(profile, max_ul_bit_rate, 1, read32_from_little_endian(qci->max_ul_bit_rate));
	}


	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_easy_req_init_ex(struct qmi_easy_req_t* er, int serv_id, unsigned short msg_id, const char* msg_name)
{
	memset(er, 0, sizeof(*er));

	/* init. member values */
	er->serv_id = serv_id;
	er->msg_id = msg_id;
	er->msg_name = msg_name;

	/* create msg */
	er->msg = qmimsg_create();
	if(!er->msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg for %s", er->msg_name);
		goto err;
	}

	return 0;

err:
	return -1;
}

// This function is to support "Asynchronous Messaging Paradiam"
// With this paradiam, Response(RESP) messages are used as ACK/NAK for Request message.
// And all asynchronous events or status information are provided to client in indication(IND) message.
int _qmi_easy_req_do_async_ex(struct qmi_easy_req_t* er, int t, int l, void* v, int to_resp, int to_ind, int verbose, int rdb)
{
	/* assign tlv */
	SYSLOG(LOG_OPERATION, "###qmimsg### requesting %s (0x%04x) - serv_id=%d", er->msg_name, er->msg_id, er->serv_id);

	/* send request */
	if(_request_qmi_tlv_ex(er->msg, er->serv_id, er->msg_id, &er->tran_id, t, l, v, 0) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed for %s", er->msg_name);
		goto err;
	}

	if(to_resp < 0) {
		SYSLOG(LOG_OPERATION, "bypass %s response", er->msg_name);
	} else {
		/* wait for response */
		er->rmsg = _wait_qmi_response_ex(er->serv_id, to_resp, er->tran_id, &er->qmi_result, &er->qmi_error, &er->qmi_ext_error, rdb);
		if(!er->rmsg) {
			SYSLOG(LOG_ERROR, "qmi response timeout for %s", er->msg_name);
			goto err;
		}

		if(er->qmi_result) {
			if(verbose) {
				unsigned short qmi_result;
				unsigned short qmi_error;
				const char* error_msg;

				/* get result and error */
				qmi_result = read16_from_little_endian(er->qmi_result);
				qmi_error = read16_from_little_endian(er->qmi_error);

				/* get error strign */
				error_msg = resourcetree_lookup(res_qmi_strings, RES_STR_KEY(QMIEX, QMIERR, qmi_error));
				if(!error_msg)
					error_msg = "unknown";

				SYSLOG(LOG_ERROR, "qmi failure result for %s (result=%d,error=0x%04x,%s)", er->msg_name, qmi_result, qmi_error, error_msg);
			}
			goto err;
		}

		if(to_ind < 0) {
			SYSLOG(LOG_OPERATION, "bypass %s asynchronous indication", er->msg_name);
		} else {
			er->imsg = _wait_qmi_indication_ex(er->serv_id, to_ind, er->msg_id, &er->qmi_result, &er->qmi_error, &er->qmi_ext_error, rdb);
			if(!er->imsg) {
				SYSLOG(LOG_ERROR, "qmi asynchronous indication timeout for %s", er->msg_name);
				goto err;
			}

			SYSLOG(LOG_OPERATION, "got %s indication", er->msg_name);
		}
	}

	return 0;
err:
	return -1;
}

int _qmi_easy_req_do_ex(struct qmi_easy_req_t* er, int t, int l, void* v, int to, int verbose, int rdb)
{
	/* assign tlv */
	SYSLOG(LOG_OPERATION, "###qmimsg### requesting %s (0x%04x) - serv_id=%d", er->msg_name, er->msg_id, er->serv_id);

	/* send request */
	if(_request_qmi_tlv_ex(er->msg, er->serv_id, er->msg_id, &er->tran_id, t, l, v, 0) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed for %s", er->msg_name);
		goto err;
	}

	if(to < 0) {
		SYSLOG(LOG_OPERATION, "bypass %s response", er->msg_name);
	} else {
		/* wait for response */
		er->rmsg = _wait_qmi_response_ex(er->serv_id, to, er->tran_id, &er->qmi_result, &er->qmi_error, &er->qmi_ext_error, rdb);
		if(!er->rmsg) {
			SYSLOG(LOG_ERROR, "qmi response timeout for %s", er->msg_name);
			goto err;
		}

		if(er->qmi_result) {
			if(verbose) {
				unsigned short qmi_result;
				unsigned short qmi_error;
				const char* error_msg;

				/* get result and error */
				qmi_result = read16_from_little_endian(er->qmi_result);
				qmi_error = read16_from_little_endian(er->qmi_error);

				/* get error strign */
				error_msg = resourcetree_lookup(res_qmi_strings, RES_STR_KEY(QMIEX, QMIERR, qmi_error));
				if(!error_msg)
					error_msg = "unknown";

				SYSLOG(LOG_ERROR, "qmi failure result for %s (result=%d,error=0x%04x,%s)", er->msg_name, qmi_result, qmi_error, error_msg);
			}
			goto err;
		}

		SYSLOG(LOG_OPERATION, "got %s response", er->msg_name);
	}

	return 0;
err:
	return -1;
}

int _qmi_easy_req_do(struct qmi_easy_req_t* er, int t, int l, void* v, int to)
{
	return _qmi_easy_req_do_ex(er, t, l, v, to, 1, 1);
}


void _qmi_easy_req_fini(struct qmi_easy_req_t* er)
{
	if(er->msg)
		qmimsg_destroy(er->msg);
}

#define _qmi_easy_req_perform(serv_id,msg,to) _qmi_easy_req_perform_ex(serv_id,msg,#msg,to)

int _qmi_easy_req_perform_ex(int serv_id, unsigned short msg_id, const char* msg_name, int to)
{
	struct qmi_easy_req_t er;
	int rc;

	rc = _qmi_easy_req_init_ex(&er, serv_id, msg_id, msg_name);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, to);

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}


int _qmi_gps_determine(int to)
{
	return _qmi_easy_req_perform(QMIPDS, QMI_PDS_DETERMINE_POSITION, to);
}

int _qmi_gps_get_auto_tracking(int* stat)
{
	struct qmi_easy_req_t er;
	const struct qmitlv_t* tlv;
	int rc;

	struct qmi_pds_auto_tracking_state_reqresp* resp;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_GET_AUTO_TRACKING_STATE);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc < 0)
		goto fini;

	tlv = _get_tlv(er.rmsg, QMI_PDS_GET_AUTO_TRACKING_STATE_RESP_TYPE, sizeof(*resp));
	if(!tlv)
		goto err;

	resp = tlv->v;
	if(stat)
		*stat = resp->auto_tracking_state;

fini:
	_qmi_easy_req_fini(&er);
	return rc;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_gps_set_auto_tracking(int stat)
{
	struct qmi_easy_req_t er;
	int rc;

	struct qmi_pds_auto_tracking_state_reqresp req;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_SET_AUTO_TRACKING_STATE);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	req.auto_tracking_state = stat ? 1 : 0;
	rc = _qmi_easy_req_do_ex(&er, QMI_PDS_SET_AUTO_TRACKING_STATE_REQ_TYPE, sizeof(req), &req, QMIMGR_GENERIC_RESP_TIMEOUT, (stat == 0) ? 0 : 1, 1);

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_gps_set_event(int parsed_pos, int satelite_info)
{
	struct qmi_easy_req_t er;
	int rc;
	char v;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_SET_EVENT_REPORT);
	if(rc < 0)
		goto fini;

	/* tlv parameters */
	v = parsed_pos ? 1 : 0;
	qmimsg_add_tlv(er.msg, QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_PARSED_POSITION_DATA, sizeof(v), &v);
	v = satelite_info ? 1 : 0;
	qmimsg_add_tlv(er.msg, QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_SATELITE_INFORMATION, sizeof(v), &v);

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_gps_end_tracking()
{
	struct qmi_easy_req_t er;
	int rc;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_END_TRACKING_SESSION);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);
fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_gps_get_default_tracking(unsigned char* operation, unsigned char* timeout, unsigned int* interval, unsigned int* accuracy)
{
	struct qmi_easy_req_t er;
	int rc;
	const struct qmitlv_t* tlv;
	struct qmi_pds_default_tracking_session_reqresp* resp;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_GET_DEFAULT_TRACKING_SESSION);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc < 0)
		goto fini;

	/* return tlv */
	tlv = _get_tlv(er.rmsg, QMI_PDS_GET_DEFAULT_TRACKING_SESSION_RESP_TYPE, sizeof(*resp));
	if(!tlv)
		goto err;

	resp = tlv->v;

	if(operation)
		*operation = resp->session_operation;
	if(timeout)
		*timeout = resp->position_data_timeout;
	if(interval)
		*interval = resp->position_data_interval;
	if(accuracy)
		*accuracy = resp->position_data_accuracy;

fini:
	_qmi_easy_req_fini(&er);
	return rc;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_gps_set_default_tracking(unsigned char operation, unsigned char timeout, unsigned int interval, unsigned int accuracy)
{
	struct qmi_easy_req_t er;
	struct qmi_pds_default_tracking_session_reqresp req;
	int rc;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_SET_DEFAULT_TRACKING_SESSION);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	memset(&req, 0, sizeof(req));
	req.session_operation = operation;
	req.position_data_timeout = timeout;
	req.position_data_interval = interval;
	req.position_data_accuracy = accuracy;
	rc = _qmi_easy_req_do_ex(&er, QMI_PDS_SET_DEFAULT_TRACKING_SESSION_REQ_TYPE, sizeof(req), &req, QMIMGR_GENERIC_RESP_TIMEOUT, 0, 1);

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_gps_set_nmea(unsigned char sentence_mask, unsigned char port, unsigned char reporting, unsigned short* ext_sentence_mask)
{
	struct qmi_easy_req_t er;
	struct qmi_pds_nmea_config_reqresp req;
	struct qmi_pds_nmea_config_resp_ext_nmea_sentence_mask req2;
	int rc;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_SET_NMEA_CONFIG);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	memset(&req, 0, sizeof(req));
	req.nmea_sentence_mask = sentence_mask;
	req.nmea_port = port;
	req.nmea_reporting = reporting;
	qmimsg_add_tlv(er.msg, QMI_PDS_SET_NMEA_CONFIG_REQ_TYPE, sizeof(req), &req);

	if(ext_sentence_mask) {
		memset(&req2, 0, sizeof(req2));
		req2.ext_nmea_sentence_mask = *ext_sentence_mask;
		qmimsg_add_tlv(er.msg, QMI_PDS_SET_NMEA_CONFIG_RESP_TYPE_EXT_NMEA_SENTENCE_MASK, sizeof(req2), &req2);
	}

	rc = _qmi_easy_req_do_ex(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT, 0, 1);
fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_gps_reset()
{
	struct qmi_easy_req_t er;
	int rc;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_RESET);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);

fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

int _qmi_gps_get_nmea(unsigned char* sentence_mask, unsigned char* port, unsigned char* reporting, unsigned short* ext_sentence_mask)
{
	struct qmi_easy_req_t er;
	struct qmi_pds_nmea_config_reqresp* resp;
	struct qmi_pds_nmea_config_resp_ext_nmea_sentence_mask* resp2;
	int rc;
	const struct qmitlv_t* tlv;

	/* initiate ext_sentence_mask */
	if(ext_sentence_mask)
		*ext_sentence_mask = 0;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_GET_NMEA_CONFIG);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc < 0)
		goto fini;

	/* get 1st tlv */
	tlv = _get_tlv(er.rmsg, QMI_PDS_GET_NMEA_CONFIG_RESP_TYPE, sizeof(*resp));
	if(!tlv)
		goto err;

	/* return values */
	resp = tlv->v;
	if(sentence_mask)
		*sentence_mask = resp->nmea_sentence_mask;
	if(port)
		*port = resp->nmea_port;
	if(reporting)
		*reporting = resp->nmea_reporting;

	/* get ext sentence mask */
	tlv = _get_tlv(er.rmsg, QMI_PDS_GET_NMEA_CONFIG_RESP_TYPE_EXT_NMEA_SENTENCE_MASK, sizeof(*resp2));
	if(tlv) {
		resp2 = tlv->v;
		if(ext_sentence_mask) {
			*ext_sentence_mask = resp2->ext_nmea_sentence_mask;
		}
	}

fini:
	_qmi_easy_req_fini(&er);
	return rc;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_gps_start_tracking(unsigned char operation, unsigned char timeout, unsigned int interval, unsigned int accuracy, unsigned int count)
{
	struct qmi_easy_req_t er;
	struct qmi_pds_start_tracking_session_req req;
	int rc;

	rc = _qmi_easy_req_init(&er, QMIPDS, QMI_PDS_START_TRACKING_SESSION);
	if(rc < 0)
		goto fini;

	/* init. tlv */
	memset(&req, 0, sizeof(req));
	req.session_control = 0;	/* manual */
	req.session_type = 0;	/* new */
	req.session_operation = operation;
	req.session_server_option = 0; /* default */
	req.position_data_timeout = timeout;
	req.position_data_count = count;
	req.position_data_interval = interval;
	req.position_data_accuracy = accuracy;
	rc = _qmi_easy_req_do(&er, QMI_PDS_START_TRACKING_SESSION_REQ_TYPE, sizeof(req), &req, QMIMGR_GENERIC_RESP_TIMEOUT);
fini:
	_qmi_easy_req_fini(&er);
	return rc;
}

// set client IP family preference
static int set_client_ip_pref(int serv_id, unsigned char iptype)
{
	struct qmimsg_t* msg = qmimsg_create();

	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	qmimsg_clear_tlv(msg);
	qmimsg_set_msg_id(msg, QMI_WDS_SET_CLIENT_IP_FAMILY_PREF);

	unsigned char byte_value = iptype;
	if (qmimsg_add_tlv(msg, 0x01, sizeof(byte_value), &byte_value) < 0) {
		SYSLOG(LOG_ERROR, "add TLV APN name failed");
		goto err;
	}

	unsigned short trans_id;
	if (qmiuniclient_write(uni, serv_id, msg, &trans_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - msg=0x%04x", msg->msg_id);
		goto err;
	}

	// wait for response
	unsigned short result, error;
	struct qmimsg_t* rmsg = _wait_qmi_response(serv_id,
			QMIMGR_GENERIC_RESP_TIMEOUT, trans_id, &result, &error, NULL);

	if(!rmsg) {
		SYSLOG(LOG_ERROR, "qmi response timeout with serv_id:%u", serv_id);
		goto err;
	}

	if (result != QMI_RESULT_CODE_SUCCESS) {
		SYSLOG(LOG_ERROR, "Failed to set IPv%u pref on serv_id:%d error 0x%x",
				iptype, serv_id, error);
		goto err;
	}

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_start_profile(unsigned char iptype, int profile_index, int* pkt_data_handle, const char** call_end_msg)
{
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	int* phys_pkt_data_handle;
	unsigned short qmi_result;
	unsigned short qmi_error;
	const struct qmitlv_t* tlv;
	unsigned short tran_id;

	unsigned short* call_end_reason;
	if(call_end_msg)
		*call_end_msg = NULL;

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	if(profile_index > 255) {
		SYSLOG(LOG_ERROR, "profile_index(%d) too big", profile_index);
		goto err;
	}

	// set IP type preference on service id
	int serv_id = iptype == IPV4 ? QMIWDS : QMIIPV6;
	if (set_client_ip_pref(serv_id, iptype) < 0) {
		SYSLOG(LOG_ERROR, "Failed to set IP preference 0x%x for profile %d", iptype, profile_index);
		goto err;
	}

	SYSLOG(LOG_OPERATION, "requesting QMI_WDS_START_NETWORK_INTERFACE");

	qmimsg_clear_tlv(msg);
	qmimsg_set_msg_id(msg, QMI_WDS_START_NETWORK_INTERFACE);

	// add technology preference
	unsigned char byte_value = 0x1; //3GPP (no CDMA)
	if (qmimsg_add_tlv(msg, QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_TECHNOLOGY,
				sizeof(byte_value), &byte_value) < 0) {
		SYSLOG(LOG_ERROR, "add TLV tech_preference failed");
		goto err;
	}

	// add IP family preference
	byte_value = iptype;
	if (qmimsg_add_tlv(msg, QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_IPPREF,
				sizeof(byte_value), &byte_value) < 0){
		SYSLOG(LOG_ERROR, "add TLV IPv%u preference failed", iptype);
		goto err;
	}

	// set profile index
	byte_value = (unsigned char) profile_index;
	if (qmimsg_add_tlv(msg, QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_PROFILE_INDEX,
				sizeof(byte_value), &byte_value) < 0){
		SYSLOG(LOG_ERROR, "add TLV profile index failed");
		goto err;
	}

	// write
	if (qmiuniclient_write(uni, serv_id, msg, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - msg=0x%04x, serv_id=%d", msg->msg_id, serv_id);
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(serv_id, QMIMGR_PDP_CONT_ACTIVATE_RESP_TIMEOUT,
			tran_id, &qmi_result, &qmi_error, NULL);
	if(!rmsg) {
		SYSLOG(LOG_ERROR, "qmi response timeout");
		goto err;
	}

	// get verbose call end reason
#ifdef V_TELSTRA_SPEC
	tlv = _get_tlv(rmsg, QMI_WDS_START_NETWORK_INTERFACE_RESP_TYPE_VERBOSE_CALL_END_REASON, 4);
	if(tlv) {
		unsigned short verbose_call_end_type;
		unsigned short verbose_call_end_reason;
		int reason_key;
		const char* reason_msg;

		verbose_call_end_type=read16_from_little_endian(*(unsigned short*)tlv->v);
		//SYSLOG(LOG_OPERATION, "verbose_reason_type=%d", verbose_call_end_type);
SYSLOG(LOG_ERROR, "verbose_reason_type=%d========================", verbose_call_end_type);
		if(verbose_call_end_type==6) {
			verbose_call_end_reason=read16_from_little_endian(*(unsigned short*)(tlv->v+2));
SYSLOG(LOG_ERROR, "verbose_call_end_reason=%d=====================", verbose_call_end_reason);
		//	SYSLOG(LOG_OPERATION, "verbose_call_end_reason=%d", verbose_call_end_reason);

			reason_key = RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, verbose_call_end_reason);

			// lookup reason
			reason_msg = resourcetree_lookup(res_qmi_strings, RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, reason_key));

			// set return
			if(reason_msg && call_end_msg)
				*call_end_msg = reason_msg;
		}
	}
#endif
	//if can't find verbose call end reason
	if(!(*call_end_msg)) {

		tlv = _get_tlv(rmsg, QMI_WDS_START_NETWORK_INTERFACE_RESP_TYPE_CALL_END_REASON, sizeof(*call_end_reason));
		if(tlv) {
			int reason_key;
			const char* reason_msg;
			unsigned short phys_call_end_reason;

			// get reason key
			phys_call_end_reason = read16_from_little_endian(*(unsigned short*)tlv->v);
			reason_key = RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, phys_call_end_reason);

			SYSLOG(LOG_OPERATION, "phys_call_end_reason=%d", phys_call_end_reason);

			// lookup reason
			reason_msg = resourcetree_lookup(res_qmi_strings, RES_STR_KEY(QMIWDS, QMI_WDS_START_NETWORK_INTERFACE, reason_key));
			if(!reason_msg)
				reason_msg = _get_unknown_code(phys_call_end_reason, "[ref] QMI Definitions");

			// set return
			if(call_end_msg)
				*call_end_msg = reason_msg;
		}

	}

	if(qmi_result && qmi_error!=QMI_ERR_NO_EFFECT) {
		SYSLOG(LOG_ERROR,"qmi failure result");
		goto err;
	}

	// get phys_pkt_data_handle
	tlv = _get_tlv(rmsg, QMI_WDS_STOP_NETWORK_INTERFACE_REQ_TYPE, sizeof(*phys_pkt_data_handle));
	if(tlv) {
		phys_pkt_data_handle = (int*)tlv->v;
		*pkt_data_handle = *phys_pkt_data_handle;
	} else {
		SYSLOG(LOG_OPERATION, "QMI_WDS_STOP_NETWORK_INTERFACE_REQ_TYPE not found");
		goto err;
	}


	SYSLOG(LOG_OPERATION, "get QMI_WDS_START_NETWORK_INTERFACE response");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_stop_profile(int pkt_data_handle, unsigned char iptype)
{
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short qmi_result=0;
	unsigned short qmi_error=0;

	unsigned short tran_id;

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	int serv_id = iptype == IPV4 ? QMIWDS : QMIIPV6;

	if(_request_qmi_tlv(msg, serv_id, QMI_WDS_STOP_NETWORK_INTERFACE, &tran_id, QMI_WDS_STOP_NETWORK_INTERFACE_REQ_TYPE, sizeof(pkt_data_handle), &pkt_data_handle) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed");
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(serv_id, QMIMGR_PDP_CONT_DEACTIVATE_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result:%d, error:0x%x)", qmi_result, qmi_error);
		goto err;
	}

	SYSLOG(LOG_OPERATION, "get QMI_WDS_STOP_NETWORK_INTERFACE response");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

// get numeric value of pdp from the profile
static int get_pdp_type(const struct db_struct_profile* profile, int profile_type_pref, unsigned char* pdp)
{
	if(_dbstruct_is_valid_member(profile, pdp_type)) {
		if(!strcasecmp(profile->pdp_type, "PDP-IP") || !strcasecmp(profile->pdp_type, "IPV4"))
			*pdp = 0;
		else if(!strcasecmp(profile->pdp_type, "PDP-PPP"))
			*pdp = profile_type_pref ? 0 : 1;
		else if(!strcasecmp(profile->pdp_type, "PDP-IPV6") || !strcasecmp(profile->pdp_type, "IPV6"))
			*pdp = profile_type_pref ? 1 : 2;
		else if(!strcasecmp(profile->pdp_type, "PDP-IPV4V6") || !strcasecmp(profile->pdp_type, "IPV4V6"))
			*pdp = profile_type_pref ? 2 : 3;
		else {
			SYSLOG(LOG_ERROR, "unknown PDP type specified - %s. "
					"must be PDPD-IP, PDP-PPP, PDP-IPV6 or PDP-IPV4V6", profile->pdp_type);
			return -1;
		}
	} else {
		*pdp = 0;
	}
   return 0;
}

int _qmi_create_profile(int profile_index, int profile_type_pref, const char* profile_name, struct db_struct_profile* profile, unsigned short* error, unsigned short* ext_error)
{
	unsigned char profile_type;

	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;

	unsigned short qmi_result;
	unsigned short qmi_error;
	unsigned short qmi_ext_error;
	unsigned short tran_id;

	int tlv_count;
	int i;

	int qmi_wds_cmd;

	*error = -1;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	tlv_count = 0;

	SYSLOG(LOG_DEBUG, "clearing TLVs");

	// add TLSv
	qmimsg_clear_tlv(msg);

	profile_type = profile_type_pref ? 1 : 0;

	// set request and request dependent TLVs
	if(profile_index < 0) {
		SYSLOG(LOG_DEBUG, "creating a new profile");

		qmimsg_set_msg_id(msg, QMI_WDS_CREATE_PROFILE);

		// add TLV - profile type
		qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_TYPE, sizeof(profile_type), &profile_type);
		tlv_count++;
		SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_TYPE");
	} else {
		struct qmi_wds_profile_index_reqresp_t req;

		SYSLOG(LOG_DEBUG, "modifying a profile - %d", profile_index);

		qmimsg_set_msg_id(msg, QMI_WDS_MODIFY_PROFILE_SETTINGS);

		if(profile_index > 255) {
			SYSLOG(LOG_ERROR, "profile_index(%d) too big", profile_index);
			goto err;
		}

		req.profile_type = profile_type;
		req.profile_index = (char)profile_index;

		tlv_count++;
		qmimsg_add_tlv(msg, QMI_WDS_MODIFY_PROFILE_SETTINGS_REQ_TYPE, sizeof(req), &req);

		SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_MODIFY_PROFILE_SETTINGS_REQ_TYPE");
	}

	/*
		MC7304 v5.5.26.2 does not allow to change the names. To avoid this module issue, router does not change names of profiles
		in the module and the router works based on profile index number.
	*/

#if 0
	// tlv - profile name
	qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_NAME, strlen(profile_name), profile_name);
	tlv_count++;
	SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_NAME");
#endif

	// tlv - pdp type
	unsigned char raw_pdp_type;
	if (get_pdp_type(profile, profile_type_pref, &raw_pdp_type) < 0) {
		goto err;
	}

	qmi_wds_cmd = profile_type_pref ? QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_PDN_TYPE : QMI_WDS_PROFILE_REQRESP_TYPE_PDP_TYPE;
	qmimsg_add_tlv(msg, qmi_wds_cmd, sizeof(raw_pdp_type), &raw_pdp_type);
	tlv_count++;
	SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_PDP_TYPE");

	// tlv - apn name
	qmi_wds_cmd = profile_type_pref ? QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_APN_NAME : QMI_WDS_PROFILE_REQRESP_TYPE_APN_NAME;
	if(_dbstruct_is_valid_member(profile, apn_name))
		qmimsg_add_tlv(msg, qmi_wds_cmd, strlen(profile->apn_name), profile->apn_name);
	else
		qmimsg_add_tlv(msg, qmi_wds_cmd, 1, "");
	tlv_count++;
	SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_APN_NAME");

	// tlv - user name
	qmi_wds_cmd = profile_type_pref ? QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_USER : QMI_WDS_PROFILE_REQRESP_TYPE_USER;
	if(_dbstruct_is_valid_member(profile, user))
		qmimsg_add_tlv(msg, qmi_wds_cmd, strlen(profile->user), profile->user);
	else
		qmimsg_add_tlv(msg, qmi_wds_cmd, 1, "");
	tlv_count++;
	SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_USER");

	// tlv - pass
	qmi_wds_cmd = profile_type_pref ? QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_PASS : QMI_WDS_PROFILE_REQRESP_TYPE_PASS;
	if(_dbstruct_is_valid_member(profile, pass))
		qmimsg_add_tlv(msg, qmi_wds_cmd, strlen(profile->pass), profile->pass);
	else
		qmimsg_add_tlv(msg, qmi_wds_cmd, 1, "");
	tlv_count++;
	SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_PASS");

	// tlv - auth
	unsigned char raw_auth_preference;
	if(_dbstruct_is_valid_member(profile, auth_preference)) {
		if(!strcasecmp(profile->auth_preference, "PAP"))
			raw_auth_preference = QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_PAP;
		else if(!strcasecmp(profile->auth_preference, "CHAP"))
			raw_auth_preference = QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_CHAP;
		else if(!strcasecmp(profile->auth_preference, "PAP|CHAP") || !strcasecmp(profile->auth_preference, "CHAP|PAP"))
			raw_auth_preference = QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_PAP | QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_CHAP;
		else if(!strcasecmp(profile->auth_preference, "NONE")) {
			raw_auth_preference = 0;
		} else {
			SYSLOG(LOG_ERROR, "unknown AUTH preference specified %s - must be PAP, CHAP or CHAP|PAP", profile->auth_preference);
			goto err;
		}
	} else {
		raw_auth_preference = QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_PAP | QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_CHAP;
	}

	/* use none as authentication if no user and password is not provided */
	if(!_dbstruct_is_valid_member(profile, user) && !_dbstruct_is_valid_member(profile, pass)) {
		raw_auth_preference = 0;

		SYSLOG(LOG_DEBUG, "[auth] no user name and password found - divert to NONE auth");
	} else {
		SYSLOG(LOG_DEBUG, "[auth] use %s", profile->auth_preference);
	}


	qmi_wds_cmd = profile_type_pref ? QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_AUTH : QMI_WDS_PROFILE_REQRESP_TYPE_AUTH;
	qmimsg_add_tlv(msg, qmi_wds_cmd, sizeof(raw_auth_preference), &raw_auth_preference);

	tlv_count++;
	SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_AUTH");

	// tlv - ipv4 address preference - only for 3GPP
	if(!profile_type_pref) {
		if(_dbstruct_is_valid_member(profile, ipv4_addr_pref)) {
			struct qmi_wds_profile_reqresp_ipv4_addr_pref pref_ip;
			in_addr_t prefIp = inet_network(profile->ipv4_addr_pref);
			if(prefIp >= 0) {
				memcpy(pref_ip.ipv4_addr_pref, &prefIp, sizeof(pref_ip.ipv4_addr_pref));
				qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_IPV4_ADDR_PREF, sizeof(pref_ip.ipv4_addr_pref), &pref_ip.ipv4_addr_pref);
				tlv_count++;
				SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_IPV4_ADDR_PREF");
			}
		}
	}

	// tlv - tft
	struct qmi_wds_profile_reqresp_tft_id tft[2];

	for(i = 0; i < __countof(profile->tft_id); i++) {

		if(_dbstruct_is_valid_member(&profile->tft_id[i], filter_id)) {
			tft[i].filter_id = profile->tft_id[i].filter_id;
			tft[i].eval_id = profile->tft_id[i].eval_id;

			if(!strcasecmp(profile->tft_id[i].ip_version, "IPv4")) {
				tft[i].ip_version = 4;
			} else if(!strcasecmp(profile->tft_id[i].ip_version, "IPv6")) {
				tft[i].ip_version = 6;
			} else {
				SYSLOG(LOG_ERROR, "unknown IP version specified %s - must be IPv4 or IPv6 - using IPv4 for sbackward compatibility", profile->tft_id[i].ip_version);
				goto err;
			}

			inet_aton(profile->tft_id[i].source_ip, (struct in_addr*)tft[i].source_ip);
			tft[i].source_ip_mask = profile->tft_id[i].source_ip_mask;
			tft[i].next_header = profile->tft_id[i].next_header;

			SYSLOG(LOG_DEBUG, "i=%d, dest_port_range_start=%d", i, profile->tft_id[i].dest_port_range_start);
			SYSLOG(LOG_DEBUG, "i=%d, dest_port_range_end=%d", i, profile->tft_id[i].dest_port_range_end);
			SYSLOG(LOG_DEBUG, "i=%d, src_port_range_start=%d", i, profile->tft_id[i].src_port_range_start);
			SYSLOG(LOG_DEBUG, "i=%d, src_port_range_end=%d", i, profile->tft_id[i].src_port_range_end);

			write16_to_little_endian(profile->tft_id[i].dest_port_range_start, tft[i].dest_port_range_start);
			write16_to_little_endian(profile->tft_id[i].dest_port_range_end, tft[i].dest_port_range_end);
			write16_to_little_endian(profile->tft_id[i].src_port_range_start, tft[i].src_port_range_start);
			write16_to_little_endian(profile->tft_id[i].src_port_range_end, tft[i].src_port_range_end);
			write32_to_little_endian(profile->tft_id[i].ipsec_spi, tft[i].ipsec_spi);
			write16_to_little_endian(profile->tft_id[i].tos_mask, tft[i].tos_mask);
			write32_to_little_endian(profile->tft_id[i].flow_label, tft[i].flow_label);

			qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_TFT_ID1 + i, sizeof(tft[i]), &tft[i]);
			tlv_count++;

			SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_TFT_ID%d", i);
		}
	}

	// tlv - pdp context secondary flag
	unsigned char raw_pdp_context_number;
	if(_dbstruct_is_valid_member(profile, pdp_context_number)) {
		raw_pdp_context_number = profile->pdp_context_number;

		qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_PDP_CONTEXT_NUMBER, sizeof(raw_pdp_context_number), &raw_pdp_context_number);
		tlv_count++;
		SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_PDP_CONTEXT_NUMBER");
	}


	// tlv - pdp context secondary flag
	unsigned char raw_pdp_context_sec_flag;
	if(_dbstruct_is_valid_member(profile, pdp_context_sec_flag)) {
		raw_pdp_context_sec_flag = profile->pdp_context_sec_flag;
		qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_SECONDARY_FLAG, sizeof(raw_pdp_context_sec_flag), &raw_pdp_context_sec_flag);
		tlv_count++;
		SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_SECONDARY_FLAG");
	}


	// tlv - pdp context primary id
	unsigned char raw_pdp_context_primary_id;
	if(_dbstruct_is_valid_member(profile, pdp_context_primary_id)) {
		raw_pdp_context_primary_id = profile->pdp_context_primary_id;

		qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_PRIMARY_ID, sizeof(raw_pdp_context_primary_id), &raw_pdp_context_primary_id);
		tlv_count++;
		SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_PRIMARY_ID");
	}

	// tlv - addr allocation preference
	unsigned char raw_addr_allocation_preference;
	if(_dbstruct_is_valid_member(profile, addr_allocation_preference)) {
		if(!strcasecmp(profile->addr_allocation_preference, "NAS")) {
			raw_addr_allocation_preference = 0;
		} else if(!strcasecmp(profile->addr_allocation_preference, "DHCP")) {
			raw_addr_allocation_preference = 1;
		} else {
			SYSLOG(LOG_ERROR, "unknown addr allocation preference specified %s - msut be NAS or DHCP", profile->addr_allocation_preference);
			goto err;
		}

		qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_ADDR_ALLOCATION_PREFERENCE, sizeof(raw_addr_allocation_preference), &raw_addr_allocation_preference);
		tlv_count++;
		SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_ADDR_ALLOCATION_PREFERENCE");
	}

	// tlv - qci
	struct qmi_wds_profile_reqresp_type_qci qci;
	if(_dbstruct_is_valid_member(profile, qci)) {
		qci.qci = profile->qci;

		SYSLOG(LOG_DEBUG, "g_dl_bit_rate=%d", profile->g_dl_bit_rate);
		SYSLOG(LOG_DEBUG, "max_dl_bit_rate=%d", profile->max_dl_bit_rate);
		SYSLOG(LOG_DEBUG, "g_ul_bit_rate=%d", profile->g_ul_bit_rate);
		SYSLOG(LOG_DEBUG, "max_ul_bit_rate=%d", profile->max_ul_bit_rate);

		write32_to_little_endian(profile->g_dl_bit_rate, qci.g_dl_bit_rate);
		write32_to_little_endian(profile->max_dl_bit_rate, qci.max_dl_bit_rate);
		write32_to_little_endian(profile->g_ul_bit_rate, qci.g_ul_bit_rate);
		write32_to_little_endian(profile->max_ul_bit_rate, qci.max_ul_bit_rate);

		qmimsg_add_tlv(msg, QMI_WDS_PROFILE_REQRESP_TYPE_QCI, sizeof(qci), &qci);
		tlv_count++;

		SYSLOG(LOG_DEBUG, "tlv added - QMI_WDS_PROFILE_REQRESP_TYPE_QCI");
	}

	// check validation
	if(msg->tlv_count != tlv_count) {
		SYSLOG(LOG_ERROR, "failed to add any of TLVs into msg(msg_id=0x%04x) - cur=%d,added=%d", msg->msg_id, msg->tlv_count, tlv_count);
		goto err;
	}

	// write
	SYSLOG(LOG_DEBUG, "writing msg(0x%04x) request", msg->msg_id);
	if(qmiuniclient_write(uni, QMIWDS, msg, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - msg=0x%04x, serv_id=%d", msg->msg_id, QMIWDS);
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMIWDS, QMIMGR_MIDLONG_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, &qmi_ext_error);
	if(!rmsg) {
// This work-around is applied to products with Vodafone manual roaming algorithm and Sierra MC7304 module.
// Because in some networks also very rarely, "Best Network Retry Sub-Process" causes APN writing failure on Sierra MC7304 module,
// And the failure status is never recovered until rebooting the module.
#if defined(V_MANUAL_ROAMING_vdfglobal) && defined(MODULE_MC7304)
		SYSLOG(LOG_ERROR, "power-cycling module - workaround for module malfunctions");
		system("reboot_module.sh --checkprofiles &");
#endif
		SYSLOG(LOG_ERROR, "qmi response timeout");
		goto err;
	}

	if(qmi_result) {
		SYSLOG(LOG_ERROR, "qmi failure result");

		*error = qmi_error;
		*ext_error = qmi_ext_error;
		goto err;
	}

	SYSLOG(LOG_OPERATION, "get QMI_WDS_CREATE_PROFILE response");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);

	return -1;
}

int _qmi_wds_get_mtu_size(int reset_db_only)
{
	struct qmi_easy_req_t er;
	const struct qmitlv_t* tlv;
	struct qmi_wds_get_runtime_settings_req req;
	int rc;

	struct qmi_wds_get_runtime_settings_resp_type_mtu* resp;

	if (reset_db_only == 1) {
		_set_reset_db("module_info.MTU");
		return 0;
	}
	rc = _qmi_easy_req_init(&er, QMIWDS, QMI_WDS_GET_RUNTIME_SETTINGS);
	if(rc < 0)
		goto err;

	memset(&req, 0, sizeof(req));
	req.requested_settings=0x2000; //Query MTU size
	if(qmimsg_add_tlv(er.msg, QMI_WDS_GET_RUNTIME_SETTINGS_REQ_TYPE, sizeof(req), &req) < 0) {
		SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv() - QMI_WDS_GET_RUNTIME_SETTINGS_REQ_TYPE");
		goto err;
	}

	/* init. tlv */
	rc = _qmi_easy_req_do(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc < 0)
		goto err;

	tlv = _get_tlv(er.rmsg, QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_MTU, sizeof(*resp));
	if(!tlv)
		goto err;

	resp = tlv->v;

	SYSLOG(LOG_ERROR, "Module MTU size=[%d]",resp->mtu);

	_set_int_db("module_info.MTU", resp->mtu, NULL);
	_qmi_easy_req_fini(&er);
	return rc;

err:
	_set_reset_db("module_info.MTU");
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_dms_control_pin(int pin_id, const char* pin, int action, int* verify_left, int* unblock_left)
{
	void* v;
	struct qmi_dms_uim_set_pin_protection_req* prot_req = NULL;
	struct qmi_dms_uim_verify_pin_req* verify_req = NULL;
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short tran_id;
	int pin_len;
	const struct qmitlv_t* tlv;

	unsigned short msg_id;
	unsigned char t;
	unsigned short l;

	/*
		action

		0 = disable pin
		1 = enable pin
		2 = verify pin
	*/

	if(verify_left)
		*verify_left = -1;
	if(unblock_left)
		*unblock_left = -1;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	// get pin length
	pin_len = strlen(pin);
	if(pin_len > 255) {
		SYSLOG(LOG_ERROR, "too long pin - pin_len=%d", pin_len);
		goto err;
	}

	if(action == 2) {
		// create prot_req
		verify_req = _malloc(sizeof(*verify_req) + pin_len);
		if(!verify_req) {
			SYSLOG(LOG_ERROR, "failed to allocate verify_req - pin_len=%d", pin_len);
			goto err;
		}

		// build request
		verify_req->pin_id = (unsigned char)(pin_id == 0) ? 1 : 2;
		verify_req->pin_length = (unsigned char)pin_len;
		strncpy((char*)verify_req->pin_value, pin, pin_len);

		// set trans parameters
		msg_id = QMI_DMS_UIM_VERIFY_PIN;
		t = QMI_DMS_UIM_VERIFY_PIN_REQ_TYPE;
		l = sizeof(*verify_req) + pin_len;
		v = verify_req;
	} else {
		// create prot_req
		prot_req = _malloc(sizeof(*prot_req) + pin_len);
		if(!prot_req) {
			SYSLOG(LOG_ERROR, "failed to allocate prot_req - pin_len=%d", pin_len);
			goto err;
		}

		// build request
		prot_req->pin_id = (unsigned char)(pin_id == 0) ? 1 : 2;
		prot_req->protection_setting = (unsigned char)(action == 0 ? 0 : 1);
		prot_req->pin_length = (unsigned char)pin_len;
		strncpy((char*)prot_req->pin_value, pin, pin_len);

		// set trans parameters
		msg_id = QMI_DMS_UIM_SET_PIN_PROTECTION;
		t = QMI_DMS_UIM_SET_PIN_PROTECTION_REQ_TYPE;
		l = sizeof(*prot_req) + pin_len;
		v = prot_req;
	}

	unsigned short qmi_result;
	unsigned short qmi_error;

	// request QMI_WDS_DELETE_PROFILE
	if(_request_qmi_tlv(msg, QMIDMS, msg_id, &tran_id, t, l, v) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed");
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, NULL);
	if(!rmsg) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	// process error
	if(qmi_result) {
		struct qmi_dms_uim_pin_resp* resp;

		tlv = _get_tlv(rmsg, QMI_DMS_UIM_SET_PIN_PROTECTION_RESP_TYPE, sizeof(struct qmi_dms_uim_pin_resp));
		if(!tlv) {
			SYSLOG(LOG_ERROR, "mandatory resp type not found");
			goto err;
		}

		resp = (struct qmi_dms_uim_pin_resp*)tlv->v;

		if(verify_left)
			*verify_left = resp->verify_retries_left;
		if(unblock_left)
			*unblock_left = resp->unblock_retries_left;
	}

	SYSLOG(LOG_OPERATION, "QMI_DMS_UIM_SET_PIN_PROTECTION_RESP_TYPE (or QMI_DMS_UIM_VERIFY_PIN_REQ_TYPE) success");

	_free(prot_req);
	_free(verify_req);

	qmimsg_destroy(msg);
	return 0;

err:
	_free(prot_req);
	_free(verify_req);

	qmimsg_destroy(msg);
	return -1;
}

int _qmi_disable_pin(int pin_id, const char* pin, int* verify_left, int* unblock_left)
{
	return _qmi_control_pin(pin_id, pin, 0, verify_left, unblock_left);
}

int _qmi_enable_pin(int pin_id, const char* pin, int* verify_left, int* unblock_left)
{
	return _qmi_control_pin(pin_id, pin, 1, verify_left, unblock_left);
}

int _qmi_verify_pin(int pin_id, const char* pin, int* verify_left, int* unblock_left)
{
	return _qmi_control_pin(pin_id, pin, 2, verify_left, unblock_left);
}

int _qmi_dms_change_pin(int pin_id, const char* pin, const char* newpin, int* verify_left, int* unblock_left)
{
	struct qmi_dms_uim_change_pin_req* req = NULL;
	struct qmi_dms_uim_change_pin_req2* req2;
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short tran_id;
	const struct qmitlv_t* tlv;
	unsigned short qmi_result;

	int pin_len;
	int newpin_len;

	unsigned short l;

	if(verify_left)
		*verify_left = -1;
	if(unblock_left)
		*unblock_left = -1;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	pin_len = strlen(pin);
	newpin_len = strlen(newpin);

	// get pin length
	if(pin_len > 255 || newpin_len > 255) {
		SYSLOG(LOG_ERROR, "too long pin - pin_len=%d,newpin_len=%d", pin_len, newpin_len);
		goto err;
	}

	// create prot_req
	l = sizeof(*req) + pin_len + sizeof(*req2) + newpin_len;
	req = _malloc(l);
	if(!req) {
		SYSLOG(LOG_ERROR, "failed to allocate req - pin_len=%d", pin_len);
		goto err;
	}
	// get req2
	req2 = (struct qmi_dms_uim_change_pin_req2*)((char*)req + sizeof(*req) + pin_len);

	// build request
	req->pin_id = (unsigned char)(pin_id == 0) ? 1 : 2;
	req->old_pin_length = (unsigned char)pin_len;
	strncpy((char*)req->old_pin_value, pin, pin_len);
	req2->new_pin_length = (unsigned char)newpin_len;
	strncpy((char*)req2->new_pin_value, newpin, newpin_len);

	// request QMI_WDS_DELETE_PROFILE
	if(_request_qmi_tlv(msg, QMIDMS, QMI_DMS_UIM_CHANGE_PIN, &tran_id, QMI_DMS_UIM_CHANGE_PIN_REQ_TYPE, l, req) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed");
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	// process error
	if(qmi_result) {
		struct qmi_dms_uim_pin_resp* resp;

		tlv = _get_tlv(rmsg, QMI_DMS_UIM_SET_PIN_PROTECTION_RESP_TYPE, sizeof(struct qmi_dms_uim_pin_resp));
		if(!tlv) {
			SYSLOG(LOG_ERROR, "mandatory resp type not found");
			goto err;
		}

		resp = (struct qmi_dms_uim_pin_resp*)tlv->v;

		if(verify_left)
			*verify_left = resp->verify_retries_left;
		if(unblock_left)
			*unblock_left = resp->unblock_retries_left;
	}

	SYSLOG(LOG_OPERATION, "QMI_DMS_UIM_CHANGE_PIN success");

	_free(req);

	qmimsg_destroy(msg);
	return 0;

err:
	_free(req);

	qmimsg_destroy(msg);
	return -1;
}

int _qmi_dms_verify_puk(int pin_id, const char* puk, const char* pin, int* verify_left, int* unblock_left)
{
	struct qmi_dms_uim_unblock_pin_req* req = NULL;
	struct qmi_dms_uim_unblock_pin_req2* req2;
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short tran_id;
	const struct qmitlv_t* tlv;

	int puk_len;
	int pin_len;

	unsigned short qmi_result;

	unsigned short l;

	if(verify_left)
		*verify_left = -1;
	if(unblock_left)
		*unblock_left = -1;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	puk_len = strlen(puk);
	pin_len = strlen(pin);

	// get pin length
	if(pin_len > 255 || puk_len > 255) {
		SYSLOG(LOG_ERROR, "too long pin - pin_len=%d,puk_len=%d", pin_len, puk_len);
		goto err;
	}

	// create prot_req
	l = sizeof(*req) + puk_len + sizeof(*req2) + pin_len;
	req = _malloc(l);
	if(!req) {
		SYSLOG(LOG_ERROR, "failed to allocate req - pin_len=%d", pin_len);
		goto err;
	}
	// get req2
	req2 = (struct qmi_dms_uim_unblock_pin_req2*)((char*)req + sizeof(*req) + puk_len);

	// build request
	req->unblock_pin_id = (unsigned char)(unsigned char)(pin_id == 0) ? 1 : 2;
	req->puk_length = (unsigned char)puk_len;
	strncpy((char*)req->puk_value, puk, puk_len);
	req2->new_pin_length = (unsigned char)pin_len;
	strncpy((char*)req2->new_pin_value, pin, pin_len);

	// request QMI_WDS_DELETE_PROFILE
	if(_request_qmi_tlv(msg, QMIDMS, QMI_DMS_UIM_UNBLOCK_PIN, &tran_id, QMI_DMS_UIM_UNBLOCK_PIN_REQ_TYPE, l, req) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed");
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	// process error
	if(qmi_result) {
		struct qmi_dms_uim_pin_resp* resp;

		tlv = _get_tlv(rmsg, QMI_DMS_UIM_UNBLOCK_PIN_RESP_TYPE, sizeof(struct qmi_dms_uim_pin_resp));
		if(!tlv) {
			SYSLOG(LOG_ERROR, "mandatory resp type not found");
			goto err;
		}

		resp = (struct qmi_dms_uim_pin_resp*)tlv->v;

		if(verify_left)
			*verify_left = resp->verify_retries_left;
		if(unblock_left)
			*unblock_left = resp->unblock_retries_left;
	}

	SYSLOG(LOG_OPERATION, "QMI_DMS_UIM_CHANGE_PIN success");

	_free(req);

	qmimsg_destroy(msg);
	return 0;

err:
	_free(req);

	qmimsg_destroy(msg);
	return -1;
}

int _qmi_dms_check_pin_status(int wait)
{
	struct qmimsg_t* msg;
	unsigned short tran_id;

	SYSLOG(LOG_DEBUG, "checking pin status - QMI_DMS_UIM_GET_PIN_STATUS");

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg - QMI_DMS_UIM_GET_PIN_STATUS");
		goto err;
	}

	if(_request_qmi(msg, QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed - QMI_DMS_UIM_GET_PIN_STATUS");
		goto err;
	}

	// wait for response
	if(wait &&
	   _wait_qmi_until(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_get_ps_attach(int timeout, int* att)
{
	unsigned short tran_id;
	unsigned short qmi_result;
	const struct qmitlv_t* tlv;

	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;

	struct qmi_nas_get_serving_system_resp* resp;

	SYSLOG(LOG_OPERATION, "getting ps attach status - timeout=%d", timeout);

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	// QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE - current service
	_request_qmi(msg, QMINAS, QMI_NAS_GET_SERVING_SYSTEM, &tran_id);

	// wait for response
	rmsg = _wait_qmi_response(QMINAS, timeout, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "checking result - QMI_NAS_GET_SERVING_SYSTEM");

	// get QMI_WDS_GET_PROFILE_LIST_RESP_TYPE
	tlv = _get_tlv(rmsg, QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE, sizeof(struct qmi_nas_get_serving_system_resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR, "QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE not found in QMI_NAS_GET_SERVING_SYSTEM");
		goto err;
	}

	// get result validation
	resp = (struct qmi_nas_get_serving_system_resp*)tlv->v;
	if(resp->ps_attach_stat != 1 && resp->ps_attach_stat != 2) {
		SYSLOG(LOG_ERROR, "incorrect ps attach stat (%d) not found in QMI_NAS_GET_SERVING_SYSTEM", resp->ps_attach_stat);
		goto err;
	}

	SYSLOG(LOG_OPERATION, "got result - %d", resp->ps_attach_stat);

	if(att)
		*att = resp->ps_attach_stat == 1;

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_wait_until_ps_attach(int att, int timeout)
{
	clock_t cur;
	clock_t start;
	clock_t timepast;

	int cur_att;

	int stat = -1;

	start = cur = _get_current_sec();

	while((timepast = (cur - start)) < timeout) {

		SYSLOG(LOG_DEBUG, "waiting - att=%d", att);

		stat = _qmi_get_ps_attach(timeout - timepast, &cur_att);
		if(!stat) {
			if((cur_att && att) || (!cur_att && !att)) {
				SYSLOG(LOG_DEBUG, "matched - cur_att=%d,att=%d", cur_att, att);
				break;
			} else {
				SYSLOG(LOG_DEBUG, "not matched - cur_att=%d,att=%d", cur_att, att);
			}
		}

		cur = _get_current_sec();
	}

	return stat;
}

int _qmi_attach(int att)
{
	unsigned short tran_id;
	unsigned short qmi_result;
	unsigned short qmi_error;

	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;

	char ps_attach_action;

	SYSLOG(LOG_OPERATION, "initiate attach req - %d", att);

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "requesting QMI_NAS_INITIATE_ATTACH");

	// build request
	ps_attach_action = att ? 0x01 : 0x02;

	// request QMI_NAS_INITIATE_ATTACH
	if(_request_qmi_tlv(msg, QMINAS, QMI_NAS_INITIATE_ATTACH, &tran_id, QMI_NAS_INITIATE_ATTACH_REQ_TYPE_PS_ATTACH_ACTION, sizeof(ps_attach_action), &ps_attach_action) < 0) {
		SYSLOG(LOG_ERROR, "failed to request for QMI_NAS_INITIATE_ATTACH");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "waiting for response - QMI_NAS_INITIATE_ATTACH");

	// wait for response
	if(att == 1)
		rmsg = _wait_qmi_response(QMINAS, QMIMGR_PS_ATTACH_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, NULL);
	else
		rmsg = _wait_qmi_response(QMINAS, QMIMGR_PS_DETACH_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, NULL);

	if(!rmsg) {
		SYSLOG(LOG_ERROR, "qmi response timeout");
		goto err;
	}

	if(qmi_result && (qmi_error != QMI_ERR_NO_EFFECT)) {
		SYSLOG(LOG_ERROR, "qmi failure result)");
		goto err;
	}


	SYSLOG(LOG_OPERATION, "QMI_NAS_INITIATE_ATTACH success - att=%d", att);

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

int _qmi_network_select(const char* plmn_select)
{
	int mode;
	int mcc;
	int mnc;
	int count;

	unsigned char register_action;
	struct qmi_nas_initiate_network_register_req_network_info req;
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short tran_id;
	unsigned short qmi_result;

	/*
		* 3GPP
		0 automatic (<oper> field is ignored)
		1 manual (<oper> field shall be present, and <AcT> optionally)

		* mode
		auto (mode=0), 2G (mode=2), 3G (mode=1) LTE (mode=3)

	*/
	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	// get parameters
	mode = mcc = mnc = 0;
	count = sscanf(plmn_select, "%d,%d,%d", &mode, &mcc, &mnc);

	SYSLOG(LOG_OPERATION, "count=%d,mode=%d,mcc=%d,mnc=%d", count, mode, mcc, mnc);

	// check validation
	if(count < 1) {
		SYSLOG(LOG_ERROR, "incorrect format detected in plmn_select (%s)", plmn_select);
		goto err;
	}

	// set msg id and clear tlv - get msg ready
	qmimsg_set_msg_id(msg, QMI_NAS_INITIATE_NETWORK_REGISTER);
	qmimsg_clear_tlv(msg);

	// set register_action
	register_action = (mode == 0) ? 1 : 2;
	SYSLOG(LOG_OPERATION, "adding register_action tlv - register_action=%d", register_action);
	if(qmimsg_add_tlv(msg, QMI_NAS_INITIATE_NETWORK_REGISTER_REQ_TYPE_REG_ACTION, sizeof(register_action), &register_action) < 0) {
		SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv() - QMI_NAS_INITIATE_NETWORK_REGISTER_REQ_TYPE_REG_ACTION");
		goto err;
	}

	// set network registeration information if manual
	if(mode > 0) {
		write16_to_little_endian(mcc, req.mobile_country_code);
		write16_to_little_endian(mnc, req.mobile_network_code);

		switch(mode) {
		case 1:
			req.radio_access_technology = 5; // UMTS
			break;

		case 2:
			req.radio_access_technology = 4; // GSM
			break;

		case 3:
			req.radio_access_technology = 8; // LTE
			break;

		default:
			SYSLOG(LOG_ERROR, "unknown mode (%d) found", mode);
			goto err;
			break;
		}

		SYSLOG(LOG_OPERATION, "adding network_info tlv - rat=%d", req.radio_access_technology);
		if(qmimsg_add_tlv(msg, QMI_NAS_INITIATE_NETWORK_REGISTER_REQ_TYPE_NETWORK_INFO, sizeof(req), &req) < 0) {
			SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv() - QMI_WDS_MODIFY_PROFILE_SETTINGS_REQ_TYPE");
			goto err;
		}
	}


	// write
	SYSLOG(LOG_DEBUG, "writing msg request - QMI_NAS_INITIATE_NETWORK_REGISTER");
	if(qmiuniclient_write(uni, QMINAS, msg, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - msg=0x%04x, serv_id=%d", msg->msg_id, QMINAS);
		goto err;
	}

	// wait for response
	rmsg = _wait_qmi_response(QMINAS, QMIMGR_LONG_RESP_TIMEOUT, tran_id, &qmi_result, NULL, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	// wait for QMI_NAS_GET_SERVING_SYSTEM #1 - no service
	SYSLOG(LOG_OPERATION, "waiting for QMI_NAS_GET_SERVING_SYSTEM indication #1");
	if(_wait_qmi_until(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, 0, QMI_NAS_GET_SERVING_SYSTEM) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout - QMIMGR_GENERIC_RESP_TIMEOUT #1");
	}

	// wait for QMI_NAS_GET_SERVING_SYSTEM #2 - new service
	SYSLOG(LOG_OPERATION, "waiting for QMI_NAS_GET_SERVING_SYSTEM indication #2");
	if(_wait_qmi_until(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, 0, QMI_NAS_GET_SERVING_SYSTEM) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout - QMIMGR_GENERIC_RESP_TIMEOUT #2");
	}

	// wait for QMI_NAS_GET_SERVING_SYSTEM #3 - registering
	SYSLOG(LOG_OPERATION, "waiting for QMI_NAS_GET_SERVING_SYSTEM indication #2");
	if(_wait_qmi_until(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, 0, QMI_NAS_GET_SERVING_SYSTEM) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout - QMIMGR_GENERIC_RESP_TIMEOUT #2");
	}

	SYSLOG(LOG_OPERATION, "system_mode = %s", _get_str_db("system_network_status.system_mode", ""));
	SYSLOG(LOG_OPERATION, "MCC = %s", _get_str_db("system_network_status.MCC", ""));
	SYSLOG(LOG_OPERATION, "MNC = %s", _get_str_db("system_network_status.MNC", ""));


	SYSLOG(LOG_OPERATION, "QMI_NAS_INITIATE_NETWORK_REGISTER_REQ_TYPE_REG_ACTION success");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

const char* _at_network_scan()
{
	char operator[QMIMGR_MAX_DB_VALUE_LENGTH];
	char el[QMIMGR_MAX_DB_VALUE_LENGTH];
	unsigned short tran_id;
	const char* result;

	int stat3gpp;
	int act3gpp;

	int stat;
	int act;

	char operator_name[64];
	char mccmnc[6 + 1];
	int mccmnc_len;

	char mcc[3 + 1];
	char mnc[3 + 1];

	static char plmn_list[QMIMGR_MAX_DB_BIGVALUE_LENGTH];
	int plmn_list_idx;

	char buf[QMIMGR_MAX_DB_VALUE_LENGTH];
	int buf_len;

	const char* cmd;

	int i;

	struct strqueue_t* q;

	// request - set long operator
	cmd = "AT+COPS=3,0";
	SYSLOG(LOG_DEBUG, "sending - %s", cmd);
	if(_request_at(cmd, QMIMGR_GENERIC_RESP_TIMEOUT, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue cmd - %s", cmd);
		goto err;
	}

	// get result
	SYSLOG(LOG_DEBUG, "waiting - %s", cmd);
	q = _wait_at_response(tran_id, QMIMGR_AT_SELECT_TIMEOUT);

	// get result
	SYSLOG(LOG_DEBUG, "waiting - %s", cmd);
	result = _get_at_result(q, 1, 1, "OK");
	if(!result) {
		SYSLOG(LOG_ERROR, "incorrect result - cmd=%s", cmd);
		goto err;
	}

	// request - get operators
	cmd = "AT+COPS=?";
	SYSLOG(LOG_DEBUG, "sending - %s", cmd);
	if(_request_at(cmd, QMIMGR_LONG_RESP_TIMEOUT, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue cmd - %s", cmd);
		goto err;
	}

	/*
		AT+COPS=?
		+COPS: (2,"Telstra Mobile","Telstra","50501",2),(1,"Telstra Mobile","Telstra","50501",0),(1,"3TELSTRA","3TELSTRA","50506",2),(3,"vodafone AU","voda AU","50503",2),(3,"YES OPTUS","Optus","50502",0),(3,"vodafone AU","voda AU","50503",0),(3,"YES OPTUS","Optus","50502",2),,(0,1,2,3,4),(0,1,2)

		OK
	*/

	// get result
	SYSLOG(LOG_DEBUG, "waiting - %s", cmd);
	q = _wait_at_response(tran_id, QMIMGR_AT_SELECT_TIMEOUT);

	// get result
	result = _get_at_result(q, 4, 1, "+COPS:");
	if(!result) {
		SYSLOG(LOG_ERROR, "incorrect result - cmd=%s", cmd);
		goto err;
	}

	plmn_list_idx = 0;

	// get band
	i = 0;
	while(!(__strtok(result, ",", "()", i++, operator, sizeof(operator)) < 0)) {

		// operator : 2,"Telstra Mobile","Telstra","50501",2

		SYSLOG(LOG_DEBUG, "operator#%d - %s", i, operator);

		// get stat3gpp
		if(__strtok(operator, ",", "\"", 0, el, sizeof(el)) < 0) {
			SYSLOG(LOG_ERROR, "failed to get stat3gpp(0) from COPS command - %s", operator);
			continue;
		}
		stat3gpp = atoi(el);
		SYSLOG(LOG_DEBUG, "operator#%d - stat=%d", i, stat3gpp);


		// get act3gpp
		if(__strtok(operator, ",", "\"", 4, el, sizeof(el)) < 0) {
			SYSLOG(LOG_ERROR, "failed to get act3gpp(4) from COPS command - %s", operator);
			continue;
		}
		act3gpp = atoi(el);
		SYSLOG(LOG_DEBUG, "operator#%d - act=%d", i, act3gpp);

		// get operator name
		if(__strtok(operator, ",", "\"", 1, el, sizeof(el)) < 0) {
			SYSLOG(LOG_ERROR, "failed to get operator name(1) from COPS command - %s", operator);
			continue;
		}
		__strncpy(operator_name, el, sizeof(operator_name));
		SYSLOG(LOG_DEBUG, "operator#%d - operator_name=%s", i, operator_name);

		// get mccmnc
		if(__strtok(operator, ",", "\"", 3, el, sizeof(el)) < 0) {
			SYSLOG(LOG_ERROR, "failed to get mccmnc(3) from COPS command - %s", operator);
			continue;
		}
		__strncpy(mccmnc, el, sizeof(mccmnc));
		SYSLOG(LOG_DEBUG, "operator#%d - mccmnc=%s", i, mccmnc);

		// check mccmnc length
		mccmnc_len = strlen(mccmnc);
		if((mccmnc_len != 6) && (mccmnc_len != 5)) {
			SYSLOG(LOG_ERROR, "incorrect mcc mnc - mccmnc=%s,len=%d", mccmnc, mccmnc_len);
			continue;
		}

		// get mcc and mnc
		__strncpy(mcc, mccmnc, 3 + 1);
		__strncpy(mnc, mccmnc + 3, 3 + 1);
		SYSLOG(LOG_DEBUG, "operator#%d - mcc=%s,mnc=%s", i, mcc, mnc);

		/*
				3GPP - <state>
				0 unknown
				1 available
				2 current
				3 forbidden

				CnS - <stat>
				Bit 0—PLMN is registered
				Bit 1—PLMN is forbidden
				Bit 2—PLMN is the home PLMN
				Bit 3—PLMN is weak
				Bit 4—PLMN supports GPRS bit
				Bits 5–7—Reserve
		*/

		// get stat
		switch(stat3gpp) {
		case 1: // available
			stat = 1;
			break;

		case 2: // current
			stat = 4;
			break;

		case 3: // forbidden
			stat = 2;
			break;

		default:
			stat = 0;
			break;
		}

		/*
				3GPP - <AcT> access technology selected
				0 GSM
				1 GSM Compact
				2 UTRAN
				3 GSM w/EGPRS
				4 UTRAN w/HSDPA
				5 UTRAN w/HSUPA
				6 UTRAN w/HSDPA and HSUPA
				7 E-UTRAN

				CnS - <AcT>
				0x00=Unknown network
				0x01=GSM network
				0x02=DCS network
				0x03=GSM DCS network
				0x04=PCS network
				0x05=GSM PCS network
				0x06=ICO network
				0x07=UMTS network
				0x08–0xFF=Reserved

				output: Telstra Mobile,505,1,<stat>,<AcT>
				<state> - this is CnS compatible state (not same as 3GPP)
				<AcT> - this is CnS compatible AcT (not same as 3GPP)
		*/
		switch(act3gpp) {
		case 0: // GSM
		case 1: // GSM Compact
		case 3: // GSM w/EGPRS
			act = 0x01;
			break;

		case 2: // UTRAN
		case 4: // UTRAN w/HSDPA
		case 5: // UTRAN w/HSUPA
		case 6: // UTRAN w/HSDPA and HSUPA
		case 7: // E-UTRAN
			act = 0x07;
			break;

		default:
			act = 0x00;
			break;
		}

		// get output line
		if(plmn_list_idx)
			snprintf(buf, sizeof(buf), "&%s,%d,%d,%d,%d", operator_name, atoi(mcc), atoi(mnc), stat, act);
		else
			snprintf(buf, sizeof(buf), "%s,%d,%d,%d,%d", operator_name, atoi(mcc), atoi(mnc), stat, act);
		buf_len = strlen(buf);

		// check length
		if(!(buf_len + 1 < sizeof(plmn_list) - plmn_list_idx)) {
			SYSLOG(LOG_ERROR, "insufficient plmn list buffer (len=%d,name_len=%d)", sizeof(plmn_list), buf_len + 1);
			goto err;
		}


		// concat the string to result
		__strncpy(plmn_list + plmn_list_idx, buf, buf_len + 1);
		plmn_list_idx += buf_len;
	}

	return plmn_list;

err:
	return NULL;
}

#if 0
// disabled to hide the compiler warning
// this function has been replaced with _at_network_scan() but we may need to use in the futunre for unknown modules
const char* _qmi_network_scan()
{
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	unsigned short qmi_result;
	unsigned short qmi_error;
	const struct qmitlv_t* tlv;
	unsigned short tran_id;

	int i;

	struct qmi_nas_perform_network_scan_resp* resp;
	struct qmi_nas_perform_network_scan_resp2* resp2;

	struct qmi_nas_perform_network_scan_resp_act* resp_act;

	static char plmn_list[QMIMGR_MAX_DB_BIGVALUE_LENGTH];

	char buf[QMIMGR_MAX_DB_VALUE_LENGTH];
	int buf_len;

	int plmn_list_idx;
	int read;
	int read_upbounce;
	int l;

	char network[QMIMGR_MAX_DB_VALUE_LENGTH];
	int stat;
	int act;
	int mcc;
	int mnc;

	unsigned char current_rat;
	unsigned char nw_rat;

	plmn_list[0] = 0;

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to create a msg");
		goto err;
	}

	// request qmi - QMI_NAS_GET_SIGNAL_STRENGTH
	if(_request_qmi(msg, QMINAS, QMI_NAS_GET_SIGNAL_STRENGTH, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed - QMI_NAS_GET_SIGNAL_STRENGTH");
		goto err;
	};

	current_rat = 0;

	// wait for response
	rmsg = _wait_qmi_response(QMINAS, QMI_NAS_GET_SIGNAL_STRENGTH, tran_id, &qmi_result, &qmi_error, NULL);
	if(rmsg) {
		struct qmi_nas_get_signal_strength_resp_sig_strength* resp_sig;

		SYSLOG(LOG_OPERATION, "got QMI_NAS_GET_SIGNAL_STRENGTH response");

		// get QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE
		tlv = _get_tlv(rmsg, QMI_NAS_GET_SIGNAL_STRENGTH_RESP_TYPE_SIG_STRENGTH, sizeof(*resp_sig));
		if(tlv) {
			SYSLOG(LOG_OPERATION, "got tlv - QMI_NAS_GET_SIGNAL_STRENGTH_RESP_TYPE_SIG_STRENGTH");

			resp_sig = (struct qmi_nas_get_signal_strength_resp_sig_strength*)tlv->v;
			current_rat = resp_sig->radio_if;
		}
	}

	SYSLOG(LOG_OPERATION, "current RAT = %d", current_rat);

	// request qmi
	SYSLOG(LOG_DEBUG, "sending QMI_NAS_PERFORM_NETWORK_SCAN");
	if(_request_qmi(msg, QMINAS, QMI_NAS_PERFORM_NETWORK_SCAN, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "qmi request failed");
		goto err;
	}


	// wait for response
	SYSLOG(LOG_DEBUG, "waiting for QMIMGR_LONG_RESP_TIMEOUT");
	rmsg = _wait_qmi_response(QMINAS, QMIMGR_LONG_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, NULL);
	if(!rmsg || qmi_result) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
		goto err;
	}

	// get QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE
	SYSLOG(LOG_DEBUG, "searching for QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE");
	tlv = _get_tlv(rmsg, QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE, sizeof(*resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR, "mandatory resp type not found - QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE");
		goto err;
	}
	// extract information
	resp = (struct qmi_nas_perform_network_scan_resp*)tlv->v;
	l = tlv->l;

	// get QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_ACT
	SYSLOG(LOG_DEBUG, "searching for QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_ACT");
	tlv = _get_tlv(rmsg, QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_ACT, sizeof(*resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR, "mandatory resp type not found - QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_ACT");
		goto err;
	}
	// extract information
	resp_act = (struct qmi_nas_perform_network_scan_resp_act*)tlv->v;

	// check validation
	if(resp->num_network_info_instances != resp_act->num_network_info_instances) {
		SYSLOG(LOG_ERROR, "resp count(%d) not matching to resp_act count(%d)", resp->num_network_info_instances, resp_act->num_network_info_instances);
		goto err;
	}


	SYSLOG(LOG_OPERATION, "num_network_info_instances=%d", resp->num_network_info_instances);

	read = sizeof(*resp);
	plmn_list_idx = 0;

	// convert to 3gpp standard from qmi format
	for(i = 0; i < resp->num_network_info_instances; i++) {

		// check length before getting the pointer
		if(read + sizeof(*resp) > l) {
			SYSLOG(LOG_ERROR, "broken packet - insufficent for header");
			goto err;
		}

		// get resp2 pointer
		resp2 = (struct qmi_nas_perform_network_scan_resp2*)((char*)resp + read);

		// check structure boundary
		read_upbounce = read + sizeof(*resp2) + resp2->network_description_length;
		if(read_upbounce > l) {
			SYSLOG(LOG_ERROR, "broken packet - out of bound in network information (i=%d,desc_len=%d,cur=%d,len=%d)", i, resp2->network_description_length, read_upbounce, l);
			goto err;
		}

		// increase read point
		read = read_upbounce;

		/*
				ex) Telstra Mobile,505,1,4,7&Telstra Mobile,505,1,1,1&3Telstra,505,6,1,7&vodafone AU,505,3,2,7&YES OPTUS,505,2,2,1&vodafone AU,505,3,2,1&YES OPTUS,505,2,2,7

				Telstra Mobile,505,1,<stat>,<AcT>

				<state>
				0 unknown
				1 available
				2 current
				3 forbidden

		*/

		SYSLOG(LOG_OPERATION, "network_status=0x%02x", resp2->network_status);

		// get <state> - this is CnS compatible state (not same as 3GPP)
		if(((resp2->network_status >> 4) & 0x03) == 1) {
			stat = 2; // forbidden
		} else if(((resp2->network_status >> 0) & 0x03) == 2) {
			stat = 1; // available
		} else if(((resp2->network_status >> 0) & 0x03) == 1) {
			stat = 4; // current
		} else {
			stat = 0;
			SYSLOG(LOG_ERROR, "unknown network stat found - 0x%02x", resp2->network_status);
		}

		// get host resp_act

		/*
				RAT

				0x00 – None (no service)
				0x01 – cdma2000® 1X
				0x02 – cdma2000 HRPD (1xEV-DO)
				0x03 – AMPS
				0x04 – GSM
				0x05 – UMTS
				0x08 – LTE

				3GPP - <AcT> access technology selected
				0 GSM
				1 GSM Compact
				2 UTRAN
				3 GSM w/EGPRS
				4 UTRAN w/HSDPA
				5 UTRAN w/HSUPA
				6 UTRAN w/HSDPA and HSUPA
				7 E-UTRAN
		*/

		SYSLOG(LOG_OPERATION, "radio_access_technology=%d", resp_act->networks[i].radio_access_technology);
		// get <AcT> - this is CnS compatible AcT (not same as 3GPP)
		nw_rat = resp_act->networks[i].radio_access_technology;
		switch(nw_rat) {
			// cdma technologies - assume these are 2G
		case 1:
		case 2:
		case 3:
			act = 0; // unknown
			break;

			// GSM
		case 4:
			act = 1; // GSM (2G)
			break;

			// UMTS - assume this is normal UTRAN
		case 5:
			act = 7; // UMTS
			break;

			// LTE
		case 8:
			act = 8; // LTE
			break;

			// take all kind of CDMA technologies as GSM 2G - getting wrong better than not working at all?
		default:
			act = 0;
			SYSLOG(LOG_ERROR, "unknown RAT(radio access technology) found - 0x%02x", nw_rat);
			break;
		}

		// fix the Sierra well-known incorrect home network problem!!
		if((stat == 4) && (current_rat != nw_rat)) {
			SYSLOG(LOG_OPERATION, "Sierra home network bug fixed - stat=%d,current_rat=%d,network rat=%d", stat, current_rat, resp_act->networks[i].radio_access_technology);
			stat = 1;
		}

		// get network name
		__strncpy(network, (char*)resp2->network_description, resp2->network_description_length + 1);

		// get mcc and mnc
		mcc = read16_from_little_endian(resp2->mobile_country_code);
		mnc = read16_from_little_endian(resp2->mobile_network_code);

		SYSLOG(LOG_OPERATION, "idx=%d, name=%s, mcc=%d,mnc=%d,stat=%d,AcT=%d", i, network, mcc, mnc, stat, act);

		// Telstra Mobile,505,1,<stat>,<AcT>
		if(i)
			snprintf(buf, sizeof(buf), "&%s,%d,%d,%d,%d", network, mcc, mnc, stat, act);
		else
			snprintf(buf, sizeof(buf), "%s,%d,%d,%d,%d", network, mcc, mnc, stat, act);

		buf_len = strlen(buf);

		// check length
		if(!(buf_len + 1 < sizeof(plmn_list) - plmn_list_idx)) {
			SYSLOG(LOG_ERROR, "insufficient plmn list buffer (len=%d,name_len=%d)", sizeof(plmn_list), buf_len + 1);
			goto err;
		}

		// concat the string to result
		__strncpy(plmn_list + plmn_list_idx, buf, buf_len + 1);
		plmn_list_idx += buf_len;
	}

	SYSLOG(LOG_OPERATION, "plmn_list=%s", plmn_list);


	SYSLOG(LOG_OPERATION, "get QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE");

	qmimsg_destroy(msg);
	return plmn_list;

err:
	qmimsg_destroy(msg);
	return NULL;
}
#endif

/* The definition of struct band_info_t has been moved to bandsel.h */

struct band_info_t band_info_huawei_em820u[] = {
#define HUAWEI_EM820U_GSM_850		0x00080000
#define HUAWEI_EM820U_GSM_900		(0x00000100|0x00000200|0x00100000)
#define HUAWEI_EM820U_GSM_1800		0x00000080
#define HUAWEI_EM820U_GSM_1900		0x00200000
#define HUAWEI_EM820U_GSM_ALL		(HUAWEI_EM820U_GSM_850|HUAWEI_EM820U_GSM_900|HUAWEI_EM820U_GSM_1800|HUAWEI_EM820U_GSM_1900)
#define HUAWEI_EM820U_WCDMA_850		0x04000000
#define HUAWEI_EM820U_WCDMA_900		(0x0002000000000000LL)
#define HUAWEI_EM820U_WCDMA_1900	0x00800000
#define HUAWEI_EM820U_WCDMA_2100	(0x00400000)
#define HUAWEI_EM820U_WCDMA_ALL		(HUAWEI_EM820U_WCDMA_2100|HUAWEI_EM820U_WCDMA_1900|HUAWEI_EM820U_WCDMA_900|HUAWEI_EM820U_WCDMA_850)
#define HUAWEI_EM820U_GSM_WCDMA_ALL 	0x3FFFFFFF
#define HUAWEI_EM820U_AWS		0x02000000
	{0x00, "Autoband", HUAWEI_EM820U_GSM_WCDMA_ALL}, // All bands
	{0x01, "GSM 900/1800", HUAWEI_EM820U_GSM_900 | HUAWEI_EM820U_GSM_1800},
	{0x03, "GSM 850/1900", HUAWEI_EM820U_GSM_850 | HUAWEI_EM820U_GSM_1900},
	{0x04, "GSM All", HUAWEI_EM820U_GSM_ALL}, // GSM ALL
	{0x05, "WCDMA All", HUAWEI_EM820U_WCDMA_ALL},
	{0x07, "WCDMA 850 GSM All", HUAWEI_EM820U_WCDMA_850 | HUAWEI_EM820U_GSM_ALL},
	{0x06, "WCDMA 850/1900 GSM 850/1900", HUAWEI_EM820U_WCDMA_850 | HUAWEI_EM820U_WCDMA_1900 | HUAWEI_EM820U_GSM_850 | HUAWEI_EM820U_GSM_1900},
	{0x08, "WCDMA 850/1900", HUAWEI_EM820U_WCDMA_850 | HUAWEI_EM820U_WCDMA_1900},
	{0x0a, "WCDMA 850/2100 GSM 900/1800", HUAWEI_EM820U_WCDMA_850 | HUAWEI_EM820U_WCDMA_2100 | HUAWEI_EM820U_GSM_900 | HUAWEI_EM820U_GSM_1800},
	{0x09, "WCDMA 850/2100", HUAWEI_EM820U_WCDMA_850 | HUAWEI_EM820U_WCDMA_2100},
	{0x0b, "WCDMA 850 GSM 900/1800", HUAWEI_EM820U_WCDMA_850 | HUAWEI_EM820U_GSM_900 | HUAWEI_EM820U_GSM_1800},
	{0x0c, "WCDMA 850", HUAWEI_EM820U_WCDMA_850}, // WCDMA 850
	{0x0d, "WCDMA 900", HUAWEI_EM820U_WCDMA_900},
	{0x0e, "WCDMA 900/2100 GSM 900/1800", HUAWEI_EM820U_WCDMA_900 | HUAWEI_EM820U_WCDMA_2100 | HUAWEI_EM820U_GSM_900 | HUAWEI_EM820U_GSM_1800}, //
	{0x0f, "WCDMA 900/2100", HUAWEI_EM820U_WCDMA_900 | HUAWEI_EM820U_WCDMA_2100},
	{0x11, "WCDMA 1700(AWS) GSM All", HUAWEI_EM820U_AWS | HUAWEI_EM820U_GSM_ALL},
	{0x10, "WCDMA 1700(AWS)", HUAWEI_EM820U_AWS},
	{0x12, "WCDMA 1900", HUAWEI_EM820U_WCDMA_1900},
	{0x13, "WCDMA 2100", HUAWEI_EM820U_WCDMA_2100},
	{0x14, "WCDMA 2100 GSM 900/1800", HUAWEI_EM820U_WCDMA_2100 | HUAWEI_EM820U_GSM_900 | HUAWEI_EM820U_GSM_1800},
};

/*
 * Changed return type from const char * to int so it has signature identical
 * to _qmi_band_get_current().
 */
int _at_band_get_current()
{
	char band[QMIMGR_MAX_DB_VALUE_LENGTH];
	unsigned short tran_id;
	const char* result;

	unsigned long long band_no;
	const char* band_name;


	struct strqueue_t* q;

	// request
	if(_request_at("AT^SYSCFG?", QMIMGR_AT_RESP_TIMEOUT, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue cmd - AT^SYSCFG?");
		goto err;
	}

	/*
		AT^SYSCFG?
		^SYSCFG:2,0,3FFFFFFF,1,2

		OK
	*/

	// get result
	q = _wait_at_response(tran_id, QMIMGR_AT_SELECT_TIMEOUT);

	// get result
	result = _get_at_result(q, 4, 1, "^SYSCFG:");
	if(!result) {
		SYSLOG(LOG_ERROR, "incorrect result - cmd=AT^SYSCFG?");
		goto err;
	}

	// get band
	if(__strtok(result, ",", NULL, 2, band, sizeof(band)) < 0) {
		SYSLOG(LOG_ERROR, "result format incorrect - %s", result);
		goto err;
	}

	SYSLOG(LOG_OPERATION, "extracted band from module - band=%s", band);

	// get band number
	if(sscanf(band, "%llx", &band_no) != 1) {
		SYSLOG(LOG_ERROR, "failed to convert band(%s)to number", band);
		goto err;
	}

	SYSLOG(LOG_OPERATION, "current selected band code from module - no=0x%llx,str=%s", band_no, band);

	band_name = NULL;

	// search band code
	int i;
	for(i = 0; i < __countof(band_info_huawei_em820u); i++) {
		if(band_info_huawei_em820u[i].code == band_no) {
			band_name = band_info_huawei_em820u[i].name;
			break;
		}
	}

	if(band_name) { // set the rdb here rather than return band_name to caller
		_set_str_db("currentband.current_selband", band_name, -1);
		return 0;
	}

err:
	return -1;
}

/* get current selected band using QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE. */
static int _qmi_band_get_current()
{
    struct qmimsg_t * msg;
    unsigned short tran_id;
    msg = qmimsg_create();
    if(!msg) {
        SYSLOG(LOG_ERROR, "failed to allocate msg for "
               "QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE");
        goto err;
    }
    SYSLOG(LOG_OPERATION, "get current band using qmi");
    _request_qmi(msg, QMINAS, QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE,
                 &tran_id);
    if(_wait_qmi_until(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
        SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result) - "
               "QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE");
        goto err;
    }
    SYSLOG(LOG_OPERATION, "qmi get current band sucessful");
    qmimsg_destroy(msg);
    return 0;
err:
    qmimsg_destroy(msg);
    return -1;
}

int _at_band_update_supports()
{
	static char lists[QMIMGR_MAX_DB_BIGVALUE_LENGTH];
	char band[QMIMGR_MAX_DB_BIGVALUE_LENGTH];
	int i;
	int total;

	// get total length of buffer
	total = 0;
	for(i = 0; i < __countof(band_info_huawei_em820u); i++) {
		if(i)
			total += 1;
		total += strlen("00,") + strlen(band_info_huawei_em820u[i].name);
	}
	total += 1;

	// check length
	if(total > sizeof(lists)) {
		SYSLOG(LOG_ERROR, "insufficient database buffer length - total=%d", total);
		goto err;
	}

	// build the list - ATMGR
	lists[0] = 0;
	for(i = 0; i < __countof(band_info_huawei_em820u); i++) {
		if(i)
			strcat(lists, "&");

		snprintf(band, sizeof(band), "%02x,%s", band_info_huawei_em820u[i].hex, band_info_huawei_em820u[i].name);
		strcat(lists, band);
	}

	_set_str_db("module_band_list", lists, -1);


	// build the list - CNSMGR
	lists[0] = 0;
	for(i = 0; i < __countof(band_info_huawei_em820u); i++) {
		if(i)
			strcat(lists, ";");
		strcat(lists, band_info_huawei_em820u[i].name);
	}

	_set_str_db("currentband.current_band", lists, -1);

	return 0;

err:
	_set_reset_db("module_band_list");
	_set_reset_db("currentband.current_band");

	return -1;
}

#if 0
/* Use QMI_DMS_GET_BAND_CAPABILITY to get supported bands */
static int _qmi_band_update_supports()
{
	struct qmimsg_t* msg;
	unsigned short tran_id;

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR,
		       "failed to allocate msg for QMI_DMS_GET_BAND_CAPABILITY");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "extract module supported bands");

	// QMI_DMS_GET_BAND_CAPABILITY
	_request_qmi(msg, QMIDMS, QMI_DMS_GET_BAND_CAPABILITY, &tran_id);
	// wait for response
	if(_wait_qmi_until(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR,
		       "qmi response timeout (or qmi failure result) - "
		       "QMIMGR_GENERIC_RESP_TIMEOUT");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "get module band capability done");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}
#endif

int _at_band_set(const char* band)
{
	int i;
	int band_idx;
	char* result;

	unsigned short tran_id;
	struct strqueue_t* q;
	int band_code;


	SYSLOG(LOG_ERROR, "band = %s", band);

	// get band code
	if(sscanf(band, "%x", &band_code) != 1)
		band_code = -1;

	// get total length of buffer
	band_idx = -1;
	for(i = 0; i < __countof(band_info_huawei_em820u); i++) {
		if(!strcasecmp(band_info_huawei_em820u[i].name, band) || (band_info_huawei_em820u[i].hex == band_code))
			band_idx = i;
	}

	// check return
	if(band_idx < 0) {
		SYSLOG(LOG_ERROR, "band not found - band=%s", band);
		goto err;
	}

	char cmd[QMIMGR_AT_COMMAND_BUF];

	/*
		AT^SYSCFG=2,0,4000000,1,2
		OK
	*/


	// request
	snprintf(cmd, sizeof(cmd), "AT^SYSCFG=2,0,%llx,1,2", band_info_huawei_em820u[band_idx].code);
	if(_request_at(cmd, QMIMGR_AT_RESP_TIMEOUT, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - cmd=%s", cmd);
		goto err;
	}

	// get result
	q = _wait_at_response(tran_id, QMIMGR_AT_SELECT_TIMEOUT);

	// get result
	result = _get_at_result(q, 1, 1, "OK");
	if(!result) {
		SYSLOG(LOG_ERROR, "incorrect result - cmd=%s", cmd);
		goto err;
	}

	return 0;
err:
	return -1;
}

/*
 * set band using QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE.
 * band is a semicolon separated list of either band name or hex,
 * each from band_info_qmi[], lte_band_info_qmi[] or band_group_info_qmi[].
 */
static int _qmi_band_set(const char* band)
{
    /* relevant tlv: mode_pref, band_pref, lte_band_pref */
    unsigned short mode_pref;
    struct qmi_band_bit_mask band_pref, lte_band_pref;
    struct qmimsg_t *msg = NULL, *rmsg;
    unsigned short tran_id;
    unsigned short qmi_result, qmi_error;

    SYSLOG(LOG_DEBUG, "qmi_band_set: band = %s", band);

    if(_qmi_build_masks_from_band(band, &mode_pref, &band_pref,
                                  &lte_band_pref)) {
        goto err;
    }

    /*
     * We should never set beyond band capability of the module.
     * They should have been updated when qmimgr starts up using
     * QMI_DMS_GET_BAND_CAPABILITY.
     */
    if(_g_band_cap.band) {
        band_pref.band &= _g_band_cap.band;
    }
    if(_g_lte_band_cap.band) {
        lte_band_pref.band &= _g_lte_band_cap.band;
    }

    SYSLOG(LOG_DEBUG, "mode_pref=%04x, band_pref=%llx, lte_band_pref=%llx",
           mode_pref, band_pref.band, lte_band_pref.band);

    msg = qmimsg_create();
    if(!msg) {
        SYSLOG(LOG_ERROR, "failed to create a msg");
        goto err;
    }

    qmimsg_set_msg_id(msg, QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE);
    qmimsg_clear_tlv(msg);
    if(qmimsg_add_tlv(msg,
                  QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_MODE_PREF,
                  sizeof(mode_pref), &mode_pref) < 0) {
        SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv() - "
               "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_MODE_PREF");
        goto err;
    }
    if(qmimsg_add_tlv(msg,
                  QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_BAND_PREF,
                  sizeof(band_pref), &band_pref) < 0) {
        SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv() - "
               "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_BAND_PREF");
    }
    if(qmimsg_add_tlv(msg,
              QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_LTE_BAND_PREF,
              sizeof(lte_band_pref), &lte_band_pref) < 0) {
        SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv() - "
               "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_LTE_BAND_PREF");
    }

    SYSLOG(LOG_DEBUG, "writing msg request - "
           "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE");
    if(qmiuniclient_write(uni, QMINAS, msg, &tran_id) < 0) {
        SYSLOG(LOG_ERROR, "failed to queue - msg=0x%04x, serv_id=%d",
               msg->msg_id, QMINAS);
        goto err;
    }
    rmsg = _wait_qmi_response(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id,
                              &qmi_result, &qmi_error, NULL);
    if(!rmsg) {
        SYSLOG(LOG_ERROR, "qmi response timeout");
        goto err;
    }
    if(qmi_result) {
        SYSLOG(LOG_ERROR, "qmi failure: result=%02x, error=%02x",
               qmi_result, qmi_error);
        goto err;
    }

    qmimsg_destroy(msg);
    return 0;
err:
    qmimsg_destroy(msg);
    return -1;
}

void _qmi_lp_set_default_wwan_profile(int link_profile_idx)
{
	char profile_name[EZQMI_DB_VAL_MAX_LEN];
	snprintf(profile_name, sizeof(profile_name), "Profile%d", link_profile_idx);

	_set_fmt_dbvar_ex(0, "link.profile.%d.enable", "0", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.apn", "", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.user", "", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.pass", "", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.auth_type", "", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.module_profile_idx", "", link_profile_idx);

	_set_fmt_dbvar_ex(0, "link.profile.%d.defaultroute", "0", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.dev", "wwan.0", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.dialstr", "atd*99#", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.name", profile_name, link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.snat", "1", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.userpeerdns", "1", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.verbose_logging", "1", link_profile_idx);

	_set_fmt_dbvar_ex(0, "link.profile.%d.autoapn", "1", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.readonly", "", link_profile_idx);
}

void _qmi_lp_del_wwan_profiles()
{
#define LINK_PROFILE_MAX	6

	const char* dev;

	int i;
	int link_profile_idx;

	for(i = 0; i < LINK_PROFILE_MAX; i++) {
		link_profile_idx = i + 1;

		dev = _get_fmt_dbvar_ex(0, "link.profile.%d.dev", "", link_profile_idx);

		/* bypass if the profile is not wwan */
		if(strcmp(dev, "wwan.0"))
			continue;

		/* set wwan link default */
		_qmi_lp_set_default_wwan_profile(link_profile_idx);
	}
}

struct walk_thru_ref_t {
	int profile;
	int def_profile;
};

int _qmi_walk_thru_profiles_find_netcomm(int profile_idx, const char* profile, const struct db_struct_profile* db_profile, void* ref)
{
	int* netcomm_detect;

	netcomm_detect = (int*)ref;

	*netcomm_detect = !strncmp(profile, QMIMGR_NETCOMM_PROFILE, strlen(QMIMGR_NETCOMM_PROFILE));

	return !netcomm_detect ? 0 : -1;
}

int _qmi_walk_thru_profiles_cb(int profile_idx, const char* profile, const struct db_struct_profile* db_profile, void* ref)
{
	int link_profile_idx;
	int en;

	char profile_name[EZQMI_DB_VAL_MAX_LEN];
	char module_profile_idx[EZQMI_DB_VAL_MAX_LEN];
	struct walk_thru_ref_t* walk_thru_ref = (struct walk_thru_ref_t*)ref;

	/* correct the base - link.profile is 1-based and qmi profile is 0-based */
	link_profile_idx = walk_thru_ref->profile++;
	snprintf(profile_name, sizeof(profile_name), "Profile%d", link_profile_idx);
	snprintf(module_profile_idx, sizeof(module_profile_idx), "%d", profile_idx);

	/* log */
	SYSLOG(LOG_INFO, "[oma-dm] write module profile to link.profile.%d] rdb", link_profile_idx);
	SYSLOG(LOG_INFO, "[oma-dm] link.profile.%d.apn = '%s'", link_profile_idx, _dbstruct_get_str_member(db_profile, apn_name));
	SYSLOG(LOG_INFO, "[oma-dm] link.profile.%d.user = '%s'", link_profile_idx, _dbstruct_get_str_member(db_profile, user));
	SYSLOG(LOG_INFO, "[oma-dm] link.profile.%d.pass = '%s'", link_profile_idx, _dbstruct_get_str_member(db_profile, pass));
	SYSLOG(LOG_INFO, "[oma-dm] link.profile.%d.auth_type = '%s'", link_profile_idx, _dbstruct_get_str_member(db_profile, auth_preference));
	SYSLOG(LOG_INFO, "[oma-dm] link.profile.%d.pdp_type = '%s'", link_profile_idx, _dbstruct_get_str_member(db_profile, pdp_type));
	SYSLOG(LOG_INFO, "[oma-dm] link.profile.%d.module_profile_idx = '%s'", link_profile_idx, module_profile_idx);

	/* skip any higher WWAN profile - we are talking only 6 WWAN profiles */
	if(link_profile_idx > 6) {
		SYSLOG(LOG_ERR, "[oma-dm] skip profile #%d, do not write profile to link.profile.%d] rdb", link_profile_idx, link_profile_idx);
		return 0;
	}

	SYSLOG(LOG_ERR, "[oma-dm] write module profile#%d to link.profile#%d (pdp_type=%s,apn=%s)", profile_idx, link_profile_idx, _dbstruct_get_str_member(db_profile, pdp_type), _dbstruct_get_str_member(db_profile, apn_name));

	/* set profile default settings */
	_qmi_lp_set_default_wwan_profile(link_profile_idx);

	/* set wwan profile */
	_set_fmt_dbvar_ex(0, "link.profile.%d.autoapn", "0", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.apn", _dbstruct_get_str_member(db_profile, apn_name), link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.user", _dbstruct_get_str_member(db_profile, user), link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.pass", _dbstruct_get_str_member(db_profile, pass), link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.auth_type", _dbstruct_get_str_member(db_profile, auth_preference), link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.pdp_type", _dbstruct_get_str_member(db_profile, pdp_type), link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.module_profile_idx", module_profile_idx, link_profile_idx);

	/* enable prfile */
	en = walk_thru_ref->def_profile == profile_idx;
	_set_fmt_dbvar_ex(0, "link.profile.%d.defaultroute", en ? "1" : "0", link_profile_idx);
	_set_fmt_dbvar_ex(0, "link.profile.%d.enable", en ? "1" : "0", link_profile_idx);


	return 0;
}

void qmimgr_on_db_command_profile(const char* db_var, int db_var_idx, const char* db_cmd, int db_cmd_idx)
{
	int id;
	char profile[QMIMGR_MAX_DB_VALUE_LENGTH];
	const char* result_rdb;

	int profile_index;
	char rdb[QMIMGR_MAX_DB_VARIABLE_LENGTH];
#if defined (MODULE_cinterion) || defined (SUB_NETIF)
	/* nothing to do */
#else
	int id_existing = 1;
#endif
	int profile_type = 0;

	// maximum 16 profile are allowed
	// each profile can have upto 2 connections, eg ipv4 and ipv6
#define MAX_PROFILE_COUNT 16
	static const unsigned char iptypes[] = { IPV4, IPV6 };
	static int pkt_data_handles[MAX_PROFILE_COUNT + 1][__countof(iptypes)] = {{0}};

	// get netcomm profile
	id = _get_int_db("profile.cmd.param.profile_id", -1);
	if(id < 1) {
#if defined (MODULE_cinterion) || defined (SUB_NETIF)
		/* nothing to do */
#else
		id_existing = 0;
#endif
		id = 0;
	}

	snprintf(profile, sizeof(profile), "%s%d", QMIMGR_NETCOMM_PROFILE, id);

	// maintain session status up to max profile count
	int pkt_data_handle = 0;
	if((0 <= id) && (id <= MAX_PROFILE_COUNT))
		pkt_data_handle = id;


	switch(db_cmd_idx) {
	case DB_COMMAND_VAL_DELETE: {

		if(_qmi_delete_profile(id) < 0) {
			SYSLOG(LOG_ERROR, "failed to delete profile %d", id);
			goto err_db_command_val_delete;
		}

		_set_str_db("profile.cmd.status", "[done]", -1);
		break;

err_db_command_val_delete:
		_set_str_db("profile.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_CLEAN: {
		int i;
		/*
					char empty[QMIMGR_MAX_DB_VALUE_LENGTH];
					unsigned short qmi_error;
		*/

		for(i = 0; i <= 20; i++) {
			_qmi_delete_profile(i);
		}

		/*
					for(i=0;i<=20;i++) {
						sprintf(empty,"empty%d",i);
						SYSLOG(LOG_DEBUG,"cleaning %d - %s",i,empty);
						_qmi_create_profile(i,empty,empty,"","",1,&qmi_error,&qmi_ext_error);
					}
		*/
		break;
	}

	case DB_COMMAND_VAL_GETDEF: {
		int def_profile_idx;

		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_GETDEF");

		def_profile_idx = _qmi_getdef(0, 1);
		if(def_profile_idx < 0) {
			SYSLOG(LOG_ERROR, "failed to get the default profile");
			goto err_db_command_val_getdef;
		}

		_set_int_db("profile.cmd.param.profile_id", 0, NULL);

		_set_str_db("profile.cmd.status", "[done]", -1);
		break;

err_db_command_val_getdef:
		_set_str_db("profile.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_SETDEF: {
		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_SETDEF");

		if(_qmi_setdef(1, 1, id) < 0) {
			SYSLOG(LOG_ERROR, "failed to set the default profile");
			goto err_db_command_val_setdef;
		}

		_set_str_db("profile.cmd.status", "[done]", -1);
		break;

err_db_command_val_setdef:
		_set_str_db("profile.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_READ: {
		struct db_struct_profile db_profile;
		int i;

		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_READ");

		SYSLOG(LOG_OPERATION, "profile_id=%d", id);

#if defined (MODULE_cinterion) || defined (SUB_NETIF)
		// This is only for display profile name list on syslog.
		// With "rdb_set wwan.0.profile.cmd.command read" will display the list on the syslog.
		// Debuging level on "wwan.0.debug" should be lower than 5.
		_qmi_find_profile(profile, 0, 0);

		profile_index = id + 1;
#else
		if(id_existing) {
			profile_index = id;
		} else {
			profile_index = _qmi_getdef(0, 1);
			if(profile_index < 0) {
				profile_index = _qmi_find_profile(profile, 0, 0);
			}
		}

#endif
		if(profile_index < 0) {
			_set_str_db("profile.cmd.status", "[error] profile not found", -1);
			goto err_db_command_val_read;
		}

		memset(&db_profile, 0, sizeof(db_profile));

		if(_qmi_get_profile(profile_index, profile_type, profile, &db_profile) < 0) {
			_set_str_db("profile.cmd.status", "[error] failed to read profile", -1);
			goto err_db_command_val_read;
		}

		_dbstruct_write_str_member_to_db(&db_profile, apn_name, "profile.cmd.param.apn");
		_dbstruct_write_str_member_to_db(&db_profile, user, "profile.cmd.param.user");
		_dbstruct_write_str_member_to_db(&db_profile, pass, "profile.cmd.param.password");
		_dbstruct_write_str_member_to_db(&db_profile, auth_preference, "profile.cmd.param.auth_type");
		_dbstruct_write_str_member_to_db(&db_profile, pdp_type, "profile.cmd.param.pdp_type");
		_dbstruct_write_str_member_to_db(&db_profile, ipv4_addr_pref, "profile.cmd.param.ipv4_addr_pref");

		// read tft params
		for(i = 0; i < __countof(&db_profile.tft_id); i++) {
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], filter_id, _get_indexed_str("profile.cmd.param.tft.filter_id", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], eval_id, _get_indexed_str("profile.cmd.param.tft.eval_id", i, NULL));
			_dbstruct_write_str_member_to_db(&db_profile.tft_id[i], ip_version, _get_indexed_str("profile.cmd.param.tft.ip_version", i, NULL));
			_dbstruct_write_str_member_to_db(&db_profile.tft_id[i], source_ip, _get_indexed_str("profile.cmd.param.tft.source_ip", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], source_ip_mask, _get_indexed_str("profile.cmd.param.tft.source_ip_mask", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], next_header, _get_indexed_str("profile.cmd.param.tft.next_header", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], dest_port_range_start, _get_indexed_str("profile.cmd.param.tft.dest_port_range_start", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], dest_port_range_end, _get_indexed_str("profile.cmd.param.tft.dest_port_range_end", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], src_port_range_start, _get_indexed_str("profile.cmd.param.tft.src_port_range_start", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], src_port_range_end, _get_indexed_str("profile.cmd.param.tft.src_port_range_end", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], ipsec_spi, _get_indexed_str("profile.cmd.param.tft.ipsec_spi", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], tos_mask, _get_indexed_str("profile.cmd.param.tft.tos_mask", i, NULL));
			_dbstruct_write_int_member_to_db(&db_profile.tft_id[i], flow_label, _get_indexed_str("profile.cmd.param.tft.flow_label", i, NULL));
		}

		_dbstruct_write_int_member_to_db(&db_profile, pdp_context_number, "profile.cmd.param.pdp_context_number");
		_dbstruct_write_int_member_to_db(&db_profile, pdp_context_sec_flag, "profile.cmd.param.pdp_context_sec_flag");
		_dbstruct_write_int_member_to_db(&db_profile, pdp_context_primary_id, "profile.cmd.param.pdp_context_primary_id");
		_dbstruct_write_str_member_to_db(&db_profile, addr_allocation_preference, "profile.cmd.param.addr_allocation_preference");
		_dbstruct_write_int_member_to_db(&db_profile, qci, "profile.cmd.param.qci");
		_dbstruct_write_int_member_to_db(&db_profile, g_dl_bit_rate, "profile.cmd.param.g_dl_bit_rate");
		_dbstruct_write_int_member_to_db(&db_profile, max_dl_bit_rate, "profile.cmd.param.max_dl_bit_rate");
		_dbstruct_write_int_member_to_db(&db_profile, g_ul_bit_rate, "profile.cmd.param.g_ul_bit_rate");
		_dbstruct_write_int_member_to_db(&db_profile, max_ul_bit_rate, "profile.cmd.param.max_ul_bit_rate");


		_set_str_db("profile.cmd.status", "[done]", -1);
		break;

err_db_command_val_read:
		_set_str_db("profile.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_WRITE: {
		unsigned short qmi_error;
		unsigned short qmi_ext_error;
#if defined (MODULE_cinterion) || defined (SUB_NETIF)
		/* nothing to do */
#else
		int skip_first;
#endif

		struct db_struct_profile db_profile;

#if !defined(NO_LTE_REATTACH)
		struct db_struct_profile new_db_profile;
		int need_reattach = 0;
#endif

		struct db_struct_profile old_db_profile;
		int write_profile = 0;
		int i;

		const char* conn_type;

		SYSLOG(LOG_DB, "got DB_COMMAND_VAL_WRITE");

		// read parameters
		memset(&db_profile, 0, sizeof(db_profile));

		_dbstruct_read_str_member_from_db(&db_profile, apn_name, "profile.cmd.param.apn");
		_dbstruct_read_str_member_from_db(&db_profile, user, "profile.cmd.param.user");
		_dbstruct_read_str_member_from_db(&db_profile, pass, "profile.cmd.param.password");
		_dbstruct_read_str_member_from_db(&db_profile, auth_preference, "profile.cmd.param.auth_type");
		_dbstruct_read_str_member_from_db(&db_profile, pdp_type, "profile.cmd.param.pdp_type");
		_dbstruct_read_str_member_from_db(&db_profile, ipv4_addr_pref, "profile.cmd.param.ipv4_addr_pref");

		// read tft params
		for(i = 0; i < __countof(&db_profile.tft_id); i++) {
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], filter_id, _get_indexed_str("profile.cmd.param.tft.filter_id", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], eval_id, _get_indexed_str("profile.cmd.param.tft.eval_id", i, NULL));
			_dbstruct_read_str_member_from_db(&db_profile.tft_id[i], ip_version, _get_indexed_str("profile.cmd.param.tft.ip_version", i, NULL));
			_dbstruct_read_str_member_from_db(&db_profile.tft_id[i], source_ip, _get_indexed_str("profile.cmd.param.tft.source_ip", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], source_ip_mask, _get_indexed_str("profile.cmd.param.tft.source_ip_mask", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], next_header, _get_indexed_str("profile.cmd.param.tft.next_header", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], dest_port_range_start, _get_indexed_str("profile.cmd.param.tft.dest_port_range_start", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], dest_port_range_end, _get_indexed_str("profile.cmd.param.tft.dest_port_range_end", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], src_port_range_start, _get_indexed_str("profile.cmd.param.tft.src_port_range_start", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], src_port_range_end, _get_indexed_str("profile.cmd.param.tft.src_port_range_end", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], ipsec_spi, _get_indexed_str("profile.cmd.param.tft.ipsec_spi", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], tos_mask, _get_indexed_str("profile.cmd.param.tft.tos_mask", i, NULL));
			_dbstruct_read_int_member_from_db(&db_profile.tft_id[i], flow_label, _get_indexed_str("profile.cmd.param.tft.flow_label", i, NULL));
		}

		_dbstruct_read_int_member_from_db(&db_profile, pdp_context_number, "profile.cmd.param.pdp_context_number");
		_dbstruct_read_int_member_from_db(&db_profile, pdp_context_sec_flag, "profile.cmd.param.pdp_context_sec_flag");
		_dbstruct_read_int_member_from_db(&db_profile, pdp_context_primary_id, "profile.cmd.param.pdp_context_primary_id");
		_dbstruct_read_str_member_from_db(&db_profile, addr_allocation_preference, "profile.cmd.param.addr_allocation_preference");
		_dbstruct_read_int_member_from_db(&db_profile, qci, "profile.cmd.param.qci");
		_dbstruct_read_int_member_from_db(&db_profile, g_dl_bit_rate, "profile.cmd.param.g_dl_bit_rate");
		_dbstruct_read_int_member_from_db(&db_profile, max_dl_bit_rate, "profile.cmd.param.max_dl_bit_rate");
		_dbstruct_read_int_member_from_db(&db_profile, g_ul_bit_rate, "profile.cmd.param.g_ul_bit_rate");
		_dbstruct_read_int_member_from_db(&db_profile, max_ul_bit_rate, "profile.cmd.param.max_ul_bit_rate");

		// check madatory fields
#if 0
		// disable this block to accept a blank APN

		if(!_dbstruct_is_valid_member(&db_profile, apn_name) || !strlen(db_profile.apn_name)) {
			SYSLOG(LOG_ERROR, "mandatory field not found - profile.cmd.param.apn");
			goto err_db_command_val_write;
		}
#endif

		SYSLOG(LOG_DEBUG, "finding profile");

#if defined (MODULE_cinterion) || defined (SUB_NETIF)
		profile_index = id + 1;
#else
		if(id_existing) {
			profile_index = id;
		} else {
			/* get default profile */
			profile_index = _qmi_getdef(0, 1);
			if(profile_index < 0) {
				// look for netcommX
				profile_index = _qmi_find_profile(profile, 0, 0);

				// use non-netcomm profile if netcomm does not exist
				if(profile_index < 0) {
					skip_first = (id == 0) ? 0 : 1;
					profile_index = _qmi_find_profile(QMIMGR_NETCOMM_PROFILE, 1, skip_first);
				}
			}

			SYSLOG(LOG_DEBUG, "matched profile = %d,", profile_index);
		}
#endif

		// get old apn setting
		conn_type = _get_str_db("conn_type", "");
		if (!strcmp(conn_type, "3g") || !strcmp(conn_type, "lte")) {
			memset(&old_db_profile, 0, sizeof(old_db_profile));
			if(_qmi_get_profile(profile_index, profile_type, profile, &old_db_profile) < 0) {
				memset(&old_db_profile, 0, sizeof(old_db_profile));
			}
		}

		struct profile_info_t {
			int profile;
			int profile_type;
		};

		/* generic profiles */
		struct profile_info_t profile_info[MAX_PROFILE_COUNT] = {
			{profile_index, profile_type},
			{ -1, -1},
		};

		struct profile_info_t* profile_info_ptr = profile_info;

#ifdef MODULE_PRI_BASED_OPERATION

		/*
			!! another exceptional behaviour for MC7354 Revision: SWI9X15C_05.05.58.01 VZW PRI !!

			According to Sierra, this is not their firmware bug but by design it is required to write a profile to
			3 different profiles -  profile #3 (default) for LTE and profile #0, profile #103 for eHRPD

			This particular structure including specified profile numbers is hard-coded in Sierra firmware
		*/

		/* VZW profiles */
		struct profile_info_t profile_info_vzw[MAX_PROFILE_COUNT] = {
			{profile_index, profile_type},	// profile#3 (default) LTE
			{103, 1},			// profile#103 eHRPD
			{0, 1},				// profile#0 eHRPD
			{ -1, -1},
		};


		const char* rdb_module_pri = strdupa(_get_str_db("priid_carrier", ""));

		if(!strcmp(rdb_module_pri, "VZW")) {
			SYSLOG(LOG_OPERATION, "VZW PRI detected, use VZW profiles");
			profile_info_ptr = profile_info_vzw;
		}
#endif

		/* check if received APN settigns are different */
		/* write profile to MC7304 only if changed, else skip writing */
		if (!strcmp(conn_type, "3g") || !strcmp(conn_type, "lte")) {
			if(strcasecmp(old_db_profile.apn_name, db_profile.apn_name) != 0) {
				write_profile = 1;
			} else if(strcmp(old_db_profile.user, db_profile.user) != 0) {
				write_profile = 1;
			} else if(strcmp(old_db_profile.pass, db_profile.pass) != 0) {
				write_profile = 1;
			} else if(strcmp(old_db_profile.ipv4_addr_pref, db_profile.ipv4_addr_pref) != 0) {
				write_profile = 1;
			} else if((strcasecmp(old_db_profile.auth_preference, db_profile.auth_preference) != 0) &&
				 (strlen(db_profile.user) || strlen(db_profile.pass))){
				write_profile = 1;
			} else if(strcasecmp(old_db_profile.pdp_type, db_profile.pdp_type) != 0) {
				/* pdp type PDP-IP and IPV4 are same, no need to update profile in this case */
				if( !((strcasecmp(old_db_profile.pdp_type, "PDP-IP") == 0) && (strcasecmp(db_profile.pdp_type, "IPV4") == 0)) ) {
					write_profile = 1;
				}
			}
		} else {
			/* not a LTE/3g connection, force write profile */
			write_profile = 1;
		}

		if( write_profile == 1) {
			while(profile_info_ptr->profile >= 0) {
				if(_qmi_create_profile(profile_info_ptr->profile, profile_info_ptr->profile_type, profile, &db_profile, &qmi_error, &qmi_ext_error) < 0) {
#ifdef CONFIG_IGNORE_INVALID_PROFILE_NUMBER
					/*
						## workaround for Sierra MC8754 ##



						MC8754 module always returns error "The request contains an invalid profile number." after modifying a profile with good parameters
						Although, the profile is modified!!

						As we do not use incorrect profile number at all by design, ignoring this error cannot cause any issue

						* detail as following
						Manufacturer: Sierra Wireless, Incorporated
						Model: MC7354
						Revision: SWI9X15C_05.05.58.01 r27044 carmd-fwbuild1 2015/03/05 00:02:40
						MEID: A0000035C4A358
						ESN: 12809715995, 8094411B
						IMEI: 355659060032210
						IMEI SV: 19

					*/

					if(qmi_error == QMI_ERR_EXTENDED_INTERNAL && qmi_ext_error == 0x05) {
						SYSLOG(LOG_DEBUG, "ignore QMI_ERR_EXTENDED_INTERNAL(0x0051/0x05) error for MC7304/MC7354");
					} else
#endif
					{
						SYSLOG(LOG_ERROR, "failed to create qmi profile apn=%s,qmi_error=0x%04x,ext_qmi_error=0x%04x", db_profile.apn_name, qmi_error, qmi_ext_error);
						goto err_db_command_val_write;
					}
				}

				SYSLOG(LOG_OPERATION, "profile created (or modified) profile_idx=%d,apn=%s", profile_info_ptr->profile, db_profile.apn_name);

				profile_info_ptr++;
			}
		}

#if !defined(NO_LTE_REATTACH)
		// get new apn setting
		if (!strcmp(conn_type, "3g") || !strcmp(conn_type, "lte")) {
			memset(&new_db_profile, 0, sizeof(new_db_profile));
			if(_qmi_get_profile(profile_index, profile_type, profile, &new_db_profile) < 0) {
				goto err_db_command_val_write;
			} else {
				if(strcmp(old_db_profile.apn_name, new_db_profile.apn_name) != 0) {
					need_reattach = 1;
				} else if(strcmp(old_db_profile.user, new_db_profile.user) != 0) {
					need_reattach = 1;
				} else if(strcmp(old_db_profile.pass, new_db_profile.pass) != 0) {
					need_reattach = 1;
				} else if(strcmp(old_db_profile.ipv4_addr_pref, new_db_profile.ipv4_addr_pref) != 0) {
					need_reattach = 1;
				} else if(strcmp(old_db_profile.auth_preference, new_db_profile.auth_preference) != 0) {
					need_reattach = 1;
				} else if(strcmp(old_db_profile.pdp_type, new_db_profile.pdp_type) != 0) {
					need_reattach = 1;
				}
			}
		}

		/*
					* Sierra specific command

					// set default profile
					SYSLOG(LOG_OPERATION,"setting default profile - index=%d",profile_index);
					if( _qmi_setdef(1,1,profile_index)<0 ) {
						SYSLOG(LOG_ERROR,"failed to set default profile - index=%d",profile_index);
						goto err_db_command_val_write;
					}
		*/
		if(need_reattach == 1) {

			SYSLOG(LOG_ERROR, "starting LTE procedure");
			SYSLOG(LOG_OPERATION, "initiate dettach network");
			if(_qmi_attach(0) < 0) {
				SYSLOG(LOG_ERROR, "failed to dettach");
				goto err_db_command_val_write;
			}


			SYSLOG(LOG_OPERATION, "waiting dettach network");
			if(_qmi_wait_until_ps_attach(0, QMIMGR_MIDLONG_RESP_TIMEOUT) < 0) {
				SYSLOG(LOG_ERROR, "timeout - waiting dettach network");
				goto err_db_command_val_write;
			}
			// clear handles for all IP family types
			for (i = 0; i < __countof(iptypes); i++) {
				pkt_data_handles[pkt_data_handle][i] = 0;
			}

			SYSLOG(LOG_OPERATION, "initiate attach network");
			if(_qmi_attach(1) < 0) {
				SYSLOG(LOG_ERROR, "failed to attach");
				goto err_db_command_val_write;
			}

			SYSLOG(LOG_OPERATION, "waiting attach network");
			if(_qmi_wait_until_ps_attach(1, QMIMGR_MIDLONG_RESP_TIMEOUT) < 0) {
				SYSLOG(LOG_ERROR, "timeout - waiting attach network");
				goto err_db_command_val_write;
			}
		}
#endif

		SYSLOG(LOG_DB, "DB_COMMAND_VAL_WRITE success");

		_set_str_db("profile.cmd.status", "[done]", -1);
		break;

err_db_command_val_write:
		_set_str_db("profile.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_ACTIVATE: {
		const char* call_end_msg;
#if defined (MODULE_cinterion) || defined (SUB_NETIF)
		/* nothing to do */
#else
		int skip_first;
#endif

		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_ACTIVATE");

		int i;
		for (i = 0; i < __countof(iptypes); i++) {
			if (pkt_data_handles[pkt_data_handle][i]) {
				SYSLOG(LOG_OPERATION, "previous connection exists. disconnecting...");
				if (_qmi_stop_profile(pkt_data_handles[pkt_data_handle][i], iptypes[i]) < 0) {
					SYSLOG(LOG_ERROR, "failed to stop previous connection - handle 0x%08x, IPv%u",
					pkt_data_handles[pkt_data_handle][i], iptypes[i]);
				}
				pkt_data_handles[pkt_data_handle][i] = 0;
			}
		}

		/* use [profile.cmd.param.profile_id] if  we need to activate module profile */
#if defined (MODULE_cinterion) || defined (SUB_NETIF)
		profile_index = id + 1;
#else
		if(id_existing) {
			profile_index = id;
		} else {
			/* get default profile */
			profile_index = _qmi_getdef(0, 1);
			if(profile_index < 0) {
				profile_index = _qmi_find_profile(profile, 0, 0);

				// use non-netcomm profile if netcomm does not exist
				if(profile_index < 0) {
					skip_first = (id == 0) ? 0 : 1;
					profile_index = _qmi_find_profile(QMIMGR_NETCOMM_PROFILE, 1, skip_first);
				}

				if(profile_index < 0) {
					SYSLOG(LOG_ERROR, "cannot find profile - %s", profile);
					goto err_db_command_val_activate;
				}
			}
		}
#endif

		SYSLOG(LOG_OPERATION, "profile found - profile=%s,index=%d", profile, profile_index);

		// read pdp type from profile and only start
		// network interface with IP family required by pdp

		struct db_struct_profile db_profile;
		memset(&db_profile, 0, sizeof(db_profile));
		if (_qmi_get_profile(profile_index, profile_type, profile, &db_profile) < 0) {
			SYSLOG(LOG_ERROR, "cannot get profile - %s, index %d", profile, profile_index);
			goto err_db_command_val_activate;
		}

		unsigned char pdp_type;
		get_pdp_type(&db_profile, 1, &pdp_type); // convert string to: 0-ipv4, 1-ipv6, 2-ipv4v6

		/* get rdb result to write result */
		for (i = 0; i < __countof(iptypes); i++) {
			if ((iptypes[i] == IPV4 && pdp_type == 1) ||
			    (iptypes[i] == IPV6 && pdp_type == 0)) {
				continue;
			}

			if(_qmi_start_profile(iptypes[i], profile_index,
						&pkt_data_handles[pkt_data_handle][i], &call_end_msg) < 0) {
				if(!call_end_msg)
					call_end_msg = "Connection setup timeout";

				result_rdb = _get_str_db("profile.cmd.param.result_rdb", "");
				/* set result to rdb */
				if(*result_rdb) {
					SYSLOG(LOG_ERROR, "failed to start connection - profile=%s,end_msg=%s,rdb='%s', IPv%u",
					profile, call_end_msg, result_rdb, iptypes[i]);
					_set_str_db_with_no_prefix(result_rdb, call_end_msg, -1);
				} else {
					SYSLOG(LOG_ERROR, "failed to start connection - profile=%s,end_msg=%s,IPv%u",
					profile, call_end_msg, iptypes[i]);
				}

				goto err_db_command_val_activate;
			}

			SYSLOG(LOG_OPERATION, "DB_COMMAND_VAL_ACTIVATE success handle=0x%08x, IPv%u",
			pkt_data_handles[pkt_data_handle][i], iptypes[i]);
		}

		_set_str_db("profile.cmd.status", "[done]", -1);

		snprintf(rdb, sizeof(rdb), "session.%d.status", id);
		_set_int_db(rdb, 1, NULL);

#if defined(SKIN_TEL)
		_qmi_wds_get_mtu_size(0);
#endif
		break;

err_db_command_val_activate:
#if defined(SKIN_TEL)
			_qmi_wds_get_mtu_size(1);
#endif
		_set_str_db("profile.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_DEACTIVATE: {
		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_DEACTIVATE");

		int i, has_connection = 0, stop_error = 0;
		for (i = 0; i < __countof(iptypes); i++) {
			if (pkt_data_handles[pkt_data_handle][i]) {
				has_connection = 1;
				if (_qmi_stop_profile(pkt_data_handles[pkt_data_handle][i], iptypes[i]) < 0) {
					SYSLOG(LOG_ERROR, "failed to stop connection - handle 0x%08x, IPv%u",
					pkt_data_handles[pkt_data_handle][i], iptypes[i]);
					stop_error = 1;
					continue;
				}

				SYSLOG(LOG_OPERATION, "DB_COMMAND_VAL_DEACTIVATE success - handle 0x%08x, IPv%u",
						pkt_data_handles[pkt_data_handle][i], iptypes[i]);

				pkt_data_handles[pkt_data_handle][i] = 0;
			}
		}

		if (!has_connection) {
			SYSLOG(LOG_ERROR, "no previous connection");
		}

		if (stop_error) {
			goto err_db_command_val_deactivate;
		}

		_set_str_db("profile.cmd.status", "[done]", -1);

#if defined(SKIN_TEL)
		_qmi_wds_get_mtu_size(1);
#endif

		snprintf(rdb, sizeof(rdb), "session.%d.status", id);
		_set_int_db(rdb, 0, NULL);
		break;

err_db_command_val_deactivate:
#if defined(SKIN_TEL)
		_qmi_wds_get_mtu_size(1);
#endif
		_set_str_db("profile.cmd.status", "[error]", -1);
		break;
	}

	default:
		SYSLOG(LOG_ERROR, "unknown command %s(%d) in %s(%d)", db_cmd, db_cmd_idx, db_var, db_var_idx);
		_set_str_db("profile.cmd.status", "[error] unknown command", -1);
		break;
	}


	// clear command - backward compatibabity
	_set_reset_db(db_var);
}


/*
	Some Qualcomm modules (many of Sierra - MC8704, MC7304, MC7354 and etc) require special commands to start or to stop NMEA stream.

	To allow future flexibility, this start/stop activity depends on an external script - we may need to support different
	commands for different manufactures.

	external stcript name : ctrl_gps_nmea.sh
*/

int qmimgr_ctrl_gps_nmea(int start)
{
	const char* opts[] = {
		"stop",
		"start"
	};

	char cmd[128];
	int rc;
	const char* opt;

	/* get opt string */
	opt = opts[start & 0x01];

	/* run command */
	snprintf(cmd, sizeof(cmd), "%s %s > /dev/null 2> /dev/null' ", "ctrl_gps_nmea.sh", opt);
	rc = _system(cmd);

	if(rc < 0) {
		SYSLOG(LOG_ERROR, "###gps### failed to control GPS nmea stream - (start=%d,cmd=%s,err=%s)", start, cmd, strerror(errno));
	} else {
		SYSLOG(LOG_OPERATION, "###gps### control GPS nmea stream (start=%d,cmd=%s)", start, cmd);
	}

	return rc;
}

void _batch_cmd_stop_gps_on_pds()
{
	/* stop nmea stream */
	SYSLOG(LOG_OPERATION, "###gps### stop nmea stream");
	qmimgr_ctrl_gps_nmea(0);

	SYSLOG(LOG_OPERATION, "###gps### stop auto tracking");
	_qmi_gps_set_auto_tracking(0);

#if 0
	SYSLOG(LOG_OPERATION, "###gps### stop event");
	_qmi_gps_set_event(0, 0);
#endif
}

int _batch_cmd_start_gps_on_pds(int agps)
{
	unsigned char timeout;
	unsigned int interval;
	unsigned int accuracy;

	/* set nmea config */
	{
		unsigned char sentence_mask = 0xff;
		unsigned char port = 0x00;
		unsigned char reporting = 0x01;
		unsigned short ex_sentence_mask = 0x0001;
		int rc;

		SYSLOG(LOG_OPERATION, "###gps### get nmea current config");

		rc = _qmi_gps_get_nmea(&sentence_mask, &port, &reporting, &ex_sentence_mask);
		if(rc < 0) {
			SYSLOG(LOG_ERR, "###gps### failed to get current nmea settings");
			goto err;
		}

		/*
			• 0x01 – GPGGA
			• 0x02 – GPRMC
			• 0x04 – GPGSV
			• 0x08 – GPGSA
			• 0x10 – GPVTG
		*/

		SYSLOG(LOG_OPERATION, "###gps### nmea - sentence_mask=0x%04x", sentence_mask);
		SYSLOG(LOG_OPERATION, "###gps### nmea - port=%d", port);
		SYSLOG(LOG_OPERATION, "###gps### nmea - reporting=%d", reporting);
		SYSLOG(LOG_OPERATION, "###gps### nmea - ex_sentence_mask=%d", ex_sentence_mask);


		/* all types of NMEA */
		sentence_mask = 0xff;
		/* 0x01 – USB */
		port = 0x01;

		reporting = 0;
		/* 0x01 – PQXFI */
		ex_sentence_mask = 0x01;

		SYSLOG(LOG_OPERATION, "###gps### set nmea config");

		if(_qmi_gps_set_nmea(sentence_mask, port, reporting, &ex_sentence_mask) < 0) {
			SYSLOG(LOG_ERR, "###gps### failed to set nmea settings");
			/* MC7304 and MC7354 does not allow to stop NMEA stream */
			/* goto err; */
		}

	}

	/*
		these default settings are from Simple AT port manager

		timeout  : 180 seconds / 3 minutes
		interval : 1 second
		accuracy : 4294967279U (maximum)
	*/
	timeout = _get_int_db_ex("sensors.gps.cmd.timeout", 180, 0);
	interval = _get_int_db_ex("sensors.gps.cmd.interval", 1, 0);
	accuracy = _get_int_db_ex("sensors.gps.cmd.accuracy", 0, 0);
	if(!accuracy)
		accuracy = 4294967279U;

	SYSLOG(LOG_OPERATION, "###gps### tracking parameters - timeout=%d,interval=%d,accuracy=%u", timeout, interval, accuracy);

	SYSLOG(LOG_OPERATION, "###gps### set default tracking session [%s]", agps ? "agps" : "standalone");
	if(_qmi_gps_set_default_tracking(agps ? QMI_PDS_START_TRACKING_SESSION_REQ_TYPE_OPMODE_MSB : QMI_PDS_START_TRACKING_SESSION_REQ_TYPE_OPMODE_STANDALONE, timeout, interval, accuracy) < 0) {
		SYSLOG(LOG_OPERATION, "###gps### failed to set default tracking");
		/* MC7304 and MC7354 does not allow to stop NMEA stream */
		/* goto err; */
	}

	SYSLOG(LOG_OPERATION, "###gps### start event");
	if(_qmi_gps_set_event(1, 1) < 0) {
		SYSLOG(LOG_ERR, "###gps### failed to set event");
		goto err;
	}

	/* Sierra MC7304 & MC7354 is buggy and these modules get frozen by doing this */
#ifndef QMIMGR_CONFIG_DISABLE_AUTO_TRACKING
	SYSLOG(LOG_OPERATION, "###gps### start auto tracking");
	if(_qmi_gps_set_auto_tracking(1) < 0) {
		SYSLOG(LOG_ERR, "###gps### failed to start auto tracking");
		goto err;
	}
#endif

	/* start nmea stream */
	SYSLOG(LOG_OPERATION, "###gps### start nmea stream");

	if(qmimgr_ctrl_gps_nmea(1) < 0) {
		SYSLOG(LOG_ERR, "###gps### failed to start nmea stream");
		goto err;
	}

	return 0;
err:
	return -1;
}

void qmimgr_on_db_command_gps(const char* db_var, int db_var_idx, const char* db_cmd, int db_cmd_idx)
{
	/*
		QMI_PDS_SET_DEFAULT_TRACKING_SESSION
		S] QMI_PDS_START_TRACKING_SESSION
		R] QMI_PDS_GPS_SERVICE_STATE_IND
		S] QMI_PDS_DETERMINE_POSITION
		QMI_PDS_SET_EVENT_REPORT
		R] QMI_PDS_GPS_SERVICE_STATE_IND
	*/


	switch(db_cmd_idx) {
	case DB_COMMAND_VAL_AGPS: {
		if(_gps_stat_agps) {
			_set_str_db_with_no_prefix("sensors.gps.0.cmd.status", "[done]", -1);
		} else {
			_set_str_db_with_no_prefix("sensors.gps.0.cmd.status", "[error]", -1);
		}
		break;
	}

	case DB_COMMAND_VAL_ENABLE: {
		/* stop previos */
		_batch_cmd_stop_gps();

#if !defined(V_HAS_AGPS_) && !defined(V_HAS_AGPS_none)
		/* try agps first */
		SYSLOG(LOG_OPERATION, "###gps### start gps - agps mode");
		if(_batch_cmd_start_gps(1) < 0) {
			SYSLOG(LOG_ERR, "###gps### failed to start AGPS");

			/* stop previos */
			_batch_cmd_stop_gps();

			SYSLOG(LOG_OPERATION, "###gps### start gps - stand alone mode");
			if(_batch_cmd_start_gps(0) < 0) {
				SYSLOG(LOG_ERR, "###gps### failed to start SGPS");
				goto err;
			}

			_gps_stat_agps = 0;
		} else {
			_gps_stat_agps = 1;
		}
#else // AGPS service is disabled.
                SYSLOG(LOG_OPERATION, "###gps### start gps - stand alone mode");
                if(_batch_cmd_start_gps(0) < 0) {
                    SYSLOG(LOG_ERR, "###gps### failed to start SGPS");
                    goto err;
                }

                _gps_stat_agps = 0;
#endif

		/* return rdb result */
		_set_str_db_with_no_prefix("sensors.gps.0.cmd.status", "[done]", -1);

		break;
	}

	case DB_COMMAND_VAL_DISABLE: {
		_batch_cmd_stop_gps();

		/* return rdb result */
		_set_str_db_with_no_prefix("sensors.gps.0.cmd.status", "[done]", -1);

		break;
	}

	default: {
		SYSLOG(LOG_ERR, "###gps### unknown command detected (cmd=%s)", db_cmd);
		goto err;
	}

	}

	_set_reset_db(db_var);
	return;

err:
	_set_str_db_with_no_prefix("sensors.gps.0.cmd.errcode", "[error]", -1);
	_set_reset_db(db_var);
}

/*
 * Save current SIM ICCID and PIN to RDB variables if required (RDB variable "sim.cmd.param.autopin" == 1)
 * SIM ICCID --> "wwan_pin_ccid"
 * SIM PIN --> "wwan_pin"
 *
 * @param pin: SIM PIN to save
 */
static void qmimgr_save_autopin(const char* pin)
{
	const char* simICCID;

	if (!pin || !_get_int_db("sim.cmd.param.autopin", 0)) {
		return;
	}
	/* save current SIM ICCID and PIN */
	simICCID = _get_str_db("system_network_status.simICCID", NULL);
	if (simICCID) {
		/* save simICCID to RDB variable "wwan_pin_ccid" (no prefix) */
		_set_str_db_ex("wwan_pin_ccid", simICCID, -1, 0);
		/* save PIN to RDB variable "wwan_pin" (no prefix) */
		_set_str_db_ex("wwan_pin", pin, -1, 0);
	}
}

void qmimgr_on_db_command_sim(const char* db_var, int db_var_idx, const char* db_cmd, int db_cmd_idx)
{
	char pin[QMIMGR_MAX_DB_VALUE_LENGTH];
	char newpin[QMIMGR_MAX_DB_VALUE_LENGTH];
	char puk[QMIMGR_MAX_DB_VALUE_LENGTH];
	int pin_id;

	int verify_left;
	int unblock_left;

	pin_id = _get_int_db("sim.cmd.param.pin_id", 0);


	if(db_cmd_idx == DB_COMMAND_VAL_ENABLEPIN)
		db_cmd_idx = DB_COMMAND_VAL_ENABLE;
	if(db_cmd_idx == DB_COMMAND_VAL_DISABLEPIN)
		db_cmd_idx = DB_COMMAND_VAL_DISABLE;

	switch(db_cmd_idx) {
	case DB_COMMAND_VAL_VERIFYPIN:
	case DB_COMMAND_VAL_ENABLE:
	case DB_COMMAND_VAL_DISABLE: {
		int stat = -1;

		__strncpy(pin, _get_str_db("sim.cmd.param.pin", ""), sizeof(pin));

		SYSLOG(LOG_OPERATION, "pin - %s", pin);

		if(db_cmd_idx == DB_COMMAND_VAL_DISABLE) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_DISABLE");

			/* verify sim card pin by enabling sim pin lock */
			stat = 0;
			if(!_simcard_pin_enabled)
				stat = _qmi_enable_pin(pin_id, pin, &verify_left, &unblock_left);

			/* disable if pin is okay */
			if(stat >= 0)
				stat = _qmi_disable_pin(pin_id, pin, &verify_left, &unblock_left);
		} else if(db_cmd_idx == DB_COMMAND_VAL_ENABLE) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_ENABLE");

			/* verify sim card pin by disabling sim pin lock */
			stat = 0;
			if(_simcard_pin_enabled)
				stat = _qmi_disable_pin(pin_id, pin, &verify_left, &unblock_left);

			/* enable if pin is okay */
			if(stat >= 0)
				stat = _qmi_enable_pin(pin_id, pin, &verify_left, &unblock_left);
		} else if(db_cmd_idx == DB_COMMAND_VAL_VERIFYPIN) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_VERIFYPIN");
			stat = _qmi_verify_pin(pin_id, pin, &verify_left, &unblock_left);
		}

		SYSLOG(LOG_OPERATION, "stat=%d", stat);

		if(stat < 0) {
			SYSLOG(LOG_ERROR, "failed in _qmi_enable_pin()");
			goto err_db_command_val_enable;
		}

		_qmi_check_pin_status(1);

		// if incorrect pin
		if((verify_left >= 0) || (unblock_left >= 0)) {
			SYSLOG(LOG_OPERATION, "incorrect pin detected");
			goto err_db_command_val_enable;
		} else {
			SYSLOG(LOG_OPERATION, "pin matched");
			qmimgr_save_autopin(pin);
		}

		_set_str_db("sim.cmd.status", "[done]", -1);
		break;

err_db_command_val_enable:
		_set_str_db("sim.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_CHANGEPIN: {
		__strncpy(pin, _get_str_db("sim.cmd.param.pin", ""), sizeof(pin));
		__strncpy(newpin, _get_str_db("sim.cmd.param.newpin", ""), sizeof(newpin));

		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_CHANGEPIN - pin=%s,newpin=%s", pin, newpin);

		if(_qmi_change_pin(pin_id, pin, newpin, &verify_left, &unblock_left) < 0) {
			SYSLOG(LOG_ERROR, "failed in _qmi_change_pin()");
			goto err_db_command_val_changepin;
		}

		_qmi_check_pin_status(1);

		// if incorrect pin
		if((verify_left >= 0) || (unblock_left >= 0)) {
			SYSLOG(LOG_OPERATION, "incorrect pin detected");
			goto err_db_command_val_enable;
		} else {
			SYSLOG(LOG_OPERATION, "pin matched");
			qmimgr_save_autopin(newpin);
		}

		_set_str_db("sim.cmd.status", "[done]", -1);
		break;

err_db_command_val_changepin:
		_set_str_db("sim.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_VERIFYPUK: {
		__strncpy(newpin, _get_str_db("sim.cmd.param.newpin", ""), sizeof(newpin));
		__strncpy(puk, _get_str_db("sim.cmd.param.puk", ""), sizeof(puk));

		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_VERIFYPUK - newpin=%s,puk=%s", newpin, puk);

		if(_qmi_verify_puk(pin_id, puk, newpin, &verify_left, &unblock_left) < 0) {
			SYSLOG(LOG_ERROR, "failed in _qmi_verify_puk()");
			goto err_db_command_val_verifypin;
		}

		_qmi_check_pin_status(1);

		// if incorrect pin
		if((verify_left >= 0) || (unblock_left >= 0)) {
			SYSLOG(LOG_OPERATION, "incorrect pin detected");
			goto err_db_command_val_enable;
		} else {
			SYSLOG(LOG_OPERATION, "pin matched");
			qmimgr_save_autopin(newpin);
		}

		_set_str_db("sim.cmd.status", "[done]", -1);
		break;

err_db_command_val_verifypin:
		_set_str_db("sim.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_CHECK: {
		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_CHECK - pin_id=%d", pin_id);

		if(_qmi_check_pin_status(1) < 0)
			goto err_db_command_val_check;

		_set_str_db("sim.cmd.status", "[done]", -1);
		break;

err_db_command_val_check:
		_set_str_db("sim.cmd.status", "[error]", -1);
		break;
	}

	default:
		SYSLOG(LOG_ERROR, "unknown command %s(%d) in %s(%d)", db_cmd, db_cmd_idx, db_var, db_var_idx);
		_set_str_db("sim.cmd.status", "[error] unknown command", -1);
		break;
	}

	// clear command - backward compatibabity
	_set_reset_db(db_var);
}

void qmimgr_on_db_command_plmn(const char* db_var, int db_var_idx, const char* db_cmd, int db_cmd_idx)
{
	char plmn_select[QMIMGR_MAX_DB_VALUE_LENGTH];

	switch(db_cmd_idx) {
		// ignore - weird web page reaction!
	case DB_COMMAND_VAL_NEGATIVE_1:
		break;

		// scan - why number commmand? stupid!
	case DB_COMMAND_VAL_1: {
		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_1 - scan");

		const char* plmn_list;

		plmn_list = _at_network_scan();
		//plmn_list=_qmi_network_scan();
		if(!plmn_list) {
			SYSLOG(LOG_ERROR, "failed in _qmi_network_scan()");
			goto err_db_command_val_1;
		}

		_set_str_db("PLMN_list", plmn_list, -1);
		_set_str_db("PLMN.cmd.status", "[done]", -1);
		break;

err_db_command_val_1:
		_set_str_db("PLMN.cmd.status", "[error]", -1);
		break;
	}

	// select - stupid number command again!
	case DB_COMMAND_VAL_5: {
		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAL_5 - select");

		__strncpy(plmn_select, _get_str_db("PLMN_select", ""), sizeof(plmn_select));

		SYSLOG(LOG_OPERATION, "plmn_select=%s", plmn_select);

		if(_qmi_network_select(plmn_select) < 0) {
			SYSLOG(LOG_ERROR, "failed in _qmi_network_select()");
			goto err_db_command_val_5;
		}

		_set_reset_db("PLMN_list");
		_set_str_db("PLMN.cmd.status", "[done]", -1);
		break;

err_db_command_val_5:
		_set_str_db("PLMN.cmd.status", "[error]", -1);
		break;
	}

	default:
		SYSLOG(LOG_ERROR, "unknown command %s(%d) in %s(%d)", db_cmd, db_cmd_idx, db_var, db_var_idx);
		_set_str_db("currentband.cmd.status", "[error] unknown command", -1);
		break;
	}

	// clear command - backward compatibabity
	_set_reset_db(db_var);
}

void qmimgr_on_db_command_band(const char* db_var, int db_var_idx, const char* db_cmd, int db_cmd_idx)
{
	char band[QMIMGR_MAX_DB_VALUE_LENGTH];

	switch(db_cmd_idx) {
	case DB_COMMAND_VAL_GET: {
		// current supported bands have been retrieved in init_bands()

		// get current selected band
		if(_qmi_band_get_current() < 0) {
			SYSLOG(LOG_ERROR, "failed to get selected band");
			goto err_db_command_val_get;
		}

		_set_str_db("currentband.cmd.status", "[done]", -1);
		break;

err_db_command_val_get:
		_set_str_db("currentband.cmd.status", "[error]", -1);
		break;
	}

	case DB_COMMAND_VAL_SET: {
		__strncpy(band, _get_str_db("currentband.cmd.param.band", ""), sizeof(band));

		if(_qmi_band_set(band) < 0) {
			SYSLOG(LOG_ERROR, "failed to set band");
			goto err_db_command_val_set;
		}

		_set_str_db("currentband.cmd.status", "[done]", -1);
		break;

err_db_command_val_set:
		_set_str_db("currentband.cmd.status", "[error]", -1);
		break;
	}

	default:
		SYSLOG(LOG_ERROR, "unknown command %s(%d) in %s(%d)", db_cmd, db_cmd_idx, db_var, db_var_idx);
		_set_str_db("currentband.cmd.status", "[error] unknown command", -1);
		break;
	}

	// clear command - backward compatibabity
	_set_reset_db(db_var);
}

void qmimgr_on_db_command_pri_profile(const char* db_var, int db_var_idx, const char* db_cmd, int db_cmd_idx)
{
#ifdef MODULE_PRI_BASED_OPERATION
	static char module_pri[QMIMGR_MAX_DB_VALUE_LENGTH];
	const char* wwan_pri;
	int force_to_update = 0;

	const char* rdb_module_pri;
	const char* rdb_iccid;
	const char* wwan_active;
	const char* omadm_progress;
	int simcard_ok;

	int vzw_pri;

	/* get current wwan profile pri */
	wwan_pri = strdupa(_get_str_db_ex("link.profile.wwan.pri", "", 0));
	wwan_active = strdupa(_get_str_db("omadm_activated", ""));

	/* store pri carrier */
	switch(db_var_idx) {
	case DB_COMMAND_VAR_OMADM_PROGRESS: {
		SYSLOG(LOG_INFO, "[oma-dm] OMADM progress changed (stat='%s')", db_cmd);
		break;
	}

	case DB_COMMAND_VAR_PRIID_CARRIER: {
		SYSLOG(LOG_DEBUG, "[oma-dm] module PRI changed (pri='%s')", db_cmd);
		break;
	}

	case DB_COMMAND_VAR_PRI_PROFILE: {

		switch(db_cmd_idx) {
		case DB_COMMAND_VAL_UPDATE: {
			SYSLOG(LOG_INFO, "[oma-dm] force to update wwan profiles");
			force_to_update = 1;

			break;
		}
		}
		break;

	}

	case DB_COMMAND_VAR_SIM_STATUS: {
		/* nothing to do */
		break;
	}
	}

	omadm_progress = strdupa(_get_str_db("cdma.otasp.progress", ""));
	rdb_module_pri = strdupa(_get_str_db("priid_carrier", ""));
	rdb_iccid = strdupa(_get_str_db("system_network_status.simICCID", ""));
	simcard_ok = !strcmp(_get_str_db("sim.status.status", ""), "SIM OK");

	/* bypass if OMA DM is progressing */
	if(atoi(omadm_progress) == 1) {
		SYSLOG(LOG_INFO, "[oma-dm] OMA-DM in progress - suspend PRI comparison");
		goto fini;
	}

	/* bypass if SIM not ready */
	if(!simcard_ok) {
		SYSLOG(LOG_DEBUG, "[oma-dm] SIM status is not okay");
		goto fini;
	}


	/* bypass if module has no pri */
	if(!*rdb_module_pri || !*wwan_active) {
		SYSLOG(LOG_INFO, "[oma-dm] module PRI not detected (pri='%s',active='%s')", rdb_module_pri, wwan_active);
		goto fini;
	}

	/* detect vzw pri */
	vzw_pri = !strcmp(rdb_module_pri, "VZW");

	/* vzw pri performs OTA-DM when a new SIM card is inserted */
	if(vzw_pri && !*rdb_iccid) {
		SYSLOG(LOG_INFO, "[oma-dm] sim card ICCID not detected (pri='%s',active='%s')", rdb_module_pri, rdb_iccid);
		goto fini;
	}

	if(vzw_pri)
		snprintf(module_pri, sizeof(module_pri), "%s(%s/%s)", rdb_module_pri, wwan_active, rdb_iccid);
	else
		snprintf(module_pri, sizeof(module_pri), "%s(%s)", rdb_module_pri, wwan_active);

	/* bypass if carrier is not changed */
	if(force_to_update || strcmp(wwan_pri, module_pri)) {
		struct walk_thru_ref_t walk_thru_ref;
		int netcomm_detect = 0;

		/* update wwan profile pri */
		_set_str_db_ex("link.profile.wwan.pri", module_pri, -1, 0);

		if(!strcmp(rdb_module_pri, "SPRINT") || vzw_pri) {

			SYSLOG(LOG_ERR, "[oma-dm] start profile synchronization (force=%d,pri='%s'-->'%s')", force_to_update, wwan_pri, module_pri);

			/* search netcomm */
			_qmi_walk_thru_profiles(_qmi_walk_thru_profiles_find_netcomm, &netcomm_detect);
			if(netcomm_detect) {
				SYSLOG(LOG_ERR, "[oma-dm] oma-dm profiles not available");
			} else {
				walk_thru_ref.profile = 1;
				walk_thru_ref.def_profile = _qmi_getdef(0, 1);
				SYSLOG(LOG_ERR, "[oma-dm] default profile detected (profile_no=%d)", walk_thru_ref.def_profile);

				/* delete all wwan profiles */
				SYSLOG(LOG_ERR, "[oma-dm] remove router WWAN profiles");
				_qmi_lp_del_wwan_profiles();

				/* pour to RDB */
				SYSLOG(LOG_ERR, "[oma-dm] write WWAN profiles");
				_qmi_walk_thru_profiles(_qmi_walk_thru_profiles_cb, &walk_thru_ref);
			}

			SYSLOG(LOG_ERR, "[oma-dm] profile synchronization finished (force=%d,pri='%s'-->'%s')", force_to_update, wwan_pri, module_pri);
		} else {
			SYSLOG(LOG_ERR, "[oma-dm] skip profile synchronization (force=%d,pri='%s'-->'%s')", force_to_update, wwan_pri, module_pri);
		}
	}

	if(!vzw_pri) {
	/* notice to connection manager - link.profile RDB is ready */
	if(!_get_int_db("link_profile_ready", 0)) {
		SYSLOG(LOG_ERR, "[oma-dm] notify to Connection manager - link_profile is ready (pri='%s'-->'%s')", wwan_pri, module_pri);
		_set_int_db("link_profile_ready", 1, NULL);
	}

	}
fini:
	return;
#endif
}

// register/deregister to receive QMI_NAS_SIG_INFO_IND
void _qmi_nas_register_sig_info_ind(int enable)
{
	unsigned short tran_id;
	unsigned short qmi_result;
	unsigned short qmi_error;

	unsigned char sig_info = enable ? 1 : 0;

	struct qmimsg_t * msg;
	struct qmimsg_t * rmsg;

	if(!is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH)) {
		SYSLOG(LOG_COMM, "nas_indication_register - feature not enabled (%s)", FEATUREHASH_CMD_SIGSTRENGTH);
		return;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### QMI_NAS_INDICATION_REGISTER (QMI_NAS_SIG_INFO_IND)");

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to create a msg");
		goto err;
	}
	qmimsg_clear_tlv(msg);
	qmimsg_set_msg_id(msg, QMI_NAS_INDICATION_REGISTER);
	qmimsg_add_tlv(msg, QMI_NAS_INDICATION_REGISTER_REQ_TYPE_SIG_INFO, sizeof(sig_info), &sig_info);
	SYSLOG(LOG_OPERATION, "###qmimux### requesting QMI_NAS_INDICATION_REGISTER");

	if(qmiuniclient_write(uni, QMINAS, msg, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to request for QMI_NAS_INDICATION_REGISTER");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### waiting for response - QMI_NAS_INDICATION_REGISTER");

	// wait for response
	rmsg = _wait_qmi_response_ex(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, NULL, 0);
	if(!rmsg) {
		SYSLOG(LOG_ERROR, "###qmimux### qmi response timeout");
		goto err;
	}
	if(qmi_result) {
		SYSLOG(LOG_ERROR, "###qmimux### qmi failure: result=%02x, error=%02x", qmi_result, qmi_error);
		goto err;
	}
	SYSLOG(LOG_INFO, "QMI_NAS_INDICATION_REGISTER (QMI_NAS_SIG_INFO_IND) succeeded. %s", enable ? "Enabled" : "Disabled");
err:
	qmimsg_destroy(msg);
}

void qmimgr_on_db_command(const char* db_var, int db_var_idx, const char* db_cmd, int db_cmd_idx)
{
	SYSLOG(LOG_OPERATION, "db_var=%s, db_var_idx=%d, db_cmd=%s, db_cmd_idx=%d", db_var, db_var_idx, db_cmd, db_cmd_idx);

	switch(db_var_idx) {
	case DB_COMMAND_VAR_BAND: {
		if(is_enabled_feature(FEATUREHASH_CMD_BANDSEL)) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_BAND db command");
			qmimgr_on_db_command_band(db_var, db_var_idx, db_cmd, db_cmd_idx);
		} else {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_BAND db command - feature disabled %s", FEATUREHASH_CMD_BANDSEL);
		}
		break;
	}

	case DB_COMMAND_VAR_PLMN: {
		if(is_enabled_feature(FEATUREHASH_CMD_PROVIDER)) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_PLMN db command");
			qmimgr_on_db_command_plmn(db_var, db_var_idx, db_cmd, db_cmd_idx);
		} else {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_PLMN db command - feature disabled %s", FEATUREHASH_CMD_PROVIDER);
		}
		break;
	}

	case DB_COMMAND_VAR_SIM: {
		if(is_enabled_feature(FEATUREHASH_CMD_SIMCARD)) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_SIM db command");
			qmimgr_on_db_command_sim(db_var, db_var_idx, db_cmd, db_cmd_idx);
		} else {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_SIM db command - feature disabled %s", FEATUREHASH_CMD_SIMCARD);
		}
		break;
	}

	case DB_COMMAND_VAR_DEBUG_LEVEL: {
		int debug_level;

		debug_level = atoi(db_cmd);
		setlogmask(LOG_UPTO(debug_level + LOG_ERROR));
		break;
	}

	case DB_COMMAND_VAR_FAST_STATS: {
		int new_fast_stats_mode;
		SYSLOG(LOG_INFO, "fast_stats_mode = %s, old_mode=%d", db_cmd, _g_fast_stats_mode);
		new_fast_stats_mode = atoi(db_cmd);
		if(_g_fast_stats_mode != new_fast_stats_mode) {
		    _g_fast_stats_mode = new_fast_stats_mode;
		    _qmi_nas_register_sig_info_ind(_g_fast_stats_mode);
		}
		break;
	}

	case DB_COMMAND_VAR_PROFILE: {
		if(is_enabled_feature(FEATUREHASH_CMD_CONNECT)) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_PROFILE db command");
			qmimgr_on_db_command_profile(db_var, db_var_idx, db_cmd, db_cmd_idx);
		} else {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_PROFILE db command - feature disabled %s", FEATUREHASH_CMD_CONNECT);
		}
		break;
	}

	case DB_COMMAND_VAR_CELL_INFO_TIMER: {
		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_CELL_INFO_TIMER db command");

		int old_timer = cell_info_timer;
		cell_info_timer = atoi(db_cmd);

		if((old_timer == 0) && (cell_info_timer != 0)) {
			SYSLOG(LOG_DEBUG, "Start Cell Info, time = %d", cell_info_timer);
			funcschedule_add(sched, 0, cell_info_timer, qmimgr_callback_on_schedule_cell_info, NULL);
		}
		break;
	}

#ifdef V_SMS_QMI_MODE_y
	case DB_COMMAND_VAR_SMS: {
		if(is_enabled_feature(FEATUREHASH_CMD_SMS)) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_SMS db command");
			qmimgr_on_db_command_sms(db_var, db_var_idx, db_cmd, db_cmd_idx);
		} else {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_SMS db command - feature disabled %s", FEATUREHASH_CMD_SMS);
		}

		break;
	}
#endif

	case DB_COMMAND_VAR_GPS: {
		if(is_enabled_feature(FEATUREHASH_CMD_GPS)) {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_GPS db command");
			qmimgr_on_db_command_gps(db_var, db_var_idx, db_cmd, db_cmd_idx);
		} else {
			SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_GPS db command - feature disabled %s", FEATUREHASH_CMD_GPS);
		}
		break;
	}

	case DB_COMMAND_VAR_SIM_STATUS:
	case DB_COMMAND_VAR_OMADM_PROGRESS:
	case DB_COMMAND_VAR_PRI_PROFILE:
	case DB_COMMAND_VAR_PRIID_CARRIER: {

		SYSLOG(LOG_OPERATION, "got DB_COMMAND_VAR_PRIID_CARRIER");
		qmimgr_on_db_command_pri_profile(db_var, db_var_idx, db_cmd, db_cmd_idx);
		break;
	}

#ifdef QMI_VOICE_y
	case DB_COMMAND_VAR_VOICE_CTRL: {
		qmivoice_on_rdb_ctrl(db_var, db_cmd);
		break;
	}

	case DB_COMMAND_VAR_VOICE_NOTI: {
		qmivoice_process_wr_msg_event();
		break;
	}
#endif

	default:
		SYSLOG(LOG_ERROR, "db command not processed - db=%s(0x%04x),cmd=%s(0x%04x)", db_var, db_var_idx, db_cmd, db_cmd_idx);
		break;
	}
}

void qmimgr_callback_on_db()
{
	int total_triggers;
	struct dbenumitem_t* item;
	int i;
	int len;

	const char* db_var;
	int db_var_idx;

	char db_cmd[QMIMGR_MAX_DB_VALUE_LENGTH];
	int db_cmd_idx;
	const char* db_var_without_prefix;

	total_triggers = dbenum_enumDb(dbenum);

	// bypass if no trigger
	if(total_triggers <= 0) {
		SYSLOG(LOG_DB, "database handler is called but no triggered variables detected - %d", total_triggers);
		return;
	}


	// print trigger information
	i = 0;
	item = dbenum_findFirst(dbenum);
	while(item) {
		SYSLOG(LOG_DB, "triggered #%d - %s", i++, item->szName);
		item = dbenum_findNext(dbenum);
	}
	SYSLOG(LOG_DB, "total %d triggered database found", total_triggers);

	// call db command handler
	item = dbenum_findFirst(dbenum);
	while(item) {
		db_var = item->szName;

		db_var_without_prefix = _get_not_prefix_dbvar(db_var);

		// get datbase variable index
		db_var_idx = dbhash_lookup(dbvars, db_var_without_prefix);
		if(db_var_idx < 0) {
			SYSLOG(LOG_ERROR, "database variable %s not subscribed but triggered", db_var);
		}

		// read command
		db_cmd[0] = 0;
		db_cmd_idx = -1;
		len = sizeof(db_cmd);


		if(rdb_get(_s, db_var, db_cmd, &len) < 0) {
			SYSLOG(LOG_ERROR, "failed to read a triggered database variable(%s) - %s", db_var, strerror(errno));
		} else {
#ifdef QMI_VOICE_y
			if((db_var_idx == DB_COMMAND_VAR_VOICE_CTRL) || (db_var_idx == DB_COMMAND_VAR_VOICE_NOTI)) {
				db_cmd_idx = -1;
				qmimgr_on_db_command(db_var_without_prefix, db_var_idx, db_cmd, db_cmd_idx);
			} else {
#endif
				if(!db_cmd[0]) {
					SYSLOG(LOG_DEBUG, "ignoring zero length command from variable %s", db_var);
				} else {
					db_cmd_idx = dbhash_lookup(dbcmds, db_cmd);
					if(db_cmd_idx < 0) {
						SYSLOG(LOG_OPERATION, "unknown command %s read from database variable %s", db_cmd, db_var);
					}

					qmimgr_on_db_command(db_var_without_prefix, db_var_idx, db_cmd, db_cmd_idx);
				}
#ifdef QMI_VOICE_y
			}
#endif
		}

		item = dbenum_findNext(dbenum);
	}
}

/*
	wait_sec      : total seconds in select loop

	* wait to get the reply transaction

	wait_serv_id : service id
	wait_tran_id : transaction id
	wait_msg_id  : 0 (zero)
	ret_msg      : return the transaction to

	* wait until the reply transaction is processed

	wait_serv_id : service id
	wait_tran_id : transaction id
	wait_msg_id  : message id
	ret_msg      : NULL

	* wait to get the AT reply transaction

	at_tran_id   : transaction id
	at_ret_q     : return the reply to


	## warning

	the returns (at_req_q and ret_msg) are valid until another call to this function
*/

int _wait_in_select(struct qmiuniclient_t* uni, long wait_sec, int wait_serv_id, unsigned short wait_tran_id, unsigned short wait_msg_id, struct qmimsg_t** ret_msg, unsigned short at_tran_id, struct strqueue_t** at_ret_q, int rdb)
{
	fd_set readfds;
	fd_set writefds;
	struct timeval tv;

	int nfds;
	int max_fd;

	clock_t cur;

	int stat;

	clock_t start;

	#ifdef MODULE_PRI_BASED_OPERATION
	static clock_t last_sync_time;
	static clock_t current_time;
	unsigned short tran_id;
	struct strqueue_t* q;
	const char* result;
	int registration_status;
	#endif

#if 0
	int waiting;

	waiting = (wait_serv_id > 0) && (wait_tran_id > 0);
#endif

	int db_fd;

	int qmistat;
	int portstat;

	// get database driver handle
	db_fd = rdb_fd(_s);
	if(db_fd < 0) {
		SYSLOG(LOG_ERROR, "database not open");
		goto err;
	}

	// get start sec
	start = _get_current_sec();

	FD_ZERO(&writefds);
	FD_ZERO(&readfds);

	qmiuniclient_setfds(uni, NULL, &readfds, &writefds);
	if(port_is_open(port))
		port_setfds(port, NULL, &readfds, &writefds);

	while(1) {
		/* bypass immediately when stopped */
		if(!loop_running) {
			SYSLOG(LOG_OPERATION, "process term detected");
			goto timeout;
		}
		// process qmi select
		qmistat = qmiuniclient_process_select(uni, wait_serv_id, wait_tran_id, wait_msg_id, ret_msg);
		// process at select
		if(port_is_open(port))
			portstat = port_process_select(port, at_tran_id, at_ret_q);
		else
			portstat = 0;

		/* bypass immediately when stopped */
		if(!loop_running) {
			SYSLOG(LOG_OPERATION, "process term detected");
			goto timeout;
		}

		if(portstat < 0) {
			SYSLOG(LOG_OPERATION, "port select error - maybe module unplug?");
			goto err;
		}

		// if qmi matched
		if(qmistat == 1) {
			SYSLOG(LOG_OPERATION, "qmi matched");
			break;
		}

		// if at matched
		if(portstat == 1) {
			SYSLOG(LOG_OPERATION, "at matched");
			break;
		} else if(portstat == 2) {
			SYSLOG(LOG_OPERATION, "at command timeout");
			goto timeout;
		}

		// run schedule
		funcschedule_exec(sched);

		// init timevak - minimum tick is 100ms
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if(wait_sec > 0)
			tv.tv_sec = 1;

		// init fds
		max_fd = -1;
		FD_ZERO(&writefds);
		FD_ZERO(&readfds);

		// add db fd
		if(rdb) {
#ifdef DEBUG
			SYSLOG(LOG_DEBUG, "putting database fd %d to readfds", db_fd);
#endif
			FD_SET(db_fd, &readfds);
			max_fd = db_fd;
		}

		// build readfds and writefds
		qmiuniclient_setfds(uni, &max_fd, &readfds, &writefds);
		if(port_is_open(port))
			port_setfds(port, &max_fd, &readfds, &writefds);

		// select
		nfds = max_fd + 1;
#ifdef DEBUG
		SYSLOG(LOG_DEBUG, "selecting nfds=%d", nfds);
#endif
		stat = select(nfds, &readfds, &writefds, NULL, &tv);

		/* update heartbeat if required */
		update_heartbeat(0);

#ifdef DEBUG
		SYSLOG(LOG_DEBUG, "woke up from select stat=%d", stat);
#endif

		if(stat < 0) {
			if(errno == EINTR)
				continue;

			SYSLOG(LOG_ERROR, "failed to select");
			goto err;
		} else if(stat == 0) {
#ifdef DEBUG
			SYSLOG(LOG_DEBUG, "select timed out");
#endif
		}

		// do database fd
		if(FD_ISSET(db_fd, &readfds)) {
			SYSLOG(LOG_DEBUG, "database fd got read event");

			SYSLOG(LOG_DEBUG, "calling database event handler");
			qmimgr_callback_on_db();
		}

		#ifdef MODULE_PRI_BASED_OPERATION
		/* check if Verizon Private Network */
		if(vzw_pripub) {
		      //SYSLOG(LOG_ERR,"[oma-dm] Verizon Private Network detected");
		      //isSynchronized = _get_int_db_ex("link.profile.wwan.isSynchronized",0,0);
		      /* check if sync already happened or not */
		      //if(!isSynchronized) {
			  current_time = _get_current_sec();
			  if(current_time-last_sync_time>=30) {
				SYSLOG(LOG_ERR,"[oma-dm] 30 seconds timer expired ...will try to sync");

				SYSLOG(LOG_ERR,"[oma-dm] calling synchronizeAPN() function ...");
				int ret;
				last_sync_time = _get_current_sec();
				/* synchronize router APN from module APN */
				ret = synchronizeAPN();
				switch(ret) {
				  case 0:
				    SYSLOG(LOG_ERR,"[oma-dm] APN synchronization happened successfully");
				    break;
				  case -2:
				    SYSLOG(LOG_ERR,"[oma-dm] Module Default Profile APN and Router Default link profile APN matches...so no need to sync");
				    break;
				  case -1:
				    SYSLOG(LOG_ERR,"[oma-dm] module profile could not be read...hence no question of sync");
				    break;
				  default:
				    break;
				}
			  }
			  //else {
				//SYSLOG(LOG_ERR,"[oma-dm] 30 seconds timer not expired,so no sync attempted ...");
			  //}
		      //}
		      //else {
			  //SYSLOG(LOG_ERR,"[oma-dm] APN synchronization already happened before");
		      //}

		      /* workaround for "Registration denied" issue */
#if 0
		      registration_status=_get_int_db_ex("wwan.0.system_network_status.reg_stat",0,0);
		      SYSLOG(LOG_ERR,"SIM registration status is %d", registration_status);
		      if(registration_status == 3) {
			  SYSLOG(LOG_ERR,"SIM registration status shows Registration denied");

			  // request - get operators
			  SYSLOG(LOG_ERROR,"sending - AT+CGATT?");
			  if(_request_at("AT+CGATT?",QMIMGR_LONG_RESP_TIMEOUT,&tran_id)<0) {
			      SYSLOG(LOG_ERR,"failed to queue cmd - AT+CGATT?");
			  }

			  /*
			  AT+CGATT?
			  +CGATT: 1
			  */

			  // get result
			  SYSLOG(LOG_ERR,"waiting - AT+CGATT?");
			  q=_wait_at_response(tran_id,QMIMGR_AT_SELECT_TIMEOUT);

			  // get result
			  result=_get_at_result(q,1,1,"+CGATT: ");
			  if(!result) {
				SYSLOG(LOG_ERR,"incorrect result - cmd=AT+CGATT?");
			  }

			  if(!strncmp(result,"1",1)) {
				SYSLOG(LOG_ERR,"setting reg_stat RDB variable");
				_set_int_db_ex("wwan.0.system_network_status.reg_stat",1,NULL,0);
			  }
		      }
#endif
		}
		#endif

		// break if timed out
		cur = _get_current_sec();
		if(cur - start >= wait_sec) {
#ifdef DEBUG
			SYSLOG(LOG_DEBUG, "select time expired - wait_sec=%ld", wait_sec);
#endif
			goto timeout;
		}
	}

	return 1;

timeout:
	return 0;

err:
	return -1;
}

#if 0
// disabled to hide the compiler warning
// this function holds the execution while it is pumping - not used for now
int _wait_at_until(unsigned short tran_id, int timeout)
{
	int stat;

	if(!tran_id)
		return -1;

	funcschedule_suspend(sched);
	stat = _wait_in_select(uni, timeout, 0, 0, 0, NULL, tran_id, NULL, 1);
	funcschedule_resume(sched);

	return stat;
}
#endif

struct strqueue_t* _wait_at_response(unsigned short tran_id, int timeout) {
	struct strqueue_t* q;
	int stat;

	q = NULL;

	funcschedule_suspend(sched);
	stat = _wait_in_select(uni, timeout, 0, 0, 0, NULL, tran_id, &q, 1);
	funcschedule_resume(sched);

	if(stat == 0) {
		SYSLOG(LOG_ERROR, "at response timeout - tran_id=%d", tran_id);
		goto err;
	}
	if(stat < 0) {
		SYSLOG(LOG_ERROR, "at response error - tran_id=%d", tran_id);
		goto err;
	}

	return q;

err:
	return NULL;
}

int _wait_qmi_until(int serv_id, int timeout, unsigned short tran_id, unsigned short msg_id)
{
	int stat;

	funcschedule_suspend(sched);
	stat = _wait_in_select(uni, timeout, serv_id, tran_id, msg_id, NULL, 0, NULL, 1);
	funcschedule_resume(sched);

	return stat;
}

struct qmimsg_t* _wait_qmi_indication_ex(int serv_id, int timeout, unsigned short msg_id, unsigned short* result, unsigned short* error, unsigned short* ext_error, int rdb) {
	struct qmimsg_t* msg;
	int stat;

	funcschedule_suspend(sched);
	stat = _wait_in_select(uni, timeout, serv_id, 0, msg_id, &msg, 0, NULL, rdb);
	funcschedule_resume(sched);

	if(stat == 0) {
		SYSLOG(LOG_ERROR, "qmi asynchronous indication timeout - msg_id=0x%04x", msg_id);
		goto err;
	} else if(stat < 0) {
		SYSLOG(LOG_ERROR, "qmi asynchronous indication error - msg_id=0x%04x", msg_id);
		goto err;
	}

	return msg;

err:
	return NULL;
}

struct qmimsg_t* _wait_qmi_indication(int serv_id, int timeout, unsigned short msg_id, unsigned short* result, unsigned short* error, unsigned short* ext_error) {
	return _wait_qmi_indication_ex(serv_id, timeout, msg_id, result, error, ext_error, 1);
}

struct qmimsg_t* _wait_qmi_response_ex(int serv_id, int timeout, unsigned short tran_id, unsigned short* result, unsigned short* error, unsigned short* ext_error, int rdb) {
	struct qmimsg_t* msg;
	int stat;

	unsigned short qmi_result;
	unsigned short qmi_error;
	unsigned short qmi_ext_error;

	funcschedule_suspend(sched);
	stat = _wait_in_select(uni, timeout, serv_id, tran_id, 0, &msg, 0, NULL, rdb);
	funcschedule_resume(sched);

	if(stat == 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout - tran_id=0x%04x", tran_id);
		goto err;
	} else if(stat < 0) {
		SYSLOG(LOG_ERROR, "qmi response error - tran_id=0x%04x", tran_id);
		goto err;
	}

	if(qmimgr_callback_on_preprocess(uni, QMI_MSGTYPE_RESP, tran_id, msg, &qmi_result, &qmi_error, &qmi_ext_error, NULL) < 0) {
		SYSLOG(LOG_ERROR, "failed in qmimgr_callback_on_preprocess() - tran_id=0x%04x", tran_id);
		goto err;
	}

	if(result)
		*result = qmi_result;

	if(error)
		*error = qmi_error;

	if(ext_error)
		*ext_error = qmi_ext_error;

	return msg;

err:
	return NULL;
}

struct qmimsg_t* _wait_qmi_response(int serv_id, int timeout, unsigned short tran_id, unsigned short* result, unsigned short* error, unsigned short* ext_error) {
	return _wait_qmi_response_ex(serv_id, timeout, tran_id, result, error, ext_error, 1);
}


int _request_qmi_tlv_ex(struct qmimsg_t* msg, int serv_id, unsigned short msg_id, unsigned short* tran_id, unsigned char t, unsigned short l, void* v, int clear)
{
	unsigned short queued_trans_id;

	// set request
	qmimsg_set_msg_id(msg, msg_id);

	// add tlv
	if(clear)
		qmimsg_clear_tlv(msg);

	if(v) {
		if(qmimsg_add_tlv(msg, t, l, v) < 0) {
			SYSLOG(LOG_ERROR, "failed to add TLV into msg(0x%04x)", msg_id);
			goto err;
		}
	}

	// write
	SYSLOG(LOG_DEBUG, "writing msg(serv=%d,id=0x%04x) request", serv_id, msg_id);
	if(qmiuniclient_write(uni, serv_id, msg, &queued_trans_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - msg=0x%04x, serv_id=%d", msg->msg_id, serv_id);
		goto err;
	}

	if(tran_id)
		*tran_id = queued_trans_id;

	SYSLOG(LOG_DEBUG, "msg(0x%04x) queued - trans_id=0x%02x", msg_id, queued_trans_id);

	return 0;

err:
	return -1;
}

int _request_qmi_tlv(struct qmimsg_t* msg, int serv_id, unsigned short msg_id, unsigned short* tran_id, unsigned char t, unsigned short l, void* v)
{
	return _request_qmi_tlv_ex(msg, serv_id, msg_id, tran_id, t, l, v, 1);
}

int _request_qmi(struct qmimsg_t* msg, int serv_id, unsigned short msg_id, unsigned short* tran_id)
{
	return _request_qmi_tlv_ex(msg, serv_id, msg_id, tran_id, 0, 0, NULL, 1);
}

int _qmi_uim_queue(struct qmimsg_t * msg, unsigned short msg_id,
                   unsigned short * tran_id, unsigned char t, unsigned short l,
                   void * v, int clear)
{
	unsigned short queued_trans_id;

	struct qmi_uim_session_info session = { 0, 0 };

	qmimsg_set_msg_id(msg, msg_id);
	if(clear) {
		qmimsg_clear_tlv(msg);
	}
	if(qmimsg_add_tlv(msg, QMI_UIM_REQ_TYPE_SESSION_INFO, sizeof(session),
	                  &session) < 0) {
		SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv - "
		       "QMI_UIM_REQ_TYPE_SESSION_INFO");
		goto err;
	}
	if(v && qmimsg_add_tlv(msg, t, l, v) < 0) {
		SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv - type=%d", t);
		goto err;
	}

	SYSLOG(LOG_DEBUG, "writing msg(serv=%d, id=0x%04x) request",
	       QMIUIM, msg_id);
	if(qmiuniclient_write(uni, QMIUIM, msg, &queued_trans_id) < 0) {
		SYSLOG(LOG_ERROR, "Failed to queue - msg=0x%04x, serv_id=%d",
		       msg_id, QMIUIM);
		goto err;
	}

	if(tran_id) {
		*tran_id = queued_trans_id;
	}
	SYSLOG(LOG_DEBUG, "msg(0x%04x) queued - trans_id=0x%02x",
	       msg_id, queued_trans_id);

	return 0;

err:
	return -1;
}

int _request_at(const char* cmd, int timeout, unsigned short* tran_id)
{
	if(!port_is_open(port)) {
		SYSLOG(LOG_ERROR, "at port not opened");
		goto err;
	}

	if(port_queue_command(port, cmd, timeout, tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue at command - %s", cmd);
		goto err;
	}

	SYSLOG(LOG_DEBUG, "at command(%d) queued", *tran_id);

	return 0;
err:
	return -1;
}

int _request_mandatory_at_msgs()
{
	return 0;
}

int _request_mandatory_qmi_msgs()
{
	struct qmimsg_t* msg;
	unsigned short tran_id;

	msg = NULL;


	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to allocate msg for mandatory qmi msgs");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "extract module related information");


	// QMI_DMS_GET_DEVICE_MFR - manufacture
	_request_qmi(msg, QMIDMS, QMI_DMS_GET_DEVICE_MFR, &tran_id);
	// wait for response
	if(_wait_qmi_until(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result) - QMIMGR_GENERIC_RESP_TIMEOUT");
	}

	// QMI_DMS_GET_DEVICE_MODEL_ID - module model name
	_request_qmi(msg, QMIDMS, QMI_DMS_GET_DEVICE_MODEL_ID, &tran_id);
	// wait for response
	if(_wait_qmi_until(QMIDMS, QMI_DMS_GET_DEVICE_MODEL_ID, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result) - QMI_DMS_GET_DEVICE_MODEL_ID");
	}

	// QMI_DMS_GET_DEVICE_REV_ID - module revision
	_request_qmi(msg, QMIDMS, QMI_DMS_GET_DEVICE_REV_ID, &tran_id);
	// wait for response
	if(_wait_qmi_until(QMIDMS, QMI_DMS_GET_DEVICE_REV_ID, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result) - QMI_DMS_GET_DEVICE_REV_ID");
	}


	// QMI_DMS_GET_DEVICE_SERIAL_NUMBERS - module serial (imei)
	_request_qmi(msg, QMIDMS, QMI_DMS_GET_DEVICE_SERIAL_NUMBERS, &tran_id);
	// wait for response
	if(_wait_qmi_until(QMIDMS, QMI_DMS_GET_DEVICE_SERIAL_NUMBERS, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result) - QMI_DMS_GET_DEVICE_SERIAL_NUMBERS");
	}

	SYSLOG(LOG_OPERATION, "module related information done");

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}
#ifdef MODULE_PRI_BASED_OPERATION
int synchronizeAPN() {
	char profile_name[QMIMGR_MAX_DB_VALUE_LENGTH];
	const char *profileDefApnRdbstr;
	char rdbStr[50];
	int profile_type=0,i,def_router_profile=1,def_module_profile=3;
	struct db_struct_profile db_profile;
	struct db_struct_profile *p_db_profile;
	int def_router_profile_status;
	char def_module_profile_str[10];

	memset(rdbStr, '\0', 50);
	memset(profile_name, '\0', QMIMGR_MAX_DB_VALUE_LENGTH);
	memset(&db_profile,0,sizeof(struct db_struct_profile));
	memset(def_module_profile_str, '\0', 10);

	SYSLOG(LOG_ERR,"[oma-dm] Inside synchronizeAPN function");

	if(count == 0) {
		/* notice to connection manager - link.profile RDB is ready */
		if(!_get_int_db_ex("wwan.0.link_profile_ready",0,0)) {
			SYSLOG(LOG_ERR,"[oma-dm] notify to Connection manager - link_profile is ready");
			_set_int_db_ex("wwan.0.link_profile_ready",1,NULL,0);
		}
		count++;
	}

	/* Get Default enabled router profile */
	for(i=1; i <=LINK_PROFILE_MAX; i++) {
		sprintf(rdbStr, "link.profile.%d.enable", i);
		def_router_profile_status = _get_int_db(rdbStr, 1);
		if(def_router_profile_status == 1) {
			def_router_profile = i;
			break;
		}
	}
	/* check if already connected or not(for public SIM) */
	/*sprintf(rdbStr, "link.profile.%d.status", def_router_profile);
	def_link_prof_status=strdupa(_get_str_db_ex(rdbStr, "",0));
	SYSLOG(LOG_ERR,"link profile status is %s", def_link_prof_status);
	if(!strcmp("up", def_link_prof_status)) {
		SYSLOG(LOG_ERR,"Must have connected ...hence no sync required further");
		//_set_int_db_ex("link.profile.wwan.isSynchronized",1,NULL,0);
	}*/

	/* Get Profile Default APN from Module */
	if( _qmi_get_profile(def_module_profile,profile_type,profile_name,&db_profile)<0 ) {
		_set_str_db("profile.cmd.status","[error] failed to read profile",-1);
		SYSLOG(LOG_ERR,"[oma-dm] module profile %d could not be read", def_module_profile);
		return -1;
	}

	SYSLOG(LOG_ERR,"[oma-dm] profile name obtained from module profile %d is %s", def_module_profile, profile_name);

	/* Get Profile Default APN name from RDB */
	sprintf(rdbStr, "link.profile.%d.apn", def_router_profile);
	profileDefApnRdbstr = strdupa(_get_str_db_ex(rdbStr, "", 0));

	p_db_profile=&db_profile;

	SYSLOG(LOG_ERR,"[oma-dm] Module Default Profile APN is %s", _dbstruct_get_str_member(p_db_profile,apn_name));
	SYSLOG(LOG_ERR,"[oma-dm] Link Profile APN obtained from RDB is %s", profileDefApnRdbstr);	
	/* sync module APN to router APN,only if they are different */
	if(strcasecmp(_dbstruct_get_str_member(p_db_profile,apn_name), profileDefApnRdbstr)) {
		/* pour to RDB */
		SYSLOG(LOG_ERR,"[oma-dm] write router WWAN profile");

		/* synchronize router wwan profile with module profile */
		_set_fmt_dbvar_ex(0,"link.profile.%d.apn",_dbstruct_get_str_member(p_db_profile,apn_name),
							  def_router_profile);
		sprintf(def_module_profile_str, "%d", def_module_profile);
		_set_fmt_dbvar_ex(0,"link.profile.%d.module_profile_idx",def_module_profile_str,
							  def_router_profile);
		_set_fmt_dbvar_ex(0,"link.profile.%d.defaultroute","1",def_router_profile);
		_set_fmt_dbvar_ex(0,"link.profile.%d.trigger","1",def_router_profile);

		/* set the isSynchronized flag so that this loop is not encountered again */
		//_set_int_db_ex("link.profile.wwan.isSynchronized",1,NULL,0);

		/* notice to connection manager - link.profile RDB is ready */
		if(!_get_int_db_ex("wwan.0.link_profile_ready",0,0)) {
		    SYSLOG(LOG_ERR,"[oma-dm] notify to Connection manager - link_profile is ready");
		    _set_int_db_ex("wwan.0.link_profile_ready",1,NULL,0);
		}

		return 0;
	}
	else {
		SYSLOG(LOG_ERR,"[oma-dm] Module Default Profile APN and Router Default link profile APN matches");
		return -2;
	}
}
#endif

int _qmi_init_bands(void)
{
	struct qmimsg_t* msg;
	unsigned short tran_id;

	SYSLOG(LOG_DEBUG, "initialize bands");

	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "failed to allocate msg for bands initialisation");
		return -1;
	}

	// get device band capability - this will set _g_band_cap & _g_lte_band_cap
	if(_request_qmi(msg, QMIDMS, QMI_DMS_GET_BAND_CAPABILITY, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - QMI_DMS_GET_BAND_CAPABILITY");
		goto err;
	}
	if(_wait_qmi_until(QMIDMS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout - QMI_DMS_GET_BAND_CAPABILITY");
		goto err;
	}

	// get current band selection
	// will trigger qmi_band_set if current band selection is not custom allowed
	if(_request_qmi(msg, QMINAS, QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE,
	                &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "failed to queue - "
		       "QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE");
		goto err;
	}
	if(_wait_qmi_until(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
		SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result) - "
		       "QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE");
		goto err;
	}

	qmimsg_destroy(msg);
	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

/*
 * register NAS indication by sending QMI_NAS_INDICATION_REGISTER request
 * Current register to receive:
 * - Network registration rejection cause
 *
 * Returns 0 on success, -1 otherwise
 */
static int qmi_nas_indication_register(void)
{
	struct qmi_easy_req_t er;
	struct qmi_nas_indication_register_network_reject_information network_reject_info;
	int rc;

	/* init req */
	rc=_qmi_easy_req_init(&er, QMINAS,QMI_NAS_INDICATION_REGISTER);
	if(rc<0)
		goto err;

	/* Enable reporting of QMI_NAS_NETWORK_REJECT_IND */
	network_reject_info.reg_network_reject = 0x01;
	/* Do not suppress reporting of QMI_NAS_SYS_INFO_IND when only the reject_cause field has changed */
	network_reject_info.suppress_sys_info = 0x00;
	qmimsg_add_tlv(er.msg, QMI_NAS_INDICATION_REGISTER_TYPE_NETWORK_REJECT_INFORMATION, sizeof(network_reject_info), &network_reject_info);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

/*
 * Set deltas to trigger nas sig info indication.
 * delta: minimum change of signal strength to trigger indication (unit: 0.1dBm)
 * rpt_rate: period (in seconds) that LTE signal is checked for reporting. 0 means default configuration.
 * avg_period: averaging period (in seconds) to be used for the LTE signal. 0 means default configuration.
 */
void _qmi_nas_config_sig_info(unsigned short delta, unsigned char rpt_rate, unsigned char avg_period)
{
	unsigned short tran_id;
	unsigned short qmi_result;
	unsigned short qmi_error;

	unsigned short cdma_rssi_delta = delta;
	unsigned short hdr_rssi_delta = delta;
	unsigned short gsm_rssi_delta = delta;
	unsigned short wcdma_rssi_delta = delta;
	unsigned short lte_rsrp_delta = delta;
	unsigned short tdscdma_rscp_delta = delta;

	struct qmi_nas_config_sig_info2_req_lte_rpt_conf lte_rpt_conf = {rpt_rate, avg_period};

	struct qmimsg_t * msg;
	struct qmimsg_t * rmsg;

	if(!is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH)) {
		SYSLOG(LOG_COMM, "nas_config_sig_info2 - feature not enabled (%s)", FEATUREHASH_CMD_SIGSTRENGTH);
		return;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### QMI_NAS_CONFIG_SIG_INFO2");

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to create a msg");
		goto err;
	}
	qmimsg_clear_tlv(msg);
	qmimsg_set_msg_id(msg, QMI_NAS_CONFIG_SIG_INFO2);
	qmimsg_add_tlv(msg, QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_CDMA_RSSI_DELTA, sizeof(cdma_rssi_delta), &cdma_rssi_delta);
	qmimsg_add_tlv(msg, QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_RSSI_DELTA, sizeof(hdr_rssi_delta), &hdr_rssi_delta);
	qmimsg_add_tlv(msg, QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_GSM_RSSI_DELTA, sizeof(gsm_rssi_delta), &gsm_rssi_delta);
	qmimsg_add_tlv(msg, QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_WCDMA_RSSI_DELTA, sizeof(wcdma_rssi_delta), &wcdma_rssi_delta);
	qmimsg_add_tlv(msg, QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RSRP_DELTA, sizeof(lte_rsrp_delta), &lte_rsrp_delta);
	qmimsg_add_tlv(msg, QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RPT_CONF, sizeof(lte_rpt_conf), &lte_rpt_conf);
	qmimsg_add_tlv(msg, QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_RSCP_DELTA, sizeof(tdscdma_rscp_delta), &tdscdma_rscp_delta);
	SYSLOG(LOG_OPERATION, "###qmimux### requesting QMI_NAS_CONFIG_SIG_INFO2");

	if(qmiuniclient_write(uni, QMINAS, msg, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to request for QMI_NAS_CONFIG_SIG_INFO2");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### waiting for response - QMI_NAS_CONFIG_SIG_INFO2");

	// wait for response
	rmsg = _wait_qmi_response_ex(QMINAS, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, &qmi_result, &qmi_error, NULL, 0);
	if(!rmsg) {
		SYSLOG(LOG_ERROR, "###qmimux### qmi response timeout");
		goto err;
	}
	if(qmi_result) {
		SYSLOG(LOG_ERROR, "###qmimux### qmi failure: result=%02x, error=%02x", qmi_result, qmi_error);
		goto err;
	}
	SYSLOG(LOG_INFO, "QMI_NAS_CONFIG_SIG_INFO2 succeeded");

err:
	qmimsg_destroy(msg);
}

int qmimgr_loop()
{
	// register all client handlers
	SYSLOG(LOG_OPERATION, "registering qmi client handlers with qmiuniclient");
	qmiuniclient_register_callback(uni, qmimgr_callback_on_common);

#ifdef CONFIG_LINUX_QMI_DRIVER
	{
		int i;
		int stat = is_quectel_qmi_proxy(qmi_port) ? 0 : -1; // skip setup QMI if using proxy

		for (i = 0; i < QMIMGR_STARTUP_RETRY_MAX && stat < 0; i++, sleep(1)) {
			/* startup packets orignal from GobiNet driver */
			stat = _qmi_set_ready();
			if(stat < 0) {
				SYSLOG(LOG_ERROR, "Device unresponsive to QMI / failed to get QMI ready response");
				continue;
			}

			stat = _qmi_set_set_sync();
			if(stat < 0) {
				SYSLOG(LOG_ERROR, "QMI CTL Sync Procedure Error / failed to get QMI sync response");
				continue;
			}

			stat = _qmi_set_data_format();
			if(stat < 0) {
				SYSLOG(LOG_ERROR, "Device cannot set requested data format / failed to set QMI data format");
				continue;
			}
		}

		if(stat < 0) {
			SYSLOG(LOG_ERROR, "QMI manager failed to start");
			goto err;
		}

		// register db handler
		for(i = 0; i < QMIUNICLIENT_SERVICE_CLIENT; i++) {
			/* skip QMICTL - CTL is born with ID 0 */
			if(i == QMICTL)
				continue;

			const char* service_name = qmi_service_cfg[i].name;
			if(!service_name)
				service_name = "unknown";

			/* bypass if disabled */
			if(!qmi_service_cfg[i].enable) {
				SYSLOG(LOG_OPERATION, "###qmimux### service (%s#%d) disabled", service_name, i);
				continue;
			}

			SYSLOG(LOG_OPERATION, "###qmimux### obtain client ID (serv_name=%s,serv_id=%d)", service_name, i);
			int client_id;
			/* QMI_CTL_GET_CLIENT_ID - get client id */
			if(_qmi_get_client_id(uni->servtrans[i]->stype, &client_id) < 0) {

				if(qmi_service_cfg[i].mandatory) {
					SYSLOG(LOG_ERROR, "failed to obtain client ID (serv_name=%s,serv_id=%d)", service_name, i);
					goto err;
				}

				SYSLOG(LOG_OPERATION, "###qmimux### failed to open an optional servtran (serv_name=%s,serv_id=%d)", service_name, i);
				qmiuniclient_destroy_servtran(uni, i);
				continue;
			}

			/* set client id */
			SYSLOG(LOG_OPERATION, "###qmimux### got client ID (serv_name=%s,serv_id=%d,client_id=%d)", service_name, i, client_id);
			qmimux_set_client_id(uni->qmux, i, uni->servtrans[i]->stype, client_id);
		}

		// setup Quectel mux data port if using quectel proxy
		if (is_quectel_qmi_proxy(qmi_port)) {
			for (stat = -1, i = 0; i < QMIMGR_STARTUP_RETRY_MAX && stat < 0; i++, sleep(1)) {
				stat = quectel_bind_mux_data_port();
				if (stat < 0) {
					SYSLOG(LOG_ERROR, "ERROR: failed to setup QMAP mux data port, try:%d", i);
				}
			}
			if (stat < 0) {
				SYSLOG(LOG_ERROR, "QMI manager failed to start");
				goto err;
			}
		}
	}
#endif

	if(_qmi_check_versions() < 0) {
		SYSLOG(LOG_ERROR, "QMI versions are incompatible! You might consider upgrading module firmware.");
		return -1;
	}

	/*
	 * Configure signal strength indication to be monitored every second,
	 * averaged over 1 second, and trigger by a minimum change of 1 dBm.
	 * Since the signal strength statistics are all accurate to 1 dBm,
	 * it does not make much sense to set delta below that value.
	 */
	_qmi_nas_config_sig_info(10, 1, 1);

	_g_fast_stats_mode = _get_int_db("fast_stats", 0);

	_qmi_nas_register_sig_info_ind(_g_fast_stats_mode);

	// register db handler
	SYSLOG(LOG_OPERATION, "registering db fd and handler with qmiuniclient");

	if(is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
		// mandatory qmi messages
		_request_mandatory_qmi_msgs();
		// mandatory at messages
		_request_mandatory_at_msgs();
	}

	// QMI_WDS_GET_PKT_SRVC_STATUS - pdp session status
	if(is_enabled_feature(FEATUREHASH_CMD_CONNECT)) {
		struct qmimsg_t* msg;
		unsigned short tran_id;

		SYSLOG(LOG_DEBUG, "generic schedule triggered");

		msg = qmimsg_create();
		if(!msg) {
			SYSLOG(LOG_ERROR, "failed to allocate msg for scheduled msgs");
			goto err_sigstrenth;
		}

		_request_qmi(msg, QMIWDS, QMI_WDS_GET_PKT_SRVC_STATUS, &tran_id);

err_sigstrenth:
		qmimsg_destroy(msg);
	}

	// initialise QMI band capability and limitation
	if(is_enabled_feature(FEATUREHASH_CMD_BANDSEL)) {
		if(_qmi_init_bands() < 0) {
			SYSLOG(LOG_ERROR, "failed to init bands");
			return -1;
		}
	}

#ifdef CONFIG_DISABLE_SCHEDULE_GENERIC_QMI
	SYSLOG(LOG_ERROR, "!!!! warning !!!! schedule for generic QMI is disabled");
#else
	// scheduling - generic
	SYSLOG(LOG_DEBUG, "adding generic schedule immeidately");
	funcschedule_add(sched, 0, 0, qmimgr_callback_on_schedule_generic_qmi, NULL);
#endif

#ifdef V_SMS_QMI_MODE_y
	/* start sms schedule */
	if(is_enabled_feature(FEATUREHASH_CMD_SMS)) {
		char sready;

		/* enable service ready event */
		sready = 1;
		_qmi_sms_register_indications(NULL, NULL, NULL, &sready, NULL);

		/* apply current sms service */
		_qmi_sms_update_sms_type(NULL);

		/* update sms cfg */
		qmimgr_update_sms_cfg();

		/* init NV route */
		_qmi_sms_init_nv_route();

		/* initially read all sms */
		_qmi_sms_readall();

		/* enable sms event */
		_qmi_sms_set_event(1);

		/* start sms schedule */
		qmimgr_callback_on_schedule_sms_qmi(NULL);

#if 0
		_qmi_sms_get_trans_layer_info();
		_qmi_sms_get_domain_ref_config();
#endif
	}
#endif


	if(is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
		// scheduling - sim card
		SYSLOG(LOG_DEBUG, "adding static sim schedule immeidately");
		funcschedule_add(sched, 0, 0, qmimgr_callback_on_schedule_sim_card_qmi, NULL);

		// scheduling - generic AT
		SYSLOG(LOG_DEBUG, "adding generic AT schedule immeidately");
		funcschedule_add(sched, 0, 0, qmimgr_callback_on_schedule_generic_at, NULL);

		// scheduling - sim card at
		if(port_is_open(port)) {
			SYSLOG(LOG_DEBUG, "adding static AT sim schedule immeidately");
			funcschedule_add(sched, 0, 0, qmimgr_callback_on_schedule_sim_card_at, NULL);
		}
	}

	// schedule cell info
	cell_info_timer = _get_int_db("cellinfo.timer", 0);
	if(cell_info_timer) {
		SYSLOG(LOG_DEBUG, "adding cell info schedule - %d sec", cell_info_timer);
		funcschedule_add(sched, 0, cell_info_timer, qmimgr_callback_on_schedule_cell_info, NULL);
	}

	if(is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		/* register NAS indication */
		if (qmi_nas_indication_register() < 0) {
			SYSLOG(LOG_ERROR, "failed to register NAS indication");
		}
	}

	// selecting
	while(loop_running) {
		// select
#ifdef DEBUG
		SYSLOG(LOG_DEBUG, "exec select-loop");
#endif
		if(_wait_in_select(uni, 1, 0, 0, 0, NULL, 0, NULL, 1) < 0)
			break;
	}

	SYSLOG(LOG_OPERATION, "out of main-loop");

	if(!loop_running) {
		SYSLOG(LOG_OPERATION, "termination by signal");
		return 0;
	} else {
		SYSLOG(LOG_OPERATION, "abnormal termination");
	}

#ifdef CONFIG_LINUX_QMI_DRIVER
err:
#endif
	return -1;
}

int is_pid_running(pid_t pid)
{
	char achProcPID[128];
	struct stat statProc;

	snprintf(achProcPID, sizeof(achProcPID), "/proc/%d", pid);
	return stat(achProcPID, &statProc) >= 0;
}

static int _in_system_call = 0;

int _system(const char* cmd)
{
	int rc;

	_in_system_call = 1;
	rc = system(cmd);
	_in_system_call = 0;

	return rc;
}

void sig_handler(int signum)
{
	pid_t	pid;
	int stat;

	if(signum != SIGHUP)
		SYSLOG(LOG_OPERATION, "caught signal %d", signum);

	switch(signum) {
	case SIGHUP:
		break;

	case SIGUSR1:
		break;

	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		SYSLOG(LOG_OPERATION, "signal detected - %d\n", signum);
		loop_running = 0;
		break;

	case SIGCHLD:
		if(_in_system_call) {
			SYSLOG(LOG_OPERATION, "bypass SIGCHLD during system() call");
		} else {
			if((pid = waitpid(-1, &stat, WNOHANG)) > 0)
				SYSLOG(LOG_OPERATION, "Child %d terminated\n", pid);
		}
		break;
	}
}

void release_singleton(void)
{
	char lockfile[128];
	sprintf(lockfile, "/var/lock/subsys/"QMIMGR_APP_NAME"%d", instance);
	unlink(lockfile);
}

void enter_singleton(void)
{
	char lockfile[128];
	char achPID[128];
	int fd;
	int cbRead;

	sprintf(lockfile, "/var/lock/subsys/"QMIMGR_APP_NAME"%d", instance);

	pid_t pid;
	int cbPID;

	fd = open(lockfile, O_RDWR | O_CREAT | O_EXCL, 0640);
	if(fd < 0) {
		if(errno == EEXIST) {
			fd = open(lockfile, O_RDONLY);

			// get PID from lock
			cbRead = read(fd, achPID, sizeof(achPID));
			if(cbRead > 0)
				achPID[cbRead] = 0;
			else
				achPID[0] = 0;

			pid = atoi(achPID);
			if(!pid || !is_pid_running(pid)) {
				SYSLOG(LOG_ERROR, "deleting the lockfile - %s", lockfile);

				close(fd);
				unlink(lockfile);

				enter_singleton();
				return;
			}
		}

		SYSLOG(LOG_ERROR, "another instance of %s already running (because creating lock file %s failed: %s)", QMIMGR_APP_NAME, lockfile, strerror(errno));
		exit(EXIT_FAILURE);
	}

	pid = getpid();
	cbPID = sprintf(achPID, "%d\n", pid);

	write(fd, achPID, cbPID);
	close(fd);
}

#if 0
void port_on_noti(struct port_t* port, const char* noti)
{
	printf("noti=%s\n", noti);
}

void port_on_recv(struct port_t* port, int tran_id, struct strqueue_t* resultq, int timeout)
{
	struct strqueue_element_t* el;

	printf("tran_id=%d\n", tran_id);

	el = strqeueu_walk_first(resultq);
	while(el) {
		printf("el->str=%s\n", el->str);
		el = strqeueu_walk_next(resultq);
	}
}

int test_port()
{
	struct port_t* port;

	fd_set readfds;
	fd_set writefds;
	struct timeval tv;

	int max_fd = 0;
	int fds;

	int stat;

	int tran_id;

	port = port_create();

	port_open(port, "/dev/ttyUSB0");

	port_register_callbacks(port, port_on_noti, port_on_recv);

	port_queue_command(port, "ati", 3);
	port_queue_command(port, "at+cops=?", 1);
	port_queue_command(port, "at+cpin?", 3);
	tran_id = port_queue_command(port, "atd0413237592;", 3);
	port_queue_command(port, "at+cops?", 3);
	port_queue_command(port, "at+csq", 3);


	port_queue_command(port, "at+chup", 3);


	while(1) {
		max_fd = -1;

		FD_ZERO(&writefds);
		FD_ZERO(&readfds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		port_setfds(port, &max_fd, &readfds, &writefds);

		fds = max_fd + 1;
		stat = select(fds, &readfds, &writefds, NULL, &tv);
		port_process_select(port, 0, NULL);

		if(stat < 0)
			break;
	}


	port_close(port);

	port_destroy(port);

	return 0;
}

int test_strqueue()
{
	struct strqueue_t* q_a;
	struct strqueue_t* q_b;

	q_a = strqueue_create();
	q_b = strqueue_create();

	struct strqueue_element_t el;

	el.str = "abc";
	el.tran_id = 1;
	el.timeout = 0;

	strqueue_add(q_a, &el);
	strqueue_add(q_a, &el);
	strqueue_add(q_a, &el);
	strqueue_add(q_a, &el);

	strqueue_vomit(q_a, q_b);
	strqueue_eat(q_a, q_b);

	strqueue_remove(q_a, 1);
	strqueue_remove(q_a, 1);
	strqueue_remove(q_a, 1);
	strqueue_remove(q_a, 1);

	strqueue_add(q_a, &el);
	strqueue_add(q_a, &el);
	strqueue_add(q_a, &el);
	strqueue_add(q_a, &el);

	strqueue_vomit(q_a, q_b);
	strqueue_eat(q_a, q_b);

	strqueue_destroy(q_a);
	strqueue_destroy(q_b);

	return 0;
}
#endif

/*
 * ------ QMI request footprint linked list ------
 *
 * Some QMI responses have to match against the corresponding requests
 * in order to figure out the exact meaning of the response.
 * For example, QMI_UIM_READ_TRANSPARENT, the request includes path & fild ID
 * of the file to be read, while the response just includes the value.
 * Without matching against the request, we have no idea what file the returned
 * value is for.
 * The request footprint linked list is implemented for this purpose.
 * Whenever, we send out a request of this kind, we append an element to the
 * list including tran_id and a user-defined data. When we receive a response
 * that needs matching, we search through the list for the tran_id.
 * If a match is found, the element is removed from the list, and the
 * user-defined data is used to explain the value included in the response.
 * Due to missing responses, some elements in the list could become stale.
 * Thus, a limit on the list length is set by MAX_REQ_FOOTPRINT_COUNT, and
 * oldest elements could be removed from the list when the limit is exceeded.
 */

/* head of qmi request footprint linked list */
static struct linkedlist * _qmi_req_footprint_head = NULL;

/*
 * Add an element to the tail of footprint linked list.
 * Params:
 *   tran_id: transaction id of the request
 *   data: pointer to the request data
 *   data_len: length of the request data.
 *   Warning: If data points to a nil-terminated string, data_len should be set
 *            to 0 so that the length will be inferred by strlen(data)+1.
 *            For other types of data, data_len must be a positive integer.
 * Return:
 *   0 if success, -1 if error
 * Note: The content pointed by data will be copied to the data field
 *       (dynamically allocated) of the newly added element.
 *       After the call returns, the caller is free to do anything to data
 *       including freeing it.
 *       If the linked list length (after addition) is longer than
 *       MAX_REQ_FOOTPRINT_COUNT, the oldest elements will be removed (with
 *       data field freed).
 */
int qmi_req_footprint_add(unsigned short tran_id, const void * data, int data_len)
{
	struct qmi_req_footprint_t * fp;
	struct linkedlist * ll;
	int count;

	if(!(fp = malloc(sizeof(*fp)))) {
		return -1;
	}
	fp->tran_id = tran_id;
	if(!data) {
		fp->data = NULL;
		fp->data_len = 0;
	} else {
		if(data_len < 1) { // infer length from strlen
			fp->data_len = strlen(data) + 1;
		} else {
			fp->data_len = data_len;
		}
		if(!(fp->data = malloc(fp->data_len))) {
			free(fp);
			return -1;
		}
		memcpy(fp->data, data, fp->data_len);
	}
	linkedlist_init(&fp->list);
	if(!_qmi_req_footprint_head) {
		_qmi_req_footprint_head = &fp->list;
	} else {
		count = 1;
		for(ll = _qmi_req_footprint_head; ll->next; ll = ll->next) {
			count++;
		}
		linkedlist_add(ll, &fp->list);
		while(count > MAX_REQ_FOOTPRINT_COUNT) {
			ll = _qmi_req_footprint_head->next;
			linkedlist_del(_qmi_req_footprint_head);
			fp = (struct qmi_req_footprint_t *)_qmi_req_footprint_head;
			_free(fp->data);
			free(fp);
			_qmi_req_footprint_head = ll;
			count--;
		}
	}

	return 0;
}

/*
 * Match and remove an element from footprint linked list.
 * Params:
 *   tran_id: transaction id of the request to be matched against and removed
 *   data: pointer to hold the pointer to the request data
 *   data_len: pointer to hold the length of the request data
 * Return:
 *   0 if no match, 1 if matched and removed
 * Note: The caller is responsible for freeing the memory pointed by data
 *       if a match is found.
 */
int qmi_req_footprint_remove(unsigned short tran_id, void ** data, int * data_len)
{
	struct linkedlist * ll;
	struct qmi_req_footprint_t * fp;

	if(!_qmi_req_footprint_head) {
		return 0;
	}
	for(ll = _qmi_req_footprint_head; ll; ll = ll->next) {
		fp = (struct qmi_req_footprint_t *)ll;
		if(fp->tran_id != tran_id) {
			continue;
		}
		if(data) {
			*data = fp->data;
		}
		if(data_len) {
			*data_len = fp->data_len;
		}
		if(ll == _qmi_req_footprint_head) {
			_qmi_req_footprint_head = ll->next;
		}
		linkedlist_del(ll);
		free(fp);
		return 1;
	}
	return 0;
}

/*
 * Remove the oldest elements from the linked list until its length is not
 * longer than MAX_REQ_FOOTPRINT_COUNT.
 * Return:
 *   0 if nothing is removed, 1 if at least one element is removed.
 */
int qmi_req_footprint_purge(void)
{
	struct linkedlist * ll;
	struct qmi_req_footprint_t * fp;
	int count = 0;

	for(ll = _qmi_req_footprint_head; ll; ll = ll->next) {
		count++;
	}
	if(count <= MAX_REQ_FOOTPRINT_COUNT) {
		return 0;
	}

	while(count > MAX_REQ_FOOTPRINT_COUNT) {
		ll = _qmi_req_footprint_head->next;
		linkedlist_del(_qmi_req_footprint_head);
		fp = (struct qmi_req_footprint_t *)_qmi_req_footprint_head;
		_free(fp->data);
		free(fp);
		_qmi_req_footprint_head = ll;
		count--;
	}
	return 1;
}

