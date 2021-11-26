/*
 * rdb_defs.h
 * Define some helper macros related to module RDB names and values
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

#ifndef SRC_FEATURE_V_MODULE_RDB_DEFS_H_
#define SRC_FEATURE_V_MODULE_RDB_DEFS_H_

#define RDB_MODULE_PREFIX "wwan.0"
#define RDB_MODULE_SIM_STATUS_VAR RDB_MODULE_PREFIX".sim.status.status"
#define RDB_MODULE_NETWORK_REGISTRATION_VAR RDB_MODULE_PREFIX".system_network_status.reg_stat"
#define RDB_MODULE_CURRENT_OPERATOR_VAR RDB_MODULE_PREFIX".system_network_status.network"
#define RDB_MODULE_CURRENT_BAND_VAR RDB_MODULE_PREFIX".system_network_status.current_band"
/* support for SIM status */
#define RDB_PUK_RETRIES_VAR RDB_MODULE_PREFIX".sim.status.retries_puk_remaining"
#define RDB_MANUAL_ROAM_RESETTING_VAR "manualroam.resetting"
#define RDB_AUTO_PIN_VAR RDB_MODULE_PREFIX".sim.autopin"
/* support for change band */
#define RDB_MODULE_BAND_LIST_VAR RDB_MODULE_PREFIX".module_band_list"
#define RDB_MODULE_BAND_LIST2_VAR RDB_MODULE_PREFIX".currentband.current_band"
#define RDB_PLMN_SELECTION_MODE_VAR RDB_MODULE_PREFIX".PLMN_selectionMode"
#define RDB_BAND_CMD_STATUS RDB_MODULE_PREFIX".currentband.cmd.status"
#define RDB_BAND_CMD_PARAM RDB_MODULE_PREFIX".currentband.cmd.param.band"
#define RDB_BAND_CMD_BACKUP_CONFIG RDB_MODULE_PREFIX".currentband.backup_config"
#define RDB_BAND_CMD_COMMAND RDB_MODULE_PREFIX".currentband.cmd.command"
#define RDB_MODULE_TYPE RDB_MODULE_PREFIX".module_type"
#define RDB_PLMN_SELECT RDB_MODULE_PREFIX".PLMN_select"
#define RDB_PLMN_COMMAND_STATE RDB_MODULE_PREFIX".PLMN_command_state"

/* registration status */
#define REG_UNREGISTERED_NO_SEARCH 0
#define REG_REGISTERED_HOME	1
#define REG_UNREGISTERED_SEARCHING	2
#define REG_REGISTRATION_DENIED	3
#define REG_UNKNOWN	4
#define REG_REGISTERED_ROAMING	5
#define REG_REGISTERED_SMS_HOME	6
#define REG_REGISTERED_SMS_ROAMING	7
#define REG_EMERGENCY	8
#define REG_NA	9
#define REG_MAX	10

#endif /* SRC_FEATURE_V_MODULE_RDB_DEFS_H_ */
