/*
 * batman:
 *    Battery monitor daemon. Register and parse uevents related to power supply
 *    and update RDB variables.
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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/netlink.h>

#include <rdb_ops.h>

#include "power_nl_event.h"
#include "battery.h"

#define DEFAULT_POLL_TIMEOUT_MS	5000

#define RDB_BATTERY_STATUS "battery.status"
#define RDB_BATTERY_PERCENT "battery.percent"
#define RDB_BATTERY_VOLTAGE "battery.voltage"
#define RDB_BATTERY_ONLINE "battery.online"
#define RDB_BATTERY_EVENT "battery.event"

/* rdb session */
static struct rdb_session *rdb_session = NULL;

/* active=0 will stop daemon */
static volatile int active=1;

/* previous battery percents reported */
static int prev_percent=-1;
/* previous battery online status reported */
static int prev_online=0;
/* previous battery charging status reported */
static battery_charging_status_t prev_charging_status = UNKNOWN;
/* previous temperature alert reported */
static int prev_max_temp_alert=0;
static int prev_min_temp_alert=0;

#define BATTERY_EVENT_EMPTY "1"
#define BATTERY_EVENT_MAX_TEMP_ALERT "2"
#define BATTERY_EVENT_MIN_TEMP_ALERT "3"

/* general signal handler */
static void signal_handler(int sig)
{
	active=0;
}

#define RDB_BUFFER_SIZE 10

/* Update RDB variables. Only update if something changes.
 *
 * Parameters:
 * 	battery_ops:	specific battery functions
 */
static void update_rdb(battery_ops_t *battery_ops)
{
	int cur_percent = -1;
	int cur_max_temp_alert = -1;
	int cur_min_temp_alert = -1;
	battery_charging_status_t cur_charging_status = UNKNOWN;
	int charging_status_changed = 0;
	char dbb[RDB_BUFFER_SIZE];

	if (!battery_ops){
		return;
	}

	/* check battery online status first */
	if (battery_ops->get_battery_online_status){

		if (battery_ops->get_battery_online_status()){
			/* battery is online */

			/* update battery online rdb */
			if (!prev_online){
				rdb_set_string(rdb_session, RDB_BATTERY_ONLINE, "1");
				prev_online = 1;
			}

			/* update charging status */
			if (battery_ops->get_battery_charging_status){
				cur_charging_status = battery_ops->get_battery_charging_status();

				if (cur_charging_status != prev_charging_status){
					switch (cur_charging_status){
						case UNKNOWN:
							rdb_set_string(rdb_session, RDB_BATTERY_STATUS, "u");
							break;

						case NOT_CHARGING:
							rdb_set_string(rdb_session, RDB_BATTERY_STATUS, "n");
							break;

						case DISCHARGING:
							rdb_set_string(rdb_session, RDB_BATTERY_STATUS, "d");
							break;

						case CHARGING:
							rdb_set_string(rdb_session, RDB_BATTERY_STATUS, "c");
							break;

						case FULL:
							rdb_set_string(rdb_session, RDB_BATTERY_STATUS, "f");
							break;

						default:
							break;
					}
					charging_status_changed = 1;
					prev_charging_status = cur_charging_status;
				}
				else{
					charging_status_changed = 0;
				}
			}
			else{
				/* for completeness as it is unlikely that battery_ops->get_battery_charging_status is changed */
				charging_status_changed = 0;
			}

			/* update battery percents */
			if (battery_ops->get_battery_percents) {
				cur_percent = battery_ops->get_battery_percents();

				if (prev_percent != cur_percent || charging_status_changed){
					sprintf(dbb,"%d",cur_percent);
					rdb_set_string(rdb_session, RDB_BATTERY_PERCENT, dbb);
					prev_percent=cur_percent;

					/* if battery is empty while discharging, set event */
					/* this alert is critical and once alert is set, the rebooting procedure started,
					 * so no need to check whether this alert are cleared */
					if (cur_percent==0 && cur_charging_status==DISCHARGING){
						rdb_set_string(rdb_session, RDB_BATTERY_EVENT, BATTERY_EVENT_EMPTY);
					}
				}
			}

			/* update battery voltage */
			if(battery_ops->get_battery_voltage){
				sprintf(dbb,"%d",battery_ops->get_battery_voltage());
				rdb_set_string(rdb_session, RDB_BATTERY_VOLTAGE, dbb);
			}

			/* over max alert */
			if(battery_ops->over_max_temperature_alert){
				cur_max_temp_alert = battery_ops->over_max_temperature_alert();
				if (prev_max_temp_alert != cur_max_temp_alert){
					/* set alert for over max temperature regardless of charging status */
					/* this alert is critical and once alert is set, the rebooting procedure started,
					 * so no need to check whether this alert are cleared */
					if (cur_max_temp_alert){
						rdb_set_string(rdb_session, RDB_BATTERY_EVENT, BATTERY_EVENT_MAX_TEMP_ALERT);
					}
					prev_max_temp_alert=cur_max_temp_alert;
				}
			}
			/* under min alert */
			if(battery_ops->under_min_temperature_alert){
				cur_min_temp_alert = battery_ops->under_min_temperature_alert();
				if (prev_min_temp_alert != cur_min_temp_alert || charging_status_changed){
					/* set alert for under min temperature while discharging */
					/* this alert is critical and once alert is set, the rebooting procedure started,
					 * so no need to check whether this alert are cleared */
					if (cur_min_temp_alert && cur_charging_status==DISCHARGING){
						rdb_set_string(rdb_session, RDB_BATTERY_EVENT, BATTERY_EVENT_MIN_TEMP_ALERT);
					}
					prev_min_temp_alert=cur_min_temp_alert;
				}
			}
		}
		else{
			/* battery is offline */

			if (prev_online){
				prev_online = 0;
				rdb_set_string(rdb_session, RDB_BATTERY_ONLINE, "0");

				/* update charging status */
				prev_charging_status = UNKNOWN;
				rdb_set_string(rdb_session, RDB_BATTERY_STATUS, "u");

				/* update battery percents */
				prev_percent=0;
				rdb_set_string(rdb_session, RDB_BATTERY_PERCENT, "0");

				/* update battery voltage */
				rdb_set_string(rdb_session, RDB_BATTERY_VOLTAGE, "0");
			}
		}
	}
}

