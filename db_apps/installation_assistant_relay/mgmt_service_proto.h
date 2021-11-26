#ifndef MGMT_SERVICE_PROTO_H_17012630112015
#define MGMT_SERVICE_PROTO_H_17012630112015
/**
 * @file mgmt_service_prot.h
 * @breif Provides public interfaces to use the management service protocol
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

#include "app_config.h"
#include "scomm_manager.h"
#include "scomm.h"

/*******************************************************************************
 * Define public macros
 ******************************************************************************/
#define MGMT_SERVICE_PROTO_SIGNATURE 0x50268B52

#define MANAGEMENT_MESSAGE_TYPE_BIT_MASK 0x3

#define MANAGEMENT_MESSAGE_TYPE_REQ 0
#define MANAGEMENT_MESSAGE_TYPE_RESP 1
#define MANAGEMENT_MESSAGE_TYPE_IND 2

#define MANAGEMENT_MESSAGE_ID_BIT_MASK 0x3f
#define MANAGEMENT_MESSAGE_ID_BIT_POS 2

#define MANAGEMENT_MESSAGE_ID_HELLO 0
#define MANAGEMENT_MESSAGE_ID_KEEP_ALIVE 1
#define MANAGEMENT_MESSAGE_ID_LED_CONTROL 2
#define MANAGEMENT_MESSAGE_ID_BATTERY_STATUS 3
#define MANAGEMENT_MESSAGE_ID_KEY_STATE_CHANGED 4
#define MANAGEMENT_MESSAGE_ID_CHANGE_BAUD_RATE 5
#define MANAGEMENT_MESSAGE_ID_NEW_BAUD_RATE 6
#define MANAGEMENT_MESSAGE_ID_AUTH 7
#define MANAGEMENT_MESSAGE_ID_ECHO 63

#define MANAGEMENT_TLV_RESPONSE_CODE_ID 0
#define MANAGEMENT_TLV_RESPONSE_CODE_OK 0
#define MANAGEMENT_TLV_RESPONSE_CODE_GENERAL_ERROR 1

#define MANAGEMENT_HELLO_TLV_SERVICE_ID 1
#define MANAGEMENT_HELLO_TLV_DEVICE_NAME 2
#define MANAGEMENT_HELLO_TLV_SW_VERSION 3
#define MANAGEMENT_HELLO_TLV_HW_VERSION 4
#define MANAGEMENT_HELLO_TLV_BAUD_RATE 5
#define MANAGEMENT_HELLO_TLV_TCP_PORT 6
#define MANAGEMENT_HELLO_TLV_FUNCTION_CODE 7

#define MANAGEMENT_LED_TLV_INDIVIDUAL_BLINK 1

#define MAX_MANAGEMENT_TLV_DATA_LEN 255

#define MAX_MANAGEMENT_SERVICE_NUM 2
#define MAX_MANAGEMENT_DEVICE_NAME 60
#define MAX_MANAGEMENT_VERSION_LEN 15
#define MAX_MANAGEMENT_BAUD_RATE_NUM 8
#define MAX_MANAGEMENT_TCP_PORT_NUM 1
#define MAX_MANAGEMENT_FUNCTION_CODE_LEN 8

#define MANAGEMENT_BATTERY_TLV_LEVEL 1
#define MANAGEMENT_BATTERY_TLV_STATUS 2

#define MANAGEMET_BATTERY_STATUS_DISCHARING 0
#define MANAGEMET_BATTERY_STATUS_CHARING 1
#define MANAGEMET_BATTERY_STATUS_FULL_CHARGED 2
#define MANAGEMET_BATTERY_STATUS_ERROR 3

#define MANAGEMENT_SPEED_TLV_BAUD_RATE 1

