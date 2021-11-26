#ifndef __SMSDEF_H__
#define __SMSDEF_H__

#include "base.h"

//#define SMS_DEBUG

#ifdef SMS_DEBUG
#define SET_SMS_LOG_MASK_TO_DEBUG_LEVEL     setlogmask(LOG_UPTO(LOG_DEBUG));
#define SET_SMS_LOG_MASK_TO_ERROR_LEVEL     setlogmask(LOG_UPTO(LOG_ERR));
#else
#define SET_SMS_LOG_MASK_TO_DEBUG_LEVEL
#define SET_SMS_LOG_MASK_TO_ERROR_LEVEL
#endif	

#define MAX_UTF8_BUF_SIZE			1024
#define MAX_UNICODE_BUF_SIZE		MAX_UTF8_BUF_SIZE * sizeof(unsigned int)
#define UINT_SIZE					sizeof(unsigned int)

#define BSIZE_16		        16
#define BSIZE_32		        32
#define BSIZE_64		        64
#define BSIZE_128		        128
#define BSIZE_256		        256
#define BSIZE_512		        512
#define BSIZE_1024		        1024

typedef struct
{
	WORD wYear;
	WORD wMonth;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wTimeZone;
} __packedStruct smstimestamp;

typedef struct
{
	unsigned short wMsgId;
	unsigned char bPckSeqNo;
	unsigned char bPckType;
	unsigned char bRemainingSegs;
	unsigned char bPayloadLen;
} __packedStruct smsenvelope;

// smsaddrinfo
typedef struct
{
	unsigned char bAddrType;
	unsigned char bAddrNoPlan;
	unsigned char bLenOfAddrPhoneNo;
	unsigned char achAddrPhoneNo[20];
} __packedStruct smsaddrinfo;


// smsdcsinfo
typedef struct
{
	unsigned char bDcsDataType;
	unsigned char bDcsClass;
	unsigned char bDcsCompression;
} __packedStruct smsdcsinfo;

// smsprotoinfo
typedef struct
{
	unsigned char bProtoType;
	unsigned char bProtoId;
} __packedStruct smsprotoinfo;


// smsheader
typedef struct
{
	union
	{
		struct
		{
			unsigned short wMsgBodySize;
		} __packedStruct;

		struct
		{
			unsigned char bResCode;
			unsigned char bRecStat;
		} __packedStruct;
	};

	smsaddrinfo servCenAddrInfo;
	smsaddrinfo srcDstAddrInfo;

	unsigned char __reservedZero1;

	smsprotoinfo protoInfo;
	smsdcsinfo dcsInfo;

	union
	{
		struct
		{
			unsigned char __reservedZero2;
			unsigned char __reservedZero3;
			unsigned char __reservedZero4;

			unsigned char __reservedZero5;

			unsigned char __ignoreField2[14];

			unsigned char achValidityPeriod[16];

			unsigned char __reservedZero6;
			unsigned char __reservedZero7;

		} __packedStruct;

		struct
		{
			unsigned char bMsgWaiting;
			unsigned char bMmiMsgWaitingIndi;
			unsigned char bMmiMsgIndiType;

			unsigned char __ignoreField1;

			unsigned char achTimestamp[14];

			unsigned char __ignoreField3[16];
			unsigned char __ignoreField4;
			unsigned char __ignoreField5;

		} __packedStruct;
	};

} __packedStruct smsheader;

// smsconfigurationdetail
typedef struct
{
	unsigned char bDefDestAddrPresent;
	smsaddrinfo defDestAddrInfo;

	unsigned char bServCenAddrPresent;
	smsaddrinfo servCenAddrInfo;

	unsigned char bDefProtoIdPresent;
	smsprotoinfo protoInfo;

	unsigned char bDefDcsPresent;
	smsdcsinfo dcsInfo;

	unsigned char bDefValidityPeriodPresent;
	unsigned char bDefValidityPeriod;

	unsigned char bSmsRoutingOpt;

} __packedStruct smsconfigurationdetail;

enum
{
	smsenvelope_packet_type_first = 3,
	smsenvelope_packet_type_intermediate = 1,
	smsenvelope_packet_type_last = 2
};

enum
{
	smsheader_addr_type_unknown = 0,
	smsheader_addr_type_international,
	smsheader_addr_type_national,
	smsheader_addr_type_networkspecific,
	smsheader_addr_type_subscriber,
	smsheader_addr_type_alphanumeric,
	smsheader_addr_type_abbreviated
};

enum
{
	smsheader_addr_numbering_unknown = 0,
	smsheader_addr_numbering_isdntelephone,
	smsheader_addr_numbering_data = 3,
	smsheader_addr_numbering_telex,
	smsheader_addr_numbering_national = 8,
	smsheader_addr_numbering_private,
	smsheader_addr_numbering_ermes
};

enum
{
	smsheader_prot_type_application = 0,
	smsheader_prot_type_telematic = 0x20,
	smsheader_prot_type_sim = 0x40,
	smsheader_prot_type_me = 0x60,
	smsheader_prot_type_sc = 0xc0,
	smsheader_prot_type_raw = 0xc1
};

enum
{
	smsheader_dcs_data_type_default = 0,
	smsheader_dcs_data_type_8bit,
	smsheader_dcs_data_type_ucs2,
	smsheader_dcs_data_type_unknown
};

enum
{
	smsheader_dcs_class_0 = 0,
	smsheader_dcs_class_1,
	smsheader_dcs_class_2,
	smsheader_dcs_class_3,
	smsheader_dcs_class_notgiven
};

enum
{
	smsheader_dcs_compression_uncompressed = 0,
	smsheader_dcs_compression_compressed
};

typedef enum
{
	sms_destination_source_phone_number = 0,
	sms_destination_service_center_phone_number
} sms_dest_addr_type;

int cnsConvPhoneNumber(const char* pDial, unsigned char* pDst, int cbDst, BOOL* pInternational);
void cnsConvSmscAddressField(const char* pAddress, int cbAddr, unsigned char* pDst);
void printMsgBody(char* msg, int len);
void printMsgBodyInt(int* msg, int len);

#endif
