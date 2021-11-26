#ifndef __ATCP_H__28112015
#define __ATCP_H__28112015
/*
 * TCP Client support
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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

#include <sys/select.h>
#include <netinet/in.h>

struct atcp_t;
struct atcp_client_t;

struct atcp_client_t {
	int csock;
	
	struct sockaddr_in client_addr;
};

typedef void (*atcp_callback_on_accept)(struct atcp_t* t,struct atcp_client_t* c,void* ref);
typedef void (*atcp_callback_on_read)(struct atcp_t* t,struct atcp_client_t* c,void* ref);


void atcp_set_callbacks(struct atcp_t* t,atcp_callback_on_accept fn_on_accept,atcp_callback_on_read fn_on_read,void* ref);
void atcp_close_client(struct atcp_t *t, struct atcp_client_t *c);
void atcp_close_all_clients(struct atcp_t *t);
void atcp_do_process(struct atcp_t *t, fd_set *readfds);
int atcp_set_fds(struct atcp_t *t, fd_set *readfds);
int atcp_open_server(struct atcp_t* t,struct in_addr* sin_addr,int port,int writeonly);
void atcp_close_server(struct atcp_t *t);
void atcp_destroy(struct atcp_t *t);
struct atcp_t *atcp_create(void);
int atcp_is_server_running(struct atcp_t* t);
		
#endif
