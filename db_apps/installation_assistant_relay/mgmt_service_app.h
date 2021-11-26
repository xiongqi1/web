#ifndef MGMT_SERVICE_APP_H_16294610122015
#define MGMT_SERVICE_APP_H_16294610122015
/**
 * @file mgmt_service_app.h
 * @brief Provides public functions and data structures to work with the management service application.
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
#include "scomm_service.h"

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
 * @brief Creates the management service application
 *
 * @param manager The context of the scomm manager who creates it
 * @param service The context of the scomm service
 * @return Return the location to the context of the management service application
 *         on success, or NULL on error
 */
mgmt_service_app_t *mgmt_service_app_create(scomm_manager_t *manager, scomm_service_t *service);

/**
 * @brief Destorys the management service application
 *
 * @param app The context of the management service application
 * @return Void
 */
void mgmt_service_app_destroy(mgmt_service_app_t *app);

/**
 * @brief Starts the management service application
 *
 * @param app The context of the management service application
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_app_start(mgmt_service_app_t *app);

/**
 * @brief Stops the management service application
 *
 * @param app The context of the management service application
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_app_stop(mgmt_service_app_t *app);

/**
 * @brief Called when the periodic timer expires
 *
 * @param app The context of the management service application
 * @return Void
 */
void mgmt_service_app_timeout(mgmt_service_app_t *app);

/**
 * @brief Sets the device name on the management service application
 *
 * @param app The context of the management service application
 * @param name The device name to be set
 * @return Void
 */
void mgmt_service_app_set_device_name(mgmt_service_app_t *app, const char *name);

#endif
