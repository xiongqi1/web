/**
 * @file message_protocol.c
 * @brief legacy management protocol funtions
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY Casa Systems ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef MESSAGE_PROTOCOL_H_11020009122019
#define MESSAGE_PROTOCOL_H_11020009122019

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

#define MANAGEMENT_HELLO_TLV_RESPONSE 0
#define MANAGEMENT_HELLO_TLV_SERVICE_ID 1
#define MANAGEMENT_HELLO_TLV_DEVICE_NAME 2
#define MANAGEMENT_HELLO_TLV_SW_VERSION 3
#define MANAGEMENT_HELLO_TLV_HW_VERSION 4
#define MANAGEMENT_HELLO_TLV_BAUD_RATE 5
#define MANAGEMENT_HELLO_TLV_TCP_PORT 6
#define MANAGEMENT_HELLO_TLV_FUNCTION_CODE 7

#define MANAGEMENT_LED_TLV_INDIVIDUAL_BLINK 1

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
#define MAX_MANAGEMENT_SERVICE_NUM 2
#define MAX_MANAGEMENT_DEVICE_NAME 60
#define MAX_MANAGEMENT_BAUD_RATE_NUM 8
#define MAX_MANAGEMENT_TCP_PORT_NUM 1
#define MAX_MANAGEMENT_SW_NAME 60
#define MAX_MANAGEMENT_HW_NAME 60
#define MAX_LED_NUM 3
#define MAX_TLV_LENGTH 256
#define MAX_MANAGEMENT_PACKET_BUFF_SIZE 1024
#define MAX_MANAGEMENT_VERSION_LEN 60

#define DEVICE_NAME "nrb-0200"
#define SW_VERSION "0.01"           //TODO, rdb?
#define SERVICE_ID_SOCKET  1        //Not very clear

enum message_type{
    REQUEST=0,
    RESPONSE,
    INDICATION
};

enum message_id {
    ID_HELLO = 0,
    ID_KEEP_ALIVE,
    ID_LED_CONTROL,
    ID_BATTERY,
    ID_KEY_STATUS,
    ID_BAUD_CHANGE,
    ID_BAUD_NEW,
    ID_AUTH
};

typedef struct tlv_message {
    unsigned char type;
    unsigned char length;
    unsigned char data[MAX_TLV_LENGTH];
} __attribute__((packed)) tlv_message_t;

typedef struct mgmt_packet {
    unsigned int tail;
    unsigned int len;
    char tlv_pdus[MAX_MANAGEMENT_PACKET_BUFF_SIZE];
} mgmt_packet_t;

typedef struct hello_message {
    unsigned int id_mask;   //use to indicate which sub tlv id type is valid in message
    unsigned char response;
    unsigned char num_services;
    unsigned char services[MAX_MANAGEMENT_DEVICE_NAME];
    char device_name[MAX_MANAGEMENT_DEVICE_NAME+1];
    char sw_ver[MAX_MANAGEMENT_SW_NAME+1];
    char hw_ver[MAX_MANAGEMENT_HW_NAME+1];
    int baudrate[MAX_MANAGEMENT_BAUD_RATE_NUM];
    unsigned char num_tcp_ports;
    unsigned short tcp_ports[MAX_MANAGEMENT_TCP_PORT_NUM];
    char * secrets;
} hello_msg_t;

typedef struct keep_alive_message {
    unsigned char tlv_none;  //message has no tlv, use it as placeholder
} keep_alive_msg_t;

/* can one indication message includes couple of LED control message?? */
typedef struct led_ctrl {
    unsigned char id;
    unsigned char color;
    unsigned short flashing_interval;
} led_ctrl_t;

typedef struct led_control_message {
    unsigned int id_mask;
    unsigned int valid_led_num; //how many valid led control requests
    unsigned char response;
    led_ctrl_t led[MAX_LED_NUM];
} led_msg_t;

typedef struct key_state_message {
    unsigned short key_mask;
    unsigned short key_state;
} key_state_msg_t;

typedef struct battery_state_message {
    unsigned char battery_capacity;
    unsigned char charging_status;
} battery_msg_t;

typedef struct change_baudrate_message {
    unsigned int id_mask;
    char response;
    unsigned int baudrate;
} change_baudrate_message_t;

typedef struct new_baudrate_message {
    unsigned char tlv_none; //message has no tlv, use it as placeholder
} new_baudrate_message_t;

typedef struct auth_message {
    unsigned int id_mask;
    unsigned char response;
    unsigned short question_size;
    char question[MAX_MANAGEMENT_AUTH_CHALLENGE_LEN];
    unsigned short answer_size;
    char answer[256];
} auth_message_t;

/*TODO: add global statistic counter to record all type of message send/receive number*/
typedef struct service_protocol {
    unsigned char valid;
    unsigned char type;
    unsigned char id;
    union {
        hello_msg_t hello_msg;
        keep_alive_msg_t keepalive_msg;
        led_msg_t  led_msg;
        battery_msg_t battery_msg;
        key_state_msg_t key_msg;
        change_baudrate_message_t changebaudrate_msg;
        new_baudrate_message_t newbaudrate_msg;
        auth_message_t auth_msg;
    } msg_data;
} service_protocol_t;


int parse_message(char * msg, int msg_len, service_protocol_t *proto_msg);
int create_init_hello_request_message(mgmt_packet_t *mgm_packet);
int create_keepalive_request_message(mgmt_packet_t *mgm_packet);
int create_battery_indication_message(mgmt_packet_t *mgm_packet);
int create_led_reponse_message(mgmt_packet_t *mgm_packet);
int create_change_baudrate_response_message(mgmt_packet_t *mgm_packet, unsigned char response);
int create_new_baudrate_reponse_message(mgmt_packet_t *mgm_packet);

int is_hello_without_baudrate(hello_msg_t * msg);
int is_hello_with_baudrate(hello_msg_t * msg);
#endif