/**
 * @file socket_server.h
 * @brief Socket listener, connection, and manager
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

#ifndef SOCKET_SERVER_H_10253127082019
#define SOCKET_SERVER_H_10253127082019
#include "file_buffer.h"
#include "socket_message.h"

#include <map>
#include <poll.h>
#include <stdint.h>

#define MAX_SOCKET_CHANNEL          16
#define POLL_FIFO_IN_POS            0
#define POLL_FIFO_OUT_POS           1
#define POLL_SERVER_SOCKET_POS      2
#define POLL_NONE_SOCKET_CONN       3
#define POLL_SIZE                   (POLL_NONE_SOCKET_CONN + \
                                    MAX_SOCKET_CHANNEL)

/**
 * Socket listner class
 *
 * Listen on TCP port and accept new connections
 */

class SocketListener {
public:
    /**
     * Constructor
     *
     */
    SocketListener()
    : m_fd(-1), m_port(-1) {
    }

    /**
     * Destructor
     *
     */
     ~SocketListener() {
     }

public:
    /**
     * Start listening on the requested port
     *
     * @param   port        Port number to listen on
     */
    void Start(uint16_t port);

    /**
     * Process event to accept new socket connection
     *
     * @param       event       Returned poll events of respected server socket file
     * @return      New socket file descriptor or -1 indicating no new socket accepted
     */
    int OnEvent(uint16_t event);

    /**
     * Get underlying socket file descriptor
     *
     * @return      The underlying socket file descriptor
     */
    int GetFd() {
        return m_fd;
    }

    /**
     * Get the port number that it listens on
     *
     * @return      The port number that listens on
     */
    int GetPort() {
        return m_port;
    }

friend class SocketServer;

protected:
    int     m_fd;       // Server socket fd
    uint16_t   m_port;     // Server port number
};

/**
 * Socket connection status
 *
 */
enum SocketStatus {
    SS_Opening,
    SS_Open,
    SS_Closing,
    SS_Closed
};

/**
 * A class manages TCP socket connections
 *
 */
class TCPSocketConnection {
public:

    /**
     * Constructor
     *
     * @param       fd      Underlying socket fd
     * @param       id      SCOMM socket channel id
     * @param       port    Socket local port number
     * @param       scomm   SCOMM file buffer
     */
    TCPSocketConnection(int fd = -1, uint8_t id = -1,
                     uint16_t port = -1, FileBuffer* scomm = nullptr);

    /**
     * Destructor
     *
     */
    ~TCPSocketConnection() {
        delete m_buffer;
    }

public:
    /**
     * Process socket polling events
     *
     * @param       event       Returned polling event
     */
    void OnEvent(uint16_t event);

    /**
     * Process a SCOMM message
     *
     * @param       msg         SCOMM message
     */
    void OnMessage(const SocketMessage& msg);


friend class SocketServer;

protected:
    int             m_fd;           // Socket fd
    uint8_t            m_id;           // SCOMM session/socket channel id
    uint16_t           m_port;         // Socket local port number
    SocketStatus    m_status;       // SCOMM socket channel status
    FileBuffer*     m_buffer;       // Socket side buffer
    FileBuffer*     m_scomm;        // SCOMM file buffer
};

struct SocketServer {

    /**
     * Register poll events
     *
     * @param       fds         Pointer to fd array
     * @prarm       len         Array length
     */
    int RegisterEvents(pollfd* fds, int len);

    /**
     * Process fd events
     *
     * @param       fds         Pointer to fd array
     * @param       len         Array length
     * @param       scomm       SCOMM file buffer
     */
    void ProcessEvents(pollfd* fds, int len, FileBuffer& scomm);

    /**
     * Process a SCOMM message
     *
     * @param       msg         SCOMM message
     * @param       scomm       SCOMM file buffer
     */
    void OnMessage(const SocketMessage& msg);

    /**
     * Check if any open/close request in progress
     *
     */
    bool HasHalfOpenCloseChannel();

    // Socket listener
    SocketListener                       m_listener;
    // Mapping from an fd to a socket connection.
    std::map<int, TCPSocketConnection*>  m_connections;
    // Mapping from a channel to a socket
    std::map<int, int>                   m_channels;
};

#endif //SOCKET_SERVER_H_10253127082019
