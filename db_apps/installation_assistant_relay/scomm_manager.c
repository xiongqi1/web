/**
 * @file scomm_manager.c
 * @brief Implements serial channel that provides mechanism to communicates processes through serial
 *        devices
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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
#include "scomm_manager.h"
#include "serial_channel.h"
#include "mgmt_service_app.h"
#include "sock_service_app.h"
#include "test_service_app.h"
#include "event_loop.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define SCOMM_MANAGER_SIGNATURE     0x53434D41 /* 'SCMA' */

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

typedef struct scomm_manager {
	scomm_service_t *scomm;
	mgmt_service_app_t *mgmt_service;
	sock_service_app_t *sock_service;
	test_service_app_t *test_service;

	serial_channel_t *channel;

	event_loop_t *loop;
	event_loop_client_t *loop_client;

	int (*data_received_cb)(void *param);
	void *data_received_param;

	pthread_t sock_service_thread;
	pthread_t test_service_thread;

	char *hostname;
	unsigned int service_mask;

	unsigned int signature;
} scomm_manager_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static void create_sock_service_thread(void);
static void create_test_service_thread(void);
static void scomm_manager_main_timeout(void *param);
static int serial_write_bytes(unsigned char *data, int len);
static int serial_read_bytes(unsigned char *data, int len);
static int serial_set_data_received(int (*data_received)(void *param), void *param);
static void data_received(void *param);
static void error_notified(void *param);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

static const scomm_driver_t serial_driver = {
	.read_bytes = serial_read_bytes,
	.write_bytes = serial_write_bytes,
	.set_data_received = serial_set_data_received,
};

static scomm_manager_t context;

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

int scomm_manager_init(void)
{
	context.scomm = scomm_service_alloc();
	if (!context.scomm) {
		goto out;
	}
	scomm_service_init(context.scomm, &serial_driver);

	context.channel = serial_channel_create();
	if (!context.channel) {
		goto free_scomm_service_out;
	}

	context.mgmt_service = mgmt_service_app_create(&context, context.scomm);
	if (!context.mgmt_service) {
		goto free_serial_channel_out;
	}

	context.sock_service = NULL;
	context.test_service = NULL;

	context.loop = event_loop_create();
	if (!context.loop) {
		goto free_mgmt_service_out;
	}

	context.signature = SCOMM_MANAGER_SIGNATURE;

	LOG_INFO_AT();
	return 0;

free_mgmt_service_out:
	mgmt_service_app_destroy(context.mgmt_service);

free_serial_channel_out:
	serial_channel_destroy(context.channel);

free_scomm_service_out:
	scomm_service_free(context.scomm);

out:
	LOG_ERR_AT();
	return -1;
}

int scomm_manager_start(void)
{
	if (serial_channel_start(context.channel)) {
		LOG_ERR_AT();
		return -1;
	}

	if (scomm_service_start(context.scomm)) {
		LOG_ERR_AT();
		return -1;
	}

	if (mgmt_service_app_start(context.mgmt_service)) {
		LOG_ERR_AT();
		return -1;
	}

	/* Initialise secondary services such as sock service */
	if (context.service_mask & SERVICE_ID_SOCKET_MASK) {
		context.sock_service = sock_service_app_create(&context, context.scomm);
		if (context.sock_service) {
			sock_service_app_set_hostname(context.sock_service, context.hostname);
			create_sock_service_thread();
		}
	}

	if (context.service_mask & SERVICE_ID_TEST_MASK) {
		context.test_service = test_service_app_create(&context, context.scomm);
		if (context.test_service) {
			create_test_service_thread();
		}
	}

	context.loop_client = event_loop_add_client(context.loop,
			serial_channel_get_fd(context.channel),
			data_received, NULL, error_notified, context.channel);

	if (!context.loop_client) {
		LOG_ERR_AT();
		return -1;
	}

	LOG_INFO_AT();
	return 0;
}

void scomm_manager_init_options(scomm_manager_option_t *options)
{
	options->device_name = NULL;
	options->hostname = NULL;
	options->serial_name = NULL;
	options->baud_rate = 0;
	options->service_mask = 0;
}

void scomm_manager_set_options(scomm_manager_option_t *options)
{
	context.service_mask = options->service_mask;

	if (options->hostname) {
		if (context.hostname) {
			free(context.hostname);
		}
		context.hostname = strdup(options->hostname);
	}

	if (context.mgmt_service) {
		mgmt_service_app_set_device_name(context.mgmt_service, options->device_name);
	}

	if (context.channel) {
		serial_channel_option_t serial_options;

		serial_channel_init_options(&serial_options);

		serial_options.name = options->serial_name;
		serial_options.baud_rate = options->baud_rate;
		serial_channel_set_options(context.channel, &serial_options);
	}
}

void scomm_manager_main_run(void)
{
	event_loop_option_t loop_options;

	event_loop_init_options(&loop_options);

	loop_options.timeout_param = &context;
	loop_options.timeout_cb = scomm_manager_main_timeout;
	loop_options.timeout_interval = SCOMM_MANAGER_IDLE_LOOP_TIMEOUT_MS;
	event_loop_set_options(context.loop, &loop_options);

	event_loop_run(context.loop);
}

