#ifndef SERIAL_CHANNEL_H_17260810122015
#define SERIAL_CHANNEL_H_17260810122015
/**
 * @file serial_channel.h
 * @brief Provides public functions and data structures to use serial channels
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
#include "app_type.h"

/*******************************************************************************
 * Define public macros
 ******************************************************************************/

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

typedef struct serial_channel serial_channel_t;

typedef struct serial_channel_option {
	const char *name;
	unsigned int baud_rate;
} serial_channel_option_t;

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

/**
 * @brief Creates a serial channel
 *
 * @return The instance of the serial channel or NULL on error
 */
serial_channel_t *serial_channel_create(void);

/**
 * @brief Destroys the serial channel
 *
 * @param channel The instance of the serial channel
 * @return Void
 */
void serial_channel_destroy(serial_channel_t *channel);

/**
 * @brief Initialises the options before using them
 *
 * @param option The option to be initialised
 * @return Void
 */
void serial_channel_init_options(serial_channel_option_t *options);

/**
 * @brief Sets the options to configure the serial channel
 *
 * @param channel The instance of the serial channel
 * @param option The options to be set
 * @return Void
 */
void serial_channel_set_options(serial_channel_t *channel, serial_channel_option_t *options);

/**
 * @brief Sets the specified baud rate on the serial channel
 *
 * @param channel The instance of the serial channel
 * @param baud_rate The baud rate to be set
 * @return Void
 */
void serial_channel_set_baud_rate(serial_channel_t *channel, unsigned int baud_rate);

/**
 * @brief Starts the serial channel
 *
 * @param channel The instance of the serial channel
 * @return 0 on success, or a negative value on error
 */
int serial_channel_start(serial_channel_t *channel);

/**
 * @brief Stops the serial channel
 *
 * @param channel The instance of the serial channel
 * @return 0 on success, or a negative value on error
 */
int serial_channel_stop(serial_channel_t *channel);

/**
 * @brief Writes data on the serial channel
 *
 * @param channel The instance of the serial channel
 * @param data The data to be written
 * @param len The number of writing data in bytes
 * @return The number of written data in bytes, or a negative value on error
 */
int serial_channel_write(serial_channel_t *channel, unsigned char *data, int len);

/**
 * @brief Reads data on the serial channel
 *
 * @param channel The instance of the serial channel
 * @param data The data to be written
 * @param len The number of reading data in bytes
 * @return The number of read data in bytes, or a negative value on error
 */
int serial_channel_read(serial_channel_t *channel, unsigned char *data, int len);

/**
 * @brief Gets the file descriptor that is associated with the serial channel
 *
 * @param channel The instance of the serial channel
 * @return The file descriptor that is associated with the serial channel
 */
int serial_channel_get_fd(serial_channel_t *channel);

#endif
