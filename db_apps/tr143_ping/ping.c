/* ----------------------------------------------------------------------------
Ping main program

Lee Huang<leeh@netcomm.com.au>

*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/resource.h>

#include "log.h"

#include "ping.h"
#include "comms.h"

#include "rdb_event.h"
#include "utils.h"

//#define _DEBUG_PACK
#define DEBUG_PACK_MSG  FASTLOG_DEBUG

#define INT32_OVERFLOOR __INT64_C(0x100000000)

#define OVERFLOW_CHECK_NUM 	0x1000000



// normally x1 < x2
// x1 --10, x2 --100
// x2 -- 0xfff000 x2 -- 100 --overflow
#define INT_OVERFLOW(x1, x2) (x1 >x2 &&  x2 < OVERFLOW_CHECK_NUM && x1 > (0x100000000LLU-OVERFLOW_CHECK_NUM))

#define ABS(x) (x>0?x:-x)
#define MAX_DELAY	10000000		//10s





//#define _PERF_TEST
#ifdef _PERF_TEST

struct perf_data
{
	int64_t start_ms;
	int64_t	total_ms;
	int 	count;
} g_perf1[10], g_perf2[10];


#define PERF1_RECORD_START(n) {\
	g_perf1[n].start_ms=gettimeofday64();\
	};

#define PERF1_RECORD_END(n) {\
	int64_t end_ms=gettimeofday64();\
	g_perf1[n].total_ms += end_ms- g_perf1[n].start_ms;\
	g_perf1[n].count++;}

#define PERF1_RECORD_END_START(n1, n2){\
	int64_t end_ms=gettimeofday64();\
	{\
		g_perf1[n1].total_ms += end_ms- g_perf1[n1].start_ms;\
		g_perf1[n1].count++;}\
	g_perf1[n2].start_ms = end_ms;\
	}

#define PERF1_PRINT(n){int i;\
	for(i =0; i< n ; i++)	{\
		printf("%d: %lld/%d =%lld\n", i, g_perf1[i].total_ms, g_perf1[i].count,g_perf1[i].total_ms/g_perf1[i].count);\
	}}

#define PERF2_RECORD_START(n) {\
	g_perf2[n].start_ms=gettimeofday64();\
	};

#define PERF2_RECORD_END(n) {\
	int64_t end_ms=gettimeofday64();\
	g_perf2[n].total_ms += end_ms- g_perf2[n].start_ms;\
	g_perf2[n].count++;}

#define PERF2_RECORD_END_START(n1, n2){\
	int64_t end_ms=gettimeofday64();\
	{\
		g_perf2[n1].total_ms += end_ms- g_perf2[n1].start_ms;\
		g_perf2[n1].count++;}\
	g_perf2[n2].start_ms = end_ms;\
	}


#define PERF2_PRINT(n){int i;\
	for(i =0; i< n; i++)	{\
		printf("%d: %lld/%d =%lld\n",i, g_perf2[i].total_ms, g_perf2[i].count,g_perf2[i].total_ms/g_perf2[i].count);\
	}}

#else

#define PERF1_RECORD_START(n)
#define PERF1_RECORD_END(n)
#define PERF1_RECORD_END_START(n1, n2)
#define PERF1_PRINT(n)
#define PERF2_RECORD_START(n)
#define PERF2_RECORD_END(n)
#define PERF2_RECORD_END_START(n1, n2)
#define PERF2_PRINT(n)
#endif


sem_t recv_packet_semaphone;

pthread_t recvTid =0;
void *socket_recv_thread(void *pData);


// process_packet process as many as possible recv packet in buffer
int process_received_packet(TConnectSession *pSession);

void calculate_rtd(TConnectSession *pSession);
void update_raw_data(TConnectSession *pSession);
// update interface stats
int update_ping_test_stats(TConnectSession*pSession);


int is_ping_canceled(TParameters *pParam)
{
	rdb_get_i_str(Diagnostics_UDPEcho_Changed, pParam->m_session_id, NULL, 0);
	if(pParam->m_console_test_mode)
	{
		if(get_ping_starttest(pParam->m_session_id) ==0) return 1;
	}
	else
	{
		if(get_ping_state(pParam->m_session_id) == DiagnosticsState_Code_Cancelled) return 1;
	}
	return 0;
}

int ping(TParameters *pParameters, TConnectSession *pSession)
{

	int byte_written;

	//TEchoPack rx_buffer;
	TEchoPack tx_buffer;
	unsigned int TestGenSN;


	int error =0;


	int64_t last_sending_time=0;

	NTCLOG_INFO( "ping>>>\n");

	error = make_connect(pSession);
	if(error)
	{
		error =DiagnosticsState_Error_InitConnectionFailed;
		return error;
	}

	/* create, initialize semaphore */
	if( sem_init(&recv_packet_semaphone,0,0) < 0)
    {
      error =DiagnosticsState_Error_InitConnectionFailed;
      return error;
    }

	//TEchoPack *rx_pack = (TEchoPack*)&rx_buffer;
	TEchoPack *tx_pack = (TEchoPack*)&tx_buffer;


	pSession->m_startTimeStamp = gettimeofday64();
	if(pSession->m_raw_data_fp)
	{
		PERF2_RECORD_START(1);
		fprintf(pSession->m_raw_data_fp, "Local start time(us): %llu (Gsi and Gri are relative to this value)\n",  pSession->m_startTimeStamp);
	}


	// initlalize thread state
	pSession->m_waiting_last_packet= 0;
	pSession->m_stop_recv_thread = 0;
	pSession->m_recv_thread_term =0;
	pSession->m_last_recv_thread_error=0;

	pSession->m_recv_packet_count =0;
	pSession->m_recv_packet_seq =0;
	pSession->m_process_packet_seq=0;


	pSession->m_BytesSent =0;// 	readonly 	0 		Bytes sent.

	pSession->m_BytesReceived=0;// 	readonly 	0 		Bytes received.

	pSession->m_PacketsSent=0;// 	readonly 	0 		Packets sent to server.

	pSession->m_PacketsSendLoss=0; //  readonly 	0 		Packets loss in sending

	pSession->m_PacketsReceived=0;// 	readonly 	0 		Packets received from server.

	pSession->m_PacketsReceiveLoss=0;// 	readonly 	0 		Packets loss in receiving.

	pSession->m_PacketsLossPercentage=0; //  readonly 	0 		Packets loss percentage (decimal as string).

	pSession->m_SendPathDelayDeltaJitterMin=INT64_MAX;
	pSession->m_SendPathDelayDeltaJitterMax=INT64_MIN;
	pSession->m_ReceivePathDelayDeltaJitterAverage=0;
	pSession->m_ReceivePathDelayDeltaJitterMin=INT64_MAX;
	pSession->m_ReceivePathDelayDeltaJitterMax=INT64_MIN;
	pSession->m_RTTAverage =0;
	pSession->m_RTTMin = INT64_MAX;
	pSession->m_RTTMax = INT64_MIN;
	pSession->m_clock_delta=0;
