#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#include "cnsmgr.h"
#include "smsrecv.h"
#include "sierracns.h"
#include "smsdef.h"
#include "gsm7.h"
#include "utf8.h"
#include "coding.h"

BOOL dbSetStr(int nObjId, int iIdx, char* pValue);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void smsrecv_destroy(smsrecv* pParser)
{
	__bypassIfNull(pParser);

	smsrecv_emptyList(pParser);

	__free(pParser);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//BOOL smsrecv_isOk(smsrecv* pParser)
//{
//	struct list_head* pos;
//
//	// get total token count
//	int cToken=0;
//	list_for_each(pos,&pParser->tokenHdr)
//		cToken++;
//
//	// check sequence number
//	int iSeq=0;
//	list_for_each(pos,&pParser->tokenHdr)
//	{
//		smstoken_t* pToken=list_entry(pos,smstoken_t,list);
//
//		if(iSeq==0 && cToken==0)
//		{
//			(pToken->smsEnv.bPckType)==
//
//		if(pToken->smsEnv.bPckSeqNo!=iSeq++)
//			return FALSE;
//
//	}
//}

///////////////////////////////////////////////////////////////////////////////
#if (0)
static int smsConvIntNibbleToChar(int nNibble)
{
	if (nNibble >= 0x0a)
		return nNibble -0x0a + 'A';

	return nNibble + '0';
}
#endif
///////////////////////////////////////////////////////////////////////////////
int smsConvCharToInt(char* src, unsigned int* dst, int cbMsg)
{
    int i;
    unsigned int upper, lower;
    for (i = 0; i < cbMsg; i++, dst++) {
        upper = ((int)(*src) << 8); src++;
        lower = (int)(*src); src++;
        *dst = upper | lower;
    }
	return cbMsg/2;;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
/* convert GSM 7 bit or UCS-2 code to ascii */
static int smsrecv_decodeMsg(char* cpBody, char* pDecodeMsg, int cbBody, unsigned char dcs_type)
{
    unsigned int* ptmpMsg = __alloc(MAX_UNICODE_BUF_SIZE);
    int cbTmpMsg = 0, cbDecodeMsg = 0;
	if(!ptmpMsg) {
        syslog(LOG_ERR, "failed to allocate decode memory %d bytes", MAX_UNICODE_BUF_SIZE);
        return -1;
	}
    syslog(LOG_ERR, "decode %s to Utf-8", (dcs_type == CNSMGR_ENCODE_UCS_2? "UCS2":"GSM7"));
	if (dcs_type == CNSMGR_ENCODE_UCS_2)
	{
		cbTmpMsg = smsConvCharToInt(cpBody, ptmpMsg, cbBody);
    	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
    	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, pDecodeMsg, cbTmpMsg);
   		printMsgBody(pDecodeMsg, cbDecodeMsg);
	} else {
        cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7(cpBody, pDecodeMsg, cbBody);
    }
	__free(ptmpMsg);
	return cbDecodeMsg;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* pMsg : max len MAX_UTF8_BUF_SIZE (1024) */
BOOL smsrecv_getMsgBody(smsrecv* pParser, void* pMsg, int cbMsg)
{
	void *pPtr = pMsg;
	int cbRemaining = cbMsg;
	char *cpBody;
	char *pDecodeMsg = NULL;
	int cbDecodeMsg;
	int bDcsDataType = 0;

	struct list_head* pos;

	list_for_each(pos, &pParser->tokenHdr)
	{
		smstoken_t* pToken = list_entry(pos, smstoken_t, list);

		// get sms env
		smsenvelope* pSmsEnv = &pToken->smsEnv;

		// get sms header
		smsheader* pSmsHdr = NULL;
		if (pSmsEnv->bPckType == smsenvelope_packet_type_first || pSmsEnv->bPckSeqNo == 0)
		{
			pSmsHdr = __getNextPtr(pSmsEnv);
			bDcsDataType = pSmsHdr->dcsInfo.bDcsDataType;
			syslog(LOG_ERR, "RX SMS encode type : %s", (bDcsDataType == CNSMGR_ENCODE_UCS_2? "UCS2":"GSM7"));
		}

		// get sms header length
		int cbSmsHdr = pSmsHdr ? sizeof(smsheader) : 0;

		// get sms body
		void* pBody = pSmsHdr ? __getNextPtr(pSmsHdr) : __getNextPtr(pSmsEnv);
		int cbBody = pSmsEnv->bPayloadLen - cbSmsHdr;

		// error if lack of space
		if (cbRemaining < cbBody)
		{
			__free(pDecodeMsg);
			return FALSE;
		}

		cpBody=(char *)pBody;
		pDecodeMsg = __alloc(MAX_UNICODE_BUF_SIZE);
		if (pDecodeMsg == NULL)
		{
			syslog(LOG_ERR, "decode memory allocation failed for %d bytes", MAX_UNICODE_BUF_SIZE);
			return FALSE;
		}

		printMsgBody(cpBody, cbBody);
		cbDecodeMsg = smsrecv_decodeMsg(cpBody, pDecodeMsg, cbBody, bDcsDataType);
		if (cbDecodeMsg < 0) {
			syslog(LOG_ERR, "decode memory allocation failed!");
			__free(pDecodeMsg);
			return FALSE;
		}
		if (pSmsEnv->bPckType == smsenvelope_packet_type_first || pSmsEnv->bPckSeqNo == 0) {
    		strcpy(pPtr, (bDcsDataType == CNSMGR_ENCODE_UCS_2)?"UCS2:":"GSM7:");
    		pPtr += 5;
   		}
		memcpy(pPtr, pDecodeMsg, cbDecodeMsg);
		pDecodeMsg = NULL;

		// decrease remaining size
		cbRemaining -= cbBody;
		// increase target
		pPtr = __getOffset(pPtr, cbDecodeMsg);
	}

	__free(pDecodeMsg);
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int smsrecv_getTotalMsgLen(smsrecv* pParser)
{
	struct list_head* pos;

	int cTotalLen = 0;

	list_for_each(pos, &pParser->tokenHdr)
	{
		smstoken_t* pToken = list_entry(pos, smstoken_t, list);

		cTotalLen += pToken->smsEnv.bPayloadLen;
	}

	return cTotalLen -sizeof(smsheader);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smsrecv_emptyList(smsrecv* pParser)
{
	while (!list_empty(&pParser->tokenHdr))
	{
		struct list_head* pNext = pParser->tokenHdr.next;
		smstoken_t* pToken = list_entry(pNext, smstoken_t, list);

		// remove the next
		list_del(pNext);

		// free token
		__free(pToken);
	}

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
smsenvelope* smsrecv_getFirstLastSmsEnv(smsrecv* pParser, BOOL fFirst)
{
	if (list_empty(&pParser->tokenHdr))
		return NULL;

	// get last token
	smstoken_t* pToken;

	if (fFirst)
		pToken = list_entry(pParser->tokenHdr.next, smstoken_t, list);
	else
		pToken = list_entry(pParser->tokenHdr.prev, smstoken_t, list);

	// get last envolope
	return &pToken->smsEnv;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smsrecv_getMsgId(smsrecv* pParser, int* pMsgId)
{
	// get last envolope
	smsenvelope* pSmsEnv = smsrecv_getFirstLastSmsEnv(pParser, TRUE);

	if (!pSmsEnv)
		return FALSE;

	*pMsgId = htons(pSmsEnv->wMsgId);
	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smsrecv_getTimeStamp(smsrecv* pParser, char* pBuf, int cbBuf)
{
	if (!cbBuf)
		return FALSE;

	// get last envolope
	smsenvelope* pSmsEnv = smsrecv_getFirstLastSmsEnv(pParser, TRUE);
	if (!pSmsEnv)
		return FALSE;

	// get sms header
	smsheader* pSmsHdr = (smsheader*)__getNextPtr(pSmsEnv);
	// get time stamp
	smstimestamp* pTmStamp = (smstimestamp*)pSmsHdr->achTimestamp;

	// get semi-oct without sign
	WORD wTimeZoneOct = ntohs(pTmStamp->wTimeZone);
	BOOL fMinus = (wTimeZoneOct & 0x0800) != 0;
	wTimeZoneOct &= ~0x0800;

	// apply the sign
	int nTimeZone = __getSemiOct(wTimeZoneOct);
	if (fMinus)
		nTimeZone = -nTimeZone;


	// get year
	int nYear = __getSemiOct(ntohs(pTmStamp->wYear)) + 2000;
	int nMonth = __getSemiOct(ntohs(pTmStamp->wMonth));
	int nDay = __getSemiOct(ntohs(pTmStamp->wDay));
	int nHour = __getSemiOct(ntohs(pTmStamp->wHour));
	int nMin = __getSemiOct(ntohs(pTmStamp->wMinute));
	int nSec = __getSemiOct(ntohs(pTmStamp->wSecond));

	// build datatime
	int cbLen = snprintf(pBuf, cbBuf, "%04d-%02d-%02d %02d:%02d:%02d", nYear, nMonth, nDay, nHour, nMin, nSec);

	pBuf[cbLen] = 0;

	return cbLen > 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int DecToStr(unsigned char *in_str, unsigned char *out_str, int str_len)
{
	int i;
	for(i=0; i < str_len; i++)
	{
		if(in_str[i] <= 9)
			out_str[i] = in_str[i]+0x30;
		else
			return 0;
	}
	return 1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smsrecv_get_dst_phone_no(smsrecv* pParser, char* pBuf, int cbBuf, sms_dest_addr_type type)
{
	char tbuf[32] = {0, };
	if (!cbBuf)
		return FALSE;

	// get last envolope
	smsenvelope* pSmsEnv = smsrecv_getFirstLastSmsEnv(pParser, TRUE);
	if (!pSmsEnv)
		return FALSE;

	// get sms header
	smsheader* pSmsHdr = (smsheader*)__getNextPtr(pSmsEnv);

	if (type == sms_destination_source_phone_number) {
		DecToStr(pSmsHdr->srcDstAddrInfo.achAddrPhoneNo, (unsigned char *)tbuf,pSmsHdr->srcDstAddrInfo.bLenOfAddrPhoneNo);
		if (pSmsHdr->servCenAddrInfo.bAddrType == smsheader_addr_type_international && tbuf[0] != '+') {
			syslog(LOG_ERR, "sync address type and source address by adding +");
			sprintf(pBuf, "+%s", tbuf);
		} else {
			strcpy(pBuf, tbuf);
		}
	} else {
		DecToStr(pSmsHdr->servCenAddrInfo.achAddrPhoneNo, (unsigned char *)pBuf,pSmsHdr->servCenAddrInfo.bLenOfAddrPhoneNo);
	}
	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SECS_TO_2000_FM_1970	3600 * 24 * 365 * 30
BOOL smsrecv_get_dst_recv_time(smsrecv* pParser, char* pBuf, int cbBuf)
{
	struct tm *now = NULL;
	time_t tl;
	int cbLen;

	if (!cbBuf)
		return FALSE;

	tl = time(NULL);

	now = localtime(&tl);

	// build datatime
	if(now->tm_year >= 100) now->tm_year -= 100;
	/* if elapsed time is greater than 30 year ( fm 1970), it's 2xxx. */
	if (tl > SECS_TO_2000_FM_1970)
		cbLen = snprintf(pBuf, cbBuf, "20%02d-%02d-%02d %02d:%02d:%02d",
											(now->tm_year), now->tm_mon, now->tm_mday,
											now->tm_hour, now->tm_min, now->tm_sec);
	else
		cbLen = snprintf(pBuf, cbBuf, "19%02d-%02d-%02d %02d:%02d:%02d",
											(now->tm_year), now->tm_mon, now->tm_mday,
											now->tm_hour, now->tm_min, now->tm_sec);

	pBuf[cbLen] = 0;

	return cbLen > 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smsrecv_isFirstToken(smsrecv* pParser, smsenvelope* pSmsEnv)
{
	return (pSmsEnv->bPckType == smsenvelope_packet_type_first || pSmsEnv->bPckSeqNo == 0);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smsrecv_isCompleted(smsrecv* pParser)
{
	// get last envolope
	smsenvelope* pSmsEnv = smsrecv_getFirstLastSmsEnv(pParser, FALSE);

	if (!pSmsEnv)
		return FALSE;

	return (pSmsEnv->bPckType == smsenvelope_packet_type_last || pSmsEnv->bRemainingSegs == 0);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smsrecv_addSmsToken(smsrecv* pParser, smsenvelope* pSmsEnv)
{
	smstoken_t* pToken = NULL;

	// allocate token
	int cbTotal = sizeof(smstoken_t) + pSmsEnv->bPayloadLen;
	__goToErrorIfFalse(pToken = __alloc(cbTotal+1));

	// copy sms envolope and payload
	memcpy(&pToken->smsEnv, pSmsEnv, pSmsEnv->bPayloadLen + sizeof(*pSmsEnv));

	// add the token to the list
	list_add_tail(&pToken->list, &pParser->tokenHdr);
	return TRUE;

error:
	__free(pToken);
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
smsrecv* smsrecv_create()
{
	smsrecv* pParser;

	// create parser
	__goToErrorIfFalse(pParser = __alloc(sizeof(smsrecv)));

	// init token header
	INIT_LIST_HEAD(&pParser->tokenHdr);
	return pParser;

error:
	smsrecv_destroy(pParser);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
const char smstools_spool_inbox_path[]="/var/spool/sms/incoming";
int polling_rx_sms_event(void)
{
	int result = -1;
	DIR *dir, *first_dir;
	struct dirent *dir_rent;
	FILE* sms_file;
	unsigned char str[100] = {0,};

	if((dir = opendir((const char *)&smstools_spool_inbox_path))== NULL) {
		return -1;
	}
	first_dir = dir;
	//SYSLOG_ERR("open dirp = %p", first_dir);
	while((dir_rent = readdir(dir)) != NULL) {
		if(strcmp(dir_rent->d_name, ".") == 0 || strcmp(dir_rent->d_name, "..") == 0) {
			continue;
		} else {
			sprintf((char*)str, "%s/%s",smstools_spool_inbox_path,dir_rent->d_name);
			sms_file = (FILE *)open((char*)str, O_RDONLY);
			if(sms_file > 0) {
				close((int)sms_file);
				result = 0;
				break;
			}
		}
	}
	//SYSLOG_ERR("close dirp = %p, curr dirp = %p", first_dir, dir);
	/* Should close dirp returned by opendir because this dirp changes after calling readdir */
	closedir(first_dir);
	if (!result) {
		dbSetStr(SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS, 0, "Total:xxx read:x unread:1 sent:x unsent:x");
		dbSetStr(SIERRACNS_PACKET_OBJ_REPORT_SMS_RECEIVED_MESSAGE_STATUS, 1, "Total:xxx read:x unread:1 sent:x unsent:x");
	}
	return result;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
