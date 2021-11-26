/**
 * @file battery_rdb.h
 * @brief RDB operation utilities for battery manager
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef BATTERY_RDB_H_10480913032019
#define BATTERY_RDB_H_10480913032019

#include <rdb_ops.h>
#include "charger.h"
#include "monitor.h"
#include <stdint.h>

/*
 * initialise RDB
 *
 * @return 0 on success; -1 on error
 */
int init_rdb(void);

/*
 * Update RDBs with given charge status
 *
 * @param stats A pointer to struct charge_stats that contains RDB values to be written
 * @return 0 on success; non-zero error code on failure
 */
int update_charge_rdbs(const struct charge_stats * stats);

/*
 * Update RDBs with given battery monitor status
 *
 * @param stats A pointer to struct monitor_stats that contains RDB values to be written
 * @return 0 on success; non-zero error code on failure
 */
int update_monitor_rdbs(const struct monitor_stats * stats);

/*
 * Get RDB file descriptor
 *
 * @return RDB file descriptor or negative error code
 */
int get_rdb_fd(void);

/*
 * Check if poweroff RDB is set to 1
 *
 * @return 1 if poweroff RDB is set to 1; 0 otherwise
 */
int poweroff_rdb_set(void);

/*
 * Finalise poweroff RDB trigger to notify other system components
 *
 * @return 0 on success; non-zero error code on failure
 */
int poweroff_rdb_finalise(void);

/*
 * Set poweroff RDB to 1 or 11 to initiate poweroff procedure
 *
 * @param force Whether to force poweroff. When force is set to 1, the poweroff
 * RDB will be set to '11' instead of the normal '1', which will skip install
 * tool mode checking and force an emergency poweroff procedure.
 * @return 0 on success; non-zero error code on failure
 */
int initiate_poweroff(int force);

/*
 * Check if OWA is connected
 *
 * @return 1 if connected; 0 otherwise
 */
int is_owa_connected(void);

/*
 * Terminate/cleanup RDB
 *
 * @note This undoes init_rdb and should be called before program exits
 */
void term_rdb(void);

/*
 * Write didt - the changing rate of current, to RDB
 *
 * @param didt Current changing rate
 * @return 0 on success; non-zero error code on failure
 */
int write_didt(int didt);

/*
 * Set battery level of the current power cycle
 *
 * @param level Battery level
 * @return 0 on success; non-zero error code on failure
 */
int set_persisted_level(uint8_t level);

/*
 * Get battery level of the previous power cycle
 *
 * @return battery level, 0xFF if there is no level saved or an error happens
 */
uint8_t get_persisted_level();

#endif // BATTERY_RDB_H_10480913032019
