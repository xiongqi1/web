/**
 * @file event_loop.c
 * @brief Implements event loop to support asynchronous operations
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
#include "event_loop.h"
#include "app_config.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define EVENT_LOOP_MAX_CLIENTS_NUM 10
#define EVENT_LOOP_DEFAULT_TIMEOUT 10000
#define MAX_EVENT_LOOP_ERROR 10

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

typedef struct event_loop_timeout {
	int interval;
	void *param;
	event_loop_cb_t cb;
} event_loop_timeout_t;

typedef struct event_loop_processed {
	void *param;
	event_loop_cb_t cb;
} event_loop_processed_t;

struct event_loop_client {
	int active;
	int fd;
	event_loop_cb_t read_cb;
	event_loop_cb_t write_cb;
	event_loop_cb_t error_cb;
	void *param;
};

struct event_loop {
	int active;
	int num_fds;

	event_loop_timeout_t timeout;
	event_loop_processed_t processed;

#ifdef USE_POLL
	struct pollfd fds[EVENT_LOOP_MAX_CLIENTS_NUM];
#else
	int max_fd;
	fd_set read_fds;
	fd_set write_fds;
	fd_set error_fds;
#endif

	event_loop_client_t clients[EVENT_LOOP_MAX_CLIENTS_NUM];
	event_loop_client_t *fd_clients[EVENT_LOOP_MAX_CLIENTS_NUM];
};

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static void event_loop_init_fds(event_loop_t *loop);
static void event_loop_handle(event_loop_t *loop);
static void event_loop_timeout(event_loop_t *loop);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

event_loop_t *event_loop_create(void)
{
	event_loop_t *loop;

	loop = malloc(sizeof(event_loop_t));
	if (loop) {
		memset(loop, 0, sizeof(event_loop_t));
		event_loop_init(loop);
	}
	return loop;
}

void event_loop_destroy(event_loop_t *loop)
{
	if (loop) {
		free(loop);
	}
}

int event_loop_init(event_loop_t *loop)
{
	loop->timeout.interval = EVENT_LOOP_DEFAULT_TIMEOUT;
	loop->timeout.cb = NULL;
	loop->timeout.param = NULL;
	loop->processed.cb = NULL;
	loop->processed.param = NULL;
	return 0;
}

void event_loop_init_options(event_loop_option_t *options)
{
	options->timeout_interval = -1;
	options->timeout_cb = NULL;
	options->timeout_param = NULL;
	options->processed_cb = NULL;
	options->processed_param = NULL;
}

void event_loop_set_options(event_loop_t *loop, event_loop_option_t *options)
{
	if (options->timeout_interval >= 0) {
		loop->timeout.interval = options->timeout_interval;
	}
	if (options->timeout_cb) {
		loop->timeout.cb = options->timeout_cb;
	}
	if (options->timeout_param) {
		loop->timeout.param = options->timeout_param;
	}
	if (options->processed_cb) {
		loop->processed.cb = options->processed_cb;
	}
	if (options->timeout_param) {
		loop->processed.param = options->processed_param;
	}
}

void event_loop_set_timeout_interval(event_loop_t *loop, int interval)
{
	if (interval >= 0) {
		loop->timeout.interval = interval;
	}
}

event_loop_client_t *
event_loop_add_client(event_loop_t *loop, int fd, event_loop_cb_t read_cb,
					event_loop_cb_t write_cb, event_loop_cb_t error_cb,
					void *cb_param)
{
	int i;
	event_loop_client_t *client;

	/* Checks whether the fd is already in use */
	for (i = 0; i < EVENT_LOOP_MAX_CLIENTS_NUM; i++) {
		client = &loop->clients[i];
		if (client->active && client->fd == fd) {
			LOG_ERR("The fd(%d) is already in use", fd);
			return NULL;
		}
	}

	for (i = 0; i < EVENT_LOOP_MAX_CLIENTS_NUM; i++) {
		client = &loop->clients[i];
		if (!client->active) {
			memset(client, 0, sizeof(event_loop_client_t));
			client->fd = fd;
			client->read_cb = read_cb;
			client->write_cb = write_cb;
			client->error_cb = error_cb;
			client->param = cb_param;
			client->active = 1;
			LOG_INFO("event_loop_add_client fd:%d\n", client->fd);
			return client;
		}
	}

	return NULL;
}

void event_loop_remove_client(event_loop_t *loop, int fd)
{
	int i;
	event_loop_client_t *client;

	for (i = 0; i < EVENT_LOOP_MAX_CLIENTS_NUM; i++) {
		client = &loop->clients[i];
		if (client->active && client->fd == fd) {
			LOG_INFO("event loop removed: %d\n", fd);
			memset(client, 0, sizeof(event_loop_client_t));
			break;
		}
	}
}

