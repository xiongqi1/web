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
#include "rdb_ops.h"

#define MAX_LINE_LENGTH	1024
/* This character set can be sent with GSM7 bit mode SMS by Sierra Wireless modems.
 * The other special characters "ìòΞ^\[]{}~|€" defined in GSM 03.38 can not be sent so should be
 * sent with UCS2 mode */
const char gsm7_char_set[] = "@£$¥èéùÇØøÅåΔ_ΦΓΛΩΠΨΣΘÆæßÉ!\"#¤%&'()*+,-./:;<=>?¡ÄÖÑÜ§¿äöñüà \n\r";
/* GSM7 bit extended character set "ìòΞ^\\[]{}~|€" can not be sent with Sierra modems with GSM7 bit mode */
const char gsm7_char_set_ext[] = "@£$¥èéùÇØøÅåΔ_ΦΓΛΩΠΨΣΘÆæßÉ!\"#¤%&'()*+,-./:;<=>?¡ÄÖÑÜ§¿äöñüà \n\rìòΞ^\\[]{}~|€";
#define RDB_VAR_SMS_SP			"wwan.0.sms.has_special_chars"
#define RDB_VAR_MANUFACTURER	"wwan.0.manufacture"
int main (int argc, char **argv)
{
	char* text;
	char *cp = (char *)&gsm7_char_set_ext[0];
	int i, msg_len;
	char ch;

	// For help text
	if (argc >= 2) {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
            printf("\nThis command is for internal system use only.\n");
            printf("It is used for checking special characters in"
                   " TX SMS message strings.\n");
            printf("Please do not run this command manually.\n\n");
            return -1;
        }
    }

	if (argc < 2) {
		syslog(LOG_ERR, "Usage : check_sp strings\n\n");
		return -1;
	}

	/* we copy the manufacturer string from rdb having max length 1024 */
	msg_len=strlen(argv[1])+1;
	text = malloc(msg_len>MAX_LINE_LENGTH? msg_len:MAX_LINE_LENGTH);
	if (!text) {
		syslog(LOG_ERR, "memory allocation(%d bytes) failed\n", MAX_LINE_LENGTH);
		return -1;
	}
	(void) memset(text, 0x00, MAX_LINE_LENGTH);

	if (rdb_open_db() <= 0)
	{
		syslog(LOG_ERR, "failed to open database!\n");
		free(text);
		return -1;
	}

	/* read modem manufacture to find Sierra Wireless modems */
	if (rdb_get_single(RDB_VAR_MANUFACTURER, text, MAX_LINE_LENGTH) == 0 &&
		strncmp(text, "Sierra Wireless", strlen("Sierra Wireless")) == 0) {
		syslog(LOG_ERR, "found Sierra Wireless modem, send extended GSM7 characters in UCS2 mode");
		cp = (char *)&gsm7_char_set[0];
	}
	(void) strcpy(&text[0], argv[1]);
	//syslog(LOG_ERR, "check txt: %s\n", text);

	for (i = 0; i < (msg_len-1); i++)	{
		ch = text[i];
		if (strchr(cp, ch) ||
			(ch >= '0' && ch <= '9') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= 'a' && ch <= 'z')) {
			continue;
		} else {
			syslog(LOG_ERR, "found special char: %c at %d\n", ch, i);
			rdb_set_single(RDB_VAR_SMS_SP, "1");
			rdb_close_db();
			free(text);
			return 0;
		}
	}
	//syslog(LOG_ERR, "no special char\n");
	rdb_set_single(RDB_VAR_SMS_SP, "0");
	rdb_close_db();
	free(text);
	return 0;
}

/*
* vim:ts=4:sw=4:
*/
