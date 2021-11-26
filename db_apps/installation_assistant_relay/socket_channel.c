/**
 * @file socket_channel.c
 * @brief Implements socket channel that provides mechanism to communicate processes through
 *        socket interface
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
#include "socket_channel.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define SOCKET_CHANNEL_DEFAULT_HOSTNAME "localhost"
#define SOCKET_CHANNEL_DEFAULT_PORT 80
#define SOCKET_CHANNEL_HOSTNAME_MAX_LEN 256
#define MAX_SOCKET_CHANNEL_WRITE_NUM 10
#define SOCKET_CHANNEL_WRITE_DELAY_MS 200

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/**
 * @brief The structure of the socket channel
 */
typedef struct socket_channel {
	int id;
	int fd;
	unsigned short port;
	char *hostname;
	void *owner;
} socket_channel_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int init_channel(socket_channel_t *channel, int id, void *owner);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

socket_channel_t *socket_channel_create(int id, void *owner)
{
	socket_channel_t *channel;

	channel = malloc(sizeof(socket_channel_t));
	if (channel) {
		memset(channel, 0, sizeof(socket_channel_t));
		init_channel(channel, id, owner);
	}

	return channel;
}

void socket_channel_destroy(socket_channel_t *channel)
{
	if (channel) {
		if (channel->hostname) {
			free(channel->hostname);
		}
		free(channel);
	}
}

void socket_channel_init_options(socket_channel_option_t *options)
{
	options->hostname = NULL;
	options->port = 0;
}

void socket_channel_set_options(socket_channel_t *channel, socket_channel_option_t *options)
{
	if (options->hostname) {
		channel->hostname = malloc(strlen(options->hostname) + 1);
		if (channel->hostname) {
			strcpy(channel->hostname, options->hostname);
		}
	}

	if (options->port) {
		channel->port = options->port;
	}

	LOG_INFO("[socket config] name:%s, port:%d\n", channel->hostname, channel->port);
}

int socket_channel_start(socket_channel_t *channel)
{
	struct sockaddr_in serveraddr;
	struct hostent *server;

	/* socket: create the socket */
	channel->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (channel->fd < 0) {
		LOG_ERR("ERROR opening socket\n");
		goto out;
	}

	/* gethostbyname: get the server's DNS entry */
	server = gethostbyname(channel->hostname);
	if (server == NULL) {
		LOG_ERR("ERROR, no such host as %s\n", channel->hostname);
		goto close_socket;
	}

	/* build the server's Internet address */
	memset((char *) &serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy((char *)&serveraddr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
	serveraddr.sin_port = htons(channel->port);

	/* connect: create a connection with the server */
	if (connect(channel->fd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		LOG_ERR("ERROR connecting\n");
		goto close_socket;
	}

	LOG_INFO("socket_channel_start: %d\n", channel->fd);
	return 0;

close_socket:
	close(channel->fd);
	channel->fd = -1;
out:
	return -1;
}

int socket_channel_stop(socket_channel_t *channel)
{
	LOG_NOTICE("socket_channel_stop socket: %d\n", channel->fd);
	if (channel->fd >= 0) {
		close(channel->fd);
		channel->fd = -1;
	}
	channel->id = -1;
	return 0;
}

int socket_channel_write(socket_channel_t *channel, unsigned char *data, int len)
{
	int result;
	int error_count;
	int sent_len;
	int remaining_len;

	error_count = 0;
	sent_len = 0;
	remaining_len = len;
	while (sent_len < len) {
		result = write(channel->fd, &data[sent_len], remaining_len);
		if (result > 0) {
			sent_len += result;
			remaining_len -= result;
			error_count = 0;
		} else {
			error_count++;
			if (error_count > MAX_SOCKET_CHANNEL_WRITE_NUM) {
				break;
			}
			usleep(SOCKET_CHANNEL_WRITE_DELAY_MS);
		}
	}
	return sent_len;
}

int socket_channel_read(socket_channel_t *channel, unsigned char *data, int len)
{
	int result;

	if (channel->fd < 0) {
		return -1;
	}

	result = read(channel->fd, data, len);
	LOG_INFO("socket_channel_read(fd:%d) len:%d, Result:%d\n", channel->fd, len, result);
	return result;
}

int socket_channel_get_fd(socket_channel_t *channel)
{
	return channel->fd;
}

unsigned short socket_channel_get_port(socket_channel_t *channel)
{
	return channel->port;
}

void *socket_channel_get_owner(socket_channel_t *channel)
{
	return channel->owner;
}

void socket_channel_set_id(socket_channel_t *channel, int id)
{
	channel->id = id;
}

int socket_channel_get_id(socket_channel_t *channel)
{
	return channel->id;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Initialises interval variables on the channel
 *
 * @return 0 on sucess, or a negative value on error
 */
static int init_channel(socket_channel_t *channel, int id, void *owner)
{
	channel->hostname = SOCKET_CHANNEL_DEFAULT_HOSTNAME;
	channel->port = SOCKET_CHANNEL_DEFAULT_PORT;
	channel->id = id;
	channel->owner = owner;
	channel->fd = -1;

	return 0;
}

