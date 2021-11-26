/**
 * @file mgmt_service_app.c
 * @brief Implements management service that provides mechanism to initiate communication and
 *        establish connections, and exchange management commands
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless limited.
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
#include "mgmt_service_app.h"
#include "mgmt_service_proto.h"
#ifndef PC_SIMULATOR
#include "rdb_app.h"
#endif
#ifdef HAVE_AUTHENTICATION
#include "authentication.h"
#endif
#include "app_config.h"
#include "app_api.h"
#include "logger.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define MGMT_SERVICE_SIGNATURE 0x4D475341 /* MGSA */

#define LED_TIMEOUT_COUNT  (3000 / SCOMM_MANAGER_ACTIVE_LOOP_TIMEOUT_MS) /* every 3 secs */
#define KEEP_ALIVE_TIMEOUT_COUNT  (15000 / SCOMM_MANAGER_ACTIVE_LOOP_TIMEOUT_MS) /* every 15 secs */
#define BAUD_RATE_TIMEOUT_COUNT  (5000 / SCOMM_MANAGER_ACTIVE_LOOP_TIMEOUT_MS) /* every 5 secs */
#define BAUD_RATE_SWITCHING_TIME_US 1000000 /* 1 sec */
/* Wait until the random generator can have enough entropy to generate pure random numbers */
#define ENOUGH_ENTROPY_WAIT_COUNT (30000 / SCOMM_MANAGER_ACTIVE_LOOP_TIMEOUT_MS) /* 30 seconds */

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/**
 * @brief The enumeration to represent all states in the management service.
 */
typedef enum {
	MGMT_SERVICE_STATE_INIT = 0,
	MGMT_SERVICE_STATE_CHANGING_BAUD_RATE,
	MGMT_SERVICE_STATE_VALIDATING_BAUD_RATE,
	MGMT_SERVICE_STATE_ATTACHED,
	MGMT_SERVICE_STATE_DETACHED,
} management_service_state_t;

#ifdef HAVE_AUTHENTICATION
/**
 * @brief The enumeration to represent authentication states
 */
typedef enum {
	AUTHENTICATION_NULL = 0,
	AUTHENTICATION_REQUIRED,
	AUTHENTICATION_REQUESTED,
	AUTHENTICATION_FAILED,
	AUTHENTICATION_SUCCESS,
} authentication_state_t;
#endif

typedef struct led_device {
	unsigned char color;
	unsigned short blink_interval;
	unsigned int timeout_count;
} led_device_t;

#ifdef HAVE_AUTHENTICATION
/**
 * @brief The enumeration to represent all types in the function code.
 */
typedef enum {
	MGMT_SERVICE_FUNCTION_CODE_UNKNOWN = 0,
	MGMT_SERVICE_FUNCTION_CODE_DEBUG_MODE_1,
} function_code_type_t;

/**
 * @brief The structure representing all necessary information for
 *        a unique function code.
 */
typedef struct function_code {
	function_code_type_t type;
	int len;
	unsigned char code[MAX_MANAGEMENT_FUNCTION_CODE_LEN];
} function_code_t;
#endif

/**
 * @brief The main structure for the management service application
 */
typedef struct mgmt_service_app {
	scomm_manager_t *manager;
	scomm_service_t *scomm_service;
	scomm_service_client_t scomm_client;

	mgmt_service_proto_t proto;
	mgmt_service_pdu_t pdu;

	char *device_name;

	management_service_state_t main_state;

	unsigned int baud_rate;
	unsigned int optimal_baud_rate;
	int baud_rate_count;

	int keep_alive_count;
	int keep_alive_timeout_count;

	int led_forced_update;
	led_device_t leds[MAX_LED_NUM];

#ifdef HAVE_AUTHENTICATION
	authentication_state_t auth_state;
	unsigned char auth_challenge[MAX_MANAGEMENT_AUTH_CHALLENGE_LEN];
	unsigned int auth_challenge_len;
#endif

	unsigned int timeout_count;

	unsigned int signature;
} mgmt_service_app_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int init_app(mgmt_service_app_t *app, scomm_manager_t *manager, scomm_service_t *service);
static void change_state(mgmt_service_app_t *app, management_service_state_t state);
static void device_attached(mgmt_service_app_t *app, int attached);
static int data_received(scomm_packet_t *packet, void *owner);
static int handle_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu);
static void send_hello_response(mgmt_service_app_t *app);
static void send_keep_alive_response(mgmt_service_app_t *app);
static void send_echo_response(mgmt_service_app_t *app, unsigned char *data, int len);
static void check_baud_rate(mgmt_service_app_t *app);
static void check_keep_alive(mgmt_service_app_t *app);
#ifdef HAVE_AUTHENTICATION
static void check_authentication(mgmt_service_app_t *app);
#endif
static void init_led(mgmt_service_app_t *app);
#ifndef PC_SIMULATOR
static void send_led_request(mgmt_service_app_t *app, unsigned char led_id, unsigned char color,
		unsigned short blink_interval);
