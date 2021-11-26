#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <math.h>

#include "log.h"
#include "rdb_event.h"
#include "utils.h"

#define LOCAL_BASE_PORT 12000
struct DiagnosticsState_Code
{
	int 	m_code;
	char*	m_desc;
};
static const struct DiagnosticsState_Code g_DiagnosticsState_Code[]=
{
	{DiagnosticsState_Code_None, "None"},
	{DiagnosticsState_Code_Request, "Requested"},
	{DiagnosticsState_Code_Completed, "Completed"},
	{DiagnosticsState_Code_Cancelled, "Cancelled"},
	{DiagnosticsState_Error_InitConnectionFailed, "Error_InitConnectionFailed"},
	{DiagnosticsState_Error_NoResponse, "Error_NoResponse"},
	{DiagnosticsState_Error_TransferFailed, "Error_TransferFailed"},
	{DiagnosticsState_Error_PasswordRequestFailed, "Error_PasswordRequestFailed"},
	{DiagnosticsState_Error_LoginFailed, "Error_LoginFailed"},
	{DiagnosticsState_Error_NoTransferMode, "Error_NoTransferMode"},
	{DiagnosticsState_Error_NoPASV, "Error_NoPASV"},
	{DiagnosticsState_Error_IncorrectSize, "Error_IncorrectSize"},
	{DiagnosticsState_Error_Timeout, "Error_Timeout"},
	{DiagnosticsState_Error_Invalid_URL, "Error_Invalid_URL"},
	{DiagnosticsState_Error_Cancelled, "Error_Cancelled"},
	{0, NULL},

};

int Diagnostics_msg2code(char *msg)
{
	int i = strlen(msg);
    if(i < 2) return DiagnosticsState_Code_None;

    for(i =0;g_DiagnosticsState_Code[i].m_desc;i++)
    {
    	if(strcmp(msg, g_DiagnosticsState_Code[i].m_desc) ==0)
    	{
    		return g_DiagnosticsState_Code[i].m_code;
    	}
    }
    return DiagnosticsState_Code_None;
}


const char *Diagnostics_code2msg(int code)
{
	int i =0;
	for(i =0; g_DiagnosticsState_Code[i].m_desc; i++)
    {
    	if(g_DiagnosticsState_Code[i].m_code == code)
    	{
    		return g_DiagnosticsState_Code[i].m_desc;
    	}
    }
	return 0;
}

/// get state code  from udpecho.diagnosticsstate
int get_ping_state(int session_id)
{
	char buf[60];
    int retValue = rdb_get_i_str(Diagnostics_UDPEcho_DiagnosticsState,  session_id,  buf,60);

    if(retValue)
    {
        return retValue;
    }
    return Diagnostics_msg2code(buf);
}

/// set udpecho.diagnosticsstate by state code
void set_ping_state(int session_id, int state)
{
	const char *p = Diagnostics_code2msg(state);
	if(p)
	{
		rdb_set_i_str(Diagnostics_UDPEcho_DiagnosticsState,  session_id,  p);
	}
}

/// get udpecho.starttest
/// $>=0 -- success
/// $<0 --- error
int get_ping_starttest(int session_id)
{
    int retValue=0;
    int err = rdb_get_i_boolean(Diagnostics_UDPEcho_StartTest,  session_id, &retValue);

    if(err)
    {
        return err;
    }
    return retValue;
}


