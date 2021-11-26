/*!
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
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

#ifndef EVENT_RDB_NAMES_H_20140623_
#define EVENT_RDB_NAMES_H_20140623_

#define RDB_NETWORKREGISTERED 		"system_network_status.registered"
#define RDB_NETWORKREG_STAT 		"system_network_status.reg_stat"
#define RDB_NETWORKROAM_STAT 		"system_network_status.roaming"
#define RDB_NETWORKCELLID_STAT 		"system_network_status.CellID"
#define RDB_NETWORK_SYSMODE 		"system_network_status.system_mode"
#define RDB_LINK_PROFILE 		"link.profile"
#define RDB_SYS_HW_CLASS_ETH		"sys.hw.class.eth"
#define RDB_SYS_POWER_SOURCE		"sys.sensors.info.powersource"

#define RDB_SDCARD_STAT			"sys.hw.class.storage.0.status"
#define RDB_FAILOVER_PRIMARY		"service.failover.x.primary"
#define RDB_WLAN_RESERVATION_MAC	"wlan.reservation.mac"
#define RDB_WLAN_CLIENTLIST		"wlan.client_list.mac"
#define RDB_IO_PREFIX				"sys.sensors.io"

#define RDB_RESET_DISABLE			"hw.reset.disable"
#define RDB_VRRP_MODE			"service.vrrp.currmode"

#define RDB_EVENT_NOTI_PREPIX		"service.eventnoti"
#define RDB_EVENT_NOTI_CMD_PREPIX	RDB_EVENT_NOTI_PREPIX".cmd"
#define RDB_EVENT_NOTI_CONF_PREPIX	RDB_EVENT_NOTI_PREPIX".conf"
#define RDB_EVENT_NOTI_TYPE_PREPIX	RDB_EVENT_NOTI_PREPIX".conf.type"
#define RDB_EVENT_NOTI_EVENT_PREPIX	RDB_EVENT_NOTI_PREPIX".event"

#define RDB_EVENT_NOTI_MAX_SIZE		RDB_EVENT_NOTI_CONF_PREPIX".max_size"
#define RDB_EVENT_NOTI_RD_IDX		RDB_EVENT_NOTI_CONF_PREPIX".rd_idx"
#define RDB_EVENT_NOTI_WR_IDX		RDB_EVENT_NOTI_CONF_PREPIX".wr_idx"
#define RDB_EVENT_NOTI_RESULT		RDB_EVENT_NOTI_CMD_PREPIX".result"

#endif /* EVENT_RDB_NAMES_H_20140623_ */

