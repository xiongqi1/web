/*
 * @file manage_app.c
 * @brief legacy OWA management app features
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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "manage_app.h"
#include "message_protocol.h"
#include "service_timer.h"
#include "com_interface.h"
#include "rdb_help.h"
#include "nc_util.h"

static state_event_t state_event = {.event=NO_OP};

static void send_msg_hello()
{
    mgmt_packet_t packet;
    create_init_hello_request_message(&packet);
    write_message(&packet);
    BLOG_DEBUG("Sent hello\n");
}

void send_msg_keepalive()
{
    mgmt_packet_t packet;
    create_keepalive_request_message(&packet);
    write_message(&packet);
    BLOG_DEBUG("Sent keepalive\n");
}

void send_msg_battery_ind()
{
    mgmt_packet_t packet;
    if (create_battery_indication_message(&packet)!=-1) {
        write_message(&packet);
        BLOG_DEBUG("Sent battery_ind\n");
    } else {
        BLOG_DEBUG("Battery info create error\n");
    }
    /* debug print battery message
    int i=0;
    for (i=0; i< packet.len;i++) {
        BLOG_DEBUG("|%02x", packet.tlv_pdus[i]);
    }
    */
}

static void do_led_control(led_msg_t *led_msg)
{
    int led_num = 0;
    int i = 0;
    led_ctrl_t led;
    led_num = led_msg->valid_led_num;
    for (i=0;i<led_num;i++){
        led = led_msg->led[i];
        rdb_set_led(led.id, led.color, led.flashing_interval);
    }
}

static void send_msg_led_control_response()
{
    mgmt_packet_t packet;
    create_led_reponse_message(&packet);
    write_message(&packet);
    BLOG_DEBUG("Sent led control response\n");
}

static void send_msg_change_baud_response()
{
    mgmt_packet_t packet;
    create_change_baudrate_response_message(&packet, MANAGEMENT_TLV_RESPONSE_CODE_OK);
    write_message(&packet);
    BLOG_DEBUG("Sent baud change response\n");
}

static void send_msg_new_baud_response()
{
    mgmt_packet_t packet;
    create_new_baudrate_reponse_message(&packet);
    write_message(&packet);
    BLOG_DEBUG("Sent new baud response\n");
}

/* baudrate seems like better to put in interface part, but here is use RDB to notice
   so put it app here
*/
static void do_baudrate_change(int baudrate)
{
    rdb_set_baudrate(baudrate);
}

static void do_baudrate_reset()
{
    rdb_set_baudrate(DEFAULT_BAUDRATE);
}

/* set related rdb to notify system the legacy owa device connection status
 *
 * @param status 1 -- connected, 3 -- disconnected
 */
static void notify_legacy_owa_connection_status(int status)
{
    rdb_set_legacy_owa_status(status);
}

static void do_disconnecting_cleanup()
{
    //do nothing so far
}