static const char *conv_battery_status_to_string(unsigned char status);
static void update_led(mgmt_service_app_t *app);
#endif
static void change_baud_rate(mgmt_service_app_t *app);
static void validate_baud_rate(mgmt_service_app_t *app);
static void acknowledge_baud_rate(mgmt_service_app_t *app);
static void set_baud_rate(mgmt_service_app_t *app, unsigned int baud_rate);
static void rollback_baud_rate(mgmt_service_app_t *app);
static void send_change_baud_rate_request(mgmt_service_app_t *app);
static void send_new_baud_rate_request(mgmt_service_app_t *app);
static void send_new_baud_rate_indication(mgmt_service_app_t *app);
#ifdef HAVE_AUTHENTICATION
static function_code_type_t get_function_code_type(const unsigned char *code, int len);
static int handle_function_code(mgmt_service_app_t *app, const unsigned char *data, int len);
static void set_authentication_result(mgmt_service_app_t *app, int authenticated);
static int handle_response_authentication(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu);
#endif
/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

#ifdef HAVE_AUTHENTICATION
/* Declares function codes that are shared with the installation assistant tool */
static const function_code_t function_code_book[] = {
	{
		MGMT_SERVICE_FUNCTION_CODE_DEBUG_MODE_1,
		8,
		{ 0x28, 0x07, 0x2c, 0x47, 0xe4, 0x7b, 0xce, 0x26 }
	},
};
#endif

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

mgmt_service_app_t *mgmt_service_app_create(scomm_manager_t *manager, scomm_service_t *service)
{
	mgmt_service_app_t *app;

	app = malloc(sizeof(mgmt_service_app_t));
	if (app) {
		memset(app, 0, sizeof(mgmt_service_app_t));
		init_app(app, manager, service);
	}

	return app;
}

void mgmt_service_app_destroy(mgmt_service_app_t *app)
{
	if (app) {
		free(app);
	}
}

int mgmt_service_app_start(mgmt_service_app_t *app)
{
	/* Dummy interface */
	return 0;
}

int mgmt_service_app_stop(mgmt_service_app_t *app)
{
	/* Dummy interface */
	return 0;
}

void mgmt_service_app_timeout(mgmt_service_app_t *app)
{
	/* Increments whenever a timeout event occurs */
	app->timeout_count++;

	check_baud_rate(app);
	check_keep_alive(app);

#ifdef HAVE_AUTHENTICATION
	check_authentication(app);
#endif

#ifndef PC_SIMULATOR
	update_led(app);
#endif
}

