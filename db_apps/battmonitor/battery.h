/*
 * Define general interface for battery implementation
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
 *
 */
#ifndef _BATTERY_H
#define _BATTERY_H

#include "power_nl_event.h"

typedef enum {
	UNKNOWN,
	NOT_CHARGING,
	DISCHARGING,
	CHARGING,
	FULL,
} battery_charging_status_t;

/* specific battery implementation */
typedef struct  {

	/* specify timeout (milliseconds) in polling power event.
	 * complying with poll() function: -1 block until a requested event occurs or until the call is interrupted
	 */
	int (*poll_timeout_msecs)(void);

	/* process received power event
	 * Return:
	 * 	1: success
	 * 	0: error
	 */
	int (*process_power_event)(power_nl_event_t* power_nl_event);

	/* process in case event polling timed out
	 * Return:
	 * 	1: success
	 * 	0: error
	 */
	int (*poll_timeout_process)(void);

	/* get battery online status
	 * IMPORTANT: this must be available to make other getter functions active
	 * Return:
	 * 		true: online
	 * 		false: not online
	 */
	int (*get_battery_online_status)(void);

	/* get battery status
	 *
	 * Return: charging status as a battery_charging_status_t
	 */
	battery_charging_status_t (*get_battery_charging_status)(void);

	/* get battery percents */
	int (*get_battery_percents)(void);

	/* get battery voltage in mV */
	int (*get_battery_voltage)(void);

	/* over max temperature alert
	 * Returns:
	 * 1:	Alert
	 * 0:	No alert
	 */
	int (*over_max_temperature_alert)(void);

	/* under min temperature alert
	 * Returns:
	 * 1:	Alert
	 * 0:	No alert
	 */
	int (*under_min_temperature_alert)(void);

	/* update other systems */
	void (*update_system)(void);
} battery_ops_t;

/* setup and register specific battery implementation
 *
 * Parameters:
 * 	ops:	battery_ops_t to attach specific implementation
 */
void setup_battery(battery_ops_t *ops);

#endif /* _BATTERY_H */