#ifdef _KERNEL_UDPECHO_TIMESTAMP_	
	pSession->m_kclock_delta =0;
#endif

	pSession->m_TestRespSN_first =0;
	pSession->m_TestRespSN_last =0;
	pSession->m_failurecount_first =0;
	pSession->m_failurecount_last =0;

	pSession->m_TestGenSN_first =2;
	pSession->m_TestGenSN_last = pSession->m_PacketCount+1;

	if(pSession->m_sendbuf) memset(pSession->m_sendbuf, 0, pSession->m_PacketSize);
	if(pSession->m_statData) memset(pSession->m_statData, 0 , (pSession->m_PacketCount+2)*sizeof(TStatData));


	error = pthread_create (&recvTid, NULL, socket_recv_thread, pSession);
	if( error )
	{
		error =  DiagnosticsState_Error_InitConnectionFailed;
		goto lab_end;
	}

	//NTCLOG_DEBUG("PacketCount=%d",(pSession->m_PacketCount+1));
    for( TestGenSN =1; TestGenSN <= pSession->m_TestGenSN_last; TestGenSN++)
    {
		int64_t this_sending_time;
		memset(tx_pack, 0, sizeof(TEchoPack));
		tx_pack->TestGenSN = htonl(TestGenSN);

		//wait for interval before sending next packet
		if(TestGenSN < pSession->m_TestGenSN_first)
		{
			// send packet to build up connection
		}
		else if(TestGenSN == pSession->m_TestGenSN_first)
		{
			int i;
			// wait 8 second in maximium for connection startup,
			// otherwise the first few packet may lose
			// the TestGenSN packet will be discarded
			for(i=0; i < 80 && pSession->m_recv_packet_count ==0; i++)
			{
				if(pSession->m_recv_thread_term) break;
				usleep(100000); //100ms
			}

		}
		else // TestGenSN > pSession->m_TestGenSN_first
		{
			if(pSession->m_PacketInterval)
			{
				int64_t duration=0;
				do{
					duration = gettimeofday64() -pSession->m_startTimeStamp -last_sending_time;

					if(pSession->m_PacketInterval > duration)
					{
						// after thread terminte process all left packet
						if(sem_trywait(&recv_packet_semaphone) ==0)
						{
							process_received_packet(pSession);
						}
						else
						{
							usleep(100); //0.1ms
						}
					}
				}while(pSession->m_PacketInterval > duration);

			}
		}

		memcpy(pSession->m_sendbuf, tx_pack, sizeof(TEchoPack));

		/* wait for the send buffer available. 
		 otherwise the sending timestamp  may be incorrect*/
		sock_select(pSession, 5);
		
		this_sending_time  =gettimeofday64() -pSession->m_startTimeStamp;

		PERF1_RECORD_START(0);

		pSession->m_statData[TestGenSN].m_Gsi = this_sending_time;


        byte_written = sock_send(pSession);
        
		PERF1_RECORD_END(0);
		
		if(pSession->m_raw_data_fp)
		{
			PERF1_RECORD_START(1);

			fprintf(pSession->m_raw_data_fp, "S,%llu,%u\n",  this_sending_time, TestGenSN);
			PERF1_RECORD_END(1);
		}


		last_sending_time = this_sending_time;

#ifdef _DEBUG_PACK
		DEBUG_PACK_MSG("ping: send TestGenSN=%d/%d", TestGenSN, pSession->m_PacketCount+1);
#endif
		if(byte_written <0)
        {
			NTCLOG_NOTICE("Error send %d", errno);
			error =DiagnosticsState_Error_TransferFailed;
			goto lab_end;
        }
		if(TestGenSN < pSession->m_TestGenSN_first) continue; // ignore first packet

		pSession->m_PacketsSent++;
        pSession->m_BytesSent += byte_written;



		if(poll_rdb(0,1) >0)
		{
			if(is_ping_canceled(pParameters))
			{
				pSession->m_stop_by_rdb_change =1;
				error =DiagnosticsState_Error_Cancelled;
				goto lab_end;
			}
		}

		if(pParameters->m_running == 0 )
		{
			error = DiagnosticsState_Error_Cancelled;
			goto lab_end;
		}

		if(pSession->m_last_recv_thread_error )
		{
			error = pSession->m_last_recv_thread_error;
			goto lab_end;
		}

		// try process recv packet as soon as possible
		while(sem_trywait(&recv_packet_semaphone) ==0)
		{
			process_received_packet(pSession);
		}

    }// for( TestGenSN =1; TestGenSN <= TestGenSN < pSession->m_TestGenSN_last; TestGenSN++)

	error =0;

