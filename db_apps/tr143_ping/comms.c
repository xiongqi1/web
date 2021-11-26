#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "log.h"
#include "comms.h"
#include "rdb_event.h"
#include "utils.h"


void init_connection(TConnectSession *pConnection, TParameters* pParameters, const char* pProtocolName)
{
	memset(pConnection, 0 ,sizeof(TConnectSession));
	pConnection->m_protocol_name= pProtocolName;
	pConnection->m_sock =-1;
	pConnection->m_parameters = pParameters;

}

/// connect to sever address
///$ 0 -- success
///$ <0-- error code
int  make_connect(TConnectSession *pSession)
{
	struct timespec tv;
	struct sockaddr_in myaddr;
	int i;
	int sockbufsize;
	
	NTCLOG_INFO("make_connect, interval=%d us\n", pSession->m_PacketInterval);
    pSession->m_sock= socket(AF_INET, SOCK_DGRAM, 0);
    if (pSession->m_sock < 0)
    {
        NTCLOG_ERR("cannot open socket");
        return DiagnosticsState_Error_InitConnectionFailed;
    }

	// response time
	tv.tv_sec = pSession->m_StragglerTimeout/1000;
	tv.tv_nsec = (pSession->m_StragglerTimeout%1000)*1000;

    setsockopt( pSession->m_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( struct timespec ));

	if(pSession->m_PacketInterval < 10000) // 10ms
	{
		// default: 0xda00(36KB)
		// the recvbuf size is increased with shorter m_PacketInterval and larger m_PacketSize
		// interval=0, size=20, =>0x24000 
		// interval=0, size=32, =>0x24800 
		// interval=0, size=1024, =>0x34000 
		// interval=99, size=20, =>0x20000 
		sockbufsize=0x20000 + pSession->m_PacketSize/16*0x400 + ((pSession->m_PacketInterval==0)?0x4000:0); //(128KB+) allow "-w0 -c100000" without packet loss on UDP
	}
	else
	{
		sockbufsize=0x20000;
	}
	setsockopt(pSession->m_sock, SOL_SOCKET, SO_RCVBUF, &sockbufsize, (int)sizeof(sockbufsize));
	
   
#if 1
	bzero(&myaddr,sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = pSession->m_local_addr;
	// try 3 times to bind local address
	for ( i =0; i< 3; i++)
	{
		myaddr.sin_port = htons(pSession->m_local_port +i);
		if(bind(pSession->m_sock,(struct sockaddr *)&myaddr,sizeof(myaddr)) <0)
		{
			//error = DiagnosticsState_Error_InitConnectionFailed;
			//close(pSession->m_sock);
			//return error;

		}
		else
		{
			pSession->m_local_port +=i;
			NTCLOG_DEBUG("Bind local address %s:%d", inet_ntoa(*((struct in_addr*)&pSession->m_local_addr)),pSession->m_local_port);
			break;
		}
	}
#endif

    bzero((char*)&pSession->m_remote_addr, sizeof( pSession->m_remote_addr));
    pSession->m_remote_addr.sin_family = AF_INET;
    pSession->m_remote_addr.sin_addr.s_addr = pSession->m_ServerAddress;
    pSession->m_remote_addr.sin_port = htons(pSession->m_ServerPort);  /* Destination Port */
	//NTCLOG_DEBUG("Server Address:%x, port:%d", pSession->m_ServerAddress, pSession->m_ServerPort);

	if(pSession->m_parameters->m_dump_raw_data)
	{
		pSession->m_raw_data_fp = fopen(RAW_DATA_FILE, "w+t");
		if(pSession->m_raw_data_fp ==0)		return DiagnosticsState_Error_InitConnectionFailed;
		setvbuf(pSession->m_raw_data_fp, 0, _IOFBF, 63000);
		fprintf(pSession->m_raw_data_fp,"Sending line format: \"S,Gsi,TestGenSN\"\n");
#ifdef _KERNEL_UDPECHO_TIMESTAMP_
		fprintf(pSession->m_raw_data_fp,"Receiving line format: \"R,Gsi,TestGenSN,Gri,TestRespSN,TestRespRecvTimeStamp,TestRespReplyTimeStamp,TestRespReplyFailureCount,kGsi,kGri,gre_Gsi,gre_Gri\"\n");

#else
		fprintf(pSession->m_raw_data_fp,"Receiving line format: \"R,Gsi,TestGenSN,Gri,TestRespSN,TestRespRecvTimeStamp,TestRespReplyTimeStamp,TestRespReplyFailureCount\"\n");
#endif
	}
	return 0;
}

void close_connect(TConnectSession *pSession)
{
	NTCLOG_INFO("close_connect,m_sock=%d\n",pSession->m_sock);
	if(pSession->m_sock >=0)
	{
		close(pSession->m_sock);

	}
	pSession->m_sock =-1;
	
}

int sock_select(TConnectSession* pSession, int seconds)
{
/* initialize loop variables */
	fd_set fdw;
	int selected;
	struct timeval tv = { .tv_sec = seconds, .tv_usec =0};

	FD_ZERO(&fdw);

	/* put database into fd read set */
	FD_SET(pSession->m_sock, &fdw);

	/* select */
	selected = select(pSession->m_sock+1 , NULL, &fdw, NULL, &tv);
	if (selected > 0  )
	{
		if(FD_ISSET(pSession->m_sock, &fdw))	return selected;// detect fd
		return 0;
	}
	// could be 0, or -1
	return selected;
}
// send packet, the real data buffer is pSession->m_sendbuf
int sock_send(TConnectSession* pSession)
{

	return sendto(pSession->m_sock, pSession->m_sendbuf, pSession->m_PacketSize, 0,   (struct sockaddr*)&pSession->m_remote_addr, (socklen_t)sizeof(pSession->m_remote_addr));
}

// recv packet, the real data buffer is pSession->m_sendbuf
int sock_recv(TConnectSession* pSession, TEchoPack *rx_pack, int64_t *recving_time)
{
	int byte_read;
	struct sockaddr_in recv_addr;
	socklen_t recv_addr_len = sizeof(struct sockaddr_in);

	byte_read = recvfrom(pSession->m_sock, pSession->m_recvbuf, pSession->m_PacketSize, 0, (struct sockaddr*)&recv_addr, &recv_addr_len);
	//NTCLOG_DEBUG("recv %d", byte_read);
	*recving_time = gettimeofday64() -pSession->m_startTimeStamp;
	if(byte_read >= sizeof(TEchoPack))
	{
		memcpy(rx_pack, pSession->m_recvbuf, sizeof(TEchoPack));
		//NTCLOG_DEBUG("recv %d", rx_pack->TestGenSN);
	}
	return byte_read;

}
