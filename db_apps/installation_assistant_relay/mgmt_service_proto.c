/**
 * @file mgmt_service_proto.c
 * @brief Implements helper functions that encodes/decodes management service packets.
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

#include "mgmt_service_proto.h"
#include "mgmt_service_app.h"
#include "util.h"
#include "logger.h"

#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/
static int mgmt_service_proto_decode_hello_result(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_hello_services(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_hello_name(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_hello_sw_version(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_hello_hw_version(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_hello_baud_rates(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_hello(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_battery_result(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_battery_level(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_battery_status(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_battery(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_speed_result(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_speed(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_echo(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_auth_result(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_auth_answer_len(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_auth_answer(unsigned char *data, int len,
		mgmt_service_pdu_t *pdu);
static int mgmt_service_proto_decode_auth(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

int mgmt_service_proto_init(mgmt_service_proto_t *proto)
{
	proto->signature = MGMT_SERVICE_PROTO_SIGNATURE;
	return 0;
}

void mgmt_service_proto_term(mgmt_service_proto_t *proto)
{
	proto->signature = 0;
}

int mgmt_service_proto_is_request_packet(mgmt_service_proto_t *proto, scomm_packet_t *packet)
{
	unsigned char *data;

	data = scomm_packet_get_begin_pointer(packet);
	return (((*data) & MANAGEMENT_MESSAGE_TYPE_BIT_MASK) == MANAGEMENT_MESSAGE_TYPE_REQ);
}

int mgmt_service_proto_is_response_packet(mgmt_service_proto_t *proto, scomm_packet_t *packet)
{
	unsigned char *data;

	data = scomm_packet_get_begin_pointer(packet);
	return (((*data) & MANAGEMENT_MESSAGE_TYPE_BIT_MASK) == MANAGEMENT_MESSAGE_TYPE_RESP);
}

int mgmt_service_proto_decode_packet(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu)
{
	int result;
	unsigned char data;

	result = -1;
	if (scomm_packet_remove_head_uint8(packet, &data) < 0) {
		LOG_ERR_AT();
		return -1;
	}

	pdu->type = data & MANAGEMENT_MESSAGE_TYPE_BIT_MASK;
	pdu->id = (data >> MANAGEMENT_MESSAGE_ID_BIT_POS) & MANAGEMENT_MESSAGE_ID_BIT_MASK;

	switch (pdu->id) {
	case MANAGEMENT_MESSAGE_ID_HELLO:
		if (pdu->type == MANAGEMENT_MESSAGE_TYPE_REQ) {
			result = mgmt_service_proto_decode_hello(proto, packet, pdu);
		}
		break;

	case MANAGEMENT_MESSAGE_ID_LED_CONTROL:
		break;

	case MANAGEMENT_MESSAGE_ID_KEEP_ALIVE:
		if (pdu->type == MANAGEMENT_MESSAGE_TYPE_REQ) {
			result = 0;
		}
		break;

	case MANAGEMENT_MESSAGE_ID_BATTERY_STATUS:
		if (pdu->type == MANAGEMENT_MESSAGE_TYPE_IND) {
			result = mgmt_service_proto_decode_battery(proto, packet, pdu);
		}
		break;

	case MANAGEMENT_MESSAGE_ID_CHANGE_BAUD_RATE:
		result = mgmt_service_proto_decode_speed(proto, packet, pdu);
		break;

	case MANAGEMENT_MESSAGE_ID_NEW_BAUD_RATE:
		result = 0;
		break;

	case MANAGEMENT_MESSAGE_ID_AUTH:
		result = mgmt_service_proto_decode_auth(proto, packet, pdu);
		break;

	case MANAGEMENT_MESSAGE_ID_ECHO:
		if (pdu->type == MANAGEMENT_MESSAGE_TYPE_REQ) {
			result = mgmt_service_proto_decode_echo(proto, packet, pdu);
		}
		break;

	default:
		LOG_INFO("Unknown type: 0x%02x, id:0x%02x\n", pdu->type, pdu->id);
		break;
	}

	LOG_INFO("Received type: 0x%02x, id:0x%02x\n", pdu->type, pdu->id);
	return result;
}

int mgmt_service_proto_encode_led_indication(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		unsigned char led_id, unsigned char color, unsigned short blink_interval)
{
	char header;
	unsigned short *n_blink_interval;
	scomm_tlv_t *tlv;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_LED_CONTROL << MANAGEMENT_MESSAGE_ID_BIT_POS) |
			MANAGEMENT_MESSAGE_TYPE_IND;
	scomm_packet_append_tail_uint8(packet, header);

	/* Individual color and blink interval */
	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_LED_TLV_INDIVIDUAL_BLINK;
	tlv->data[0] = led_id;
	tlv->data[1] = color;
	/* Network order */
	n_blink_interval = (unsigned short *)&tlv->data[2];
	*n_blink_interval = htons(blink_interval);

	tlv->length = 4;
	scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));
	return 0;
}