void event_loop_run(event_loop_t *loop)
{
	int result;
	int error_count;
#ifndef USE_POLL
	struct timeval tv;
#endif

	error_count = 0;
	while (1) {
		event_loop_init_fds(loop);
#ifdef USE_POLL
		result = poll(loop->fds, loop->num_fds, loop->timeout.interval);
#else
		tv.tv_sec = 0;
		tv.tv_usec = loop->timeout.interval * 1000;
		result = select(loop->max_fd + 1, &loop->read_fds, &loop->write_fds, &loop->error_fds, &tv);
#endif
		if (result > 0) {
			error_count = 0;
			event_loop_handle(loop);
		} else if (result == 0) {
			error_count = 0;
			event_loop_timeout(loop);
		} else {
			LOG_INFO("event_loop_run error: %d\n", result);
			event_loop_handle(loop);
			if (error_count++ > MAX_EVENT_LOOP_ERROR) {
				LOG_ERR("Too many errors on the event loop: %d\n", result);
				exit(1);
			}
		}
	}
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Initialises file descriptors to be polled
 *
 * @param loop The context of the event loop
 * @return Void
 */
static void event_loop_init_fds(event_loop_t *loop)
{
	int i;
	event_loop_client_t *client;

	loop->num_fds = 0;
#ifndef USE_POLL
	loop->max_fd = 0;
	FD_ZERO(&loop->read_fds);
	FD_ZERO(&loop->write_fds);
	FD_ZERO(&loop->error_fds);
#endif

	for (i = 0; i < EVENT_LOOP_MAX_CLIENTS_NUM; i++) {
		client = &loop->clients[i];
		if (client->active) {
#ifdef USE_POLL
			loop->fds[loop->num_fds].fd = client->fd;
			loop->fds[loop->num_fds].events = 0;
			if (client->read_cb) {
				loop->fds[loop->num_fds].events |= POLLIN;
			}
			if (client->write_cb) {
				loop->fds[loop->num_fds].events |= POLLOUT;
			}
			if (client->error_cb) {
				loop->fds[loop->num_fds].events |= POLLERR;
			}
#else
			if (client->read_cb) {
				FD_SET(client->fd, &loop->read_fds);
			}
			if (client->write_cb) {
				FD_SET(client->fd, &loop->write_fds);
			}
			if (client->error_cb) {
				FD_SET(client->fd, &loop->error_fds);
			}
			if (client->fd > loop->max_fd) {
				loop->max_fd = client->fd;
			}
#endif
			loop->fd_clients[loop->num_fds] = client;
			loop->num_fds++;
		}
	}
}

/**
 * @brief Called when the events occur and handles the registered actions
 *
 * @param loop The context of the event loop
 * @return Void
 */
static void event_loop_handle(event_loop_t *loop)
{
	int i;
	int read_handled;
	int write_handled;
	int error_handled;
	event_loop_client_t *client;

	read_handled = write_handled = error_handled = 0;
	for (i = 0; i < loop->num_fds; i++) {
		client = loop->fd_clients[i];
		if (!client) {
			LOG_INFO("unexpected client %d\r\n", i);
			continue;
		}
#ifdef USE_POLL
		if (loop->fds[i].revents & POLLIN) {
			if (client->read_cb) {
				client->read_cb(client->param);
				read_handled++;
			}
		}
		if (loop->fds[i].revents & POLLOUT) {
			if (client->write_cb) {
				client->write_cb(client->param);
				write_handled++;
			}
		}
		if (loop->fds[i].revents & POLLERR) {
			LOG_INFO("event_loop_handle POLLERR\r\n");
			if (client->error_cb) {
				client->error_cb(client->param);
				error_handled++;
			}
		}
#else
		if (FD_ISSET(client->fd, &loop->read_fds)) {
			if (client->read_cb) {
				client->read_cb(client->param);
				read_handled++;
			}
		}
		if (FD_ISSET(client->fd, &loop->write_fds)) {
			if (client->write_cb) {
				client->write_cb(client->param);
				write_handled++;
			}
		}
		if (FD_ISSET(client->fd, &loop->error_fds)) {
			if (client->error_cb) {
				client->error_cb(client->param);
				error_handled++;
			}
		}
#endif
	}

	if (loop->processed.cb) {
		loop->processed.cb(loop->processed.param);
	}

	LOG_DEBUG("Event(%p) R:%d,W:%d,E:%d\r\n", loop, read_handled, write_handled, error_handled);
}

/**
 * @brief Called when the timeout expires in the event loop
 *
 * @param loop The context of the event loop
 * @return Void
 */
static void event_loop_timeout(event_loop_t *loop)
{
	if (loop->timeout.cb) {
		loop->timeout.cb(loop->timeout.param);
	}
}

