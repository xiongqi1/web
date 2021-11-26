/*!
* Copyright Notice:
* Copyright (C) 2011 NetComm Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of NetComm Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY NETCOMM ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
* BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
* SUCH DAMAGE.
*
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#ifdef PLATFORM_PLATYPUS
#include <nvram.h>
#endif
#include <sys/times.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../models/model_default.h"
#include "../util/at_util.h"
#include "../util/rdb_util.h"
#include "../util/scheduled.h"
#include "../util/cfg_util.h"
#include "../dyna.h"
#include "pdu.h"
#include "sms.h"
#include "gsm7.h"
#include "utf8.h"
#include "coding.h"

extern volatile BOOL sms_disabled;
extern volatile BOOL pdu_mode_sms;
extern volatile BOOL ira_char_supported;
extern volatile BOOL numeric_cmgl_index;

///////////////////////////////////////////////////////////////////////////////
#define RDB_SMS_ENCODING_SCHEME		"smstools.conf.coding_scheme"
sms_encoding_type smstools_encoding_scheme = DCS_7BIT_CODING;
void read_smstools_encoding_scheme(void)
{
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
	nvram_init(RT2860_NVRAM);
	rd_data = nvram_bufget(RT2860_NVRAM, RDB_SMS_ENCODING_SCHEME);
	if (!rd_data)
		goto ret;
#else
	char rd_data[BSIZE_32] = {0, };
	if (rdb_get_single(RDB_SMS_ENCODING_SCHEME, rd_data, BSIZE_32) != 0)
		goto ret;
#endif
	if (strlen(rd_data) == 0)
		goto ret;
	if (strcmp(rd_data, "UCS2") == 0)
		smstools_encoding_scheme = DCS_UCS2_CODING;
	else
		smstools_encoding_scheme = DCS_7BIT_CODING;
ret:
	SYSLOG_INFO("rdb data '%s', set SMS encoding scheme to %s",
			rd_data, (smstools_encoding_scheme == DCS_7BIT_CODING? "GSM7":"UCS2"));
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
}
///////////////////////////////////////////////////////////////////////////////
int notiSMSRecv(const char* s)
{
	struct log_sms_message* pLSM = 0;
	char* pNewS = strdup(s);
	int cbLen = -1;
	char* szSMS;

	if (sms_disabled) {
		SYSLOG_ERR("SMS feature is disabled. check appl options");
		goto error;
	}

	if (!pNewS) {
		SYSLOG_ERR("failed to allocate memory in %s()", __func__);
		goto error;
	}

	// +CMT: ,48
	char* pATRes = pNewS;
	pATRes += strlen("+CMT:");

	// get first token
	const char* pToken;

	// skip the first token
	char* pSavePtr;
	pToken = strtok_r(pATRes, ",", &pSavePtr);
	if (!pToken) {
		SYSLOG_INFO("got +CMT but failed to get tokens");
		/* read out message field if exists */
		szSMS =(char*) direct_at_read(1);
		goto error;
	}
	/* stupid Cinterion PHS8-P does not follow 3GPP TS 27.005 format.
	* It responds with "+CMT: 25" without colon. SO we need to check it. */
	if (!pSavePtr || strlen(pSavePtr) == 0) {
		cbLen = atoi(pToken);
		SYSLOG_INFO("got +CMT & failed to get tokens but found length field anyway: %d", cbLen);
	}

	if(!is_cinterion_cdma) // Temporary code for unmatching URC response of Cinterion PSV8 module
	{
		// get the 2nd token
		if (cbLen < 0) {
			pToken = strtok_r(NULL, ",", &pSavePtr);
			if (!pToken) {
				SYSLOG_INFO("got +CMT but failed to get tokens");
				szSMS =(char*) direct_at_read(1);
				goto error;
			}
			cbLen = atoi(pToken);
		}

		// read following line - this is part of notification
		szSMS =(char*) direct_at_read(1);
		if (!szSMS)	{
			SYSLOG_ERR("no SMS-deliver message followed");
			goto error;
		}
	}
	else
	{
		// get the 2nd token
		 szSMS = strtok_r(NULL, ",", &pSavePtr);

		if (!szSMS)	{
			SYSLOG_ERR("no SMS-deliver message followed");
			goto error;
		}
	}


	if(!is_cinterion_cdma)
	{
		/* total pdu part len = (smsc_addr_len+1+pdu_len)*2 */
		char tmp[3] = {0,};
		strncpy(tmp, szSMS, 2);
		int cbStrLen = (atoi(tmp) + 1 + cbLen) * 2 ;
		SYSLOG_NOTICE("got a SMS notification (cbLen=%d,cbStrLen=%d)", cbLen, cbStrLen);
	}

	// create logical sms message structure
	if (!is_cinterion_cdma)
		pLSM = pduCreateLogSMSMessage(szSMS, SMS_DELIVER);
	else
		pLSM = pduCreateCDMALogSMSMessage(szSMS, SMS_DELIVER);

	if (!pLSM) {
		SYSLOG_INFO("failed to parse PDU");
		goto error;
	}

	SYSLOG_NOTICE("DCS parsed - cmd=0x%02x,param=0x%02x", pLSM->dcs.cmd, pLSM->dcs.param);

	switch (pLSM->dcs.cmd) {
		case SMS_DCS_CMD_MWI_DISCARD:
		case SMS_DCS_CMD_MWI_STORE1:
		case SMS_DCS_CMD_MWI_STORE2:
		{

			int indiType = pLSM->dcs.param & SMS_DCS_PARAM_INDI_TYPE;
			int indiActive = pLSM->dcs.param & SMS_DCS_PARAM_INDI_ACTIVE;

			SYSLOG_NOTICE("Message Waiting Indication Group detected - type=%d,active=%d", indiType, indiActive);

			if (indiType == SMS_DCS_PARAM_INDI_TYPE_VM) {
				SYSLOG_NOTICE("VM detected");

				if (indiActive)
					rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "active");
				else
					rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "inactive");
			}

			break;
		}

		default:
			SYSLOG_ERR("unknown DCS: %d", pLSM->dcs.cmd);
			break;
	}


	dynaFree(pLSM);

	if (pNewS)
		free(pNewS);

	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "calls.event"), "ring");

error:
	dynaFree(pLSM);

	if (pNewS)
		free(pNewS);

	return -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SECS_TO_2000_FM_1970	3600 * 24 * 365 * 30
