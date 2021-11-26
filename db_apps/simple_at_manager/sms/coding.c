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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include "cdcs_syslog.h"
#include "pdu.h"
#include "sms.h"
#include "gsm7.h"
#include "utf8.h"
#include "coding.h"
#include "../util/at_util.h"

#ifdef NO_USE
int smssend_encodeGsmBit7(char* pMsg, char* pEncodeMsg, int cbMsg)
{
	return iso2gsm(pMsg, cbMsg, pEncodeMsg, (cbMsg)*3);
}
#endif

unsigned int ConvCharTohex(char chNibble)
{
	if (('0' <= chNibble) && (chNibble <= '9'))
		return chNibble -'0';

	return ((chNibble | ('a' ^ 'A')) - 'a' + 0x0a) & 0x0f;
}

int ConvStrToChar(const char* pSrc, int cbSrc, char* pDst, int cbDst)
{
	int iBuf = 0;
	char temp[2];

	while (iBuf < cbDst)
	{
		temp[0] = ConvCharTohex(*pSrc++);
		temp[1] = ConvCharTohex(*pSrc++);
		pDst[iBuf++] = (char)((temp[0] << 4) | (temp[1]));
	}
	if (iBuf <= cbDst)
	{
		pDst[iBuf] = 0;
		return iBuf;
	}
	pDst[cbDst] = 0;
	return -1;
}

int ConvStrToInt(const char* pSrc, int cbSrc, unsigned int* pDst, int cbDst)
{
	int iBuf = 0;
	unsigned int temp[4];

	while (iBuf < cbDst)
	{
		temp[0] = ConvCharTohex(*pSrc++);
		temp[1] = ConvCharTohex(*pSrc++);
		temp[2] = ConvCharTohex(*pSrc++);
		temp[3] = ConvCharTohex(*pSrc++);
		pDst[iBuf++] = (unsigned int)((temp[0] << 12) | (temp[1] << 8) | (temp[2] << 4) | (temp[3]));
	}
	if (iBuf <= cbDst)
	{
		pDst[iBuf] = 0;
		return iBuf;
	}
	pDst[cbDst] = 0;
	return -1;
}


#ifdef UTF_8_LIB_FUNCTION
int smssend_encodeUnicodeFromUtf8(char* pMsg, int cbpMsg, unsigned int* pUnicodeMsg, int cbpUnicodeMsg)
{
	int cbUnicodeMsg = 0;

	/* encode UTF-8 to Unicode */
	(void) memset(pUnicodeMsg, 0x0, cbpUnicodeMsg);
	cbUnicodeMsg = 0;
	cbUnicodeMsg = (int)utf8_to_wchar((const char *)pMsg, (size_t)cbpMsg, (wchar_t *)pUnicodeMsg, (size_t)cbpUnicodeMsg, 0);
	SYSLOG_DEBUG("encoded Unicode len = %d", cbUnicodeMsg);
	return cbUnicodeMsg;
}
#else
int smssend_encodeUnicodeFromUtf8(char* pMsg, int cbpMsg, unsigned int* pUnicodeMsg, int cbpUnicodeMsg)
{
	unsigned int uc1, uc2, uc3;
	unsigned char c1, c2, c3;
	int cbUnicodeMsg = 0, i;

	/* encode UTF-8 to Unicode */
	(void) memset(pUnicodeMsg, 0x0000, cbpUnicodeMsg);
	cbUnicodeMsg = 0;
	for (i = 0; i < cbpMsg; i++, pMsg++) {
		c1 = (unsigned char)(*pMsg);

		/* single digit char */
		if (c1 < 0xc2) {
			pUnicodeMsg[cbUnicodeMsg++] = c1;
		}
		/* two digits char */
		else if (c1 >= 0xc2 && c1 <= 0xdf ) {
			i++; pMsg++;
			c2 = (unsigned char)(*pMsg);
			uc1 = ((c1 & 0x3c ) << 6) & 0xff00;
			uc2 = ((c1 & 0x03) * 0x40 + (c2 & 0x7f)) & 0x00ff;
			pUnicodeMsg[cbUnicodeMsg++] = uc1 | uc2;
		}
		/* three digits char */
		else if (c1 >= 0xe0 && c1 <= 0xef ) {
			i++; pMsg++;
			c2 = (unsigned char)(*pMsg);
			i++; pMsg++;
			c3 = (unsigned char)(*pMsg);
			uc1 = ((c1 & 0x0f) << 12) & 0xff00;
			uc2 = ((c2 & 0x3c) << 6) & 0xff00;
			uc3 = ((c2 & 0x03) * 0x40 + (c3 & 0x7f)) & 0x00ff;
			pUnicodeMsg[cbUnicodeMsg++] = uc1 | uc2 | uc3;
		}
		/* not support 4 digits char yet */
		else {
			SYSLOG_ERR("encoding error, not support 4 digits char yet");
			i += 3;
			pMsg += 3;
		}
	}
	return cbUnicodeMsg;
}
#endif