void mgmt_service_app_set_device_name(mgmt_service_app_t *app, const char *name)
{
	if (app->device_name) {
		free(app->device_name);
	}
	app->device_name = strdup(name);
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Initialises internal variables on the management service application
 *
 * @param app The context of the management service application
 * @param manager The context of the scomm manager
 * @param service The context of the scomm service
 * @return 0 on success, or a negative value on error
 */
static int init_app(mgmt_service_app_t *app, scomm_manager_t *manager, scomm_service_t *service)
{
	app->manager = manager;
	app->scomm_service = service;
	app->scomm_client.service_id = 0;
	app->scomm_client.data_received_cb = data_received;
	app->scomm_client.owner = app;
	scomm_service_add_client(app->scomm_service, &app->scomm_client);

	init_led(app);

	app->device_name = strdup(DEFAULT_DEVICE_NAME);
	mgmt_service_proto_init(&app->proto);

	app->main_state = MGMT_SERVICE_STATE_INIT;

	app->baud_rate = DEFAULT_SERIAL_BAUD_RATE;
	app->optimal_baud_rate = 0;
	app->baud_rate_count = 0;

	app->keep_alive_count = 0;
	app->keep_alive_timeout_count = 0;

#ifdef HAVE_AUTHENTICATION
	app->auth_state = AUTHENTICATION_NULL;
#endif

	app->timeout_count = 0;

	app->signature = MGMT_SERVICE_SIGNATURE;

	LOG_INFO_AT();
	return 0;
}

/**
 * @brief Changes the state to the specified state.
 *
 * @param app The context of the management service application
 * @param state The new state to be set
 * @return Void
 */
static void change_state(mgmt_service_app_t *app, management_service_state_t state)
{
	if (app->main_state != state) {
		LOG_NOTICE("[MGMT] change state from %d to %d\n", app->main_state, state);
		app->main_state = state;
		/* Do something before entering the new state */
		switch (state) {
		case MGMT_SERVICE_STATE_CHANGING_BAUD_RATE:
		case MGMT_SERVICE_STATE_VALIDATING_BAUD_RATE:
		case MGMT_SERVICE_STATE_ATTACHED:
			/* Fast polling is needed */
			scomm_manager_change_polling_interval(1);
			break;

		case MGMT_SERVICE_STATE_DETACHED:
			/* Slow polling is enough */
			scomm_manager_change_polling_interval(0);
			break;

		default:
			break;
		}
	}
}

/**
 * @brief Informs whether Installation Assistant is attached or not
 *
 * @param app The context of the management service application
 * @param attached Indicates whether Installation Assistant is attached or not
 * @return Void
 */
static void device_attached(mgmt_service_app_t *app, int attached)
{
	management_service_state_t state;

	if (attached) {
		state = MGMT_SERVICE_STATE_ATTACHED;
	} else {
		state = MGMT_SERVICE_STATE_DETACHED;
	}

	if (state == app->main_state) {
		return;
	}

	change_state(app, state);
#ifndef PC_SIMULATOR
	rdb_app_set_device_attached(attached);
#endif
	scomm_manager_device_attached(attached);
	LOG_INFO("[MGMT] device_attached: %d\n", attached);
}

/**
 * @brief Called when the data packet arrives
 *
 * @param packets The received packet on the scomm service
 * @param owner The context of the management service application
 * @return 0 on success, or a negative value on error
 */
static int data_received(scomm_packet_t *packet, void *owner)
{
	int result;
	mgmt_service_app_t *app = owner;

	assert((app && app->signature == MGMT_SERVICE_SIGNATURE));

	result = -1;
	if (mgmt_service_proto_decode_packet(&app->proto, packet, &app->pdu)) {
		LOG_INFO_AT();
		goto out;
	}

	if (handle_pdu(app, &app->pdu)) {
		LOG_INFO_AT();
		goto out;
	}

	result = 0;
out:
	scomm_packet_destroy(packet);
	return result;
}

#ifndef PC_SIMULATOR
/**
 * @brief Sends the request to control led devices
 *
 * @param app The context of the management service application
 * @return Void
 */
static void send_led_request(mgmt_service_app_t *app, unsigned char led_id, unsigned char color,
		unsigned short blink_interval)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		return;
	}

	LOG_DEBUG("LED(%d) Color:%d, Int:%d\n", led_id, color, blink_interval);
	mgmt_service_proto_encode_led_indication(&app->proto, packet, led_id, color, blink_interval);
	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	scomm_packet_destroy(packet);
}
#endif

/**
 * @brief Sends a CHANGE BAUD RATE request message to the peer device
 *
 * @param app The context of the management service application
 * @return Void
 */
static void send_change_baud_rate_request(mgmt_service_app_t *app)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		return;
	}

	mgmt_service_proto_encode_change_baud_rate_request(&app->proto, packet, app->optimal_baud_rate);

	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	scomm_packet_destroy(packet);
	LOG_INFO_AT();
}

