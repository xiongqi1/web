#ifndef SOCKET_CHANNEL_H_12134817122015
#define SOCKET_CHANNEL_H_12134817122015
/**
 * @file socket_channel.h
 * @brief Provides public functions and data structures to use socket channels
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

typedef struct socket_channel socket_channel_t;

/**
 * @brief The structure of the options on the channel
 */
typedef struct socket_channel_option {
	char *hostname;
	unsigned short port;
} socket_channel_option_t;

/**
 * @brief Creates an instance of the socket channel
 *
 * @return The instance of the socket channel on success, or NULL on error
 */
socket_channel_t *socket_channel_create(int id, void *owner);

/**
 * @brief Destorys the instance of the socket channel
 *
 * @return Void
 */
void socket_channel_destroy(socket_channel_t *channel);

/**
 * @brief Initialises the options before setting them to the channel
 *
 * @return Void
 */
void socket_channel_init_options(socket_channel_option_t *options);

/**
 * @brief Sets the options on the channel
 *
 * @param channel The instance of the socket channel
 * @param options The options to be set
 * @return Void
 */
void socket_channel_set_options(socket_channel_t *channel, socket_channel_option_t *options);

/**
 * @brief Starts the socket channel
 *
 * @param channel The instance of the socket channel
 * @return 0 on success, or a negative value on error
 */
int socket_channel_start(socket_channel_t *channel);

/**
 * @brief Stops the socket channel
 *
 * @param channel The instance of the socket channel
 * @return 0 on success, or a negative value on error
 */
int socket_channel_stop(socket_channel_t *channel);

/**
 * @brief Writes data to the socket channel
 *
 * @param channel The instance of the socket channel
 * @param data The location of the memory to be written
 * @param len The number of data in bytes
 * @return The length of data written, or a negative value on error
 */
int socket_channel_write(socket_channel_t *channel, unsigned char *data, int len);

/**
 * @brief Reads data the socket channel
 *
 * @param channel The instance of the socket channel
 * @param data The location of the memory to be read
 * @param len The number of data in bytes
 * @return The length of data read, or a negative value on error
 */
int socket_channel_read(socket_channel_t *channel, unsigned char *data, int len);

/**
 * @brief Gets the file descriptor on the channel
 *
 * @param channel The instance of the socket channel
 * @return The file descriptor or a negative value on error
 */
int socket_channel_get_fd(socket_channel_t *channel);

/**
 * @brief Gets the file descriptor on the channel
 *
 * @param channel The instance of the socket channel
 * @return The file descriptor or 0 on error
 */
unsigned short socket_channel_get_port(socket_channel_t *channel);

/**
 * @brief Gets the owner of the channel
 *
 * @param channel The instance of the socket channel
 * @return The owner of the channel or NULL on error
 */
void *socket_channel_get_owner(socket_channel_t *channel);

/**
 * @brief Sets the channel ID on the channel
 *
 * @param channel The instance of the socket channel
 * @param id The channel ID to be set
 * @return The owner of the channel or NULL on error
 */
void socket_channel_set_id(socket_channel_t *channel, int id);

/**
 * @brief Gets the ID of the channel
 *
 * @param channel The instance of the socket channel
 * @return The owner of the channel or NULL on error
 */
int socket_channel_get_id(socket_channel_t *channel);

#endif
