/**
 * @file test_service_app.c
 * @brief Implements service which forwards test commands and the corresponding responses
 *        between a test tool and a test device. It's not interested in the content of
 *        test commands and responses.
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
#include "test_service_app.h"
#include "scomm_manager.h"
#include "event_loop.h"
#include "util.h"
#include "oss.h"
#include "logger.h"
#include "app_api.h"

#include "scomm_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

/* Polling time on the sock service, it affects how quickly new request is accepted */
#define TEST_SERVICE_LOOP_TIMEOUT_MS 3000

#define TEST_SERVICE_UDP_PORT 15800 /* Picks up from non-well known ports */

#define MAX_CMD_REQUEST_LEN 256 /* The command should be less than this size */
#define MAX_CMD_RESPONSE_LEN 512 /* The response should be less than this size */

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/**
 * @brief The main structure of the sock service application
 */
typedef struct test_service_app {
	scomm_manager_t *manager;
	scomm_service_t *scomm_service;
	scomm_service_client_t scomm_client;

	/* event loop variables */
	event_loop_t *loop;
	event_loop_client_t *loop_client;

	int fd;
	unsigned short port;

	struct sockaddr cmd_req_addr;
	socklen_t cmd_req_addr_len;

	unsigned char cmd_req[MAX_CMD_REQUEST_LEN];
	unsigned char cmd_resp[MAX_CMD_RESPONSE_LEN];
} test_service_app_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int init_app(test_service_app_t *app, scomm_manager_t *manager, scomm_service_t *service);
static int init_connection(test_service_app_t *app);
static void poll_app(void *param);
static int scomm_data_received(scomm_packet_t *packet, void *owner);
static int send_command_response(test_service_app_t *app, const unsigned char *resp, int len);
static int send_command_request(test_service_app_t *app, const unsigned char *req, int len);
static void test_data_received(void *param);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

test_service_app_t *test_service_app_create(scomm_manager_t *manager, scomm_service_t *service)
{
	test_service_app_t *app;

	app = malloc(sizeof(test_service_app_t));
	if (!app) {
		LOG_ERR_AT();
		goto out;
	}
	memset(app, 0, sizeof(test_service_app_t));

	app->loop = event_loop_create();
	if (!app->loop) {
		LOG_ERR_AT();
		goto out;
	}

	if (init_app(app, manager, service)) {
		LOG_ERR_AT();
		goto out;
	}

	return app;

out:
	LOG_ERR("test_service: failed to create a test service\n");
	test_service_app_destroy(app);
	return NULL;
}

void test_service_app_destroy(test_service_app_t *app)
{
	if (app) {
		if (app->loop) {
			event_loop_destroy(app->loop);
		}
		free(app);
	}
}

int test_service_app_start(test_service_app_t *app)
{
	if (app->loop_client && (app->fd >= 0)) {
		event_loop_remove_client(app->loop, app->fd);
	}

	app->loop_client = event_loop_add_client(app->loop, app->fd,
								test_data_received,
								NULL,
								NULL,
								app);

	LOG_NOTICE_AT();
	return 0;
}

int test_service_app_stop(test_service_app_t *app)
{
	if (app->loop_client) {
		event_loop_remove_client(app->loop, app->fd);
		app->loop_client = NULL;
	}

	LOG_NOTICE_AT();
	return 0;
}

