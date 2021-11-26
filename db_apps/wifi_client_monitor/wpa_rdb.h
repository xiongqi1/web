/*
 * wpa_rdb.h
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

#ifndef WPA_RDB_H_
#define WPA_RDB_H_
#include "wpa_supp.h"

/*
 * Initialise RDB session and related variable
 * @rdb_idx: index in RDB variable (e.g for extra status wlan_sta.{INDEX}.extra_status)
 *
 * returns 0 if success, negative error code otherwise
 */
int init_rdb(const char *rdb_idx);

/*
 * Update RDB variable
 * @event: parsed wpa_supplicant event
 */
int update_rdb(wpa_event_parse_t event);

/*
 * Reset RDB variables
 */
void reset_rdb(void);

/*
 * Reset and free RDB resources
 */
void deinit_rdb(void);

#endif /* WPA_RDB_H_ */