/**
 * @brief Sends a NEW BAUD RATE request message to the peer device
 *
 * @param app The context of the management service application
 * @return Void
 */
static void send_new_baud_rate_request(mgmt_service_app_t *app)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		return;
	}

	mgmt_service_proto_encode_new_baud_rate_request(&app->proto, packet);

	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	scomm_packet_destroy(packet);
	LOG_INFO_AT();
}

/**
 * @brief Sends a NEW BAUD RATE indication message to the peer device
 *
 * @param app The context of the management service application
 * @return Void
 */
static void send_new_baud_rate_indication(mgmt_service_app_t *app)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		return;
	}

	mgmt_service_proto_encode_new_baud_rate_indication(&app->proto, packet);

	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	scomm_packet_destroy(packet);
	LOG_INFO_AT();
}

/**
 * @brief Sends the response in reply to the hello request
 *
 * @param app The context of the management service application
 * @return Void
 */
static void send_hello_response(mgmt_service_app_t *app)
{
	int service_mask;
	scomm_packet_t *packet;
	int num_ports;
	unsigned short source_ports[MAX_MANAGEMENT_TCP_PORT_NUM];

	packet = scomm_packet_create();
	if (!packet) {
		return;
	}

	mgmt_service_proto_encode_hello_response(&app->proto, packet, app->device_name);

	/* Add optional parameters */
	service_mask = scomm_manager_get_available_service_mask();
	if (service_mask) {
		mgmt_service_proto_add_service_id_in_hello_response(&app->proto, packet, service_mask);
	}

	if (app->optimal_baud_rate) {
		mgmt_service_proto_add_baud_rate_in_hello_response(&app->proto, packet, 1,
				&app->optimal_baud_rate);
	}

	num_ports = get_source_ports(source_ports, MAX_MANAGEMENT_TCP_PORT_NUM);
	if (num_ports > 0) {
		mgmt_service_proto_add_tcp_port_in_hello_response(&app->proto, packet, num_ports,
				source_ports);
	}

	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	scomm_packet_destroy(packet);
	LOG_INFO_AT();
}

/**
 * @brief Sends the response in reply to the keep alive request
 *
 * @param app The context of the management service application
 * @return Void
 */
static void send_keep_alive_response(mgmt_service_app_t *app)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		return;
	}

	mgmt_service_proto_encode_keep_alive_response(&app->proto, packet);
	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	scomm_packet_destroy(packet);
	LOG_INFO_AT();
}

/**
 * @brief Sends the response in reply to the echo request
 *
 * @param app The context of the management service application
 * @return Void
 */
static void send_echo_response(mgmt_service_app_t *app, unsigned char *data, int len)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		return;
	}

	mgmt_service_proto_encode_echo_response(&app->proto, packet, data, len);
	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	scomm_packet_destroy(packet);
	LOG_INFO_AT();
}

#ifdef HAVE_AUTHENTICATION
/**
 * @brief Sends a request to authenticate the attached device
 *
 * @param app The context of the management service application
 * @return Void
 */
static int send_authentication_request(mgmt_service_app_t *app)
{
	int result = 0;
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		return -1;
	}

	if (authentication_generate_challenge(app->auth_challenge, MAX_MANAGEMENT_AUTH_CHALLENGE_LEN)) {
		result = -1;
		goto out;
	}

	app->auth_challenge_len = MAX_MANAGEMENT_AUTH_CHALLENGE_LEN;
	mgmt_service_proto_encode_auth_request(&app->proto, packet, app->auth_challenge,
		app->auth_challenge_len, MAX_MANAGEMENT_AUTH_ALG_DEFAULT);
	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);

out:
	scomm_packet_destroy(packet);
	return result;
}
#endif

/**
 * @brief Handles the hello request
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the hello request
 * @return 0 on success, or a negative value on error
 */
