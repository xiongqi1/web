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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include "cdcs_syslog.h"
#include "smsdef.h"
#include "gsm7.h"
#include "utf8.h"
#include "coding.h"
#include "utils.h"

unsigned int ConvCharTohex(char chNibble)
{
	if (('0' <= chNibble) && (chNibble <= '9'))
		return chNibble -'0';

	return ((chNibble | ('a' ^ 'A')) - 'a' + 0x0a) & 0x0f;
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


/* max len of pUnicodeMsg = max len of pMsg*UINT_SIZE */
int smssend_encodeUnicodeFromUtf8(char* pMsg, unsigned int* pUnicodeMsg, int cbMsg)
{
    int cbUnicodeMsg = 0;

	/* encode UTF-8 to Unicode */
	cbUnicodeMsg = 0;
	cbUnicodeMsg = (int)utf8_to_wchar((const char *)pMsg, (size_t)cbMsg, (wchar_t *)pUnicodeMsg, (size_t)cbMsg, 0);
	syslog(LOG_DEBUG, "encoded Unicode len = %d", cbUnicodeMsg);
	return cbUnicodeMsg;
}

int smssend_encodeGsmBit7FromUnicode(unsigned int* pMsg, char* pEncodeMsg, int cbMsg)
{
    return unicode2gsm(pMsg, cbMsg, pEncodeMsg, (cbMsg)*2);
}

int smsrecv_decodeUtf8FromUnicode(unsigned int* pMsg, char* pDecodeMsg, int cbMsg)
{
    int cbDecodeMsg = 0;

	/* dncode Unicode to UTF-8*/
	(void) memset(pDecodeMsg, 0x00, MAX_UNICODE_BUF_SIZE);
	cbDecodeMsg = 0;
	cbDecodeMsg = (int)wchar_to_utf8((const wchar_t *)pMsg, (size_t)cbMsg, pDecodeMsg, (size_t)cbMsg*sizeof(unsigned int), 0);
	return cbDecodeMsg;
}

int smsrecv_decodeUnicodeFromGsmBit7(char* pMsg, unsigned int* pDecodeMsg, int cbMsg)
{
    return gsm2unicode(pMsg, cbMsg, pDecodeMsg, cbMsg);
}

/* pDecodeMsg : MAX_UNICODE_BUF_SIZE */
int smsrecv_decodeUtf8FromGsmBit7(char* pMsg, char* pDecodeMsg, int cbMsg)
{
    unsigned int* ptmpMsg = __alloc(MAX_UTF8_BUF_SIZE);
	int cbTmpMsg = 0, cbDecodeMsg = 0;
	if(!ptmpMsg) {
        syslog(LOG_ERR, "failed to allocate decode memory %d bytes", MAX_UTF8_BUF_SIZE);
        return -1;
	}
	/* decode GSM7 bits to Unicode, then decode to UTF-8 */	
    cbTmpMsg = smsrecv_decodeUnicodeFromGsmBit7(pMsg, ptmpMsg, cbMsg);
    syslog(LOG_DEBUG,"decoded to Unicode, len %d", cbTmpMsg);
    if (cbTmpMsg <= 0) {
        syslog(LOG_ERR, "Unicode decoding error");
        __free(ptmpMsg);
        return cbTmpMsg;
    }
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, pDecodeMsg, cbTmpMsg);
	syslog(LOG_DEBUG, "decoded to UTF-8, len %d", cbDecodeMsg);
    if (cbDecodeMsg <= 0) {
        syslog(LOG_ERR, "Unicode decoding error");
        __free(ptmpMsg);
        return cbDecodeMsg;
    }
	printMsgBody(pDecodeMsg, cbDecodeMsg);
        __free(ptmpMsg);
    return cbDecodeMsg;
}

/* pDecodeMsg : MAX_UNICODE_BUF_SIZE */
int smsrecv_decodeUtf8FromUnicodeStr(char* pMsg, char* pDecodeMsg, int cbMsg)
{
    unsigned int* ptmpMsg = __alloc(MAX_UTF8_BUF_SIZE);
	int cbTmpMsg = 0, cbDecodeMsg = 0;
	if(!ptmpMsg) {
        syslog(LOG_ERR, "failed to allocate decode memory %d bytes", MAX_UTF8_BUF_SIZE);
        return -1;
	}
    cbTmpMsg = ConvStrToInt((const char *)pMsg, cbMsg, ptmpMsg, cbMsg/4);
	syslog(LOG_DEBUG, "converted to Unicode integer, len %d", cbTmpMsg);
    if (cbTmpMsg <= 0) {
        syslog(LOG_ERR, "Unicode decoding error");
        __free(ptmpMsg);
        return cbTmpMsg;
    }
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, pDecodeMsg, cbTmpMsg);
	syslog(LOG_DEBUG, "decoded to UTF-8, len %d", cbDecodeMsg);
    if (cbDecodeMsg <= 0) {
        syslog(LOG_ERR, "Unicode decoding error");
        __free(ptmpMsg);
        return cbDecodeMsg;
    }
	printMsgBody(pDecodeMsg, cbDecodeMsg);
    __free(ptmpMsg);
    return cbDecodeMsg;
}