///$ ==0 update
///$<0 -- error code
int load_session_rdb(TConnectSession*pSession)
{
	int retValue;
	char buffer[120];
	unsigned int session_id = pSession->m_parameters->m_session_id;

	retValue = rdb_get_i_uint(Diagnostics_UDPEcho_CoS, session_id, &pSession->m_if_cos);
	if(retValue <0) return DiagnosticsState_Error_InitConnectionFailed;

	retValue = rdb_get_i_str(Diagnostics_UDPEcho_ServerAddress,  session_id,  buffer, 120);
	if(retValue <0) return retValue;
	if(!inet_aton(buffer, (struct in_addr*)&pSession->m_ServerAddress))
	{
		return DiagnosticsState_Error_InitConnectionFailed;
	}
	retValue = rdb_get_i_str(Diagnostics_UDPEcho_InterfaceAddress,  session_id,  buffer, 120);
	if(retValue <0) return retValue;
	if(!inet_aton(buffer, (struct in_addr*)&pSession->m_local_addr))
	{
		return DiagnosticsState_Error_InitConnectionFailed;
	}
	pSession->m_local_port = LOCAL_BASE_PORT;

///Diagnostics.UDPEcho.ServerPort 	uint 	readwrite 	none 	0 		UDP port address of the UDPEchoPlus server to send pings to.
///#define Diagnostics_UDPEcho_ServerPort  "Diagnostics.UDPEcho.ServerPort"
	retValue = rdb_get_i_uint(Diagnostics_UDPEcho_ServerPort,  session_id,  &pSession->m_ServerPort);
	if(retValue <0) return retValue;
	if(pSession->m_ServerPort ==0) return DiagnosticsState_Error_IncorrectSize;

///Diagnostics.UDPEcho.PacketCount 	unit 	readwrite 	none 	0 		Number of ping packets to send. Limited by memory considerations, assignment may return 9004 - resources exceeded.
///#define Diagnostics_UDPEcho_PacketCount  "Diagnostics.UDPEcho.PacketCount"
	retValue = rdb_get_i_uint(Diagnostics_UDPEcho_PacketCount,  session_id,  &pSession->m_PacketCount);
	if(retValue <0) return retValue;
	if(pSession->m_PacketCount ==0)  return DiagnosticsState_Error_IncorrectSize;


///Diagnostics.UDPEcho.PacketSize 	unit 	readwrite 	none 	20 		Size of packets to send in bytes. This is the payload size, not including the IP and UDP header, the minimum is 20 (to fit UDPEchoPlus fields), the maximum is 65,503 (limited by IPv4).
///#define Diagnostics_UDPEcho_PacketSize "Diagnostics.UDPEcho.PacketSize"
	retValue = rdb_get_i_uint(Diagnostics_UDPEcho_PacketSize,  session_id,  &pSession->m_PacketSize);
	if(retValue <0) return retValue;
	if(pSession->m_PacketSize <20 || pSession->m_PacketSize>65503) return DiagnosticsState_Error_IncorrectSize;

///Diagnostics.UDPEcho.PacketInterval 	unit 	readwrite 	none 	1000 		Interval between packets sent in milliseconds. Zero means to send as fast as possible.
///#define Diagnostics_UDPEcho_PacketInterval "Diagnostics.UDPEcho.PacketInterval"
	retValue = rdb_get_i_uint(Diagnostics_UDPEcho_PacketInterval,  session_id,  &pSession->m_PacketInterval);
	if(retValue <0) return DiagnosticsState_Error_IncorrectSize;
	pSession->m_PacketInterval *=1000; // change to us

///Diagnostics.UDPEcho.StragglerTimeout 	unit 	readwrite 	none 	5000 		Time in milliseconds to wait after last packet is sent for replies to arrive.
///#define Diagnostics_UDPEcho_StragglerTimeout "Diagnostics.UDPEcho.StragglerTimeout"
	retValue = rdb_get_i_uint(Diagnostics_UDPEcho_StragglerTimeout,  session_id,  &pSession->m_StragglerTimeout);
	if(retValue <0) return DiagnosticsState_Error_IncorrectSize;

	if(pSession->m_StragglerTimeout ==0) pSession->m_StragglerTimeout=5000;


	pSession->m_SendPathDelayDeltaJitterMin=INT64_MAX;
	pSession->m_SendPathDelayDeltaJitterMax=INT64_MIN;
	pSession->m_ReceivePathDelayDeltaJitterAverage=0;
	pSession->m_ReceivePathDelayDeltaJitterMin=INT64_MAX;
	pSession->m_ReceivePathDelayDeltaJitterMax=INT64_MIN;
	pSession->m_RTTAverage =0;
	pSession->m_RTTMin = INT64_MAX;
	pSession->m_RTTMax = INT64_MIN;

	return 0;
}

inline int rdb_set_i_ms(const char* name, int session_id, int64_t ms)
{
	if(ms == INT64_MAX || ms== INT64_MIN)
	{
		rdb_set_i_uint(name, session_id, 0);
		return 0;
	}
	//if(timeval_cmp(ms, &MIN_TIMEVAL) == 0) return 0;

	return rdb_set_i_float("%.3f", name, session_id, ((double)ms)/1000);
}


