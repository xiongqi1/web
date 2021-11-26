#ifndef __SIERRAHIP_H__
#define __SIERRAHIP_H__

#include <termios.h>
#include <unistd.h>

#include "base.h"
#include "binqueue.h"

typedef struct
{
	unsigned char bFramingHdr;
	unsigned short wPayLoadLen;
	unsigned char bMsgId;
	unsigned char bParam;
} __packedStruct sierrahip_hdr;

typedef struct
{
	unsigned char bFramingTail;
} __packedStruct sierrahip_tail;

#define SIERRAHIP_MAX_PAYLOAD_LEN			1024
#define SIERRAHIP_MAX_HIP_LEN					(sizeof(sierrahip_hdr)+sizeof(sierrahip_tail)+SIERRAHIP_MAX_PAYLOAD_LEN)
#define	SIERRAHIP_MIN_HIP_LEN					(sizeof(sierrahip_hdr)+sizeof(sierrahip_tail))
#define SIERRAHIP_READ_BUF_LEN				1024
#define SIERRAHIP_WRITE_BUF_LEN				(SIERRAHIP_MAX_PAYLOAD_LEN*10)


#define SIERRAHIP_PACKET_STX					0x7e
#define SIERRAHIP_PACKET_ESC					0x7d
#define SIERRAHIP_PACKET_ESC_STX			0x5e
#define SIERRAHIP_PACKET_ESC_ESC			0x5d

typedef enum
{
	sierrahip_parse_stage_none = 0,
	sierrahip_parse_stage_stx,
	sierrahip_parse_stage_body,
	sierrahip_parse_stage_esc,
} sierrahip_parse_stage;

typedef struct
{
	// hip read buffers
	char* pHipReadBuf;
	char* pHipReadBufPtr;
	char* pHipReadTmpBuf;

	// hip write buffer
	char* pHipWriteBuf;

	int hHipDev;

	BOOL fTermioOld;
	struct termios termiosOld;

	void* lpfnHipHandler;
	void* pRef;

	sierrahip_parse_stage parseStage;

	binqueue* pWriteQ;

} sierrahip;

typedef void (SIERRAHIP_HIPHANDLER)(sierrahip* pHip, unsigned char bMsgId, unsigned char bParam, void* pPayLoad, unsigned short cbPayLoad, void* pRef);

sierrahip* sierrahip_create(void);
void sierrahip_destroy(sierrahip* pHip);

void sierrahip_setHipHandler(sierrahip* pHip, SIERRAHIP_HIPHANDLER* lpfnHipHandler, void* pRef);
BOOL sierrahip_writeHip(sierrahip* pHip, unsigned char bMsgId, unsigned char bParam, void* pPayLoad, int cbPayLoad);

int sierrahip_getHandle(sierrahip* pHip);
BOOL sierrahip_isWriteBufEmpty(sierrahip* pHip);
void sierrahip_onWrite(sierrahip* pHip);
int sierrahip_onRead(sierrahip* pHip);

void sierrahip_closeHipDev(sierrahip* pHip);
BOOL sierrahip_openHipDev(sierrahip* pHip, const char* lpszHipDev);


#endif