void service_state_run()
{
    static mgm_state_t running_state = INIT;
    static int retry_count = 0;  //use for exit
    int baudrate = 0;

    BLOG_DEBUG("Running State 1: %d \n", running_state);
    BLOG_DEBUG("State.event: %d \n", state_event.event);
    switch (running_state)
    {
    case INIT:
        //waiting connected;
    case CONNECTING:
        send_msg_hello();
        start_response_timer(GENERAL_RESPONSE_TIMEOUT_S);
        running_state = CONNECTING_WAITING;
        break;
    case CONNECTING_WAITING:
        if (state_event.event == HELLO_RESPONSE_NO_BAUDCHANGE)
        {
            start_battery_report_timer();
            start_keepalive_timer();
            start_response_timer(CONNECTION_LOST_TIME_S);
            notify_legacy_owa_connection_status(OWA_LEGACY_CONNECTED);
            running_state = CONNECTED;
        }
        else if (state_event.event == HELLO_RESPONSE_WITH_BAUDCHANGE)
        {
            running_state = BAUDRATE_CHANGING;
            start_response_timer(CONNECTION_LOST_TIME_S);
        }
        else if (state_event.event == RESPONSE_TIMEOUT)
        {
            running_state = CONNECTING;
            start_response_timer(GENERAL_RESPONSE_TIMEOUT_S);
            if (retry_count++ >= MAX_TRY_NUM_BEFORE_EXIT) {
                notify_legacy_owa_connection_status(OWA_LEGACY_DISCONNECTED);
            }
        }
        break;

    case CONNECTED:
        if (state_event.event == LED_CONTROL_REQUEST)
        {
            stop_response_timer();
            do_led_control((led_msg_t*)state_event.param);
            send_msg_led_control_response();
            start_response_timer(CONNECTION_LOST_TIME_S);
        }
        if (state_event.event == LED_CONTROL_INDICATION)
        {
            stop_response_timer();
            do_led_control((led_msg_t*)state_event.param);
            start_response_timer(CONNECTION_LOST_TIME_S);
        }
        if (state_event.event == KEEP_ALIVE_RESPONSE)
        {
            stop_response_timer();
            start_response_timer(CONNECTION_LOST_TIME_S);
        }
        if (state_event.event == RESPONSE_TIMEOUT)
        {
            running_state = DISCONNECTED;
            notify_legacy_owa_connection_status(OWA_LEGACY_DISCONNECTED);
            stop_all_timers();
        }
        break;
    case BAUDRATE_CHANGING:
        if (state_event.event == CHANGE_BAUDRATE_REQ)
        {
            send_msg_change_baud_response();
            start_response_timer(CONNECTION_LOST_TIME_S);
            running_state = BAUDRATE_CHANGE;
        }
        else if (state_event.event == RESPONSE_TIMEOUT)
        {
            notify_legacy_owa_connection_status(OWA_LEGACY_CONNECTED);
            running_state = CONNECTED;
        }
        break;
    case BAUDRATE_CHANGE:
        if (state_event.event == NEW_BAUDRATE_REQ)
        {
            stop_response_timer();  //the changing baudrate needs longer timer??
            send_msg_new_baud_response();
            memcpy(&baudrate, state_event.param, 4);
            do_baudrate_change(baudrate);
            running_state = BAUDRATE_VALIDATE;
        }
        else if (state_event.event == RESPONSE_TIMEOUT)
        {
            stop_response_timer();
            do_baudrate_reset();
            notify_legacy_owa_connection_status(OWA_LEGACY_CONNECTED);
            running_state = CONNECTED;
        }
        break;
    case BAUDRATE_VALIDATE:
        if (state_event.event == NEW_BAUDRATE_IND)
        {
            running_state = CONNECTED;
        }
        else if (state_event.event == RESPONSE_TIMEOUT)
        {
            do_baudrate_reset();
            notify_legacy_owa_connection_status(OWA_LEGACY_CONNECTED);
            running_state = CONNECTED;
        }
        break;
    case DISCONNECTED:
        stop_all_timers();
        do_disconnecting_cleanup();
        break;
    default:
        running_state = INIT;
        break;
    }
    state_event.event = NO_OP;
    BLOG_DEBUG("Running State 2: %d \n", running_state);
}

int set_state_event(state_event_t evt)
{
    int ret = 0;
    static int in_set = 0;
    if (in_set == 0)
    {
        in_set = 1;
        memcpy((char *)&state_event, (char *)&evt, sizeof(state_event_t));
        in_set = 0;
    }
    else
    { //in case someone working on set at same time
        ret = -1;
        BLOG_DEBUG("set event suffers race condition");
    }
    return ret;
}

static int translate_msg_to_event(service_protocol_t *msg, state_event_t *evt)
{
    int ret = 0;
    if (!msg->valid)
    {
        return -1;
    }
    switch (msg->id)
    {
    case MANAGEMENT_MESSAGE_ID_HELLO:
        if (is_hello_without_baudrate(&msg->msg_data.hello_msg))
        {
            evt->event = HELLO_RESPONSE_NO_BAUDCHANGE;
        };
        if (is_hello_with_baudrate(&msg->msg_data.hello_msg))
        {
            evt->event = HELLO_RESPONSE_WITH_BAUDCHANGE;
        };
        break;
        //TODO: for authentication message
    case MANAGEMENT_MESSAGE_ID_CHANGE_BAUD_RATE:
        if (msg->type == RESPONSE) {
            evt->event = CHANGE_BAUDRATE_REQ;
            memcpy(evt->param, &msg->msg_data.changebaudrate_msg.baudrate, sizeof(int));
        }
        break;
    case MANAGEMENT_MESSAGE_ID_KEEP_ALIVE:
        if (msg->type == RESPONSE){
            evt->event = KEEP_ALIVE_RESPONSE;
        }
        break;
    case MANAGEMENT_MESSAGE_ID_LED_CONTROL:
        if (msg->type == REQUEST) {
            evt->event = LED_CONTROL_REQUEST;
        } else if (msg->type == INDICATION) {
            evt->event = LED_CONTROL_INDICATION;
        }
        memcpy(evt->param, (char *)&msg->msg_data.led_msg, sizeof(led_msg_t));
        break;
    default:
        break;
    }
    return ret;
}

void process_message(char *msg, int msg_len)
{
    state_event_t evt;
    int status;
    service_protocol_t srv_msg;

    memset(&srv_msg, 0, sizeof(srv_msg));
    status = parse_message(msg, msg_len, &srv_msg);

    if (status == -1)
    {
        BLOG_DEBUG("parse msg error\n");
        return;
    }
    status = translate_msg_to_event(&srv_msg, &evt);
    if (status == -1)
    {
        BLOG_DEBUG("translate to event\n");
        return;
    }
    if (status != -1) set_state_event(evt);
}
