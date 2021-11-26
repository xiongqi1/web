/*
 * wpa_supp.h
 * Define supporting types and functions to parse wpa_supplicant events and messages
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

#ifndef WPA_SUPP_H_
#define WPA_SUPP_H_

/* return value of parsing function */
typedef enum {
	/* Authentication completed successfully and data connection enabled */
	PARSE_WPA_EVENT_CONNECTED,
	/* AP-STA is disconnected */
	PARSE_WPA_EVENT_DISCONNECTED,
	/* association is rejected */
	PARSE_WPA_EVENT_ASSOC_REJECT,
	/* EAP authentication completed successfully */
	PARSE_WPA_EVENT_EAP_SUCCESS,
	/* EAP authentication failed */
	PARSE_WPA_EVENT_EAP_FAILURE,
	/* scan started */
	PARSE_WPA_EVENT_SCAN_STARTED,
	/* scan results */
	PARSE_WPA_EVENT_SCAN_RESULTS,
	/* wpa_supplicant is exiting */
	PARSE_WPA_SUPP_TERMINATING,
	/* maybe password is incorrect */
	PARSE_WPA_MAYBE_INCORRECT_PASSWORD,
	/* unknown message */
	PARSE_WPA_UNKNOWN,
	/* error on parsing */
	PARSE_WPA_PARSE_ERROR,
} wpa_event_parse_t;

/*
 * parse wpa_supplicant messages
 * @msg: message to be parsed
 */
wpa_event_parse_t parse_wpa_supplicant_msg(char *msg);

#endif /* WPA_SUPP_H_ */