int mgmt_service_proto_encode_change_baud_rate_request(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, unsigned int baud_rate)
{
	char header;
	scomm_tlv_t *tlv;
	unsigned int *nb_baud_rate;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_CHANGE_BAUD_RATE << MANAGEMENT_MESSAGE_ID_BIT_POS) |
			MANAGEMENT_MESSAGE_TYPE_REQ;
	scomm_packet_append_tail_uint8(packet, header);

	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_SPEED_TLV_BAUD_RATE;
	nb_baud_rate = (unsigned int *)&tlv->data[0];
	*nb_baud_rate = htonl(baud_rate);
	tlv->length = 4;
	scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));

	return 0;
}

int mgmt_service_proto_encode_new_baud_rate_request(mgmt_service_proto_t *proto,
		scomm_packet_t *packet)
{
	char header;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_NEW_BAUD_RATE << MANAGEMENT_MESSAGE_ID_BIT_POS) |
			MANAGEMENT_MESSAGE_TYPE_REQ;
	scomm_packet_append_tail_uint8(packet, header);

	return 0;
}

int mgmt_service_proto_encode_new_baud_rate_indication(mgmt_service_proto_t *proto,
		scomm_packet_t *packet)
{
	char header;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_NEW_BAUD_RATE << MANAGEMENT_MESSAGE_ID_BIT_POS) |
			MANAGEMENT_MESSAGE_TYPE_IND;
	scomm_packet_append_tail_uint8(packet, header);

	return 0;
}

int mgmt_service_proto_encode_hello_response(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		const char *device_name)
{
	char header;
	scomm_tlv_t *tlv;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_HELLO << MANAGEMENT_MESSAGE_ID_BIT_POS) |
			MANAGEMENT_MESSAGE_TYPE_RESP;
	scomm_packet_append_tail_uint8(packet, header);

	/* Hello TLVs */
	/* Response Code */
	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_TLV_RESPONSE_CODE_ID;
	tlv->data[0] = MANAGEMENT_TLV_RESPONSE_CODE_OK;
	tlv->length = 1;
	scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));

	/* Device Name */
	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_HELLO_TLV_DEVICE_NAME;
	strcpy((char *)tlv->data, device_name);
	tlv->length = strlen((char *)tlv->data);
	scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));

	return 0;
}

int mgmt_service_proto_add_service_id_in_hello_response(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, unsigned int service_mask)
{
	scomm_tlv_t *tlv;
	int num_services = 0;

	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_HELLO_TLV_SERVICE_ID;
	/* Service ID */
	if (service_mask & SERVICE_ID_SOCKET_MASK) {
		tlv->data[num_services++] = SERVICE_ID_SOCKET;
	}
	if (service_mask & SERVICE_ID_TEST_MASK) {
		tlv->data[num_services++] = SERVICE_ID_TEST;
	}
	tlv->length = num_services;
	scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));

	return 0;
}

