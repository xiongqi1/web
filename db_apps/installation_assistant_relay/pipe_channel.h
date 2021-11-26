#ifndef PIPE_CHANNEL_H_15204610082016
#define PIPE_CHANNEL_H_15204610082016
/**
 * @file pipe_channel.h
 * @brief Provides public functions and data structures to use pipe channels
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

#include "app_type.h"

typedef struct pipe_channel pipe_channel_t;

/**
 * @brief Creates an instance of the pipe channel
 *
 * @return The instance of the pipe channel on success, or NULL on error
 */
pipe_channel_t *pipe_channel_create(void);

/**
 * @brief Destorys the instance of the pipe channel
 *
 * @return Void
 */
void pipe_channel_destroy(pipe_channel_t *channel);

/**
 * @brief Starts the pipe channel
 *
 * @param channel The instance of the pipe channel
 * @return 0 on success, or a negative value on error
 */
int pipe_channel_start(pipe_channel_t *channel);

/**
 * @brief Stops the pipe channel
 *
 * @param channel The instance of the pipe channel
 * @return 0 on success, or a negative value on error
 */
int pipe_channel_stop(pipe_channel_t *channel);

/**
 * @brief Writes data to the pipe channel
 *
 * @param channel The instance of the pipe channel
 * @param data The location of the memory to be written
 * @param len The number of data in bytes
 * @return The length of data written, or a negative value on error
 */
int pipe_channel_write(pipe_channel_t *channel, void *data, int len);

/**
 * @brief Reads data the pipe channel
 *
 * @param channel The instance of the pipe channel
 * @param data The location of the memory to be read
 * @param len The number of data in bytes
 * @return The length of data read, or a negative value on error
 */
int pipe_channel_read(pipe_channel_t *channel, void *data, int len);

/**
 * @brief Gets the read file descriptor on the channel
 *
 * @param channel The instance of the pipe channel
 * @return The file descriptor or a negative value on error
 */
int pipe_channel_get_read_fd(pipe_channel_t *channel);

/**
 * @brief Gets the write file descriptor on the channel
 *
 * @param channel The instance of the pipe channel
 * @return The file descriptor or a negative value on error
 */
int pipe_channel_get_write_fd(pipe_channel_t *channel);

#endif
