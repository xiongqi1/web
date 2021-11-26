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

#ifndef CODING_H_20120830
#define CODING_H_20120830

#define MAX_UTF8_BUF_SIZE			1024
#define MAX_UNICODE_BUF_SIZE		MAX_UTF8_BUF_SIZE * sizeof(unsigned int)

#ifdef NO_USE
int smssend_encodeGsmBit7(char* pMsg, char* pEncodeMsg, int cbMsg);
#endif
int ConvStrToChar(const char* pSrc, int cbSrc, char* pDst, int cbDst);
int ConvStrToInt(const char* pSrc, int cbSrc, unsigned int* pDst, int cbDst);
int smssend_encodeUnicodeFromUtf8(char* pMsg, int cbpMsg, unsigned int* pUnicodeMsg, int cbpUnicodeMsg);
int smssend_encodeGsmBit7FromUnicode(unsigned int* pMsg, char* pEncodeMsg, int cbMsg);
int smssend_encodeASCIIFromUnicode(unsigned int* pMsg, char* pEncodeMsg, int cbMsg);
int smsrecv_decodeUnicodeFromGsmBit7(char* pMsg, unsigned int* pDecodeMsg, int cbMsg);
int smsrecv_decodeUnicodeFromASCII(char* pMsg, unsigned int* pDecodeMsg, int cbMsg);
int smsrecv_decodeUtf8FromUnicode(unsigned int* pMsg, char* pDecodeMsg, int cbMsg);
int smsrecv_decodeUtf8FromGsmBit7(char* pMsg, char* pDecodeMsg, int cbMsg);
int smsrecv_decodeUtf8FromUnicodeStr(char* pMsg, char* pDecodeMsg, int cbMsg);

#endif  /* CODING_H_20120830 */

