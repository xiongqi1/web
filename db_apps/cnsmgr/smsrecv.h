#ifndef __SMSPARSER_H__
#define __SMSPARSER_H__

#include "base.h"
#include "list.h"

#include "smsdef.h"

typedef struct
{
	struct list_head list;

	smsenvelope smsEnv;
} smstoken_t;

typedef struct
{
	struct list_head tokenHdr;
} smsrecv;


BOOL smsrecv_getTimeStamp(smsrecv* pParser, char* pBuf, int cbBuf);
BOOL smsrecv_getMsgId(smsrecv* pParser, int* pMsgId);
BOOL smsrecv_emptyList(smsrecv* pParser);
int smsrecv_getTotalMsgLen(smsrecv* pParser);
BOOL smsrecv_getMsgBody(smsrecv* pParser, void* pMsg, int cbMsg);
void smsrecv_destroy(smsrecv* pParser);
BOOL smsrecv_isFirstToken(smsrecv* pParser, smsenvelope* pSmsEnv);
BOOL smsrecv_isCompleted(smsrecv* pParser);
BOOL smsrecv_addSmsToken(smsrecv* pParser, smsenvelope* pSmsEnv);
smsrecv* smsrecv_create();
BOOL smsrecv_get_dst_phone_no(smsrecv* pParser, char* pBuf, int cbBuf, sms_dest_addr_type type);
BOOL smsrecv_get_dst_recv_time(smsrecv* pParser, char* pBuf, int cbBuf);
int polling_rx_sms_event(void);


#endif
