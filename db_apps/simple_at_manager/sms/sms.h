/*!
* Copyright Notice:
* Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
* CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
* SUCH DAMAGE.
*
*/

#ifndef HEADER_GUARD_SMS_20110628_
#define HEADER_GUARD_SMS_20110628_

#include "../model/model.h"
#include "../at/at.h"

//#define ZERO_LEN_PDU_MSG_TEST
//#define LONG_PDU_MSG_TEST
//#define ALPHANUMERIC_DEST_NUM_PDU_MSG_TEST

#define SET_LOG_LEVEL_TO_SMS_LOGMASK_LEVEL	setlogmask(LOG_UPTO(log_db[LOGMASK_SMS].loglevel));
#define SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL   setlogmask(LOG_UPTO(log_db[LOGMASK_AT].loglevel));


/* use UTF8 <--> UNICODE conversion function defined in utf8.c */
#define UTF_8_LIB_FUNCTION

#define BSIZE_16		        16
#define BSIZE_32		        32
#define BSIZE_64		        64
#define BSIZE_128		        128
#define BSIZE_256		        256
#define BSIZE_512		        512
#define BSIZE_1024		        1024

#define UINT_SIZE				sizeof(unsigned int)

/* max user data field length for each cases */
#define MAX_UD_LEN_SINGLE_GSM7	160		/* 160 * 7 bits = 1120 bits = 140 bytes */ 	
#define MAX_UD_LEN_SINGLE_UCS2	70		/* 70 * 16 bits = 1120 bits = 140 bytes */
#define MAX_UD_LEN_CONCAT_GSM7	(MAX_UD_LEN_SINGLE_GSM7-7)	/* 140 - 6 bytes header = 134 bytes = 1072 bits, 1072/7 = 153 GSM7 char */
#define MAX_UD_LEN_CONCAT_UCS2	(MAX_UD_LEN_SINGLE_UCS2-3)	/* 140 - 6 bytes header = 134 bytes = 1072 bits, 1072/16 = 67 UCS2 char*/

typedef enum {
	REC_UNREAD = 0,
	REC_READ,
	STO_UNSENT,
	STO_SENT,
	ALL
} sms_msg_type;

extern int notiSMSRecv(const char* s);
extern int read_smstools_inbox_path(void);
extern int handleNewSMSIndicator(const char* s);
extern int default_handle_command_sms(const struct name_value_t* args);
int UpdateSmsMemStatus(int updateMem);
void printMsgBody(char* msg, int len);
void printMsgBodyInt(int* msg, int len);
int char_set_save_restore(int save);

#endif // HEADER_GUARD_SMS_20110628_

/*
* vim:ts=4:sw=4:
*/
