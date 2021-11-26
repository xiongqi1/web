/**
 * @file socket_message.h
 * @brief scomm socket message encoding and decoding
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
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

#ifndef SOCKET_MESSAGE_H_10253127082019
#define SOCKET_MESSAGE_H_10253127082019

#include <stdint.h>

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

#define SOCKET_SESSION_TYPE_BIT_MASK 0x3
#define SOCKET_SESSION_TYPE_BIT_POS 2

#define MIN_SOCKET_ALLOWED_PACKETS_NUM 1
#define MAX_ACCEPTABLE_RX_PACKETS 3

struct SocketMessage {
    /**
    * Encoding constructor
    *
    * @param    message_type    Message type
    * @param    session_type    Session type
    * @param    session_id      Session/channel id
    * @param    buffer          Payload buffer
    * @param    len             Payload buffer length
    *
    */
    SocketMessage(uint8_t message_type, uint8_t session_type,
                  uint8_t session_id, uint8_t* buffer, int len);

    /**
    * Decoding  constructor
    *
    * @param    buffer      Message buffer
    * @param    len         Message buffer length
    *
    */
    SocketMessage(uint8_t* buffer, int len);

    /**
    * Decoding a message from buffer
    *
    * @param    buffer      Message buffer
    * @param    len         Message buffer length
    * @return   A pointer a socket message object
    *
    */
    static SocketMessage* Decode(uint8_t* buffer, int len);

    /**
    * Virtual destructor to be sure memory is released correctly.
    *
    */
     virtual ~SocketMessage() {
        delete[] m_buffer;
    }

    uint8_t    m_message_type;     // Message type(Data/Control)
    uint8_t    m_session_type;     // Session/channel type(TCP/UDP)
    uint8_t    m_session_id;       // Session/channle id

    uint8_t*   m_buffer;           // Message buffer
    int     m_length;           // Message buffer length
};

struct SocketDataMessage : public SocketMessage {
    /**
    * Encoding constructor
    *
    * @param    session_id      Session/Channel id
    * @param    buffer          Data buffer
    * @param    len             Data buffer length
    * @param    session_type    Session type
    *
    */
    SocketDataMessage(uint8_t session_id, uint8_t* buffer, int len,
                      uint8_t session_type = SOCKET_MESSAGE_CHANNEL_TYPE_TCP);

    /**
    * Decoding  constructor
    *
    * @param    buffer      Message buffer
    * @param    len         Message buffer length
    *
    */
    SocketDataMessage(uint8_t* buffer, int len);


    uint8_t*   m_tcp_data;           // TCP socket data
    int     m_tcp_data_length;    // TCP socket data length
};

struct SocketControlMessage : public SocketMessage {
    /**
    * Encoding constructor
    *
    * @param    session_id      Session/Channel id
    * @param    command_type    Command type
    * @param    command_id      Command id
    * @param    buffer          Payload buffer
    * @param    len             Payload length
    * @param    session_type    Session type
    *
    */
    SocketControlMessage(uint8_t session_id, uint8_t command_type, uint8_t command_id,
                         uint8_t* buffer, int len,
                         uint8_t session_type = SOCKET_MESSAGE_CHANNEL_TYPE_TCP);

    /**
    * Decoding  constructor
    *
    * @param    buffer      Message buffer
    * @param    len         Message buffer length
    *
    */
    SocketControlMessage(uint8_t* buffer, int len);

    uint8_t    m_command_type;       // Command type(Request, Response, Indication)
    uint8_t    m_command_id;         // Command id(Open, Close, Buffer available)

    uint8_t*   m_payload;           // Paylaod
    int     m_payload_length;    // Payload length
};

#endif // SOCKET_MESSAGE_H_10253127082019