/* Set up initial values for RDB variables
 *
 * Parameters:
 * 	battery_ops:	specific battery functions
 */
static void init(battery_ops_t *battery_ops)
{
	char dbb[RDB_BUFFER_SIZE];

	if (!battery_ops){
		return;
	}

	/* check battery online status first */
	if (battery_ops->get_battery_online_status){

		if (battery_ops->get_battery_online_status()){

			/* battery is online */
			rdb_create_string(rdb_session, RDB_BATTERY_ONLINE, "1", 0, DEFAULT_PERM);
			prev_online = 1;

			/* update charging status */
			if (battery_ops->get_battery_charging_status){
				prev_charging_status = battery_ops->get_battery_charging_status();

				switch (prev_charging_status){
					case UNKNOWN:
						rdb_create_string(rdb_session, RDB_BATTERY_STATUS, "u", 0, DEFAULT_PERM);
						break;

					case NOT_CHARGING:
						rdb_create_string(rdb_session, RDB_BATTERY_STATUS, "n", 0, DEFAULT_PERM);
						break;

					case DISCHARGING:
						rdb_create_string(rdb_session, RDB_BATTERY_STATUS, "d", 0, DEFAULT_PERM);
						break;

					case CHARGING:
						rdb_create_string(rdb_session, RDB_BATTERY_STATUS, "c", 0, DEFAULT_PERM);
						break;

					case FULL:
						rdb_create_string(rdb_session, RDB_BATTERY_STATUS, "f", 0, DEFAULT_PERM);
						break;

					default:
						break;
				}
			}

			/* update battery percents */
			if (battery_ops->get_battery_percents) {
				prev_percent = battery_ops->get_battery_percents();
				sprintf(dbb,"%d",prev_percent);
				rdb_create_string(rdb_session, RDB_BATTERY_PERCENT, dbb, 0, DEFAULT_PERM);
			}

			/* update battery voltage */
			if(battery_ops->get_battery_voltage){
				sprintf(dbb,"%d",battery_ops->get_battery_voltage());
				rdb_create_string(rdb_session, RDB_BATTERY_VOLTAGE, dbb, 0, DEFAULT_PERM);
			}
		}
		else{
			/* battery is offline */

			rdb_create_string(rdb_session, RDB_BATTERY_ONLINE, "0", 0, DEFAULT_PERM);
			prev_online = 0;

			/* update charging status */
			if (battery_ops->get_battery_charging_status){

				rdb_create_string(rdb_session, RDB_BATTERY_STATUS, "u", 0, DEFAULT_PERM);
				prev_charging_status = UNKNOWN;
			}

			/* update battery percents */
			if (battery_ops->get_battery_percents) {
				rdb_create_string(rdb_session, RDB_BATTERY_PERCENT, "0", 0, DEFAULT_PERM);
				prev_percent=0;
			}

			/* update battery voltage */
			if(battery_ops->get_battery_voltage){
				rdb_create_string(rdb_session, RDB_BATTERY_VOLTAGE, "0", 0, DEFAULT_PERM);
			}
		}
	}
}


