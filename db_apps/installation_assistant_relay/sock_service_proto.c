/**
 * @file sock_service_proto.c
 * @brief Implements service that encodes/decodes scomm packets.
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
#include "sock_service_proto.h"
#include "sock_service_app.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

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

int sock_service_proto_init(sock_service_proto_t *proto)
{
	proto->signature = SOCK_SERVICE_APP_SIGNATURE;
	return 0;
}

void sock_service_proto_term(sock_service_proto_t *proto)
{
	proto->signature = 0;
}

int sock_service_proto_is_control_packet(sock_service_proto_t *proto, scomm_packet_t *packet)
{
	unsigned char *data;

	data = scomm_packet_get_begin_pointer(packet);

	if ((*data & SOCKET_MESSAGE_TYPE_BIT_MASK) == SOCKET_MESSAGE_TYPE_CONTROL) {
		return 1;
	} else {
		return 0;
	}
}

int sock_service_proto_decode_packet(sock_service_proto_t *proto, scomm_packet_t *packet,
		sock_service_pdu_t *pdu)
{
	unsigned char data;

	if (scomm_packet_remove_head_uint8(packet, &data)) {
		LOG_INFO("[SSP]Too short packet.1\r\n");
		return -1;
	}

	pdu->type = data & SOCKET_MESSAGE_TYPE_BIT_MASK;
	pdu->more_data = (data >> SOCKET_MESSAGE_MF_BIT_POS) & SOCKET_MESSAGE_MF_BIT_MASK;
	pdu->channel_type = (data >> SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS) & SOCKET_MESSAGE_CHANNEL_TYPE_BIT_MASK;
	pdu->channel_id = (data >> SOCKET_MESSAGE_CHANNEL_ID_BIT_POS) & SOCKET_MESSAGE_CHANNEL_ID_BIT_MASK;

	if (pdu->type == SOCKET_MESSAGE_TYPE_CONTROL) {
		unsigned short port;
		unsigned char num_packets;

		if (scomm_packet_remove_head_uint8(packet, &data)) {
			LOG_INFO("[SSP]Too short packet.2\r\n");
			return -1;
		}
		pdu->u.control.command_type = data & SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_BIT_MASK;
		pdu->u.control.command_id = (data & SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_MASK) >>
			SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS;

		switch (pdu->u.control.command_id) {
		case SOCKET_MESSAGE_CONTROL_COMMAND_ID_OPEN_SOCKET:
			if (scomm_packet_remove_head_uint16(packet, &port)) {
				LOG_INFO("[SSP]Too short packet.3\r\n");
				return -1;
			}
			pdu->u.control.u.open_socket.port = ntohs(port);
			break;

		case SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL:
			if (scomm_packet_remove_head_uint8(packet, &num_packets)) {
				LOG_INFO("[SSP]Too short packet.4\r\n");
				return -1;
			}
			pdu->u.control.u.buffer_avail.num_packets = num_packets;
			break;

		default:
			break;
		}
		LOG_DEBUG("CTRL T:%d, CT:%d, CID:%d\r\n", pdu->type, pdu->channel_type, pdu->channel_id);
	} else {
		pdu->u.data.payload = scomm_packet_get_begin_pointer(packet);
		pdu->u.data.len = scomm_packet_get_data_length(packet);
		LOG_DEBUG("DATA T:%d, CT:%d, CID:%d, LEN:%d\r\n",
				pdu->type, pdu->channel_type, pdu->channel_id, pdu->u.data.len);
	}
	return 0;
}

int sock_service_proto_encode_data_packet(sock_service_proto_t *proto, scomm_packet_t *packet,
		sock_type_t type, unsigned char id, unsigned char *data, int len)
{
	char header;

	header = (SOCKET_MESSAGE_TYPE_DATA << SOCKET_MESSAGE_TYPE_BIT_POS) |
		(id << SOCKET_MESSAGE_CHANNEL_ID_BIT_POS);
	if (type == SOCK_TYPE_TCP) {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_TCP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	} else {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_UDP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	}

	scomm_packet_append_tail_uint8(packet, header);
	scomm_packet_append_tail_bytes(packet, data, len);

	return 0;
}

int sock_service_proto_encode_close_socket_indication(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type)
{
	unsigned char header;
	unsigned char command;

	header = (SOCKET_MESSAGE_TYPE_CONTROL << SOCKET_MESSAGE_TYPE_BIT_POS) |
		(id << SOCKET_MESSAGE_CHANNEL_ID_BIT_POS);
	if (type == SOCK_TYPE_TCP) {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_TCP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	} else {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_UDP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	}
	scomm_packet_append_tail_uint8(packet, header);

	command = SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND |
		(SOCKET_MESSAGE_CONTROL_COMMAND_ID_CLOSE_SOCKET << SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS);

	scomm_packet_append_tail_uint8(packet, command);

	return 0;
}

int sock_service_proto_encode_open_socket_response(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type, unsigned short port)
{
	unsigned char header;
	unsigned char command;

	header = (SOCKET_MESSAGE_TYPE_CONTROL << SOCKET_MESSAGE_TYPE_BIT_POS) |
		(id << SOCKET_MESSAGE_CHANNEL_ID_BIT_POS);
	if (type == SOCK_TYPE_TCP) {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_TCP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	} else {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_UDP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	}
	scomm_packet_append_tail_uint8(packet, header);

	command = SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_RESP |
		(SOCKET_MESSAGE_CONTROL_COMMAND_ID_OPEN_SOCKET << SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS);

	scomm_packet_append_tail_uint8(packet, command);
	scomm_packet_append_tail_uint16(packet, htons(port));

	return 0;
}

int sock_service_proto_encode_close_socket_response(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type)
{
	unsigned char header;
	unsigned char command;

	header = (SOCKET_MESSAGE_TYPE_CONTROL << SOCKET_MESSAGE_TYPE_BIT_POS) |
		(id << SOCKET_MESSAGE_CHANNEL_ID_BIT_POS);
	if (type == SOCK_TYPE_TCP) {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_TCP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	} else {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_UDP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	}
	scomm_packet_append_tail_uint8(packet, header);

	command = SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_RESP |
		(SOCKET_MESSAGE_CONTROL_COMMAND_ID_CLOSE_SOCKET << SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS);

	scomm_packet_append_tail_uint8(packet, command);

	return 0;
}

int sock_service_proto_encode_buffer_avail_indication(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type, unsigned char num_packets)
{
	unsigned char header;
	unsigned char command;

	header = (SOCKET_MESSAGE_TYPE_CONTROL << SOCKET_MESSAGE_TYPE_BIT_POS) |
		(id << SOCKET_MESSAGE_CHANNEL_ID_BIT_POS);
	if (type == SOCK_TYPE_TCP) {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_TCP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	} else {
		header |= (SOCKET_MESSAGE_CHANNEL_TYPE_UDP << SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS);
	}
	scomm_packet_append_tail_uint8(packet, header);

	command = SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND |
		(SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL << SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS);

	scomm_packet_append_tail_uint8(packet, command);
	scomm_packet_append_tail_uint8(packet, num_packets);

	return 0;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

