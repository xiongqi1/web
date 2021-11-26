#ifndef __SIERRACNS_H__
#define __SIERRACNS_H__


#include <semaphore.h>

#include "base.h"
#include "sierrahip.h"
#include "list.h"

//#define GPS_PKT_DEBUG
#define SIERRACNS_PACKET_TIMEOUT						3						// 3 seconds
#define SIERRACNS_PACKET_RETRY							3						// 3 seconds
#define SIERRACNS_PACKET_PARAMMAXLEN					246
#define SIERRACNS_PACKET_MAX_PAYLOAD_LEN_1ST_PKT		150

#define SIERRACNS_PACKET_MSGID_HOST						0x2b
#define SIERRACNS_PACKET_MSGID_MODEM					0x6b

#define SIERRACNS_PACKET_OP_GET							0x01
#define SIERRACNS_PACKET_OP_GET_RESPONSE				0x02
#define SIERRACNS_PACKET_OP_SET							0x03
#define SIERRACNS_PACKET_OP_SET_RESPONSE				0x04
#define SIERRACNS_PACKET_OP_NOTIFY_ENABLE				0x05
#define SIERRACNS_PACKET_OP_NOTIFY_ENABLE_RESP			0x06
#define SIERRACNS_PACKET_OP_NOTIFICATION				0x07

// non-notification objects
#define SIERRACNS_PACKET_OBJ_HEARTBEAT								0x0000
#define SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_VERSION				0x0001
#define SIERRACNS_PACKET_OBJ_RETURN_FIRMWARE_BUILD_DATE				0x0002
#define SIERRACNS_PACKET_OBJ_RETURN_HARDWARE_VERSION				0x0003
#define SIERRACNS_PACKET_OBJ_RETURN_BOOT_VERSION					0x0004

#define SIERRACNS_PACKET_OBJ_RETURN_RADIO_TEMPERATURE				0x0008
#define SIERRACNS_PACKET_OBJ_RETURN_RADIO_VOLTAGE					0x0009

#define SIERRACNS_PACKET_OBJ_REPORT_CURRENT_BAND					0x0016
#define SIERRACNS_PACKET_OBJ_SET_CURRENT_BAND						0x0017
#define SIERRACNS_PACKET_OBJ_RETURN_MODEM_DATE_AND_TIME				0x001b

#define SIERRACNS_PACKET_OBJ_REPORT_NETWORK_STATUS					0x1000
#define SIERRACNS_PACKET_OBJ_REPORT_RADIO_INFORMATION				0x1001
#define SIERRACNS_PACKET_OBJ_SERVICE_PROVIDER_NAME					0x1003
#define SIERRACNS_PACKET_OBJ_RETURN_IMSI							0x1004
#define SIERRACNS_PACKET_OBJ_AVAILABLE_SERVICE_DETAIL				0x1006
#define SIERRACNS_PACKET_OBJ_MANAGE_RADIO_POWER						0x0007
#define SIERRACNS_PACKET_OBJ_DISABLE_MODEM							0x1011

#define SIERRACNS_PACKET_OBJ_RETURN_SIM_STATUS						0x1041
#define	SIERRACNS_PACKET_OBJ_RETURN_MODEM_MODEL						0x1063
#define SIERRACNS_PACKET_OBJ_RETURN_IMEI							0x1067

#define SIERRACNS_PACKET_OBJ_RETURN_SIM_SMS_CONFIGURATION_DETAILS	0x102b
#define SIERRACNS_PACKET_OBJ_COPY_MO_SMS_MESSAGE_TO_SIM				0x1087
#define SIERRACNS_PACKET_OBJ_SEND_MO_SMS_MESSAGE_TO_NETWORK			0x1088
#define SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS		0x1020
#define SIERRACNS_PACKET_OBJ_GET_UNREAD_SMS_MESSAGE					0x1021
#define SIERRACNS_PACKET_OBJ_GET_READ_SMS_MESSAGE					0x1023
#define SIERRACNS_PACKET_OBJ_DELETE_MO_SMS_MESSAGE					0x1029

#define SIERRACNS_PACKET_OBJ_INITIATE_VOICE_CALL					0x102d
#define SIERRACNS_PACKET_OBJ_REPORT_CALL_PROGRESS					0x1066
#define SIERRACNS_PACKET_OBJ_MANAGE_MISSED_VOICE_CALLS_COUNT		0x102c

