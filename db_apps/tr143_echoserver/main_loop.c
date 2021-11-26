
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

#include "comms.h"
#include "log.h"
#include "rdb_event.h"
#include "utils.h"

#define DEFAULT_UDP_PORT	5000
#define MAX_PACKET_SIZE		65505

TConnectSession g_session;
TStatistics g_statistics;

unsigned char datagram[MAX_PACKET_SIZE];

static void bail(const char *on_what) {
	SYSLOG_ERR("%s: %s\n", strerror(errno), on_what);
}

void init_gSession (void) {
	g_session.m_PrevEchoServerEnabled = 0;
	g_session.m_PrevEchoPlusEnabled = 0;
	g_session.m_if_name[0] = 0;
	g_session.m_sock = -1;
	memset(&g_session.m_inet_ServerAddr, 0, sizeof(struct sockaddr_in));
	memset(&g_session.m_inet_ServerAddr, 0, sizeof(struct sockaddr_in));
	g_session.m_startTimeStamp = 0;
	g_session.m_tmpbuf = NULL;
	g_session.m_stop_by_rdb_change = 0;
}

void init_gStatistics(void) {
	g_statistics.m_TestRespSN = 0;
	g_statistics.m_TestRespReplyFailureCount = 0;

	g_statistics.m_PacketReceived = 0;
	g_statistics.m_PacketsResponded = 0;
	g_statistics.m_BytesReceived = 0;
	g_statistics.m_BytesResponded = 0;
	g_statistics.m_TimeFirstPacketReceived[0] = 0;
	g_statistics.m_TimeLastPacketReceived[0] = 0;
}

void set_rdbStatistics() {
	packetsReceived2rdb(g_statistics.m_PacketReceived);
	packetsResponded2rdb(g_statistics.m_PacketsResponded);
	bytesReceived2rdb(g_statistics.m_BytesReceived);
	bytesResponded2rdb(g_statistics.m_BytesResponded);
	timeFirstPacketReceivedr2rdb(g_statistics.m_TimeFirstPacketReceived);
	timeLastPacketReceived2rdb(g_statistics.m_TimeLastPacketReceived);
}

