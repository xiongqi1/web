#ifndef __PDU_H__
#define __PDU_H__

#include <time.h>
#include <stdint.h>
#include <linux/types.h>
#include <syslog.h>
#include "../logmask.h"

#define __packed__	__attribute__((packed))

// 3GPP - address type in Address fields
enum addr_type
{
	atUnknown=0,
	atInternational=1,
	atNational=2,
	atNetwork=3,
	atSubscriber=4,
	atAlphanumeric=5,
	atAbbr=6,
	atReserved=7
};

// 3GPP - plan indentification in Address fields
enum numbering_plan_ident
{
	npiUnknown=0,
	npiIsdnTelephoneNumberingPlan=1,
	npiDataNumberingPlan=3,
	npiTelexNumberingPlan=4,
	npiServiceCenterSpecific1=5,
	npiServiceCenterSpecific2=6,
	npiNationalNumberingPlan=8,
	npiPrivateNumberingPlan=9,
	npiErmesNumberingPlan=10,
	npiReserved=15,
};


// 3GPP - Address fields
#define MAX_ADDR_LEN	32
struct addr_fields
{
	unsigned char cbAddrLen;
	unsigned char bAddressType;
	char addrNumber[0];
} __packed__;

typedef enum
{
	MOBILE_NUMBER = 0,
	SMSC_NUMBER
} addr_field_type;

// 3GPP - timestamp fields
struct timestamp_fields
{
	unsigned char bYear;
	unsigned char bMonth;
	unsigned char bDay;
	unsigned char bHour;
	unsigned char bMinute;
	unsigned char bSecond;
	unsigned char bTZ;
} __packed__;

typedef enum
{
	SMS_DELIVER = 0,	/* SMS_DELIVER_REPORT */
	SMS_SUBMIT,		/* SMS_SUBMIT_REPORT */
	SMS_STATUS_REPORT,	/* SMS_COMMAND */
	SMS_RESERVED_TYPE
} tpmti_field_type;

typedef enum
{
	MORE_MSG = 0,
	NO_MORE_MSG
} tpmms_field_type;

typedef enum
{
	UDH_NON_EXIST = 0,
	UDH_EXIST
} tpudhi_field_type;

// 3GPP - sms deliver type : 3GPP TS 123.040 9.2.2
struct sms_deliver_type
{
	unsigned int mti:2;				/* Message Type Indicator */
	unsigned int mms:1;				/* More Message to Send */
	unsigned int lp:1;				/* Loop Prevention */
	unsigned int __reserved:1;
	unsigned int sri:1;				/* Status Report Indication */
	unsigned int udhi:1;			/* User Data Header Indicator */
	unsigned int rp:1;				/* Reply Path */
} __packed__;

// 3GPP - sms submit type
struct sms_submit_type
{
	unsigned int mti:2;				/* Message Type Indicator */
	unsigned int rd:1;				/* Reject Duplicates */
	unsigned int vpf:2;				/* Validity Period Format */
	unsigned int srr:1;				/* Status Report Request */
	unsigned int udhi:1;			/* User Data Header Indicator */
	unsigned int rp:1;				/* Reply Path */
} __packed__;

#define SMS_DCS_PARAM_INDI_TYPE				0x03
	#define SMS_DCS_PARAM_INDI_TYPE_VM		0x00
	#define SMS_DCS_PARAM_INDI_TYPE_FM		0x01
	#define SMS_DCS_PARAM_INDI_TYPE_EM		0x02
	#define SMS_DCS_PARAM_INDI_TYPE_OM		0x03
#define SMS_DCS_PARAM_INDI_ACTIVE			0x08

#define SMS_DCS_CMD_MWI_DISCARD				0x0c
#define SMS_DCS_CMD_MWI_STORE1				0x0d
#define SMS_DCS_CMD_MWI_STORE2				0x0e

struct sms_dcs
{
	unsigned int param:4;
	unsigned int cmd:4;
} __packed__;

typedef enum
{
	NO_VPF = 0,
	ENHANCED_VPF_FORMAT,
	RELATIVE_VPF_FORMAT,
	ABSOLUTE_VPF_FORMAT
} tpvpf_field_type;

struct enhanced_tpvp_fields
{
	unsigned char enh_vp[7];
} __packed__;


