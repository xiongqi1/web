/**
 * @file sock_service_app.c
 * @brief Implements service which manages connections between socket and scomm.
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
#include "sock_service_app.h"
#include "sock_service_proto.h"
#include "socket_channel.h"
#include "pipe_channel.h"
#include "event_loop.h"
#include "util.h"
#include "oss.h"
#include "logger.h"
#include "app_api.h"

#include "scomm_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

/* should be less than the maximum size of payload in scomm_packet_t */
#define MAX_SOCKET_RX_BUFFER_LEN MIN(512, MAX_SCOMM_PACKET_SIZE)

#define MAX_SOCKET_CHANNEL_NUM 16

#define DEFAULT_SOCKET_HOSTNAME "127.0.0.1"

/* How many packets allowed to send when there is no acknowledgement for a long time */
#define MIN_SOCKET_ALLOWED_PACKETS_NUM 1

/* Wait before sending another packet when there is no acknowledgement for the following time */
#define SOCK_SERVICE_SYNC_TIMEOUT_MS 5000

/* Accepts the 3 packets without acknowledgement */
#define MAX_ACCEPTABLE_RX_PACKETS 3

/* Polling time on the sock service, it affects how quickly new request is accepted */
#define SOCK_SERVICE_LOOP_TIMEOUT_MS 250

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/**
 * @brief The message structure flowing through the pipe channel
 */
typedef struct sock_service_ipc_message {
	scomm_packet_t *packet;
	sock_service_pdu_t *pdu;
} sock_service_ipc_message_t;

/**
 * @brief The main structure of the sock service application
 */
typedef struct sock_service_app {
	scomm_manager_t *manager;
	scomm_service_t *scomm_service;
	scomm_service_client_t scomm_client;

	/* sock service protocol helper */
	sock_service_proto_t proto;

	int closing_channels[MAX_SOCKET_CHANNEL_NUM];
	socket_channel_t *channels[MAX_SOCKET_CHANNEL_NUM];

	/* event loop variables */
	event_loop_t *loop;
	event_loop_client_t *loop_pipe_client;
	event_loop_client_t *loop_sock_clients[MAX_SOCKET_CHANNEL_NUM];

	pipe_channel_t *pipe;

	/* Connection managing variables */
	int attached; /**< Indicates whether device is attached or not */
	int detaching; /**< Indicates detaching is in progress or not */
	oss_sync_t *attached_sync;

	/* Flow control variables */
	int num_allowed_packets;
	oss_sync_t *flow_control_sync;

	char *hostname;

	unsigned int signature;
} sock_service_app_t;

/**
 * @brief Represents the mapping between source and target port
 */
typedef struct route_port {
	unsigned short source_port;
	unsigned short target_port;
} route_port_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int init_app(sock_service_app_t *app, scomm_manager_t *manager, scomm_service_t *service);
static void init_channels(sock_service_app_t *app);
static void poll_app(void *param);
static void device_attached(sock_service_app_t *app, int attached);
static void wait_connection(sock_service_app_t *app);
static void wait_peer_ready(sock_service_app_t *app);
static int scomm_data_received(scomm_packet_t *packet, void *owner);
static int open_socket(sock_service_app_t *app, unsigned char id, unsigned short port);
static void close_socket(sock_service_app_t *app, unsigned char id);
static void close_socket_scheduled(sock_service_app_t *app, unsigned char id);
static int send_close_socket_indication(sock_service_app_t *app, int id, sock_type_t type);
static int send_open_socket_response(sock_service_app_t *app, int id, sock_type_t type,
		unsigned short port);
static int send_close_socket_response(sock_service_app_t *app, int id, sock_type_t type);
static int send_buffer_avail_indication(sock_service_app_t *app, int id, sock_type_t type,
		unsigned char num_packets);
static int should_handle_scomm_packet_immediately(sock_service_app_t *app,
		sock_service_pdu_t *pdu);
static int handle_scomm_packet(sock_service_app_t *app, sock_service_pdu_t *pdu);
static int handle_scomm_control_packet(sock_service_app_t *app, sock_service_pdu_t *pdu);
static int handle_scomm_data_packet(sock_service_app_t *app, sock_service_pdu_t *pdu);
static void pipe_data_received(void *param);
static void pipe_error_notified(void *param);
static int open_pipe(sock_service_app_t *app);
static void close_pipe(sock_service_app_t *app);
static void flush_pipe(sock_service_app_t *app);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

