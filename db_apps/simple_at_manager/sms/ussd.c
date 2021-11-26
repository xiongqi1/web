/*!
* Copyright Notice:
* Copyright (C) 2012 NetComm Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of NetComm Wireless Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
* NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include "rdb_ops.h"
#include "../rdb_names.h"
#include "cdcs_syslog.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../models/model_default.h"
#include "../util/at_util.h"
#include "../util/rdb_util.h"
#include "sms.h"
#include "pdu.h"
#include "coding.h"
#include "utf8.h"
#include "gsm7.h"
#include "ussd.h"
#include "../logmask.h"

#ifdef USSD_SUPPORT
///////////////////////////////////////////////////////////////////////////////
/* Cinterion modules send chunk USSD response using 0xc3, 0x96, 0x30, 0x41 as line feed character so
 * conversion to 0x0a (GSM7) is necessary */
void remove_trailing_garbage(char* pDecodeMsg, int* cbDecodeMsg)
{
	int i, j, conv_cnt = 0;
	char* pTmpS = __alloc(MAX_UNICODE_BUF_SIZE);
	if (cinterion_type == cinterion_type_umts || cinterion_type == cinterion_type_2g) {
		(void) memcpy(pTmpS, pDecodeMsg, MAX_UNICODE_BUF_SIZE);
		(void) memset(pDecodeMsg, 0x00, MAX_UNICODE_BUF_SIZE);
		for (i = 0, j = 0; i < *cbDecodeMsg; i++, j++) {
			if (pTmpS[i] == 0xc3 && pTmpS[i+1] == 0x96 && pTmpS[i+2] == 0x30 && pTmpS[i+3] == 0x41) {
				SYSLOG_DEBUG("replace trailing garbage 0xc3, 0x96, 0x30, 0x41 to 0x0a at %d", i);
				pDecodeMsg[j] = 0x0a;
				i+=3;
				conv_cnt++;
			} else {
				pDecodeMsg[j] = pTmpS[i];
			}
		}
		*cbDecodeMsg -= conv_cnt;
	}
	__free(pTmpS);
}

