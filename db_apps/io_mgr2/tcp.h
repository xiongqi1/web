#ifndef __CDCS_TCP_H__16062016
#define __CDCS_TCP_H__16062016

/*
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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
 *
 *  TCP listener and server client classes
 */

#include <cdcs_fileselector.h>

/*
 * This class provides functionality for a TCP connection
 * The TCP listener constructs this class when a connection is received
 */
class TcpServer {
public:
    TcpServer(FileSelector *pFS_):
    pFS(pFS_),
    sock(-1) {}

    virtual ~TcpServer();

    int getFd() { return sock; }
    void onConnect(int sock);
    virtual void onConnected() = 0;
    virtual void onDisconnected(){}
    void onRead();
    virtual void OnDataRead(char *buf,int len){}
private:
    FileSelector * pFS;
    int sock;
    void disconnect();
};

// This class provides listener functionality
class TcpListener {
public:
    static bool isValidAddress(const char *address);
    TcpListener();
    ~TcpListener();
    bool listen(const char *address, int port, FileSelector *pFS);
    virtual void onConnect() = 0;
    FileSelector *getFS() { return pFS; }
    int getSock() { return sock; }
private:
    int sock;
    FileSelector *pFS;
};

#endif
