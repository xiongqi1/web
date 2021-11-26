#ifndef TEST_SERVICE_APP_H_13203922062016
#define TEST_SERVICE_APP_H_13203922062016
/**
 * @file test_service_app.h
 * @brief Provides public functions and data structures to work with the test service application.
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
 * @brief Creates an instance of the test_service application
 *
 * @param manager The device manager who owns the test_service application
 * @param service The context of the scomm service
 * @return 0 on success, or a negative value on error
 */
test_service_app_t *test_service_app_create(scomm_manager_t *manager, scomm_service_t *service);

/**
 * @brief Destroys an instance of the test service application
 *
 * @param app The instance of the test service application
 * @return 0 on success, or a negative value on error
 */
void test_service_app_destroy(test_service_app_t *app);

/**
 * @brief Starts the test service application
 *
 * @param app The instance of the test service application
 * @return 0 on success, or a negative value on error
 */
int test_service_app_start(test_service_app_t *app);

/**
 * @brief Stops the test service application
 *
 * @param app The instance of the test service application
 * @return 0 on success, or a negative value on error
 */
int test_service_app_stop(test_service_app_t *app);

/**
 * @brief Runs the test service application on the event loop
 *
 * @param app The instance of the test service application
 * @return Void
 */
void test_service_app_run(test_service_app_t *app);

#endif