const char ussd_msg_file[30] = "/tmp/ussd_msgs";
/* m value in 3GPP TS 127.007, 7.15
 * 0 : no further user action required
 * 1 : further user action required
 * 2 : USSD terminated by network
 * 3 : other local client has responded
 * 4 : operation not supported
 * 5 : network time out
*/
static int last_ussd_response_type = 0;
int handle_ussd_notification(const char* s)
{
#ifdef  USSD_UCS2_TEST
	char* pNewS = __alloc(MAX_UNICODE_BUF_SIZE);
#else
	char* pNewS = strdup(s);
#endif
	char *pStart, *pEnd, *pDcs, *pType;
	const char* at_line;
	int fp, dcs, msg_len, fOK;
	int ret = 1;
	char* pDecodeMsg = __alloc(MAX_UNICODE_BUF_SIZE);
	int cbDecodeMsg = 0;
	struct sms_dcs tmp_dcs;
	sms_encoding_type coding_type = DCS_7BIT_CODING;
	char* command = __alloc(BSIZE_16);
	char* response = __alloc(BSIZE_16);

#define ERR_CHECK(len) if (len <= 0) { SYSLOG_ERR("USSD msg decoding error"); goto exit_ussd_noti; }

	//SYSLOG_ERR("Got USSD noti msg");
	__goToErrorIfFalse(pNewS)
	__goToErrorIfFalse(pDecodeMsg)

	strcpy(pNewS, s);
	
#ifdef  USSD_UCS2_TEST
	#define UCS2_USSD_TEST_MSG  "+CUSD: 1,\"002D002D002000540065006C007300740" \
                                "072006100200020002D002D000A0031002E0020004D0" \
                                "079004100630063006F0075006E0074000A0032002E0" \
                                "0200043007200650064006900740020004D006500320" \
                                "055000A0033002E002000430061006C006C006500720" \
                                "0200054006F006E00650073000A0034002E002000540" \
                                "06F006E00650073002F0050006900630073000A00350" \
                                "02E0020004E006500770073002F00570074006800720" \
                                "00A0036002E002000530070006F00720074000A00370" \
                                "02E002000470061006D00650073000A0038002E00200" \
                                "0460075006E002F0049006E0066006F000A0039002E0" \
                                "020004D00790041006C0065007200740073002F00480" \
                                "06C0070000A00300030002E00200048006F006D0065" \
                                "\",72"
	strcpy(pNewS, UCS2_USSD_TEST_MSG);
#endif

	SET_LOG_LEVEL_TO_USSD_LOGMASK_LEVEL
	pStart = strchr(pNewS, 0x22);	/* first " */
	pEnd = strrchr(pNewS, 0x22);	/* last " */

	SYSLOG_DEBUG("ussd noti : %s", pNewS);

	if (!pStart || !pEnd)
		goto exit_ussd_noti;

	printMsgBody(pNewS, strlen(pNewS)+3);

	fp = open(ussd_msg_file, O_CREAT | O_WRONLY | O_TRUNC);
	if(fp < 0)
	{
		SYSLOG_ERR("fail to create ussd msg file %s\n", ussd_msg_file);
		__goToError()
	}

	/* check if the response includes DCS field.
	 * UCS2 type message consists of 1 line but other types returns multi line.
	 */
	//SYSLOG_ERR("*(pEnd+1) = %c", *(pEnd+1));
	msg_len = pEnd - pStart - 1;
	if (*(pEnd+1) == ',') {
		pDcs = pEnd+2;
		if (!pDcs) {
			SYSLOG_ERR("Parse error: pDcs is Null");
		} else {
			SYSLOG_DEBUG("USSD DCS field is %s", pDcs);
			dcs = atoi(pDcs);
			tmp_dcs.cmd = (dcs & 0xf0) >> 4;
			tmp_dcs.param = (dcs & 0x0f);
			coding_type = parse_msg_coding_type(&tmp_dcs);
			SYSLOG_DEBUG("DCS = %d, msg_len = %d, encoded to %s", dcs, msg_len,
				((coding_type == DCS_UCS2_CODING)? "UCS2":
				((coding_type == DCS_7BIT_CODING)? "GSM7":"8BIT")));
		}
	}

	if (coding_type == DCS_UCS2_CODING) {
		cbDecodeMsg = smsrecv_decodeUtf8FromUnicodeStr((char*)(pStart+1), pDecodeMsg, msg_len);
		ERR_CHECK(cbDecodeMsg);
		SYSLOG_DEBUG("raw USSD message : %s", pDecodeMsg);
		write(fp, pDecodeMsg, cbDecodeMsg);
	} else if (coding_type == DCS_8BIT_CODING) {
		strcpy(pDecodeMsg, (char*)(pStart+1));
		cbDecodeMsg = strlen(pDecodeMsg);
		ERR_CHECK(cbDecodeMsg);
		SYSLOG_DEBUG("raw USSD message : %s", pDecodeMsg);
		write(fp, pDecodeMsg, cbDecodeMsg);
	} else {
		//+CUSD: 1,"-- Telstra  --
		if (strstr(pNewS, "+CUSD") != 0)
		{
			pType = pNewS + 7;
			//SYSLOG_ERR("pNewS = %s, *pNewS = %c",pNewS, *pNewS);
			last_ussd_response_type = (int)(*pType - '0');
			//SYSLOG_ERR("last_ussd_response_type = %d",last_ussd_response_type);
			if (last_ussd_response_type >= 3)
				ret = 0;

			/* Cinterion modules send chunk USSD response using 0xc3, 0x96, 0x30, 0x41 as line feed character so
			 * conversion to 0x0a (GSM7) is necessary */
			if (cinterion_type == cinterion_type_2g && last_ussd_response_type == 1) {
				/* stupid Cinterion 2G module enters text entry mode after sending
				 * USSD response, ctrl-z should be sent to exit. */
				command[0] = 0x1A; // ctrl-z
				command[1] = 0x00;
				(void) at_send_with_timeout(command, response, "", &fOK, 2, BSIZE_16);
			}

		}
		
		if (!pStart || !pEnd)
		{
			goto exit_ussd_noti;
		}

		/* process 1 line notification */
		if (pEnd && pEnd != pStart)
		{
			cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7((char*)(pStart+1), pDecodeMsg, (pEnd - pStart -1));
			ERR_CHECK(cbDecodeMsg);
			remove_trailing_garbage(pDecodeMsg, &cbDecodeMsg);
			write(fp, pDecodeMsg, cbDecodeMsg);
			printMsgBody(pDecodeMsg, cbDecodeMsg);
		}
		else
		{
			cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7((char*)(pStart+1), pDecodeMsg, (strlen(pStart)-1));
			ERR_CHECK(cbDecodeMsg);
			remove_trailing_garbage(pDecodeMsg, &cbDecodeMsg);
			write(fp, pDecodeMsg, cbDecodeMsg);
			write(fp, "\n", 1);
			// read following line - this is part of notification
			do
			{
				at_line = direct_at_read(1);
				if (at_line)
				{
					if (at_line[0] == '+' || at_line[0] == '^' || at_line[0] == '*' ||
						strncasecmp(at_line, "at", 2) == 0)
					{
						SYSLOG_ERR("skip response : %s", at_line);
						continue;
					}
					SYSLOG_DEBUG("ussd noti : %s", at_line);
					pEnd = strrchr(at_line, 0x22);	/* last " */
					if (pEnd) {
						cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7((char*)at_line, pDecodeMsg, (strlen(at_line)-strlen(pEnd)));
						ERR_CHECK(cbDecodeMsg);
						remove_trailing_garbage(pDecodeMsg, &cbDecodeMsg);
						write(fp, pDecodeMsg, cbDecodeMsg);
					} else {
						cbDecodeMsg = smsrecv_decodeUtf8FromGsmBit7((char*)at_line, pDecodeMsg, strlen(at_line));
						ERR_CHECK(cbDecodeMsg);
						remove_trailing_garbage(pDecodeMsg, &cbDecodeMsg);
						write(fp, pDecodeMsg, cbDecodeMsg);
					}
					write(fp, "\n", 1);
				}
			} while (at_line);
		}
	}
exit_ussd_noti:
	close(fp);
	rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), (ret? "[done] USSD cmd":"[error] network noti fail"));
	SYSLOG_ERR("Processed USSD noti msg");
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
#ifdef  USSD_UCS2_TEST
	__free(pNewS);
