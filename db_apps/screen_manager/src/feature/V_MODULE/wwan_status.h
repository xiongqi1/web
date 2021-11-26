/*
 * wwan_status.h
 * WWAN status data structure and updating function
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef SRC_FEATURE_V_MODULE_WWAN_STATUS_H_
#define SRC_FEATURE_V_MODULE_WWAN_STATUS_H_

#include <rdb_ops.h>

#define STATUS_LENGTH 64
#define IP_LENGTH 16
#define APN_LENGTH 32

typedef struct wwan_data_ {
	char status[STATUS_LENGTH];
	char ip[IP_LENGTH];
	char apn[APN_LENGTH];
} wwan_data_t;

/*
 * read wwan status
 * @rdb_sess: rdb session
 * @p_wwan_status_data: pointer to output wwan status data structure
 */
void scrm_module_update_wwan_status(struct rdb_session *rdb_sess, wwan_data_t *p_wwan_status_data);

#endif /* SRC_FEATURE_V_MODULE_WWAN_STATUS_H_ */
