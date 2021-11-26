#ifndef __COMMS_H
#define __COMMS_H
#define MAX_IFNAME_LEN	32
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include "parameters.h"
#include <sys/time.h>

#define MAX_GS_CACHE 1000
#define DEFAULT_RECV_TIMEOUT 	1 //ms



static const struct timeval MAX_TIMEVAL = {LONG_MAX, LONG_MAX};
static const struct timeval MIN_TIMEVAL = {LONG_MIN, LONG_MIN};

typedef struct TStatData
{
	int64_t m_Gsi;		// sending timestamp
	int64_t m_Gri;		// receiving timestamp
	union
	{
		struct {
			long	m_rtd;
			long  	m_send_path_IPDV;
			long 	m_recv_path_IPDV;
#ifdef _KERNEL_UDPECHO_TIMESTAMP_
			long 	m_krtd;
			long 	m_gre_rtd;
#endif

		}m_stats;
		struct {
			unsigned int TestRespSN;
			unsigned int TestRespRecvTimeStamp;
			unsigned int TestRespReplyTimeStamp;
			unsigned int TestRespReplyFailureCount;
#ifdef _KERNEL_UDPECHO_TIMESTAMP_
			int64_t	   kgsi;
			int64_t 	   kgri;
			int64_t	   gre_gsi;
			int64_t 	   gre_gri;
#endif

			
		}m_rawData;
	};
	int 	m_TestGenSN_next;//point to received TestGenSN
	char 	m_received; // whether this packet is received;
	char	m_success; // the valid packet
}TStatData;


typedef struct  TConnectSession
{
	TParameters*	m_parameters;
	const char*		m_protocol_name;
	char			m_if_name[MAX_IFNAME_LEN+1];	// ifname
	
	int				m_if_istemp;	// wether the interface is temporary
	int64_t 		m_if_rx;	// rx bytes from interface
	int64_t		m_if_tx;	// tx bytes from interface
	unsigned int	m_if_cos;	// cos value 0 --tc4, 5 -- tc1, 4 -- tc2
	int				m_if_rx_pkts;	// rx packets from interface
	int				m_if_tx_pkts;	// tx packets from interface

	char			m_sedge_if_name[MAX_IFNAME_LEN+1];	// smartedge ifname
	int64_t 		m_sedge_if_rx;	// rx bytes from interface
	int64_t 		m_sedge_if_tx;	// tx bytes from interface
	int				m_sedge_if_rx_pkts;	// rx packets from interface
	int				m_sedge_if_tx_pkts;	// tx packets from interface

	unsigned int 	m_ServerAddress;//IPv4 address of the UDPEcho server to send pings to. Server
									//must be reachable via the test interface


	unsigned int 	m_ServerPort;//UDP port of the UDPEcho server to send pings to

	int 			m_sock;

	struct sockaddr_in m_remote_addr;

	unsigned int 	m_local_addr;
	unsigned int 	m_local_port;

	int64_t			m_startTimeStamp;



	TStatData		*m_statData;


	char*			m_send_recv_buf;

	char*			m_sendbuf;

	char*			m_recvbuf;

	FILE*			m_raw_data_fp;


	unsigned int 	m_PacketCount;// 	readwrite 	0 		Number of ping packets to send. Limited by memory considerations, assignment may return 9004 - resources exceeded.

	unsigned int 	m_PacketSize;// 	readwrite 	20 		Size of packets to send in bytes. This is the payload size, not including the IP and UDP header, the minimum is 20 (to fit UDPEchoPlus fields), the maximum is 65,503 (limited by IPv4).

	unsigned int 	m_PacketInterval;// readwrite 	1000 		Interval between packets sent in milliseconds. Zero means to send as fast as possible.

	unsigned int 	m_StragglerTimeout;// readwrite 	5000 		Time in milliseconds to wait after last packet is sent for replies to arrive.

	int 	m_BytesSent;// 	readonly 	0 		Bytes sent.

	int 	m_BytesReceived;// 	readonly 	0 		Bytes received.

	int 	m_PacketsSent;// 	readonly 	0 		Packets sent to server.

	int 	m_PacketsSendLoss; //  readonly 	0 		Packets loss in sending

	int 	m_PacketsReceived;// 	readonly 	0 		Packets received from server.

	int 	m_PacketsReceiveLoss;// 	readonly 	0 		Packets loss in receiving.

	unsigned int 	m_PacketsLossPercentage; //  readonly 	0 		Packets loss percentage (decimal as string).


	int64_t		m_RTTAverage; //  	readonly 	0 		Average round-trip time in milliseconds (decimal as string).

	int64_t 		m_RTTMax; //  	readonly 	0 		Maximum round-trip time in milliseconds (decimal as string).

	int64_t 		m_RTTMin; //  	readonly 	0 		Minimum round-trip time in milliseconds (decimal as string).


	int64_t		m_SendPathDelayDeltaJitterAverage; // readonly 0 		Average packet delay delta in the up-stream direction (decimal milliseconds as string).

	int64_t 		m_SendPathDelayDeltaJitterMin; //  	readonly 0 		Minimum packet delay delta in the up-stream direction (decimal milliseconds as string).

	int64_t 		m_SendPathDelayDeltaJitterMax; //  	readonly 0 		Maximum packet delay delta in the up-stream direction (decimal milliseconds as string).

	int64_t 		m_ReceivePathDelayDeltaJitterAverage; // readonly 	0 		Average packet delay delta in the down-stream direction (decimal milliseconds as string).

	int64_t 		m_ReceivePathDelayDeltaJitterMin; //  	readonly 	0 		Minimum packet delay delta in the down-stream direction (decimal milliseconds as string).

	int64_t 		m_ReceivePathDelayDeltaJitterMax; //  	readonly 	0 		Maximum packet delay delta in the down-stream direction (decimal milliseconds as string).


	int64_t 		m_clock_delta; // clock delta value between local and remote machine

#ifdef _KERNEL_UDPECHO_TIMESTAMP_	
	int64_t		m_kclock_delta;
#endif

	unsigned int 	m_TestRespSN_first;
	unsigned int 	m_TestRespSN_last;

	unsigned int 	m_failurecount_first;
	unsigned int 	m_failurecount_last;

	unsigned int 	m_TestGenSN_first;
	unsigned int 	m_TestGenSN_last;
	

	int 			m_stop_by_rdb_change;

	int 			m_last_recv_thread_error;


	int 			m_waiting_last_packet;

	int 			m_recv_packet_count;
	int 			m_recv_packet_seq;
	int 			m_process_packet_seq;


	int 			m_stop_recv_thread;
	int 			m_recv_thread_term;

	int 			m_test_ready;
}TConnectSession;

void init_connection(TConnectSession *pConnection, TParameters* pParameters, const char* pProtocolName);

/// connect to sever address
///$ 0 -- success
///$ <0-- error code
int  make_connect(TConnectSession *pSession);

void close_connect(TConnectSession *pSession);


struct TEchoPack;

int sock_select(TConnectSession* pSession, int seconds);

// send packet, the real data buffer is pSession->m_sendbuf
int sock_send(TConnectSession* pSession);
// recv packet, the real data buffer is pSession->m_sendbuf
int sock_recv(TConnectSession* pSession, struct TEchoPack *rx_pack, int64_t *recving_time);


#define RAW_DATA_FILE "/NAND/udpecho.rawdata.txt"

#define RAW_DATA_FILE_TMP "/NAND/udpecho.rawdata.txt.tmp"
#endif