#endif	
	__free(pDecodeMsg);
	__free(command);
	__free(response);
	return 0;
error:
	rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), "[error] network noti fail");
#ifdef  USSD_UCS2_TEST
	__free(pNewS);
#endif	
	__free(pDecodeMsg);
	__free(command);
	__free(response);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
void setupUssdMode(void)
{
	int stat, fOK, i;
	const char *setup_cmd[] = { "AT+CSMP=,,0,0", "AT+CSCS=\"IRA\"", 0 };
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	__goToErrorIfFalse(response)
	for (i = 0; setup_cmd[i]; i++) {
		stat = at_send(setup_cmd[i], response, "", &fOK, 0);
		if (stat < 0 || !fOK) {
			SYSLOG_ERR("'%s' command failed", setup_cmd[i]);
		}
	}
	free(response);
error:	
	return;
}
///////////////////////////////////////////////////////////////////////////////
typedef enum
{
	SESSION_BEGIN,
	SELECT_MENU,
	SESSION_END,
} ussd_command_type;
const char *ussd_cmd_str[3] = { "dial", "select", "end" };
static int handleUssdCommand(ussd_command_type ussd_cmd)
{
	int stat, fOK;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char command[BSIZE_64];
	char* pATRes, *pToken = 0;
	char cmd_str[BSIZE_32];
	char err_str[BSIZE_32];
	__goToErrorIfFalse(response)

	/* read dial string. ex) Telstra "#100#"
	 * or menu selection */
	if (ussd_cmd != SESSION_END &&
	(rdb_get_single(rdb_name(RDB_USSD_MESSAGE, ""), cmd_str, BSIZE_32) != 0 || strlen(cmd_str) == 0))
	{
		SYSLOG_ERR("failed to read cmd string db");
		rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), "[error] rdb(cmd str) fail");
		goto ret_error_wo_result;
	}

	/* setup USSD mode before sending commands */
	//setupUssdMode();

	/* send dial command */
	(void) memset(command, 0x00, BSIZE_64);
	if (ussd_cmd == SESSION_BEGIN) {
		/* Cinterion 2G module, BGS2-E deliver UCS2 format content when its character
		 * set is set to UCS2 so should set GSM mode during USSD sesstion */
		if (cinterion_type == cinterion_type_2g && char_set_save_restore(1) < 0) {
			__goToError()
		}
		//sprintf(command, "AT+CUSD=1,\"#100#\",15");
		//printMsgBody(command, BSIZE_64);
		sprintf(command, "AT+CUSD=1,\"%s\",15", cmd_str);
		//printMsgBody(command, BSIZE_64);
	} else if (ussd_cmd == SELECT_MENU) {
		/* Cinterion 2G module, BGS2-E deliver UCS2 format content when its character
		 * set is set to UCS2 so should set GSM mode during USSD sesstion */
		if (cinterion_type == cinterion_type_2g && char_set_save_restore(1) < 0) {
			__goToError()
		}
		sprintf(command, "AT+CUSD=1,\"%s\"", cmd_str);
	} else {
		/* Cinterion 2G module, BGS2-E deliver UCS2 format content when its character
		 * set is set to UCS2 so should set GSM mode during USSD sesstion */
		if (cinterion_type == cinterion_type_2g) {
			char_set_save_restore(0);
		}
		sprintf(command, "AT+CUSD=0");		/* cancel session */
	}

	rdb_set_single(rdb_name(RDB_USSD_MESSAGE, ""), "");

	stat = at_send(command, response, "", &fOK, 0);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("'%s' command failed", command);
		__goToError()
	}

	/* Ericsson module does not send CUSD notification instead it include
	 * USSD contents within command response */
	if (ussd_cmd != SESSION_END) {
		if (is_ericsson && strstr(response, "+CUSD") != 0)
		{
			(void) handle_ussd_notification(response);
		} else {
			if(at_wait_notification(10) < 0) {
				__goToError()
			} else {
				goto ret_ok;
			}
		}
	}

	/* at+cusd=2 command shoule be sent to end normal active session */
	if (ussd_cmd == SESSION_END && last_ussd_response_type == 1)
	{
		SYSLOG_NOTICE("USSD status : %s", pToken);
		sprintf(command, "AT+CUSD=2");		/* end session */
		stat = at_send(command, response, "", &fOK, 0);
		if (stat < 0 || !fOK) {
			SYSLOG_ERR("'%s' command failed", command);
			//rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), "[error] CUSD cmd fail");
			//return -1;
		}
		/* check session end before send +cusd=2 */
		sprintf(command, "AT+CUSD?");
		stat = at_send(command, response, "", &fOK, 0);
		if (stat < 0 || !fOK || strlen(response) == 0) {
			SYSLOG_ERR("'%s' command failed", command);
			__goToError()
		}
		pToken=response;
		pToken += strlen("+CUSD: ");
		if (!pToken) {
			SYSLOG_ERR("got +CUSD but failed to get status");
			__goToError()
		}
		if (strcmp(pToken, "0")) {
			__goToError()
		} else {
			goto ret_ok;
		}
	}

	/* check +CME ERROR : <err> */
	pATRes = response;
	if (strstr(pATRes, "+CME ERROR") != 0)
	{
		SYSLOG_ERR("'%s' command failed", command);
		pATRes += 12;	/* "+CME ERROR: " */
		sprintf(err_str, "[error] CUSD cmd, %s", pATRes);
		rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), err_str);
		goto ret_error_wo_result;
	}
	__free(response);
	if (ussd_cmd == SESSION_END)
	{
		goto ret_ok;
	}
	return 0;