///$ ==0 update
///$<0 -- error code
int update_all_session(TConnectSession *pSession, int update_file)
{
	int retValue;
	unsigned int session_id = pSession->m_parameters->m_session_id;

#ifdef _DUMP_RAW
	if(pSession->m_statData && update_file)
	{
		int i;
		FILE *fp;
		sprintf(pSession->m_parameters->m_stats_name, "/tmp/udpecho.%d.statsname.txt", pSession->m_parameters->m_session_id);
		fp = fopen(pSession->m_parameters->m_stats_name,"wt");
		if(fp)
		{
			fprintf(fp,"SN,rtt,sendpathdelaydelta,receivepathdelaydelta\n" );
			// jitter calculation
			// J=J+(|D(i-1,i)|-J)/16
			for(i=pSession->m_TestGenSN_first; i<= pSession->m_TestGenSN_last; i++)
			{
				if(pSession->m_statData[i].m_success)
				{
					fprintf(fp, "%d,%0.3f,%.3f,%.3f\n",i,
								((double)pSession->m_statData[i].m_rtd)/1000,
								((double)pSession->m_statData[i].m_send_path_IPDV)/1000,
								((double)pSession->m_statData[i].m_recv_path_IPDV)/1000 );
				}
				else
				{
					fprintf(fp,"%d,,,,\n",i );
				}
			}
			fclose(fp);
			retValue = rdb_set_i_str(Diagnostics_UDPEcho_StatsName,  session_id,  pSession->m_parameters->m_stats_name);
			if(retValue <0) return retValue;

		}
		else
		{
			retValue = rdb_set_i_str(Diagnostics_UDPEcho_StatsName,  session_id,  "");
		}
	}
	else
	{
		retValue = rdb_set_i_str(Diagnostics_UDPEcho_StatsName,  session_id,  "");
	}

#endif

// calculate jitter, min/avg/max
	if(pSession->m_statData )
	{
		int i;

		long jitter1=0;
		long jitter2=0;
		long tmp=0;
		int count =0;
		// jitter calculation
		// J=J+(|D(i-1,i)|-J)/16
		for(i= pSession->m_TestGenSN_first; i<= pSession->m_TestGenSN_last; i++)
		{
			if(pSession->m_statData[i].m_success)
			{
				tmp = pSession->m_statData[i].m_stats.m_send_path_IPDV;
				//printf("%d: s_IPDV=%d, r_IPDV=%d\n", i, pSession->m_statData[i].m_stats.m_send_path_IPDV,
				//		pSession->m_statData[i].m_stats.m_recv_path_IPDV);
				jitter1 += (tmp>=0?tmp:-tmp - jitter1)/16;
				if(jitter1 < pSession->m_SendPathDelayDeltaJitterMin )
				{
					if(jitter1 > 0)
					{
						pSession->m_SendPathDelayDeltaJitterMin = jitter1;
					}
				}
				if(jitter1 > pSession->m_SendPathDelayDeltaJitterMax )
				{
					pSession->m_SendPathDelayDeltaJitterMax = jitter1;
				}
				pSession->m_SendPathDelayDeltaJitterAverage += jitter1;

				tmp = pSession->m_statData[i].m_stats.m_recv_path_IPDV;
				jitter2 += (tmp>=0?tmp:-tmp - jitter2)/16;
				if(jitter2 < pSession->m_ReceivePathDelayDeltaJitterMin )
				{
					if(jitter2 > 0)
					{
						pSession->m_ReceivePathDelayDeltaJitterMin = jitter2;
					}
				}
				if(jitter2 > pSession->m_ReceivePathDelayDeltaJitterMax )
				{
					pSession->m_ReceivePathDelayDeltaJitterMax = jitter2;
				}
				pSession->m_ReceivePathDelayDeltaJitterAverage += jitter2;

				if(pSession->m_statData[i].m_stats.m_rtd < pSession->m_RTTMin)
				{
					pSession->m_RTTMin = pSession->m_statData[i].m_stats.m_rtd;
				}

				if( pSession->m_statData[i].m_stats.m_rtd > pSession->m_RTTMax)
				{
					pSession->m_RTTMax = pSession->m_statData[i].m_stats.m_rtd;
				}

				pSession->m_RTTAverage +=  pSession->m_statData[i].m_stats.m_rtd;

				count++;
			}
		}
		if( count > 0)
		{

			pSession->m_SendPathDelayDeltaJitterAverage /=count;
			pSession->m_ReceivePathDelayDeltaJitterAverage /=count;
			pSession->m_RTTAverage 	/=count;
		}
	}//if(pSession->m_statData )


///Diagnostics.UDPEcho.PacketsSent 	uint 	readonly 	none 	0 		Number of packets sent.
///#define Diagnostics_UDPEcho_PacketsSent  "Diagnostics.UDPEcho.PacketsSent"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_PacketsSent,  session_id,  pSession->m_PacketsSent);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.PacketsSendloss 	uint 	readonly 	none 	0 		Number of packets sent.
///#define Diagnostics_UDPEcho_PacketsSendLoss  "Diagnostics.UDPEcho.PacketsSendLoss"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_PacketsSendLoss,  session_id,  pSession->m_PacketsSendLoss);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.PacketsReceived 	uint 	readonly 	none 	0 		Number of packets received.
///#define Diagnostics_UDPEcho_PacketsReceived  "Diagnostics.UDPEcho.PacketsReceived"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_PacketsReceived,  session_id,  pSession->m_PacketsReceived);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.PacketsReceiveLoss 	uint 	readonly 	none 	0 		packets loss in receiving.
///#define Diagnostics_UDPEcho_PacketsReceiveLoss  "Diagnostics.UDPEcho.PacketsReceiveLoss"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_PacketsReceiveLoss,  session_id,  pSession->m_PacketsReceiveLoss);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.BytesSent 	uint 	readonly 	none 	0 		Bytes sent.
///#define Diagnostics_UDPEcho_BytesSent  "Diagnostics.UDPEcho.BytesSent"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_BytesSent,  session_id,  pSession->m_BytesSent);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.BytesReceived 	uint 	readonly 	none 	0 		Bytes received.
///#define Diagnostics_UDPEcho_BytesReceived  "Diagnostics.UDPEcho.BytesReceived"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_BytesReceived,  session_id,  pSession->m_BytesReceived);
	if(retValue <0) return retValue;


///#define Diagnostics_UDPEcho_RXPkts	"udpecho.%i.rxpkts"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_RXPkts,  session_id,  pSession->m_if_rx_pkts);
	if(retValue <0) return retValue;
///#define Diagnostics_UDPEcho_TXPkts	"udpecho.%i.txpkts"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_TXPkts,  session_id,  pSession->m_if_tx_pkts);
	if(retValue <0) return retValue;

///#define Diagnostics_UDPEcho_RXPktsSmartedge	"udpecho.%i.smartedgerxpkts"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_RXPktsSmartedge,  session_id,  pSession->m_sedge_if_rx_pkts);
	if(retValue <0) return retValue;
///#define Diagnostics_UDPEcho_TXPktsSmartedge	"udpecho.%i.smartedgetxpkts"
	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_TXPktsSmartedge,  session_id,  pSession->m_sedge_if_tx_pkts);
	if(retValue <0) return retValue;


	retValue = rdb_set_i_uint(Diagnostics_UDPEcho_RXPkts,  session_id,  pSession->m_if_rx_pkts);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.PacketsLossPercentage 	string 	readonly 	none 	0 		Packets loss percentage (decimal as string).