lab_end:

	if( recvTid)
	{
		if(error)
		{
			// recv thread force terminated
			/// sending error
			pSession->m_stop_recv_thread= 1;
#ifdef _DEBUG_PACK
			DEBUG_PACK_MSG("ping: Sending error, stop thread");
#endif

		}
		else
		{
			usleep(pSession->m_StragglerTimeout*500); // wait half of m_StragglerTimeout, then stop thread.

			pSession->m_waiting_last_packet=1;
			// busy wait continue process until finish
			while (!pSession->m_recv_thread_term)
			{
				if(sem_trywait(&recv_packet_semaphone) ==0)
				{
					process_received_packet(pSession);
				}

			}
			// after thread terminte process all left packet
			while(sem_trywait(&recv_packet_semaphone) ==0)
			{
				process_received_packet(pSession);
			}


			if(pSession->m_last_recv_thread_error )
			{
				error = pSession->m_last_recv_thread_error;
			}
		}
		pthread_join(recvTid, NULL);

	}

	sem_destroy(&recv_packet_semaphone);

	///5).PacketsLossPercentage
	if(pSession->m_PacketsSent>0)
	{
		int rttpacketlost =  pSession->m_PacketsSent - pSession->m_PacketsReceived;
		calculate_rtd(pSession);
		
		pSession->m_PacketsLossPercentage = (rttpacketlost)*10000 /pSession->m_PacketsSent;
		if(pSession->m_PacketsReceived)
		{
			unsigned int respSN = pSession->m_TestRespSN_last - pSession->m_TestRespSN_first +1;
			unsigned int failurecount = pSession->m_failurecount_last - pSession->m_failurecount_first;
			//Sent packet loss = TestGenSN – (TestRespSN + TestRespReplyFailureCount)
#ifdef _DEBUG_PACK

				DEBUG_PACK_MSG("ping: respSN %d-%d+1 =%d, failurecount=%d, rttpacketloss=%d",pSession->m_TestRespSN_last, pSession->m_TestRespSN_first, respSN, failurecount, rttpacketlost);
#endif
			pSession->m_PacketsSendLoss = pSession->m_PacketsSent - (respSN + failurecount);
			if (pSession->m_PacketsSendLoss < 0 )pSession->m_PacketsSendLoss =0;
			if(pSession->m_PacketsSendLoss > rttpacketlost) pSession->m_PacketsSendLoss =rttpacketlost;
		}
		pSession->m_PacketsReceiveLoss = rttpacketlost - pSession->m_PacketsSendLoss;
	}
	
	update_ping_test_stats(pSession);
	
	update_all_session(pSession, error ==0);

	update_raw_data(pSession);

	close_connect(pSession);

	//if ping stopped and caused by user request
	if(error && pSession->m_stop_by_rdb_change){
		error = DiagnosticsState_Error_Cancelled;
	}

	NTCLOG_INFO("ping %d<<<\n", error);

	PERF1_PRINT(2);
	PERF2_PRINT(0);
	return error;
}//int ping(TParameters *pParameters, TConnectSession *pSession)

