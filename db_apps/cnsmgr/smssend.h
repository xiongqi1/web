#ifndef __SMSTRANS_H__
#define __SMSTRANS_H__

#include "base.h"
#include "list.h"
#include "smsdef.h"
#include "sierracns.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct 
{
	struct list_head listToken;
	smsconfigurationdetail smsConfig;
} smssend;

///////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	struct list_head list;

	char* pDstNumber;

	char* pMsg;
	int cbMsg;

	int iSeg;
	int cTotalSeg;

	int EncodeType;
	clock_t clkPut;
} smstoken;

///////////////////////////////////////////////////////////////////////////////////////////////////
void smssend_destroy(smssend* pSend);
smssend* smssend_create(void);
void smssend_setSmsConfig(smssend* pSend, smsconfigurationdetail* pSmsConfig);
int smssend_getMsgPacket(smssend* pSend,void *pDst,int cbDst);
void smssend_wasteMsg(smssend* pSend);
BOOL smssend_addMsg(smssend* pSend, const char* szDstNumber, const char* szMsg, int encode_type, int cbMsg);
int smssend_encodeMsg(char* pMsg, char* pEncodeMsg, int cbMsg, int encode_type);
BOOL smssend_isDoneMsg(smssend* pSend);
BOOL smssend_isEmpty(smssend* pSend);

#endif
