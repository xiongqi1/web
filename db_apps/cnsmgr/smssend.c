#include <stdlib.h>
#include "smssend.h"
#include <arpa/inet.h>
#include "syslog.h"
#include "cnsmgr.h"
#include "smsdef.h"
#include "gsm7.h"
#include "utf8.h"
#include "coding.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
void smssend_destroy(smssend* pSend)
{
	__bypassIfNull(pSend);
	__free(pSend);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void smssend_freeToken(smstoken* pToken)
{

	__bypassIfNull(pToken->pMsg);
	__free(pToken->pMsg);
	__bypassIfNull(pToken->pDstNumber);
	__free(pToken->pDstNumber);
	__bypassIfNull(pToken);
	__free(pToken);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
smstoken* smssend_allocToken(int cbDstNumber,int cbMsg)
{
	smstoken* pToken;

	__goToErrorIfFalse(pToken=__allocObj(smstoken));
	__goToErrorIfFalse(pToken->pDstNumber=__alloc(cbDstNumber+1));
	__goToErrorIfFalse(pToken->pMsg=__alloc(cbMsg+1));
	return pToken;

error:
	smssend_freeToken(pToken);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void smssend_wasteMsg(smssend* pSend)
{
	if(list_empty(&pSend->listToken))
		return;

	smstoken* pToken;

	if(__isAssigned(pToken=list_entry(pSend->listToken.next,smstoken,list)))
	{
		list_del(&pToken->list);
		smssend_freeToken(pToken);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////
smstoken* smssend_getToken(smssend* pSend)
{
	smstoken* pToken;

	__goToErrorIfFalse(!list_empty(&pSend->listToken));
	pToken=list_entry(pSend->listToken.next,smstoken,list);

	return pToken;
error:
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void smssend_setSmsConfig(smssend* pSend, smsconfigurationdetail* pSmsConfig)
{
	memcpy(&pSend->smsConfig,pSmsConfig,sizeof(pSend->smsConfig));
}
///////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smssend_isDoneMsg(smssend* pSend)
{
	smstoken* pToken;

	__goToErrorIfFalse(pToken=smssend_getToken(pSend));

	syslog(LOG_DEBUG,"cTotalSeg %d, iSeg %d\n", pToken->cTotalSeg, pToken->iSeg);
	return pToken->cTotalSeg && (pToken->iSeg >= pToken->cTotalSeg);

error:
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void printSmsMsgPacket(smsenvelope *se, smsheader *sh, char *body)
{
	int i;
	char buf[256];

	syslog(LOG_DEBUG,"==============================================================\n");
	syslog(LOG_DEBUG,"          SMS Tx Message packet contents\n");
	syslog(LOG_DEBUG,"==============================================================\n");
	syslog(LOG_DEBUG,"msg_id                  %d\n", se->wMsgId);
	syslog(LOG_DEBUG,"seq_num                 %d\n", se->bPckSeqNo);
	syslog(LOG_DEBUG,"packet_type             %d\n", se->bPckType);
	syslog(LOG_DEBUG,"remain_sms_seg          %d\n", se->bRemainingSegs);
	syslog(LOG_DEBUG,"payload_len             %d\n", se->bPayloadLen);
	syslog(LOG_DEBUG,"\n");
	if (sh) {
	syslog(LOG_DEBUG,"total_msg_body_size     %d\n", ntohs(sh->wMsgBodySize));
	syslog(LOG_DEBUG,"sc addr_type            %d\n", sh->servCenAddrInfo.bAddrType);
	syslog(LOG_DEBUG,"sc bAddrNoPlan          %d\n", sh->servCenAddrInfo.bAddrNoPlan);
	syslog(LOG_DEBUG,"sc bLenOfAddrPhoneNo    %d\n", sh->servCenAddrInfo.bLenOfAddrPhoneNo);
	(void)memset(buf, 0x00, 256);
	sprintf(buf,"sc achAddrPhoneNo      ");
	for (i=0; i<20;i++) sprintf(buf,"%s %x", buf, sh->servCenAddrInfo.achAddrPhoneNo[i]); strcat (buf,"\n");syslog(LOG_DEBUG,"%s", buf);
	syslog(LOG_DEBUG,"dst addr_type           %d\n", sh->srcDstAddrInfo.bAddrType);
	syslog(LOG_DEBUG,"dst bAddrNoPlan         %d\n", sh->srcDstAddrInfo.bAddrNoPlan);
	syslog(LOG_DEBUG,"dst bLenOfAddrPhoneNo   %d\n", sh->srcDstAddrInfo.bLenOfAddrPhoneNo);
	(void)memset(buf, 0x00, 256);
	sprintf(buf,"dst achAddrPhoneNo     ");
	for (i=0; i<20;i++) sprintf(buf,"%s %x", buf, sh->srcDstAddrInfo.achAddrPhoneNo[i]); strcat(buf,"\n");syslog(LOG_DEBUG,"%s", buf);
	syslog(LOG_DEBUG,"bProtoType              %d\n", sh->protoInfo.bProtoType);
	syslog(LOG_DEBUG,"bProtoId                %d\n", sh->protoInfo.bProtoId);
	syslog(LOG_DEBUG,"bDcsDataType            %d\n", sh->dcsInfo.bDcsDataType);
	syslog(LOG_DEBUG,"bDcsClass               %d\n", sh->dcsInfo.bDcsClass);
	syslog(LOG_DEBUG,"bDcsCompression         %d\n", sh->dcsInfo.bDcsCompression);
	(void)memset(buf, 0x00, 256);
	sprintf(buf,"msg_timestamp          ");
	for (i=0; i<14;i++) sprintf(buf,"%s %x", buf, sh->achTimestamp[i]); strcat(buf,"\n");syslog(LOG_DEBUG,"%s", buf);
	(void)memset(buf, 0x00, 256);
	sprintf(buf,"validity_period        ");
	for (i=0; i<16;i++) sprintf(buf,"%s %x", buf, sh->achValidityPeriod[i]); strcat(buf,"\n");syslog(LOG_DEBUG,"%s", buf);
	}
	syslog(LOG_DEBUG,"pBody                   %s\n", body);
	(void)memset(buf, 0x00, 256);
	for (i=0; i<20;i++) sprintf(buf,"%s 0x%02x", buf, body[i]); strcat (buf,"\n");syslog(LOG_DEBUG,"%s", buf);
	syslog(LOG_DEBUG,"==============================================================\n");
}

int smssend_getMsgPacket(smssend* pSend,void *pDst,int cbDst)
{
	smstoken* pToken;

	__goToErrorIfFalse(pToken=smssend_getToken(pSend));

	BOOL fFirstPck=__isFalse(pToken->cTotalSeg);

	// get available body length
	// 140 bytes for first message & 240 bytes for second message
	int cbSMsgLen = SIERRACNS_PACKET_PARAMMAXLEN - sizeof(smsenvelope);

	// get sms envelope pointer
	smsenvelope* pSmsEnv = (smsenvelope*)pDst;

	// get total segments
	int cbMsg=pToken->cbMsg;
	static int msg_id = 0;	/* save msg ID for concatenated msg */

	if(fFirstPck)
	{
		//////////////////////////////
		// build ## first envolope  //
		//////////////////////////////
		syslog(LOG_DEBUG,"building first packet\n");
		int cSeg;
		/* message ID between 0 to 255 */
		pSmsEnv->wMsgId = rand() % 255;
		msg_id = pSmsEnv->wMsgId;
		if (cbMsg <= SIERRACNS_PACKET_MAX_PAYLOAD_LEN_1ST_PKT)
		{
			cSeg = 1;
			// set length
			pSmsEnv->bPayloadLen = sizeof(smsheader) + cbMsg;
		}
		else
		{
			cSeg = 2 + (cbMsg - 1 - SIERRACNS_PACKET_MAX_PAYLOAD_LEN_1ST_PKT)/cbSMsgLen;
			// set length
			pSmsEnv->bPayloadLen = sizeof(smsheader) + SIERRACNS_PACKET_MAX_PAYLOAD_LEN_1ST_PKT;
		}

		// set packet segment index
		pSmsEnv->bPckSeqNo = 0;
		pSmsEnv->bPckType = smsenvelope_packet_type_first;
		// set remaining segment
		pSmsEnv->bRemainingSegs = cSeg - 1;
		syslog(LOG_DEBUG,"remaining segments = %d\n", pSmsEnv->bRemainingSegs);

		////////////////////////////
		// build ## sms header ## //
		////////////////////////////
		smsheader* pSmsHdr = (smsheader*)__getNextPtr(pSmsEnv);
		__zeroObj(pSmsHdr);

		// set destination information
		smsaddrinfo* pDstAddrInfo = &pSmsHdr->srcDstAddrInfo;
		BOOL fInternational;
		int cbLen = cnsConvPhoneNumber(pToken->pDstNumber, pDstAddrInfo->achAddrPhoneNo, sizeof(pDstAddrInfo->achAddrPhoneNo), &fInternational);

		pDstAddrInfo->bAddrType = fInternational ? smsheader_addr_type_international : smsheader_addr_type_national;
		pDstAddrInfo->bAddrNoPlan = smsheader_addr_numbering_isdntelephone;
		pDstAddrInfo->bLenOfAddrPhoneNo = (BYTE)cbLen;

		// set total body count
		pSmsHdr->wMsgBodySize = htons(cbMsg);

		// get sms configuration
		smsconfigurationdetail* pSmsCfg = &pSend->smsConfig;

		// set destination service information
		if (pSmsCfg->bServCenAddrPresent)
			pSmsHdr->servCenAddrInfo = pSmsCfg->servCenAddrInfo;

		// set proto information
		if (pSmsCfg->bDefProtoIdPresent)
			pSmsHdr->protoInfo = pSmsCfg->protoInfo;

		// set DCS information
		if (pSmsCfg->bDefDcsPresent)
			pSmsHdr->dcsInfo = pSmsCfg->dcsInfo;

		/* override DCS class to send SMS always. Some SIM card has different DCS class pre-setting which
		 * prevent SMS display/store/notification in remote device.
		 */
		pSmsHdr->dcsInfo.bDcsClass = 0x01;

		/* set DCS data type field */
		pSmsHdr->dcsInfo.bDcsDataType = pToken->EncodeType;

		// set validity period
		if (pSmsCfg->bDefValidityPeriod)
		{
			pSmsHdr->achValidityPeriod[0] = 0x02;
			pSmsHdr->achValidityPeriod[1] = pSmsCfg->bDefValidityPeriod;
		}

		void* pBody = __getNextPtr(pSmsHdr);
		int cbBody = pSmsEnv->bPayloadLen -sizeof(smsheader);

		memcpy(pBody, pToken->pMsg, cbBody);

		// set message segment information
		pToken->iSeg=1;
		pToken->cTotalSeg=cSeg;

		printSmsMsgPacket(pSmsEnv, pSmsHdr, pBody);

		//syslog(LOG_DEBUG,"return %d = %d + %d", sizeof(*pSmsEnv) + pSmsEnv->bPayloadLen,
		//		sizeof(*pSmsEnv),pSmsEnv->bPayloadLen);
		return sizeof(*pSmsEnv) + pSmsEnv->bPayloadLen;
	}

	////////////////////////
	// process next bodys //
	////////////////////////
	__goToErrorIfFalse(pToken->iSeg < pToken->cTotalSeg);

	// get segment number
	int iSeg=++pToken->iSeg;
	// get message body pointer
	char* pCurMsg = pToken->pMsg + SIERRACNS_PACKET_MAX_PAYLOAD_LEN_1ST_PKT + ((iSeg-2) * cbSMsgLen);
	// get remaining bytes
	int cbRemaining = cbMsg - (pCurMsg - pToken->pMsg);
	// get remaining segment
	int cRemSeg = pToken->cTotalSeg - iSeg;

	syslog(LOG_DEBUG,"seg idx = %d, remain seg %d, remain bytes %d \n", iSeg - 1, cRemSeg, cbRemaining);

	//////////////////////////
	// build ## sms body ## //
	//////////////////////////

	void* pBody = __getNextPtr(pSmsEnv);
	int cbBody = __min(cbRemaining, cbSMsgLen);

	memcpy(pBody, pCurMsg, cbBody);

	//////////////////////////////
	// build ## sms envolpoe ## //
	//////////////////////////////

	pSmsEnv->wMsgId = msg_id;

	// set packet segment index
	pSmsEnv->bPckSeqNo = (BYTE)(iSeg - 1);

	// set packet type
	if (!cRemSeg)
		pSmsEnv->bPckType = smsenvelope_packet_type_last;
	else
		pSmsEnv->bPckType = smsenvelope_packet_type_intermediate;

	// set remaining segment
	pSmsEnv->bRemainingSegs = (BYTE)cRemSeg;
	// set length
	pSmsEnv->bPayloadLen = cbBody;
	printSmsMsgPacket(pSmsEnv, NULL, pBody);
	//syslog(LOG_DEBUG,"return %d = %d + %d", sizeof(*pSmsEnv) + pSmsEnv->bPayloadLen,
	//		sizeof(*pSmsEnv),pSmsEnv->bPayloadLen);
	return sizeof(*pSmsEnv) + pSmsEnv->bPayloadLen;

error:
	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smssend_isEmpty(smssend* pSend)
{
	return list_empty(&pSend->listToken);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
BOOL smssend_addMsg(smssend* pSend, const char* szDstNumber, const char* szMsg, int encode_type, int cbMsg)
{
	// get length
	int cbDstNumber=strlen(szDstNumber)+1;

	// allocate token
	smstoken* pToken;
	__goToErrorIfFalse(pToken=smssend_allocToken(cbDstNumber,cbMsg+1));

	// copy
	strcpy(pToken->pDstNumber,szDstNumber);
	memcpy(pToken->pMsg, szMsg, cbMsg);

	/* mark ucs-2 field */
	pToken->EncodeType = encode_type;

	// initiate members
	pToken->cbMsg=cbMsg;

	// add
	list_add(&pToken->list,&pSend->listToken);

	return TRUE;

error:
	smssend_freeToken(pToken);
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////
int smsConvCharNibbleToInt(char chNibble)
{
	if (('0' <= chNibble) && (chNibble <= '9'))
		return chNibble -'0';

	return ((chNibble | ('a' ^ 'A')) - 'a' + 0x0a) & 0x0f;
}
///////////////////////////////////////////////////////////////////////////////
int smsConvIntToChar(unsigned int* src, char* dst, int cbMsg)
{
    int i;
    for (i = 0; i < cbMsg; i++, src++) {
        *dst++ = (char)((*src & 0xff00) >> 8);
        *dst++ = (char)(*src & 0x00ff);
    }
	return cbMsg*2;;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
/* max len of pEncodeMsg = max len of pMsg * 2 */
int smssend_encodeMsg(char* pMsg, char* pEncodeMsg, int cbMsg, int encode_type)
{
	/* utf8 msg len <= unicode msg len, so max len of ptmpMsg is cbMsg*UINT_SIZE. */
    unsigned int* ptmpMsg = __alloc(cbMsg*UINT_SIZE);
    int cbTmpMsg = 0, cbEncodeMsg = 0;
	if(!ptmpMsg) {
        syslog(LOG_ERR, "failed to allocate encode memory %d bytes", cbMsg*UINT_SIZE);
        return -1;
	}
    syslog(LOG_ERR, "encode to %s", (encode_type == CNSMGR_ENCODE_UCS_2? "UCS2":"GSM7"));
	(void) memset(ptmpMsg, 0x0, cbMsg*UINT_SIZE);
	cbTmpMsg = smssend_encodeUnicodeFromUtf8(pMsg, ptmpMsg, cbMsg);
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	if (encode_type == CNSMGR_ENCODE_UCS_2) {
		cbEncodeMsg = smsConvIntToChar(ptmpMsg, pEncodeMsg, cbTmpMsg);
	} else {
		cbEncodeMsg = smssend_encodeGsmBit7FromUnicode(ptmpMsg, pEncodeMsg, cbTmpMsg);
	}
	printMsgBody(pEncodeMsg, cbEncodeMsg);
	__free(ptmpMsg);
	return cbEncodeMsg;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
smssend* smssend_create(void)
{
	smssend* pSend;

	__goToErrorIfFalse(pSend=__allocObj(smssend));

	INIT_LIST_HEAD(&pSend->listToken);

	return pSend;

error:
	smssend_destroy(pSend);
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
