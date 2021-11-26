/*
 * wpa_supp.c
 * Supporting functions to parse wpa_supplicant events and messages
 *
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
 */

#include <string.h>
#include <wpa_ctrl.h>
#include "wpa_supp.h"

#define PASSWORD_MAY_BE_INCORRECT_STR "WPA: 4-Way Handshake failed - pre-shared key may be incorrect"
#define str_start_with(s1, s2) (strncmp(s1, s2, strlen(s2)) == 0)

/*
 * parse wpa_supplicant messages
 */
wpa_event_parse_t parse_wpa_supplicant_msg(char *msg)
{
	char *pos = msg;

	if (!msg){
		return PARSE_WPA_PARSE_ERROR;
	}

	/* skip priority indication if any */
	if (*pos == '<') {
		pos = strchr(pos, '>');
		if (pos)
			pos++;
		else
			pos = msg;
	}
	/* parse */
	if (str_start_with(pos, WPA_EVENT_CONNECTED)){
		/* AP-STA connected */
		return PARSE_WPA_EVENT_CONNECTED;
	}
	else if (str_start_with(pos, WPA_EVENT_DISCONNECTED)){
		/* AP-STA disconnected */
		return PARSE_WPA_EVENT_DISCONNECTED;
	}
	else if (str_start_with(pos, WPA_EVENT_ASSOC_REJECT)){
		/* assocication is rejected */
		return PARSE_WPA_EVENT_ASSOC_REJECT;
	}
	else if (str_start_with(pos, WPA_EVENT_EAP_SUCCESS)){
		/* EAP authentication completed successfully */
		return PARSE_WPA_EVENT_EAP_SUCCESS;
	}
	else if (str_start_with(pos, WPA_EVENT_EAP_FAILURE)){
		/* EAP authentication failed */
		return PARSE_WPA_EVENT_EAP_FAILURE;
	}
	else if (str_start_with(pos, WPA_EVENT_SCAN_STARTED)){
		/* scan started */
		return PARSE_WPA_EVENT_SCAN_STARTED;
	}
	else if (str_start_with(pos, WPA_EVENT_SCAN_RESULTS)){
		/* scan results */
		return PARSE_WPA_EVENT_SCAN_RESULTS;
	}
	else if (str_start_with(pos, WPA_EVENT_TERMINATING)){
		/* wpa_supplicant is terminating */
		return PARSE_WPA_SUPP_TERMINATING;
	}
	else if (str_start_with(pos, PASSWORD_MAY_BE_INCORRECT_STR)){
		/* password may be incorrect */
		return PARSE_WPA_MAYBE_INCORRECT_PASSWORD;
	}

	return PARSE_WPA_UNKNOWN;
}
