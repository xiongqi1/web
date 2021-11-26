#ifndef __HFI_H__06052016
#define __HFI_H__06052016
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
 * This file contains high frequency interface code
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* sockserver */
struct sockserver_t {
	int idx;
	struct atcp_t* t; /* atcp object */
	struct atcp_client_t* client; /* IO stream output client */

	int port_valid;
	int ipaddr_valid;

	/* IO stream output client address !!warning!! port number is host-byte-ordered*/
	struct sockaddr_in saddr;

	unsigned long long session_tm;
	int session_valid;

	struct binqueue_t* in_sq; /* input stream session queue */

	int input_stream_mode; /* input stream mode */
};

/* socket servers index */
enum {
	sockserver_output=0,
	sockserver_input,
	sockserver_count,
};

/* convert socket server to name */
extern const char* sockserver_str_name[];

/* socket servers */
extern struct sockserver_t sservers[sockserver_count];

void sockserver_disconnect_client(struct sockserver_t* ss);
void enable_input_stream_mode(int en);

void ch_queue_clear();
void ch_queue_fini();
int ch_queue_init();
int sockserver_is_input_mode_running();
void sockserver_collect_dead_connections();
void sockserver_disconnect_client(struct sockserver_t* ss);
int sockserver_reinit(struct sockserver_t* ss);
int dump_to_high_freq_interface( int ch_index,unsigned int* ch_samples,int sample_count,unsigned long long ms64);
int feed_ch_queue();
int dump_session_queue_to_ch_queue();
void sockserver_on_read(struct atcp_t* t,struct atcp_client_t* c,void* ref);
void sockserver_on_accept(struct atcp_t* t,struct atcp_client_t* c,void* ref);

#endif