void test_service_app_run(test_service_app_t *app)
{
	event_loop_option_t loop_options;

	event_loop_init_options(&loop_options);

	loop_options.timeout_param = app;
	loop_options.timeout_cb = poll_app;
	loop_options.timeout_interval = TEST_SERVICE_LOOP_TIMEOUT_MS;
	loop_options.processed_param = app;
	loop_options.processed_cb = poll_app;
	event_loop_set_options(app->loop, &loop_options);

	event_loop_run(app->loop);
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Initialises internal variables on the test service application
 *
 * @param app The context of the test service application
 * @param manager The pointer to the scomm manager
 * @param service The pointer to the scomm service
 * @return 0 on success, or a negative value on error
 */
static int init_app(test_service_app_t *app, scomm_manager_t *manager, scomm_service_t *service)
{
	app->manager = manager;
	app->scomm_service = service;
	app->scomm_client.service_id = SERVICE_ID_TEST;
	app->scomm_client.data_received_cb = scomm_data_received;
	app->scomm_client.owner = app;
	scomm_service_add_client(app->scomm_service, &app->scomm_client);

	app->fd = -1;
	app->port = TEST_SERVICE_UDP_PORT;

	if (init_connection(app)) {
		LOG_ERR_AT();
		return -1;
	}

	return 0;
}

/**
 * @brief Initialises a connection to the testing tool.
 *
 * @param app The context of the test service application
 * @return 0 on success, or a negative value on error
 */
static int init_connection(test_service_app_t *app)
{
	struct sockaddr_in saddr;

	app->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (app->fd < 0) {
		LOG_ERR("test_service: failed to create a socket\n");
		return -1;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(app->port);
	saddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(app->fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in))) {
		LOG_ERR("test_service: failed to bind the socket\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Does peridioc jobs when the application is not busy.
 *
 * @param app The context of the test service application
 * @return Void
 */
static void poll_app(void *param)
{
	/* Nothing to do */
	LOG_INFO("poll test_service\n");
}

/**
 * @brief Send a test command response to the testing tool.
 *
 * @param app The context of the test service application
 * @param resp The command response coming from the test device
 * @param len The number of bytes in resp
 * @return 0 on success, or a negative value on error.
 */
static int send_command_response(test_service_app_t *app, const unsigned char *resp, int len)
{
	int sent_len;

	if (app->fd < 0) {
		LOG_WARN("test service: sending a response on the closed socket(%d)\n", app->fd);
		return -1;
	}

	sent_len = sendto(app->fd, resp, len, 0, &app->cmd_req_addr, app->cmd_req_addr_len);
	if (sent_len <= 0) {
		LOG_WARN("test service: failed to write a repsponse(%d,%d,%s)\n", app->fd, sent_len,
				strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief Handles data coming from the scomm channel
 *
 * @param packet The received packet from the scomm channel
 * @param owner The context of the test service application
 * @return 0 on success, or a negative value on error
 */
static int scomm_data_received(scomm_packet_t *packet, void *owner)
{
	int resp_len;
	int result = -1;
	test_service_app_t *app = owner;

	resp_len = scomm_packet_get_data_length(packet);
	if (resp_len >= sizeof(app->cmd_resp)) {
		LOG_ERR("Too big test cmd\n");
		goto out;
	}

	if (scomm_packet_remove_head_bytes(packet, app->cmd_resp, resp_len)) {
		LOG_ERR("Invalid packet for a test cmd response\n");
		goto out;
	}

	app->cmd_resp[resp_len] = '\0';
	LOG_NOTICE("test cmd response: (%d,%s)\n", resp_len, app->cmd_resp);
	if (send_command_response(app, app->cmd_resp, resp_len)) {
		LOG_ERR("test service: failed to send the command response\n");
	}

out:
	scomm_packet_destroy(packet);
	return result;
}

/**
 * @brief Sends a test command to the test device
 *
 * @param app The context of the test service application
 * @param req The location to the data to be sent
 * @param len The number of bytes in the req
 * @return 0 on success, or a negative value on error
 */
static int send_command_request(test_service_app_t *app, const unsigned char *req, int len)
{
	int result = -1;
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		LOG_ERR_AT();
		return -1;
	}

	if (scomm_packet_append_tail_bytes(packet, req, len)) {
		LOG_ERR_AT();
		goto free_packet_out;
	}

	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	result = 0;

free_packet_out:
	scomm_packet_destroy(packet);
	return result;
}

/**
 * @brief Reads a packet from the testing tool and forwards it to the test device.
 *
 * @param param The context of the test service application
 * @return Void
 */
static void test_data_received(void *param)
{
	int recv_len;
	struct sockaddr_in *saddr;
	test_service_app_t *app = param;

	assert(app);
	if (app->fd < 0) {
		LOG_WARN("test service: data received on the closed socket(%d)\n", app->fd);
		return;
	}

	app->cmd_req_addr_len = sizeof(app->cmd_req_addr);
	recv_len = recvfrom(app->fd, app->cmd_req, (sizeof(app->cmd_req) - 1),
						0, &app->cmd_req_addr, &app->cmd_req_addr_len);
	if (recv_len <= 0) {
		LOG_WARN("test service: no data received(%d,%d,%s)\n", app->fd, recv_len, strerror(errno));
		return;
	}

	saddr = (struct sockaddr_in *)&app->cmd_req_addr;
	LOG_DEBUG("test_data_received recv fd: %d, addr: %s, port: %d\n",
			app->fd, inet_ntoa(saddr->sin_addr), ntohs(saddr->sin_port));

	app->cmd_req[recv_len] = '\0';
	LOG_INFO("test_data_received recv_len: %d, cmd_req: %s\n", recv_len, app->cmd_req);

	if (send_command_request(app, app->cmd_req, recv_len)) {
		LOG_WARN("test service: failed to send the command request\n");
	}
}