void * socket_recv_thread(void *pData)
{
	int error =0;
	int byte_read;
	TEchoPack rx_buffer;

	int64_t	Gri =0;


	TEchoPack *rx_pack = (TEchoPack*)&rx_buffer;
	TConnectSession *pSession = (TConnectSession*) pData;

	NTCLOG_INFO("socket_recv_thread >>>\n");

	setpriority(PRIO_PROCESS, 0, -15);

// 1) while state is not finished
	while(!pSession->m_stop_recv_thread )
	{

		byte_read = sock_recv(pSession, rx_pack, &Gri);

		if(byte_read <0)
		{
			if(errno == EAGAIN)
			{
				// 4) if receiving state already finished, exit simplily
				if( pSession->m_stop_recv_thread) break;

				NTCLOG_INFO("sock_recv: timeout");

				//2)  if last packet has been sent, no recv packet,
				// until StragglerTimeout, then report timeout ?
				if(pSession->m_waiting_last_packet)
				{
					/// recv thread forced terminate:
					/// wait for last packet and last sock_recv timeout
					pSession->m_stop_recv_thread = 1;
					//2.1) if no any recv packet, report timeout
					if (pSession->m_PacketsReceived ==0)
					{
						error = DiagnosticsState_Error_Timeout;
					}
				}

				continue; // continue with next receving
			}
			NTCLOG_NOTICE("Error receive");
			/// recv thread forced terminate
			/// socket error
			pSession->m_stop_recv_thread =1;
			error = DiagnosticsState_Error_NoResponse;
			goto out;
		}
		//6) check valid TechoPack
		if(byte_read >= sizeof(TEchoPack))
		{
			// 6.1) get correct byte order of data
			rx_pack->TestGenSN = ntohl(rx_pack->TestGenSN);
			//6.2.1) all received bytes
			pSession->m_recv_packet_count ++;
			//NTCLOG_DEBUG("recv_thread: recv_count=%d, TestGenSN=%d", pSession->m_recv_packet_count, rx_pack->TestGenSN);
			//6.2) if it is valid TestGenSN, save it
			if(rx_pack->TestGenSN >=  pSession->m_TestGenSN_first && rx_pack->TestGenSN <=  pSession->m_TestGenSN_last)
			{
				TStatData *pStats = &pSession->m_statData[rx_pack->TestGenSN];
				pSession->m_BytesReceived += byte_read;

				//6.3) the packed is really sent
				if(pStats->m_Gsi)
				{
					//6.4) the packet has not received
					if( pStats->m_received ==0)
					{
						// save received packet
						pStats->m_received=1;
						pStats->m_Gri= Gri;
						pStats->m_rawData.TestRespSN =rx_pack->TestRespSN;
						pStats->m_rawData.TestRespRecvTimeStamp= rx_pack->TestRespRecvTimeStamp;
						pStats->m_rawData.TestRespReplyTimeStamp= rx_pack->TestRespReplyTimeStamp;
						pStats->m_rawData.TestRespReplyFailureCount= rx_pack->TestRespReplyFailureCount;
#ifdef _KERNEL_UDPECHO_TIMESTAMP_

						if(pSession->m_kclock_delta == 0)
						{
							pSession->m_kclock_delta = rx_pack->gre_gsi;
						}
						pStats->m_rawData.kgsi =rx_pack->kgsi - pSession->m_kclock_delta;
						pStats->m_rawData.kgri =rx_pack->kgri- pSession->m_kclock_delta;
						pStats->m_rawData.gre_gsi =rx_pack->gre_gsi - pSession->m_kclock_delta;
						pStats->m_rawData.gre_gri =rx_pack->gre_gri- pSession->m_kclock_delta;
#endif

						// set index for this received packet
						pSession->m_statData[pSession->m_recv_packet_seq].m_TestGenSN_next= rx_pack->TestGenSN;
						//DEBUG_PACK_MSG("recv_thread: seq=%d, TestGenSN=%d, TestRespSN=%d\n", pSession->m_recv_packet_seq, rx_pack->TestGenSN, ntohl(rx_pack->TestRespSN));
						pSession->m_recv_packet_seq++;

						sem_post(&recv_packet_semaphone);
						if(pSession->m_recv_packet_seq >= pSession->m_PacketCount) break;
						// notify
					}
					else
					{
#ifdef _DEBUG_PACK
						DEBUG_PACK_MSG("recv_thread: TestGenSN=%u is received again, ignore it", rx_pack->TestGenSN);
#endif
					}//if( pStats->m_received ==0)
				}
				else
				{
#ifdef _DEBUG_PACK
						DEBUG_PACK_MSG("recv_thread: TestGenSN=%u is never sent", rx_pack->TestGenSN);
#endif
				}//if(pStats->m_Gsi)

			}// if( valid rx_pack->TestGenSN )


		}
		else
		{
			error =DiagnosticsState_Error_IncorrectSize;

		}//if(byte_read >= sizeof(TEchoPack))

	}//while(!pSession->m_stop_recv_thread )
out:
	pSession->m_last_recv_thread_error = error;
	pSession->m_recv_thread_term =1;

	NTCLOG_INFO("socket_recv_thread %d<<<\n", error);
	
	return 0;
}//void * socket_recv_thread(void *pData)