// 3GPP - User Data Header field -- concatenation type
struct udh_fields
{
	int udh_len;		/* length of UDH : 5 or 6 for concatenation msg*/
	int info_id;		/* Information-Element_Identifier 'A' : 00 : concat type */
	int info_len;		/* length of Information-Element_Identifier 'A' */
	int ref_no;			/* Concatenated short message reference number */
	int msg_no;			/* total number of short messages in the concatenated short message */
	int seq_no;			/* Sequence number of the current short message. start from 1 */
} __packed__;

// logical sms deliver/submit message structure
struct log_sms_message {
	unsigned int cbSMSC;

	const struct addr_fields* pSMSCNumber;

	union {
		struct sms_deliver_type smsDeliverType;
		struct sms_submit_type smsSubmitType;
	} msg_type;

	int SubmitMsgRef;							/* submit message only */

	union {
		struct addr_fields* pOrigAddr;
		struct addr_fields* pDestAddr;
	} addr;

	unsigned int dwPID;
	struct sms_dcs dcs;

	struct timestamp_fields* pTimeStamp;		/* deliver message only */

	union {										/* submit message only */
		unsigned int rel_tpvp;					/* relative TP VP format */
		struct timestamp_fields* abs_tpvp;		/* absolute TP VP format */
		struct enhanced_tpvp_fields* enh_tpvp;	/* enhanced TP VP format */
	} SubmitValidPeriod;

	unsigned int cbUserData;
	const void* pUserData;

	unsigned int cbRawSMSMessage;
	void* pRawSMSMessage;
	struct udh_fields udh;						/* user data header */
};

/* TS 123.040 9.2.3.24 */
typedef enum
{
	DCS_7BIT_CODING = 0,
	DCS_8BIT_CODING,
	DCS_UCS2_CODING
} sms_encoding_type;

typedef enum
{
	INFO_ELEM_CONCAT_MSG_8BIT_REF = 0,			/* Concatenated short messages, 8-bit reference number */
	INFO_ELEM_SPECIAL_MSG_IND = 1,				/* Special SMS Message Indication */
	INFO_ELEM_VALUE_NOT_USED = 3,				/* Value not used to avoid misinterpretation as <LF> character */
	INFO_ELEM_APPL_PORT_ADDR_SCHEME_8BIT = 4,	/* Application port addressing scheme, 8 bit address */
	INFO_ELEM_APPL_PORT_ADDR_SCHEME_16BIT = 5,	/* Application port addressing scheme, 16 bit address */
	INFO_ELEM_SMSC_CONTROL_PARAM = 6,			/* SMSC Control Parameters */
	INFO_ELEM_UDH_SRC_IND = 7,					/* UDH Source Indicator */
	INFO_ELEM_CONCAT_MSG_16BIT_REF = 8,			/* Concatenated short messages, 16-bit reference number */
	INFO_ELEM_WIRELESS_CTL_MSG_PROT = 9,		/* Wireless Control Message Protocol */
	INFO_ELEM_TEXT_FORMATTING = 10,				/* Text Formatting */
	INFO_ELEM_PREDEF_SOUND = 11,				/* Predefined Sound */
	INFO_ELEM_USER_DEF_SOUND = 12,				/* User Defined Sound (iMelody max 128 bytes) */
	INFO_ELEM_PREDEF_ANIMATION = 13,			/* Predefined Animation */
	INFO_ELEM_LARGE_ANIMATION = 14,				/* Large Animation (16*16 times 4 = 32*4 =128 bytes) */
	INFO_ELEM_SMALL_ANIMATION = 15,				/* Small Animation (8*8 times 4 = 8*4 =32 bytes) */
	INFO_ELEM_LARGE_PICTURE = 16,				/* Large Picture (32*32 = 128 bytes) */
	INFO_ELEM_SMALL_PICTURE = 17,				/* Small Picture (16*16 = 32 bytes) */
	INFO_ELEM_VAR_PICTURE = 18,					/* Variable Picture */
	INFO_ELEM_USER_PROMPT_IND = 19,				/* User prompt indicator */
	INFO_ELEM_EXT_OBJ = 20,						/* Extended Object */
	INFO_ELEM_REUSED_EXT_OBJ = 21,				/* Reused Extended Object */
	INFO_ELEM_COMPRESSION_CONTROL = 22,			/* Compression Control */
	INFO_ELEM_OBJ_DISTRIBUTION_IND = 23,		/* Object Distribution Indicator */
	INFO_ELEM_STD_WVG_OBJ = 24,					/* Standard WVG object */
	INFO_ELEM_CHAR_SIZE_WVG_OBJ = 25,			/* Character Size WVG object */
	INFO_ELEM_EXT_OBJ_DATA_REQ_CMD = 26,		/* Extended Object Data Request Command */
	INFO_ELEM_RFC_822_EMAIL_HDR = 32,			/* RFC 822 E-Mail Header */
	INFO_ELEM_HYPERLINK_FMT_ELEM = 33,			/* Hyperlink format element */
	INFO_ELEM_REPLY_ADDR_ELEM = 34,				/* Reply Address Element */
	INFO_ELEM_ENHANCED_VMAIL_INFO = 35,			/* Enhanced Voice Mail Information */
	INFO_ELEM_NAT_LANG_SGL_SFT = 36,			/* National Language Single Shift */
	INFO_ELEM_NAT_LANG_LOCK_SFT = 37			/* National Language Locking Shift */
} sms_udh_info_elem_type;


