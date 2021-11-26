#ifndef SCOMM_SERVICE_H_10234801122015
#define SCOMM_SERVICE_H_10234801122015
/*
 * @file scomm_service.h
 * @brief Provides public interfaces to use the scomm service
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
#include "scomm.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Allocates a scomm service instance
 *
 * @return The instance of the scomm service on success, or NULL on error
 */
scomm_service_t *scomm_service_alloc(void);

/**
 * @brief Deallocates the scomm service instance
 *
 * @param service The service to be deallocated
 * @return Void
 */
void scomm_service_free(scomm_service_t *service);

/**
 * @brief Initialises the scomm service
 *
 * @param service The service to be initialised
 * @return 0 on success, or a negative value on error
 */
int scomm_service_init(scomm_service_t *service, const scomm_driver_t *driver);

/**
 * @brief Terminates the scomm service
 *
 * @param service The service to be terminated
 * @return Void
 */
void scomm_service_term(scomm_service_t *service);

/**
 * @brief Starts the scomm service
 *
 * @param service The service to be started
 * @return 0 on success, or a negative value on error
 */
int scomm_service_start(scomm_service_t *service);

/**
 * @brief Stops the scomm service
 *
 * @param service The service to be stopped
 * @return 0 on success, or a negative value on error
 */
int scomm_service_stop(scomm_service_t *service);

/**
 * @brief Adds the client to the scomm service
 *
 * @param service The service where the client is added
 * @param client The client to be added
 * @return 0 on success, or a negative value on error
 */
int scomm_service_add_client(scomm_service_t *service, const scomm_service_client_t *client);

/**
 * @brief Removes the client from the scomm service
 *
 * @param service The service where the client is removed
 * @param client The client to be removed
 * @return 0 on success, or a negative value on error
 */
int scomm_service_remove_client(scomm_service_t *service, const scomm_service_client_t *client);

/**
 * @brief Called when data comes from the lower(data link) layer
 *
 * @param service The service where data is received
 * @param id The service id to distinguish the client
 * @return The number of data in bytes on success, or a negative value on error
 */
int scomm_service_data_received(scomm_service_t *service, int id, scomm_packet_t *packet);

/**
 * @brief Sends the packet through the scomm service by the client
 *
 * @param service The service where data is delivered
 * @param client The client who wants to send the packet
 * @return The number of data in bytes on success, or a negative value on error
 */
int scomm_service_send_data(scomm_service_t *service, const scomm_service_client_t *client,
                        scomm_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
