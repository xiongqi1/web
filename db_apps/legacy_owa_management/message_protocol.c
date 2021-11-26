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
#include <string.h>
#include <endian.h>
#include <arpa/inet.h>
#include "message_protocol.h"
#include "nc_util.h"
#include "rdb_help.h"

/**
 * @brief The list of baud rates can be supported
 * @note The maximum number of baud rates is limited to MAX_MANAGEMENT_BAUD_RATE_NUM.
 */
const unsigned int supported_baud_rates[] = {
    230400,
    460800,
    921600,
    1500000,
    2000000,
    3000000
};

static int parse_hello(char *msg, int msg_len, service_protocol_t *srv_msg)
{
    unsigned char tlv_type, tlv_len;
    unsigned char temp;
    unsigned int id_mask = 0;
    int ret = 0;

    while (msg_len !=0) {
        tlv_type = *msg;
        tlv_len = *(++msg);
        switch (tlv_type) {
            case MANAGEMENT_HELLO_TLV_RESPONSE:
                if (tlv_len != 1) {
                    ret = -1;
                    break;
                };
                srv_msg->msg_data.hello_msg.response = *(++msg);
                id_mask |= (1 << MANAGEMENT_HELLO_TLV_RESPONSE);
                break;

            case MANAGEMENT_HELLO_TLV_SERVICE_ID:
                temp = tlv_len>MAX_MANAGEMENT_SERVICE_NUM? MAX_MANAGEMENT_SERVICE_NUM:tlv_len;
                srv_msg->msg_data.hello_msg.num_services = temp;
                memcpy(srv_msg->msg_data.hello_msg.services, ++msg, temp);
                id_mask |= (1 << MANAGEMENT_HELLO_TLV_SERVICE_ID);
                break;

            case MANAGEMENT_HELLO_TLV_DEVICE_NAME:
                temp = tlv_len>MAX_MANAGEMENT_DEVICE_NAME?MAX_MANAGEMENT_DEVICE_NAME:tlv_len;
                memcpy(srv_msg->msg_data.hello_msg.device_name, ++msg, temp);
                id_mask |= (1 << MANAGEMENT_HELLO_TLV_DEVICE_NAME);
                break;

            case MANAGEMENT_HELLO_TLV_SW_VERSION:
            case MANAGEMENT_HELLO_TLV_HW_VERSION:
            case MANAGEMENT_HELLO_TLV_BAUD_RATE:
                ret = -1;
                break;
            case MANAGEMENT_HELLO_TLV_TCP_PORT:
                temp = tlv_len>MAX_MANAGEMENT_TCP_PORT_NUM*2? MAX_MANAGEMENT_TCP_PORT_NUM*2:tlv_len;
                srv_msg->msg_data.hello_msg.num_tcp_ports = temp/2;
                memcpy(srv_msg->msg_data.hello_msg.tcp_ports, ++msg, temp);
                id_mask |= (1 << MANAGEMENT_HELLO_TLV_TCP_PORT);
                break;
            case MANAGEMENT_HELLO_TLV_FUNCTION_CODE:
                ret = -1;
                break;
            default:
                ret = -1;
                break;

        }
        msg += tlv_len;
        msg_len -= (tlv_len +2);
    }
    srv_msg->msg_data.hello_msg.id_mask = id_mask;
    return ret;
}

static int parse_keepalive(char *msg, int msg_len, service_protocol_t *srv_msg)
{
    srv_msg->msg_data.keepalive_msg.tlv_none = 1;
    return 0;
}

static int parse_changebaudrate(char *msg, int msg_len, service_protocol_t *srv_msg)
{
    unsigned char tlv_type, tlv_len;
    int ret = 0;

    tlv_type = *msg;
    tlv_len = *(++msg);
    if(tlv_type != MANAGEMENT_SPEED_TLV_BAUD_RATE || tlv_len != 4) {
        ret = -1;
    } else {
        srv_msg->msg_data.changebaudrate_msg.baudrate = ntohl((unsigned int)(*++msg));
    }
    return ret;
}

static int parse_newbaudrate(char *msg, int msg_len, service_protocol_t *srv_msg)
{
    srv_msg->msg_data.newbaudrate_msg.tlv_none = 1;
    return 0;
}

static int parse_led(char *msg, int msg_len, service_protocol_t *srv_msg)
{
    unsigned char tlv_type, tlv_len;
    unsigned char temp=0;
    int ret = 0;

    while (msg_len !=0) {
        tlv_type = *msg;
        tlv_len = *(++msg);
        if (tlv_type != MANAGEMENT_LED_TLV_INDIVIDUAL_BLINK) {
            ret = -1;
            break;
        } else {
            srv_msg->msg_data.led_msg.led[temp].id = *(++msg);
            srv_msg->msg_data.led_msg.led[temp].color = *(++msg);
            srv_msg->msg_data.led_msg.led[temp].flashing_interval = ntohs(*(unsigned short *)(++msg));
            msg +=2; //to next tlv
            srv_msg->msg_data.led_msg.valid_led_num++;
            msg_len -= (tlv_len+2);
            //msg += tlv_len;
            if (++temp > MAX_LED_NUM) break;
        }
    }
    return ret;
}