int mgmt_service_proto_add_baud_rate_in_hello_response(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, int num_baud_rates,
		const unsigned int baud_rates[MAX_MANAGEMENT_BAUD_RATE_NUM])
{
	int i;
	scomm_tlv_t *tlv;
	unsigned int *nb_baud_rate;

	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_HELLO_TLV_BAUD_RATE;
	for (i = 0; i < num_baud_rates; i++) {
		nb_baud_rate = (unsigned int *)&tlv->data[i * 4];
		*nb_baud_rate = htonl(baud_rates[i]);
	}
	tlv->length = 4 * num_baud_rates;
	scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));

	return 0;
}

int mgmt_service_proto_add_tcp_port_in_hello_response(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, int num_tcp_ports,
		const unsigned short tcp_ports[MAX_MANAGEMENT_TCP_PORT_NUM])
{
	int i;
	scomm_tlv_t *tlv;
	unsigned short *nb_tcp_port;

	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_HELLO_TLV_TCP_PORT;
	for (i = 0; i < num_tcp_ports; i++) {
		nb_tcp_port = (unsigned short *)&tlv->data[i * 2];
		*nb_tcp_port = htons(tcp_ports[i]);
	}
	tlv->length = 2 * num_tcp_ports;
	scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));

	return 0;
}

int mgmt_service_proto_encode_keep_alive_response(mgmt_service_proto_t *proto, scomm_packet_t *packet)
{
	char header;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_KEEP_ALIVE << 2) | MANAGEMENT_MESSAGE_TYPE_RESP;
	scomm_packet_append_tail_uint8(packet, header);

	return 0;
}

int mgmt_service_proto_encode_echo_response(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		unsigned char *data, int len)
{
	int i;
	char header;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_ECHO << MANAGEMENT_MESSAGE_ID_BIT_POS) |
			MANAGEMENT_MESSAGE_TYPE_RESP;

	/* Fill data to be echoed */
	scomm_packet_append_tail_uint8(packet, header);
	for (i = 0; i < len; i++) {
		scomm_packet_append_tail_uint8(packet, data[i]);
	}

	return 0;
}

