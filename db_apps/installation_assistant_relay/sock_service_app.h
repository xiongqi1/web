#ifndef SOCK_SERVICE_APP_H_15183330112015
#define SOCK_SERVICE_APP_H_15183330112015
/**
 * @file sock_service_app.h
 * @brief Provides public functions and data structures to work with the sock service application.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM DETECTIONLESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM DETECTIONLESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include "app_type.h"
#include "sock_service_proto.h"


/*******************************************************************************
 * Define public macros
 ******************************************************************************/

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

/**
 * @brief Creates an instance of the sock_service application
 *
 * @param manager The device manager who owns the sock_service application
 * @param service The context of the scomm service
 * @return 0 on success, or a negative value on error
 */
sock_service_app_t *sock_service_app_create(scomm_manager_t *manager, scomm_service_t *service);

/**
 * @brief Destorys an instance of the sock_service application
 *
 * @param app The instance of the sock service application
 * @return 0 on success, or a negative value on error
 */
void sock_service_app_destroy(sock_service_app_t *app);

/**
 * @brief Initialises the sock service application
 *
 * @param app The instance of the sock service application
 * @return 0 on success, or a negative value on error
 */
int sock_service_app_init(sock_service_app_t *app, scomm_manager_t *manager);

/**
 * @brief Starts the sock service application
 *
 * @param app The instance of the sock service application
 * @return 0 on success, or a negative value on error
 */
int sock_service_app_start(sock_service_app_t *app);

/**
 * @brief Stops the sock service application
 *
 * @param app The instance of the sock service application
 * @return 0 on success, or a negative value on error
 */
int sock_service_app_stop(sock_service_app_t *app);

/**
 * @brief Sets the device name on the sock service application
 *
 * @param app The instance of the sock service application
 * @param name The hostname to be set
 * @return Void
 */
void sock_service_app_set_hostname(sock_service_app_t *app, const char *name);

/**
 * @brief Runs the sock service application on the event loop
 *
 * @param app The instance of the sock service application
 * @return Void
 */
void sock_service_app_run(sock_service_app_t *app);

#endif