static int handle_request_hello_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	if (pdu->u.hello.valid_mask & (1 << MANAGEMENT_HELLO_TLV_SW_VERSION)) {
#ifndef PC_SIMULATOR
		rdb_app_set_sw_version(pdu->u.hello.sw_version);
#endif
		LOG_INFO("IA SW version: %s\n", pdu->u.hello.sw_version);
	}

	if (pdu->u.hello.valid_mask & (1 << MANAGEMENT_HELLO_TLV_HW_VERSION)) {
#ifndef PC_SIMULATOR
		rdb_app_set_hw_version(pdu->u.hello.hw_version);
#endif
		LOG_INFO("IA HW version: %s\n", pdu->u.hello.hw_version);
	}

	/* Check whether the peer device supports the optimal baud rate */
	app->optimal_baud_rate = 0;
	if (pdu->u.hello.valid_mask & (1 << MANAGEMENT_HELLO_TLV_BAUD_RATE)) {
		int idx;
		unsigned int baud_rate = get_optimal_baud_rate();

		if (baud_rate) {
			for (idx = 0; idx < pdu->u.hello.num_baud_rates; idx++) {
				if (baud_rate == pdu->u.hello.baud_rates[idx]) {
					app->optimal_baud_rate = baud_rate;
				}
			}
		}
	}

#ifdef HAVE_AUTHENTICATION
	if (pdu->u.hello.valid_mask & (1 << MANAGEMENT_HELLO_TLV_FUNCTION_CODE)) {
		handle_function_code(app, pdu->u.hello.function_code, pdu->u.hello.function_code_len);
		if ((app->auth_state == AUTHENTICATION_NULL) && authentication_has_key()) {
			LOG_NOTICE("Auth required\n");
			app->auth_state = AUTHENTICATION_REQUIRED;
		}
		LOG_INFO_AT();
	}
#endif

	/* Now, everything is ready to send the HELLO response */
	send_hello_response(app);

	/* Change state depending on whether the baud rate is negotiatable with the peer device */
	if (app->optimal_baud_rate) {
		LOG_NOTICE("Use new baud rate: %d\n", app->optimal_baud_rate);
		change_state(app, MGMT_SERVICE_STATE_CHANGING_BAUD_RATE);
		change_baud_rate(app);
	} else {
		LOG_INFO("Keep default baud rate\n");
		device_attached(app, 1);
	}

	return 0;
}

/**
 * @brief Handles the keep alive request
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the keep live request
 * @return 0 on success, or a negative value on error
 */
static int handle_request_keep_alive_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	app->keep_alive_count++;
	send_keep_alive_response(app);

	/* It's not necessary to inform that Installation Assistant is attached here.
	 * but That makes it easy to handle unusual cases */
	device_attached(app, 1);
	return 0;
}

/**
 * @brief Handles the echo request
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the echo request
 * @return 0 on success, or a negative value on error
 */
static int handle_request_echo_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	send_echo_response(app, pdu->u.echo.payload, pdu->u.echo.len);
	return 0;
}

/**
 * @brief Handles the response pdu for the management service
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the response for the management service
 * @return 0 on success, or a negative value on error
 */
static int handle_request_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	int result = -1;

	switch (pdu->id) {
	case MANAGEMENT_MESSAGE_ID_HELLO:
		result = handle_request_hello_pdu(app, pdu);
		break;

	case MANAGEMENT_MESSAGE_ID_KEEP_ALIVE:
		result = handle_request_keep_alive_pdu(app, pdu);
		break;

	case MANAGEMENT_MESSAGE_ID_ECHO:
		result = handle_request_echo_pdu(app, pdu);
		break;

	default:
		break;
	}

	return result;
}

/**
 * @brief handles a CHANGE BAUD RATE reponse message from the peer device
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the change baud response
 * @return 0 on success, or a negative value on error
 */
static int handle_response_change_baud_rate(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	if (app->main_state == MGMT_SERVICE_STATE_CHANGING_BAUD_RATE) {
		if (pdu->u.speed.valid_mask & (1 << MANAGEMENT_TLV_RESPONSE_CODE_ID)) {
			if (pdu->u.speed.result == MANAGEMENT_TLV_RESPONSE_CODE_OK) {
				app->baud_rate_count = 0;
				set_baud_rate(app, app->optimal_baud_rate);
				validate_baud_rate(app);
				change_state(app, MGMT_SERVICE_STATE_VALIDATING_BAUD_RATE);
				return 0;
			} else {
				/* The peer doesn't want to change the baud rate */
				device_attached(app, 1);
				return 0;
			}
		}
	}

	LOG_WARN_AT();
	return -1;
}

