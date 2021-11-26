/**
 * @file scomm_datalink.c
 * @brief Implements the data layer which is in charge of detecting data error and encoding
 *        Decoding the service id
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
#include "scomm_frame.h"

#include "crc.h"
#include "logger.h"

#include <arpa/inet.h>

#include <stdio.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define SCOMM_DATALINK_PROTO_VERSION_POS 0
#define SCOMM_DATALINK_PROTO_VERSION_MASK 0x7
#define SCOMM_DATALINK_PROTO_VERSION 1

#define SCOMM_DATALINK_PROTO_SERVICE_ID_POS 3
#define SCOMM_DATALINK_PROTO_SERVICE_ID_MASK 0x1f

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

int scomm_datalink_init(scomm_datalink_t *datalink, scomm_service_t *service,
						const scomm_driver_t *driver)
{
	datalink->service = service;
	if (scomm_frame_init(&datalink->frame, datalink, driver) < 0) {
		return -1;
	}
	return 0;
}

void scomm_datalink_term(scomm_datalink_t *datalink)
{
	scomm_frame_term(&datalink->frame);
}

int scomm_datalink_start(scomm_datalink_t *datalink)
{
	if (scomm_frame_start(&datalink->frame) < 0) {
		return -1;
	}
	return 0;
}

int scomm_datalink_stop(scomm_datalink_t *datalink)
{
	if (scomm_frame_stop(&datalink->frame) < 0) {
		return -1;
	}
	return 0;
}

int scomm_datalink_data_received(scomm_datalink_t *datalink, scomm_packet_t *packet)
{
	int id;
	unsigned char dl_header;
	unsigned short recv_crc;
	unsigned short calc_crc;

	if (scomm_packet_remove_tail_uint16(packet, &recv_crc) < 0) {
		LOG_ERR("scomm_datalink_data_received too short 1\r\n");
		goto out;
	}

	recv_crc = ntohs(recv_crc);
	calc_crc = crc16_calculate(scomm_packet_get_begin_pointer(packet),
								scomm_packet_get_data_length(packet));

	if (recv_crc != calc_crc) {
		scomm_packet_print(packet);
		LOG_ERR("crc mismatch %d, %d\r\n", recv_crc, calc_crc);
		goto out;
	}

	if (scomm_packet_remove_head_uint8(packet, &dl_header) < 0) {
		LOG_ERR("scomm_datalink_data_received too short 2\r\n");
		goto out;
	}

	if ((dl_header & SCOMM_DATALINK_PROTO_VERSION_MASK) != 1) {
		LOG_ERR("scomm_datalink_data_received too short 3\r\n");
		goto out;
	}

	id = (dl_header >> SCOMM_DATALINK_PROTO_SERVICE_ID_POS) & SCOMM_DATALINK_PROTO_VERSION_MASK;
	//scomm_packet_print(packet);
	return scomm_service_data_received(datalink->service, id, packet);

out:
	LOG_ERR("scomm_datalink_data_received error\r\n");
	/* drop the packet if it is not valid */
	scomm_packet_destroy(packet);
	return -1;
}

/**
 * @brief Encodes data link information on the packet and deliver it to the frame layer.
 *
 * @param datalink The pointer to the context of the data link layer
 * @param id The service id
 * @param packet The pointer to the packet where data will be written.
 * @return The number of bytes to be written
 */
int scomm_datalink_send_data(scomm_datalink_t *datalink, int id, scomm_packet_t *packet)
{
	int result;
	unsigned char dl_header;
	unsigned short crc;

	dl_header = (id << SCOMM_DATALINK_PROTO_SERVICE_ID_POS) | SCOMM_DATALINK_PROTO_VERSION;
	scomm_packet_append_head_uint8(packet, dl_header);
	crc = crc16_calculate(scomm_packet_get_begin_pointer(packet),
						scomm_packet_get_data_length(packet));

	crc = htons(crc);
	scomm_packet_append_tail_uint16(packet, crc);
	//scomm_packet_print(packet);
	result = scomm_frame_send_data(&datalink->frame, packet);

	LOG_INFO("scomm_datalink_send_data len:%d\r\n", scomm_packet_get_data_length(packet));
	return (result > 3) ? (result - 3) : -1;
}