sock_service_app_t *sock_service_app_create(scomm_manager_t *manager, scomm_service_t *service)
{
	sock_service_app_t *app;

	app = malloc(sizeof(sock_service_app_t));
	if (!app) {
		LOG_ERR_AT();
		goto out;
	}
	memset(app, 0, sizeof(sock_service_app_t));

	app->loop = event_loop_create();
	if (!app->loop) {
		LOG_ERR_AT();
		goto out;
	}

	app->pipe = pipe_channel_create();
	if (!app->pipe) {
		LOG_ERR_AT();
		goto out;
	}

	app->attached_sync = oss_sync_create();
	if (!app->attached_sync) {
		LOG_ERR_AT();
		goto out;
	}

	app->flow_control_sync = oss_sync_create();
	if (!app->flow_control_sync) {
		LOG_ERR_AT();
		goto out;
	}

	init_app(app, manager, service);

	return app;

out:
	sock_service_app_destroy(app);
	return NULL;
}

void sock_service_app_destroy(sock_service_app_t *app)
{
	if (app) {
		if (app->hostname) {
			free(app->hostname);
		}
		if (app->attached_sync) {
			oss_sync_destroy(app->attached_sync);
		}
		if (app->flow_control_sync) {
			oss_sync_destroy(app->flow_control_sync);
		}
		if (app->pipe) {
			pipe_channel_destroy(app->pipe);
		}
		if (app->loop) {
			event_loop_destroy(app->loop);
		}
		free(app);
	}
}

int sock_service_app_start(sock_service_app_t *app)
{
	int i;

	if (app->attached) {
		LOG_NOTICE("sock service already started\n");
		return -1;
	}

	open_pipe(app);
	for (i = 0; i < MAX_SOCKET_CHANNEL_NUM; i++) {
		app->channels[i] = NULL;
	}

	device_attached(app, 1);
	LOG_NOTICE_AT();
	return 0;
}

int sock_service_app_stop(sock_service_app_t *app)
{
	int i;

	if (!app->attached) {
		LOG_NOTICE("Trying to stop sock service but it was not started before\n");
		return -1;
	}

	close_pipe(app);
	for (i = 0; i < MAX_SOCKET_CHANNEL_NUM; i++) {
		if (app->channels[i]) {
			close_socket_scheduled(app, i);
		}
	}

	device_attached(app, 0);
	LOG_NOTICE_AT();
	return 0;
}

void sock_service_app_run(sock_service_app_t *app)
{
	event_loop_option_t loop_options;

	wait_connection(app);

	event_loop_init_options(&loop_options);

	loop_options.timeout_param = app;
	loop_options.timeout_cb = poll_app;
	loop_options.timeout_interval = SOCK_SERVICE_LOOP_TIMEOUT_MS;
	loop_options.processed_param = app;
	loop_options.processed_cb = poll_app;
	event_loop_set_options(app->loop, &loop_options);

	event_loop_run(app->loop);
}

