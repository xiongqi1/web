/**
 * @file pipe_channel.c
 * @brief Implements pipe channel that provides mechanism to communicate processes through
 *        pipe interface
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
#include "pipe_channel.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/**
 * @brief The structure of the pipe channel
 */
typedef struct pipe_channel {
	int fds[2];
} pipe_channel_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

pipe_channel_t *pipe_channel_create(void)
{
	pipe_channel_t *channel;

	channel = malloc(sizeof(pipe_channel_t));
	if (channel) {
		memset(channel, 0, sizeof(pipe_channel_t));
		channel->fds[0] = channel->fds[1] = -1;
	}

	return channel;
}

void pipe_channel_destroy(pipe_channel_t *channel)
{
	if (channel) {
		free(channel);
	}
}

int pipe_channel_start(pipe_channel_t *channel)
{
	if (pipe(channel->fds)) {
		LOG_ERR("Fails to create pipe\n");
		return -1;
	}

	if ((channel->fds[0] < 0) || (channel->fds[1] < 0)) {
		LOG_ERR("Invalid pipe file descriptors\n");
		return -1;
	}

	/* Non-blocking read */
	fcntl(channel->fds[0], F_SETFL, fcntl(channel->fds[0], F_GETFL) | O_NONBLOCK);

	LOG_NOTICE("pipe_channel_start(R %d, W %d)\n", channel->fds[0], channel->fds[1]);
	return 0;
}

int pipe_channel_stop(pipe_channel_t *channel)
{
	LOG_NOTICE("pipe_channel_stop(R %d, W %d)\n", channel->fds[0], channel->fds[1]);

	if (channel->fds[0] >= 0) {
		close(channel->fds[0]);
		channel->fds[0] = -1;
	}
	if (channel->fds[1] >= 0) {
		close(channel->fds[1]);
		channel->fds[1] = -1;
	}

	return 0;
}

int pipe_channel_write(pipe_channel_t *channel, void *data, int len)
{
	if (channel->fds[1] < 0) {
		return -1;
	}

	return write(channel->fds[1], data, len);
}

int pipe_channel_read(pipe_channel_t *channel, void *data, int len)
{
	if (channel->fds[0] < 0) {
		return -1;
	}

	return read(channel->fds[0], data, len);
}

int pipe_channel_get_read_fd(pipe_channel_t *channel)
{
	return channel->fds[0];
}

int pipe_channel_get_write_fd(pipe_channel_t *channel)
{
	return channel->fds[1];
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