#define MANAGEMENT_AUTH_TLV_CHALLENGE_LEN 1
#define MANAGEMENT_AUTH_TLV_CHALLENGE 2
#define MANAGEMENT_AUTH_TLV_ANSWER_LEN 3
#define MANAGEMENT_AUTH_TLV_ANSWER 4
#define MANAGEMENT_AUTH_TLV_ALG 5

#define MAX_MANAGEMENT_AUTH_CHALLENGE_LEN 64
#define MAX_MANAGEMENT_AUTH_ANSWER_LEN 256 /* RSA key 2048-bit */
#define MAX_MANAGEMENT_AUTH_ALG_DEFAULT 0

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

/**
 * @brief Represents the content of the hello packet
 */
typedef struct mgmt_service_hello_t {
	unsigned int valid_mask;
	unsigned char result;
	unsigned char num_services;
	unsigned char services[MAX_MANAGEMENT_SERVICE_NUM];
	char name[MAX_MANAGEMENT_DEVICE_NAME + 1];
	char sw_version[MAX_MANAGEMENT_VERSION_LEN + 1];
	char hw_version[MAX_MANAGEMENT_VERSION_LEN + 1];
	unsigned char num_baud_rates;
	unsigned int baud_rates[MAX_MANAGEMENT_BAUD_RATE_NUM];
	unsigned char function_code_len;
	unsigned char function_code[MAX_MANAGEMENT_FUNCTION_CODE_LEN];
} mgmt_service_hello_t;

/**
 * @brief The structure of the led group
 */
typedef struct mgmt_service_led_group {
	unsigned short mask;
	unsigned short value;
} mgmt_service_led_group_t;

/**
 * @brief The structure of the individual led
 */
typedef struct mgmt_service_led_individual {
	unsigned char id;
	unsigned char color;
	unsigned short blink_interval;
} mgmt_service_led_individual_t;

/**
 * @brief Represents the content of the led packet
 */
typedef struct mgmt_service_led {
	unsigned int valid_mask;
	unsigned char result;
	unsigned short group_mask;
	unsigned short group_value;
	unsigned char num_leds;
	mgmt_service_led_individual_t individuals[MAX_LED_NUM];
} mgmt_service_led_t;

/**
 * @brief Represents the content of the battery packet
 */
typedef struct mgmt_service_battery {
	unsigned int valid_mask;
	unsigned char result;
	unsigned char capacity;
	unsigned char status;
} mgmt_service_battery_t;

/**
 * @brief Represents the content to enhance the speed
 */
typedef struct mgmt_service_speed {
	unsigned int valid_mask;
	unsigned char result;
	unsigned int baud_rate;
} mgmt_service_speed_t;

/**
 * @brief Represents the content of the authentication packet
 */
typedef struct mgmt_service_auth {
	unsigned int valid_mask;
	unsigned char result;
	unsigned char answer[MAX_MANAGEMENT_AUTH_ANSWER_LEN];
	unsigned short expected_ans_len;
	unsigned short received_ans_len;
} mgmt_service_auth_t;

/**
 * @brief Represents the content of the echo packet
 */
typedef struct mgmt_service_echo {
	unsigned char *payload;
	int len;
} mgmt_service_echo_t;

/**
 * @brief The structure of the pdu on the management service
 */
typedef struct mgmt_service_pdu_t {
	int type;
	int id;
	union {
		mgmt_service_hello_t hello;
		mgmt_service_led_t led;
		mgmt_service_battery_t battery;
		mgmt_service_speed_t speed;
		mgmt_service_auth_t auth;
		mgmt_service_echo_t echo;
	} u;
} mgmt_service_pdu_t;

/**
 * @brief The structure of the management service protocol
 */
typedef struct mgmt_service_proto {
	unsigned int signature;
} mgmt_service_proto_t;

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialises the management service protocol
 *
 * @param proto The context of the management service protocol
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_init(mgmt_service_proto_t *proto);

/**
 * @brief Terminates the management service protocol
 *
 * @param proto The context of the management service protocol
 * @return Void
 */