struct log_sms_message* pduCreateLogSMSMessage(const char* szStrSMSMessage, tpmti_field_type msg_type);

char* pduCreateHumanAddr(const struct addr_fields* pAF, addr_field_type addr_type);
int pduGetAddrLen(const struct addr_fields* pAF);
struct tm* pduCreateLinuxTime(struct timestamp_fields* pTF);
int pduConvSemiOctToStr(const char* halfOcts, int cbHalfOcts, char* achBuf, int cbBuf);

char* pduCreateGSM8BitFromGSM7Bit(const char* pGSM7Bit,int cb8Bit);
char* pduCreateASCIIFromGSM8Bit(const char* pGSM8Bit,int cbGSM8Bit);
int pduConvCharNibbleToInt(char chNibble);

int pduConvDcsToInt(const struct sms_dcs* dcs);
sms_encoding_type parse_msg_coding_type(struct sms_dcs *dcs);
int decode_pdu_msg_body(struct log_sms_message* pLSM, unsigned char* msg_body, sms_encoding_type coding_type);

int createPduData(char *destno, int int_no, int ucs_coding, unsigned char *pEncodeMsg,
				  int cbEncodeMsg, unsigned char *pPduData, struct addr_fields *smsc_addr, int udh_len);
void* pduCreateOctByStr(const char* szStrHex, int* pIntHexLen);


/* ---------------------------------------------*/
/*			CDMA			*/
/* ---------------------------------------------*/
typedef enum
{
	PARAM_ID_TELESERVICE_ID		= 0x00,
	PARAM_ID_SERVICE_CATEGORY	= 0x01,
	PARAM_ID_ORIG_ADDR		= 0x02,
	PARAM_ID_ORIG_SUBADDR		= 0x03,
	PARAM_ID_DEST_ADDR		= 0x04,
	PARAM_ID_DEST_SUBADDR		= 0x05,
	PARAM_ID_BEARER_REPLY		= 0x06,
	PARAM_ID_CAUSE_CODES		= 0x07,
	PARAM_ID_BEARER_DATA		= 0x08,

	PARAM_ID_MAX			= 0xFF,
} cdma_sms_parameter_id_e_type;

typedef enum
{
	CDMA_SMS_TELE_ID_CMT_91		= 0x1000, //4096
	CDMA_SMS_TELE_ID_CPT_95		= 0x1001, //4097
	CDMA_SMS_TELE_ID_CMT_95		= 0x1002, //4098
	CDMA_SMS_TELE_ID_VMN_95		= 0x1003, //4099
	CDMA_SMS_TELE_ID_WAP		= 0x1004, //4100
	CDMA_SMS_TELE_ID_WEMT		= 0x1005, //4101
	CDMA_SMS_TELE_ID_SCPT		= 0x1006, //4102
	CDMA_SMS_TELE_ID_CATPT		= 0x1007, //4103

	CDMA_SMS_TELE_ID_UNKNOWN	= 0xFFFF,
} cdma_sms_teleservice_id_e_type;

