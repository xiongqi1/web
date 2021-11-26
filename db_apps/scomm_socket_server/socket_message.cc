/**
 * @file socket_message.cc
 * @brief Implement scomm socket message encoding and decoding
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

#include <string.h>
#include "socket_message.h"
#include "utils.h"

SocketMessage::SocketMessage(uint8_t message_type, uint8_t session_type, uint8_t session_id, uint8_t* buffer, int len):
m_message_type(message_type), m_session_type(session_type), m_session_id(session_id)
{

    m_length = len + 1;
    m_buffer = new uint8_t[m_length];

    uint8_t& header = m_buffer[0];
    header = (message_type & SOCKET_MESSAGE_TYPE_BIT_MASK) <<
            SOCKET_MESSAGE_TYPE_BIT_POS;
    header |= (session_type & SOCKET_MESSAGE_CHANNEL_TYPE_BIT_MASK) <<
            SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS;
    header |= (session_id & SOCKET_MESSAGE_CHANNEL_ID_BIT_MASK) <<
            SOCKET_MESSAGE_CHANNEL_ID_BIT_POS;

    if (buffer) {
        memcpy(m_buffer + 1, buffer, len);
    }
}

SocketMessage::SocketMessage(uint8_t* buffer, int len)
{
    m_length = len;
    m_buffer = new uint8_t[m_length];
    memcpy(m_buffer, buffer, m_length);

    uint8_t header =  m_buffer[0];
    m_message_type = (header >> SOCKET_MESSAGE_TYPE_BIT_POS) &
                    SOCKET_MESSAGE_TYPE_BIT_MASK;
    m_session_type = (header >> SOCKET_MESSAGE_CHANNEL_TYPE_BIT_POS) &
                    SOCKET_MESSAGE_CHANNEL_TYPE_BIT_MASK;
    m_session_id = (header >> SOCKET_MESSAGE_CHANNEL_ID_BIT_POS) &
                    SOCKET_MESSAGE_CHANNEL_ID_BIT_MASK;
}

SocketMessage* SocketMessage::Decode(uint8_t* buffer, int len)
{
    uint8_t header =  buffer[0];
    uint8_t type = (header >> SOCKET_MESSAGE_TYPE_BIT_POS) & SOCKET_MESSAGE_TYPE_BIT_MASK;
    if (SOCKET_MESSAGE_TYPE_DATA == type) {
        return new SocketDataMessage(buffer, len);
    }
    else {
        return new SocketControlMessage(buffer, len);
    }
}

SocketDataMessage::SocketDataMessage(uint8_t session_id, uint8_t* buffer, int len, uint8_t session_type)
:SocketMessage(SOCKET_MESSAGE_TYPE_DATA, session_type, session_id, buffer, len)
{
    m_tcp_data = m_buffer + 1;
    m_tcp_data_length = len;
}

SocketDataMessage::SocketDataMessage(uint8_t* buffer, int len)
:SocketMessage(buffer, len)
{
    m_tcp_data = m_buffer + 1;
    m_tcp_data_length = len - 1;
}

SocketControlMessage::SocketControlMessage(uint8_t session_id, uint8_t command_type, uint8_t command_id,
                                           uint8_t* buffer, int len, uint8_t session_type)
:SocketMessage(SOCKET_MESSAGE_TYPE_CONTROL, session_type, session_id, nullptr, len + 1),
m_command_type(command_type), m_command_id(command_id), m_payload(nullptr), m_payload_length(0)
{
    uint8_t& header = m_buffer[1];
    header = (command_type & SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_BIT_MASK) <<
                                    SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_BIT_POS;

    header |= (command_id & SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_MASK) <<
                                    SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS;

    if (len && buffer) {
        memcpy(m_buffer + 2, buffer, len);
        m_payload = m_buffer + 2;
        m_payload_length = len;
    }
}

SocketControlMessage::SocketControlMessage(uint8_t* buffer, int len)
:SocketMessage(buffer, len)
{
    uint8_t& header = m_buffer[1];
    m_command_type = (header >> SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_BIT_POS) &
                    SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_BIT_MASK;
    m_command_id = (header >> SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_POS) &
                    SOCKET_MESSAGE_CONTROL_COMMAND_ID_BIT_MASK;

    BLOG_DEBUG("command type/id: %d/%d\n", m_command_type, m_command_id);

    m_payload_length = len - 2;
    if (m_payload_length) {
        m_payload = m_buffer + 2;
    }
    else {
        m_payload = nullptr;
    }
}