int parse_message(char * msg, int msg_len, service_protocol_t *proto_msg )
{
    enum message_id id;
    enum message_type type;
    service_protocol_t * srv_msg;

    int ret = 0;
    char * tlv_1st_head;

    type = *msg & 0x03;
    id = *msg >>2;

    srv_msg = proto_msg;
    srv_msg->id = id;
    srv_msg->type = type;
    tlv_1st_head = msg +1;

    BLOG_DEBUG("Got message id: %d type: %d\n", id, type);
#ifdef CLI_DEBUG
    char msg_dbg[256];
    int i =0;
    memcpy(msg_dbg, msg, msg_len);
    msg_dbg[msg_len] = 0x0;
    for (i=0;i<msg_len;i++){
        BLOG_DEBUG("|%02x",msg_dbg[i]);
    }
    BLOG_DEBUG("\n");
#endif
    switch (id) {
        case ID_HELLO:
            ret = parse_hello(tlv_1st_head, msg_len-1, srv_msg);
            break;
        case ID_KEEP_ALIVE:
            ret = parse_keepalive(tlv_1st_head, msg_len-1, srv_msg);
            break;
        case ID_BATTERY:
            ret = -1;
            break;
        case ID_BAUD_CHANGE:
            ret = parse_changebaudrate(tlv_1st_head, msg_len-1, srv_msg);
            break;
        case ID_BAUD_NEW:
            ret = parse_newbaudrate(tlv_1st_head, msg_len-1, srv_msg);
            break;
        case ID_LED_CONTROL:
            ret = parse_led(tlv_1st_head, msg_len-1, srv_msg);
            break;
        default:
            ret = -1;
            break;
    }
    if (ret == -1) {
        srv_msg->valid = 0;
    } else {
        srv_msg->valid = 1;
    }
    return ret;
}


static int assemble_tlv(mgmt_packet_t *mgm_packet, tlv_message_t * tlv)
{
    if ((tlv->length + mgm_packet->len) > MAX_MANAGEMENT_PACKET_BUFF_SIZE) {
        return -1;
    };
    mgm_packet->tlv_pdus[mgm_packet->tail++] = tlv->type;
    mgm_packet->tlv_pdus[mgm_packet->tail++] = tlv->length;
    memcpy(mgm_packet->tlv_pdus+mgm_packet->tail, tlv->data, tlv->length);
    mgm_packet->tail += tlv->length;
    mgm_packet->len += tlv->length+2;
    return 0;
}

int create_init_hello_request_message(mgmt_packet_t *mgm_packet)
{
    tlv_message_t tlv_msg;
    char header;
    int length = 0;
    //int i=0;
    int ret = 0;
    char label_info[MAX_MANAGEMENT_VERSION_LEN];

    // management header
    header = (MANAGEMENT_MESSAGE_ID_HELLO << MANAGEMENT_MESSAGE_ID_BIT_POS) |
                MANAGEMENT_MESSAGE_TYPE_REQ;
    mgm_packet->tlv_pdus[0] = header;
    mgm_packet->tail = 1;
    mgm_packet->len  = 1;

    // device name
    rdb_get_hw_model(label_info, &length);
    tlv_msg.type = MANAGEMENT_HELLO_TLV_DEVICE_NAME;
    tlv_msg.length = length;
    strcpy((char *)tlv_msg.data, label_info);
    ret = assemble_tlv(mgm_packet, &tlv_msg);

    // SW version
    rdb_get_sw_ver(label_info, &length);
    length = MIN(length, MAX_MANAGEMENT_VERSION_LEN);
    tlv_msg.type = MANAGEMENT_HELLO_TLV_SW_VERSION;
    tlv_msg.length = length;
    memcpy(tlv_msg.data, label_info, length);
    ret = assemble_tlv(mgm_packet, &tlv_msg);

    // HW version
    rdb_get_hw_ver(&label_info[0], &length);
    tlv_msg.type = MANAGEMENT_HELLO_TLV_HW_VERSION;
    tlv_msg.length = length;
    memcpy(tlv_msg.data, label_info, length);
    ret = assemble_tlv(mgm_packet, &tlv_msg);

    // Services
    tlv_msg.type = MANAGEMENT_HELLO_TLV_SERVICE_ID;
    tlv_msg.length = 1;
    tlv_msg.data[0] = SERVICE_ID_SOCKET;
    ret = assemble_tlv(mgm_packet, &tlv_msg);

#ifdef SUPPORT_BAUDRATE_CHANGE
    // Baudrates
    tlv_msg.type = MANAGEMENT_HELLO_TLV_BAUD_RATE;
    length = ARRAY_SIZE(supported_baud_rates);
    tlv_msg.length = 4 * length;
    for (i=0;i<length;i++) {
        memcpy ((tlv_msg.data+i*4), htonl(supported_baud_rates[i]), 4);
    }
    ret = assemble_tlv(mgm_packet, &tlv_msg);
#endif

    return ret;
}