typedef enum
{
	CDMA_SMS_DIGIT_MODE_4_BIT	= 0,  // 4bit DTMF
	CDMA_SMS_DIGIT_MODE_8_BIT	= 1,

	CDMA_SMS_DIGIT_MODE_MAX		= 0xFF
} cdma_sms_digit_mode_e_type;

typedef enum
{
	CDMA_SMS_NUMBER_MODE_NONE_DATA_NETWORK	= 0,
	CDMA_SMS_NUMBER_MODE_DATA_NETWORK	= 1,

	CDMA_SMS_NUMBER_MODE_DATA_NETWORK_MAX	= 0xFF
} cdma_sms_number_mode_e_type;

typedef enum
{
	CDMA_SMS_NUMBER_UNKNOWN		= 0,

	/* Number mode '0' */
	CDMA_SMS_NUMBER_INTERNATIONAL	= 1,
	CDMA_SMS_NUMBER_NATIONAL	= 2,
	CDMA_SMS_NUMBER_NETWORK		= 3,
	CDMA_SMS_NUMBER_SUBSCRIBER	= 4,
	CDMA_SMS_NUMBER_ALPHANUMERIC	= 5, /* GSM SMS: addr value is GSM 7-bit chars */
	CDMA_SMS_NUMBER_ABBREVIATED	= 6,
	CDMA_SMS_NUMBER_RESERVED_7	= 7,

	/* Number mode '1' */
	CDMA_SMS_NUMBER_DATA_IP		= 1,
	CDMA_SMS_NUMBER_INTERNET_EMAIL	= 2,

	CDMA_SMS_NUMBER_MAX32		= 0x10000000

} cdma_sms_number_type_e_type;

typedef enum
{
	CDMA_SMS_NUMBER_PLAN_UNKNOWN	= 0,
	CDMA_SMS_NUMBER_PLAN_ISDN	= 1,
	CDMA_SMS_NUMBER_PLAN_DATA	= 3,
	CDMA_SMS_NUMBER_PLAN_TELEX	= 4,
	CDMA_SMS_NUMBER_PLAN_PRIVATE	= 9,

	CDMA_SMS_NUMBER_PLAN_MAX	= 0xFF,
} cdma_sms_number_plan_e_type;

typedef struct cdma_sms_address_s
{
	cdma_sms_digit_mode_e_type	digit_mode;
	cdma_sms_number_mode_e_type	number_mode;
	cdma_sms_number_type_e_type	number_type;
	cdma_sms_number_plan_e_type	number_plan;
	uint8_t				num_fields;
	uint8_t				chari[MAX_ADDR_LEN+1];
	struct addr_fields		log_header;
	uint8_t				log_body[MAX_ADDR_LEN+1];
} __packed__ cdma_sms_address_s_type;

typedef struct cdam_sms_subaddress_s
{
	uint8_t		type;
	uint8_t		odd;
	uint8_t		num_fields;
	uint8_t		chari[MAX_ADDR_LEN+1];
} cdam_sms_subaddress_s_type;


typedef enum
{
	CDMA_BD_SUB_MSG_TYPE_UNKNOWN	= 0,
	CDMA_BD_SUB_MSG_TYPE_DELIVER	= 1,
	CDMA_BD_SUB_MSG_TYPE_SUBMIT	= 2,
	CDMA_BD_SUB_MSG_TYPE_CANCEL	= 3,
	CDMA_BD_SUB_MSG_TYPE_DELI_ACK	= 4,
	CDMA_BD_SUB_MSG_TYPE_USER_ACK	= 5,
	CDMA_BD_SUB_MSG_TYPE_READ_ACK	= 6,
	CDMA_BD_SUB_MSG_TYPE_DELI_REPORT	= 7,
	CDMA_BD_SUB_MSG_TYPE_SUBMIT_REPORT	= 8,

	CDMA_BD_SUB_MSG_TYPE__MAX	= 0xFF,
} cdma_sms_bd_sub_msg_type_e_type;

typedef struct cdma_sms_bd_msg_id_s
{
	cdma_sms_bd_sub_msg_type_e_type	bd_sub_msg_type;
	uint16_t			bd_sub_msg_id;
	uint8_t				bd_sub_header_ind;
} cdma_sms_bd_msg_id_s_type; 

