/*
	This program is TCP to Diag port relay bridge only for Quanta module.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <fcntl.h>
#include <termios.h>
#include "rdb_ops.h"
#include <sys/ioctl.h>

#define LISTEN_PORT		30000
#define MAX_PACKET_SIZE		65535
#define WORKER_THREAD_STACKSIZE 1024*100
#define TIMEOUT_CNT		20


#undef DEBUG
#undef DEBUG_RXTX
// #define DEBUG
// #define DEBUG_RXTX


#ifdef DEBUG
#define SYSLOG_ERR(...)     printf(__VA_ARGS__); putchar('\n')
#define SYSLOG_INFO(...)    printf(__VA_ARGS__); putchar('\n')
#define SYSLOG_DEBUG(...)   printf(__VA_ARGS__); putchar('\n')

#else
#define SYSLOG_ERR(...)
#define SYSLOG_INFO(...)
#define SYSLOG_DEBUG(...)
#endif

#ifdef DEBUG_RXTX
#define SYSLOG_NOLF(...)   printf(__VA_ARGS__)
#endif

extern int rdb_init(int fCreate);
extern int  open_rdb(void);

int	g_running = 1;

static int max(int a, int b, int c, int d)
{
     int m = a;
     (void)((m < b) && (m = b)); //these are not conditional statements.
     (void)((m < c) && (m = c)); //these are just boolean expressions.
     (void)((m < d) && (m = d));
     return m;
}

static void sig_handler(int signum)
{
	SYSLOG_DEBUG("caught signal %d", signum);
	switch (signum)
	{
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			g_running = 0;
			break;
	}
}

static void bail(const char *on_what) {
	SYSLOG_ERR("%s: %s\n", strerror(errno), on_what);
	exit(1);
}

int main(void) {

	int	listenfd = -1, connfd = -1, devfd = -1, rdb_fd = -1;
	int	readLen = 0, writeLen = 0, sentLen = 0;
	uint8_t	sockbuf[MAX_PACKET_SIZE];
	uint8_t	devbuf[MAX_PACKET_SIZE];
	int	totalRxLen;
	
	int 		len, ret, on = 1;
	socklen_t	addrlen;
	struct sockaddr_in cliaddr;
	struct sockaddr_in srvaddr;
	
	fd_set	readfds;
	int	nfds = 0, selected = 0;
	struct timeval timeout;
	
	struct termios oldtio, newtio;
	
	int	timeout_cnt;
	
//////////////////////
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
//////////////////////

	ret = rdb_init(1);
	if(ret != 0) {
		SYSLOG_ERR("ERROR! cannot initialize rdb");
		goto error;
	}

	ret = open_rdb();
	if(ret != 0) {
		SYSLOG_ERR("ERROR! cannot subcrible rdb");
		goto error;
	}
	
	memset( &srvaddr, 0, sizeof(srvaddr));
	
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_port = htons(LISTEN_PORT);
	srvaddr.sin_addr.s_addr = ntohl(INADDR_ANY);

	if ((listenfd = socket(PF_INET, SOCK_STREAM,0)) < 0)
		bail("socket()");
	
	SYSLOG_DEBUG("listenfd = %d", listenfd);
	
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0)
		bail("setsockopt(SO_REUSEADDR)");

	len = sizeof(srvaddr);
	if ( bind(listenfd, (struct sockaddr *) &srvaddr, len) < 0 )
		bail("bind()");
	
	SYSLOG_DEBUG("Bind socket to addr: %s:%d", inet_ntoa(srvaddr.sin_addr), ntohs(srvaddr.sin_port));
	
	if ( listen(listenfd, 10) < 0 )
		bail("listen()");
	
	SYSLOG_DEBUG("Start listening...");
	
	addrlen = sizeof(cliaddr);

	SYSLOG_DEBUG("start main loop");
	while (g_running) {
//for test		rdb_fd = rdb_get_fd();
		rdb_fd = -1;
		nfds = 1 + max(listenfd, connfd, devfd, rdb_fd);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		
		FD_ZERO(&readfds);

		if (listenfd > 0)
			FD_SET(listenfd, &readfds);

		if (connfd > 0)
			FD_SET(connfd, &readfds);
		
		if (devfd > 0)
			FD_SET(devfd, &readfds);

		if (rdb_fd > 0)
			FD_SET(rdb_fd, &readfds);

		selected = select(nfds, &readfds, NULL, NULL, &timeout);
		
		if (!g_running) {
			SYSLOG_DEBUG("running flag off");
			break;
		}
		else if (selected < 0)  // ERROR
		{
			// if system call
			if (errno == EINTR)
			{
				SYSLOG_DEBUG("system call detected");
				continue;
			}
			
			SYSLOG_ERR("select() punk - error#%d(str%s)",errno,strerror(errno));
			break;
		}
		else if (selected == 0)  // Timeout
		{
			continue;
		}

		if ((connfd > 0) && (devfd > 0) && FD_ISSET(connfd, &readfds)  )
		{
			memset(devbuf, 0, sizeof(devbuf));
			totalRxLen = 0;
			while(1) {
				memset(sockbuf, 0, sizeof(sockbuf));
				readLen = recv(connfd, sockbuf, MAX_PACKET_SIZE, 0);

				if ((readLen == 0) || (readLen < 0 && errno != EINTR && errno != EWOULDBLOCK)) {
					SYSLOG_DEBUG("Socket Connection Down on recv(), fd = %d", connfd);
					close(connfd);
					connfd = -1;
					break;
				}
				if (readLen < 0 && errno == EWOULDBLOCK) {
					break;
				}
				if ((totalRxLen + readLen) > sizeof(devbuf)) {
					bail("Buffer Overflow");
				}

				memcpy(devbuf+totalRxLen, sockbuf, readLen);
				totalRxLen += readLen;
			}
#if DEBUG_RXTX
			int i;
			SYSLOG_DEBUG("RX length from CLIENT = %d", totalRxLen);
			SYSLOG_NOLF("Rx: ");
			for (i=0; i < totalRxLen ; i++) {
				SYSLOG_NOLF("%02x ", devbuf[i]);
			}
			SYSLOG_NOLF("\n");
#endif

			sentLen = 0;
			writeLen = 0;
			while (totalRxLen > sentLen){
				writeLen = write(devfd, devbuf+sentLen, totalRxLen-sentLen);
				SYSLOG_DEBUG("Sent Bytes to USB [%d]", writeLen);

				if (writeLen <= 0 && errno != EINTR ) {
					SYSLOG_DEBUG("Diag Connection Down on write() fd=%d", devfd);
					close(devfd);
					devfd = -1;
					break;
				}
				sentLen = sentLen + writeLen;
			}
		}
		
		if ((connfd > 0) && (devfd > 0) && FD_ISSET(devfd, &readfds) )
		{
			memset(sockbuf, 0, MAX_PACKET_SIZE);
			readLen = read(devfd, sockbuf, MAX_PACKET_SIZE);
			if (readLen <= 0 && errno != EINTR) {
				SYSLOG_DEBUG("Diag Connection Down on read(), fd = %d", devfd);
				close(devfd);
				devfd = -1;
			}
#if DEBUG_RXTX
			int i;
			SYSLOG_DEBUG("RX length from USB = %d", readLen);
			SYSLOG_NOLF("Tx: ");
			for (i=0; i < readLen ; i++) {
				SYSLOG_NOLF("%02x ", sockbuf[i]);
			}
			SYSLOG_NOLF("\n");
#endif

			sentLen = 0;
			writeLen = 0;
			while (readLen > sentLen){
				writeLen = send(connfd, sockbuf+sentLen, readLen-sentLen, 0);
				SYSLOG_DEBUG("Sent Bytes to Client [%d]", writeLen);

				if (writeLen < 0 && errno != EINTR && errno != EWOULDBLOCK) {
					SYSLOG_DEBUG("Socket Connection Down on send(), fd = %d", connfd);
					close(connfd);
					connfd = -1;
					break;
				}
				sentLen = sentLen + writeLen;
			}
		}

		if (connfd < 0 || devfd < 0) {
			close(connfd);
			close(devfd);
			connfd = -1;
			devfd = -1;
		}

		if (FD_ISSET(listenfd, &readfds) && (connfd < 0))
		{

			if ( (connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &addrlen)) < 0 )
				bail("accept()");

			SYSLOG_DEBUG("Connection Request from fd=%d: %s:%d", connfd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

			ret = fcntl(connfd, F_GETFL, 0);
			SYSLOG_DEBUG("fcntl flags = %d", ret);
			fcntl(connfd, F_SETFL, ret | O_NONBLOCK);

			char value[128] = {0,};
			
			if ((devfd < 0)) {
				rdb_get_single("wwan.0.diag_if", value, sizeof(value));

				timeout_cnt = 0;

				while ((strlen(value) == 0) || (devfd=open(value, O_RDWR | O_NOCTTY | O_TRUNC))  < 0 ) {
					if(!g_running)
						break;

					if (timeout_cnt < TIMEOUT_CNT)
						timeout_cnt++;
					else {
						SYSLOG_DEBUG("Diag Port open timeout");
						break;
					}

					SYSLOG_DEBUG("Waiting Diag port up");
					sleep(1);
					rdb_get_single("wwan.0.diag_if", value, sizeof(value));

				}

				if (devfd > 0) {
					SYSLOG_DEBUG("Diag Device open Success devfd = %d", devfd);
					tcgetattr(devfd,&oldtio);
					memcpy(&newtio, &oldtio, sizeof(struct termios));
					cfmakeraw(&newtio);
					newtio.c_cc[VMIN] = 1;
					newtio.c_cc[VTIME] = 0;
					tcsetattr(devfd, TCSAFLUSH, &newtio);
				}
			}
		}
	}
	
	close(devfd);
	close(connfd);
	close(listenfd);
	
error:
	rdb_close_db();

	return 0;
}