/*
 * main function
 * - Setup specific battery implementation
 * - Setup RDB variables
 * - Open netlink socket
 * - Listen to netlink socket, parse uevent, and update RDB variables
 */
int main(int argc, char *argv[])
{
	battery_ops_t *battery_ops = NULL;
	power_nl_event_t *power_nl_event = NULL;
	struct sockaddr_nl nls;
	struct pollfd pfd;
	char buf[512];
	int rval;
	int len;
	int timeout_msecs;

	openlog("batman", LOG_PID | LOG_PERROR, LOG_DEBUG);

	/* open rdb */
    if (rdb_open(NULL, &rdb_session)) {
    	syslog(LOG_ERR, "Unable to open rdb");
        return 1;
    }

    /* setup power_nl_event */
    power_nl_event = (power_nl_event_t*) malloc(sizeof(power_nl_event_t));
    if (!power_nl_event){
    	goto error;
    }
    memset(power_nl_event, 0, sizeof(power_nl_event_t));

    /* setup battery_ops */
    battery_ops = (battery_ops_t*) malloc(sizeof(battery_ops_t));
    if (!battery_ops){
    	goto error;
    }
    memset(battery_ops, 0, sizeof(battery_ops_t));

	/* register signal handler */
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* setup specific battery implementation */
	setup_battery(battery_ops);
	/* get timeout from implementation */
	if (battery_ops->poll_timeout_msecs){
		timeout_msecs = battery_ops->poll_timeout_msecs();
	}
	else{
		timeout_msecs = DEFAULT_POLL_TIMEOUT_MS;
	}

	/* initialise power state */
	init(battery_ops);

	/* Set up for listening on the netlink socket for hotplug events */
	memset(&nls,0,sizeof(struct sockaddr_nl));
	nls.nl_family = AF_NETLINK;
	nls.nl_pid = getpid();
	nls.nl_groups = -1;
	pfd.events = POLLIN;
	pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (pfd.fd==-1) {
		syslog(LOG_ERR, "Can't open netlink socket.");
		goto error;
	}
	/* start listening */
	if (bind(pfd.fd, (void *)&nls, sizeof(struct sockaddr_nl))) {
		syslog(LOG_ERR, "Bind failed");
		goto error;
	}

	/* listen to netlink socket, parse uevent, and update RDB variables */
	while (active) {
		rval=poll(&pfd, 1, timeout_msecs);
		if (rval<0) {
			syslog(LOG_ERR, "Failed in poll()=%d, errno=%d", rval, errno);
			break;
		}
		else if (rval == 0){
			/* timeout */
			if (battery_ops->poll_timeout_process){
				if (battery_ops->poll_timeout_process()){
					/* update rdb with values provided by specific battery implementation */
					update_rdb(battery_ops);

					/* if specific battery implementation needs to update other systems, it will do here */
					if (battery_ops->update_system){
						battery_ops->update_system();
					}
				}
			}
		}
		else{
			len = recv(pfd.fd, buf, sizeof(buf), MSG_DONTWAIT);
			if (len<0) {
				syslog(LOG_ERR, "Failed in recv()=%d", len);
				break;
			}

			/* parse uevent */
			if (parse_nl_event(power_nl_event, buf, len)){
				/* call specific battery uevent processing function */
				if(battery_ops->process_power_event){
					if (battery_ops->process_power_event(power_nl_event)){

						/* update rdb with values provided by specific battery implementation */
						update_rdb(battery_ops);

						/* if specific battery implementation needs to update other systems, it will do here */
						if (battery_ops->update_system){
							battery_ops->update_system();
						}
					}
				}
			}
			/* clear power event */
			clear_nl_event(power_nl_event);
		}
	}

	free(battery_ops);
	free(power_nl_event);
	rdb_close(&rdb_session);
	closelog();

	return 0;

error:
	if (battery_ops){
		free(battery_ops);
	}

	if (power_nl_event){
		free(power_nl_event);
	}

	rdb_close(&rdb_session);
	closelog();

	return 1;
}