// process_receiveed_packet process as many as possible recv packet in buffer
int process_received_packet(TConnectSession *pSession)
{
	int error =0;
	TEchoPack 	rx_buffer;



	TEchoPack *rx_pack = (TEchoPack*)&rx_buffer;

	TStatData *pStats1, *pStats;
	// 2) get the slot which receive thread put a mark int
	pStats1 = &pSession->m_statData[pSession->m_process_packet_seq];
	pSession->m_process_packet_seq++;
#ifdef _DEBUG_PACK
	DEBUG_PACK_MSG("\nrecv: packet= %d, TestGenSN=%u", pSession->m_process_packet_seq, pStats1->m_TestGenSN_next);
#endif

	//3 ) valid TestRespSN
	if(pStats1->m_TestGenSN_next > 0)
	{
		rx_pack->TestGenSN = pStats1->m_TestGenSN_next;
		pStats = &pSession->m_statData[rx_pack->TestGenSN];
		rx_pack->TestRespSN = ntohl(pStats->m_rawData.TestRespSN);


		// 4) get correct byte order of data
		rx_pack->TestRespRecvTimeStamp= ntohl(pStats->m_rawData.TestRespRecvTimeStamp);
		rx_pack->TestRespReplyTimeStamp= ntohl(pStats->m_rawData.TestRespReplyTimeStamp);
		rx_pack->TestRespReplyFailureCount= ntohl(pStats->m_rawData.TestRespReplyFailureCount);

#ifdef _KERNEL_UDPECHO_TIMESTAMP_

		rx_pack->kgsi =	pStats->m_rawData.kgsi;
		rx_pack->kgri = pStats->m_rawData.kgri;
		rx_pack->gre_gsi =	pStats->m_rawData.gre_gsi;
		rx_pack->gre_gri = pStats->m_rawData.gre_gri;
		
#endif

		//7) get sending information
		//Gsi = pStats->m_Gsi;
		//Gri = pStats->m_Gri;
		if(pSession->m_raw_data_fp)
		{
						
#ifdef _KERNEL_UDPECHO_TIMESTAMP_
			fprintf(pSession->m_raw_data_fp, "R,%llu,%u,%llu,%u,%u,%u,%u,%llu,%llu,%llu,%llu\n",
						pStats->m_Gsi,
						rx_pack->TestGenSN,
						pStats->m_Gri,
						rx_pack->TestRespSN,
						rx_pack->TestRespRecvTimeStamp,
						rx_pack->TestRespReplyTimeStamp,
						rx_pack->TestRespReplyFailureCount,
						rx_pack->kgsi,
						rx_pack->kgri,
						rx_pack->gre_gsi,
						rx_pack->gre_gri
						
						);

#else

			fprintf(pSession->m_raw_data_fp, "R,%llu,%u,%llu,%u,%u,%u,%u\n",
						pStats->m_Gsi,
						rx_pack->TestGenSN,
						pStats->m_Gri,
						rx_pack->TestRespSN,
						rx_pack->TestRespRecvTimeStamp,
						rx_pack->TestRespReplyTimeStamp,
						rx_pack->TestRespReplyFailureCount);

#endif
						
		}

		//.... continue process this packet

#ifdef _DEBUG_PACK
		DEBUG_PACK_MSG("recv: tx: Gsi=%lld, Gri=%lld",  pStats->m_Gsi, pStats->m_Gri);
		DEBUG_PACK_MSG("recv: rx: TestGenSN=%u, Rri=%u, Rsi=%u, TestRespSN=%u, failure=%u",
						rx_pack->TestGenSN,  rx_pack->TestRespRecvTimeStamp,
					rx_pack->TestRespReplyTimeStamp, rx_pack->TestRespSN,rx_pack->TestRespReplyFailureCount);
#endif

		//int64_t RrPrevious =Rri;
		//int64_t RsPrevious =Rsi;
		if(pSession->m_PacketsReceived ==0)
		{
			pSession->m_TestRespSN_first = rx_pack->TestRespSN;
			pSession->m_TestRespSN_last= rx_pack->TestRespSN;
			pSession->m_failurecount_first = rx_pack->TestRespReplyFailureCount;
		}
		else
		{
			// record TestRespSN for calculating send/receive failure count
			if(rx_pack->TestRespSN < pSession->m_TestRespSN_first)
			{
				pSession->m_TestRespSN_first = rx_pack->TestRespSN;
				pSession->m_failurecount_first = rx_pack->TestRespReplyFailureCount;
			}

			if(rx_pack->TestRespSN > pSession->m_TestRespSN_last)
			{
				pSession->m_TestRespSN_last= rx_pack->TestRespSN;
				pSession->m_failurecount_last = rx_pack->TestRespReplyFailureCount;
			}
		}
		pStats->m_success =1;

		pSession->m_PacketsReceived++;
		//8) if this is last sent packet
		if(pSession->m_waiting_last_packet)
		{
			//9) if last packet received packet, done!!!
			//if (TestGenSN_last == rx_pack->TestGenSN)
			if(pSession->m_PacketsReceived == pSession->m_PacketsSent)
			{
				/// recv thread normal terminate
				/// last packet is received, terminate as quick as possible
#ifdef _DEBUG_PACK
				//DEBUG_PACK_MSG("process:m_PacketsReceived == pSession->m_PacketsSent");
#endif
				pSession->m_stop_recv_thread= 1;
			}
		}

		error =0;

	}//	if(pStats1->m_TestGenSN_next > 0)


	return 0;
}//int proccess_packet(TConnectSession *pSession)


