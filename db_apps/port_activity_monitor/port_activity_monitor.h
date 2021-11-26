#ifndef __PORT_ACTIVITY_MONITOR_H__02062016
#define __PORT_ACTIVITY_MONITOR_H__02062016

/*
 * This file declares the initialise_port_activity_monitors(),
 * update_port_activity_monitors() and
 * uninitialise_port_activity_monitor_chain functions defined in
 * port_activity_monitor.c and the associated data structure
 * port_activity_monitor for use by main().
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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

#include <time.h>
#include <limits.h>

/*
 * port_activity_monitor containing settings and
 * state for each port monitored.
 */

typedef struct _port_activity_monitor {
	/*
	 * The next entry in the linked list of port_activity_monitor s.
	 */
	struct _port_activity_monitor* next_port_activity_monitor;

	/*
	 * The network port to monitor.
	 */
	char network_port[NAME_MAX];

	/*
	 * These are initialised from
	 * sys.watchdog.packet_monitor.<network_port>.enabled,
	 * sys.watchdog.packet_monitor.<network_port>.max_reboots and
	 * sys.watchdog.packet_monitor.<network_port>.timeout
	 *
	 * Activity monitor activation status <1=enable|0=disable>.
	 */
	int		activity_monitor_enabled;

	/*
	 * Watchdog maximum reboot count. If the system reboots more times
	 * than this, the watchdog will not attempt to reboot the system again.
	 */
	int		activity_monitor_max_reboots;

	/*
	 * Watchdog timeout period in seconds.
	 * If shutdown does not complete within this time period, the system
	 * will be rebooted or reset.
	 */
	time_t	activity_monitor_timeout_seconds;

	/*
	 * These are set to the time activity was last detected
	 * from the relevant variables in /sys/class/net/<network_port>/statistics
	 */
	time_t	rx_last_activity_time;

	/*
	 * Set to the earliest of the above two variables.
	 */
	time_t	oldest_activity_time;

	/*
	 * The (future) time at which a recommit needs to be done in order
	 * to prevent the watchdog from expiring.
	 */
	time_t	next_recommit_check_time;
} port_activity_monitor;

/*
 * Read the comma separated list port_list_rdb_variable and
 * allocate and initialise a port activity monitor for each entry.
 * Returns 0 if the memory for the structure cannot be allocated.
 * The memory allocated for the structures can be released with
 * uninitialise_port_activity_monitor.
 */
port_activity_monitor* initialise_port_activity_monitors(const char* port_list_rdb_variable);

/*
 * Uninitialise and deallocate the chain of glb structure s.
 */
void uninitialise_port_activity_monitor_chain(port_activity_monitor* port_activity_monitor);

/*
 * For each port_activity_monitor in the list, check if the sysfs file
 * /sys/class/net/<network_port>/statistics/rx_bytes has
 * changed and if so, extend the time on its watchdog.
 * Update the RDB variable
 * sys.watchdog.packet_monitor.<network_port>.rx_bytes with
 * the values from the sysfs file.
 */
void update_port_activity_monitors(port_activity_monitor* port_activity_monitor);

#endif /* __PORT_ACTIVITY_MONITOR_H__02062016 */