void scomm_manager_device_attached(int attached)
{
	if (attached) {
		if (context.sock_service) {
			if (sock_service_app_start(context.sock_service)) {
				LOG_ERR_AT();
			}
		}
		if (context.test_service) {
			test_service_app_start(context.test_service);
		}
	} else {
		if (context.sock_service) {
			if (sock_service_app_stop(context.sock_service)) {
				LOG_ERR_AT();
			}
		}
		if (context.test_service) {
			test_service_app_stop(context.test_service);
		}
	}
}

unsigned int scomm_manager_get_available_service_mask(void)
{
	unsigned int service_mask;

	service_mask = 0;
	if (context.mgmt_service) {
		service_mask |= SERVICE_ID_MANAGEMENT_MASK;
	}

	if (context.sock_service) {
		service_mask |= SERVICE_ID_SOCKET_MASK;
	}

	if (context.test_service) {
		service_mask |= SERVICE_ID_TEST_MASK;
	}

	return service_mask;
}

int scomm_manager_change_baud_rate(int baud_rate)
{
	if (!context.channel) {
		LOG_ERR_AT();
		return -1;
	}

	if (serial_channel_stop(context.channel)) {
		LOG_ERR_AT();
		return -1;
	}

	LOG_NOTICE("Set baud rate to %d\n", baud_rate);
	serial_channel_set_baud_rate(context.channel, baud_rate);
	if (serial_channel_start(context.channel)) {
		LOG_ERR_AT();
		return -1;
	}

	return 0;
}

void scomm_manager_change_polling_interval(int fast)
{
	if (fast) {
		event_loop_set_timeout_interval(context.loop, SCOMM_MANAGER_ACTIVE_LOOP_TIMEOUT_MS);
	} else {
		event_loop_set_timeout_interval(context.loop, SCOMM_MANAGER_IDLE_LOOP_TIMEOUT_MS);
	}
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Thread routine to run the sock_server thread
 *
 * @param param The instance of the main application
 * @return Void
 */
static void *do_sock_service_thread(void *param)
{
	if (context.sock_service) {
		sock_service_app_run(context.sock_service);
	}
	return param;
}

/**
 * @brief Create a sock service thread
 *
 * @param param The instance of the main application
 * @return Void
 */
static void create_sock_service_thread(void)
{
	if (pthread_create(&context.sock_service_thread, NULL, do_sock_service_thread, &context)) {
		fprintf(stderr, "Error creating sock_service thread\n");
	}
}

/**
 * @brief Thread routine to run the test_server thread
 *
 * @param param The instance of the main application
 * @return Void
 */
static void *do_test_service_thread(void *param)
{
	if (context.test_service) {
		test_service_app_run(context.test_service);
	}
	return param;
}

/**
 * @brief Create a test service thread
 *
 * @param param The instance of the main application
 * @return Void
 */
static void create_test_service_thread(void)
{
	if (pthread_create(&context.test_service_thread, NULL, do_test_service_thread, &context)) {
		fprintf(stderr, "Error creating test_service thread\n");
	}
}

static void scomm_manager_main_timeout(void *param)
{
	mgmt_service_app_timeout(context.mgmt_service);
}

/**
 * @brief Writes the number of bytes on the serial channel
 *
 * @param data The data to be written
 * @param len The number of data in bytes
 * @return The number of written data or a negative value on error
 */
static int serial_write_bytes(unsigned char *data, int len)
{
	return serial_channel_write(context.channel, data, len);
}

/**
 * @brief Reads the number of bytes on the serial channel
 *
 * @param data The data to be read
 * @param len The number of data in bytes
 * @return The number of read data or a negative value on error
 */
static int serial_read_bytes(unsigned char *data, int len)
{
	return serial_channel_read(context.channel, data, len);
}

/**
 * @brief Sets a callback function and a parameter for the function.
 *
 * @param data_received_cb The function to be called when rx data is available on the serial port
 * @param param The paramter to be used as an argument when the callback function is called
 * @return 0 on success, or a negative value on error
 */
static int serial_set_data_received(int (*data_received_cb)(void *param), void *param)
{
	context.data_received_cb = data_received_cb;
	context.data_received_param = param;
	return 0;
}

/**
 * @brief Called when rx data is available on the serial port
 *
 * @param param The parameter that registered when the callback function is set
 * @return Void
 */
static void data_received(void *param)
{
	if (context.data_received_cb) {
		context.data_received_cb(context.data_received_param);
	}
}

/**
 * @brief Called when an error occurred in the serial channel.
 *
 * @param param The parameter that registered when the callback function is set
 * @return Void
 */
static void error_notified(void *param)
{
	/* There is nothing to do if the error occurred at the serial communication */
	LOG_ERR("Terminate the program\r\n");
	exit(1);
}
