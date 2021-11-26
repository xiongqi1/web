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

#ifndef MANAGE_APP_H_11020009122019
#define MANAGE_APP_H_11020009122019

#define DEFAULT_BAUDRATE  115200
#define GENERAL_RESPONSE_TIMEOUT_S 10
#define MAX_TIME_BEFORE_EXIT_S (10*60)
#define MAX_TRY_NUM_BEFORE_EXIT  (MAX_TIME_BEFORE_EXIT_S/GENERAL_RESPONSE_TIMEOUT_S)
#define OWA_LEGACY_CONNECTED 1
#define OWA_LEGACY_DISCONNECTED 3
#define OWA_LEGACY_DETECTION_TIMEOUT 4


typedef enum {
    NO_OP,
    BATT_REP_TIMER_EXPIRE,
    KEEP_ALIVE_TIMER_EXPIRE,
    CONN_LOST_TIMER_EXPIRE,
    RESPONSE_TIMEOUT,
    HELLO_RESPONSE_NO_BAUDCHANGE,
    HELLO_RESPONSE_WITH_BAUDCHANGE,
    KEEP_ALIVE_RESPONSE,
    BAUDRATE_CHANGE_EFFECTED,
    CHANGE_BAUDRATE_REQ,
    CHANGE_BAUDRATE_RESP,
    NEW_BAUDRATE_REQ,
    NEW_BAUDRATE_RESP,
    NEW_BAUDRATE_IND,
    LED_CONTROL_REQUEST,
    LED_CONTROL_INDICATION
} mgm_event_t;

typedef enum mgm_state {
    INIT,
    CONNECTING,
    CONNECTING_WAITING,
    BAUDRATE_CHANGING,
    BAUDRATE_CHANGE,
    BAUDRATE_VALIDATE,
    CONNECTED,
    DISCONNECTED
} mgm_state_t;

typedef struct {
    mgm_event_t event;
    char param[256];
} state_event_t;

/*
 * main app state machine to implement all logics
 * which required by requirement
 */
void service_state_run();

/* set global state event variable for state machine
 *
 * @param evt state_event_t type variable to store different type of event and event parameter
 * @return 0 on success, none-zero error code for failure
 */
int set_state_event(state_event_t evt);

/* interpret received message from interface and translate into related event
 *
 * @param msg pointer to received message buffer
 * @param msg_len message length
 */
void process_message(char *msg, int msg_len);

/*
 * get battery information and send it through interface
 */
void send_msg_battery_ind();

/*
 * create keep alive message and send it through interface
 */
void send_msg_keepalive();
#endif
