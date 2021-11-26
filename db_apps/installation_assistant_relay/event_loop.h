#ifndef EVENT_LOOP_H_16152010122015
#define EVENT_LOOP_H_16152010122015
/**
 * @file event_loop.h
 * @brief Provides public functions and data structures to use event loop
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

/*******************************************************************************
 * Define public macros
 ******************************************************************************/

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

typedef struct event_loop_client event_loop_client_t;
typedef struct event_loop event_loop_t;

typedef void (*event_loop_cb_t)(void *param);

typedef struct event_loop_option {
	int timeout_interval;
	void *timeout_param;
	event_loop_cb_t timeout_cb;

	void *processed_param;
	event_loop_cb_t processed_cb;
} event_loop_option_t;

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

/**
 * @brief Creates an instance of the event loop
 *
 * @return The instance of the event loop
 */
event_loop_t *event_loop_create(void);

/**
 * @brief Destroys the instance of the event loop
 *
 * @param loop The instance of the event loop
 * @return Void
 */
void event_loop_destroy(event_loop_t *loop);

/**
 * @brief Initialise the event loop
 *
 * @param loop The instance of the event loop
 * @return 0 on success, or a negative value on error
 */
int event_loop_init(event_loop_t *loop);

/**
 * @brief Intialises the options for the event loop
 *
 * @param options The options to be initialised
 * @return Void
 */
void event_loop_init_options(event_loop_option_t *options);

/**
 * @brief Sets the options for the event loop
 *
 * @param loop The instance of the event loop
 * @param options The options to be set
 * @return Void
 */
void event_loop_set_options(event_loop_t *loop, event_loop_option_t *options);

/**
 * @brief Sets the timeout interval for the event loop
 *
 * @param loop The instance of the event loop
 * @param interval Indicates how often the timer expires
 * @return Void
 */
void event_loop_set_timeout_interval(event_loop_t *loop, int interval);

/**
 * @brief Runs the event loop to wait for events and executes the actions
 *
 * @param loop The instance of the event loop
 * @return Void
 */
void event_loop_run(event_loop_t *loop);

/**
 * @brief Adds the client into the event loop
 *
 * @param loop The instance of the event loop
 * @param read_cb The function pointer to be called when data is ready to be read
 * @param write_cb The function pointer to be called when data write is done
 * @param error_cb The function pointer to be called when an error occurs
 * @param param The argument when the function is called back
 * @return The instance of the client of the event loop on sucess, or
 *         NULL on error
 */
event_loop_client_t *
event_loop_add_client(event_loop_t *loop, int fd, event_loop_cb_t read_cb,
		event_loop_cb_t write_cb, event_loop_cb_t error_cb, void *cb_param);

/**
 * @brief Removes the client from the event loop
 *
 * @param loop The instance of the event loop
 * @param fd The file descriptor which is associated with the client
 * @return Void
 */
void event_loop_remove_client(event_loop_t *loop, int fd);

#endif