void sock_service_app_set_hostname(sock_service_app_t *app, const char *name)
{
	if (app->hostname) {
		free(app->hostname);
	}
	app->hostname = strdup(name);
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Initialise internal variables on the sock service application
 *
 * @param app The context of the sock service application
 * @param manager The pointer to the scomm manager
 * @param service The pointer to the scomm service
 * @return 0 on success, or a negative value on error
 */
static int init_app(sock_service_app_t *app, scomm_manager_t *manager, scomm_service_t *service)
{
	app->signature = SOCK_SERVICE_APP_SIGNATURE;

	app->manager = manager;
	app->scomm_service = service;
	app->hostname = strdup(DEFAULT_SOCKET_HOSTNAME);
	app->attached = 0;
	app->detaching = 0;
	app->num_allowed_packets = MIN_SOCKET_ALLOWED_PACKETS_NUM;

	app->scomm_client.service_id = SERVICE_ID_SOCKET;
	app->scomm_client.data_received_cb = scomm_data_received;
	app->scomm_client.owner = app;
	scomm_service_add_client(app->scomm_service, &app->scomm_client);

	init_channels(app);

	sock_service_proto_init(&app->proto);

	LOG_INFO_AT();
	return 0;
}

/**
 * @brief Initialises variables relating to the communication channels
 *
 * @param app The context of the sock service application
 * @return Void
 */
static void init_channels(sock_service_app_t *app)
{
	int i;

	for (i = 0; i < MAX_SOCKET_CHANNEL_NUM; i++) {
		app->closing_channels[i] = 0;
		app->channels[i] = NULL;
	}
}

/**
 * @brief Does peridioc jobs when the application is not busy.
 *
 * @param param The context of the sock service application
 * @return Void
 */
static void poll_app(void *param)
{
	int i;
	sock_service_app_t *app = param;

	/* Closes the sockets which is scheduled to close */
	for (i = 0; i < MAX_SOCKET_CHANNEL_NUM; i++) {
		if (app->channels[i] && app->closing_channels[i]) {
			LOG_NOTICE("poll_app close socket: %d\n", i);
			close_socket(app, i);
		}
	}

	/*
	 * If Installation Assistant is detached but the application is still running,
	 * Suspend the application until the Installation Assistant is attached again.
	 */
	if (!app->attached && app->detaching) {
		/* Now it waits for next connection */
		app->detaching = 0;
		wait_connection(app);
	}
}

/**
 * @brief Called when Installation Assistant is attached or detached.
 *
 * @param app The context of the sock service application
 * @param attached Indicates whether Installation Assistant is attached or not
 * @return Void
 */
static void device_attached(sock_service_app_t *app, int attached)
{
	LOG_INFO("[SockA] device_attached : %d\n", attached);
	app->attached = attached;
	if (attached) {
		oss_sync_signal(app->attached_sync);
	} else {
		/* Postponed detaching process until it handles all pending events */
		app->detaching = 1;
	}
}

/**
 * @brief Waits until Installation Assistant is attached.
 *
 * @param app The context of the sock service application
 * @return Void
 */
static void wait_connection(sock_service_app_t *app)
{
	LOG_NOTICE("waiting connection : %d\n", app->attached);
	while (!app->attached) {
		oss_sync_wait(app->attached_sync, 0);
	}
	LOG_NOTICE("new connection established: %d\n", app->attached);
}

/**
 * @brief Waits until the peer is avaible to accept next packet
 *
 * @param app The context of the sock service application
 * @return Void
 */
static void wait_peer_ready(sock_service_app_t *app)
{
	if (!app->num_allowed_packets && app->flow_control_sync) {
		oss_sync_wait(app->flow_control_sync, SOCK_SERVICE_SYNC_TIMEOUT_MS);
		/* Set minimum value if the sync timer expires */
		if (!app->num_allowed_packets) {
			LOG_WARN("Allow to send one packet in congestion\n");
			app->num_allowed_packets = MIN_SOCKET_ALLOWED_PACKETS_NUM;
		} else {
			LOG_INFO("Allow to send %d packets\n", app->num_allowed_packets);
		}
	}
}

/**
 * @brief Handles the received packet for the socket service
 *
 * @param app The context of the sock service application
 * @param pdu The decoded pdu for socket service
 * @return 0 on success, or a negative value on error
 */
static int handle_scomm_data_packet(sock_service_app_t *app, sock_service_pdu_t *pdu)
{
	int id = pdu->channel_id;

	if ((id < 0) || (id >= MAX_SOCKET_CHANNEL_NUM)) {
		LOG_ERR_AT();
		return -1;
	}

	if (app->channels[id]) {
		if (pdu->u.data.len != socket_channel_write(app->channels[id],
					pdu->u.data.payload, pdu->u.data.len)) {
			LOG_NOTICE("socket writing error: %d\n", id);
			close_socket_scheduled(app, id);
			send_close_socket_indication(app, id, SOCK_TYPE_TCP);
		} else {
			LOG_DEBUG("]]]SRX(%d) L:%d[[[\n", id, pdu->u.data.len);
		}
	} else {
		LOG_ERR_AT();
		return -1;
	}

	send_buffer_avail_indication(app, id, pdu->u.control.command_type,
			MAX_ACCEPTABLE_RX_PACKETS);
	return 0;
}

/**
 * @brief Reads a packet from the socket channel and forward it to the serial channel
 *
 * @param param The context of the sock service application
 * @return Void
 */
static void socket_data_received(void *param)
{
	int id;
	int length;
	static int packet_seq = 0;
	unsigned char buffer[MAX_SOCKET_RX_BUFFER_LEN];
	scomm_packet_t *packet;
	sock_service_app_t *app;
	socket_channel_t *channel = param;

	if (!channel || (socket_channel_get_fd(channel) < 0)) {
		return;
	}

	id = socket_channel_get_id(channel);
	if ((id < 0) || (id >= MAX_SOCKET_CHANNEL_NUM)) {
		LOG_ERR_AT();
		return;
	}

	app = socket_channel_get_owner(channel);
	if (!app || (app->signature != SOCK_SERVICE_APP_SIGNATURE)) {
		LOG_ERR_AT();
		return;
	}

	if (!app->channels[id]) {
		LOG_WARN("Trying to read a closed socket ID: %d\n", id);
		return;
	}

	/* Wait until the peer is ready to accept the next packet */
	wait_peer_ready(app);

	packet = scomm_packet_create();
	if (!packet) {
		LOG_ERR_AT();
		return;
	}

	length = socket_channel_read(channel, buffer, sizeof(buffer));
	if (length <= 0) {
		LOG_NOTICE("Socket ID:%d read error: %d\n", id, length);
		close_socket_scheduled(app, id);
		send_close_socket_indication(app, id, SOCK_TYPE_TCP);
		goto out;
	}
	sock_service_proto_encode_data_packet(&app->proto, packet, SOCK_TYPE_TCP, id,
			buffer, length);

	scomm_service_send_data(app->scomm_service, &app->scomm_client, packet);
	if (app->num_allowed_packets > 0) {
		app->num_allowed_packets--;
	}

	/* Only for debugging, it will be removed soon */
	if (length == sizeof(buffer)) {
		packet_seq++;
	} else {
		packet_seq = 0;
	}
	LOG_DEBUG("[[[STX(%d)[%d] L:%d,RP:%d]]]\n", id, packet_seq, length, app->num_allowed_packets);

out:
	scomm_packet_destroy(packet);
}

/**
 * @brief Called when the socket has an error
 *
 * @param param The context of the sock service application
 * @return Void
 */
static void socket_error_notified(void *param)
{
	int id;
	sock_service_app_t *app;
	socket_channel_t *channel = param;

	if (!channel || (socket_channel_get_fd(channel) < 0)) {
		return;
	}

	id = socket_channel_get_id(channel);
	if ((id < 0) || (id >= MAX_SOCKET_CHANNEL_NUM)) {
		return;
	}

	app = socket_channel_get_owner(channel);
	if (!app || (app->signature != SOCK_SERVICE_APP_SIGNATURE)) {
		LOG_ERR_AT();
		return;
	}

	close_socket_scheduled(app, id);
	send_close_socket_indication(app, id, SOCK_TYPE_TCP);
	LOG_NOTICE("socket error notified ID:%d\n", id);
}

/**
 * @brief Opens the socket with the given port in the channel
 *
 * @param app The context of the sock service application
 * @param id The channel ID to use the socket
 * @param port The port number to be opened
 * @return 0 on success, or a negative value on error
 */
static int open_socket(sock_service_app_t *app, unsigned char id, unsigned short port)
{
	socket_channel_option_t options;

	LOG_INFO_AT();
	if (app->channels[id]) {
		close_socket(app, id);
	}

	app->channels[id] = socket_channel_create(id, app);
	if (!app->channels[id]) {
		LOG_ERR_AT();
		goto out;
	}

	socket_channel_init_options(&options);

	options.hostname = app->hostname;
	options.port = get_target_port(port);
	socket_channel_set_options(app->channels[id], &options);

	if (socket_channel_start(app->channels[id])) {
		LOG_ERR_AT();
		goto close_socket_out;
	}

	app->loop_sock_clients[id] = event_loop_add_client(app->loop,
			socket_channel_get_fd(app->channels[id]),
			socket_data_received,
			NULL,
			socket_error_notified,
			app->channels[id]);

	if (!app->loop_sock_clients[id]) {
		LOG_ERR_AT();
		goto stop_channel_out;
	}

	LOG_NOTICE("open_socket ID: %d\n", id);
	return 0;

stop_channel_out:
	socket_channel_stop(app->channels[id]);
close_socket_out:
	close_socket(app, id);
out:
	send_close_socket_indication(app, id, SOCK_TYPE_TCP);
	return -1;
}

/**
 * @brief Schedules closing the socket.
 *
 * @param app The context of the sock service application
 * @param id The channel ID to use the socket
 * @return Void
 */
static void close_socket_scheduled(sock_service_app_t *app, unsigned char id)
{
	if (app->channels[id]) {
		app->closing_channels[id] = 1;
		LOG_DEBUG("close_socket_scheduled ID: %d\n", id);
	}
}

/**
 * @brief Close the socket which is associated with the given channel
 *
 * @param app The context of the sock service application
 * @param id The channel ID to use the socket
 * @return Void
 */
static void close_socket(sock_service_app_t *app, unsigned char id)
{
	if (app->channels[id]) {
		event_loop_remove_client(app->loop, socket_channel_get_fd(app->channels[id]));
		socket_channel_stop(app->channels[id]);
		socket_channel_destroy(app->channels[id]);
		app->channels[id] = NULL;
	} else {
		LOG_NOTICE("channel(%d) is already closed\n", id);
	}
	app->closing_channels[id] = 0;
	LOG_NOTICE("close_socket ID: %d\n", id);
}

/**
 * @brief Called when data is available on the pipe
 *
 * @param param The context of the sock service application
 * @return 0 on success, or a negative value on error
 */
static void pipe_data_received(void *param)
{
	sock_service_ipc_message_t message;
	sock_service_app_t *app = param;

	/* Discard the callback if the param is invalid */
	if (!app || (app->signature != SOCK_SERVICE_APP_SIGNATURE)) {
		LOG_ERR_AT();
		return;
	}

	memset(&message, 0, sizeof(message));
	if (pipe_channel_read(app->pipe, &message, sizeof(message)) == sizeof(message)) {
		if (message.pdu && message.packet) {
			handle_scomm_packet(app, message.pdu);
			scomm_packet_destroy(message.packet);
			free(message.pdu);
		} else {
			LOG_ERR_AT();
		}
	} else {
		LOG_WARN_AT();
	}
}

/**
 * @brief Called when an error occurs on the pipe
 *
 * @param param The context of the sock service application
 * @return Void
 */
static void pipe_error_notified(void *param)
{
	LOG_ERR_AT();
}

/**
 * @brief Opens the pipe
 *
 * @param app The context of the sock service application
 * @return 0 on success, or a negative value on error
 */
static int open_pipe(sock_service_app_t *app)
{
	if (pipe_channel_start(app->pipe)) {
		LOG_ERR_AT();
		return -1;
	}

	app->loop_pipe_client = event_loop_add_client(app->loop,
			pipe_channel_get_read_fd(app->pipe),
			pipe_data_received,
			NULL,
			pipe_error_notified,
			app);

	if (!app->loop_pipe_client) {
		LOG_ERR_AT();
		return -1;
	}

	LOG_NOTICE_AT();
	return 0;
}

/**
 * @brief Closes the pipe
 *
 * @param app The context of the sock service application
 * @return Void
 */
static void close_pipe(sock_service_app_t *app)
{
	/* Flush before closing the pipe */
	flush_pipe(app);

	if (app->loop_pipe_client) {
		event_loop_remove_client(app->loop, pipe_channel_get_read_fd(app->pipe));
	}

	if (pipe_channel_stop(app->pipe)) {
		LOG_ERR_AT();
	}

	LOG_NOTICE_AT();
}

/**
 * @brief Flush the pipe
 *
 * @param app The context of the sock service application
 * @return Void
 */
static void flush_pipe(sock_service_app_t *app)
{
	sock_service_ipc_message_t message;

	while (pipe_channel_read(app->pipe, &message, sizeof(message)) == sizeof(message)) {
		if (message.pdu && message.packet) {
			scomm_packet_destroy(message.packet);
			free(message.pdu);
		}
	}
}

/**
 * @brief Checks whether the pdu should be handled immediately
 *
 * @param app The context of the sock service application
 * @param pdu The decoded pdu for socket service
 * @return 1 if it should be handled right now, 0 otherwise
 */
static int should_handle_scomm_packet_immediately(sock_service_app_t *app,
		sock_service_pdu_t *pdu)
{
	if (pdu->type == SOCKET_MESSAGE_TYPE_CONTROL) {
		if (pdu->u.control.command_type == SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND) {
			/*
			 * Buffer available indication should be handled immediately to release
			 * the sock service app if it's suspended.
			 */
			if (pdu->u.control.command_id == SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL) {
				return 1;
			}
		}
	}

	return 0;
}

/**
 * @brief Handles control data coming from the scomm channel
 *
 * @param app The context of the sock service application
 * @param pdu The decoded pdu for socket service
 * @return 0 on success, or a negative value on error
 */
static int handle_scomm_packet(sock_service_app_t *app, sock_service_pdu_t *pdu)
{
	int result;

	if (pdu->type == SOCKET_MESSAGE_TYPE_CONTROL) {
		result = handle_scomm_control_packet(app, pdu);
	} else {
		result = handle_scomm_data_packet(app, pdu);
	}

	return result;
}

/**
 * @brief Handles the response in the control command
 *
 * @param app The context of the sock service application
 * @param pdu The decoded pdu for socket service
 * @return 0 on success, or a negative value on error
 */
static int handle_scomm_control_request(sock_service_app_t *app, sock_service_pdu_t *pdu)
{
	int result = 0;

	/* We support TCP only */
	if (pdu->channel_type != SOCK_TYPE_TCP) {
		send_close_socket_indication(app, pdu->channel_id, pdu->channel_type);
		return -1;
	}

	switch (pdu->u.control.command_id) {
	case SOCKET_MESSAGE_CONTROL_COMMAND_ID_OPEN_SOCKET:
		LOG_NOTICE("COMMAND_ID_OPEN_SOCKET: %d\n", pdu->channel_id);
		open_socket(app, pdu->channel_id, pdu->u.control.u.open_socket.port);
		send_open_socket_response(app, pdu->channel_id, pdu->u.control.command_type,
				pdu->u.control.u.open_socket.port);
		break;

	case SOCKET_MESSAGE_CONTROL_COMMAND_ID_CLOSE_SOCKET:
		LOG_NOTICE("COMMAND_ID_CLOSE_SOCKET: %d\n", pdu->channel_id);
		close_socket(app, pdu->channel_id);
		send_close_socket_response(app, pdu->channel_id, pdu->u.control.command_type);
		break;

	default:
		LOG_WARN("Unknown Request ID:%d\n", pdu->u.control.command_id);
		result = -1;
		break;
	}

	return result;
}

/**
 * @brief Handles the indication in the control command
 *
 * @param app The context of the sock service application
 * @param pdu The decoded pdu for socket service
 * @return 0 on success, or a negative value on error
 */
static int handle_scomm_control_indication(sock_service_app_t *app, sock_service_pdu_t *pdu)
{
	int result;

	switch (pdu->u.control.command_id) {
	case SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL:
		if (!app->num_allowed_packets) {
			app->num_allowed_packets = pdu->u.control.u.buffer_avail.num_packets;
			if (app->flow_control_sync) {
				LOG_INFO("signal flow control\n");
				oss_sync_signal(app->flow_control_sync);
			}
		} else {
			app->num_allowed_packets = pdu->u.control.u.buffer_avail.num_packets;
		}
		LOG_INFO("Avail buffer:%d\n", app->num_allowed_packets);
		break;

	default:
		LOG_WARN("Unknown Indication ID:%d\n", pdu->u.control.command_id);
		result = -1;
		break;
	}

	return result;
}

/**
 * @brief Handles control data coming from the scomm channel
 *
 * @param app The context of the sock service application
 * @param pdu The decoded pdu for socket service
 * @return 0 on success, or a negative value on error
 */
static int handle_scomm_control_packet(sock_service_app_t *app, sock_service_pdu_t *pdu)
{
	int result;

	result = -1;
	switch (pdu->u.control.command_type) {
	case SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_REQ:
		result = handle_scomm_control_request(app, pdu);
		break;

	case SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND:
		result = handle_scomm_control_indication(app, pdu);
		break;

	default:
		break;
	}

	LOG_INFO_AT();
	return result;
}

/**
 * @brief Handles data coming from the scomm channel
 *
 * @param packet The received packet from the scomm channel
 * @param owner The context of the sock service application
 * @return 0 on success, or a negative value on error
 */
static int scomm_data_received(scomm_packet_t *packet, void *owner)
{
	sock_service_pdu_t *pdu;
	sock_service_app_t *app = owner;

	pdu = malloc(sizeof(sock_service_pdu_t));
	if (!pdu) {
		LOG_ERR_AT();
		goto free_out;
	}

	if (sock_service_proto_decode_packet(&app->proto, packet, pdu)) {
		LOG_ERR_AT();
		goto free_out;
	}

	if (should_handle_scomm_packet_immediately(app, pdu)) {
		handle_scomm_packet(app, pdu);
		scomm_packet_destroy(packet);
		free(pdu);
	} else {
		sock_service_ipc_message_t message;

		/*
		 * Pass the packet on the pipe so that it can be handled by sock service thread
		 * instead of scomm manager thread. It prevents any race condition between
		 * two threads.
		 */
		message.packet = packet;
		message.pdu = pdu;
		if (pipe_channel_write(app->pipe, &message, sizeof(message)) != sizeof(message)) {
			LOG_ERR_AT();
		}
	}

	return 0;

free_out:
	if (pdu) {
		free(pdu);
	}
	scomm_packet_destroy(packet);
	return -1;
}

/**
 * @brief Sends the repsonse for the indication of the close socket
 *
 * @param app The context of the sock service application
 * @param id The channed ID
 * @param type The type of the socket to be used
 * @return 0 on success, or a negative value on error
 */
static int send_close_socket_indication(sock_service_app_t *app, int id, sock_type_t type)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		goto out;
	}

	if (sock_service_proto_encode_close_socket_indication(&app->proto, packet, id, type)) {
		goto packet_destroy_out;
	}

	if (scomm_service_send_data(app->scomm_service, &app->scomm_client, packet) < 0) {
		goto packet_destroy_out;
	}
	scomm_packet_destroy(packet);

	LOG_NOTICE_AT();
	return 0;