void calculate_rtd(TConnectSession *pSession)
{


	int64_t 	Gsi =0;
	int64_t	Gri =0;
	int64_t	Rri =0;
	int64_t	Rsi =0;


	
	TEchoPack 	rx_buffer;

	TEchoPack *rx_pack = (TEchoPack*)&rx_buffer;

	int64_t 	send_path_delay_previous =0;
	int64_t 	recv_path_delay_previous =0;
	int64_t	Rri_previous =0;
	int64_t 	Rsi_previous =0;
	
	int 		TestRespSN_previous=0;
	int 		server_clock_turn_over =0;
	int 		TestGenSN=1;
	
	for( TestGenSN= pSession->m_TestGenSN_first; TestGenSN <= pSession->m_TestGenSN_last; TestGenSN ++)
	{
		TStatData *pStats = &pSession->m_statData[TestGenSN];
		if (pStats->m_received ==0)
		{
			// packet lost
			continue;
		}
		rx_pack->TestRespSN = ntohl(pStats->m_rawData.TestRespSN);
		rx_pack->TestRespRecvTimeStamp= ntohl(pStats->m_rawData.TestRespRecvTimeStamp);
		rx_pack->TestRespReplyTimeStamp= ntohl(pStats->m_rawData.TestRespReplyTimeStamp);
		rx_pack->TestRespReplyFailureCount= ntohl(pStats->m_rawData.TestRespReplyFailureCount);


		Gsi = pStats->m_Gsi;
		Gri = pStats->m_Gri;
		
		Rri = rx_pack->TestRespRecvTimeStamp;
		Rsi = rx_pack->TestRespReplyTimeStamp;


		// check clock turn over
		if (server_clock_turn_over  ==0)
		{
			if(Rsi  < Rri)
			{
				server_clock_turn_over =1;
				NTCLOG_DEBUG("process: Rsi < Rri, clock overflow");
				Rsi += INT32_OVERFLOOR;
			}
			else if(rx_pack->TestRespSN > TestRespSN_previous) // in order
			{
				// if current timer  < previous timer
				if(INT_OVERFLOW(Rsi_previous , Rsi) &&
						INT_OVERFLOW(Rri_previous, Rri))  // turn around
				{
					server_clock_turn_over =1;

				}

			}
			else if(rx_pack->TestRespSN < TestRespSN_previous) // inverse order
			{
				// if current time > previous time
				if(INT_OVERFLOW(Rsi, Rsi_previous) &&
						INT_OVERFLOW(Rri, Rri_previous))  // turn around
				{

					server_clock_turn_over =1;
				}

			}
			if(	server_clock_turn_over ==1)
			{
				Rsi += INT32_OVERFLOOR;
				Rri += INT32_OVERFLOOR;
				NTCLOG_DEBUG("process: clock overflow at %d", rx_pack->TestRespSN);
		
			}

		}
		else
		{
			Rsi += INT32_OVERFLOOR;
			Rri += INT32_OVERFLOOR;
		
		}
		TestRespSN_previous = rx_pack->TestRespSN;

		//Round Trip Delay
		//RTDi = Gri - Gsi
		//Effective-RTDi = Gri - Gsi – (TestRespReplyTimeStamp - TestRespRecvTimeStamp)

		int64_t RTD = Gri- Gsi -(Rsi- Rri);
		int64_t send_path_delay = Rri - Gsi;
		int64_t recv_path_delay = Gri - Rsi;
		if(pSession->m_clock_delta==0)
		{
			pSession->m_clock_delta = send_path_delay - RTD/2;
		}
		send_path_delay -= pSession->m_clock_delta;
		recv_path_delay += pSession->m_clock_delta;

#ifdef _DEBUG_PACK
		DEBUG_PACK_MSG("Cal: RTD =%llu, Gri -Gsi =%llu, Rsi- Rri =%llu, clock_delta=%llu", RTD, Gri- Gsi, Rsi- Rri, pSession->m_clock_delta);

		DEBUG_PACK_MSG("Cal: send_path Gsi=%lld -> Rri =%lld, delay = %lld", Gsi, Rri, send_path_delay);
		DEBUG_PACK_MSG("Cal: recv_path Rsi=%lld -> Gri =%lld, delay = %lld", Rsi, Gri, recv_path_delay);
#endif


		pStats->m_stats.m_rtd= (long)RTD;

#ifdef _KERNEL_UDPECHO_TIMESTAMP_
		rx_pack->kgsi =	pStats->m_rawData.kgsi;
		rx_pack->kgri = pStats->m_rawData.kgri;
		pStats->m_stats.m_krtd =(long)(rx_pack->kgri- rx_pack->kgsi -(Rsi- Rri));
		rx_pack->gre_gsi =	pStats->m_rawData.gre_gsi;
		rx_pack->gre_gri = pStats->m_rawData.gre_gri;
		pStats->m_stats.m_gre_rtd =(long)(rx_pack->gre_gri- rx_pack->gre_gsi -(Rsi- Rri));
		
#endif


		if(pSession->m_PacketsReceived  > 1 &&
			ABS(send_path_delay) <MAX_DELAY &&
			ABS(recv_path_delay) <MAX_DELAY &&
			send_path_delay_previous &&
			recv_path_delay_previous )
		{
			//send Packet Delay
			//Send time delta= Gsi - GsPrevious
			//Receive time delta = Rri – RrPrevious
			//Sent path IPDV (i) = Send time delta – Receive time delta
			int64_t IPDV1, IPDV2;

			IPDV1 = send_path_delay - send_path_delay_previous;




			// recv packet delay
			//Response time delta = Rsi – RsPrevious
			//Receive time delta = Gri - GrPrevious
			//Receive path IPDV (i) = Response time delta – Receive time delta

			//IPDV2 = (Rsi- RsPrevious) - (Gri - GrPrevious);
			IPDV2 = recv_path_delay - recv_path_delay_previous;


#ifdef _DEBUG_PACK


			DEBUG_PACK_MSG("Cal: IPDV1 =%lld,IPDV2 =%lld\n", IPDV1, IPDV2);
#endif
			pStats->m_stats.m_send_path_IPDV=(long)IPDV1;
			pStats->m_stats.m_recv_path_IPDV=(long)IPDV2;

			//NTCLOG_DEBUG("IPDV1=%ld.%ld, IPDV2=%ld.%ld",
			//				IPDV1.tv_sec, IPDV1.tv_usec,IPDV2.tv_sec, IPDV2.tv_usec);
		}
		else
		{
			pStats->m_stats.m_send_path_IPDV=0;
			pStats->m_stats.m_recv_path_IPDV=0;

		}// send(recv)_path_delay_previous is valid

		send_path_delay_previous = send_path_delay;
		recv_path_delay_previous = recv_path_delay;
		Rsi_previous = Rsi;
		Rri_previous = Rri;
		
	}
	
}//int calculate_rtd(TConnectSession *pSession, TComputeData *pData)