int smssend_encodeGsmBit7FromUnicode(unsigned int* pMsg, char* pEncodeMsg, int cbMsg)
{
	return unicode2gsm(pMsg, cbMsg, pEncodeMsg, (cbMsg)*2);
}

// Convert Unicode to CDMA 8-Bit ASCII
// Argurment:
//	- pMsg -> pointer of  int type input buffer
//	- pEncodeMsg -> pointer of char type 8-bit output buffer
//	- cbMsg -> the number of characters on the input buffer
//
// Return Value:
//	the number of characters on the output buffer
//
int smssend_encodeASCIIFromUnicode(unsigned int* pMsg, char* pEncodeMsg, int cbMsg)
{
	int dest_count = 0;

	if (pMsg == NULL || pEncodeMsg == NULL || cbMsg <= 0)
		return 0;

	while ( dest_count < cbMsg) {
		pEncodeMsg[dest_count] = (char) pMsg[dest_count];
		dest_count++;
	}

	pEncodeMsg[dest_count] = 0;

	return dest_count;
}

#ifdef UTF_8_LIB_FUNCTION
int smsrecv_decodeUtf8FromUnicode(unsigned int* pMsg, char* pDecodeMsg, int cbMsg)
{
	int cbDecodeMsg = 0;

	/* dncode Unicode to UTF-8*/
	(void) memset(pDecodeMsg, 0x00, MAX_UNICODE_BUF_SIZE);
	cbDecodeMsg = 0;
	cbDecodeMsg = (int)wchar_to_utf8((const wchar_t *)pMsg, (size_t)cbMsg, pDecodeMsg, (size_t)cbMsg*sizeof(unsigned int), 0);
	return cbDecodeMsg;
}

#else
int smsrecv_decodeUtf8FromUnicode(unsigned int* pMsg, char* pDecodeMsg, int cbMsg)
{
	unsigned int uc;
	int cbDecodeMsg = 0, i;

	/* encode UTF-8 to Unicode */
	(void) memset(pDecodeMsg, 0x00, MAX_UNICODE_BUF_SIZE);
	cbDecodeMsg = 0;

	for (i = 0; i < cbMsg; i++, pMsg++) {
		uc = *pMsg;
		if (uc < 0x80) {
			*pDecodeMsg++ = uc; cbDecodeMsg++;
		} else if (uc < 0x800) {
			*pDecodeMsg++ = 192 + uc/64;
			*pDecodeMsg++ = 128 + uc%64;
			cbDecodeMsg += 2;
		} else if (uc-0xd800u < 0x800) {
			/* error, stop decoding */
			break;;
		} else if (uc < 0x10000) {
			*pDecodeMsg++ = 224 + uc/4096;
			*pDecodeMsg++ = 128 + uc/64%64;
			*pDecodeMsg++ = 128 + uc%64;
			cbDecodeMsg += 3;
		} else if (uc < 0x110000) {
			*pDecodeMsg++ = 240 + uc/262144;
			*pDecodeMsg++ = 128 + uc/4096%64;
			*pDecodeMsg++ = 128 + uc/64%64;
			*pDecodeMsg++ = 128 + uc%64;
			cbDecodeMsg += 4;
		} else {
			/* error, stop decoding */
			break;;
		}
	}
	return cbDecodeMsg;
}
#endif