void connection_init(void) {
	char value[128] = {0,};
	unsigned int udpPort;
	char serverIP[16] = {0,};
	int retVal = -1;
	
	g_session.m_sock = -1;
	
	if(rdb_get_single(UDPEchoConfig_Interface, value, sizeof(value)) == 0) {
		strncpy(g_session.m_if_name, value, sizeof(g_session.m_if_name));
	}
	else {
		g_session.m_if_name[0] = 0;
	}

	if(!strcmp(g_session.m_if_name, "LAN")){
		retVal = getLanIp(serverIP);
	}
	else if (!strcmp(g_session.m_if_name, "WAN")){
		retVal = getWanIp(serverIP);
	}
	else if (!strcmp(g_session.m_if_name, "3GWAN")){
		retVal = get3GwanIp(serverIP);
	}
	
	if (retVal < 0 || serverIP[0] == 0)
		strcpy(serverIP, "0.0.0.0");
	
	memset(&g_session.m_inet_ServerAddr, 0, sizeof(g_session.m_inet_ServerAddr));
	g_session.m_inet_ServerAddr.sin_family = AF_INET;
	
	if(rdb_getint(UDPEchoConfig_UDPPort, &udpPort) < 0)
		udpPort = DEFAULT_UDP_PORT;

	if (udpPort > 0 )
		g_session.m_inet_ServerAddr.sin_port = htons(udpPort);

	g_session.m_inet_ServerAddr.sin_addr.s_addr = inet_addr(serverIP);
	
	if(rdb_get_single(UDPEchoConfig_SourceIPAddress, value, sizeof(value)) == 0) {
		g_session.m_inet_SourceAddr.sin_addr.s_addr = inet_addr(value);
	}
	else {
		g_session.m_inet_SourceAddr.sin_addr.s_addr = INADDR_NONE;
	}

	g_session.m_startTimeStamp = gettimeofday64();
	init_gStatistics();
	set_rdbStatistics();
}
int main_loop(TParameters *pParameters)
{
	int fSockClosed;
	int cur_ServerEnabled;
	int cur_EchoPlueEnabled;
	int len_addr;
	int len_rxpkt, len_txpkt;
	int z;
	int64_t t;
	struct sockaddr_in clientAddr;

	
	pParameters->m_running =1;
	pParameters->m_force_rdb_read =1;
	init_gSession();
	init_gStatistics();

	SYSLOG_DEBUG("start main loop");
	while (pParameters->m_running) {
		
		/* initialize loop variables */
		fd_set fdr;
		int selected;
		int sock_fd = g_session.m_sock;
		int rdb_fd = rdb_get_fd();
		int nfds = 1 + (sock_fd > rdb_fd ? sock_fd : rdb_fd);
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
		
		FD_ZERO(&fdr);
		
		fSockClosed = g_session.m_sock < 0;
		if(!fSockClosed)
			FD_SET(sock_fd, &fdr);
		
		FD_SET(rdb_fd, &fdr);
		
		selected = select(nfds, &fdr, NULL, NULL, &timeout);
		
		if(!pParameters->m_running)
		{
			SYSLOG_DEBUG("running flag off");
			break;
		}
		else if (selected < 0)
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
		else if (selected > 0 || pParameters->m_force_rdb_read)
		{
			if (FD_ISSET(rdb_fd, &fdr) || pParameters->m_force_rdb_read) {
			 	pParameters->m_force_rdb_read = 0;
				SYSLOG_DEBUG("got RDB event");
				cur_ServerEnabled = is_UDPEchoConfigEnabled();
				cur_EchoPlueEnabled = is_EchoPlusEnabled();
				
				if(cur_ServerEnabled != g_session.m_PrevEchoServerEnabled) {
					g_session.m_PrevEchoServerEnabled = cur_ServerEnabled;
					rdb_set_boolean(TR143_IPTABLE_TRIGGER, TRUE);
					
					if(cur_ServerEnabled == TRUE) {
						SYSLOG_DEBUG("UDP SERVER Enabled");
						connection_init();
						g_session.m_sock = socket(AF_INET, SOCK_DGRAM, 0);
						
						SYSLOG_DEBUG("Socket Number=%d", g_session.m_sock);
						SYSLOG_DEBUG("Socket Server IP=%s", inet_ntoa(g_session.m_inet_ServerAddr.sin_addr));
						SYSLOG_DEBUG("Socket Server PORT=%d", ntohs(g_session.m_inet_ServerAddr.sin_port));
						SYSLOG_DEBUG("Socket Source IP=%s", inet_ntoa(g_session.m_inet_SourceAddr.sin_addr));
						
						if(g_session.m_sock == -1) {
							bail("socket()");
							continue;
						}
						
						bind(g_session.m_sock, (struct sockaddr *) &g_session.m_inet_ServerAddr, sizeof(g_session.m_inet_ServerAddr));
					}
					else {
						SYSLOG_DEBUG("UDP SERVER Disabled");
						// TODO
						if(g_session.m_sock >= 0) {
							close(g_session.m_sock);
							g_session.m_sock = -1;
						}
					}
				}
				
				if(cur_EchoPlueEnabled != g_session.m_PrevEchoPlusEnabled) {
					SYSLOG_DEBUG("UDP Echo Plus Changed to: %d", cur_EchoPlueEnabled);
					g_session.m_PrevEchoPlusEnabled = cur_EchoPlueEnabled;
				}
			}

			if (!fSockClosed && FD_ISSET(sock_fd, &fdr)) {
				SYSLOG_DEBUG("got event on Socket");
				len_addr = sizeof(clientAddr);
				memset(datagram, 0, sizeof(datagram));
				len_rxpkt = recvfrom(g_session.m_sock, datagram, sizeof(datagram), 0, (struct sockaddr *) &clientAddr, &len_addr);

				SYSLOG_DEBUG("EchoPlusEnabled=%d, len_rxpkt=%d", cur_EchoPlueEnabled, len_rxpkt);
				
				if (cur_EchoPlueEnabled == TRUE) {
					if((g_session.m_inet_SourceAddr.sin_addr.s_addr != INADDR_ANY) && (g_session.m_inet_SourceAddr.sin_addr.s_addr != clientAddr.sin_addr.s_addr ))
						continue;
				}
				
				if(len_rxpkt > 0) {
					datagram[len_rxpkt] = 0;
					
					if (cur_EchoPlueEnabled == TRUE) {
						TEchoPack *tx_buf = (TEchoPack *) &datagram;
						
						SYSLOG_DEBUG("Rxd TestGenSN =%d", ntohl(tx_buf->TestGenSN));
						
						t = gettimeofday64() - g_session.m_startTimeStamp;
						
						g_statistics.m_PacketReceived++;
						g_statistics.m_BytesReceived += len_rxpkt;
						if (g_statistics.m_PacketReceived == 1) {
							CurTime2Str(g_statistics.m_TimeFirstPacketReceived);
						}
						else {
							CurTime2Str(g_statistics.m_TimeLastPacketReceived);
						}
						g_statistics.m_TestRespSN++;
						tx_buf->TestRespSN = htonl(g_statistics.m_TestRespSN);
						tx_buf->TestRespRecvTimeStamp = htonl((unsigned int) t);
						tx_buf->TestRespReplyFailureCount = htonl(g_statistics.m_TestRespReplyFailureCount);
						
						if(len_rxpkt > sizeof(TEchoPack))
							len_txpkt = len_rxpkt;
						else
							len_txpkt = sizeof(TEchoPack);
						
						t = gettimeofday64() - g_session.m_startTimeStamp;
						tx_buf->TestRespReplyTimeStamp=htonl((unsigned int) t);
					}
					else {
						len_txpkt = len_rxpkt;
					}
					
					do {
						z = sendto(g_session.m_sock, datagram, len_txpkt, 0, (struct sockaddr *) &clientAddr, sizeof(clientAddr));
					} while( z == -1 && errno == EINTR);
					
					if (cur_EchoPlueEnabled == TRUE) {
						if (z == -1) {
							g_statistics.m_TestRespReplyFailureCount++;
							SYSLOG_DEBUG("Sendto Failure TestRespReplyFailureCount=%d, errno=%d: %s", g_statistics.m_TestRespReplyFailureCount, errno, strerror(errno));
						}
						else if (z > 0) {
							g_statistics.m_PacketsResponded++;
							g_statistics.m_BytesResponded += z;
							SYSLOG_DEBUG("Sendto Success PacketsResponded=%d, BytesResponded=%d", g_statistics.m_PacketsResponded, g_statistics.m_BytesResponded);
						}
					}
					set_rdbStatistics();
				}
				else {
					if (cur_EchoPlueEnabled == TRUE) {
						g_statistics.m_TestRespReplyFailureCount++;
						SYSLOG_DEBUG("Recvfrom Failure TestRespReplyFailureCount=%d, errno=%d: %s", g_statistics.m_TestRespReplyFailureCount, errno, strerror(errno));
					}
				}
			}
		}
		
	}

	return 0;
}
