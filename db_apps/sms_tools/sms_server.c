/*!
* Copyright Notice:
* Copyright (C) 2011 NetComm Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
* CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
* SUCH DAMAGE.
*
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <syslog.h>

#include "rdb_ops.h"

#define MAX_PKT_LEN 					1024
#define MAX_RETRY_LIMIT                 10
#define RETRY_INTERVAL                  1

#define SMS_ACK		0
#define SMS_NAK		1

volatile int running = 1;
static char *server_addr[2];
static unsigned int server_port;
static int listening_sock;              /*  listening socket          */
static int connect_sock;                /*  connection socket         */
static struct sockaddr_in servaddr;  	/*  socket address structure  */
static unsigned char recv_pkt[MAX_PKT_LEN] = {0};
static unsigned char send_pkt[MAX_PKT_LEN] = {0};

#define DEBUG
#ifdef DEBUG
//#define debug(fmt, arg...) do { printf(fmt, ##arg); syslog(LOG_DEBUG, fmt, ##arg); } while (0)
#define debug(fmt, arg...) do { syslog(LOG_DEBUG, fmt, ##arg); } while (0)
#else
#define debug(fmt, arg...) do { } while (0)
#endif

static void parse_arg(int argc, char **argv)
{
	int argi;
	char *endptr;

	argi 		= optind;
	server_port = strtol(argv[argi++], &endptr, 0);
	if (*endptr) {
		syslog(LOG_ERR, "Invalid port number\n");
		exit(EXIT_FAILURE);
	}
	server_addr[0] = argv[argi++];
	server_addr[1] = argv[argi];
	syslog(LOG_DEBUG, "SMS IP1 %s, IP2 %s, Port %d\n", server_addr[0], server_addr[1], server_port);
}

static void sig_handler(int signum)
{
	switch(signum)
	{
		default:
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
			running = 0;
			break;
}
}

static void init_sms_server(void)
{
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
}

static int socket_open(void)
{
	int retry = 0;

retry_socket:
	if ((listening_sock = socket(AF_INET, SOCK_STREAM,0)) < 0)
	{
		if (retry++ > MAX_RETRY_LIMIT)
		{
			syslog(LOG_ERR, "failed to create listening socket : %s\n", strerror(errno));
			return -1;
		}
		else
		{
			goto retry_socket;
		}
	}
	return 0;
}

