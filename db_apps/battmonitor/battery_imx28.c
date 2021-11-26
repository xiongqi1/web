/*
 * Implementing a specific battery implementation which cooperates
 * with i.MX28 battery charger driver
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
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "battery.h"
#include "power_nl_event.h"

/* internal states */
static long int batt_percents=-1;
static int battery_online=0;
static battery_charging_status_t batt_charging_status = UNKNOWN;
static long int battery_voltage = 0;
static long int battery_temp_alert_max=0;
static long int battery_temp_alert_min=0;

/* macro-s defining param names */
#define POWER_SUPPLY_NAME "POWER_SUPPLY_NAME"
#define POWER_SUPPLY_PRESENT "POWER_SUPPLY_PRESENT"
#define POWER_SUPPLY_STATUS "POWER_SUPPLY_STATUS"
#define POWER_SUPPLY_VOLTAGE_NOW "POWER_SUPPLY_VOLTAGE_NOW"
#define POWER_SUPPLY_CAPACITY "POWER_SUPPLY_CAPACITY"
#define POWER_SUPPLY_TEMP_ALERT_MIN "POWER_SUPPLY_TEMP_ALERT_MIN"
#define POWER_SUPPLY_TEMP_ALERT_MAX "POWER_SUPPLY_TEMP_ALERT_MAX"

#define STR_NOT_CHARGING "Not charging"
#define STR_CHARGING "Charging"
#define STR_DISCHARGING "Discharging"
#define STR_FULL "Full"
/* power supply name of battery in uevent */
#define BATT_SUPPLY_NAME "batt"

#define POLL_TIMEOUT_MS 5000

static void imx28_parse_str(const char *value_str, long int *dest)
{
	long int val;
	char *endptr;

	if (!dest){
		return;
	}

	if (value_str){
		val = strtol(value_str, &endptr, 10);
		if (*endptr == '\0'){
			*dest = val;
		}
		else{
			*dest = 0;
		}
	}
	else{
		*dest = 0;
	}
}

/* process received power event */
static int imx28_process_power_event(power_nl_event_t* power_nl_event)
{
	if (!strcmp(power_event_find_param(power_nl_event, POWER_SUPPLY_NAME), BATT_SUPPLY_NAME)){

		const char *s_batt_present = power_event_find_param(power_nl_event, POWER_SUPPLY_PRESENT);
		const char *s_batt_status = power_event_find_param(power_nl_event, POWER_SUPPLY_STATUS);
		const char *s_batt_volt = power_event_find_param(power_nl_event, POWER_SUPPLY_VOLTAGE_NOW);
		const char *s_batt_capacity = power_event_find_param(power_nl_event, POWER_SUPPLY_CAPACITY);
		const char *s_batt_temp_alert_max = power_event_find_param(power_nl_event, POWER_SUPPLY_TEMP_ALERT_MAX);
		const char *s_batt_temp_alert_min = power_event_find_param(power_nl_event, POWER_SUPPLY_TEMP_ALERT_MIN);

		/* battery presence */
		if (!strcmp(s_batt_present, "1")){
			battery_online = 1;
		}
		else{
			battery_online = 0;
		}

		/* status */
		if (s_batt_status){
			if (!strcmp(s_batt_status, STR_NOT_CHARGING)){
				batt_charging_status = NOT_CHARGING;
			}
			else if (!strcmp(s_batt_status, STR_CHARGING)){
				batt_charging_status = CHARGING;
			}
			else if (!strcmp(s_batt_status, STR_DISCHARGING)){
				batt_charging_status = DISCHARGING;
			}
			else if (!strcmp(s_batt_status, STR_FULL)){
				batt_charging_status = FULL;
			}
			else {
				batt_charging_status = UNKNOWN;
			}
		}
		else{
			batt_charging_status = UNKNOWN;
		}

		/* voltage */
		imx28_parse_str(s_batt_volt, &battery_voltage);

		/* percents */
		imx28_parse_str(s_batt_capacity, &batt_percents);

		/* max temp alert */
		imx28_parse_str(s_batt_temp_alert_max, &battery_temp_alert_max);

		/* min temp alert */
		imx28_parse_str(s_batt_temp_alert_min, &battery_temp_alert_min);

		return 1;
	}
	else{
		return 0;
	}
}

/* get battery online status
 * Return:
 * 		1: online
 * 		0: not online
 */
static int imx28_get_battery_online_status(void)
{
	return battery_online;
}

/* get battery status */
static battery_charging_status_t imx28_get_battery_charging_status(void)
{
	return batt_charging_status;
}

/* get battery percents */
static int imx28_get_battery_percents(void)
{
	return batt_percents;
}

/* get battery voltage in mV */
static int imx28_get_battery_voltage(void)
{
	return battery_voltage;
}

/* parse a uevent line of which format is "property=value"
 * Parameters:
 * 	line:		string uevent line
 * 	property:	property in line
 *
 * Return: value ("0" means value 0 or error, no need to distinguish in this case)
 */
