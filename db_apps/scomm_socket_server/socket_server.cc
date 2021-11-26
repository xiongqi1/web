/**
 * @file socket_server.cc
 * @brief Implement Socket listener, connection, and manager
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

#include "socket_server.h"
#include "utils.h"
#include "socket_message.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <set>

#define SOCKET_READ_SIZE        250

void SocketListener::Start(uint16_t port)
{
    if (-1 != m_fd) {
        BLOG_ERR("socket listener has started\n");
        return;
    }

    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == m_fd) {
        BLOG_ERR("could not open socket %d\n", errno);
        return;
    }

    int enable = 1;
    if (setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        BLOG_ERR("could not set socket option %d\n", errno);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(m_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
        BLOG_ERR("could not bind on port %d %d\n", (int)port, errno);
        return;
    }

    if (listen(m_fd, MAX_SOCKET_CHANNEL)) {
        BLOG_ERR("could not listen on socket %d %d\n", m_fd, errno);
    }

    m_port = port;
    BLOG_NOTICE("listening on port %d\n", (int)port);
}

int SocketListener::OnEvent(uint16_t event)
{
    BLOG_DEBUG("listener event %d\n", event);
    if (!(POLLIN & event)) {
        return -1;
    }

    int socket = accept(m_fd, NULL, 0);
    if (-1 == socket) {
        BLOG_ERR("could not accept socket %d\n", errno);
    }

    BLOG_NOTICE("new socket accepted %d\n", socket);

    return socket;
}

TCPSocketConnection::TCPSocketConnection(int fd, uint8_t id, uint16_t port, FileBuffer* scomm)
: m_fd(fd), m_id(id), m_port(port), m_status(SS_Opening),
m_buffer(new FileBuffer(fd)), m_scomm(scomm)
{
    port = htons(port);
    SocketControlMessage msg(id, SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_REQ,
                             SOCKET_MESSAGE_CONTROL_COMMAND_ID_OPEN_SOCKET,
                            (uint8_t*)&port, sizeof(port));
    m_scomm->Buffer(msg.m_buffer, msg.m_length, true);
    BLOG_DEBUG("new connection object constructed\n");
}

void TCPSocketConnection::OnEvent(uint16_t event)
{
    BLOG_DEBUG("socket event/status %d/%d\n", event, (int)m_status);

    if (((POLLERR & event) || (POLLHUP & event)) && m_status == SS_Open) {
        SocketControlMessage msg(m_id, SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_REQ,
                            SOCKET_MESSAGE_CONTROL_COMMAND_ID_CLOSE_SOCKET,
                            nullptr, 0);
        m_scomm->Buffer(msg.m_buffer, msg.m_length, true);
        m_status = SS_Closing;
        return;
    }

     if ((POLLIN & event) && m_status == SS_Open) {
        uint8_t buf[SOCKET_READ_SIZE];
        int count = read(m_fd, buf, SOCKET_READ_SIZE);
        BLOG_INFO("socket(%d) data in size %d\n", m_fd, count);

        if (count > 0) {
            SocketDataMessage msg(m_id, buf, count);
            m_scomm->Buffer(msg.m_buffer, msg.m_length, true);
        }
        else {
            if (-1 == count) {
                BLOG_ERR("socket read error %d %d\n", m_fd, errno);
            }

            SocketControlMessage msg(m_id, SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_REQ,
                                SOCKET_MESSAGE_CONTROL_COMMAND_ID_CLOSE_SOCKET,
                                nullptr, 0);
            m_scomm->Buffer(msg.m_buffer, msg.m_length, true);
            m_status = SS_Closing;
            return;
        }
    }

    if ((POLLOUT & event) && m_status == SS_Open) {
        m_buffer->OnWrite(event);
    }
}

void TCPSocketConnection::OnMessage(const SocketMessage& msg)
{
    BLOG_DEBUG("socket message type %d, session type %d, status: %d\n", msg.m_message_type,
               msg.m_session_type, (int)m_status);

    // for data message
    if (m_status == SS_Open && msg.m_message_type == SOCKET_MESSAGE_TYPE_DATA &&
        msg.m_session_type == SOCKET_MESSAGE_CHANNEL_TYPE_TCP) {
        SocketDataMessage* pMessage = (SocketDataMessage*)&msg;
        m_buffer->Buffer(pMessage->m_tcp_data, pMessage->m_tcp_data_length);
        BLOG_DEBUG("tcp data recieved %d\n", pMessage->m_tcp_data_length);

        uint8_t avial = MAX_ACCEPTABLE_RX_PACKETS;
        SocketControlMessage msg(m_id, SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND,
            SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL, &avial, sizeof(avial));
        m_scomm->Buffer(msg.m_buffer, msg.m_length, true);

        return;
    }

    // for control message
    if (msg.m_message_type == SOCKET_MESSAGE_TYPE_CONTROL &&
        msg.m_session_type == SOCKET_MESSAGE_CHANNEL_TYPE_TCP) {

        SocketControlMessage* pMessage = (SocketControlMessage*)&msg;
        BLOG_INFO("tcp control message recieved, channel %d\n", m_id);
        BLOG_DEBUG("socket command type %d, command id %d, status: %d\n", pMessage->m_command_type,
                   pMessage->m_command_id, (int)m_status);

        if (m_status == SS_Opening &&
           pMessage->m_command_type == SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_RESP &&
           pMessage->m_command_id == SOCKET_MESSAGE_CONTROL_COMMAND_ID_OPEN_SOCKET) {
            m_status = SS_Open;
            BLOG_INFO("new tcp socket channel open, channel %d\n", m_id);
        }
        else if ((m_status == SS_Closing || m_status == SS_Open) &&
           (pMessage->m_command_type == SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_RESP ||
            pMessage->m_command_type == SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND) &&
            pMessage->m_command_id == SOCKET_MESSAGE_CONTROL_COMMAND_ID_CLOSE_SOCKET) {
            m_status = SS_Closed;
            BLOG_INFO("tcp socket closed, channel %d\n", m_id);
        }
        else if (m_status == SS_Open &&
            pMessage->m_command_type == SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_REQ &&
            pMessage->m_command_id == SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL) {

            uint8_t avial = MAX_ACCEPTABLE_RX_PACKETS;
            SocketControlMessage msg(m_id, SOCKET_MESSAGE_CONTROL_COMMAND_TYPE_IND,
                SOCKET_MESSAGE_CONTROL_COMMAND_ID_BUFFER_AVAIL, &avial, sizeof(avial));
            m_scomm->Buffer(msg.m_buffer, msg.m_length, true);
            BLOG_INFO("buffer size request responded\n");
        }
    }
}

int SocketServer::RegisterEvents(pollfd* fds, int len)
{
    int registered = 0;
    // don't make the legacy product too busy, if there is an pending open/close
    // we stop processing anything on the socket to serialize open/close processes.
    if (HasHalfOpenCloseChannel()) {
        return registered;
    }

    for(auto& ic: m_connections) {
        if (registered >= len) {
            break;
        }

        if (ic.second->m_status == SS_Open) {
            fds->events = POLLIN | POLLHUP | POLLERR;
                BLOG_DEBUG("register for socket reading and hanging up %d\n", ic.second->m_fd);
            if (!ic.second->m_buffer->IsEmpty()) {
                fds->events |= POLLOUT;
                BLOG_DEBUG("register for socket writting %d\n", ic.second->m_fd);
            }
        }
        else {
            continue;
        }

        fds->fd = ic.second->m_fd;
        registered++;
        fds++;
    }

    return registered;
}

void SocketServer::ProcessEvents(pollfd* fds, int len, FileBuffer& scomm) {
    BLOG_DEBUG("to process socket events\n");

    uint16_t server_socket_events = fds[POLL_SERVER_SOCKET_POS].revents;

    // processs hang up events
    BLOG_DEBUG("to process error events\n");
    for(int i=POLL_NONE_SOCKET_CONN; i<len; i++) {
        if (fds[i].revents & POLLHUP || fds[i].revents & POLLERR) {
            auto it = m_connections.find(fds[i].fd);
            if (it == m_connections.end()) {
                continue;
            }
            uint16_t event = fds[i].revents & (POLLHUP | POLLERR);
            if (event) {
                it->second->OnEvent(event);
            }

            // can't start/close two connections at the same time
            // otherwise the legacy product might ignore the second open/close request.
            if (HasHalfOpenCloseChannel()) {
                return;
            }

            fds[i].revents &= ~(POLLHUP | POLLERR);
        }
    }

    // accept a new connection
    if (server_socket_events & POLLIN && m_connections.size() < MAX_SOCKET_CHANNEL) {
        BLOG_DEBUG("to accept a new connection\n");
        int socket = m_listener.OnEvent(POLLIN);
        if (-1 != socket) {
            if (MAX_SOCKET_CHANNEL == m_connections.size()) {
                close(socket);
                BLOG_NOTICE("discard a connection - no more free channel\n");
            }
            else {

                // find a free channel id
                std::set<uint8_t> channels;
                for(uint8_t i=0; i<MAX_SOCKET_CHANNEL; i++) {
                    channels.insert(i);
                }

                for(auto& ic: m_connections) {
                    channels.erase(channels.find(ic.second->m_id));
                }
                uint8_t channel = *channels.begin();

                // add a new connection
                m_connections[socket] = new TCPSocketConnection(socket, channel,
                                                m_listener.GetPort(), &scomm);
                m_channels[channel] = socket;

                BLOG_INFO("new connection accepted %d %d\n", socket, channel);
                // can't start/close two connections at the same time
                // otherwise the legacy product might ignore the second open/close request.
                return;
            }
        }
    }

    BLOG_DEBUG("to process socket in/out events\n");
     // check all IN/OUT events
    for(int i=POLL_NONE_SOCKET_CONN; i<len; i++) {
        if (fds[i].revents & (POLLIN | POLLOUT)) {
            auto it = m_connections.find(fds[i].fd);
            if (it == m_connections.end()) {
                continue;
            }
            it->second->OnEvent(fds[i].revents);
            fds[i].revents &= ~(POLLIN | POLLOUT);

            // can't close two connections in the same time
            // otherwise the legacy product might ignore the second open request.
            if (HasHalfOpenCloseChannel()) {
                return;
            }
        }
    }

    BLOG_DEBUG("all sockets event processing is done\n");
}

void SocketServer::OnMessage(const SocketMessage& msg)
{
    // find fd from channel id
    auto ic = m_channels.find(msg.m_session_id);
    if (ic == m_channels.end()) {
        BLOG_WARNING("message discarded on channel %d\n", msg.m_session_id);
        return;
    }

    // find connection from fd
    auto it = m_connections.find(ic->second);
    if (it == m_connections.end()) {
        BLOG_WARNING("message discarded on channel %d\n", msg.m_session_id);
        return;
    }

    // process the message for this connection
    it->second->OnMessage(msg);

    // remove a connection if it's closed
    if (it->second->m_status == SS_Closed) {
        delete it->second;
        m_connections.erase(it);
        m_channels.erase(ic);
        BLOG_INFO("connection closed %d\n", msg.m_session_id);
    }
}

bool SocketServer::HasHalfOpenCloseChannel()
{
    // Subsequent connections could not be be established if one
    // of a request/response message is discarded due to CRC error.
    /*
    for(auto &ic: m_connections) {
        if (ic.second->m_status == SS_Opening ||
            ic.second->m_status == SS_Closing) {
            BLOG_INFO("half open/close channel found %d %d %d\n",
                   ic.first, ic.second->m_id, ic.second->m_status);
            return true;
        }
    }
    */

    return false;
}