int mgmt_service_proto_encode_auth_request(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, unsigned char *challenge,
		unsigned short len, unsigned char alg)
{
	int i;
	char header;
	scomm_tlv_t *tlv;
	int challenge_pos;
	int challenge_chunk;
	int challenge_remainder;
	unsigned short *challenge_len;

	/* Management Header */
	header = (MANAGEMENT_MESSAGE_ID_AUTH << MANAGEMENT_MESSAGE_ID_BIT_POS) |
			MANAGEMENT_MESSAGE_TYPE_REQ;
	scomm_packet_append_tail_uint8(packet, header);

	/* Challenge message length */
	tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
	tlv->type = MANAGEMENT_AUTH_TLV_CHALLENGE_LEN;
	tlv->length = 2;
	challenge_len = (unsigned short *)&tlv->data[0];
	*challenge_len = htons(len);
	scomm_packet_forward_tail_bytes(packet, (tlv->length + sizeof(scomm_tlv_t)));

	/* Challenge message */
	challenge_pos = 0;
	challenge_chunk = len / MAX_MANAGEMENT_TLV_DATA_LEN;
	challenge_remainder = len % MAX_MANAGEMENT_TLV_DATA_LEN;
	for (i = 0; i < challenge_chunk; i++) {
		if (scomm_packet_get_tail_avail_length(packet) >=
			(MANAGEMENT_AUTH_TLV_CHALLENGE + sizeof(scomm_tlv_t))) {
			tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
			tlv->type = MANAGEMENT_AUTH_TLV_CHALLENGE;
			tlv->length = MAX_MANAGEMENT_TLV_DATA_LEN;
			memcpy(tlv->data, &challenge[challenge_pos++], MAX_MANAGEMENT_TLV_DATA_LEN);
			scomm_packet_forward_tail_bytes(packet, (tlv->length + sizeof(scomm_tlv_t)));
		} else {
			return -1;
		}
	}

	if (challenge_remainder) {
		if (scomm_packet_get_tail_avail_length(packet) >=
			(challenge_remainder + sizeof(scomm_tlv_t))) {
			tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
			tlv->type = MANAGEMENT_AUTH_TLV_CHALLENGE;
			tlv->length = challenge_remainder;
			memcpy(tlv->data, &challenge[challenge_pos], challenge_remainder);
			scomm_packet_forward_tail_bytes(packet, (tlv->length + sizeof(scomm_tlv_t)));
		} else {
			return -1;
		}
	}

	/* Algorithm */
	if (alg != MAX_MANAGEMENT_AUTH_ALG_DEFAULT) {
		if (scomm_packet_get_tail_avail_length(packet) >=
			(1 + sizeof(scomm_tlv_t))) {
			tlv = (scomm_tlv_t *)scomm_packet_get_end_pointer(packet);
			tlv->type = MANAGEMENT_AUTH_TLV_ALG;
			tlv->length = 1;
			tlv->data[0] = alg;
			scomm_packet_forward_tail_bytes(packet, (tlv->length + 2));
		} else {
			return -1;
		}
	}
	return 0;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Decodes the result entity on the hello packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello_result(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if (len != 1) {
		pdu->u.hello.result = MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR;
		return -1;
	}

	pdu->u.hello.result = data[0];
	return 0;
}

/**
 * @brief Decodes the service entity on the hello packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello_services(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	int i;
	int max_len;

	max_len = (len >= MAX_MANAGEMENT_SERVICE_NUM) ? (MAX_MANAGEMENT_SERVICE_NUM - 1) : len;

	pdu->u.hello.num_services = max_len;
	for (i = 0; i < max_len; i++) {
		pdu->u.hello.services[i] = data[i];
	}

	return 0;
}

/**
 * @brief Decodes the name entity on the hello packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello_name(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	int copy_len;

	copy_len = (len > MAX_MANAGEMENT_DEVICE_NAME) ? MAX_MANAGEMENT_DEVICE_NAME : len;
	memcpy(pdu->u.hello.name, data, copy_len);
	pdu->u.hello.name[copy_len] = '\0';

	return 0;
}

/**
 * @brief Decodes the sw version entity on the hello packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello_sw_version(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	int copy_len;

	copy_len = (len > MAX_MANAGEMENT_VERSION_LEN) ? MAX_MANAGEMENT_VERSION_LEN : len;
	memcpy(pdu->u.hello.sw_version, data, copy_len);
	pdu->u.hello.sw_version[copy_len] = '\0';

	return 0;
}

/**
 * @brief Decodes the hw version entity on the hello packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello_hw_version(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	int copy_len;

	copy_len = (len > MAX_MANAGEMENT_VERSION_LEN) ? MAX_MANAGEMENT_VERSION_LEN : len;
	memcpy(pdu->u.hello.hw_version, data, copy_len);
	pdu->u.hello.hw_version[copy_len] = '\0';

	return 0;
}

/**
 * @brief Decodes the service entity on the hello packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello_baud_rates(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	int i;
	int num_baud_rates;

	num_baud_rates = MIN((len / 4), MAX_MANAGEMENT_BAUD_RATE_NUM);
	pdu->u.hello.num_baud_rates = num_baud_rates;
	for (i = 0; i < num_baud_rates; i++) {
		pdu->u.hello.baud_rates[i] = ntohl(*((unsigned int *)&data[i * 4]));
		LOG_INFO("hello_baud_rates %d, baud %d\n", i, pdu->u.hello.baud_rates[i]);
	}

	return 0;
}

/**
 * @brief Decodes the function code entity on the hello packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello_function_code(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	/* if the length of the function code is larger than expected, discard it */
	if (len > MAX_MANAGEMENT_FUNCTION_CODE_LEN) {
		return -1;
	}
	pdu->u.hello.function_code_len = len;
	memcpy(pdu->u.hello.function_code, data, len);

	return 0;
}

