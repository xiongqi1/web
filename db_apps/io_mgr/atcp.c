/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include "atcp.h"
#include "commonIncludes.h"

#define ATCP_MAX_CLIENTS	10

struct atcp_t {
	int lsock;
	
	int writeonly;
	
	atcp_callback_on_accept fn_on_accept;
	atcp_callback_on_read fn_on_read;
	
	struct atcp_client_t clients[ATCP_MAX_CLIENTS];
	
	void* ref;
};

static struct atcp_client_t* atcp_get_client_to_use(struct atcp_t* t)
{
	int i;
	struct atcp_client_t* c;
	
	/* search available socket */
	for(i=0;i<ATCP_MAX_CLIENTS;i++) {
		c=&t->clients[i];
		
		if(c->csock<0)
			return c;
	}
	
	return NULL;
}

void atcp_set_callbacks(struct atcp_t* t,atcp_callback_on_accept fn_on_accept,atcp_callback_on_read fn_on_read,void* ref)
{
	t->ref=ref;
	t->fn_on_accept=fn_on_accept;
	t->fn_on_read=fn_on_read;
}

void atcp_close_client(struct atcp_t* t,struct atcp_client_t* c)
{
	if(c->csock>=0)
		close(c->csock);
	c->csock=-1;
}

void atcp_close_all_clients(struct atcp_t* t)
{
	int i;
	struct atcp_client_t* c;
	
	for(i=0;i<ATCP_MAX_CLIENTS;i++) {
		c=&t->clients[i];
		atcp_close_client(t,c);
	}
}

void atcp_do_process(struct atcp_t* t,fd_set* readfds)
{
	struct atcp_client_t* c;
	int csock;
	int i;
	
	int flags;
	
	struct sockaddr_in client_addr; 
	int client_addr_len;
	
	/* if accept */
	if(FD_ISSET(t->lsock,readfds)) {
				
		/* accept */
		client_addr_len=sizeof(client_addr);
		csock=accept(t->lsock, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
		if(csock<0) {
			DBG(LOG_ERR,"failed in accept() - %s",strerror(errno));
			goto fini_accept;
		}
		
		/* get a client to use */
		c=atcp_get_client_to_use(t);
		if(!c) {
			DBG(LOG_ERR,"too many client connection requests");
			close(csock);
			goto fini_accept;
		}
		
		/* set non-block access */
		flags = fcntl(csock, F_GETFL);
		if(fcntl(csock, F_SETFL, flags | O_NONBLOCK | O_CLOEXEC)<0) {
			DBG(LOG_ERR,"failed in fcntl() - %s",strerror(errno));
		}
		
		/* store socket to client */
		c->csock=csock;
		c->client_addr=client_addr;
		
		/* call callback */
		if(t->fn_on_accept) {
			t->fn_on_accept(t,c,t->ref);
		}
	}
	
fini_accept:
	
	/* if read */
	if(!t->writeonly) {
		for(i=0;i<ATCP_MAX_CLIENTS;i++) {
			c=&t->clients[i];
			
			/* bypass if not created */
			if(c->csock<0)
				continue;
			
			/* call fn_on_read if triggered */
			if(FD_ISSET(c->csock,readfds)) {
				/* call callback */
				if(t->fn_on_read)
					t->fn_on_read(t,c,t->ref);
			}
		}
	}
}

int atcp_set_fds(struct atcp_t* t,fd_set* readfds)
{
	int i;
	struct atcp_client_t* c;
	
	int max_fd=-1;
	
	/* set listen socket */
	FD_SET(t->lsock,readfds);
	if(t->lsock>max_fd)
		max_fd=t->lsock;
	
	/* set client sockets */
	if(!t->writeonly) {
		for(i=0;i<COUNTOF(t->clients);i++) {
			c=&t->clients[i];
			
			if(c->csock>=0) {
				FD_SET(c->csock,readfds);
				
				if(c->csock>max_fd)
					max_fd=c->csock;
			}
		}
	}
	
	return max_fd;
}

int atcp_is_server_running(struct atcp_t* t)
{
	return t->lsock>=0;
}

int atcp_open_server(struct atcp_t* t,struct in_addr* sin_addr,int port,int writeonly)
{
	struct sockaddr_in serv_addr; 
	int reuse=1;
	
	/* store write only flag */
	t->writeonly=writeonly;
	
	/* create a unnamed socket */	
	t->lsock = socket(AF_INET, SOCK_STREAM, 0);
	if(t->lsock<0) {
		DBG(LOG_ERR,"failed to create a unnamed socket for listening (port=%d)- %s",port,strerror(errno));
		goto err;
	}
	
	/* set socket option - SO_REUSEADDR */
	setsockopt(t->lsock,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	
	/* bind to a port */	
	memset(&serv_addr,0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = *sin_addr;
	serv_addr.sin_port = htons(port); 
	if(bind(t->lsock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0) {
		DBG(LOG_ERR,"failed to bind to port (port=%d) - %s",port,strerror(errno));
		goto err;
	}
	
	/* create a listen socket */
	if(listen(t->lsock, ATCP_MAX_CLIENTS)<0) {
		DBG(LOG_ERR,"failed to create a listen socket (port=%d) - %s",port,strerror(errno));
		goto err;
	}
	
	return 0;
err:
	return -1;	
}

void atcp_close_server(struct atcp_t* t)
{
	/* close all clients */
	atcp_close_all_clients(t);
	
	/* close listening socket */
	if(t->lsock>=0)
		close(t->lsock);
	t->lsock=-1;
}

void atcp_destroy(struct atcp_t* t)
{
	if(!t)
		return;
	
	/* close listening socket */
	atcp_close_server(t);
	
	free(t);
}

struct atcp_t* atcp_create()
{
	struct atcp_t* t;
	int i;
	struct atcp_client_t* c;

	/* allocate an object */
	t=calloc(1,sizeof(*t));
	if(!t) {
		DBG(LOG_ERR,"failed to allocate an object");
		goto err;
	}

	/* init. members */
	t->lsock=-1;

	/* init. each clinet */
	for(i=0;i<COUNTOF(t->clients);i++) {
		c=&t->clients[i];
		c->csock=-1;
	}

	return t;

err:
	return NULL;
}