typedef enum
{
	CDMA_BD_SUB_MSG_ENC_8BIT_OCT	= 0x00, // 8-bit
	CDMA_BD_SUB_MSG_ENC_EXT_PM	= 0x01, // varies
	CDMA_BD_SUB_MSG_ENC_7BIT_ACSII	= 0x02, // 7-bit
	CDMA_BD_SUB_MSG_ENC_IA5		= 0x03, // 7-bit
	CDMA_BD_SUB_MSG_ENC_UNICODE	= 0x04, // 16-bit
	CDMA_BD_SUB_MSG_ENC_JIS		= 0x05, // 8 or 16-bit
	CDMA_BD_SUB_MSG_ENC_KOREAN	= 0x06, // 8 or 16-bit
	CDMA_BD_SUB_MSG_ENC_LATIN_HEB	= 0x07, // 8-bit
	CDMA_BD_SUB_MSG_ENC_LATIN	= 0x08, // 8-bit
	CDMA_BD_SUB_MSG_ENC_7BIT_GSM	= 0x09, // 7-bit
	CDMA_BD_SUB_MSG_ENC_EXT_GSM	= 0x0A, //

	CDMA_BD_SUB_MSG_ENC_MAX		= 0xFF,
} cdma_sms_bd_sub_msg_enc_e_type;

#define MAX_USER_DATA_LEN		512		
typedef struct cdma_sms_bd_user_data_s
{
	cdma_sms_bd_sub_msg_enc_e_type	bd_sub_msg_enc;
	uint8_t				bd_sub_msg_type;
	uint8_t				bd_sub_num_fields;
	struct sms_dcs			log_dcs;
	unsigned int			log_cbUserData;
	uint8_t				log_chari[MAX_USER_DATA_LEN];
} cdma_sms_bd_user_data_s_type;

typedef  struct cdma_sms_bd_timestamp_s
{
	uint8_t				year;
	uint8_t				month;
	uint8_t				day;
	uint8_t				hours;
	uint8_t				minutes;
	uint8_t				seconds;
	struct timestamp_fields		log_timestamp;
} __packed__ cdma_sms_bd_timestamp_s_type;

typedef struct cdma_sms_bearer_data_s
{
	cdma_sms_bd_msg_id_s_type	bd_msg_id;
	cdma_sms_bd_user_data_s_type	bd_user_data;
	cdma_sms_bd_timestamp_s_type	bd_timestamp;
} cdma_sms_bearer_data_s_type;

/*
 * CDMA Point-to-Point Message Format
 *
 *  Mobile Originate Message
 *	- Teleservice Identifier	(Mandatory)
 *	- Service Category		(Optional)
 *	- Destination Address		(Mandatory)
 *	- Destination Subaddress	(Optional)
 *	- Bearer Reply Option		(Optional)
 *	- Bearer Data			(Optional)
 *
 *  Mobile Terminate Message
 *	- Teleservice Identifier	(Mandatory)
 *	- Service Category		(Optional)
 *	- Orininating Address		(Mandatory)
 *	- Orininating Subaddress	(Optional)
 *	- Bearer Reply Option		(Optional)
 *	- Bearer Data			(Optional)
*/

typedef struct cdma_sms_message_s
{
	int 					is_mo;
	cdma_sms_teleservice_id_e_type		teleservice_id;
	uint16_t				serv_category;
	cdma_sms_address_s_type			address;
	cdam_sms_subaddress_s_type		subaddress;
	uint8_t					reply_seq;
	cdma_sms_bearer_data_s_type		bearer_data;
} cdma_sms_message_s_type;

#define PDUP(fmt, ...) if (log_db[LOGMASK_PDU].loglevel >= LOG_DEBUG) syslog(LOG_DEBUG, fmt, ##__VA_ARGS__); else

struct log_sms_message* pduCreateCDMALogSMSMessage(const char* szStrSMSMessage, tpmti_field_type msg_type);
int createCDMAPduData(char *destno, int int_no, int ucs_coding, unsigned char *pEncodeMsg,
				  int cbEncodeMsg, unsigned char *pPduData, struct addr_fields *smsc_addr, int udh_len);


#endif