int create_keepalive_request_message(mgmt_packet_t *mgm_packet)
{
    char header;

    // management header
    header = (MANAGEMENT_MESSAGE_ID_KEEP_ALIVE << MANAGEMENT_MESSAGE_ID_BIT_POS) |
                MANAGEMENT_MESSAGE_TYPE_REQ;
    mgm_packet->tlv_pdus[0] = header;
    mgm_packet->tail = 1;
    mgm_packet->len  = 1;
    return 0;
}

int create_battery_indication_message(mgmt_packet_t *mgm_packet)
{
    tlv_message_t tlv_msg;
    unsigned char value;
    char header;
    int ret=0;

    // management header
    header = (MANAGEMENT_MESSAGE_ID_BATTERY_STATUS << MANAGEMENT_MESSAGE_ID_BIT_POS) |
                MANAGEMENT_MESSAGE_TYPE_IND;
    mgm_packet->tlv_pdus[0] = header;
    mgm_packet->tail = 1;
    mgm_packet->len  = 1;

    // battery level
    value = rdb_get_battery_level();
    BLOG_DEBUG("battery level: %d\n",value);
    if (value != -1) {
        tlv_msg.type = MANAGEMENT_BATTERY_TLV_LEVEL;
        tlv_msg.length =1;
        tlv_msg.data[0]= value;
        ret = assemble_tlv(mgm_packet, &tlv_msg);
    } else {
        ret = -1;
    }

    // battery status
    value = rdb_get_battery_status();
    BLOG_DEBUG("battery status: %d\n",value);
    if (value != -1) {
        tlv_msg.type = MANAGEMENT_BATTERY_TLV_STATUS;
        tlv_msg.length =1;
        tlv_msg.data[0]= value;
        ret = assemble_tlv(mgm_packet, &tlv_msg);
    } else {
        ret = -1;
    }
    return ret;
}


int create_led_reponse_message(mgmt_packet_t *mgm_packet)
{
    tlv_message_t tlv_msg;
    char header;
    int ret =0;

    // management header
    header = (MANAGEMENT_MESSAGE_ID_LED_CONTROL << MANAGEMENT_MESSAGE_ID_BIT_POS) |
                MANAGEMENT_MESSAGE_TYPE_RESP;
    mgm_packet->tlv_pdus[0] = header;
    mgm_packet->tail = 1;
    mgm_packet->len  = 1;
    // led response always OK, right?
    tlv_msg.type = MANAGEMENT_TLV_RESPONSE_CODE_OK;
    tlv_msg.length = 1;
    tlv_msg.data[0] = 1; // why use this SERVICE_ID_SOCKET;
    ret = assemble_tlv(mgm_packet, &tlv_msg);
    return ret;
}

int create_change_baudrate_response_message(mgmt_packet_t *mgm_packet, unsigned char response)
{
    tlv_message_t tlv_msg;
    char header;
    int ret =0;

    // management header
    header = (MANAGEMENT_MESSAGE_ID_CHANGE_BAUD_RATE << MANAGEMENT_MESSAGE_ID_BIT_POS) |
                MANAGEMENT_MESSAGE_TYPE_RESP;
    mgm_packet->tlv_pdus[0] = header;
    mgm_packet->tail = 1;
    mgm_packet->len  = 1;

    // Response
    tlv_msg.type = MANAGEMENT_TLV_RESPONSE_CODE_ID;
    tlv_msg.length = 1;
    tlv_msg.data[0] = response;
    ret = assemble_tlv(mgm_packet, &tlv_msg);

    return ret;
}

int create_new_baudrate_reponse_message(mgmt_packet_t *mgm_packet)
{
    char header;

    // management header
    header = (MANAGEMENT_MESSAGE_ID_NEW_BAUD_RATE << MANAGEMENT_MESSAGE_ID_BIT_POS) |
                MANAGEMENT_MESSAGE_TYPE_RESP;
    mgm_packet->tlv_pdus[0] = header;
    mgm_packet->tail = 1;
    mgm_packet->len  = 1;
    return 0;
}

int is_hello_without_baudrate(hello_msg_t * msg)
{
    if ((msg->id_mask & (1 << MANAGEMENT_HELLO_TLV_RESPONSE)) == (1 << MANAGEMENT_HELLO_TLV_RESPONSE) &&
        (msg->id_mask & (1 << MANAGEMENT_HELLO_TLV_BAUD_RATE)) != (1 << MANAGEMENT_HELLO_TLV_BAUD_RATE)) {
            return 1;
    } else {
        return 0;
    }
}

int is_hello_with_baudrate(hello_msg_t * msg)
{
    if ((msg->id_mask & (1 << MANAGEMENT_HELLO_TLV_RESPONSE)) == (1 << MANAGEMENT_HELLO_TLV_RESPONSE) &&
        (msg->id_mask & (1 << MANAGEMENT_HELLO_TLV_BAUD_RATE)) == (1 << MANAGEMENT_HELLO_TLV_BAUD_RATE)) {
            return 1;
    } else {
        return 0;
    }
}