/**
 * @brief handles a NEW BAUD RATE response message from the peer device
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the change baud response
 * @return 0 on success, or a negative value on error
 */
static int handle_response_new_baud_rate(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	if (app->main_state == MGMT_SERVICE_STATE_VALIDATING_BAUD_RATE) {
		app->baud_rate_count = 0;
		acknowledge_baud_rate(app);
		device_attached(app, 1);
	}

	return 0;
}

#ifdef HAVE_AUTHENTICATION
/**
 * @brief Set the authentication result
 *
 * @param app The context of the management service application
 * @param authenticated Boolean to indicate whether the authentication
 *        completes successfully.
 */
static void set_authentication_result(mgmt_service_app_t *app, int authenticated)
{
	if (authenticated) {
		app->auth_state = AUTHENTICATION_SUCCESS;
#ifndef PC_SIMULATOR
		rdb_app_create_or_set_authenticated("1");
#endif
		LOG_NOTICE("Authentication successful\n");
	} else {
		app->auth_state = AUTHENTICATION_FAILED;
#ifndef PC_SIMULATOR
		rdb_app_create_or_set_authenticated("0");
#endif
		LOG_WARN("Authentication failed\n");
	}
}

/**
 * @brief handles a Authentication response message from the peer device
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the received authentication request
 * @return 0 on success, or a negative value on error
 */
static int handle_response_authentication(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	if ((app->main_state == MGMT_SERVICE_STATE_ATTACHED) &&
		(app->auth_state == AUTHENTICATION_REQUESTED)) {
		if (pdu->u.auth.result != MANAGEMENT_TLV_RESPONSE_CODE_OK) {
			set_authentication_result(app, 0);
			return 0;
		}

		if (!(pdu->u.auth.valid_mask & (1 << MANAGEMENT_AUTH_TLV_ANSWER_LEN)) ||
			!(pdu->u.auth.valid_mask & (1 << MANAGEMENT_AUTH_TLV_ANSWER))) {
			set_authentication_result(app, 0);
			return 0;
		}

		if (!authentication_verify(pdu->u.auth.answer, pdu->u.auth.received_ans_len,
			app->auth_challenge, app->auth_challenge_len)) {
			set_authentication_result(app, 1);
		} else {
			set_authentication_result(app, 0);
		}
	}

	return 0;
}
#endif

/**
 * @brief Handles the response pdu for the management service
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the received request
 * @return 0 on success, or a negative value on error
 */
static int handle_response_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	int result = -1;

	switch (pdu->id) {
	case MANAGEMENT_MESSAGE_ID_CHANGE_BAUD_RATE:
		result = handle_response_change_baud_rate(app, pdu);
		break;

	case MANAGEMENT_MESSAGE_ID_NEW_BAUD_RATE:
		result = handle_response_new_baud_rate(app, pdu);
		break;

#ifdef HAVE_AUTHENTICATION
	case MANAGEMENT_MESSAGE_ID_AUTH:
		result = handle_response_authentication(app, pdu);
		break;
#endif

	default:
		break;
	}

	return result;
}

/**
 * @brief Handles the response pdu for the management service
 *
 * @param app The context of the management service application
 * @param pdu The pdu of the indication for the management service
 * @return 0 on success, or a negative value on error
 */
static int handle_indication_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	int result = -1;

	switch (pdu->id) {
	case MANAGEMENT_MESSAGE_ID_BATTERY_STATUS:
		result = 0;
#ifndef PC_SIMULATOR
		rdb_app_set_battery_remaining_capacity(pdu->u.battery.capacity);
		rdb_app_set_battery_charging_status(conv_battery_status_to_string(pdu->u.battery.status));
#endif
		LOG_INFO("Battery Capacity: %d, Status: %d\n", pdu->u.battery.capacity,
				pdu->u.battery.status);
		break;

	default:
		break;
	}

	return result;
}

