/*
 *
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
 *  TCP listener and server classes
 */

#include "tcp.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static const int listenQueLen = 4;

TcpServer::~TcpServer()
{
	disconnect();
}

// This is called when an error or event indicates a
// loss of network connection
void TcpServer::disconnect()
{
	if (sock >= 0) {
		onDisconnected(); // Call the sub class so it knows
		pFS->removeMonitor(sock); // No need to monitor any more
		close(sock);
		sock = -1;
	}
}

// This is called by the file selector when data or event is available
void TcpServer::onRead()
{
	char buf[1024];
	int len=read(sock,buf,sizeof(buf)-1); // size is one less so we can add null
	if(len == 0) {
//		DBG(LOG_INFO,"socket input disconnected (EOF)");
		disconnect();
		return;
	}

	/* bypass if any error */
	if(len < 0) {
//		DBG(LOG_INFO,"socket input disconnected (read failure) - %s",strerror(errno));
		disconnect();
		return;
	}
	buf[len] = 0; // add a null to terminate the string
	OnDataRead(buf,len); // Call the sub class with the data
}

// This is called by the file selector when data or event is available
// Use the param to point to the class
static void onRead(void * param, void *pFm)
{
	TcpServer * pTc = (TcpServer *)param;
	pTc->onRead();
}

// This listener calls this when a connection is made
void TcpServer::onConnect(int acceptSock)
{
	/* accept */
	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);
	sock = accept(acceptSock, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
	if(sock < 0) {
//		DBG(LOG_ERR,"failed in accept() - %s",strerror(errno));
		return;
	}

	/* set non-block access */
	int flags = fcntl(sock, F_GETFL);
	if(fcntl(sock, F_SETFL, flags | O_NONBLOCK | O_CLOEXEC) < 0) {
//		DBG(LOG_ERR,"failed in fcntl() - %s",strerror(errno));
		disconnect();
		return;
	}
	pFS->addMonitor(sock, ::onRead, this);
	onConnected(); // Call the sub class with the event
}

// The address must be in IPv4 numbers-and-dots notation
bool TcpListener::isValidAddress(const char *address)
{
	/* convert str to in_addr */
	struct in_addr sin_addr;
	if(!inet_aton(address,&sin_addr)) {
		return false;
	}
	return true;
}

// constructor just initializes some members
// main work is done in listen()
TcpListener::TcpListener():
	sock(-1),
	pFS(0)
{
}

// destructor removes the file monitor and closes the socket
TcpListener::~TcpListener()
{
	if ( sock >= 0 ) {
		if (pFS) {
			pFS->removeMonitor(sock);
		}
		close(sock);
	}
}

// FileSector calls here when a connection is made.
// we just call the sub class as only it knows what client to create
// to handle the connection

static void processConnect(void *param, void *pFm)
{
	TcpListener * pThis = (TcpListener *)param;
	pThis->onConnect();
}

// This is the main worker routine for the listener
// setup a TCP listen by opening a socket and binding
// The file selector is used to to wait for a connection
bool TcpListener::listen(const char *address, int port, FileSelector *pFS_)
{
//	DBG(LOG_DEBUG,"%s:%d", address, port);
	struct sockaddr_in serv_addr;
	/* bind to a port */
	memset(&serv_addr, 0, sizeof(serv_addr));

	/* convert str to in_addr */
	if(!inet_aton(address, &serv_addr.sin_addr)) {
		return false;
	}

	/* create a unnamed socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0) {
//		DBG(LOG_ERR,"failed to create a unnamed socket for listening - %s", strerror(errno));
		return false;
	}
	/* set socket option - SO_REUSEADDR */
	int reuse=1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	if(bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
//		DBG(LOG_ERR,"failed to bind to port (port=%d) - %s", port, strerror(errno));
		return false;
	}

	/* create a listen socket */
	if(::listen(sock, listenQueLen) < 0) {
//		DBG(LOG_ERR,"failed to create a listen socket (port=%d) - %s", port, strerror(errno));
		return false;
	}

	pFS = pFS_;
	pFS->addMonitor(sock, ::processConnect, this);
	return true;
}
