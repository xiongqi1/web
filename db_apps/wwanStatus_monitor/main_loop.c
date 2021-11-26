
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
#include <pthread.h>
#include <ctype.h>
#include <limits.h>

#include "comms.h"
#include "log.h"
#include "rdb_ops.h"

static void bail(const char *on_what) {
	SYSLOG_ERR("%s: %s\n", strerror(errno), on_what);
	exit(1);
}

//TODO
static int getLanIp(char * name) {
	strcpy(name, "br0");
	return 1;
}

static int extract_digit(char * src ) {
	char dest[MAX_RDB_BUFFER];

	char *pstr1 = src;
	char *pstr2 = dest;
	
	while(*pstr1) {
		if( isdigit ( *pstr1 ) ) {
			*pstr2++ = *pstr1;
		}
		*pstr1++;
	}
	*pstr2 = 0;
	
	return atoi(dest);
}

static void str_tolower(char * src, char *dest ) {
	char *pstr1 = src;
	char *pstr2 = dest;
	
	while(*pstr1) {
		*pstr2++ = tolower(*pstr1++);
	}
	*pstr2 = 0;
	return;
}

static uint64_t htonll(uint64_t value) {
    int num = 42;
    if (*(char *)&num == 42) {
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}

static void * client_handler ( void * arg) {
	int 		connfd;
	char 		buf[MAX_PACKET_SIZE];
	int		len_rxpkt, len_txpkt, ret;
	char 		dateBuf[16], timeBuf[16];
	
	time_t current;
	struct tm timeptr;
	char rdbBuf[MAX_RDB_BUFFER], destBuf[MAX_RDB_BUFFER];
	
	connfd = *((int *) arg);
	free(arg);

	SYSLOG_DEBUG("Created pthread...fd = %d", connfd);

	pthread_detach(pthread_self());
	
read_loop:	
	len_rxpkt = read(connfd, buf, MAX_PACKET_SIZE);
	
	SYSLOG_DEBUG("Length of received data = %d", len_rxpkt);

	if(len_rxpkt > 0) {
		buf[len_rxpkt] = 0;
		
		typeRequest * rx_buf = (typeRequest *) &buf;

		SYSLOG_DEBUG("request id= 0x%x", rx_buf->req_id);
		
		if (rx_buf->req_id == REQ_ID) {
			typeResponse * tx_buf = (typeResponse *) &buf;
			
			memset(buf, 0, sizeof(buf));
			memset(dateBuf, 0, sizeof(dateBuf));
			memset(timeBuf, 0, sizeof(timeBuf));
			len_txpkt = sizeof(typeResponse);

			current = time(NULL);
			localtime_r(&current, &timeptr);
			
			SYSLOG_DEBUG("Date:DDMMYY %02d%02d%02d and time HHMM: %02d%02d \n", 
					timeptr.tm_mday, timeptr.tm_mon+1, (timeptr.tm_year + 1900)%100,
					timeptr.tm_hour, timeptr.tm_min);

			sprintf(dateBuf,"%02d%02d%02d", timeptr.tm_mday, timeptr.tm_mon+1, (timeptr.tm_year + 1900) % 100);
			sprintf(timeBuf,"%02d%02d", timeptr.tm_hour, timeptr.tm_min);
			
			tx_buf->reply_id = REPLY_ID;
			tx_buf->result_code = RESULT_OK;
			
			rdbBuf[0] = 0;
			
			ret = rdb_get_single("uboot.sn", rdbBuf, sizeof(rdbBuf));
			
			if ( (ret != 0) || (atoll(rdbBuf) == 0 ) ) {
				SYSLOG_DEBUG("Fail to get uboot.sn");
				//tx_buf->device_sn = htonll(gParam.g_deviceSN);
				if (gParam.g_deviceSN) {
					tx_buf->device_sn = gParam.g_deviceSN;
				} else {
					tx_buf->device_sn = 0xFFFFFFFFFFFFFFFFULL;
				}
			}
			else {
				SYSLOG_DEBUG("uboot.sn = %s", rdbBuf);
				//tx_buf->device_sn = htonll(atoll(rdbBuf));
				tx_buf->device_sn = atoll(rdbBuf);
			}

//			memcpy(tx_buf->device_sn, "12345678", sizeof(tx_buf->device_sn)); //TODO

			memcpy(tx_buf->status_date, dateBuf, sizeof(tx_buf->status_date));
			memcpy(tx_buf->status_time, timeBuf, sizeof(tx_buf->status_time));

			/* SYSTEM_MODE 4, Wi-Fi mode was added in Rev. B. */
			if (rdb_get_single("wlan_sta.0.sta.connStatus", rdbBuf, sizeof(rdbBuf)) == 0 && strcmp(rdbBuf, "up") == 0) {
				tx_buf->system_mode = 4;
				SYSLOG_DEBUG("Wi-Fi connected, set system_mode to %d", tx_buf->system_mode);
			}
			else if (rdb_get_single("wwan.0.system_network_status.system_mode", rdbBuf, sizeof(rdbBuf)) != 0) {
				tx_buf->system_mode = 0;
			}
			else {
				str_tolower(rdbBuf, destBuf);
				
				SYSLOG_DEBUG("system_mode=%s:%s", rdbBuf, destBuf);
				
				if ( (strstr(destBuf,"gsm")) || (strstr(destBuf,"gprs")) ) {
					tx_buf->system_mode = 1;
				}
				else if ( (strstr(destBuf,"edge")) ) {
					tx_buf->system_mode = 2;
				}
				else if ( (strstr(destBuf,"umts")) || (strstr(destBuf,"hsdpa")) || (strstr(destBuf,"hsupa")) || (strstr(destBuf,"hspa")) || (strstr(destBuf,"lte")) ) {
					tx_buf->system_mode = 3;
				}
				else {
					tx_buf->system_mode = 0;
				}
			}
			
			/* SYSTEM_MODE 4, Wi-Fi mode was added in Rev. B. */
			if (tx_buf->system_mode == 4 || tx_buf->system_mode == 0) {
				tx_buf->signal_quality_rssi = 99;
				tx_buf->signal_quality_ber = 99;
			}
			else {
				if (rdb_get_single("wwan.0.radio.information.signal_strength", rdbBuf, sizeof(rdbBuf)) != 0) {
					tx_buf->signal_quality_rssi = 99;
				}
				else {
					int rssi = extract_digit(rdbBuf);
					SYSLOG_DEBUG("signal_quality_rssi=%s", rdbBuf);
					if (rssi == 0 )
						tx_buf->signal_quality_rssi = 99;
					else if (rssi >= 113 )
						tx_buf->signal_quality_rssi = 0;
					else if (rssi <= 51 )
						tx_buf->signal_quality_rssi = 31;
					else
						tx_buf->signal_quality_rssi = (uint8_t) (113 - rssi) / 2;
				}
				if (rdb_get_single("wwan.0.radio.information.signal_strength.bars", rdbBuf, sizeof(rdbBuf)) != 0) {
					tx_buf->signal_quality_ber = 99;
				}
				else {
					SYSLOG_DEBUG("signal_quality_ber=%s", rdbBuf);
					tx_buf->signal_quality_ber = atoi(rdbBuf);
				}
			}
			
			SYSLOG_DEBUG("Length of Tx Packet=%d", len_txpkt);
			
			do {
				ret = send(connfd, buf, len_txpkt, 0);
			} while( ret == -1 && errno == EINTR);
		}
		/* read until the client request disconnection */
		goto read_loop;
	} else {
		SYSLOG_DEBUG("client disconnected, close socket and wait next connection request");
	}
	
	close(connfd);
	return NULL;
}

void main_loop(void) {
	int		listenfd, *connfd;
	int 		len, ret;
	socklen_t	addrlen;
	pthread_t	tid;
	struct sockaddr_in cliaddr;
	struct sockaddr_in srvaddr;
	
	char ifname[16] = {0,};
	int on = 1;
	pthread_attr_t attr;
	
	
	SYSLOG_DEBUG("start main loop");
	
	memset( &srvaddr, 0, sizeof(srvaddr));
	
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_port = htons(gParam.g_serverPort);
	srvaddr.sin_addr.s_addr = ntohl(INADDR_ANY);
	
	if ((listenfd = socket(PF_INET, SOCK_STREAM,0)) < 0)
		bail("socket()");
	
	SYSLOG_DEBUG("listenfd = %d", listenfd);

	if(getLanIp(ifname) < 0)
		bail("getLanIp()");
	
	SYSLOG_DEBUG("name of interface = [%s]", ifname);
	
	len = strlen(ifname);
	if(len) {
		if( setsockopt(listenfd, SOL_SOCKET, SO_BINDTODEVICE, ifname, len+1) < 0 )
			bail("setsockopt(SO_BINDTODEVICE)");
		
		SYSLOG_DEBUG("setsockopt(SO_BINDTODEVICE)");
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0)
		bail("setsockopt(SO_REUSEADDR)");

	len = sizeof(srvaddr);
	if ( bind(listenfd, (struct sockaddr *) &srvaddr, len) < 0 )
		bail("bind()");
	
	SYSLOG_DEBUG("Bind socket to addr: %s:%d", inet_ntoa(srvaddr.sin_addr), ntohs(srvaddr.sin_port));
	
	if ( listen(listenfd, 32) < 0 )
		bail("listen()");
	
	SYSLOG_DEBUG("Start listening...");
	
	addrlen = sizeof(cliaddr);
	
	// worker thread attribute init
	if(pthread_attr_init(&attr)) {
		perror("pthread_attr_init");
		exit(1);
	}
	pthread_attr_getstacksize(&attr, (size_t *) &ret);
	SYSLOG_DEBUG("default stack size: %d (min %d)\n", ret, PTHREAD_STACK_MIN);
	if(pthread_attr_setstacksize(&attr, WORKER_THREAD_STACKSIZE)) {
		bail("pthread_attr_setstacksize");
	}

	while (gParam.g_running) {
		fd_set fdr;
		int selected;
		int nfds = listenfd + 1;
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
		
		FD_ZERO(&fdr);
		
		if ( listenfd > 0 ) {
			FD_SET(listenfd, &fdr);
		}
		else {
			SYSLOG_ERR("Error: Wrong listenfd: %s\n", strerror(errno));
			break;
		}
		
		selected = select(nfds, &fdr, NULL, NULL, &timeout);

		if ( !gParam.g_running ) {
			SYSLOG_DEBUG("running flag off");
			break;
		}

		if ( selected == 0 ) {
			continue; // Time Out
		}
		else if (selected < 0) {
			// if system call
			if (errno == EINTR)
			{
				SYSLOG_DEBUG("system call detected");
				continue;
			}
			
			SYSLOG_ERR("select() punk - error#%d(str%s)",errno,strerror(errno));
			break;
		}
		
		
		if ( FD_ISSET(listenfd, &fdr) ) {
			connfd = malloc(sizeof(int));
			if ( (*connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &addrlen)) < 0 )
				bail("accept()");
		
			SYSLOG_DEBUG("Connection Request from fd=%d: %s:%d", *connfd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		
			if((ret = pthread_create( &tid, &attr, &client_handler, connfd)) != 0) {
				SYSLOG_ERR("Failure on pthread_create(): %d, connfd=%d", ret, *connfd);
				close(*connfd);
				free(connfd);
			}
		}
	}

	close(listenfd);
}
