#ifndef SOCK_SERVICE_PROTO_H_15413230112015
#define SOCK_SERVICE_PROTO_H_15413230112015
/**
 * @file sock_service_prot.h
 * @brief provides public interfaces to encode/decode packets on the sock service
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
#include "scomm_manager.h"
#include "scomm.h"

/*******************************************************************************
 * Define public macros
 ******************************************************************************/
#define SOCKET_MESSAGE_TYPE_BIT_MASK 0x1
#define SOCKET_MESSAGE_TYPE_BIT_POS 0
#define SOCKET_MESSAGE_MF_BIT_MASK 0x1
#define SOCKET_MESSAGE_MF_BIT_POS 1
#define SOCKET_MESSAGE_CHANNEL_TYPE_BIT_MASK 0x3
#define SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS  2
#define SOCKET_MESSAGE_CHANNEL_ID_BIT_MASK 0xF
#define SOCKET_MESSAGE_CHANNEL_ID_BIT_POS  4

#define SOCKET_MESSAGE_TYPE_DATA 0
#define SOCKET_MESSAGE_TYPE_CONTROL 1

#define SOCKET_MESSAGE_CHANNEL_TYPE_TCP 0
#define SOCKET_MESSAGE_CHANNEL_TYPE_UDP 1

#define SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_BIT_MASK 0x3
#define SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_BIT_POS 0
#define SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_MASK 0x3F
#define SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS 2

#define SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_REQ 0
#define SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_RESP 1
#define SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND 2

#define SOCKET_MESSAGE_CONTROL_COMMAND_ID_OPEN_SOCKET 0
#define SOCKET_MESSAGE_CONTROL_COMMAND_ID_CLOSE_SOCKET 1
#define SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL 2

#define SOCK_SERVICE_APP_SIGNATURE 0x61D036A1

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

/**
 * @brief The structure of the pdu to build the request of the open socket
 */
typedef struct sock_service_open_socket_pdu {
	unsigned short port;
} sock_service_open_socket_pdu_t;

/**
 * @brief The structure of the pdu to indicate the available buffer
 */
typedef struct sock_service_buffer_avail_pdu_t {
	unsigned char num_packets;
} sock_service_buffer_avail_pdu_t;

/**
 * @brief The strucutre of the control pdu on the sock service
 */
typedef struct sock_service_control_pdu {
	unsigned char command_type;
	unsigned char command_id;
	union {
		sock_service_open_socket_pdu_t open_socket;
		sock_service_buffer_avail_pdu_t buffer_avail;
	} u;
} sock_service_control_pdu_t;

/**
 * @brief The strucutre of the data pdu on the sock service
 */
typedef struct sock_service_data_pdu {
	unsigned char *payload;
	int len;
} sock_service_data_pdu_t;

/**
 * @brief The strucutre of the pdu on the sock service
 */
typedef struct sock_service_pdu {
	unsigned char type;
	unsigned char more_data;
	unsigned char channel_type;
	unsigned char channel_id;
	union {
		sock_service_control_pdu_t control;
		sock_service_data_pdu_t data;
	} u;
} sock_service_pdu_t;

/**
 * @brief The strucutre of the sock service protocol
 */
typedef struct sock_service_proto {
	unsigned int signature;
} sock_service_proto_t;

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialises the sock service protocol
 *
 * @param proto The context of the sock service protocol
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_init(sock_service_proto_t *proto);

/**
 * @brief Terminates the sock service protocol
 *
 * @param proto The context of the sock service protocol
 * @return Void
 */
void sock_service_proto_term(sock_service_proto_t *proto);

/**
 * @brief Starts the sock service protocol
 *
 * @param proto The context of the sock service protocol
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_start(sock_service_proto_t *proto);

/**
 * @brief Stops the sock service protocol
 *
 * @param proto The context of the sock service protocol
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_stop(sock_service_proto_t *proto);

/**
 * @brief Checks whether the packet is a control packet or not
 *
 * @param proto The context of the sock service protocol
 * @param packet The packet to be checked
 * @return 1 if the packet is a control packet, 0 otherwise
 */
int sock_service_proto_is_control_packet(sock_service_proto_t *proto, scomm_packet_t *packet);

/**
 * @brief Decodes the packet that came to the sock service
 *
 * @param proto The context of the sock service protocol
 * @param packet The packet to be decoded
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_decode_packet(sock_service_proto_t *proto, scomm_packet_t *packet,
									sock_service_pdu_t *pdu);

/**
 * @brief Encodes the data packet that leaves from the sock service
 *
 * @param proto The context of the sock service protocol
 * @param packet The packet to store the encoded data
 * @param type The type of the socket to be encoded
 * @param id The channel id of the scomm channel
 * @param data The data to be encoded
 * @param len The number of data in bytes
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_encode_data_packet(sock_service_proto_t *proto, scomm_packet_t *packet,
		sock_type_t type, unsigned char id, unsigned char *data, int len);


int sock_service_proto_encode_close_socket_indication(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type);

/**
 * @brief Encodes the response packet in reply to the request of the open socket
 *
 * @param proto The context of the sock service protocol
 * @param packet The packet to store the encoded data
 * @param id The channel id of the scomm channel
 * @param type The type of the socket to be encoded
 * @param port The port to be opened/closed
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_encode_open_socket_response(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type, unsigned short port);

/**
 * @brief Encodes the response packet in reply to the request of the close socket
 *
 * @param proto The context of the sock service protocol
 * @param packet The packet to store the encoded data
 * @param id The channel id of the scomm channel
 * @param type The type of the socket to be encoded
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_encode_close_socket_response(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type);

/**
 * @brief Encodes the indication packet to inform how many packets it can accept
 *
 * @param proto The context of the sock service protocol
 * @param packet The packet to store the encoded data
 * @param id The channel id of the scomm channel
 * @param type The type of the socket to be encoded
 * @param num_packets The number of packet it can accept.
 * @return 0 on success, or a negative value on error
 */
int sock_service_proto_encode_buffer_avail_indication(sock_service_proto_t *proto, scomm_packet_t *packet,
		int id, sock_type_t type, unsigned char num_packets);

#endif
