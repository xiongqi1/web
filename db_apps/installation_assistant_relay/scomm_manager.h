#ifndef SCOMM_MANAGER_H_17135705012016
#define SCOMM_MANAGER_H_17135705012016
/**
 * @file scomm_manager.h
 * @brief Provides public functions and data structures to run scomm_manager
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

#define SERVICE_ID_MANAGEMENT 0
#define SERVICE_ID_SOCKET 1
#define SERVICE_ID_TEST 2

#define SERVICE_ID_MANAGEMENT_MASK (1 << SERVICE_ID_MANAGEMENT)
#define SERVICE_ID_SOCKET_MASK (1 << SERVICE_ID_SOCKET)
#define SERVICE_ID_TEST_MASK (1 << SERVICE_ID_TEST)

#define SCOMM_MANAGER_IDLE_LOOP_TIMEOUT_MS 30000 /* every 30 secs */
#define SCOMM_MANAGER_ACTIVE_LOOP_TIMEOUT_MS 200 /* every 200 millisecs */

/**
 * @brief Defines the type of sockets
 */
typedef enum {
	SOCK_TYPE_TCP = 0,
	SOCK_TYPE_UDP,
} sock_type_t;

/**
 * @brief Represents configurable options on the scomm_manager
 */
typedef struct scomm_manager_option {
	const char *device_name;
	const char *hostname;
	const char *serial_name;
	unsigned int baud_rate;
	unsigned int service_mask;
} scomm_manager_option_t;

/**
 * @brief Intialises the scomm_manager
 *
 * @return Void
 */
int scomm_manager_init(void);

/**
 * @brief Starts the scomm_manager
 *
 * @return 0 on success, or a negative value on error
 */
int scomm_manager_start(void);

/**
 * @brief Intialises the options before using them
 *
 * @param options The options to be initialised
 * @return Void
 */
void scomm_manager_init_options(scomm_manager_option_t *options);

/**
 * @brief Sets the options on the scomm_manager
 *
 * @param options The options to be set
 * @return Void
 */
void scomm_manager_set_options(scomm_manager_option_t *options);

/**
 * @brief Main routine on the scomm_manager
 *
 * @return Void
 */
void scomm_manager_main_run(void);

/**
 * @brief Called when Installation Assistant is attached or detached
 *
 * @param attached Indicates whether Installation Assistant is attached or not
 * @return Void
 */
void scomm_manager_device_attached(int attached);

/**
 * @brief Gets available service mask
 *
 * @return Returns the available service mask
 */
unsigned int scomm_manager_get_available_service_mask(void);

/**
 * @brief Changes serial speed to the specified baud rate
 *
 * @return 0 on success, or a negative value on error
 */
int scomm_manager_change_baud_rate(int baud_rate);

/**
 * @brief Changes the polling interval according to the specified mode.
 *
 * @param fast Indicates whether it's a fast polling or not.
 * @return Void
 */
void scomm_manager_change_polling_interval(int fast);

#endif