void mgmt_service_proto_term(mgmt_service_proto_t *proto);

/**
 * @brief Starts the management service protocol
 *
 * @param proto The context of the management service protocol
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_start(mgmt_service_proto_t *proto);

/**
 * @brief Stops the management service protocol
 *
 * @param proto The context of the management service protocol
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_stop(mgmt_service_proto_t *proto);


/**
 * @brief Checks whether the packet is a control packet or not.
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be checked
 * @return 1 if the packet is a control packet, 0 otherwise
 */
int mgmt_service_proto_is_request_packet(mgmt_service_proto_t *proto, scomm_packet_t *packet);

/**
 * @brief Decodes the packet coming to management service
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be decoded
 * @param pdu The pdu that is decoded from the packet
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_decode_packet(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		mgmt_service_pdu_t *pdu);

/**
 * @brief Encodes the request packet to control an individual led
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @param led_id The led ID to be controlled
 * @param color The color to be set on the led
 * @param blink_interval Indicates how quickly the led blinks
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_encode_led_indication(mgmt_service_proto_t *proto, scomm_packet_t *packet,
        unsigned char led_id, unsigned char color, unsigned short blink_interval);

/**
 * @brief Encodes the request packet to change the baud rate
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @param baud_rate The baud rate to be used for the serial communication
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_encode_change_baud_rate_request(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, unsigned int baud_rate);

/**
 * @brief Encodes the request packet to validate the new baud rate is good.
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_encode_new_baud_rate_request(mgmt_service_proto_t *proto,
		scomm_packet_t *packet);

/**
 * @brief Encodes the indication packet to validate the new baud rate is good.
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_encode_new_baud_rate_indication(mgmt_service_proto_t *proto,
		scomm_packet_t *packet);

/**
 * @brief Encodes the response packet to carry hello information
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @param device_name The name of the device
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_encode_hello_response(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		const char *device_name);

/**
 * @brief Encodes the response packet to carry the list of services in hello response
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @param service_mask The mask to indicate which service is available
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_add_service_id_in_hello_response(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, unsigned int service_mask);

/**
 * @brief Encodes the response packet to carry the list of baud rates in hello response
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @param num_baud_rates The number of baud rates the device can support
 * @param baud_rates The list of baud rates the device can support
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_add_baud_rate_in_hello_response(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, int num_baud_rates,
		const unsigned int baud_rates[MAX_MANAGEMENT_BAUD_RATE_NUM]);

/**
 * @brief Encodes the response packet to carry the list of tcp ports in hello response
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @param num_tcp_ports The number of tcp ports the device wants to listen
 * @param tcp_ports The list of tcp ports the device wants to listen
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_add_tcp_port_in_hello_response(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, int num_tcp_ports,
		const unsigned short tcp_ports[MAX_MANAGEMENT_TCP_PORT_NUM]);

/**
 * @brief Encodes the request packet to carry keep alive information
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_encode_keep_alive_response(mgmt_service_proto_t *proto, scomm_packet_t *packet);

/**
 * @brief Encodes the request packet to carry echo information
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @return 0 on success, or a negative value on error
 *
 * @note This function is used for testing the channel
 */
int mgmt_service_proto_encode_echo_response(mgmt_service_proto_t *proto, scomm_packet_t *packet,
		unsigned char *data, int len);

/**
 * @brief Encodes the request packet to start authenticating with the attached device
 *
 * @param proto The context of the management service protocol
 * @param packet The packet to be encoded
 * @param challenge The challenged message in the authentication request
 * @param len The length of the challenge message
 * @param alg Algorithm to be used for conducting authentication
 * @return 0 on success, or a negative value on error
 */
int mgmt_service_proto_encode_auth_request(mgmt_service_proto_t *proto,
		scomm_packet_t *packet, unsigned char *challenge, unsigned short len, unsigned char alg);

#ifdef __cplusplus
}
#endif

#endif
