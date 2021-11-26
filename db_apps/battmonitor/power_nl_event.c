/*
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
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
#include <stdlib.h>
#include <string.h>

#include "power_nl_event.h"

/* parse uevent and populate params in power_nl_event_t */
int parse_nl_event(power_nl_event_t *p_event, char *buffer, int size )
{
	char *s = buffer;
	char *end;

	int first = 1;

	if (size == 0){
		return 0;
	}

	/* Ensure the buffer is zero-terminated */
	buffer[size-1] = '\0';

	end = s + size;
	while (s < end) {
		if (first) {
			char *p;
			/*
			 * each line in uevent is terminated by '\0'
			 * first line must contain "@"
			 */
			for (p = s; *p != '@'; p++){
				if (!*p) {
					/* no '@' */
					return 0;
				}
			}
			first = 0;
		} else {
			/* only care about SUBSYSTEM power_supply */
			if (!strncmp(s, "SUBSYSTEM=", strlen("SUBSYSTEM="))){

				if (strncmp(s + strlen("SUBSYSTEM="), "power_supply", strlen("power_supply"))){
					return 0;
				}
			}
			else {
				if (p_event->param_counter < MAX_NL_PARAMS){
					p_event->param[p_event->param_counter++] = strdup(s);
				}
				else{
					/* too many params */
					return 0;
				}
			}
		}
		s+= strlen(s) + 1;
	}
	return 1;
}

/* clear power_nl_event_t */
void clear_nl_event(power_nl_event_t *p_event)
{
	int i;
	for (i = 0; i < p_event->param_counter && i < MAX_NL_PARAMS; i++) {
		if (!p_event->param[i]){
			break;
		}
		else{
			free(p_event->param[i]);
			p_event->param[i] = 0;
		}
	}
	p_event->param_counter = 0;
}

/* find param in a power_nl_event_t.
 * param format is NAME=VALUE
 * return param's value. */
const char *power_event_find_param(power_nl_event_t *p_event, const char *param_name)
{
	size_t len = strlen(param_name);

	for (int i = 0; p_event->param[i] && i < p_event->param_counter && i < MAX_NL_PARAMS; ++i) {
		const char *ptr = p_event->param[i] + len;

		if (!strncmp(p_event->param[i], param_name, len) && *ptr == '='){
			return ++ptr;
		}
	}

	return NULL;
}