static void sms_get_current_time(char *pBuf, int cbBuf)
{
	struct tm *now = NULL;
	time_t tl;
	int cbLen;

	tl = time(NULL);
	now = localtime(&tl);

	/* build date time */
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
}
///////////////////////////////////////////////////////////////////////////////
volatile int max_mem_capacity = 0;
static int CheckSmsStorage(void)
{
	int total_cnt, used_cnt, stat, fOK;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char* tmp_str = alloca(BSIZE_32);
	char command[BSIZE_32];
	const char* pToken;
	char* pSavePtr;
	char* pATRes;
	int mem_type;	/* 0: SIM, 1: ME */
	__goToErrorIfFalse(response)

#ifndef USE_SIM_AS_SMS_STORAGE
	__free(response);
	return 1;
#endif

	sprintf(command, "AT+CPMS?");
	stat = at_send(command, response, "", &fOK, 0);
	if (stat < 0 || !fOK || strlen(response) == 0) {
		SYSLOG_ERR("%s command failed, Can not read new SMS message", command);
		__goToError()
	}

	/* +CPMS: "SM",2,30,"SM",2,30,"SM",2,30 */
	pATRes=response;
	pATRes += strlen("+CPMS:");

	/* get first token & skip the first token */
	pToken = strtok_r(pATRes, ",", &pSavePtr);
	if (!pToken)
		SYSLOG_ERR("got +CPMS but failed to get tokens");
	mem_type = (strcmp(pToken, "SM"));
	SYSLOG_NOTICE("Message storage type is %s", mem_type? "ME":"SM");

	/* get the 2nd token : total message count */
	pToken = strtok_r(NULL, ",", &pSavePtr);
	if (!pToken) {
		SYSLOG_ERR("failed to get total message count");
		__goToError()
	}
	used_cnt = atoi(pToken);

	/* get the 3rd token : total SIM/ME storage space */
	pToken = strtok_r(NULL, ",", &pSavePtr);
	if (!pToken) {
		SYSLOG_ERR("failed to get total message storage space");
		__goToError()
	}
	total_cnt = atoi(pToken);
	if (total_cnt > 0)
		max_mem_capacity = total_cnt;
	SYSLOG_NOTICE("%s storage : %d / %d", mem_type? "ME":"SM", used_cnt, total_cnt);
	sprintf(tmp_str, "used:%d total:%d", used_cnt, total_cnt);
	rdb_set_single(rdb_name(RDB_SMS_STATUS3, ""), tmp_str);
	if (used_cnt == total_cnt)
		__goToError()

	__free(response);
	return 0;
error:
	__free(response);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int deleteSmsMessage(int index)
{
	int stat, fOK;
	char command[BSIZE_32];
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	__goToErrorIfFalse(response)

	sprintf(command, "AT+CMGD=%d", index);
	SYSLOG_DEBUG("delete message : send AT+CMGD=%d command", index);
	stat = at_send(command, response, "", &fOK, 0);
	__free(response);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("%s command failed, Can not delete message", command);
		__goToError()
	}
	return 0;
error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
const char sms_msg_file[30] = "/tmp/sms_msgs";
int readOneUnreadMsg(int index);
static int UpdateSmsMemStatusText(void)
{
	int stat, fOK, i, mem_idx;
	char command[BSIZE_32];
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char* response2 = __alloc(AT_RESPONSE_MAX_SIZE);
	char* tmp_str = alloca(BSIZE_64);
	char *pToken, *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	int msg_cnt[5] = {0,};
	int fp = 0, msg_len = 0, skip_index, dcs = 0;
	char* pDecodeMsg = __alloc(MAX_UNICODE_BUF_SIZE);
	int cbDecodeMsg = 0;
	struct sms_dcs tmp_dcs;
	sms_encoding_type coding_type = DCS_7BIT_CODING;
	struct sms_deliver_type *pFO = NULL;
	char FO;
	char* pUdhDecodeMsg = __alloc(MAX_UTF8_BUF_SIZE);
	int cbUdhDecodeMsg;

	__goToErrorIfFalse(response)
	__goToErrorIfFalse(response2)
	__goToErrorIfFalse(pDecodeMsg)
	__goToErrorIfFalse(pUdhDecodeMsg)

	fp = open(sms_msg_file, O_CREAT | O_WRONLY | O_TRUNC);
	if(fp < 0) {
		SYSLOG_ERR("fail to create sms msg file %s\n", sms_msg_file);
		__goToError()
	}

	for (mem_idx = 0; mem_idx < max_mem_capacity; mem_idx++) {
		sprintf(command, "AT+CMGR=%d", mem_idx);
		SYSLOG_INFO("send %s command", command);
		stat = at_send(command, response, "", &fOK, 0);
		if (stat < 0 || !fOK)
			continue;
		SYSLOG_INFO("%s : %s", command, response);

		/* write all msg list and contents to file because rdb variable can not
		 * contain huge strings */
		pATRes = response;
		pToken = strtok_r(pATRes, "\n", &pSavePtr);
		while (pToken) {
			if (strstr(pToken, "+CMGR:") != 0) {
				if (strstr(pToken, "\"STO UNSENT\"") != 0) {
					msg_cnt[STO_UNSENT]++;
					/* delete unsent SMS from memory */
					deleteSmsMessage(mem_idx);
					while (pToken) {
						pToken = strtok_r(NULL, "\n", &pSavePtr);
					}
					break;
				}

				/* write to file with attached index & get msg length */
				pToken2 = strtok_r((char *)pToken, ",", &pSavePtr2);
				if (strstr(pToken2, "\"REC READ\"") != 0) {
					sprintf(response2, "+CMGR: %d,\"REC READ\",%s", mem_idx, pSavePtr2);
					msg_cnt[REC_READ]++;
					skip_index = 7;
				}
				else if (strstr(pToken2, "\"REC UNREAD\"") != 0) {
					sprintf(response2, "+CMGR: %d,\"REC UNREAD\",%s", mem_idx, pSavePtr2);
					msg_cnt[REC_UNREAD]++;
					skip_index = 7;
					(void) readOneUnreadMsg(mem_idx);
				}
				else {
					sprintf(response2, "+CMGR: %d,\"STO SENT\",%s", mem_idx, pSavePtr2);
					msg_cnt[STO_SENT]++;
					skip_index = 5;
				}

				/* skip until <fo> field */
				for (i = 0; i< skip_index-2; i++)
					pToken2 = strtok_r(NULL, ",", &pSavePtr2);
				/* read <fo> and parse
				 * bit 6 : UDHI : 0 = no UDH, 1 = UDH
				 * bit 2 : MMS  : 0 = more msg, 1 = no more msg
				 */
				FO = (char)(pToken2? atoi(pToken2):0);
				pFO = (struct sms_deliver_type *)&FO;
				if (strstr(response2, "\"REC READ\"") != 0 || strstr(response2, "\"REC UNREAD\"") != 0) {
					SYSLOG_NOTICE("<fo> : 0x%02x, %s, %s", FO,
						(pFO->udhi == UDH_EXIST? "UDH exist":"UDH not exist"),
						(pFO->mms == MORE_MSG? "more msg":"no more msg"));
				} else {
					SYSLOG_ERR("<fo> field of SENT message in Text mode should be ignored");
					pFO->udhi = UDH_NON_EXIST;
				}

				write(fp, response2, strlen(response2));
				write(fp, "\n", 1);

				/* do not increase msg count for partial messages */
				if (pFO->udhi == UDH_EXIST && pFO->mms == MORE_MSG) {
					if (strstr(response2, "\"REC READ\"") != 0) {
						msg_cnt[REC_READ]--;
					}
					else if (strstr(response2, "\"REC UNREAD\"") != 0) {
						msg_cnt[REC_UNREAD]--;
					}
				}

				/* read DCS and parse */
				for (i = 0; i< 2; i++)
					pToken2 = strtok_r(NULL, ",", &pSavePtr2);
				dcs = (pToken2? atoi(pToken2):0);
				tmp_dcs.cmd = (dcs & 0xf0) >> 4;
				tmp_dcs.param = (dcs & 0x0f);
				coding_type = parse_msg_coding_type(&tmp_dcs);

				/* move to message length field */
				while(pSavePtr2) {
					pToken2 = strtok_r(NULL, ",", &pSavePtr2);
				}
				if (!pToken2) {
					SYSLOG_ERR("can not retrieve msg length");
					while (pToken) {
						pToken = strtok_r(NULL, "\n", &pSavePtr);
					}
					break;
				}
				msg_len = atoi(pToken2);
			}
			else if (!pToken || strlen(pToken) == 0 || strcmp(pToken, "OK") == 0) {
				break;
			}
			else {
				/* reuse response2 buffer for message body string buffer */
				(void) memset(response2, 0x00, AT_RESPONSE_MAX_SIZE);
				while (pToken) {
					sprintf(response2, "%s%s\n", response2, pToken);
					pToken = strtok_r(NULL, "\n", &pSavePtr);
				}
				SYSLOG_NOTICE("msg_len = %d, encoded to %s : %s", msg_len,
					((coding_type == DCS_UCS2_CODING)? "UCS2":
					((coding_type == DCS_7BIT_CODING)? "GSM7":"8BIT")), response2);
				if (pFO->udhi == UDH_EXIST && pFO->mms == NO_MORE_MSG)
					write(fp, "CONCAT-LAST:", strlen("CONCAT-LAST:"));
				else if (pFO->udhi == UDH_EXIST && pFO->mms == MORE_MSG)
					write(fp, "CONCAT-MID:", strlen("CONCAT-MID:"));
				/* read message line */
				if (coding_type == DCS_7BIT_CODING)	{			/* GSM 7 bit coding */
					SYSLOG_DEBUG("detect GSM 7 bit encoded msg");
					/* If UDH field is exist then the message body is two IRA long hexa
					 * decimal number even though DCS field indicates GSM 7bit coding.
					 * Need to convert to GSM 7bit hexa message in this case. */
					if (pFO->udhi == UDH_EXIST) {
						/* Msg length field represents real GSM 7 bit message length so
						 * IRA string length is two times of this field value */
						cbUdhDecodeMsg = ConvStrToChar(response2, msg_len*2, pUdhDecodeMsg, msg_len);
						cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7(pUdhDecodeMsg, pDecodeMsg, msg_len);
					} else {
						cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7(response2, pDecodeMsg, msg_len);
					}
					if(cbDecodeMsg < 0) {
						SYSLOG_ERR("fail to decode Utf8 from GSM7");
						__goToError()
					}
					write(fp, "GSM7:", strlen("GSM7:"));
					/* 3G/4G modules return parsed output so just use that result without decoding
					 * except that UDH field exists. */
					if (pFO->udhi == UDH_EXIST) {
						write(fp, pDecodeMsg, cbDecodeMsg);
						SYSLOG_DEBUG("decoded UTF-8 msg: %s", pDecodeMsg);
					} else {
						write(fp, response2, msg_len);
						SYSLOG_DEBUG("decoded UTF-8 msg: %s", response2);
					}
					write(fp, "\n", 1);
				} else if (coding_type == DCS_UCS2_CODING) {	/* UCS 2 coding */
					SYSLOG_DEBUG("detect UCS 2 encoded msg");
					cbDecodeMsg = smsrecv_decodeUtf8FromUnicodeStr(response2, pDecodeMsg, msg_len*4);
					if(cbDecodeMsg < 0) {
						SYSLOG_ERR("fail to decode Utf8 from Unicode");
						__goToError()
					}
					write(fp, "UCS2:", strlen("UCS2:"));
					write(fp, pDecodeMsg, cbDecodeMsg);
					write(fp, "\n", 1);
					SYSLOG_DEBUG("decoded UTF-8 msg: %s", pDecodeMsg);
				} else {	// text mode 8 bit is not supported yet
					SYSLOG_NOTICE("found 8bit or binary coding scheme : 0x%2x, msg : %s", dcs, response2);
					write(fp, "8BIT:", strlen("8BIT:"));
					write(fp, response2, strlen(response2));
					write(fp, "\n", 1);
				}
			}
			pToken = strtok_r(NULL, "\n", &pSavePtr);
		}
	}

	sprintf(tmp_str, "Total:%d read:%d unread:%d sent:%d unsent:%d",
			msg_cnt[REC_READ]+msg_cnt[REC_UNREAD]+msg_cnt[STO_UNSENT]+msg_cnt[STO_SENT],
			msg_cnt[REC_READ], msg_cnt[REC_UNREAD], msg_cnt[STO_SENT], msg_cnt[STO_UNSENT]);
	SYSLOG_NOTICE("sms status : %s", tmp_str);
	rdb_set_single(rdb_name(RDB_SMS_STATUS2, ""), tmp_str);

	/* if there were unsent message deleted, call this function recursively to
	 * update message state again.
	 */
	if (msg_cnt[STO_UNSENT]) {
		SYSLOG_NOTICE("Deleted %d Unsent messages, update again!", msg_cnt[STO_UNSENT]);
		UpdateSmsMemStatus(1);
	}
	if (fp > 0) close(fp);
	__free(pUdhDecodeMsg);
	__free(pDecodeMsg);
	__free(response2);
	__free(response);
	return 0;
error:
	if (fp > 0) close(fp);
	__free(pUdhDecodeMsg);
	__free(pDecodeMsg);
	__free(response2);
	__free(response);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
extern void pduFreeLogSMSMessage(void* pRef);
static int UpdateSmsMemStatusPdu(void)
{
	int stat, fOK, mem_idx;
	char command[BSIZE_32];
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char *pToken, *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	char* tmp_str=alloca(BSIZE_64);
	int msg_cnt[5] = {0,};
	int fp = 0, cbLen, cbStrLen;
	sms_msg_type msg_type;
	struct log_sms_message* pLSM = 0;
	struct tm* ts;
	unsigned char* msg_body = __alloc(MAX_UNICODE_BUF_SIZE);
	sms_encoding_type coding_type;
	char *tmp_str2, *tmp_str3;
	char time_str[30];

#if (defined ZERO_LEN_PDU_MSG_TEST) || (defined LONG_PDU_MSG_TEST) || (defined ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST)
	unsigned char* temp = __alloc(AT_RESPONSE_MAX_SIZE);
#endif

	__goToErrorIfFalse(response)
	__goToErrorIfFalse(msg_body)
#if (defined ZERO_LEN_PDU_MSG_TEST) || (defined LONG_PDU_MSG_TEST) || (defined ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST)
	__goToErrorIfFalse(temp)
#endif

	fp = open(sms_msg_file, O_CREAT | O_WRONLY | O_TRUNC);
	if(fp < 0) {
		SYSLOG_ERR("fail to create sms msg file %s\n", sms_msg_file);
		__goToError()
	}

	for (mem_idx = 0; mem_idx < max_mem_capacity; mem_idx++) {
		sprintf(command, "AT+CMGR=%d", mem_idx);
		SYSLOG_INFO("send %s command", command);
		stat = at_send(command, response, "", &fOK, 0);
		if (stat < 0 || !fOK)
			continue;
		SYSLOG_INFO("%s : %s", command, response);

		/* write all msg list and contents to file because rdb variable can not
		 * contain huge strings */
		pATRes = response;
		pToken = strtok_r(pATRes, "\n", &pSavePtr);

		if (!pToken || strlen(pToken) == 0 || strcmp(pToken, "OK") == 0 || strstr(pToken, "+CMGR:") == 0 )
			continue;

		pToken += strlen("+CMGR: ");

		/* Ericsson : +CMGR: 3,: 3,,112<cr/lf>07911614786007F001040A814013511
         *            477000864006B003100320033....
		 * need to skip garbage bytes */
		if (is_ericsson) {
			char c = *pToken;
			while (c != ':' && c != '\n' && c != '\r') {
				pToken++;
				c = *pToken;
			}
			if (c != ':') {
				SYSLOG_ERR("failed to find ':' in PDU sms msg");
				continue;
			}
			pToken++;
		}

		pToken2 = strtok_r(pToken, ",", &pSavePtr2);
		if (!pToken2) {
			SYSLOG_INFO("got +CMGR but failed to get tokens");
		}
		msg_type = atoi(pToken2);
		if (msg_type < REC_UNREAD || msg_type > ALL) {
			SYSLOG_ERR("msg type field parsing error, ignore this msg");
			continue;
		}
		msg_cnt[msg_type]++;

		/* delete unsent SMS from memory */
		if (msg_type == STO_UNSENT) {
			deleteSmsMessage(mem_idx);
			continue;
		}

		/* read a new msg and save to file */
		if (msg_type == REC_UNREAD)
			(void) readOneUnreadMsg(mem_idx);

		/* get the 2nd token */
		pToken2 = strtok_r(NULL, ",", &pSavePtr2);
		cbLen = atoi(pToken2);

		/* total pdu part len = (smsc_addr_len+1+pdu_len)*2 */
		char tmp[3] = {0,};
		strncpy(tmp, pSavePtr, 2);
		cbStrLen = (atoi(tmp) + 1 + cbLen) * 2 ;
		SYSLOG_INFO("pdu package length (cbLen=%d,cbStrLen=%d)", cbLen, cbStrLen);

#ifdef ZERO_LEN_PDU_MSG_TEST
		strcpy(temp, "07911356048801802409B913000012F400032101211000208000");
		SYSLOG_ERR("TEST MSG: %s", temp);
		pSavePtr = temp;
#endif
#ifdef LONG_PDU_MSG_TEST
		// Rxed on Cinterion
		strcpy(temp, "07911614786007F0440B911634905651F2000021117261759044A0050"
                     "003E60301E8E5391D442FCFE9207A794E07D1CB733A885E9ED341F4F2"
                     "9C0EA297E77410BD3CA783E8E5391D442FCFE9207A794E07D1CB733A8"
                     "85E9ED341F4F29C0EA297E77");
		// Rxed on Sierra
		//strcpy(temp, "07911614786007F0440B911634905651F2000021117261346444A00"
        //             "50003E30301E8E5391D442FCFE9207A794E07D1CB733A885E9ED341"
        //             "F4F29C0EA297E77410BD3CA783E8E5391D442FCFE9207A794E07D1C"
        //             "B733A885E9ED341F4F29C0EA297E77410BD3CA783E8E5391D442FCF"
        //             "E9207A794E07D1CB733A885E9ED341F4F29C0EA297E77410BD3CA78"
        //             "3E8E5391D442FCFE9207A794E07D1CB733A885E9ED341F4F29C0EA2"
        //             "97E7");
		SYSLOG_ERR("TEST MSG: %s", temp);
		pSavePtr = temp;
#endif
#ifdef ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST
		strcpy(temp, "07911614786007F00414D0D4327B4E978789617A18000061209231640244"
					 "81D977FD642F83EAF33219549B95406F3328FFAECB4131170C761482C861"
					 "7A18C44EB7D374D03D4D4783623610399C9F83D86533DD05A2A2D37350DA"
					 "6D7E83DAE13C485");
		SYSLOG_ERR("TEST MSG: %s", temp);
		pSavePtr = temp;
#endif

		/* create logical sms message structure */
		if (msg_type == REC_UNREAD || msg_type == REC_READ) {
			if (!is_cinterion_cdma)
				pLSM = pduCreateLogSMSMessage((const char *)pSavePtr, SMS_DELIVER);
			else
				pLSM = pduCreateCDMALogSMSMessage((const char *)pSavePtr, SMS_DELIVER);
		}
		else {
			if (!is_cinterion_cdma)
				pLSM = pduCreateLogSMSMessage((const char *)pSavePtr, SMS_SUBMIT);
			else
				pLSM = pduCreateCDMALogSMSMessage((const char *)pSavePtr, SMS_SUBMIT);
		}

		if (!pLSM) {
			SYSLOG_ERR("failed to parse PDU, ignore this msg");
			dynaFree(pLSM);
			continue;
		}

		/* write to file with attached index & get msg length */
		ts = 0;
		if (pLSM->msg_type.smsDeliverType.mti == SMS_DELIVER ||
			pLSM->msg_type.smsDeliverType.mti == SMS_RESERVED_TYPE)
			ts = pduCreateLinuxTime(pLSM->pTimeStamp);

		/* parse message coding type from dcs */
		coding_type = parse_msg_coding_type((struct sms_dcs *)&pLSM->dcs);
		SYSLOG_DEBUG("decode %s encoded message body to Utf8",
					((coding_type == DCS_UCS2_CODING)? "UCS2":
					((coding_type == DCS_7BIT_CODING)? "GSM7":"8BIT")));
		if (decode_pdu_msg_body(pLSM, msg_body, coding_type) < 0)
			strcpy((char *)msg_body, "MESSAGE BODY DECODING ERROR");

		/* do not increase msg count for partial messages
		 * UDHI field location is same for delivery and submit type but
		 * submit message has no MMS field. So refer to parsed UDH fields.
		 */
		if (pLSM->msg_type.smsDeliverType.udhi == UDH_EXIST && pLSM->udh.seq_no < pLSM->udh.msg_no) {
			msg_cnt[msg_type]--;
		}

		tmp_str2 = tmp_str3 = 0;
		switch (msg_type) {
			case REC_UNREAD:
			case REC_READ:
				/*  +CMGR: "REC UNREAD","+61438404841",,"11/04/28,10:27:34+40",145,4,0,0,"+61418706700",145,48
					sms_test ?=!@#$%? &*()_+-=?(?)?<?>?/?!;:'",./<>?
					+CMGR: <stat>,<oa>,[<alpha>],<scts>  [,<tooa>,<fo>,<pid>,<dcs>,	; <fo> first octet (17 for submit)
					<sca>,<tosca>,<length>]<CR><LF><data>
				*/
				tmp_str2 = pduCreateHumanAddr(pLSM->addr.pOrigAddr, MOBILE_NUMBER);
				tmp_str3 = pduCreateHumanAddr(pLSM->pSMSCNumber, SMSC_NUMBER);

				// let's decode alpha-numeric address from funky Telstra
				if ((pLSM->addr.pOrigAddr->bAddressType & 0xf0) == 0xd0) {
					SYSLOG_DEBUG("found alpha-numeric address type, pLSM->addr.pOrigAddr->bAddressType = 0x%02x", pLSM->addr.pOrigAddr->bAddressType);
					char* pAddrDecodeMsg = __alloc(MAX_UNICODE_BUF_SIZE);
					int cbAddrDecodeMsg = 0;
					struct log_sms_message* pAddrLSM = dynaCreate(sizeof(struct log_sms_message), pduFreeLogSMSMessage);
					if (!pAddrLSM) {
						SYSLOG_ERR("failed to parse PDU, ignore this msg");
						dynaFree(pLSM);
						__goToError()
					}
					SYSLOG_DEBUG("found alpha-numeric address type, pLSM->addr.pOrigAddr->bAddressType = 0x%02x", pLSM->addr.pOrigAddr->bAddressType);
					(void) memset(pAddrLSM, 0x00, sizeof(struct log_sms_message));
					(void) memset(pAddrDecodeMsg, 0x00, MAX_UNICODE_BUF_SIZE);
					pAddrLSM->pRawSMSMessage = dynaCreate(pLSM->addr.pOrigAddr->cbAddrLen, 0);
					pAddrLSM->cbRawSMSMessage = pLSM->addr.pOrigAddr->cbAddrLen;
					(void) memset(pAddrLSM->pRawSMSMessage, 0x00, pAddrLSM->cbRawSMSMessage);
					pAddrLSM->pUserData = pAddrLSM->pRawSMSMessage;
					pAddrLSM->cbUserData = ((pLSM->addr.pOrigAddr->cbAddrLen/2)*8 - 7)/7 + 1;
					memcpy(pAddrLSM->pUserData, pLSM->addr.pOrigAddr->addrNumber, pAddrLSM->cbUserData);
					cbAddrDecodeMsg = decode_pdu_msg_body(pAddrLSM, (unsigned char *)pAddrDecodeMsg, coding_type);
					SYSLOG_DEBUG("cbAddrDecodeMsg = %d, pAddrDecodeMsg = %s", cbAddrDecodeMsg, pAddrDecodeMsg);
					strcpy(tmp_str2, pAddrDecodeMsg);
					dynaFree(pAddrLSM);
				}

				if (ts)
					sprintf(time_str, "%04d/%02d/%02d,%02d:%02d:%02d",
							ts->tm_year, ts->tm_mon, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
				else
					strcpy(time_str, "");

				sprintf(response, "+CMGR: %d,\"REC %sREAD\",\"%s%s\",,\"%s\",%d,4,0,%d,\"%s%s\",%d,%d\n%s%s%s",
						mem_idx, (msg_type == REC_UNREAD? "UN":""),
						(pLSM->addr.pOrigAddr->bAddressType == 145? "+":""), tmp_str2,
						time_str,
						pLSM->addr.pOrigAddr->bAddressType, pduConvDcsToInt(&pLSM->dcs),
						(pLSM->pSMSCNumber->bAddressType == 145? "+":""), tmp_str3,
						pLSM->pSMSCNumber->bAddressType, pLSM->cbUserData,
						(pLSM->msg_type.smsDeliverType.udhi == UDH_EXIST?
						(pLSM->udh.seq_no < pLSM->udh.msg_no? "CONCAT-MID:":"CONCAT-LAST:"):""),
						((coding_type == DCS_UCS2_CODING)? "UCS2:":((coding_type == DCS_7BIT_CODING)? "GSM7:":"8BIT:")),
						msg_body);
				break;
			case STO_SENT:
				/* +CMGR: "STO SENT","0431154177",,129,17,0,0,167,"+61418706700",145,8
					sms test
					+CMGR: <stat>,<da>,[<alpha>][,<toda>,<fo>,<pid>,<dcs>,[<vp>],    ; <fo> first octet (17 for submit)
					<sca>,<tosca>,<length>]<CR><LF><data>							; <vp> valid period (167 for submit default)
				*/
				tmp_str2 = pduCreateHumanAddr(pLSM->addr.pDestAddr, MOBILE_NUMBER);
				tmp_str3 = pduCreateHumanAddr(pLSM->pSMSCNumber, SMSC_NUMBER);
				sprintf(response, "+CMGR: %d,\"STO SENT\",\"%s%s\",,%d,17,0,%d,167,\"%s%s\",%d,%d\n%s%s%s",
						mem_idx,
						(pLSM->addr.pDestAddr->bAddressType == 145? "+":""), tmp_str2,
						pLSM->addr.pDestAddr->bAddressType, pduConvDcsToInt(&pLSM->dcs),
						(pLSM->pSMSCNumber->bAddressType == 145? "+":""), tmp_str3,
						pLSM->pSMSCNumber->bAddressType, pLSM->cbUserData,
						(pLSM->msg_type.smsDeliverType.udhi == UDH_EXIST?
						(pLSM->udh.seq_no < pLSM->udh.msg_no? "CONCAT-MID:":"CONCAT-LAST:"):""),
						((coding_type == DCS_UCS2_CODING)? "UCS2:":((coding_type == DCS_7BIT_CODING)? "GSM7:":"8BIT:")),
						msg_body);
				break;
			default:
				break;
		}
		write(fp, response, strlen(response));
		write(fp, "\n", 1);
		dynaFree(pLSM);
		dynaFree(ts);
		dynaFree(tmp_str2);
		dynaFree(tmp_str3);
	}

	sprintf(tmp_str, "Total:%d read:%d unread:%d sent:%d unsent:%d",
			msg_cnt[REC_READ]+msg_cnt[REC_UNREAD]+msg_cnt[STO_UNSENT]+msg_cnt[STO_SENT],
			msg_cnt[REC_READ], msg_cnt[REC_UNREAD], msg_cnt[STO_SENT], msg_cnt[STO_UNSENT]);
	SYSLOG_NOTICE("sms status : %s", tmp_str);
	rdb_set_single(rdb_name(RDB_SMS_STATUS2, ""), tmp_str);

	/* if there were unsent message deleted, call this function recursively to
	 * update message state again.
	 */
	if (msg_cnt[STO_UNSENT]) {
		SYSLOG_NOTICE("Deleted %d Unsent messages, update again!", msg_cnt[STO_UNSENT]);
		UpdateSmsMemStatus(1);
	}
	if (fp > 0) close(fp);
	__free(response);
	__free(msg_body);
#if (defined ZERO_LEN_PDU_MSG_TEST) || (defined LONG_PDU_MSG_TEST) || (defined ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST)
	__free(temp);
#endif
	return 0;
error:
	if (fp > 0) close(fp);
	__free(response);
	__free(msg_body);
#if (defined ZERO_LEN_PDU_MSG_TEST) || (defined LONG_PDU_MSG_TEST) || (defined ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST)
	__free(temp);
#endif
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int UpdateSmsMemStatus(int updateMem)
{
	char* tmp_str=alloca(BSIZE_32);
#ifndef USE_SIM_AS_SMS_STORAGE
	return 1;
#endif

	(void) memset(tmp_str, 0x0, BSIZE_32);
	(void) rdb_get_single(rdb_name(RDB_SMS_STATUS3, ""), tmp_str, BSIZE_32);
	if (max_mem_capacity == 0 || updateMem || strlen(tmp_str) == 0)
		CheckSmsStorage();

	if (pdu_mode_sms)
		return UpdateSmsMemStatusPdu();
	else
		return UpdateSmsMemStatusText();
}
///////////////////////////////////////////////////////////////////////////////
static int SaveSmsTimeStamp(int index)
{
	char* tmp_str=alloca(BSIZE_32);
#ifndef PLATFORM_PLATYPUS
	char* s=alloca(BSIZE_32);
#endif
	char nv_var_name[BSIZE_32];

#ifndef USE_SIM_AS_SMS_STORAGE
	return 1;
#endif

	sms_get_current_time(tmp_str, BSIZE_32);
	rdb_set_single(rdb_name(RDB_SMS_RD_RXTIME, ""), tmp_str);
	(void) memset(nv_var_name, 0x00, BSIZE_32);
	sprintf(nv_var_name, "sms_txrx_time_%d", index);
#ifdef PLATFORM_PLATYPUS
	nvram_init(RT2860_NVRAM);
	nvram_set(RT2860_NVRAM, nv_var_name, tmp_str);
	nvram_close(RT2860_NVRAM);
	return 1;
#else
	if (rdb_get_single(nv_var_name, s, BSIZE_32) != 0) {
		SYSLOG_NOTICE("create '%s'", nv_var_name);
		if(rdb_create_variable(nv_var_name, "", CREATE | PERSIST, ALL_PERM, 0, 0)<0)
		{
			SYSLOG_ERR("failed to create '%s'", nv_var_name);
		}
	}
	rdb_set_single(nv_var_name, tmp_str);
	return 1;
#endif
}
///////////////////////////////////////////////////////////////////////////////
void printMsgBody(char* msg, int len)
{
	unsigned char* buf = (unsigned char *)msg;
	unsigned char buf2[BSIZE_256] = {0x0,};
	int i, j = len/16, k = len % 16;
	SYSLOG_DEBUG("---- sms msg body : %d bytes ---", len);
	for (i = 0; i < j; i++)	{
		SYSLOG_DEBUG("%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]);
	}
	j = i;
	for (i = 0; i < k; i++)
		sprintf((char *)buf2, "%s%02x, ", buf2, buf[j*16+i]);
	SYSLOG_DEBUG("%s", buf2);
}

void printMsgBodyInt(int* msg, int len)
{
	unsigned int* buf = (unsigned int *)msg;
	char buf2[BSIZE_256]={0x0,};
    int i, j = len/16, k = len % 16;
	SYSLOG_DEBUG("---- sms msg body : %d bytes ---", len);
	for (i = 0; i < j; i++) {
        SYSLOG_DEBUG("%04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]);
	}
	j = i;
	for (i = 0; i < k; i++)
        sprintf(buf2, "%s%04x, ", buf2, buf[j*16+i]);
	SYSLOG_DEBUG("%s", buf2);
}

///////////////////////////////////////////////////////////////////////////////
/* read sms tools path information */
#if defined PLATFORM_AVIAN
#define DEF_SMS_INBOX_PATH		"/system/cdcs/usr/sms/inbox"
#elif defined PLATFORM_BOVINE
#define DEF_SMS_INBOX_PATH		"/usr/local/cdcs/sms/inbox"
#elif defined PLATFORM_PLATYPUS
#define DEF_SMS_INBOX_PATH		"/var/sms/inbox"
#elif defined PLATFORM_PLATYPUS2
#define DEF_SMS_INBOX_PATH		"/var/sms/inbox"
#else
#define DEF_SMS_INBOX_PATH		"/usr/local/cdcs/sms/inbox"
#endif
#define RDB_SMS_INBOX_PATH		"smstools.inbox_path"
const char smstools_spool_inbox_path[]="/var/spool/sms/incoming";
char smstools_local_inbox_path[BSIZE_128]={0,};
int read_smstools_inbox_path(void)
{
	if(rdb_get_single(RDB_SMS_INBOX_PATH, (char *)&smstools_local_inbox_path[0], BSIZE_128) < 0) {
		return 0;
	}
	if (strcmp(smstools_local_inbox_path, "") == 0) {
		strcpy(smstools_local_inbox_path, DEF_SMS_INBOX_PATH);
	}
	SYSLOG_INFO("smstools_local_inbox_path = '%s'", smstools_local_inbox_path);
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
#ifndef USE_SIM_AS_SMS_STORAGE
#define SMS_FILE_INDEX_LIMIT 9999
static long get_new_rxmsg_file_index(void)
{
	struct dirent **local_inbox_name_list, **spool_inbox_name_list;
	struct stat sb;
	char tmp_idx_str[6] = {0,};
	int local_inbox_cnt, spool_inbox_cnt, i, j, found = 0, oldest_idx = SMS_FILE_INDEX_LIMIT;
	char msg_file[BSIZE_128] = {0,};
	long int msg_time, oldest_msg_time = 0;;

	if (!smstools_local_inbox_path[0]) {
		if (read_smstools_inbox_path() != 0) {
			return -1;
		}
	}
	local_inbox_cnt = scandir((char *)&smstools_local_inbox_path[0], &local_inbox_name_list, 0, alphasort);
	if (local_inbox_cnt < 0) {
		SYSLOG_ERR("%s may not exist or failed to open", smstools_local_inbox_path);
		return -1;
	}
	spool_inbox_cnt = scandir((char *)&smstools_spool_inbox_path[0], &spool_inbox_name_list, 0, alphasort);
	SYSLOG_NOTICE("local_inbox_cnt %d, spool_inbox_cnt %d", local_inbox_cnt, spool_inbox_cnt);
	for (i = 0; i < SMS_FILE_INDEX_LIMIT; i++) {
		if (local_inbox_cnt >= 2) {
			for (j = 0, found = 0; j < local_inbox_cnt; j++) {
				if(strcmp(local_inbox_name_list[j]->d_name, ".") == 0 || strcmp(local_inbox_name_list[j]->d_name, "..") == 0)
					continue;
				if (strstr(local_inbox_name_list[j]->d_name, "read")) {
					strncpy(tmp_idx_str, &local_inbox_name_list[j]->d_name[6], 5);
					if (atoi(tmp_idx_str) == i) {
						found = 1;

						/* read file creation time and update oldest file time and index */
						sprintf(msg_file, "%s/%s", smstools_local_inbox_path, local_inbox_name_list[j]->d_name);
						if (stat(msg_file, &sb) == -1) {
							SYSLOG_ERR("fail to get status of file '%s'", msg_file);
						} else {
							msg_time = (long int)sb.st_ctime;
							if (oldest_msg_time == 0 || msg_time < oldest_msg_time) {
								oldest_msg_time = msg_time;
								oldest_idx = i;
							}
							//SYSLOG_ERR("file '%s', msg_time %ld, oldest time %ld, oldest idx %d", msg_file, msg_time, oldest_msg_time, oldest_idx);
						}
						break;
					}
				}
			}
			/* if target index exists, find next index */
			if (found) {
				continue;
			}
		}
		if (spool_inbox_cnt >= 2) {
			for (j = 0, found = 0; j < spool_inbox_cnt; j++) {
				if(strcmp(spool_inbox_name_list[j]->d_name, ".") == 0 || strcmp(spool_inbox_name_list[j]->d_name, "..") == 0)
					continue;
				if (strstr(spool_inbox_name_list[j]->d_name, "read")) {
					strncpy(tmp_idx_str, &spool_inbox_name_list[j]->d_name[6], 5);
					if (atoi(tmp_idx_str) == i) {
						found = 1;

						/* read file creation time and update oldest file time and index */
						sprintf(msg_file, "%s/%s", smstools_spool_inbox_path, spool_inbox_name_list[j]->d_name);
						if (stat(msg_file, &sb) == -1) {
							SYSLOG_ERR("fail to get status of file '%s'", msg_file);
						} else {
							msg_time = (long int)sb.st_ctime;
							if (oldest_msg_time == 0 || msg_time < oldest_msg_time) {
								oldest_msg_time = msg_time;
								oldest_idx = i;
							}
							//SYSLOG_ERR("file '%s', msg_time %ld, oldest time %ld, oldest idx %d", msg_file, msg_time, oldest_msg_time, oldest_idx);
						}
						break;
					}
				}
			}
			/* if target index exists, find next index */
			if (found) {
				continue;
			}
		}
		break;
	}
	free(local_inbox_name_list);
	free(spool_inbox_name_list);
	/* If target index does not exist in local inbox nor spool inbox,
	 * then use it. This new index can be empty index or last index 99999.
	 * If there is no empty index, new message will overwrite oldest message. */
	if (i == SMS_FILE_INDEX_LIMIT) {
		SYSLOG_NOTICE("reach to limit, overwrite to oldest msg index = %d", oldest_idx);
		return oldest_idx;
	}
	SYSLOG_DEBUG("new index = %d", i);
	return i;
}
#endif
///////////////////////////////////////////////////////////////////////////////
char new_msg_file[BSIZE_128] = {0,};
static FILE * create_new_rxmsg_file(int idx)
{
	long new_idx;
	FILE* fp;

#ifdef USE_SIM_AS_SMS_STORAGE
	new_idx = idx;
#else
	new_idx = get_new_rxmsg_file_index();
	if (new_idx < 0) {
		return 0;
	}
#endif
	sprintf(new_msg_file, "%s/rxmsg_%05ld_unread", smstools_spool_inbox_path, new_idx);

	SYSLOG_NOTICE("creating new msg file %s", new_msg_file);
	fp = fopen(new_msg_file, "w+");
	if(!fp)	{
		SYSLOG_ERR("failed to create new rx msg file %s", new_msg_file);
		return 0;
	}
	return fp;
}
///////////////////////////////////////////////////////////////////////////////
static int delete_new_rxmsg_file(void)
{
	if(remove((const char *)new_msg_file) < 0) {
		SYSLOG_ERR("failed to delete new rx msg file %s", new_msg_file);
		return -1;
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int calc_initial_buffer_size(void)
{
	int used_cnt, stat, fOK;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char command[BSIZE_32];
	const char* pToken;
	char* pSavePtr;
	char* pATRes;
	__goToErrorIfFalse(response)

	sprintf(command, "AT+CPMS?");
	stat = at_send(command, response, "", &fOK, 0);
	if (stat < 0 || !fOK || strlen(response) == 0) {
		SYSLOG_ERR("%s command failed, Can not read new SMS message", command);
		__goToError()
	}

	/* +CPMS: "SM",2,30,"SM",2,30,"SM",2,30 */
	pATRes=response;
	pATRes += strlen("+CPMS:");

	/* get first token & skip the first token */
	pToken = strtok_r(pATRes, ",", &pSavePtr);
	/* get the 2nd token : total message count */
	pToken = strtok_r(NULL, ",", &pSavePtr);
	if (!pToken) {
		SYSLOG_ERR("failed to get total message count");
		__goToError()
	}
	used_cnt = atoi(pToken);
	__free(response);
	return (used_cnt/20+1);
error:
	__free(response);
	return 1;
}
///////////////////////////////////////////////////////////////////////////////
static int GetFirstUnreadMsgIdx(void)
{
	int stat, fOK;
	/* max size 10k can contain almost 30 sms msgs list for worst case of UCS2 coding
	 * if fail with CMGL command for size limit, increase buffer more
	 * start with enough buffer size depending on current message count */
	#define BASE_CMGL_RESP_SIZE		1024*5
	#define MAX_INCREASEMENT_LIMIT	10
	static int multiplier = 0;
	static int inc_cnt = 0;
	static long max_cmgl_resp_size;
	char* response;
	char* tmp_str=alloca(BSIZE_64);
	char *pToken, *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	int unread_cnt = 0;
	int first_index = -1, msg_idx;
	sms_msg_type msg_type = REC_UNREAD;

	if (multiplier == 0) {
		multiplier = calc_initial_buffer_size();
		SYSLOG_DEBUG("initial multiplier = %d", multiplier);
	}
retry_with_increased_size:
	max_cmgl_resp_size = (BASE_CMGL_RESP_SIZE*multiplier);
	SYSLOG_DEBUG("multiplier = %d, max_cmgl_resp_size = %ld", multiplier, max_cmgl_resp_size);
	response = __alloc(max_cmgl_resp_size);
	__goToErrorIfFalse(response)

	if (numeric_cmgl_index) {
		SYSLOG_DEBUG("send AT+CMGL=4 command");
		stat = at_send_with_timeout("AT+CMGL=4", response, "", &fOK, 3, max_cmgl_resp_size);
	} else {
		SYSLOG_DEBUG("send AT+CMGL=\"ALL\" command");
		stat = at_send_with_timeout("AT+CMGL=\"ALL\"", response, "", &fOK, 3, max_cmgl_resp_size);
	}
	if (stat < 0 || !fOK) {
		if (stat == -255 && inc_cnt < MAX_INCREASEMENT_LIMIT){
			multiplier++;inc_cnt++;
			__free(response);
			goto retry_with_increased_size;
		} else {
			__goToError()
		}
	}

	pATRes = response;
	pToken = strtok_r(pATRes, "\n", &pSavePtr);
	while (pToken) {
		/* count-up all read/unread message */
		if (strstr(pToken, "+CMGL:") != 0) {
			pToken += strlen("+CMGL: ");
			pToken2 = strtok_r(pToken, ",", &pSavePtr2);
			if (!pToken2) {
				SYSLOG_ERR("failed to parse msg index");
				goto get_next_token;
			}
			msg_idx = (sms_msg_type)atoi(pToken2);
			if (pdu_mode_sms) {
				pToken = strtok_r(NULL, ",", &pSavePtr2);
				msg_type = (sms_msg_type)atoi(pToken);
			}

#ifdef USE_SIM_AS_SMS_STORAGE
			if ( (!pdu_mode_sms && strstr(pSavePtr2, "REC UNREAD") != 0) ||
				(pdu_mode_sms && msg_type == REC_UNREAD)	) {
#else
			if ( (!pdu_mode_sms && (strstr(pSavePtr2, "REC UNREAD") != 0 || strstr(pSavePtr2, "REC READ") != 0)) ||
				(pdu_mode_sms && (msg_type == REC_UNREAD || msg_type == REC_READ))) {
#endif
				unread_cnt++;
				if (first_index == -1) {
					first_index = msg_idx;
					SYSLOG_ERR("first unread index = %d", first_index);
				}
			}
		}
get_next_token:
		pToken = strtok_r(NULL, "\n", &pSavePtr);
	}
	/* TODO : If total count is needed, implement a function to read memory status later */
	sprintf(tmp_str, "Total:xxx read:x unread:%d sent:x unsent:x", unread_cnt);
	if (unread_cnt)
		SYSLOG_NOTICE("sms status : %s", tmp_str);
	//rdb_set_single(rdb_name(RDB_SMS_STATUS, ""), tmp_str);
	__free(response);
	return first_index;
error:
	__free(response);
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
/* concatenate all partial msg files if received all */
static BOOL concatenate_partial_msg_if_ready(int ref_no, int msg_no)
{
	int i;
	FILE *fpw, *fpr;
	struct stat buffer;
	char partial_msg_file[BSIZE_128] = {0,};
	int c;

	for (i = 1; i <= msg_no; i++) {
		snprintf(partial_msg_file, BSIZE_128, "%s/partmsg_%d_%d_%d", smstools_spool_inbox_path, ref_no, msg_no, i);
		fpr = fopen(partial_msg_file, "r");
		if (!fpr) {
			SYSLOG_DEBUG("partial msg file %s does not exist, quit", partial_msg_file);
			return FALSE;
		} else {
			SYSLOG_DEBUG("partial msg file %s found", partial_msg_file);
		}
		fclose(fpr);
	}

	SYSLOG_INFO("-----------------------------------------------------------------------");
	SYSLOG_NOTICE("All %d partial msg files found, begin concatenation...", msg_no);
	SYSLOG_INFO("-----------------------------------------------------------------------");

	fpw = create_new_rxmsg_file(0);
	if (!fpw) {
		SYSLOG_ERR("failed to create new msg file");
		return FALSE;
	}

	for (i = 1; i <= msg_no; i++) {
		sprintf(partial_msg_file, "%s/partmsg_%d_%d_%d", smstools_spool_inbox_path, ref_no, msg_no, i);
		fpr = fopen(partial_msg_file, "r");
		if (!fpr) {
			SYSLOG_ERR("failed to open partial msg file %s", partial_msg_file);
			fclose(fpw);
			return FALSE;
		}

		while( ( c = fgetc(fpr) ) != EOF )
			fputc(c, fpw);
		fclose(fpr);

		if(remove((const char *)partial_msg_file) < 0) {
			SYSLOG_ERR("failed to delete partial msg file %s", partial_msg_file);
			continue;
		}
	}
	fclose(fpw);
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
int readOneUnreadMsg(int index)
{
	int stat, fOK, i, msg_len = 0;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	unsigned char* write_line = __alloc(AT_RESPONSE_MAX_SIZE);
	char command[BSIZE_32];
	const char* pToken;
	char *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	char* tmp_str = alloca(BSIZE_32);
	char* tmp_str2 = alloca(BSIZE_32);
	BOOL is_voice_mail_noti = FALSE;
	BOOL from_mbdn = FALSE;
	int dcs = -1;
	char achDestNo[BSIZE_32];
	char achMBDN[BSIZE_32];
	char achNetworkName[BSIZE_32];
	FILE *fp = 0;
	struct sms_dcs tmp_dcs;
	sms_encoding_type coding_type;
	struct log_sms_message* pLSM = 0;
	struct tm* ts = 0;
	char *tmp_str3;
	char* pDecodeMsg = __alloc(MAX_UNICODE_BUF_SIZE);
	int cbDecodeMsg = 0;
	struct sms_deliver_type *pFO = NULL;
	char FO;
	char* pUdhDecodeMsg = __alloc(MAX_UTF8_BUF_SIZE);
	int cbUdhDecodeMsg;
	tpudhi_field_type tphdhi = UDH_NON_EXIST;
	tpmms_field_type tpmms = NO_MORE_MSG;
	int result = -1;
	BOOL alpha_numeric_dest = FALSE;
	char partial_msg_file[BSIZE_128] = {0,};

#if (defined ZERO_LEN_PDU_MSG_TEST) || (defined LONG_PDU_MSG_TEST) || (defined ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST)
	unsigned char* temp = __alloc(AT_RESPONSE_MAX_SIZE);
#endif

	__goToErrorIfFalse(response)
	__goToErrorIfFalse(write_line)
	__goToErrorIfFalse(pDecodeMsg)
	__goToErrorIfFalse(pUdhDecodeMsg)
#if (defined ZERO_LEN_PDU_MSG_TEST) || (defined LONG_PDU_MSG_TEST) || (defined ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST)
	__goToErrorIfFalse(temp)
#endif

	if (index < 0) {
		SYSLOG_ERR("ignore invalid index (index=%d)", index);
		goto return_wo_delete;
	}

	SYSLOG_NOTICE("read a unread message (index=%d)", index);

	/* read a message */
	sprintf(command, "AT+CMGR=%d", index);
	stat = at_send(command, response, "", &fOK, 0);

	if (stat < 0 || !fOK) {
		SYSLOG_ERR("%s command failed, Can not read a SMS message", command);
		__goToError()
	}

	if (strlen(response) == 0) {
		SYSLOG_ERR("failed to read sms");
		__goToError()
	}

	fp = create_new_rxmsg_file(index);
	if (!fp)
		goto return_wo_delete;	/* do not delete msg to try later */

	rdb_set_single_int(rdb_name(RDB_SMS_CMD_ID, ""), index);

	if (pdu_mode_sms) {
		/* Ericsson : +CMGR: 3,: 3,,112<cr/lf>07911614786007F001040A814013511
         * 477000864006B003100320033....
         */
		/* Others   : +CMGR: 1,,43<cr/lf>07911614786007F0040B911634484048F100
         * 11116082511083041BF3F61C442FCFE920B3FCDD06D972301C48068BC56036190E
         */
		pATRes = response;
		if (strstr(pATRes, "+CMGR:") == 0 ) {
			SYSLOG_ERR("missing +CMGR command response");
			__goToError()
		}
		pToken = strtok_r(pATRes, "\n", &pSavePtr);
		if (!pToken || strlen(pToken) == 0 || strcmp(pToken, "OK") == 0 || strstr(pToken, "+CMGR:") == 0 ) {
			SYSLOG_ERR("+CMGR command response parsing error");
			__goToError()
		}

#ifdef ZERO_LEN_PDU_MSG_TEST
		strcpy(temp, "07911356048801802409B913000012F400032101211000208000");
		SYSLOG_ERR("TEST MSG: %s", temp);
		pSavePtr = temp;
#endif
#ifdef LONG_PDU_MSG_TEST
		// Rxed on Cinterion
		strcpy(temp, "07911614786007F0440B911634905651F2000021117261759044A0050"
                     "003E60301E8E5391D442FCFE9207A794E07D1CB733A885E9ED341F4F2"
                     "9C0EA297E77410BD3CA783E8E5391D442FCFE9207A794E07D1CB733A8"
                     "85E9ED341F4F29C0EA297E77");
		// Rxed on Sierra
		//strcpy(temp, "07911614786007F0440B911634905651F2000021117261346444A00"
        //             "50003E30301E8E5391D442FCFE9207A794E07D1CB733A885E9ED341"
        //             "F4F29C0EA297E77410BD3CA783E8E5391D442FCFE9207A794E07D1C"
        //             "B733A885E9ED341F4F29C0EA297E77410BD3CA783E8E5391D442FCF"
        //             "E9207A794E07D1CB733A885E9ED341F4F29C0EA297E77410BD3CA78"
        //             "3E8E5391D442FCFE9207A794E07D1CB733A885E9ED341F4F29C0EA2"
        //             "97E7");
		SYSLOG_ERR("TEST MSG: %s", temp);
		pSavePtr = temp;
#endif
#ifdef ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST
		strcpy(temp, "07911614786007F00414D0D4327B4E978789617A18000061209231640244"
					 "81D977FD642F83EAF33219549B95406F3328FFAECB4131170C761482C861"
					 "7A18C44EB7D374D03D4D4783623610399C9F83D86533DD05A2A2D37350DA"
					 "6D7E83DAE13C485");
		SYSLOG_ERR("TEST MSG: %s", temp);
		pSavePtr = temp;
#endif

		/* create logical sms message structure */
		if (!is_cinterion_cdma)
			pLSM = pduCreateLogSMSMessage((const char *)pSavePtr, SMS_DELIVER);
		else
			pLSM = pduCreateCDMALogSMSMessage((const char *)pSavePtr, SMS_DELIVER);

		if (!pLSM) {
			SYSLOG_ERR("failed to parse PDU, ignore this msg");
			dynaFree(pLSM);
			__goToError()
		}

		/* write to file with attached index & get msg length */
		if (pLSM->msg_type.smsDeliverType.mti == SMS_DELIVER ||
			pLSM->msg_type.smsDeliverType.mti == SMS_RESERVED_TYPE)
			ts = pduCreateLinuxTime(pLSM->pTimeStamp);

		/* parse message coding type from dcs */
		coding_type = parse_msg_coding_type((struct sms_dcs *)&pLSM->dcs);
		cbDecodeMsg = decode_pdu_msg_body(pLSM, (unsigned char *)pDecodeMsg, coding_type);
		if (cbDecodeMsg < 0) {
			strcpy((char *)pDecodeMsg, "MESSAGE BODY DECODING ERROR");
			dynaFree(pLSM);
			__goToError()
		}

		// let's decode alpha-numeric address from funky Telstra
		if ((pLSM->addr.pOrigAddr->bAddressType & 0xf0) == 0xd0) {
			char* pAddrDecodeMsg = __alloc(MAX_UNICODE_BUF_SIZE);
			int cbAddrDecodeMsg = 0;
			struct log_sms_message* pAddrLSM = dynaCreate(sizeof(struct log_sms_message), pduFreeLogSMSMessage);
			if (!pAddrLSM) {
				SYSLOG_ERR("failed to parse PDU, ignore this msg");
				dynaFree(pLSM);
				__goToError()
			}
			SYSLOG_DEBUG("found alpha-numeric address type, pLSM->addr.pOrigAddr->bAddressType = 0x%02x", pLSM->addr.pOrigAddr->bAddressType);
			alpha_numeric_dest = TRUE;
			(void) memset(pAddrLSM, 0x00, sizeof(struct log_sms_message));
			(void) memset(pAddrDecodeMsg, 0x00, MAX_UNICODE_BUF_SIZE);
			pAddrLSM->pRawSMSMessage = dynaCreate(pLSM->addr.pOrigAddr->cbAddrLen, 0);
			pAddrLSM->cbRawSMSMessage = pLSM->addr.pOrigAddr->cbAddrLen;
			(void) memset(pAddrLSM->pRawSMSMessage, 0x00, pAddrLSM->cbRawSMSMessage);
			pAddrLSM->pUserData = pAddrLSM->pRawSMSMessage;
			pAddrLSM->cbUserData = ((pLSM->addr.pOrigAddr->cbAddrLen/2)*8 - 7)/7 + 1;
			memcpy(pAddrLSM->pUserData, pLSM->addr.pOrigAddr->addrNumber, pAddrLSM->cbUserData);
			cbAddrDecodeMsg = decode_pdu_msg_body(pAddrLSM, (unsigned char *)pAddrDecodeMsg, coding_type);
			SYSLOG_DEBUG("cbAddrDecodeMsg = %d, pAddrDecodeMsg = %s", cbAddrDecodeMsg, pAddrDecodeMsg);
			(void) memset(tmp_str, 0x00, BSIZE_32);
			strcpy(tmp_str, pAddrDecodeMsg);
			dynaFree(pAddrLSM);
		} else {
			/* save parsed parameters to rdb variables */
			tmp_str3 = pduCreateHumanAddr(pLSM->addr.pOrigAddr, MOBILE_NUMBER);
			sprintf(tmp_str, "%s%s", (pLSM->addr.pOrigAddr->bAddressType == 145? "+":""), tmp_str3);
			dynaFree(tmp_str3);
		}
	} else {
		/* +CMGR: "REC UNREAD","+61459829101",,"11/04/05,11:45:18+40",145,4,0,25,"+61418706700",145,31 <cr>data */
		pATRes=response;
		pATRes += strlen("+CMGR:");

		/* get first token & skip the first token */
		pToken = strtok_r(pATRes, ",", &pSavePtr);
		if (!pToken)
			SYSLOG_ERR("got +CMGR but failed to get tokens");

		/* get the 2nd token : sender */
		(void) memset(tmp_str, 0x00, BSIZE_32);
		pToken = strtok_r(NULL, ",", &pSavePtr);
		pToken++;
		if (!pToken) {
			SYSLOG_ERR("failed to get sms sender");
		} else {
			(void) strncpy(tmp_str, pToken, strlen(pToken)-1);
		}
	}

	rdb_set_single(rdb_name(RDB_SMS_RD_DST, ""), tmp_str);
	if (strlen(tmp_str))
		fprintf(fp, "From: %s\n", tmp_str);
	else
		fprintf(fp, "From: \n");
	int dest_len=strlen(tmp_str);
	(void) memset((char *)&achDestNo[0], 0x00, BSIZE_32);
	if (!alpha_numeric_dest) {
		/* save last 10-digit number only which will be used to compare with MBDN/MBN later */
		if (dest_len >=10)
			tmp_str+=(dest_len-10);
	}
	(void) strcpy((char *)&achDestNo[0], tmp_str);
	(void) memset(tmp_str, 0x00, BSIZE_32);

	/* for Quanta modem */
	if(rdb_get_single(rdb_name(RDB_NETWORKNAME, "unencoded"), (char *)&achNetworkName[0], BSIZE_32)>=0)
		(void) rdb_get_single(rdb_name(RDB_NETWORKNAME, ""), (char *)&achNetworkName[0], BSIZE_32);
	(void) rdb_get_single(rdb_name(RDB_SIM_DATA_MBDN, ""), tmp_str, BSIZE_32);
	int mbdn_len=strlen(tmp_str);
	(void) memset((char *)&achMBDN[0], 0x00, BSIZE_32);
	if (mbdn_len >=10)
		tmp_str+=(mbdn_len-10);
	(void) strcpy((char *)&achMBDN[0], tmp_str);
	if (strcmp((const char *)&achDestNo[0], (const char *)&achMBDN[0]) == 0) {
		from_mbdn = TRUE;
		SYSLOG_INFO("MBDN(%s) and dest no(%s) are matching", achMBDN, achDestNo);
	} else {
		(void) memset(tmp_str, 0x00, BSIZE_32);
		(void) rdb_get_single(rdb_name(RDB_SIM_DATA_MBN, ""), tmp_str, BSIZE_32);
		mbdn_len=strlen(tmp_str);
		(void) memset((char *)&achMBDN[0], 0x00, BSIZE_32);
		if (mbdn_len >=10)
			tmp_str+=(mbdn_len-10);
		(void) strcpy((char *)&achMBDN[0], tmp_str);
		if (strcmp((const char *)&achDestNo[0], (const char *)&achMBDN[0]) == 0) {
			from_mbdn = TRUE;
			SYSLOG_INFO("MBN(%s) and dest no(%s) are matching", achMBDN, achDestNo);
		}
	}

	if (( strcmp((const char *)&achDestNo[0], "101") == 0 &&
			strstr((const char *)&achNetworkName[0], "Telstra") != 0) ||
			( strcmp((const char *)&achDestNo[0], "321") == 0 &&
			strstr((const char *)&achNetworkName[0], "Optus") != 0)) {
		is_voice_mail_noti = TRUE;
		SYSLOG_INFO("Found voice mail noti msg : %s, %s", achNetworkName, achDestNo);
	} else
		SYSLOG_INFO("Found none-voice mail noti msg : %s, %s", achNetworkName, achDestNo);

	if (pdu_mode_sms) {
		/* service center time stamp */
		if (ts) {
			sprintf(tmp_str, "%04d/%02d/%02d,%02d:%02d:%02d",
					ts->tm_year, ts->tm_mon, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
			rdb_set_single(rdb_name(RDB_SMS_RD_TIME, ""), tmp_str);
		} else {
			rdb_set_single(rdb_name(RDB_SMS_RD_TIME, ""), "");
		}
		/* save current time stamp as received time */
		if (SaveSmsTimeStamp(index) < 0)
			SYSLOG_ERR("failed to save current time stamp for index [%d]", index);

		/* DCS field */
		dcs = pduConvDcsToInt(&pLSM->dcs);

		/* SMSC field */
		tmp_str3 = pduCreateHumanAddr(pLSM->pSMSCNumber, SMSC_NUMBER);
		sprintf(tmp_str, "%s%s", (pLSM->pSMSCNumber->bAddressType == 145? "+":""), tmp_str3);
		dynaFree(tmp_str3);
	} else {
		/* get the 3rd token & skip it & get the 4th token : service center time stamp */
		(void) memset(tmp_str2, 0x00, BSIZE_32);
		pToken = strtok_r(NULL, "\n", &pSavePtr);
		pToken+=2;
		if (!pToken) {
			SYSLOG_ERR("failed to get sms service center time stamp");
		} else {
			(void) strncpy(tmp_str2, pToken, 17);
			sprintf(tmp_str, "20%s", tmp_str2);
		}
		rdb_set_single(rdb_name(RDB_SMS_RD_TIME, ""), tmp_str);

		/* save current time stamp as received time */
		if (SaveSmsTimeStamp(index) < 0)
			SYSLOG_ERR("failed to save current time stamp for index [%d]", index);

		/* read DCS field */
		pToken2 = strtok_r((char *)pToken, ",", &pSavePtr2);

		if(!is_cinterion_cdma)
		{
			/* skip until <fo> field */
			for (i = 0; i< 3; i++)
				pToken2 = strtok_r(NULL, ",", &pSavePtr2);
			/* read <fo> and parse
			 * bit 6 : UDHI : 0 = no UDH, 1 = UDH
			 * bit 2 : MMS  : 0 = more msg, 1 = no more msg
			 */
			FO = (char)(pToken2? atoi(pToken2):0);
		}
		else
		{
			pToken2 = strtok_r(NULL, ",", &pSavePtr2);
			FO = (char)4;
		}

		pFO = (struct sms_deliver_type *)&FO;
		SYSLOG_ERR("<fo> : 0x%02x, %s, %s", FO,
			(pFO->udhi == UDH_EXIST? "UDH exist":"UDH not exist"),
			(pFO->mms == MORE_MSG? "more msg":"no more msg"));

		if(!is_cinterion_cdma)
		{
			/* read DCS and parse */
			for (i = 0; i< 2; i++)
				pToken2 = strtok_r(NULL, ",", &pSavePtr2);
			SYSLOG_ERR("DCS field = %s", pToken2);
			dcs = (char)(pToken2? atoi(pToken2):0);
		}
		else
			dcs = (char)0;


		if(!is_cinterion_cdma)
		{
			/* read SMSC field */
			(void) memset(tmp_str, 0x00, BSIZE_32);
			pToken2 = strtok_r(NULL, ",", &pSavePtr2);
			pToken2++;
			if (!pToken2)
				SYSLOG_ERR("failed to get SMSC");
			else
				(void) strncpy(tmp_str, pToken2, strlen(pToken2)-1);
		}
		else
			tmp_str[0] = 0;
	}
	rdb_set_single(rdb_name(RDB_SMS_RD_SMSC, ""), tmp_str);
	if (strlen(tmp_str))
		fprintf(fp, "From_SMSC: %s\n", tmp_str);
	else
		fprintf(fp, "From_SMSC: \n");

	/* write time stamp */
	(void) memset(tmp_str, 0x00, BSIZE_32);
	(void) rdb_get_single(rdb_name(RDB_SMS_RD_TIME, ""), tmp_str, BSIZE_32);
	if (strlen(tmp_str))
		fprintf(fp, "Sent: %s\n", tmp_str);
	else
		fprintf(fp, "Sent: \n");
	fprintf(fp, "Subject: GSM1\n");
	fprintf(fp, "Alphabet:  ISO\n");
	fprintf(fp, "UDH: false\n\n");

	/* read message line */
	tmp_dcs.cmd = (dcs & 0xf0) >> 4;
	tmp_dcs.param = (dcs & 0x0f);
	coding_type = parse_msg_coding_type(&tmp_dcs);

	(void) memset(write_line, 0x00, AT_RESPONSE_MAX_SIZE);
	if (pdu_mode_sms) {
		/* message body */
		sprintf((char *)write_line, "%s", ((coding_type == DCS_UCS2_CODING)? "UCS2:":
										  (coding_type == DCS_7BIT_CODING)? "GSM7:":"8BIT:"));
		if (cbDecodeMsg > 0) {
			(void) memcpy((char *)&write_line[5], pDecodeMsg, cbDecodeMsg);
		}
		SYSLOG_DEBUG("decoded UTF-8 msg: %s", pDecodeMsg);
	} else {
		/* read message length */
		for (i = 0; i< 2; i++)
			pToken2 = strtok_r(NULL, ",", &pSavePtr2);
		if (!pToken2) {
			SYSLOG_ERR("can not retrieve msg length");
			msg_len = 0;
		} else {
			msg_len = atoi(pToken2);
		}

		// for test
		//msg_len = 0;
		//pSavePtr = NULL;
		//coding_type = DCS_UCS2_CODING;

		if (coding_type == DCS_UCS2_CODING) {
			msg_len *= 4;
		}
		SYSLOG_DEBUG("msg_len = %d, msg_body = %s", msg_len, pSavePtr);

		if (msg_len)
			printMsgBody(pSavePtr, msg_len);

		/* read message line */
		if (coding_type == DCS_7BIT_CODING)	{			/* GSM 7 bit coding */
			SYSLOG_DEBUG("detect GSM 7 bit encoded msg");
			/* If UDH field is exist then the message body is two IRA long hexa
			 * decimal number even though DCS field indicates GSM 7bit coding.
			 * Need to convert to GSM 7bit hexa message in this case. */
			if (pFO->udhi == UDH_EXIST) {
				/* Msg length field represents real GSM 7 bit message length so
			 	 * IRA string length is two times of this field value */
				cbUdhDecodeMsg = ConvStrToChar(pSavePtr, msg_len*2, pUdhDecodeMsg, msg_len);
				cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7(pUdhDecodeMsg, pDecodeMsg, msg_len);
			} else {
				cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7(pSavePtr, pDecodeMsg, msg_len);
			}
			if(cbDecodeMsg < 0) {
				SYSLOG_ERR("fail to decode Utf8 from GSM7");
				dynaFree(pLSM);
				dynaFree(ts);
				__goToError()
			}
			//strcpy(pDecodeMsg, "test msg");cbDecodeMsg=strlen("test msg");
			sprintf((char *)write_line, "GSM7:");
			/* 3G/4G modules return parsed output so just use that result without decoding
			 * except that UDH field exists. */
			if (pFO->udhi == UDH_EXIST) {
				(void) memcpy((char *)&write_line[5], pDecodeMsg, cbDecodeMsg);
				SYSLOG_DEBUG("decoded UTF-8 msg: %s", pDecodeMsg);
			} else {
				(void) memcpy((char *)&write_line[5], pSavePtr, msg_len);
				SYSLOG_DEBUG("decoded UTF-8 msg: %s", pSavePtr);
			}
		} else if (coding_type == DCS_UCS2_CODING) {	/* UCS 2 coding */
			SYSLOG_DEBUG("detect UCS 2 encoded msg");
			if (!pSavePtr || strlen(pSavePtr) == 0) {
				SYSLOG_ERR("msg body is NULL, ");
				sprintf((char *)write_line, "UCS2:");
			} else {
				cbDecodeMsg = smsrecv_decodeUtf8FromUnicodeStr(pSavePtr, pDecodeMsg, msg_len);
				if(cbDecodeMsg < 0) {
					SYSLOG_ERR("fail to decode Utf8 from Unicode");
					dynaFree(pLSM);
					dynaFree(ts);
					__goToError()
				}
				sprintf((char *)write_line, "UCS2:");
				(void) memcpy((char *)&write_line[5], pDecodeMsg, cbDecodeMsg);
				SYSLOG_DEBUG("decoded UTF-8 msg: %s", pDecodeMsg);
			}
		} else {										// text mode 8 bit is not supported yet
			SYSLOG_NOTICE("found 8 bit or binary coding scheme : 0x%2x", dcs);
			sprintf((char *)write_line, "GSM7:%s", pSavePtr);
		}
	}
	rdb_set_single(rdb_name(RDB_SMS_RD_MSG, ""), (char *)write_line);
	fprintf(fp, "%s", (char *)write_line);

	/* check voice mail notification */
	if (from_mbdn || is_voice_mail_noti || (dcs >= 0xc0 && dcs <= 0xef)) {
		/* check if dcs field indicates msg waiting group */
		if (dcs >= 0xc0 && dcs <= 0xef) {		/* message waiting group */
			SYSLOG_NOTICE("found message waiting indicator");
			if (dcs & 0x08)
				rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "active");
			else
				rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "inactive");
		} else if (from_mbdn) {
			/* Telus send empty body SMS when all voice mail deleted. */
			if (msg_len > 0)
				rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "active");
			else
				rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "inactive");
		}

		dynaFree(pLSM);
		dynaFree(ts);

		/* delete voice mail from memory */
		deleteSmsMessage(index);

		fclose(fp);
		fp = 0;
		delete_new_rxmsg_file();

		rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "calls.event"), "ring");
		result = 0;
		goto return_wo_delete;
	}

	fclose(fp);
	fp = 0;

#ifndef USE_SIM_AS_SMS_STORAGE
	deleteSmsMessage(index);
#endif

	/* Process partial messages that may not arrive in sequence so need to wait until
	 * receiving all partial messages and combine them.
	 * 1. for 1st concatenated message : Let it go with header information
	 *                               and message body with coding method tag.
	 *                               Rename msg file name to new partial msg name
	 *                               Return -255 to not notify new message arrival yet.
	 * 2. for mid/last concatenated messages: Rename msg file name to new partial msg name and clear
	 *                               then write message body to it without coding method tag.
	 *                               Return -255 to not notify new message arrival yet.
	 * 3. when receiving all partial messages
	 *                               Create new msg file and combine all partial message files to it
	 *                               then delete all partial message files and notify new message arrival now.
	 */
	if ((pdu_mode_sms && pLSM->msg_type.smsDeliverType.udhi == UDH_EXIST) ||
		(!pdu_mode_sms && pFO && pFO->udhi == UDH_EXIST)) {
		tphdhi = UDH_EXIST;
	}
	/* Sometimes smsDeliverType.mms does not indicate properly, in that case udh field should be referred. */
	if ((pdu_mode_sms && (pLSM->msg_type.smsDeliverType.mms == MORE_MSG || pLSM->udh.seq_no < pLSM->udh.msg_no)) ||
	    (!pdu_mode_sms && pFO && pFO->mms == MORE_MSG)) {
	    tpmms = MORE_MSG;
	}
	SYSLOG_NOTICE("<fo> : %s, %s", (tphdhi == UDH_EXIST? "UDH exist":"UDH not exist"),
		(tpmms == MORE_MSG? "more msg":"no more msg"));

	if (tphdhi == UDH_EXIST) {

		/* form partial msg file name */
		SYSLOG_NOTICE("found partial msg: ref no = %d, %d of %d", pLSM->udh.ref_no, pLSM->udh.seq_no, pLSM->udh.msg_no);
		snprintf(partial_msg_file, BSIZE_128, "%s/partmsg_%d_%d_%d", smstools_spool_inbox_path, pLSM->udh.ref_no, pLSM->udh.msg_no, pLSM->udh.seq_no);
		result = -255;

		/* Rename 1st msg file which contains header information to new partial msg name */
		if (pLSM->udh.seq_no == 1) {
			SYSLOG_DEBUG("renaming %s --> %s", new_msg_file, partial_msg_file);
			if (rename((const char *)&new_msg_file, (const char *)&partial_msg_file) != 0) {
				SYSLOG_ERR("failed to renaming...");
				goto return_wo_delete;
			}
		}
		/* Write message body to partial msg file */
		else {
			delete_new_rxmsg_file();
			fp = fopen(partial_msg_file, "w");
			if(!fp)	{
				SYSLOG_ERR("failed to open partial msg file %s", partial_msg_file);
				goto return_wo_delete;
			}
			if (!strncmp((char *)write_line, "GSM7:", 5) ||
				!strncmp((char *)write_line, "8BIT:", 5) ||
				!strncmp((char *)write_line, "UCS2:", 5)) {
				fprintf(fp, "%s", (char *)&write_line[5]);
			} else {
				fprintf(fp, "%s", (char *)write_line);
			}
			fclose(fp);
			fp = 0;
		}

		/* concatenate all partial msg files if received all */
		if (concatenate_partial_msg_if_ready(pLSM->udh.ref_no, pLSM->udh.msg_no)) {
			result = 0;
		}
	}

error:
#ifndef USE_SIM_AS_SMS_STORAGE
	deleteSmsMessage(index);
#endif
return_wo_delete:
	dynaFree(pLSM);
	dynaFree(ts);

	if (fp > 0) fclose(fp);
#if (defined ZERO_LEN_PDU_MSG_TEST) || (defined LONG_PDU_MSG_TEST) || (defined ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST)
	__free(temp);
#endif
	__free(pUdhDecodeMsg);
	__free(pDecodeMsg);
	__free(write_line);
	__free(response);
	return result;
}
///////////////////////////////////////////////////////////////////////////////
/* process '+CMTI: "SM",0' notification */
BOOL processing_rxmsg = FALSE;
int handleNewSMSIndicator(const char* s)
{
	char* pNewS = strdup(s);
	int index;
	const char* pToken;
	char* pSavePtr;
	char* pATRes = pNewS;
	int result;

	if (sms_disabled) {
		SYSLOG_ERR("SMS feature is disabled. check appl options");
		goto error;
	}

	if (!pNewS)	{
		SYSLOG_ERR("failed to allocate memory in %s()", __func__);
		goto error;
	}

	SYSLOG_NOTICE("--------------------------------------------------------");

	if(processing_rxmsg) {
		SYSLOG_NOTICE("*** processing previous rx msg, returning from %s()", __func__);
		goto error;
	}

	processing_rxmsg = TRUE;

	/* +CMTI: "SM",5 */
	pATRes += strlen("+CMTI:");

	/* get first token & skip the first token */
	pToken = strtok_r(pATRes, ",", &pSavePtr);
	if (!pToken)
		SYSLOG_ERR("got +CMTI but failed to get tokens");

	/* get the 2nd token */
	pToken = strtok_r(NULL, ",", &pSavePtr);
	if (!pToken) {
		SYSLOG_ERR("failed to get sms index");
		goto error;
	}
	index = atoi(pToken);
	if (pNewS)
		free(pNewS);

	SET_LOG_LEVEL_TO_SMS_LOGMASK_LEVEL

	result = readOneUnreadMsg(index);
	/* to invoke sms template */
	if (result == 0)
		rdb_set_single(rdb_name(RDB_SMS_STATUS, ""), "Total:xxx read:x unread:1 sent:x unsent:x");
#ifdef USE_SIM_AS_SMS_STORAGE
	/* do not update storage status if current message is not last one */
	if (result != -255) {
		/* update storage status */
		(void)UpdateSmsMemStatus(1);
		//rdb_set_single(rdb_name(RDB_SMS_STATUS, ""), "[done] newmsg");
	}
#endif

	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL

	SYSLOG_NOTICE("--------------------------------------------------------");
	processing_rxmsg = FALSE;
	return result;

error:
	if (pNewS)
		free(pNewS);
	processing_rxmsg = FALSE;
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int polling_rx_sms_event(void)
{
	int next_msg_idx, result = -1;
	DIR *dir, *first_dir;
	struct dirent *dir_rent;
	FILE* sms_file;
	unsigned char *str = __alloc(BSIZE_1024);
	if (!str)
		goto ret_result;

	if(processing_rxmsg) {
		SYSLOG_NOTICE("*** processing previous rx msg, returning from %s()", __func__);
		goto ret_result;
	}

	if (rdb_get_single(rdb_name(RDB_SIM_STATUS, ""), (char *)str, BSIZE_32) != 0) {
		SYSLOG_INFO("failed to read storage status, skip AT+CMGL command");
		goto ret_result;
	}

	if (strcmp((char *)str, "SIM OK") != 0) {
		//SYSLOG_ERR("SIM is not ready, skip AT+CMGL command");
		goto ret_result;
	}

	processing_rxmsg = TRUE;
	next_msg_idx = GetFirstUnreadMsgIdx();

	if (next_msg_idx >= 0) {
		SYSLOG_NOTICE("--------------------------------------------------------");
		SET_LOG_LEVEL_TO_SMS_LOGMASK_LEVEL
		while (next_msg_idx >= 0) {
			result = readOneUnreadMsg(next_msg_idx);
			next_msg_idx = GetFirstUnreadMsgIdx();
		}
		/* to invoke sms template */
		if (result == 0)
			rdb_set_single(rdb_name(RDB_SMS_STATUS, ""), "Total:xxx read:x unread:1 sent:x unsent:x");
#ifdef USE_SIM_AS_SMS_STORAGE
		/* do not update storage status if current message is not last one */
		if (result != -255) {
			/* update storage status */
			(void)UpdateSmsMemStatus(1);
			//rdb_set_single(rdb_name(RDB_SMS_STATUS, ""), "[done] newmsg");
		}
#endif
		SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
		SYSLOG_NOTICE("--------------------------------------------------------");
	} else {
		if ((dir = opendir((const char *)&smstools_spool_inbox_path))== NULL) {
			processing_rxmsg = FALSE;
			goto ret_result;
		}
		first_dir = dir;
		//SYSLOG_ERR("open dirp = %p", first_dir);
		result = -1;
		while ((dir_rent = readdir(dir)) != NULL) {
			if (strcmp(dir_rent->d_name, ".") == 0 ||
			   strcmp(dir_rent->d_name, "..") == 0 ||
			   strncmp(dir_rent->d_name, "partmsg", 7) == 0) {
				continue;
			} else {
				sprintf((char *)str, "%s/%s",smstools_spool_inbox_path,dir_rent->d_name);
				sms_file = (FILE *)open((char *)str, O_RDONLY);
				if (sms_file > 0) {
					close((int)sms_file);
					if (result != -255)
						result = 0;
				}
			}
		}
		//SYSLOG_ERR("close dirp = %p, curr dirp = %p", first_dir, dir);
		/* Should close dirp returned by opendir because this dirp changes after calling readdir */
		closedir(first_dir);
		if (result == 0)
			rdb_set_single(rdb_name(RDB_SMS_STATUS, ""), "Total:xxx read:x unread:1 sent:x unsent:x");
	}
ret_result:
	processing_rxmsg = FALSE;
	__free(str);
	return result;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum {
	SEND_USING_CMGW_CMD = 0,	/* using CMGW cmd storing msg to SMS storage */
	SEND_USING_CMGS_CMD,		/* using CMGS cmd used for Avian Platform to use UCS2 encoding */
	SEND_DIAG_MSG			/* using CMGS cmd used for Diagnostic response tx */
} tx_mode_enum_type;
/*
 * max pEncodeMsg len = max pMsg len * sizeof(uint)
 * pMsg : UTF-8. max length = 4 bytes/char in worst case
 * pEncodeMsg : GSM 7bits(Hex). max length = 2 bytes/char = pMsg len * 2
 *              UCS2(IRA). max length = 4 bytes/char = pMsg len * 4
 */
static int smssend_encodeMsg(char* pMsg, int cbpMsg, char* pEncodeMsg, int cbpEncodeMsg, int* ucs2mode)
{
	unsigned char nibble;
	int i, j;
	unsigned int* ptmpMsg = __alloc(cbpEncodeMsg);
	int cbTmpMsg = 0, cbEncodeMsg = 0;
	unsigned int uc;
	__goToErrorIfFalse(ptmpMsg)

	SYSLOG_DEBUG("smstools_encoding_scheme = %d, ucs2mode = %d", smstools_encoding_scheme, *ucs2mode);
	if (smstools_encoding_scheme == DCS_UCS2_CODING || *ucs2mode)
		SYSLOG_NOTICE("UCS2 mode, encoding to UCS2");
	else
		SYSLOG_NOTICE("GSM7 mode, encoding to Unicode then encoding to GSM7");

	cbTmpMsg = smssend_encodeUnicodeFromUtf8(pMsg, cbpMsg, ptmpMsg, cbpEncodeMsg);

	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);

	if (smstools_encoding_scheme == DCS_UCS2_CODING || *ucs2mode) {
		for (i = 0; i < cbTmpMsg; i++) {
			uc = ptmpMsg[i];
			for (j = 0; j < 4; j++, pEncodeMsg++, cbEncodeMsg++) {
				nibble = (uc & (0xf000 >> (j*4))) >> ((4-j-1)*4);
				*pEncodeMsg = (nibble <= 9)? ('0'+nibble):('A'+nibble-0x0a);
			}
		}
		goto ret_size;
	} else {
		cbEncodeMsg = smssend_encodeGsmBit7FromUnicode(ptmpMsg, pEncodeMsg, cbTmpMsg);
		printMsgBody(pEncodeMsg, cbEncodeMsg);
		goto ret_size;
	}
ret_size:
	__free(ptmpMsg);
	return cbEncodeMsg;
error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static int smssend_encodeCDMAMsg(char* pMsg, int cbpMsg, char* pEncodeMsg, int cbpEncodeMsg, int* ucs2mode)
{
	unsigned char nibble;
	int i, j;
	unsigned int* ptmpMsg = __alloc(cbpEncodeMsg);
	int cbTmpMsg = 0, cbEncodeMsg = 0;
	unsigned int uc;

	__goToErrorIfFalse(ptmpMsg)

	SYSLOG_DEBUG("smstools_encoding_scheme = %d, ucs2mode = %d", smstools_encoding_scheme, *ucs2mode);
	if (smstools_encoding_scheme == DCS_UCS2_CODING || *ucs2mode)
		SYSLOG_NOTICE("UCS2 mode, encoding to UCS2");
	else
		SYSLOG_NOTICE("ASCII7 mode, encoding to Unicode then encoding to ASCII7");

	cbTmpMsg = smssend_encodeUnicodeFromUtf8(pMsg, cbpMsg, ptmpMsg, cbpEncodeMsg);

	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);

	if (smstools_encoding_scheme == DCS_UCS2_CODING || *ucs2mode) {
		for (i = 0; i < cbTmpMsg; i++) {
			uc = ptmpMsg[i];
			for (j = 0; j < 4; j++, pEncodeMsg++, cbEncodeMsg++) {
				nibble = (uc & (0xf000 >> (j*4))) >> ((4-j-1)*4);
				*pEncodeMsg = (nibble <= 9)? ('0'+nibble):('A'+nibble-0x0a);
			}
		}
		goto ret_size;
	} else {
		cbEncodeMsg = smssend_encodeASCIIFromUnicode(ptmpMsg, pEncodeMsg, cbTmpMsg);
		printMsgBody(pEncodeMsg, cbEncodeMsg);
		goto ret_size;
	}
ret_size:
	__free(ptmpMsg);
	return cbEncodeMsg;
error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
#define RDB_VAR_SMS_ROUTING_OPTION  "smstools.conf.mo_service"
static int smssend_configure(int* ucs2mode)
{
	int stat, fOK, need_reconfigure = 0;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	BOOL support_gsm7_mode = FALSE;
	BOOL support_ira_mode = FALSE;
	char encode_mode[10] = "";
	char command[20] = "";
	char tempstr[BSIZE_32];
	int routing_option;
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
#endif
	__goToErrorIfFalse(response)

reconfigure:
	if (need_reconfigure == 0) {
		/* determine encoding scheme depending on modem's ability */
		stat = at_send("AT+CSCS=?", response, "", &fOK, AT_RESPONSE_MAX_SIZE);
		if (stat < 0 || !fOK) {
			SYSLOG_ERR("AT+CSCS=? command failed");
			__goToError()
		}
		support_gsm7_mode=(strstr(response,"GSM") != 0);
		support_ira_mode=(strstr(response,"IRA") != 0);     /* use IRA for UCS2 mode  */
		if(!support_gsm7_mode && !support_ira_mode) {
			SYSLOG_ERR("Module does not support any possible character scheme");
			__goToError()
		}
		if (smstools_encoding_scheme == DCS_UCS2_CODING || *ucs2mode || pdu_mode_sms) {
			/* At this stage, the module should support IRA because it already checked at launching time
			 * and if not supports, message text is divided into single message size in sendsms scripts.*/
			if (support_ira_mode) {
				SYSLOG_NOTICE("set character scheme to IRA for %s mode", (pdu_mode_sms? "PDU":"UCS2"));
				strcpy(encode_mode, "IRA");
			} else {
				/* Cinterion 2G module, BGS2-E does not have IRA option in +CSCS command but
				 * it seems to work in IRA mode implicitly so set to UCS2 mode here */
				if (cinterion_type == cinterion_type_2g && (smstools_encoding_scheme == DCS_UCS2_CODING || *ucs2mode)) {
					SYSLOG_NOTICE("need IRA for UCS2/PDU but not support, change to UCS2 mode for Cinterion 2G module");
					strcpy(encode_mode, "UCS2");
				} else {
					SYSLOG_NOTICE("need IRA for UCS2/PDU but module does not support so change to GSM7 mode");
					strcpy(encode_mode, "GSM");
					 *ucs2mode = 0;
					/* force encoding scheme to GSM7 for this message tx */
					smstools_encoding_scheme = DCS_7BIT_CODING;
					need_reconfigure = 1;
					goto reconfigure;
				}
			}
		} else {
			if (support_gsm7_mode) {
				SYSLOG_NOTICE("set character scheme to GSM7 mode");
				strcpy(encode_mode, "GSM");
			} else {
				SYSLOG_NOTICE("need GSM7 but module does not support GSM7 so change to IRA for UCS2/PDU mode");
				strcpy(encode_mode, "IRA");
				*ucs2mode = 1;
				need_reconfigure = 1;
				goto reconfigure;
			}
		}

		sprintf(command, "AT+CSCS=\"%s\"", encode_mode);
		stat = at_send(command, response, "", &fOK, AT_RESPONSE_MAX_SIZE);
		if (stat < 0 || !fOK) {
			SYSLOG_ERR("%s command failed", command);
			__goToError()
		}
	}

	/* +CSMP: fo, vp, pid, dcs */
	/* set to UCS-2 mode or default mode*/
	/* Ericsson modem does not support CSMP command */
	if (!is_ericsson && !is_cinterion_cdma) {
		sprintf(command, "AT+CSMP=%d,,0,%d",
				(cinterion_type == cinterion_type_2g? 17:1),
				(*ucs2mode? 8:0));
		stat = at_send(command, response, "", &fOK, AT_RESPONSE_MAX_SIZE);
		if (stat < 0 || !fOK) {
			//SYSLOG_ERR("AT+CSMP command failed, Can not change text mode");
			//__goToError()
			SYSLOG_ERR("AT+CSMP command failed, Can not change text mode but keep going");
		}
	} else {
		SYSLOG_NOTICE("skip AT+CSMP command");
	}

	/* set SMS routing option */
	if(!is_cinterion_cdma)
	{
#if defined(PLATFORM_PLATYPUS)
		nvram_init(RT2860_NVRAM);
		rd_data = nvram_bufget(RT2860_NVRAM, RDB_VAR_SMS_ROUTING_OPTION);
		if (rd_data) {
			strcpy(tempstr, rd_data);
#else
		if (rdb_get_single(RDB_VAR_SMS_ROUTING_OPTION, tempstr, BSIZE_32) == 0) {
#endif
			routing_option = atoi(tempstr);
			if (routing_option < 0 || routing_option > 3) {
				SYSLOG_ERR("invalid routing option value %d, use current setting", routing_option);
			} else {
				sprintf(command, "AT+CGSMS=%d", routing_option);
				SYSLOG_NOTICE("set SMS routing option to %s",
					routing_option==0? "Packet-switched":routing_option==1? "Circuit-switched":
					routing_option==2? "Packet-switched preferred":"Circuit-switched preferred");
				stat = at_send(command, response, "", &fOK, AT_RESPONSE_MAX_SIZE);
				if (stat < 0 || !fOK) {
					SYSLOG_ERR("%s command failed but try to send message", command);
				}
			}
		} else {
			SYSLOG_ERR("failed to read %s, use current setting", RDB_VAR_SMS_ROUTING_OPTION);
		}

#if defined(PLATFORM_PLATYPUS)
		nvram_strfree(rd_data);
		nvram_close(RT2860_NVRAM);
#endif
	}

	__free(response);
	return 0;

error:
	__free(response);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
#define RDB_VAR_SMS_SP  "sms.has_special_chars"
int check_extended_char(void)
{
	char tempstr[BSIZE_32];
	if (rdb_get_single(rdb_name(RDB_VAR_SMS_SP,""), tempstr, BSIZE_32) != 0) {
		SYSLOG_ERR("failed to read %s", RDB_VAR_SMS_SP);
		return -1;
	}
	if (strcmp(tempstr, "1") == 0)
		return 1;
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
/* Cinterion 2G module, BGS2-E responds UCS2 format address so
 * should set GSM mode before reading address and restore to previous
 * setting later. */
int char_set_save_restore(int save)
{
	int stat, fOK;
	char* response = alloca(BSIZE_64);
	static int need_to_restore = 0;
	char command[20] = "";

	stat = at_send("AT+CSCS?", response, "", &fOK, AT_RESPONSE_MAX_SIZE);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("AT+CSCS? command failed");
		return -1;
	}

	if (save) {
		need_to_restore = 0;
		if (strstr(response, "UCS2")) {
			SYSLOG_NOTICE("change to GSM mode to read SMSC address");
			need_to_restore = 1;
			sprintf(command, "AT+CSCS=\"GSM\"");
			stat = at_send(command, response, "", &fOK, AT_RESPONSE_MAX_SIZE);
			if (stat < 0 || !fOK) {
				SYSLOG_ERR("%s command failed", command);
				return -1;
			}
		}
	} else if(need_to_restore) {
		sprintf(command, "AT+CSCS=\"UCS2\"");
		stat = at_send(command, response, "", &fOK, AT_RESPONSE_MAX_SIZE);
		if (stat < 0 || !fOK) {
			SYSLOG_ERR("%s command failed", command);
			return -1;
		}
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
void get_smsc_addr(struct addr_fields* smsc_addr)
{
	int stat, fOK, int_addr = 0;
	char* response = alloca(BSIZE_64);
	const char* pToken;
	char *pSavePtr, *pATRes;
	char* command = __alloc(AT_RESPONSE_MAX_SIZE);

	/* Cinterion 2G module, BGS2-E responds UCS2 format address so
	 * should set GSM mode before reading address */
	if (cinterion_type == cinterion_type_2g && char_set_save_restore(1) < 0) {
		return;
	}

	(void) memset(smsc_addr->addrNumber, 0x00, sizeof(smsc_addr)+BSIZE_32);
	smsc_addr->cbAddrLen = 0;
	smsc_addr->bAddressType = 0x80;	/* unknown type-of-number, unknown numbering plan */

	stat = at_send("AT+CSCA?", response, "", &fOK, BSIZE_64);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("AT+CSCA? command failed");
		return;
	}
	/* parse +CSCA: "+61418706700",145 */
	pATRes = response;
	pATRes += (strlen("+CSCA: ")+1);

	/* get smsc addr */
	pToken = strtok_r(pATRes, ",", &pSavePtr);
	if (!pToken) {
		SYSLOG_ERR("got +CSCA but failed to parse smsc addr");
		return;
	}

	if (*pToken == '+') {
		pToken++;
		int_addr = 1;
	}
	smsc_addr->cbAddrLen = strlen(pToken)-1;
	strncpy(smsc_addr->addrNumber, pToken, smsc_addr->cbAddrLen);

	if (!pSavePtr) {
		SYSLOG_ERR("failed to parse smsc addr type");
		smsc_addr->bAddressType = 0x80;	/* unknown type-of-number, unknown numbering plan */
		return;
	}

	SYSLOG_NOTICE("smsc addr : '%s'", smsc_addr->addrNumber);
	smsc_addr->bAddressType = atoi(pSavePtr);

	/* Cinterion 2G module, BGS2-E responds UCS2 format address so
	 * should set GSM mode before reading address */
	if (cinterion_type == cinterion_type_2g && char_set_save_restore(0) < 0) {
		return;
	}

	return;
}
///////////////////////////////////////////////////////////////////////////////
extern BOOL sms_msg_body_command;
extern int sms_msg_body_len;
#define MAX_TX_RETRY_CNT	2
static int send_one_message(tx_mode_enum_type txmode, char *pSingleMsg, int cbpSingleMsg,
							  int need_ucs2_encode, int int_addr, char *achDestNo, int udh_len)
{
	int index = 0, retry_cnt = 0, status, fOK;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char* command = __alloc(AT_RESPONSE_MAX_SIZE);
	unsigned char* pPduData = 0;
	const char* pToken;
	char *pSavePtr, *pATRes;
	int pdu_pkt_len = 0;
	struct addr_fields* smsc_addr = alloca(sizeof(smsc_addr)+MAX_ADDR_LEN);
	int result = -1;

	__goToErrorIfFalse(response)
	__goToErrorIfFalse(command)

	if (pdu_mode_sms) {
		if(!is_cinterion_cdma)
			get_smsc_addr(smsc_addr);
		pPduData = __alloc(MAX_UNICODE_BUF_SIZE);
		__goToErrorIfFalse(pPduData)
		(void) memset(pPduData, 0x00, MAX_UNICODE_BUF_SIZE);

		if(!is_cinterion_cdma)
			pdu_pkt_len = createPduData((char *)(achDestNo[0] == '+'? &achDestNo[1]:&achDestNo[0]),
					int_addr, need_ucs2_encode, (unsigned char *)pSingleMsg,
					cbpSingleMsg, pPduData, smsc_addr, udh_len);
		else
			pdu_pkt_len = createCDMAPduData((char *)(achDestNo[0] == '+'? &achDestNo[1]:&achDestNo[0]),
					int_addr, need_ucs2_encode, (unsigned char *)pSingleMsg,
					cbpSingleMsg, pPduData, smsc_addr, udh_len);
	}

	/* send diagnostic command ack/error SMS and do not save to SMS storage */
	if (txmode != SEND_USING_CMGW_CMD) {
send_retry_for_CMGS_command:
		/* write message to memory */
		(void) memset(command, 0x00, AT_RESPONSE_MAX_SIZE);
		if (pdu_mode_sms)
			sprintf(command, "AT+CMGS=%d", pdu_pkt_len);
			//sprintf(command, "AT+CMGS=%d", 34);
		else
			sprintf(command, "AT+CMGS=\"%s\",%s", achDestNo, (int_addr? "145":"129"));
		status = at_send_and_forget(command,&fOK);	// send dest no and cr
		/* This 1 second delay is specially given for Ericsson module which shows quite slow
		** response but it is not harmful for other modules.
		*/
		sleep(1);
		(void) memset(command, 0x00, AT_RESPONSE_MAX_SIZE);
		if (pdu_mode_sms) {
			sprintf(command, "%s", pPduData);
			sms_msg_body_len = strlen((char *)pPduData) + 2;
			command[strlen(command)] = 0x1A; // ctrl-z
		} else {
			memcpy(command, pSingleMsg, cbpSingleMsg);
			sms_msg_body_len = cbpSingleMsg + 1;
			command[cbpSingleMsg] = 0x1A; // ctrl-z
		}
		printMsgBody(command, sms_msg_body_len);

		/* send SMS directly */
		/* Mark to skip CR at the end of msg body. Most 3G modules do not matter of this last
		** CR but Ericsson module does.
		** wait for response for 20 seconds (for Cinterion module)
		*/
		sms_msg_body_command = TRUE;
		status = at_send_with_timeout(command, response, "", &fOK, 20, AT_RESPONSE_MAX_SIZE);
		sms_msg_body_command = FALSE;
		if (status < 0 || !fOK) {
			SYSLOG_ERR("AT+CMGS command failed, retry cnt = %d", ++retry_cnt);
			if (retry_cnt >= MAX_TX_RETRY_CNT) {
				SYSLOG_ERR("AT+CMGS command failed %d times, Can not send message", retry_cnt);
				result = -2;	/* mark for keeping failed msg */
				goto return_result;
			} else {
				goto send_retry_for_CMGS_command;
			}
		}
		/* +CMGS: xx or +CMS ERROR : <err> */
		pATRes = response;
		SYSLOG_NOTICE("Tx result : %s", pATRes);
		if (strstr(pATRes, "CMS ERROR") != 0) {
			SYSLOG_ERR("AT+CMGS command failed, resp = %s, retry cnt = %d", response, ++retry_cnt);
			if (retry_cnt >= MAX_TX_RETRY_CNT) {
				SYSLOG_ERR("AT+CMGS command failed %d times, keep failed msg for retry", retry_cnt);
				result = -2;	/* mark for keeping failed msg */
				goto return_result;
			} else {
				goto send_retry_for_CMGS_command;
			}
		}
		if (strstr(pATRes, "+CMGS:") != 0) {
			SYSLOG_NOTICE("Tx message sent");
			result = 0;
			goto return_result;
		}
	}

	/* send normal SMS and save to SMS storage */
	else
	{
		/* write message to memory */
		(void) memset(command, 0x00, AT_RESPONSE_MAX_SIZE);
		if (pdu_mode_sms)
			sprintf(command, "AT+CMGW=%d", pdu_pkt_len);
			//sprintf(command, "AT+CMGW=%d", 26);
			//sprintf(command, "AT+CMGW=%d", 51);
		else
			sprintf(command, "AT+CMGW=\"%s\",%s", achDestNo, (int_addr? "145":"129"));
		status = at_send_and_forget(command,&fOK);	// send dest no and cr
		/* This 1 second delay is specially given for Ericsson module which shows quite slow
		** resosponse but it is not harmful for other modules.
		*/
		sleep(1);
		(void) memset(command, 0x00, AT_RESPONSE_MAX_SIZE);
		if (pdu_mode_sms) {
			sprintf(command, "%s", pPduData);
			//sprintf(command, "%s", "07911614786007F011000AA140830484140000AA0"
            //        "DF3F61CD49E9F41F4F29C0E8A01");
			//sprintf(command, "%s", "07911614786007F011000AA140952819100008AA2"
            //        "600670073006D003700200070006400750020006D006F00640065002"
            //        "0007B0073006D0073007D");
			sms_msg_body_len = strlen((char *)pPduData) + 2;
			command[strlen(command)] = 0x1A; // ctrl-z
		} else {
			memcpy(command, pSingleMsg, cbpSingleMsg);
			sms_msg_body_len = cbpSingleMsg + 1;
			command[cbpSingleMsg] = 0x1A; // ctrl-z
		}
		printMsgBody(command, sms_msg_body_len);

		/* Mark to skip CR at the end of msg body. Most 3G modules do not matter of this last
		** CR but Ericsson module does.
		** wait for response for 20 seconds (for Cinterion module)
		*/
		sms_msg_body_command = TRUE;
		status = at_send_with_timeout(command, response, "", &fOK, 20, AT_RESPONSE_MAX_SIZE);
		sms_msg_body_command = FALSE;
		if (status < 0 || !fOK) {
			SYSLOG_ERR("AT+CMGW command failed, status %d, fok %d", status, fOK);
			goto send_error;
		}
		/* +CMGW: 1 or +CMS ERROR : <err> */
		pATRes = response;
		if (strstr(pATRes, "CMS ERROR") != 0) {
			SYSLOG_ERR("AT+CMGW command failed, Can not store Tx message: %s", response);
			goto send_error;
		}
		pATRes += strlen("+CMGW:");
		/* get first token : stored message index */
		if (strstr(pATRes, ",")) {
			pToken = strtok_r(pATRes, ",", &pSavePtr);
		} else {
			pToken = pATRes;
		}
		if (!pToken) {
			SYSLOG_ERR("got +CMGW but failed to get stored message index");
			goto send_error;
		}
		index = atoi(pToken);
		SYSLOG_NOTICE("Tx SMS stored (index=%d)", index);

		if (SaveSmsTimeStamp(index) < 0)
			SYSLOG_ERR("failed to save current time stamp for index [%d]", index);
send_retry_for_CMGW_command:
		/* send SMS from memory */
		(void) memset(command, 0x00, AT_RESPONSE_MAX_SIZE);
		sprintf(command, "AT+CMSS=%d", index);
		status = at_send_with_timeout(command, response, "", &fOK, 10, AT_RESPONSE_MAX_SIZE);
		if (status < 0 || !fOK)
			goto send_error;
		/* +CMSS: <mr> or +CMS ERROR : <err> */
		pATRes = response;
		if (strstr(pATRes, "CMS ERROR") != 0)
			goto send_error;

		if (strstr(pATRes, "+CMSS:") != 0) {
			SYSLOG_NOTICE("Tx message sent");

#ifdef USE_SIM_AS_SMS_STORAGE
			/* update storage status */
			(void)UpdateSmsMemStatus(1);
#endif

#ifndef USE_SIM_AS_SMS_STORAGE
			/* delete sent msg from SMS storage */
			deleteSmsMessage(index);
#endif

			result = 0;
			goto return_result;
		}

send_error:
		retry_cnt++;
		if (strlen(response))
			SYSLOG_ERR("AT+CMSS command failed, Can not send Tx message: %s, retry_cnt = %d", response, retry_cnt);
		else
			SYSLOG_ERR("AT+CMSS command failed, Can not send Tx message, retry_cnt = %d", retry_cnt);
		if (retry_cnt < MAX_TX_RETRY_CNT)
			goto send_retry_for_CMGW_command;
 		SYSLOG_ERR("AT+CMSS command failed %d times, keep failed msg for retry", retry_cnt);
		result = -2;	/* mark for keeping failed msg */
		/* delete unsent msg from SMS storage */
		deleteSmsMessage(index);
	}
error:
return_result:
	__free(response);
	__free(command);
	__free(pPduData);
	return result;
}
///////////////////////////////////////////////////////////////////////////////
static int handleSmsSendCommand(tx_mode_enum_type txmode)
{
	int need_ucs2_encode = 0, msg_len = 0, int_addr = 0;
	char *msg_body = 0, *pEncodeMsg = 0, *pSingleMsg = 0, *cp;
	char *achDestNo = alloca(BSIZE_32);
	int cbEncodeMsg, result = -1;
	char msg_fname[RDB_COMMAND_MAX_LEN];
	struct stat sb;
	int fp, total_msg_cnt, curr_msg_cnt, msg_ref_no, rd_cnt;
	int enc_msg_limit, one_msg_limit, cbpSingleMsg, remaining_bytes, udh_len;
	int cnt=0;
	char *udh = alloca(12);

	/* don't need to check storage space for diag command */
	if (txmode == SEND_USING_CMGW_CMD && CheckSmsStorage() < 0)	{
		SYSLOG_ERR("failed to check storage, can't not send sms");
		__goToError()
	}

	/* read destination number */
	(void) memset(achDestNo, 0x00, BSIZE_32);
	if (rdb_get_single(rdb_name(RDB_SMS_CMD_TO, ""), achDestNo, BSIZE_32) != 0)	{
		SYSLOG_ERR("failed to read sms destination db");
		__goToError()
	}

	/* set international dialling plan only with leading '+' */
	if (achDestNo[0] == '+')
		int_addr = 1;
	if(!is_cinterion_cdma)
	{
		/*----------------------------------------------*/
		/* set configuration 							*/
		/*----------------------------------------------*/
		/* if UCS2 encoding mode or special characters that can not be sent with GSM 7 bit mode found,
		 * use UCS2 encoding mode */
		read_smstools_encoding_scheme();
		//SYSLOG_ERR("smstools_encoding_scheme == %d, txmode = %d", smstools_encoding_scheme, txmode);
		if (smstools_encoding_scheme == DCS_UCS2_CODING) {
			need_ucs2_encode = 1;
		} else {
			need_ucs2_encode = check_extended_char();
			//SYSLOG_ERR("check_extended_char = %d", need_ucs2_encode);
		}
		if (smssend_configure(&need_ucs2_encode) < 0) {
			__goToError()
		}
	}

	SYSLOG_NOTICE("UCS2 encoding need? %d", need_ucs2_encode);

	/*----------------------------------------------*/
	/* read message from file						*/
	/*----------------------------------------------*/
	#if (0)
	/* read message body */
	if (rdb_get_single(rdb_name(RDB_SMS_CMD_MSG, ""), msg_body, BSIZE_1024) != 0) {
		SYSLOG_ERR("failed to read sms message body");
		rdb_set_single(rdb_name(RDB_SMS_CMD_TX_ST, ""), "[error] rdb(msg) fail");
		goto return_result;
	}
	#endif
	if (rdb_get_single(rdb_name(RDB_SMS_CMD_FNAME, ""), msg_fname, RDB_COMMAND_MAX_LEN) != 0) {
		SYSLOG_ERR("failed to read tx sms message file");
		__goToError()
	}
	sprintf(msg_fname, "%s.raw", msg_fname);
	if (stat(msg_fname, &sb) == -1) {
		SYSLOG_ERR("failed to stat of %s", msg_fname);
		__goToError();
    }
	if (sb.st_size == 0) {
		SYSLOG_ERR("message body length is zero, can not send this message.");
		__goToError()
	}
	fp = open(msg_fname, O_RDONLY);
	if (fp < 0) {
		SYSLOG_ERR("failed to open msg file %s", msg_fname);
		__goToError();
	}
	msg_len = sb.st_size;
	msg_body = __alloc(msg_len);
	__goToErrorIfFalse(msg_body)
	rd_cnt = read(fp, msg_body, msg_len);
	close(fp);
	if (rd_cnt < msg_len) {
		msg_len = rd_cnt;
	}
	msg_body[msg_len] = 0;
	SYSLOG_NOTICE("successfully read %d bytes", msg_len);

	// This loop is to determine SMS encoding type on CDMA module.
	if(is_cinterion_cdma)
	{
		need_ucs2_encode = 0;
		read_smstools_encoding_scheme();

		for (cnt=0; cnt < msg_len; cnt++)
		{
			if(msg_body[cnt] >= 0x80)
			{
				need_ucs2_encode = 1;
				break;
			}
		}

		if (smstools_encoding_scheme == DCS_UCS2_CODING)
				need_ucs2_encode = 1;

		SYSLOG_NOTICE("CDMA SMS: UCS2 encoding need? %d", need_ucs2_encode);
	}

	/*----------------------------------------------*/
	/* encode entire message body					*/
	/*----------------------------------------------*/
	/* msg_body should not include null in its body. now it's UTF-8 from console or web. */
	printMsgBody(msg_body, msg_len);
	enc_msg_limit = msg_len*UINT_SIZE;
	pEncodeMsg = __alloc(enc_msg_limit);
	__goToErrorIfFalse(pEncodeMsg)
		if (!is_cinterion_cdma)
			cbEncodeMsg = smssend_encodeMsg(msg_body, msg_len, pEncodeMsg, enc_msg_limit, &need_ucs2_encode);
		else
			cbEncodeMsg = smssend_encodeCDMAMsg(msg_body, msg_len, pEncodeMsg, enc_msg_limit, &need_ucs2_encode);

	if (cbEncodeMsg <= 0) {
		__goToError()
	}
	SYSLOG_NOTICE("UTF-8 msg len = %d, %s encoded msg len = %d",
				msg_len, (need_ucs2_encode? "UCS2":"GSM7"), cbEncodeMsg);

	/*----------------------------------------------*/
	/* Calculating total message count and          */
	/* length of a single message                   */
	/*----------------------------------------------*/
	/*
	 * GSM7 mode : 1 UTF-8 char (1 byte) --> 1 unicode char (2 bytes) --> 1 GSM7 code (7 bits)
	 * UCS2 mode : 1 normal UTF-8 char (1 byte) --> 1 unicode char (2 bytes) --> 1 UCS2 code (16 bits)
	 *             1 special UTF-8 char (up to 4 bytes) --> 1 unicode char (2 bytes) --> 1 UCS2 code (16 bits)
	 *
	 * GSM7 mode: cbEncodeMsg <= 160  : single msg
	 *			  cbEncodeMsg > 160   : if concatenating tx is possible, total msg count = cbEncodeMsg / 153
	 * 							   	    else total msg count = cbEncodeMsg / 160
	 *
	 * UCS2 mode: cbEncodeMsg/4 <= 70 : single msg
	 *			  cbEncodeMsg/4 > 70  : if concatenating tx is possible, total msg count = cbEncodeMsg / (67*4)
	 * 							         else total msg count = cbEncodeMsg / 280
	 */

	udh_len = 0;
	if (need_ucs2_encode) {
		if (cbEncodeMsg/4 <= MAX_UD_LEN_SINGLE_UCS2) {
			total_msg_cnt = 1;
			one_msg_limit = cbEncodeMsg;
		} else if (!ira_char_supported || !pdu_mode_sms || is_cinterion_cdma) {
			total_msg_cnt = (cbEncodeMsg/4+MAX_UD_LEN_SINGLE_UCS2-1)/MAX_UD_LEN_SINGLE_UCS2;
			one_msg_limit = MAX_UD_LEN_SINGLE_UCS2*4;
		} else {
			total_msg_cnt = (cbEncodeMsg/4+MAX_UD_LEN_CONCAT_UCS2-1)/MAX_UD_LEN_CONCAT_UCS2;
			one_msg_limit = MAX_UD_LEN_CONCAT_UCS2*4;
			udh_len = 12;	/* 12 IRA bytes for 6 bytes header */
		}
	} else {
		if (cbEncodeMsg <= MAX_UD_LEN_SINGLE_GSM7) {
			total_msg_cnt = 1;
			one_msg_limit = cbEncodeMsg;
		} else if (!ira_char_supported || !pdu_mode_sms || is_cinterion_cdma) {
			total_msg_cnt = (cbEncodeMsg+MAX_UD_LEN_SINGLE_GSM7-1)/MAX_UD_LEN_SINGLE_GSM7;
			one_msg_limit = MAX_UD_LEN_SINGLE_GSM7;
		} else {
			total_msg_cnt = (cbEncodeMsg+MAX_UD_LEN_CONCAT_GSM7-1)/MAX_UD_LEN_CONCAT_GSM7;
			one_msg_limit = MAX_UD_LEN_CONCAT_GSM7;
			udh_len = 12;	/* 12 IRA bytes for 6 bytes header */
		}
	}
	if (total_msg_cnt > 256) {
		SYSLOG_ERR("total concatenated msg count %d is larger than 256", total_msg_cnt);
		__goToError();
	}
	SYSLOG_NOTICE("ira_ucs2_sup %d, pdu mode %d, msg fsize = %lld, enc_msg_len %d, tot_msg_cnt = %d, one_msg_limit %d, udh_len %d",
		ira_char_supported, pdu_mode_sms, (long long)sb.st_size, cbEncodeMsg, total_msg_cnt, one_msg_limit, udh_len);
	/*----------------------------------------------*/
	/* send each message							*/
	/*----------------------------------------------*/
	pSingleMsg = __alloc(one_msg_limit+udh_len);
	msg_ref_no = random()%256;
	__goToErrorIfFalse(pSingleMsg)
	cp = pEncodeMsg;
	for (curr_msg_cnt = 0; curr_msg_cnt < total_msg_cnt; curr_msg_cnt++) {
		(void) memset(pSingleMsg, 0x00, one_msg_limit+udh_len);
		remaining_bytes = cbEncodeMsg-curr_msg_cnt*one_msg_limit;
		cbpSingleMsg = udh_len + (remaining_bytes < one_msg_limit? remaining_bytes:one_msg_limit);
		/* form User Data Header
		 *	05 - length of UDH
		 *	00 - Information-Element_Identifier 'A' : 00 : concat type
		 *	03 - length of Information-Element_Identifier 'A'
		 *	xx - Concatenated short message reference number.
		 *	xx - Maximum number of short messages in the concatenated short message.
		 *	xx - Sequence number of the current short message. start from 1
		 */
		if (udh_len) {
			sprintf(udh, "050003%02X%02X%02X", msg_ref_no, total_msg_cnt, (curr_msg_cnt+1));
			memcpy(pSingleMsg, udh, udh_len);
		}
		memcpy(pSingleMsg+udh_len, (char *)(cp+curr_msg_cnt*one_msg_limit), cbpSingleMsg-udh_len);
		SYSLOG_NOTICE("txmode %d, msg len %d, udh len %d, ref no %d, tot cnt %d, curr cnt %d",
			txmode, cbpSingleMsg, udh_len, msg_ref_no, total_msg_cnt, curr_msg_cnt+1);
		result = send_one_message(txmode, pSingleMsg, cbpSingleMsg, need_ucs2_encode, int_addr, achDestNo, udh_len);
		if (result < 0) {
			SYSLOG_ERR("failed to send %d / %d msg ", (curr_msg_cnt+1), total_msg_cnt);
			__goToError()
		}
	}

	rdb_set_single(rdb_name(RDB_SMS_CMD_TX_ST, ""), "[done] send");
	result = 0;
	goto return_result;

error:
	SYSLOG_ERR("Can not send Tx message");
	if (result == -2)
		rdb_set_single(rdb_name(RDB_SMS_CMD_TX_ST, ""), "[error] txfailed with retry count over");
	else
		rdb_set_single(rdb_name(RDB_SMS_CMD_TX_ST, ""), "[error] send sms failed");
return_result:
	__free(pSingleMsg);
	__free(pEncodeMsg);
	__free(msg_body);
	return result;
}
///////////////////////////////////////////////////////////////////////////////
static int handleSmsDeleteCommand(int all)
{
	int stat, fOK;
	char* response = alloca(BSIZE_32);
	char* command = __alloc(AT_RESPONSE_MAX_SIZE);
	char* pATRes;
	char achMsgId[BSIZE_1024];
	char* pToken;
	char* pSavePtr;
	__goToErrorIfFalse(command)

	/* read message id */
	if (rdb_get_single(rdb_name(RDB_SMS_CMD_ID, ""), achMsgId, BSIZE_1024) != 0) {
		SYSLOG_ERR("failed to read sms message id db");
		rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[error] rdb(id) fail");
		__goToError()
	}

	/* write message from memory */
	(void) memset(command, 0x00, AT_RESPONSE_MAX_SIZE);
	if (all) {
		sprintf(command, "AT+CMGD=1,4");
	} else {
		sprintf(command, "AT");
		pToken = strtok_r(achMsgId, " ", &pSavePtr);
		while (pToken) {
			sprintf(command, "%s+CMGD=%s;", command, pToken);
			pToken = strtok_r(NULL, " ", &pSavePtr);
		}
	}
	stat = at_send_with_timeout(command, response, "", &fOK, 10, BSIZE_32);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("AT+CMGD command failed, Can not delete message");
		rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[error] CMGD cmd fail");
		__goToError()
	}

	/* check +CMS ERROR : <err> */
	pATRes = response;
	if (strstr(pATRes, "CMS ERROR") != 0) {
		SYSLOG_ERR("AT+CMGD command failed, Can not delete message: %s", response);
		rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[error] CMGD cmd fail");
		__goToError()
	}

#ifdef USE_SIM_AS_SMS_STORAGE
	/* update storage status */
	(void)UpdateSmsMemStatus(1);
//#else
//	(void) UpdateUnreadMsgCount();
#endif

	if (all)
		rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[done] delall");
	else
		rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[done] delete");
	__free(command);
	return 0;
error:
	__free(command);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleSmsSetSmscCommand(void)
{
	int stat, fOK, int_addr = 0;
	char* response = alloca(BSIZE_32);
	char* command = __alloc(AT_RESPONSE_MAX_SIZE);
	char achNewAddr[64];
	const char* pToken;
	char *pSavePtr, *pATRes;
	__goToErrorIfFalse(command)

	/* read new SMSC address */
	if (rdb_get_single(rdb_name(RDB_SMS_CMD_TO, ""), achNewAddr, 64) != 0) {
		SYSLOG_ERR("failed to read sms to addr db");
		rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[error] rdb(id) fail");
		__goToError()
	}

	/* determine destination address type : 145 = international, 129 = domestic */
	if (achNewAddr[0] == '+')
		int_addr = 1;

	/* set SMSC addr */
	(void) memset(command, 0x00, AT_RESPONSE_MAX_SIZE);

	/* Cinterion 2G module, BGS2-E responds UCS2 format address so
	 * should set GSM mode before reading address */
	if (cinterion_type == cinterion_type_2g && char_set_save_restore(1) < 0) {
		goto err_return;
	}

	sprintf(command, "AT+CSCA=\"%s\",%s", achNewAddr, int_addr? "145":"129");
	stat = at_send_with_timeout(command, NULL, "", &fOK, 5, 0);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("AT+CSCA command failed, Can not set SMSC address");
		goto err_return;
	}

	stat = at_send("AT+CSCA?", response, "", &fOK, 64);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("AT+CSCA? command failed");
		goto err_return;
	}
	pATRes = response;
	pATRes += (strlen("+CSCA: ")+1);
	pToken = strtok_r(pATRes, "\"", &pSavePtr);
	if (!pToken) {
		SYSLOG_ERR("got +CSCA but failed to parse smsc addr");
		goto err_return;
	} else {
		rdb_set_single(rdb_name(RDB_SMS_SMSC_ADDR, ""), pToken);
	}
	rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[done] setsmsc");

	/* Cinterion 2G module, BGS2-E responds UCS2 format address so
	 * should set GSM mode before reading address */
	if (cinterion_type == cinterion_type_2g) {
		(void) char_set_save_restore(0);
	}

	__free(command);
	return 0;
err_return:
	rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[error] CSCA cmd fail");
error:
	__free(command);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
extern BOOL at_manager_initialized;
int default_handle_command_sms(const struct name_value_t* args)
{
	if (sms_disabled) {
		SYSLOG_ERR("SMS feature is disabled. check appl options");
		goto error;
	}

	/* bypass if incorrect argument */
	if (!args || !args[0].value) {
		SYSLOG_ERR("expected command, got NULL");
		goto error;
	}

	/* allow 'readall' command before atmgr initialized */
	if (!at_manager_initialized && memcmp(args[0].value, "readall", 7))
		return -1;

	SET_LOG_LEVEL_TO_SMS_LOGMASK_LEVEL

	/* process SENDDIAG command for ack/error msg of diagnostics */
	if (memcmp(args[0].value, "senddiag", 8) == 0) {
		SYSLOG_NOTICE("Got SMS SENDDIAG command");
		if (handleSmsSendCommand(SEND_DIAG_MSG) < 0)
			goto error;
	}

	/* process SEND command */
	else if (memcmp(args[0].value, "send", 4) == 0) {
		SYSLOG_NOTICE("Got SMS SEND command");
#ifdef USE_SIM_AS_SMS_STORAGE
		if (handleSmsSendCommand(SEND_USING_CMGW_CMD) < 0)
#else
		/* do not save sent msg to SMS storage always */
		if (handleSmsSendCommand(SEND_USING_CMGS_CMD) < 0)
#endif

			goto error;
	}

	/* process READUNREAD command */
	/* at the moment no application uses this command */
	else if (memcmp(args[0].value, "readunread", 10) == 0) {
		SYSLOG_NOTICE("Got SMS READUNREAD command");
		if (readOneUnreadMsg(GetFirstUnreadMsgIdx()) < 0) {
			rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[error]");
			goto error;
		} else {
			rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[done]");
		}
	}

	/* process READALL command */
	else if (memcmp(args[0].value, "readall", 7) == 0) {
		SYSLOG_NOTICE("Got SMS READALL command");
		if (UpdateSmsMemStatus(1) < 0) {
			rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[error] readall fail");
			goto error;
		} else {
			rdb_set_single(rdb_name(RDB_SMS_CMD_ST, ""), "[done] readall");
		}
	}

	/* process DELETE command */
	else if (memcmp(args[0].value, "delete", 6) == 0) {
		SYSLOG_NOTICE("Got SMS DELETE command");
		if (handleSmsDeleteCommand(0) < 0)
			goto error;
	}

	/* process DELETEALL command */
	else if (memcmp(args[0].value, "delall", 6) == 0) {
		SYSLOG_NOTICE("Got SMS DELALL command");
		if (handleSmsDeleteCommand(1) < 0)
			goto error;
	}

	/* process SETSMSC command */
	else if (memcmp(args[0].value, "setsmsc", 6) == 0) {
		SYSLOG_NOTICE("Got SMS SETSMSC command");
		if (handleSmsSetSmscCommand() < 0)
			goto error;
	}

	else {
		SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
		goto error;
	}

	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return 0;

error:

	SYSLOG_ERR("SMS [%s] command failed", args[0].value);
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return -1;
}

/*
* vim:ts=4:sw=4:
*/

