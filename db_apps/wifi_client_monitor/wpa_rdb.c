/*
 * wpa_rdb.c
 * Helper functions to work with RDB
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
#include <errno.h>
#include <stdio.h>
#include <rdb_ops.h>
#include "wpa_supp.h"

#define RDB_BUFFER_SIZE 10

#define RDB_WPA_EXTRA_STATUS_UNKNOWN	"0"
#define RDB_WPA_EXTRA_STATUS_ASSOC_REJECT	"1"
#define RDB_WPA_EXTRA_STATUS_EAP_SUCCESS	"2"
#define RDB_WPA_EXTRA_STATUS_EAP_FAILURE	"3"
#define RDB_WPA_EXTRA_STATUS_MAYBE_INCORRECT_PASSWORD	"4"

/* rdb session */
static struct rdb_session *rdb_session = NULL;
char wlan_sta_extra_status_rdb[128];

/*
 * Initialise RDB
 */
int init_rdb(const char *rdb_idx_str)
{
	int ret;
	char dbb[RDB_BUFFER_SIZE];
	int len = sizeof(dbb);

	if (!rdb_idx_str){
		return -EINVAL;
	}

	ret = rdb_open(NULL, &rdb_session);
	if (ret){
		return ret;
	}

	snprintf(wlan_sta_extra_status_rdb, sizeof(wlan_sta_extra_status_rdb), "wlan_sta.%s.extra_status", rdb_idx_str);
	/* create rdb variable if necessary */
	if (rdb_get_string(rdb_session, wlan_sta_extra_status_rdb, dbb, len)){
		ret = rdb_create_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_UNKNOWN, 0, DEFAULT_PERM);
		if (ret){
			return ret;
		}
	}

	return 0;

}

/*
 * Update RDB
 */
int update_rdb(wpa_event_parse_t event)
{
	static int scan_started_count = 0;

	/*
	 * After association fails due to incorrect password, wpa_supplicant starts scan nearly immediately.
	 * That makes Web UI which is polling at slow speed unable to get updated of previous status.
	 * Hence only reset extra status if scan starts 3 times in a row.
	 */
	if (event != PARSE_WPA_EVENT_SCAN_STARTED && event != PARSE_WPA_EVENT_SCAN_RESULTS && event != PARSE_WPA_UNKNOWN){
		scan_started_count = 0;
	}

	switch (event){
		case PARSE_WPA_EVENT_CONNECTED:
			rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_UNKNOWN);
			/* if wifi state status is also derived from rdb variable, it will be set here */
			break;

		case PARSE_WPA_EVENT_ASSOC_REJECT:
			rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_ASSOC_REJECT);
			break;

		case PARSE_WPA_EVENT_EAP_SUCCESS:
			rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_EAP_SUCCESS);
			break;

		case PARSE_WPA_EVENT_EAP_FAILURE:
			rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_EAP_FAILURE);
			break;

		case PARSE_WPA_EVENT_SCAN_STARTED:
			scan_started_count++;
			if (scan_started_count >= 3){
				scan_started_count = 0;
				rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_UNKNOWN);
			}
			break;

		case PARSE_WPA_SUPP_TERMINATING:
			rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_UNKNOWN);
			break;

		case PARSE_WPA_MAYBE_INCORRECT_PASSWORD:
			rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_MAYBE_INCORRECT_PASSWORD);
			break;

		default:
			break;
	}

	return 0;
}

/*
 * Reset RDB variables
 */
void reset_rdb(void)
{
	if (rdb_session){
		rdb_set_string(rdb_session, wlan_sta_extra_status_rdb, RDB_WPA_EXTRA_STATUS_UNKNOWN);
	}
}

/*
 * Reset and free RDB resources
 */
void deinit_rdb(void)
{
	if (rdb_session){
		reset_rdb();
		rdb_close(&rdb_session);
		rdb_session = NULL;
	}
}