#define SIERRACNS_PACKET_OBJ_CALL_CONTROL_FOR_UMTS					0x7009
#define SIERRACNS_PACKET_OBJ_SELECT_AUDIO_PROFILE					0x001a
#define SIERRACNS_PACKET_OBJ_SEND_DTMF_OVERDIAL_DIGITS				0x1039

#define SIERRACNS_PACKET_OBJ_RETURN_PROFILE_SUMMARY					0x7001
#define SIERRACNS_PACKET_OBJ_MANAGE_PACKET_SESSION					0x7004
#define SIERRACNS_PACKET_OBJ_RETURN_IP_ADDERSS						0x7006

#define SIERRACNS_PACKET_OBJ_READ_PROFILE							0x7002
#define SIERRACNS_PACKET_OBJ_WRITE_PROFILE							0x7003
#define SIERRACNS_PACKET_OBJ_MANAGE_PROFILE							0x700e


#define SIERRACNS_PACKET_OBJ_REPORT_SYSTEM_NETWORK_STATUS			0x700a
#define SIERRACNS_PACKET_OBJ_ENABLE_CHV1_VERIFICATION				0x103f
#define SIERRACNS_PACKET_OBJ_CHANGE_CHV_CODES						0x1040
#define SIERRACNS_PACKET_OBJ_VERIFY_CHV_CODE						0x103e
#define SIERRACNS_PACKET_OBJ_SEND_MEP_UNLOCK_CODE					0x1019

#define SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_RESULT				0x0f0a
#define SIERRACNS_PACKET_OBJ_REPORT_POSITION_DET_FAILURE			0x0f0e
#define SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_COMPLETE			0x0f0b
#define SIERRACNS_PACKET_OBJ_REPORT_LOCATION_FIX_ERROR				0x0f0c

#define SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX					0x0f02
#define SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION					0x0f04
#define SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION				0x0f05

#define SIERRACNS_PACKET_OBJ_SELECT_PLMN							0x100f
#define SIERRACNS_PACKET_OBJ_MANUAL_PLMN_READINESS					0x103c
#define SIERRACNS_PACKET_OBJ_AVAILABLE_PLMN_LIST					0x1042

#define SIERRACNS_PACKET_OBJ_PRI									0x0023
#define SIERRACNS_PACKET_OBJ_RSCP									0x700b

#define SIERRACNS_PACKET_OBJ_RETURN_ICCID							0x7007


typedef enum
{
	sierracns_stat_success = 0,
	sierracns_stat_failure,
	sierracns_stat_timeout
} sierracns_stat;


typedef struct
{
	unsigned short wObjectId;
	unsigned char bOpType;
	unsigned char __reserved1;
	unsigned int dwAppId;
	unsigned short wParamLen;
} __packedStruct sierracns_hdr;


typedef struct
{
	struct list_head list;

	int nTryCnt;
	clock_t tickSent;

	int cbCnsPck;
	sierracns_hdr* pCnsPck;

	void* pUsrRef;

} cnsentry;

typedef struct
{
	struct list_head list;

	int nRepeatSec;
	clock_t tickInsert;

	int cbCnsPck;
	sierracns_hdr* pCnsPck;

	BOOL fSent;
	BOOL fStopIfError;

	BOOL fError;
} permcnsentry;


typedef struct
{
	sierrahip* pHip;

	struct list_head queueHdr;

	void* lpfnCnsHandler;

	struct list_head permList;

} sierracns;

typedef void (SIERRACNS_CNSHANDLER)(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, sierracns_stat cnsStat);

sierracns* sierracns_create(void);
void sierracns_destroy(sierracns* pCns);

BOOL sierracns_openDev(sierracns* pCns, const char* lpszDev);
void sierracns_closeDev(sierracns* pCns);

BOOL sierracns_write(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam);
BOOL sierracns_writeNotifyEnable(sierracns* pCns, unsigned short wObjId);

BOOL sierracns_writeGet(sierracns* pCns, unsigned short wObjId);
BOOL sierracns_writeSet(sierracns* pCns, unsigned short wObjId);

BOOL sierracns_addPerm(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, int repeatSec, BOOL fStopIfError);
BOOL sierracns_addPermGet(sierracns* pCns, unsigned short wObjId, int repeatSec, int fStopIfError);

void sierracns_setCnsHandler(sierracns* pCns, SIERRACNS_CNSHANDLER* lpfnCnsHandler);

void sierracns_onTick(sierracns* pCns);

#endif