/**
 * @brief Handles the received pdu for the management service
 *
 * @param app The context of the management service application
 * @param pdu The received pdu for the management service
 * @return 0 on success, or a negative value on error
 */
static int handle_pdu(mgmt_service_app_t *app, mgmt_service_pdu_t *pdu)
{
	int result = -1;

	switch (pdu->type) {
	case MANAGEMENT_MESSAGE_TYPE_REQ:
		result = handle_request_pdu(app, pdu);
		break;

	case MANAGEMENT_MESSAGE_TYPE_RESP:
		result = handle_response_pdu(app, pdu);
		break;

	case MANAGEMENT_MESSAGE_TYPE_IND:
		result = handle_indication_pdu(app, pdu);
		break;

	default:
		break;
	}

	return result;
}

/**
 * @brief Changes the baud rate to the optimal baud rate
 *
 * @param app The context of the management service application
 * @return Void
 */
static void change_baud_rate(mgmt_service_app_t *app)
{
	send_change_baud_rate_request(app);
}

/**
 * @brief Validates the change of the baud rate is successful.
 *
 * @param app The context of the management service application
 * @return Void
 */
static void validate_baud_rate(mgmt_service_app_t *app)
{
	send_new_baud_rate_request(app);
}

/**
 * @brief Acknowledges the change of the baud rate is successful.
 *
 * @param app The context of the management service application
 * @return Void
 */
static void acknowledge_baud_rate(mgmt_service_app_t *app)
{
	send_new_baud_rate_indication(app);
}

/**
 * @brief Sets the specified baud rate.
 *
 * @param app The context of the management service application
 * @param baud_rate The baud rate to be set.
 * @return Void
 */
static void set_baud_rate(mgmt_service_app_t *app, unsigned int baud_rate)
{
	/* Give enough switching time to complete the previous serial operation */
	usleep(BAUD_RATE_SWITCHING_TIME_US);

	scomm_manager_change_baud_rate(baud_rate);

	app->baud_rate = baud_rate;
}

/**
 * @brief Roll the default baud rate back if it's not.
 *
 * @param app The context of the management service application
 * @return Void
 */
static void rollback_baud_rate(mgmt_service_app_t *app)
{
	if (app->baud_rate != DEFAULT_SERIAL_BAUD_RATE) {
		set_baud_rate(app, DEFAULT_SERIAL_BAUD_RATE);
	}
}

/**
 * @brief Checks the procedure to the change of a baud rate is still in progress.
 *
 * @param app The context of the management service application
 * @return Void
 */
static void check_baud_rate(mgmt_service_app_t *app)
{
	if ((app->main_state == MGMT_SERVICE_STATE_CHANGING_BAUD_RATE) ||
		(app->main_state == MGMT_SERVICE_STATE_VALIDATING_BAUD_RATE)) {
		app->baud_rate_count++;
		LOG_INFO("check_baud_rate (%d, %d)\n", app->baud_rate_count, BAUD_RATE_TIMEOUT_COUNT);
		if (app->baud_rate_count > BAUD_RATE_TIMEOUT_COUNT) {
			device_attached(app, 1);
			rollback_baud_rate(app);
		}
	}
}

#ifdef HAVE_AUTHENTICATION
/**
 * @brief Gets the type of the function code
 *
 * @param code The binary value of the function code
 * @param len The length of the function code
 * @return A type of function code if found in the code book, or
 *         MGMT_SERVICE_FUNCTION_CODE_UNKNOWN otherwise
 */
static function_code_type_t get_function_code_type(const unsigned char *code, int len)
{
	int i;
	const function_code_t *sc;

	for (i = 0; i < ARRAY_SIZE(function_code_book); i++) {
		sc = &function_code_book[i];
		if ((sc->len == len) && !memcmp(sc->code, code, len)) {
			return sc->type;
		}
	}

	return MGMT_SERVICE_FUNCTION_CODE_UNKNOWN;
}