///#define Diagnostics_UDPEcho_PacketsLossPercentage "Diagnostics.UDPEcho.PacketsLossPercentage"
	retValue = rdb_set_i_float("%.2f", Diagnostics_UDPEcho_PacketsLossPercentage,  session_id,  ((double)pSession->m_PacketsLossPercentage)/100);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.RTTAverage 	string 	readonly 	none 	0 		Average round-trip time in milliseconds (decimal as string).
//#define Diagnostics_UDPEcho_RTTAverage "Diagnostics.UDPEcho.RTTAverage"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_RTTAverage,  session_id,  pSession->m_RTTAverage);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.RTTMax 	string 	readonly 	none 	0 		Maximum round-trip time in milliseconds (decimal as string).
///#define Diagnostics_UDPEcho_RTTMax "Diagnostics.UDPEcho.RTTMax"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_RTTMax,  session_id,  pSession->m_RTTMax);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.RTTMin 	string 	readonly 	none 	0 		Minimum round-trip time in milliseconds (decimal as string).
///#define Diagnostics_UDPEcho_RTTMin "Diagnostics.UDPEcho.RTTMin"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_RTTMin,  session_id,  pSession->m_RTTMin);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.SendPathDelayDeltaJitterAverage 	string 	readonly 	none 	0 		Average packet delay delta in the up-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_SendPathDelayDeltaJitterAverage "Diagnostics.UDPEcho.SendPathDelayDeltaJitterAverage"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_SendPathDelayDeltaJitterAverage,  session_id,  pSession->m_SendPathDelayDeltaJitterAverage);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.SendPathDelayDeltaJitterMin 	string 	readonly 	none 	0 		Minimum packet delay delta in the up-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_SendPathDelayDeltaJitterMin "Diagnostics.UDPEcho.SendPathDelayDeltaJitterMin"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_SendPathDelayDeltaJitterMin,  session_id,  pSession->m_SendPathDelayDeltaJitterMin);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.SendPathDelayDeltaJitterMax 	string 	readonly 	none 	0 		Maximum packet delay delta in the up-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_SendPathDelayDeltaJitterMax "Diagnostics.UDPEcho.SendPathDelayDeltaJitterMax"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_SendPathDelayDeltaJitterMax,  session_id,  pSession->m_SendPathDelayDeltaJitterMax);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterAverage 	string 	readonly 	none 	0 		Average packet delay delta in the down-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterAverage "Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterAverage"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterAverage,  session_id,  pSession->m_ReceivePathDelayDeltaJitterAverage);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterMin 	string 	readonly 	none 	0 		Minimum packet delay delta in the down-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMin "Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterMin"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMin,  session_id,  pSession->m_ReceivePathDelayDeltaJitterMin);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterMax 	string 	readonly 	none 	0 		Maximum packet delay delta in the down-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMax "Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterMax"
	retValue = rdb_set_i_ms(Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMax,  session_id,  pSession->m_ReceivePathDelayDeltaJitterMax);
	if(retValue <0) return retValue;



	return 0;
}