packet_destroy_out:
	LOG_ERR("send_close_socket_response error\n");
	scomm_packet_destroy(packet);
out:
	return -1;
}

/**
 * @brief Sends the repsonse for the request of the open socket
 *
 * @param app The context of the sock service application
 * @param id The channed ID
 * @param type The type of the socket to be used
 * @param port The port to be opened
 * @return 0 on success, or a negative value on error
 */
static int send_open_socket_response(sock_service_app_t *app, int id, sock_type_t type,
								unsigned short port)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		goto out;
	}

	if (sock_service_proto_encode_open_socket_response(&app->proto, packet, id,
				type, port)) {
		goto packet_destroy_out;
	}

	if (scomm_service_send_data(app->scomm_service, &app->scomm_client, packet) < 0) {
		goto packet_destroy_out;
	}

	scomm_packet_destroy(packet);
	LOG_INFO_AT();
	return 0;

packet_destroy_out:
	LOG_ERR("send_open_socket_response error\n");
	scomm_packet_destroy(packet);
out:
	return -1;
}

/**
 * @brief Sends the repsonse for the request of the close socket
 *
 * @param app The context of the sock service application
 * @param id The channed ID
 * @param type The type of the socket to be used
 * @return 0 on success, or a negative value on error
 */
static int send_close_socket_response(sock_service_app_t *app, int id, sock_type_t type)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		goto out;
	}

	if (sock_service_proto_encode_close_socket_response(&app->proto, packet, id, type)) {
		goto packet_destroy_out;
	}

	if (scomm_service_send_data(app->scomm_service, &app->scomm_client, packet) < 0) {
		goto packet_destroy_out;
	}
	scomm_packet_destroy(packet);

	LOG_INFO_AT();
	return 0;

packet_destroy_out:
	LOG_ERR("send_close_socket_response error\n");
	scomm_packet_destroy(packet);
out:
	return -1;
}

/**
 * @brief Sends information regarding how many packets it can accept.
 *
 * @param app The context of the sock service application
 * @param id The index of the channel in the list of channels
 * @param type The type of the socket
 * @param num_packets The number of packets available to accept
 * @return 0 on success, a negative value on error
 */
static int send_buffer_avail_indication(sock_service_app_t *app, int id, sock_type_t type,
		unsigned char num_packets)
{
	scomm_packet_t *packet;

	packet = scomm_packet_create();
	if (!packet) {
		goto out;
	}

	if (sock_service_proto_encode_buffer_avail_indication(&app->proto, packet, id,
				type, num_packets)) {
		goto packet_destroy_out;
	}

	if (scomm_service_send_data(app->scomm_service, &app->scomm_client, packet) < 0) {
		goto packet_destroy_out;
	}
	scomm_packet_destroy(packet);
	LOG_INFO("Buffer avail ID:%d, Num:%d\n", id, num_packets);
	return 0;

packet_destroy_out:
	LOG_ERR("send_buffer_avail_indication error\n");
	scomm_packet_destroy(packet);
out:
	return -1;
}