ret_ok:
	rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), "[done] USSD cmd");
	__free(response);
	return 0;
error:
	rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), "[error] CUSD cmd fail");
ret_error_wo_result:	
	__free(response);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int getUssdStatus(void)
{
	int stat, fOK, ussd_status;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char command[BSIZE_32];
	char* pToken;
	__goToErrorIfFalse(response)

	sprintf(command, "AT+CUSD?");
	stat = at_send(command, response, "", &fOK, 0);
	if (stat < 0 || !fOK || strlen(response) == 0) {
		SYSLOG_ERR("%s command failed, Can not read USSD status", command);
		__goToError()
	}

	pToken=response;
	pToken += strlen("+CUSD: ");
	if (!pToken)
		SYSLOG_ERR("got +CUSD but failed to get status");
	SYSLOG_NOTICE("USSD status : %s", pToken);
	ussd_status = atoi(pToken);
	if (ussd_status > 5)
		SYSLOG_ERR("status code is out of range");
	rdb_set_single(rdb_name(RDB_USSD_MESSAGE, ""), pToken);
	__free(response);
	return 0;
error:	
	__free(response);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int default_handle_command_ussd(const struct name_value_t* args)
{
	ussd_command_type ussd_cmd;

	/* bypass if incorrect argument */
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		goto error;
	}
	SET_LOG_LEVEL_TO_USSD_LOGMASK_LEVEL

	for (ussd_cmd = SESSION_BEGIN; ussd_cmd <= SESSION_END; ussd_cmd++)
	{
		if (memcmp(args[0].value, ussd_cmd_str[ussd_cmd], strlen(ussd_cmd_str[ussd_cmd])) == 0)
		{
			SYSLOG_NOTICE("Got USSD '%s' command", ussd_cmd_str[ussd_cmd]);
			if (handleUssdCommand(ussd_cmd) < 0)
			{
				goto error;
			}
			else
			{
				SYSLOG_NOTICE("processed USSD '%s' command", ussd_cmd_str[ussd_cmd]);
				SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
				return 0;
			}
		}
	}

	/* process STATUS command */
	if (memcmp(args[0].value, "status", 6) == 0)
	{
		SYSLOG_NOTICE("Got USSD STATUS command");
		if (getUssdStatus() < 0)
		{
			rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), "[error]");
			goto error;
		}
		else
		{
			rdb_set_single(rdb_name(RDB_USSD_CMD_RESULT, ""), "[done]");
		}
		SYSLOG_NOTICE("processed USSD STATUS command");
	}

	else
	{
		SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
		goto error;
	}

	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return 0;

error:
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	SYSLOG_ERR("USSD [%s] command failed", args[0].value);
	return -1;
}
#endif	/* USSD_SUPPORT */