int smsrecv_decodeUnicodeFromGsmBit7(char* pMsg, unsigned int* pDecodeMsg, int cbMsg)
{
	return gsm2unicode(pMsg, cbMsg, pDecodeMsg, cbMsg);
}

// Convert CDMA 8-Bit ASCII to Unicode
// Argurment:
//	- pMsg -> pointer of char type 8-bit input buffer 
//	- pEncodeMsg -> pointer of integer type output buffer
//	- cbMsg -> the number of characters on the input buffer
//
// Return Value:
//	the number of characters on the output buffer
//
int smsrecv_decodeUnicodeFromASCII(char* pMsg, unsigned int* pDecodeMsg, int cbMsg)
{
	int dest_count = 0;

	if (pMsg == NULL || pDecodeMsg == NULL || cbMsg <= 0)
		return 0;

	while ( dest_count < cbMsg) {
		pDecodeMsg[dest_count] = (unsigned int) pMsg[dest_count];
		dest_count++;
	}

	pDecodeMsg[dest_count] = 0;

	return dest_count;
}

/* pDecodeMsg : MAX_UNICODE_BUF_SIZE */
int smsrecv_decodeUtf8FromGsmBit7(char* pMsg, char* pDecodeMsg, int cbMsg)
{
	unsigned int* ptmpMsg = __alloc(MAX_UTF8_BUF_SIZE);
	int cbTmpMsg = 0, cbDecodeMsg = 0;
	__goToErrorIfFalse(ptmpMsg)

	/* decode GSM7 bits to Unicode, then decode to UTF-8 */
	cbTmpMsg = smsrecv_decodeUnicodeFromGsmBit7(pMsg, ptmpMsg, cbMsg);
	SYSLOG_DEBUG("decoded to Unicode, len %d", cbTmpMsg);
	if (cbTmpMsg <= 0) {
		SYSLOG_ERR("Unicode decoding error");
		__free(ptmpMsg);
		return cbTmpMsg;
	}
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, pDecodeMsg, cbTmpMsg);
	SYSLOG_DEBUG("decoded to UTF-8, len %d", cbDecodeMsg);
	__free(ptmpMsg);
	if (cbDecodeMsg <= 0) {
		SYSLOG_ERR("Unicode decoding error");
		return cbDecodeMsg;
	}
	printMsgBody(pDecodeMsg, cbDecodeMsg);
	return cbDecodeMsg;
error:
	return -1;
}

/* pDecodeMsg : MAX_UNICODE_BUF_SIZE */
int smsrecv_decodeUtf8FromUnicodeStr(char* pMsg, char* pDecodeMsg, int cbMsg)
{
	unsigned int* ptmpMsg = __alloc(MAX_UTF8_BUF_SIZE);
	int cbTmpMsg = 0, cbDecodeMsg = 0;
	__goToErrorIfFalse(ptmpMsg)

	cbTmpMsg = ConvStrToInt((const char *)pMsg, cbMsg, ptmpMsg, cbMsg/4);
	SYSLOG_DEBUG("converted to Unicode integer, len %d", cbTmpMsg);
	if (cbTmpMsg <= 0) {
		SYSLOG_ERR("Unicode decoding error");
		__free(ptmpMsg);
		return cbTmpMsg;
	}
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, pDecodeMsg, cbTmpMsg);
	SYSLOG_DEBUG("decoded to UTF-8, len %d", cbDecodeMsg);
	__free(ptmpMsg);
	if (cbDecodeMsg <= 0) {
		SYSLOG_ERR("Unicode decoding error");
		return cbDecodeMsg;
	}
	printMsgBody(pDecodeMsg, cbDecodeMsg);
	return cbDecodeMsg;
error:
	return -1;
}