/**
 * @brief Handles the function code message.
 *
 * @param app The context of the management service application
 * @param code The binary value of the function code
 * @param len The length of the function code
 * @return 0 on success, or a negative value on error
 */
static int handle_function_code(mgmt_service_app_t *app, const unsigned char *code, int len)
{
	int result = 0;
	function_code_type_t code_type = get_function_code_type(code, len);

	switch (code_type) {
	case MGMT_SERVICE_FUNCTION_CODE_DEBUG_MODE_1:
#ifndef PC_SIMULATOR
		rdb_app_create_or_set_debug_mode("1");
#endif
		break;

	default:
		LOG_WARN("[MGMT] An unknown function code has been received\n");
		result = -1;
		break;
	}

	return result;
}
#endif

/**
 * @brief Checks whether it is still attached to the device
 *
 * @param app The context of the management service application
 * @return Void
 */
static void check_keep_alive(mgmt_service_app_t *app)
{
	if (app->main_state == MGMT_SERVICE_STATE_ATTACHED) {
		if (app->keep_alive_count > 0) {
			app->keep_alive_count = 0;
			app->keep_alive_timeout_count = 0;
		} else {
			app->keep_alive_timeout_count++;
			if (app->keep_alive_timeout_count > KEEP_ALIVE_TIMEOUT_COUNT) {
				device_attached(app, 0);
				rollback_baud_rate(app);
				app->keep_alive_timeout_count = 0;
			}
		}
	}
}

#ifdef HAVE_AUTHENTICATION
/**
 * @brief Checks whether authentication is required. If required, send a
 *        Authentication Request to the attached device
 *
 * @param app The context of the management service application
 * @return Void
 */
static void check_authentication(mgmt_service_app_t *app)
{
	if ((app->main_state == MGMT_SERVICE_STATE_ATTACHED) &&
		(app->auth_state == AUTHENTICATION_REQUIRED) &&
		(app->timeout_count > ENOUGH_ENTROPY_WAIT_COUNT)) {
		if (send_authentication_request(app)) {
			LOG_WARN("Authentication request failed\n");
			app->auth_state = AUTHENTICATION_FAILED;
		} else {
			LOG_NOTICE("Authentication requested\n");
			app->auth_state = AUTHENTICATION_REQUESTED;
		}
	}
}
#endif

/**
 * @brief Intialises LEDs
 *
 * @param app The context of the management service application
 * @return Void
 */
static void init_led(mgmt_service_app_t *app)
{
	int i;
	led_device_t *led;

	app->led_forced_update = 0;
	for (i = 0; i < MAX_LED_NUM; i++) {
		led = &app->leds[i];

		led->timeout_count = 0;
		led->color = 0;
		led->blink_interval = 0;
	}
}

#ifndef PC_SIMULATOR
/**
 * @brief Update leds if there are any changes or timeout
 *
 * @param app The context of the management service application
 * @return Void
 */
static void update_led(mgmt_service_app_t *app)
{
	int i;
	led_device_t *led;
	unsigned char color;
	unsigned short blink_interval;

	if (app->main_state != MGMT_SERVICE_STATE_ATTACHED) {
		return;
	}

	for (i = 0; i < MAX_LED_NUM; i++) {
		led = &app->leds[i];

		led->timeout_count++;
		if (!rdb_app_get_led_status(i, &color, &blink_interval)) {
			if ((app->led_forced_update) ||
				(led->color != color) ||
				(led->blink_interval != blink_interval) ||
				(led->timeout_count > LED_TIMEOUT_COUNT)) {
				led->color = color;
				led->blink_interval = blink_interval;
				led->timeout_count = 0;
				send_led_request(app, i, color, blink_interval);
			}
		}
	}
	app->led_forced_update = 0;
}

/**
 * @brief Converts status code to string
 *
 * @param status The status of the battery
 * @return A string on success, or "Error" on error.
 */
static const char *conv_battery_status_to_string(unsigned char status)
{
	const char *battery_status_str[] = {
		"Discharging",
		"Charging",
		"Fully Charged",
		"Error"
	};

	if (status < ARRAY_SIZE(battery_status_str)) {
		return battery_status_str[status];
	}

	return "Error";
}
#endif