void update_raw_data(TConnectSession *pSession)
{

	if(pSession->m_statData && pSession->m_raw_data_fp)
	{
		char line[256];
		FILE *fp = fopen(RAW_DATA_FILE_TMP, "wt");
		if(fp)
		{
			fflush(pSession->m_raw_data_fp);
			fseek(pSession->m_raw_data_fp,0,SEEK_SET);
			while(fgets(line, 255, pSession->m_raw_data_fp))
			{
				if(line[0] =='S' && line[1] ==',')
				{
					int64_t Gsi;
					unsigned int TestGenSN;
					if( sscanf(line, "S,%llu,%u", &Gsi, &TestGenSN) ==2)
					{
						if(TestGenSN  < pSession->m_TestGenSN_first)
						{
							fputs(line, fp);
						}
						else if ( TestGenSN <= pSession->m_TestGenSN_last )
						{
							TStatData *pStat =& pSession->m_statData[TestGenSN ];
							if(pStat->m_received)
							{
								if(pStat->m_success)
								{
#ifdef _KERNEL_UDPECHO_TIMESTAMP_
									fprintf(fp, "S,%llu,%u,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f\n", Gsi, TestGenSN,
									(double)pStat->m_stats.m_rtd/1000, 
									(double)pStat->m_stats.m_send_path_IPDV/1000, 
									(double)pStat->m_stats.m_recv_path_IPDV/1000,
									(double)pStat->m_stats.m_krtd/1000,
									(double)pStat->m_stats.m_gre_rtd/1000
									);

#else
									fprintf(fp, "S,%llu,%u,%0.3f,%0.3f,%0.3f\n", Gsi, TestGenSN,
									(double)pStat->m_stats.m_rtd/1000, 
									(double)pStat->m_stats.m_send_path_IPDV/1000, 
									(double)pStat->m_stats.m_recv_path_IPDV/1000);
#endif

								
								
								}
								
							}
							else
							{
								fprintf(fp, "S,%llu,%u,lost\n",Gsi, TestGenSN);
							
							} 
						
						}
					}
					
				}
				else if(line[0] =='S' && line[1] =='e' && line[2] =='n'&&line[3] =='d')
				{
#ifdef _KERNEL_UDPECHO_TIMESTAMP_
					fprintf(fp,"Sending line format: \"S,Gsi,TestGenSN,rtt,S_IPDV,R_IPDV,krtt,gre_rtt\"\n");
#else
					fprintf(fp,"Sending line format: \"S,Gsi,TestGenSN,rtt,S_IPDV,R_IPDV\"\n");
#endif
					fprintf(fp, "\"Clock Offset\"(us): %lld\n", pSession->m_clock_delta);
					
				}
				else
				{
					fputs(line, fp);
				}
				
			}
			fclose(fp);
			fclose(pSession->m_raw_data_fp);
			pSession->m_raw_data_fp =NULL;
			rename(RAW_DATA_FILE_TMP, RAW_DATA_FILE);
		}//if(fp)
			
	
	}
	
}