static long int imx28_parse_uevent_line(char *line, const char *property)
{
	char *value;
	long int val;
	char *endptr;

	if (!line){
		return 0;
	}

	value = line + strlen(property) + 1;

	if (value){
		val = strtol(value, &endptr, 10);
		if (*endptr == '\0'){
			return val;
		}
		else{
			return 0;
		}
	}
	else{
		return 0;
	}
}

/* read uevent sys file */
static int imx28_read_uevent_file(void)
{
#define BATT_UEVENT_PATH "/sys/class/power_supply/batt/uevent"
#define MAX_LEN 40
	char *line = NULL;
	size_t nbytes = MAX_LEN;
	int bytes_read;
	FILE *fr;
	char *value;

	line = (char*)malloc(nbytes+1);
	if (!line){
		return 0;
	}

	/* open the file for reading */
	fr = fopen (BATT_UEVENT_PATH, "r");
	if (fr){
		/* read line by line */
		while( (bytes_read = getline(&line, &nbytes, fr)) != -1)
		{
			/* remove newline character */
			if (bytes_read>1 && line[bytes_read-1]=='\n'){
				line[bytes_read-1]='\0';
				/* no need to decrease bytes_read as it's not used in code below */
			}
			/* parse each line */
			if (!strncmp(line, POWER_SUPPLY_PRESENT, strlen(POWER_SUPPLY_PRESENT))){
				value = line + strlen(POWER_SUPPLY_PRESENT) + 1;

				if (!strcmp(value, "1")){
					battery_online = 1;
				}
				else{
					battery_online = 0;
				}
			}
			else if (!strncmp(line, POWER_SUPPLY_STATUS, strlen(POWER_SUPPLY_STATUS))){
				value = line + strlen(POWER_SUPPLY_STATUS) + 1;

				/* status */
				if (!strcmp(value, STR_NOT_CHARGING)){
					batt_charging_status = NOT_CHARGING;
				}
				else if (!strcmp(value, STR_CHARGING)){
					batt_charging_status = CHARGING;
				}
				else if (!strcmp(value, STR_DISCHARGING)){
					batt_charging_status = DISCHARGING;
				}
				else if (!strcmp(value, STR_FULL)){
					batt_charging_status = FULL;
				}
				else {
					batt_charging_status = UNKNOWN;
				}
			}
			else if (!strncmp(line, POWER_SUPPLY_VOLTAGE_NOW, strlen(POWER_SUPPLY_VOLTAGE_NOW))){
				/* voltage */
				battery_voltage = imx28_parse_uevent_line(line, POWER_SUPPLY_VOLTAGE_NOW);
			}
			else if (!strncmp(line, POWER_SUPPLY_CAPACITY, strlen(POWER_SUPPLY_CAPACITY))){
				/* percents */
				batt_percents = imx28_parse_uevent_line(line, POWER_SUPPLY_CAPACITY);
			}
			else if (!strncmp(line, POWER_SUPPLY_TEMP_ALERT_MAX, strlen(POWER_SUPPLY_TEMP_ALERT_MAX))){
				/* max temp alert */
				battery_temp_alert_max = imx28_parse_uevent_line(line, POWER_SUPPLY_TEMP_ALERT_MAX);
			}
			else if (!strncmp(line, POWER_SUPPLY_TEMP_ALERT_MIN, strlen(POWER_SUPPLY_TEMP_ALERT_MIN))){
				/* min temp alert */
				battery_temp_alert_min = imx28_parse_uevent_line(line, POWER_SUPPLY_TEMP_ALERT_MIN);
			}
		}

		if (line){
			free(line);
		}
		fclose(fr);
		return 1;
	}
	else{
		if (line){
			free(line);
		}
		return 0;
	}
}

/* specify poll timeout in ms */
static int imx28_poll_timeout(void)
{
	return POLL_TIMEOUT_MS;
}

/* temperature alert */
static int imx28_over_max_temperature_alert(void)
{
	return (battery_temp_alert_max!=0);
}
static int imx28_under_min_temperature_alert(void)
{
	return (battery_temp_alert_min!=0);
}

/* setup initial states */
static void imx28_init(void)
{
	imx28_read_uevent_file();
}

/* setup and hook battery functions implementation */
void setup_battery(battery_ops_t *batt_ops)
{
	imx28_init();

	batt_ops->poll_timeout_msecs = imx28_poll_timeout;
	batt_ops->process_power_event = imx28_process_power_event;
	batt_ops->poll_timeout_process = imx28_read_uevent_file;
	batt_ops->get_battery_online_status = imx28_get_battery_online_status;
	batt_ops->get_battery_charging_status = imx28_get_battery_charging_status;
	batt_ops->get_battery_percents = imx28_get_battery_percents;
	batt_ops->get_battery_voltage = imx28_get_battery_voltage;
	batt_ops->over_max_temperature_alert = imx28_over_max_temperature_alert;
	batt_ops->under_min_temperature_alert = imx28_under_min_temperature_alert;
}