/**
 * @brief Decodes the hello packet
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be decoded
 * @param pdu The pdu to be decoded from the packet
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_hello(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu)
{
	int result;
	scomm_tlv_t *tlv;

	pdu->u.hello.valid_mask = 0;
	while (scomm_packet_get_data_length(packet) > 0) {
		tlv = (scomm_tlv_t *)scomm_packet_get_begin_pointer(packet);
		switch (tlv->type) {
		case MANAGEMENT_TLV_RESPONSE_CODE_ID:
			result = mgmt_service_proto_decode_hello_result(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_HELLO_TLV_SERVICE_ID:
			result = mgmt_service_proto_decode_hello_services(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_HELLO_TLV_DEVICE_NAME:
			result = mgmt_service_proto_decode_hello_name(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_HELLO_TLV_SW_VERSION:
			result = mgmt_service_proto_decode_hello_sw_version(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_HELLO_TLV_HW_VERSION:
			result = mgmt_service_proto_decode_hello_hw_version(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_HELLO_TLV_BAUD_RATE:
			result = mgmt_service_proto_decode_hello_baud_rates(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_HELLO_TLV_FUNCTION_CODE:
			result = mgmt_service_proto_decode_hello_function_code(tlv->data, tlv->length, pdu);
			break;

		default:
			LOG_INFO("[Mgmt-Prot] Unknown Hello tlv: %d\n", tlv->type);
			result = -1;
			break;
		}

		if (!result) {
			pdu->u.hello.valid_mask |= (1 << tlv->type);
		}

		CHK_RET(scomm_packet_forward_head_bytes(packet, (tlv->length + 2)), 0);
	}
	return 0;
}

/**
 * @brief Decodes the result entity on the battery packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_battery_result(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if (len != 1) {
		pdu->u.battery.result = MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR;
		return -1;
	}

	pdu->u.battery.result = data[0];
	return 0;
}

/**
 * @brief Decodes the battery level entity on the battery packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_battery_level(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if (len != 1) {
		return -1;
	}

	pdu->u.battery.capacity = data[0];
	return 0;
}

/**
 * @brief Decodes the battery status entity on the battery packet
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_battery_status(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if (len != 1) {
		return -1;
	}

	pdu->u.battery.status = data[0];
	return 0;
}

/**
 * @brief Decodes the battery packet
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be decoded
 * @param pdu The pdu to be decoded from the packet
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_battery(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu)
{
	int result;
	scomm_tlv_t *tlv;

	pdu->u.battery.valid_mask = 0;
	while (scomm_packet_get_data_length(packet) > 0) {
		tlv = (scomm_tlv_t *)scomm_packet_get_begin_pointer(packet);
		switch (tlv->type) {
		case MANAGEMENT_TLV_RESPONSE_CODE_ID:
			result = mgmt_service_proto_decode_battery_result(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_BATTERY_TLV_LEVEL:
			result = mgmt_service_proto_decode_battery_level(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_BATTERY_TLV_STATUS:
			result = mgmt_service_proto_decode_battery_status(tlv->data, tlv->length, pdu);
			break;

		default:
			result = -1;
			break;
		}

		if (!result) {
			pdu->u.battery.valid_mask |= (1 << tlv->type);
		}

		CHK_RET(scomm_packet_forward_head_bytes(packet, (tlv->length + 2)), 0);
	}
	LOG_INFO("Battery level: %d, status: %d\n", pdu->u.battery.capacity, pdu->u.battery.status);
	return 0;
}

/**
 * @brief Decode the result of the message that is used to change the baud rate
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_speed_result(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if (len != 1) {
		pdu->u.speed.result = MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR;
		return -1;
	}

	pdu->u.speed.result = data[0];
	return 0;
}

/**
 * @brief Decode the message that is used to change the baud rate
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be decoded
 * @param pdu The pdu to be decoded from the packet
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_speed(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu)
{
	int result;
	scomm_tlv_t *tlv;

	pdu->u.speed.valid_mask = 0;
	while (scomm_packet_get_data_length(packet) > 0) {
		tlv = (scomm_tlv_t *)scomm_packet_get_begin_pointer(packet);
		switch (tlv->type) {
		case MANAGEMENT_TLV_RESPONSE_CODE_ID:
			result = mgmt_service_proto_decode_speed_result(tlv->data, tlv->length, pdu);
			break;

		default:
			result = -1;
			break;
		}

		if (!result) {
			pdu->u.speed.valid_mask |= (1 << tlv->type);
		}

		CHK_RET(scomm_packet_forward_head_bytes(packet, (tlv->length + 2)), 0);
	}
	return 0;
}

/**
 * @brief Decodes the echo packet
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be decoded
 * @param pdu The pdu to be decoded from the packet
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_echo(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu)
{
	pdu->u.echo.payload = scomm_packet_get_begin_pointer(packet);
	pdu->u.echo.len = scomm_packet_get_data_length(packet);
	return 0;
}

/**
 * @brief Decode the result of the message that is used to authenticate
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_auth_result(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if (len != 1) {
		pdu->u.auth.result = MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR;
		return -1;
	}

	pdu->u.auth.result = data[0];
	return 0;
}

/**
 * @brief Decode the length of the challenge message that is used to
*         authenticate the attached device
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_auth_answer_len(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if (len != 2) {
		pdu->u.auth.result = MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR;
		return -1;
	}

	pdu->u.auth.expected_ans_len = ntohs(*((unsigned short *)data));
	if (pdu->u.auth.expected_ans_len > MAX_MANAGEMENT_AUTH_ANSWER_LEN) {
		pdu->u.auth.result = MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR;
		return -1;
	}

	return 0;
}

/**
 * @brief Decode the challenge message that is used to authenticate the
 *        attached device
 *
 * @param data The location of data to be decoded
 * @param len The number of data in bytes
 * @param pdu The pdu to be decoded from the data
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_auth_answer(unsigned char *data, int len, mgmt_service_pdu_t *pdu)
{
	if ((pdu->u.auth.received_ans_len + len) > pdu->u.auth.expected_ans_len) {
		pdu->u.auth.result = MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR;
		return -1;
	}

	memcpy(&pdu->u.auth.answer[pdu->u.auth.received_ans_len], data, len);
	pdu->u.auth.received_ans_len += len;
	return 0;
}

/**
 * @brief Decode the message that is used to authenticate the attached device.
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be decoded
 * @param pdu The pdu to be decoded from the packet
 * @return 0 on success, or a negative value on error
 */
