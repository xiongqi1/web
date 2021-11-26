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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "cdcs_types.h"
#include "cdcs_syslog.h"
#include "at_util.h"

BOOL is_at_command(const char* command)
{
	if (!(command && (command[0] == 'a' || command[0] == 'A') && (command[1] == 't' || command[1] == 'T')))
	{
		return FALSE;
	}
	for (; *command; ++command)
	{
		if (*command < 32 || *command == 127)
		{
			return FALSE;
		}
	}
	return TRUE;
}

int tokenize_at_response(char* response)
{
	char* r;
	BOOL is_first_token = TRUE;
	int count = 1;
	BOOL quoted = FALSE;
	for (r = response; *r != '\0'; ++r)
	{
		switch (*r)
		{
			case ',':
				if (quoted)
				{
					break;
				}
				++count;
				*r = '\0';
				break;
			case ' ':
				if (!is_first_token || r == response || *(r - 1) != ':')
				{
					break;
				}
				is_first_token = FALSE;
				++count;
				*r = '\0';
				break;
			case '"':
				quoted = !quoted;
				break;
			default:
				break;
		}
	}
	if (quoted)
	{
		SYSLOG_ERR("invalid AT response '%s'", response);
		return -1;
	}
	return count;
}

int tokenize_with_semicolon(char* response)
{
	char* r;
	BOOL is_first_token = TRUE;
	int count = 1;
	BOOL quoted = FALSE;
	for (r = response; *r != '\0'; ++r)
	{
		switch (*r)
		{
			case ';':
				if (quoted)
				{
					break;
				}
				++count;
				*r = '\0';
				break;
			case ' ':
				if (!is_first_token || r == response || *(r - 1) != ':')
				{
					break;
				}
				is_first_token = FALSE;
				++count;
				*r = '\0';
				break;
			case '"':
				quoted = !quoted;
				break;
			default:
				break;
		}
	}
	if (quoted)
	{
		SYSLOG_ERR("invalid AT response '%s'", response);
		return -1;
	}
	return count;
}
const char* get_token(const char* tokenized, unsigned int index)
{
	while (index--)
	{
		tokenized += (strlen(tokenized) + 1);
	}
	return tokenized;
}

int get_rid_of_newline_char(char * input_string)
{
	char * pos1;

	if(!input_string)
		return -1;

	pos1 = strrchr(input_string, '\n');
	while(pos1 != NULL)
	{
		strcpy(pos1, pos1+1);
		pos1 = strrchr(input_string, '\n');
	}
	return 0;
}

void str_replace(char *st, const char *orig, const char *repl) {
    static char buffer[256];
    char *ch;
    if (strlen(st) > 256) {
        SYSLOG_ERR("too long string to run this replace command");
        return;
    }
    if (!(ch = strstr(st, orig)))
        return;
    strncpy(buffer, st, ch-st);
    buffer[ch-st] = 0;
    sprintf(buffer+(ch-st), "%s%s", repl, ch+strlen(orig));
    (void) strcpy(st, buffer);
}

void* __alloc(int cbLen)
{
	void* pMem = malloc(cbLen+1);
	if (pMem) __zeroMem(pMem, cbLen+1);
	else SYSLOG_ERR("mem alloc failed for %d bytes request", cbLen+1);
	return pMem;
}

void __free(void* pMem)
{
	if (pMem) free(pMem);
}

const char* get_str_block(const char* in)
{
	static char token[256];

	int psp;
	int sp=1;

	char* t=token;
	char ch;

	while(psp=sp,(ch=*in++)!=0) {
		
		sp=isspace(ch);

		/* skip starting spaces */
		if(psp && sp) {
			continue;
		}
		/* break */
		else if(!psp && sp) {
			break;
		}

		*t++=ch;
	}

	*t=NULL;

	return token;
}

