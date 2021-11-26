#ifndef __COMMS_H
#define __COMMS_H
#define MAX_IFNAME_LEN	128
#include <netinet/in.h>

typedef struct TParameters
{
	int	m_running;
	int	m_force_rdb_read;         // force read rdb
	int	m_verbosity;	// verbostiy level
} TParameters;



typedef struct  TConnectSession
{
	int			m_EchoServerEnabled;
	int			m_EchoPlusEnabled;
	int			m_PrevEchoServerEnabled;
	int			m_PrevEchoPlusEnabled;

	char			m_if_name[MAX_IFNAME_LEN];	// ifname

// 	uint32_t		m_ServerAddr;//IPv4 address of the UDPEcho server to bind.
// 	uint16_t		m_ServerPort;//UDP port of the UDPEcho server to bind.

	int 			m_sock;

	struct sockaddr_in 	m_inet_ServerAddr; //IPv4 address and port of the UDPEcho server to bind.
	struct sockaddr_in 	m_inet_SourceAddr; //IPv4 address of the UDPEcho Client allowed.

	int64_t			m_startTimeStamp;

	// from rdb variable
	char*			m_tmpbuf;

	int 			m_stop_by_rdb_change;
}TConnectSession;

typedef struct  TStatistics
{
	// for UDP response packet
	uint32_t		m_TestRespSN;
// 	uint32_t		m_TestRespRecvTimeStamp;
// 	uint32_t		m_TestRespReplyTimeStamp;
	uint32_t		m_TestRespReplyFailureCount;

	//for setting rdb
	unsigned int		m_PacketReceived;
	unsigned int		m_PacketsResponded;
	unsigned int		m_BytesReceived;
	unsigned int		m_BytesResponded;
	char			m_TimeFirstPacketReceived[32];
	char			m_TimeLastPacketReceived[32];

}TStatistics;

typedef struct TEchoPack{
/*	UDP part
	unsigned short SourcePort;
	unsigned short DestinationPort;
	unsigned short Length;
	unsigned short Checksum;
	*/
	uint32_t TestGenSN;
	uint32_t TestRespSN;
	uint32_t TestRespRecvTimeStamp;
	uint32_t TestRespReplyTimeStamp;
	uint32_t TestRespReplyFailureCount;
} __attribute__((packed)) TEchoPack;

#define TRUE     1
#define FALSE   0
#endif