static int bind_socket(void)
{
	int retry = 0;

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(server_port);

retry_bind:
	if (bind(listening_sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
	{
		if (retry++ > MAX_RETRY_LIMIT)
		{
			syslog(LOG_ERR, "failed to bind listening socket : %s\n", strerror(errno));
			return -1;
		}
		else
		{
			goto retry_bind;
		}
	}
	return 0;
}

static int listen_socket(void)
{
	int retry = 0;

retry_listen:
	/* use 1 instead of LISTENQ to allow only 1 connection at the moment */
	//if (listen(listening_sock, 0) < 0)
	//if (listen(listening_sock, 1024*1024) < 0)
	//if (listen(listening_sock, 2048*1024) < 0)
	/* default is 128 in /proc/sys/net/ipv4/tcp_max_syn_backlog & /proc/sys/net/core/somaxconn */
	if (listen(listening_sock, 128) < 0)
	{
		if (retry++ > MAX_RETRY_LIMIT)
		{
			syslog(LOG_ERR, "failed to listen socket : %s\n", strerror(errno));
			return -1;
		}
		else
		{
			goto retry_listen;
		}
	}
	return 0;
}

static int check_remote_address(int addr_idx, struct sockaddr_in client_addr)
{
	struct hostent* he;
	u_long rhost_addr;

	he = gethostbyname(server_addr[addr_idx]);
	if (!he) return -1;
	memcpy(&rhost_addr, he->h_addr_list[0], 4);
	/* If remote host is set to 0.0.0.0 then accept anything */
	if (rhost_addr && rhost_addr != client_addr.sin_addr.s_addr)
	{
		return -1;
	}
	return 0;
}

static ssize_t read_packet(int sockfd, void *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char c, *buf;

	buf = vptr;
	for (n = 1; n < maxlen; n++)
	{
		if(!running) return -1;
		if ((rc = read(sockfd, &c, 1)) == 1)
		{
			*buf++ = c;
			/*debug("read 1 char : %c\n", c);*/
			if (c == '\0')
				break;
		}
		else if (rc == 0)
		{
			if (n == 1) return 0;
			else break;
		}
		else
		{
			if (errno == EINTR)
			{
				syslog(LOG_ERR, "errno == EINTR, continue\n");
				continue;
			}
			return -1;
		}
	}

	*buf = 0;
	return n;
}

#define MAX_DEST_NO_SIZE	32
#define MAX_MSG_BODY_SIZE	512
#define DEST_TAG_OPEN		"<Destination Number>"
#define DEST_TAG_CLOSE		"</Destination Number>"
#define MSG_TAG_OPEN		"<Message Body>"
#define MSG_TAG_CLOSE		"</Message Body>"

static int check_tag(char *str, const char *tag)
{
	if (strncmp(str, tag, strlen(tag)) != 0)
	{
		syslog(LOG_ERR, "Can not find tag '%s'\n", tag);
	}
	return 0;
}

char dest_no[MAX_DEST_NO_SIZE] = { 0x00, };
char msg_body[MAX_MSG_BODY_SIZE] = { 0x00, };

static int sendsms(void)
{
	char temp[128];
	(void) memset(send_pkt, 0x00, MAX_PKT_LEN);
	sprintf((char *)&send_pkt[0], "sendsms \"%s\" \"%s\" DIAG", dest_no, msg_body);
	system((const char*)&send_pkt[0]);
	if (rdb_get_single("wwan.0.sms.cmd.send.status", temp, 128) != 0)
	{
		syslog(LOG_ERR, "Cannot read '%s'\n", "wwan.0.sms.cmd.send.status");
		return SMS_NAK;
	}
	if (strcmp(temp, "[done] send") == 0)
	{
		return SMS_ACK;
	}
	return SMS_NAK;
}

static int parsing_and_sendsms(char *rx_pkt)
{
	char *r = rx_pkt;
	char *sp;

	/* parsing destination number */
	if (check_tag(r, DEST_TAG_OPEN) < 0) return SMS_NAK;
	r += strlen(DEST_TAG_OPEN);
	sp = strstr(r, DEST_TAG_CLOSE);
	if (!sp)
	{
		syslog(LOG_ERR, "Cannot find tag '%s'\n", DEST_TAG_CLOSE);
		return SMS_NAK;
	}
	if (check_tag(sp, DEST_TAG_CLOSE) < 0) return SMS_NAK;
	(void) memset(dest_no, 0x00, MAX_DEST_NO_SIZE);
	(void) strncpy(dest_no, r, sp-r);
	syslog(LOG_ERR, "destination no = %s\n", dest_no);
	r = sp + strlen(DEST_TAG_CLOSE) + 1;

	/* parsing message body */
	if (check_tag(r, MSG_TAG_OPEN) < 0) return SMS_NAK;
	r += strlen(MSG_TAG_OPEN);
	sp = strstr(r, MSG_TAG_CLOSE);
	if (!sp)
	{
		syslog(LOG_ERR, "Cannot find tag '%s'\n", MSG_TAG_CLOSE);
		return SMS_NAK;
	}
	if (check_tag(sp, MSG_TAG_CLOSE) < 0) return SMS_NAK;
	(void) memset(msg_body, 0x00, MAX_MSG_BODY_SIZE);
	(void) strncpy(msg_body, r, sp-r);
	syslog(LOG_ERR, "msg body = %s\n", msg_body);
	return sendsms();
}

const char* result_str[]= {"<Result>Successful</Result>", "<Result>Failed</Result>"};
static void send_sms_result(int sockfd, int result)
{
	int retry = 0;
	char final_result[32];

retry_send_response:
	(void) memset((char *)&final_result[0], 0x00, 32);
	strcpy(final_result, result_str[result]);
	if (send(sockfd, final_result, strlen(final_result)+1, 0) < 0) {
		if (retry++ > MAX_RETRY_LIMIT)
		{
			syslog(LOG_ERR, "Failed to send SMS TX result: %s.\n", strerror(errno));
		}
		else
		{
			goto retry_send_response;
		}
	}
	else
	{
		syslog(LOG_DEBUG, "Successfully sent SMS TX result.\n");
	}
}

static void process_incoming_packet(void)
{
	struct timeval listentv;
	fd_set listenset;
	int selected;
	struct sockaddr_in TCPClientAddr;
	socklen_t tcpClientLen = sizeof(TCPClientAddr);
	int rd_cnt, ret;

	do
	{
		FD_ZERO(&listenset);
		FD_SET(listening_sock, &listenset);
		listentv.tv_sec = 0;
		listentv.tv_usec = 1000000;
		selected = select(listening_sock + 1, &listenset, 0, 0, &listentv);
		if(!running) { break; }

		if (selected > 0)	/* not timeout and got some data */
		{
			/*
			NOTE that accept returns a new file descriptor each time. This is because being in
			server mode we can have many clients in theory connected to us.
			Although in our case we limit it to one.
			NOTE - at the moment this call blocks until a client wants to connect...
			*/
			debug("waiting for accept remote connection request\n");
			debug("---------------------------------------------------------------------\n");
			connect_sock = accept(listening_sock, (struct sockaddr*) &TCPClientAddr, &tcpClientLen);
			if (connect_sock == -1)
			{
				syslog(LOG_ERR, "sock accept is -1, give up this accept\n");
				continue;
			}
			/*debug("Got connect socket %d\n", connect_sock);*/

			/*
			for socket non-blocking. Make sure that we set this to non-blocking otherwise
			it will block on subsequent accept calls in ONLINE mode and then no data can be processed.
			We need to call accept in online mode so that we can accept a new connection in and kill the old one.
			*/
			//fcntl(listening_sock, F_SETFL, O_NONBLOCK);

			/* check remote address is same as specified server address */
			if (check_remote_address(0, TCPClientAddr) < 0 && check_remote_address(1, TCPClientAddr) < 0)
			{
				syslog(LOG_ERR, "wrong remote server address, give up this accept\n");
				close(connect_sock);
				continue;
			}
			/*debug("passed address check\n");*/

//read_packet:
			/* read incoming packet which ends with ETX or return timeout */
			(void) memset(recv_pkt, 0x00, MAX_PKT_LEN);
			rd_cnt = read_packet(connect_sock, &recv_pkt, MAX_PKT_LEN);
			if (rd_cnt >= 0)
			{
				debug("rd_cnt = %d, err = %s\n", rd_cnt, strerror(errno));
				if (rd_cnt > 0)
				{
					debug("recv_pkt = %s\n", recv_pkt);
				}
			}
			if ( rd_cnt <= 0)
			{
				if (rd_cnt == 0)
				{
					debug("remote client closed socket, close local client socket\n");
				}
				else
				{
					syslog(LOG_ERR, "Timeout while reading incoming packet, packet may be incorrectly formatted.\n");
				}
				debug("wait for next connection request\n");
				close(connect_sock);
				continue;
			}

			/* convert packet to sms message */
			ret = parsing_and_sendsms((char *)&recv_pkt[0]);
			/*ret = SMS_ACK;*/
			debug("sendsms() result = %s\n", (ret==SMS_ACK)? "SMS ACK":"SMS NAK");
			send_sms_result(connect_sock, ret);
			//debug("response from client = %d, err = %s\n", ret, strerror(errno));
			//debug("wait for next incoming packet\n");
			debug("close client socket from server side\n");
			close(connect_sock);
			continue;
			//goto read_packet;
		}
	} while (running);
}

int main(int argc, char **argv)
{

	// For help text
	if (argc >= 2) {
		if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
			printf("\nThis command is for internal system use only.\n");
			printf("It is used to support SMS server feature.\n");
            printf("Please do not run this command manually.\n\n");
			return -1;
		}
	}

	if (argc < 4)
	{
		syslog(LOG_ERR, "Missing arguments.\n");
		exit(EXIT_FAILURE);
	}

	/* parse argument and initialize */
	parse_arg(argc, argv);
	init_sms_server();

	if (rdb_open_db() <= 0)
	{
		syslog(LOG_ERR, "Failed to open database!\n");
		exit(-1);
	}

	/* open tcp socket */
	if (socket_open() < 0)	exit(EXIT_FAILURE);

	/* bind & listen tcp listening socket */
	#define EXIT_SOCKET		goto exit_socket
	if (bind_socket() < 0) EXIT_SOCKET;
	if (listen_socket() < 0) EXIT_SOCKET;

	/* wait for incoming packet & process the packet */
	process_incoming_packet();

exit_socket:
	close(listening_sock);
	rdb_close_db();
	exit(0);
}

/*
* vim:ts=4:sw=4:
*/
