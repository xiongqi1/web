/**
 * @file serial_channel.c
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

#include "serial_channel.h"
#include "logger.h"

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <linux/serial.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define SERIAL_DEFAULT_DEVICE_NAME "/dev/ttyHSL0"
#define SERIAL_DEFAULT_BAUD_RATE 115200

/* Retry for 10 secs until the write gives up */
#define MAX_SERIAL_CHANNEL_WRITE_NUM 50
#define SERIAL_CHANNEL_WRITE_DELAY_US 20000

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

typedef struct serial_channel {
	int fd;
	char *name;
	unsigned int baud_rate;
	struct termios old_tio;
	struct termios new_tio;
} serial_channel_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int init_channel(serial_channel_t *channel);
static int get_tio_baud_rate(int baud_rate);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

serial_channel_t *serial_channel_create(void)
{
	serial_channel_t *channel;

	channel = malloc(sizeof(serial_channel_t));
	if (channel) {
		memset(channel, 0, sizeof(serial_channel_t));
		init_channel(channel);
	}

	return channel;
}

void serial_channel_destroy(serial_channel_t *channel)
{
	if (channel) {
		if (channel->name) {
			free(channel->name);
		}
		free(channel);
	}
}

unsigned int serial_channel_get_baud_rate(serial_channel_t *channel)
{
	return channel->baud_rate;
}

void serial_channel_set_baud_rate(serial_channel_t *channel, unsigned int baud_rate)
{
	channel->baud_rate = baud_rate;
}

void serial_channel_init_options(serial_channel_option_t *options)
{
	options->name = NULL;
	options->baud_rate = 0;
}

void serial_channel_set_options(serial_channel_t *channel, serial_channel_option_t *options)
{
	if (options->name) {
		if (channel->name) {
			free(channel->name);
		}
		channel->name = strdup(options->name);
	}

	if (options->baud_rate) {
		serial_channel_set_baud_rate(channel, options->baud_rate);
	}
	LOG_INFO("serial set_options name: %s, baud rate: %d\n", channel->name, channel->baud_rate);
}

int serial_channel_start(serial_channel_t *channel)
{
	unsigned int baud_rate;

	channel->fd = open(channel->name, O_RDWR | O_NONBLOCK);

	if (channel->fd < 0) {
		LOG_ERR("Error opening serial port \n");
		return -1;
	}

	memset(&channel->new_tio, 0, sizeof(struct termios)); /* clear struct for new port settings */

	/* man termios get more info on below settings */
	baud_rate = get_tio_baud_rate(channel->baud_rate);
	channel->new_tio.c_cflag = baud_rate | CS8 | CLOCAL | CREAD;

	channel->new_tio.c_iflag = 0;
	channel->new_tio.c_oflag = 0;
	channel->new_tio.c_lflag = 0;

	/* Non-blocking read */
	channel->new_tio.c_cc[VMIN] = 0;
	channel->new_tio.c_cc[VTIME] = 0;

	/* now clean the modem line and activate the settings for the port */
	tcgetattr(channel->fd, &channel->old_tio);
	tcflush(channel->fd, TCIOFLUSH);
	tcsetattr(channel->fd, TCSANOW, &channel->new_tio);

	LOG_INFO("serial start name: %s, baud rate: %d\n", channel->name, channel->baud_rate);
	return 0;
}

int serial_channel_stop(serial_channel_t *channel)
{
	if (channel->fd >= 0) {
		tcflush(channel->fd, TCIOFLUSH);
		tcsetattr(channel->fd, TCSANOW, &channel->old_tio);
		close(channel->fd);
	}

	return 0;
}

int serial_channel_write(serial_channel_t *channel, unsigned char *data, int len)
{
	int result;
	int sent_len;
	int remaining_len;
	int error_count;

	sent_len = 0;
	error_count = 0;
	remaining_len = len;
	LOG_MUTE("[TX]");
	while (sent_len < len) {
		LOG_MUTE("%02x:", data[sent_len]);
		result = write(channel->fd, &data[sent_len], remaining_len);
		if (result > 0) {
			sent_len += result;
			remaining_len -= result;
		} else {
			error_count++;
			if (error_count > MAX_SERIAL_CHANNEL_WRITE_NUM) {
				LOG_ERR("[SC]write fails trying: %d, written: %d\n", len, remaining_len);
				break;
			}
			usleep(SERIAL_CHANNEL_WRITE_DELAY_US);
			LOG_INFO("[SC]write fails R: %d, L: %d\n", result, remaining_len);
		}
	}
	LOG_MUTE("\n");
	return sent_len;
}

int serial_channel_read(serial_channel_t *channel, unsigned char *data, int len)
{
	int result;

    result = read(channel->fd, data, len);
	LOG_MUTE("R:%d, L:%d, D:%02x\n", result, len, data[0]);
	return result;
}

int serial_channel_get_fd(serial_channel_t *channel)
{
	return channel->fd;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Initialises internal variables on the channel
 *
 * @param channel The instance of the serial channel
 * @return 0 on success, or a negative value on error
 */
static int init_channel(serial_channel_t *channel)
{
	channel->fd = -1;
	channel->name = strdup(SERIAL_DEFAULT_DEVICE_NAME);
	channel->baud_rate = SERIAL_DEFAULT_BAUD_RATE;
	LOG_INFO_AT();
	return 0;
}

/**
 * @brief Gets the value of the termio for the given baud_rate
 *
 * @param baud_rate The baud rate to be converted
 * @return The converted value for the baud_rate
 */
static int get_tio_baud_rate(int baud_rate)
{
	switch (baud_rate) {
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 500000:
		return B500000;
	case 576000:
		return B576000;
	case 921600:
		return B921600;
	case 1000000:
		return B1000000;
	case 1152000:
		return B1152000;
	case 1500000:
		return B1500000;
	case 2000000:
		return B2000000;
	case 2500000:
		return B2500000;
	case 3000000:
		return B3000000;
	case 3500000:
		return B3500000;
	case 4000000:
		return B4000000;
	default:
		return -1;
	}
}