static int mgmt_service_proto_decode_auth(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu)
{
	int result;
	scomm_tlv_t *tlv;

	pdu->u.auth.result = MANAGEMENT_TLV_RESPONSE_CODE_OK;
	pdu->u.auth.valid_mask = 0;
	pdu->u.auth.expected_ans_len = 0;
	pdu->u.auth.received_ans_len = 0;
	while (scomm_packet_get_data_length(packet) > 0) {
		tlv = (scomm_tlv_t *)scomm_packet_get_begin_pointer(packet);
		switch (tlv->type) {
		case MANAGEMENT_TLV_RESPONSE_CODE_ID:
			result = mgmt_service_proto_decode_auth_result(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_AUTH_TLV_ANSWER_LEN:
			result = mgmt_service_proto_decode_auth_answer_len(tlv->data, tlv->length, pdu);
			break;

		case MANAGEMENT_AUTH_TLV_ANSWER:
			result = mgmt_service_proto_decode_auth_answer(tlv->data, tlv->length, pdu);
			break;

		default:
			result = -1;
			break;
		}

		if (!result) {
			pdu->u.auth.valid_mask |= (1 << tlv->type);
		}

		CHK_RET(scomm_packet_forward_head_bytes(packet, (tlv->length + sizeof(scomm_tlv_t))), 0);
	}
	return 0;
}