// update interface stats
int update_ping_test_stats(TConnectSession*pSession)
{
	int64_t rx, tx;
	int rx_pkts, tx_pkts;
	int error=0;
	if(pSession->m_test_ready)
	{
		if(get_if_rx_tx(pSession->m_if_name, &rx,&tx, &rx_pkts, &tx_pkts))
		{
			pSession->m_if_rx = rx - pSession->m_if_rx;
			pSession->m_if_tx = tx - pSession->m_if_tx;
			pSession->m_if_rx_pkts = rx_pkts - pSession->m_if_rx_pkts;
			pSession->m_if_tx_pkts = tx_pkts - pSession->m_if_tx_pkts;
			error=-1;
		}
		if(get_if_rx_tx(pSession->m_sedge_if_name, &rx,&tx, &rx_pkts, &tx_pkts))
		{
			pSession->m_sedge_if_rx = rx - pSession->m_sedge_if_rx;
			pSession->m_sedge_if_tx = tx - pSession->m_sedge_if_tx;
			pSession->m_sedge_if_rx_pkts = rx_pkts - pSession->m_sedge_if_rx_pkts;
			pSession->m_sedge_if_tx_pkts = tx_pkts - pSession->m_sedge_if_tx_pkts;
			error=-1;
		}
	}
	return error;
}
