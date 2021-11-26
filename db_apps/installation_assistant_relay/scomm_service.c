/**
 * @file scomm_service.c
 * @brief Implements service layer
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
#include "scomm_service.h"
#include "scomm_datalink.h"

#include "logger.h"

#include "oss.h"

#include <string.h>
#include <stdlib.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define WRITE_LOCK_TIMEOUT_MS 1000

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

struct scomm_service {
	oss_lock_t *lock;
	scomm_datalink_t datalink;
	const scomm_service_client_t *clients[MAX_SCOMM_SERVICE_NUM];
};

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

scomm_service_t *scomm_service_alloc(void)
{
	scomm_service_t *service;

	service = malloc(sizeof(scomm_service_t));
	if (service) {
		memset(service, 0, sizeof(scomm_service_t));
	}

	return service;
}

void scomm_service_free(scomm_service_t *service)
{
	if (service) {
		free(service);
	}
}

int scomm_service_init(scomm_service_t *service, const scomm_driver_t *driver)
{
	service->lock = oss_lock_create();
	if (!service->lock) {
		LOG_ERR("Creating a lock fails in scomm_service\n");
		return -1;
	}
	scomm_datalink_init(&service->datalink, service, driver);
	return 0;
}

void scomm_service_term(scomm_service_t *service)
{
	scomm_datalink_term(&service->datalink);
	if (service->lock) {
		oss_lock_destroy(service->lock);
	}
}

int scomm_service_start(scomm_service_t *service)
{
	scomm_datalink_start(&service->datalink);
	return 0;
}

int scomm_service_stop(scomm_service_t *service)
{
	scomm_datalink_stop(&service->datalink);
	return 0;
}

int scomm_service_add_client(scomm_service_t *service, const scomm_service_client_t *client)
{
	if (client->service_id >= MAX_SCOMM_SERVICE_NUM) {
		return -1;
	}
	service->clients[client->service_id] = client;
	return 0;
}

int scomm_service_remove_client(scomm_service_t *service, const scomm_service_client_t *client)
{
	if (client->service_id >= MAX_SCOMM_SERVICE_NUM) {
		return -1;
	}
	service->clients[client->service_id] = NULL;
	return 0;
}

int scomm_service_data_received(scomm_service_t *service, int id, scomm_packet_t *packet)
{
	if ((id >= MAX_SCOMM_SERVICE_NUM) || (id < 0)) {
		return -1;
	}

	if (service->clients[id] && service->clients[id]->data_received_cb) {
		return service->clients[id]->data_received_cb(packet, service->clients[id]->owner);
	}
	LOG_INFO("scomm_service_data_received error(%d)\r\n", id);
	return -1;
}

int scomm_service_send_data(scomm_service_t *service, const scomm_service_client_t *client,
						scomm_packet_t *packet)
{
	int result;
	
	oss_lock(service->lock, WRITE_LOCK_TIMEOUT_MS);
	result = scomm_datalink_send_data(&service->datalink, client->service_id, packet);
	oss_unlock(service->lock);

	return result;
}

