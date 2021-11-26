/**
 * @file rdb_app.c
 * @brief Implements the application to use rdb variables.
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
#include "rdb_app.h"
#include "logger.h"
#include "util.h"

#include <rdb_ops.h>

#include <stdio.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define DEVICE_ATTACHED_RDB_VAR "service.nrb200.attached"
#define BATTERY_REMAINING_CAPACITY_RDB_VAR "service.nrb200.batt.charge_percentage"
#define BATTERY_CHARGING_STATUS_RDB_VAR "service.nrb200.batt.status"
#define LED_0_COLOR_RDB_VAR "service.nrb200.led.0.color"
#define LED_0_BLINK_INTERVAL_RDB_VAR "service.nrb200.led.0.blink_interval"
#define LED_1_COLOR_RDB_VAR "service.nrb200.led.1.color"
#define LED_1_BLINK_INTERVAL_RDB_VAR "service.nrb200.led.1.blink_interval"
#define LED_2_COLOR_RDB_VAR "service.nrb200.led.2.color"
#define LED_2_BLINK_INTERVAL_RDB_VAR "service.nrb200.led.2.blink_interval"
#define SW_VERSION_RDB_VAR "service.nrb200.sw_ver"
#define HW_VERSION_RDB_VAR "service.nrb200.hw_ver"
#define DEBUG_MODE_RDB_VAR "service.nrb200.debug_mode"
#define AUTHENTICATION_RDB_VAR "service.nrb200.authenticated"

#define MAX_RDB_INT_VAR_LEN 16

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/**
 * @brief The main structure of the rdb application
 */
typedef struct rdb_app {
	struct rdb_session *session;
} rdb_app_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int create_rdb_var(const char *var, const char *value);
static int set_rdb_var(const char *var, const char *value);
static int get_rdb_int(const char *var, int *value);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

static const char *led_color_rdb_vars[] = {
	LED_0_COLOR_RDB_VAR,
	LED_1_COLOR_RDB_VAR,
	LED_2_COLOR_RDB_VAR
};

static const char *led_blink_interval_rdb_vars[] = {
	LED_0_BLINK_INTERVAL_RDB_VAR,
	LED_1_BLINK_INTERVAL_RDB_VAR,
	LED_2_BLINK_INTERVAL_RDB_VAR
};

static rdb_app_t context;

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

int rdb_app_init(void)
{
	int i;

	if (rdb_open(NULL, &context.session)) {
		LOG_ERR("Opening a rdb session fails\n");
		return -1;
	}

	create_rdb_var(DEVICE_ATTACHED_RDB_VAR, "0");
	create_rdb_var(BATTERY_REMAINING_CAPACITY_RDB_VAR, "0");
	create_rdb_var(BATTERY_CHARGING_STATUS_RDB_VAR, "Error");

	for (i = 0; i < ARRAY_SIZE(led_color_rdb_vars); i++) {
		create_rdb_var(led_color_rdb_vars[i], "0");
	}

	for (i = 0; i < ARRAY_SIZE(led_blink_interval_rdb_vars); i++) {
		create_rdb_var(led_blink_interval_rdb_vars[i], "0");
	}

	create_rdb_var(SW_VERSION_RDB_VAR, "0.0.0");
	create_rdb_var(HW_VERSION_RDB_VAR, "0.0");

	return 0;
}

void rdb_app_term(void)
{
	rdb_close(&context.session);
}

void rdb_app_set_device_attached(int attached)
{
	if (attached) {
		set_rdb_var(DEVICE_ATTACHED_RDB_VAR, "1");
	} else {
		set_rdb_var(DEVICE_ATTACHED_RDB_VAR, "0");
	}
}

void rdb_app_set_battery_remaining_capacity(unsigned char percentage)
{
	char capacity_str[MAX_RDB_INT_VAR_LEN];

	snprintf(capacity_str, sizeof(capacity_str), "%u", percentage);
	set_rdb_var(BATTERY_REMAINING_CAPACITY_RDB_VAR, capacity_str);
}

void rdb_app_set_battery_charging_status(const char *status)
{
	set_rdb_var(BATTERY_CHARGING_STATUS_RDB_VAR, status);
}

void rdb_app_set_sw_version(const char *version)
{
	set_rdb_var(SW_VERSION_RDB_VAR, version);
}

void rdb_app_set_hw_version(const char *version)
{
	set_rdb_var(HW_VERSION_RDB_VAR, version);
}

void rdb_app_create_or_set_debug_mode(const char *mode)
{
	create_rdb_var(DEBUG_MODE_RDB_VAR, mode);
}

void rdb_app_create_or_set_authenticated(const char *authenticated)
{
	create_rdb_var(AUTHENTICATION_RDB_VAR, authenticated);
}

int rdb_app_get_led_status(int id, unsigned char *color, unsigned short *blink_interval)
{
	int value;

	if ((id < 0) ||
		(id >= ARRAY_SIZE(led_color_rdb_vars)) ||
		(id >= ARRAY_SIZE(led_blink_interval_rdb_vars))) {
		return -1;
	}

	if (get_rdb_int(led_color_rdb_vars[id], &value)) {
		return -1;
	}
	*color = (unsigned char)value;

	if (get_rdb_int(led_blink_interval_rdb_vars[id], &value)) {
		return -1;
	}
	*blink_interval = (unsigned short)value;

	return 0;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Creates the variable if it's not created yet.
 *
 * @param var The name of the variable to be set
 * @param value The value to be set to the variable
 * @return 1 on success, or a negative value on error
 */
static int create_rdb_var(const char *var, const char *value)
{
	/* If it's already created, skip it */
	if (rdb_set_string(context.session, var, value)) {
		if (rdb_create_string(context.session, var, value, 0, 0)) {
			LOG_ERR("Setting %s variable fails\n", var);
			return -1;
		}
	}

	return 0;
}

/**
 * @brief Sets the given value to the specified rdb variable
 *
 * @param var The name of the variable to be set
 * @param value The value to be set to the variable
 * @return 1 on success, or a negative value on error
 */
static int set_rdb_var(const char *var, const char *value)
{
	if (rdb_set_string(context.session, var, value)) {
		LOG_ERR("Setting %s variable fails\n", var);
		return -1;
	}
	return 0;
}

/**
 * @brief Gets the given value to the specified rdb variable
 *
 * @param var The name of the variable to be set
 * @param value The value to be set to the variable
 * @return 1 on success, or a negative value on error
 */
static int get_rdb_int(const char *var, int *value)
{
	if (rdb_get_int(context.session, var, value)) {
		LOG_ERR("Getting %s variable fails\n", var);
		return -1;
	}
	return 0;
}

