#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <math.h>

#include "log.h"
#include "rdb_event.h"
#include "utils.h"


//return value
// 1 -> TRUE
// 0 -> FALSE
// -1 -> ERROR
int is_UDPEchoConfigEnabled()
{
	int isEnabled =0;

    if(rdb_get_boolean(UDPEchoConfig_Enable, &isEnabled))
        return -1;

	return isEnabled;
}

int is_EchoPlusEnabled()
{
	int isEnabled =0;

    if(rdb_get_boolean(UDPEchoConfig_EchoPlusEnabled, &isEnabled))
        return -1;

	return isEnabled;
}

///$ ==0 update
///$<0 -- error code
int load_session_rdb(TConnectSession*pSession)
{
#if 0 // TODO
	int retValue;
	char buffer[120];

	retValue = rdb_get_single(Diagnostics_UDPEcho_ServerAddress, buffer, 120);
	if(retValue <0) return retValue;
	if(!inet_aton(buffer, (struct in_addr*)&pSession->m_ServerAddress))
			return DiagnosticsState_Error_InitConnectionFailed;

///Diagnostics.UDPEcho.ServerPort 	uint 	readwrite 	none 	0 		UDP port address of the UDPEchoPlus server to send pings to.
///#define Diagnostics_UDPEcho_ServerPort  "Diagnostics.UDPEcho.ServerPort"
	retValue = rdb_get_int(Diagnostics_UDPEcho_ServerPort, &pSession->m_ServerPort);
	if(retValue <0) return retValue;
	if(pSession->m_ServerPort ==0) return DiagnosticsState_Error_InitConnectionFailed;

///Diagnostics.UDPEcho.PacketCount 	unit 	readwrite 	none 	0 		Number of ping packets to send. Limited by memory considerations, assignment may return 9004 - resources exceeded.
///#define Diagnostics_UDPEcho_PacketCount  "Diagnostics.UDPEcho.PacketCount"
	retValue = rdb_get_int(Diagnostics_UDPEcho_PacketCount, &pSession->m_PacketCount);
	if(retValue <0) return retValue;
	if(pSession->m_PacketCount ==0)  return DiagnosticsState_Error_InitConnectionFailed;


///Diagnostics.UDPEcho.PacketSize 	unit 	readwrite 	none 	20 		Size of packets to send in bytes. This is the payload size, not including the IP and UDP header, the minimum is 20 (to fit UDPEchoPlus fields), the maximum is 65,503 (limited by IPv4).
///#define Diagnostics_UDPEcho_PacketSize "Diagnostics.UDPEcho.PacketSize"
	retValue = rdb_get_int(Diagnostics_UDPEcho_PacketSize, &pSession->m_PacketSize);
	if(retValue <0) return retValue;
	if(pSession->m_PacketSize <20 || pSession->m_PacketSize>65503) return DiagnosticsState_Error_IncorrectSize;

///Diagnostics.UDPEcho.PacketInterval 	unit 	readwrite 	none 	1000 		Interval between packets sent in milliseconds. Zero means to send as fast as possible.
///#define Diagnostics_UDPEcho_PacketInterval "Diagnostics.UDPEcho.PacketInterval"
	retValue = rdb_get_int(Diagnostics_UDPEcho_PacketInterval, &pSession->m_PacketInterval);
	if(retValue <0) return DiagnosticsState_Error_InitConnectionFailed;

///Diagnostics.UDPEcho.StragglerTimeout 	unit 	readwrite 	none 	5000 		Time in milliseconds to wait after last packet is sent for replies to arrive.
///#define Diagnostics_UDPEcho_StragglerTimeout "Diagnostics.UDPEcho.StragglerTimeout"
	retValue = rdb_get_int(Diagnostics_UDPEcho_StragglerTimeout, &pSession->m_StragglerTimeout);
	if(retValue <0) return DiagnosticsState_Error_InitConnectionFailed;

	pSession->m_RTTMin = INT64_MAX;
	pSession->m_SendPathPacketDelayDeltaMin =INT64_MAX;
	pSession->m_ReceivePathPacketDelayDeltaMin =INT64_MAX;

	pSession->m_RTTMax = INT64_MIN;
	pSession->m_SendPathPacketDelayDeltaMax =INT64_MIN;
	pSession->m_ReceivePathPacketDelayDeltaMax =INT64_MIN;
#endif
	return 0;
}

inline int rdb_set_ms(const char* name, int64_t t)
{
	if(t == INT64_MAX || t== INT64_MIN)
	{
		rdb_set_sint(name, 0);
		return 0;
	}

	return rdb_set_sint(name, t/1000);
}

///$ ==0 update
///$<0 -- error code
int update_all_session(TConnectSession *pSession)
{
#if 0 // TODO
	int retValue;

///Diagnostics.UDPEcho.PacketsSent 	uint 	readonly 	none 	0 		Number of packets sent.
///#define Diagnostics_UDPEcho_PacketsSent  "Diagnostics.UDPEcho.PacketsSent"
	retValue = rdb_set_int(Diagnostics_UDPEcho_PacketsSent, pSession->m_PacketsSent);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.PacketsReceived 	uint 	readonly 	none 	0 		Number of packets received.
///#define Diagnostics_UDPEcho_PacketsReceived  "Diagnostics.UDPEcho.PacketsReceived"
	retValue = rdb_set_int(Diagnostics_UDPEcho_PacketsReceived, pSession->m_PacketsReceived);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.BytesSent 	uint 	readonly 	none 	0 		Bytes sent.
///#define Diagnostics_UDPEcho_BytesSent  "Diagnostics.UDPEcho.BytesSent"
	retValue = rdb_set_int(Diagnostics_UDPEcho_BytesSent, pSession->m_BytesSent);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.BytesReceived 	uint 	readonly 	none 	0 		Bytes received.
///#define Diagnostics_UDPEcho_BytesReceived  "Diagnostics.UDPEcho.BytesReceived"
	retValue = rdb_set_int(Diagnostics_UDPEcho_BytesReceived, pSession->m_BytesReceived);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.PacketsLossPercentage 	string 	readonly 	none 	0 		Packets loss percentage (decimal as string).
///#define Diagnostics_UDPEcho_PacketsLossPercentage "Diagnostics.UDPEcho.PacketsLossPercentage"
	retValue = rdb_set_int(Diagnostics_UDPEcho_PacketsLossPercentage, pSession->m_PacketsLossPercentage);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.RTTAverage 	string 	readonly 	none 	0 		Average round-trip time in milliseconds (decimal as string).
//#define Diagnostics_UDPEcho_RTTAverage "Diagnostics.UDPEcho.RTTAverage"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_RTTAverage, pSession->m_RTTAverage);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.RTTMax 	string 	readonly 	none 	0 		Maximum round-trip time in milliseconds (decimal as string).
///#define Diagnostics_UDPEcho_RTTMax "Diagnostics.UDPEcho.RTTMax"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_RTTMax, pSession->m_RTTMax);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.RTTMin 	string 	readonly 	none 	0 		Minimum round-trip time in milliseconds (decimal as string).
///#define Diagnostics_UDPEcho_RTTMin "Diagnostics.UDPEcho.RTTMin"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_RTTMin, pSession->m_RTTMin);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.SendPathPacketDelayDeltaAverage 	string 	readonly 	none 	0 		Average packet delay delta in the up-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_SendPathPacketDelayDeltaAverage "Diagnostics.UDPEcho.SendPathPacketDelayDeltaAverage"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_SendPathPacketDelayDeltaAverage, pSession->m_SendPathPacketDelayDeltaAverage);
	if(retValue <0) return retValue;


///Diagnostics.UDPEcho.SendPathPacketDelayDeltaMin 	string 	readonly 	none 	0 		Minimum packet delay delta in the up-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_SendPathPacketDelayDeltaMin "Diagnostics.UDPEcho.SendPathPacketDelayDeltaMin"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_SendPathPacketDelayDeltaMin, pSession->m_SendPathPacketDelayDeltaMin);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.SendPathPacketDelayDeltaMax 	string 	readonly 	none 	0 		Maximum packet delay delta in the up-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_SendPathPacketDelayDeltaMax "Diagnostics.UDPEcho.SendPathPacketDelayDeltaMax"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_SendPathPacketDelayDeltaMax, pSession->m_SendPathPacketDelayDeltaMax);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.ReceivePathPacketDelayDeltaAverage 	string 	readonly 	none 	0 		Average packet delay delta in the down-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_ReceivePathPacketDelayDeltaAverage "Diagnostics.UDPEcho.ReceivePathPacketDelayDeltaAverage"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_ReceivePathPacketDelayDeltaAverage, pSession->m_ReceivePathPacketDelayDeltaAverage);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.ReceivePathPacketDelayDeltaMin 	string 	readonly 	none 	0 		Minimum packet delay delta in the down-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_ReceivePathPacketDelayDeltaMin "Diagnostics.UDPEcho.ReceivePathPacketDelayDeltaMin"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_ReceivePathPacketDelayDeltaMin, pSession->m_ReceivePathPacketDelayDeltaMin);
	if(retValue <0) return retValue;

///Diagnostics.UDPEcho.ReceivePathPacketDelayDeltaMax 	string 	readonly 	none 	0 		Maximum packet delay delta in the down-stream direction (decimal milliseconds as string).
///#define Diagnostics_UDPEcho_ReceivePathPacketDelayDeltaMax "Diagnostics.UDPEcho.ReceivePathPacketDelayDeltaMax"
	retValue = rdb_set_ms(Diagnostics_UDPEcho_ReceivePathPacketDelayDeltaMax, pSession->m_ReceivePathPacketDelayDeltaMax);
	if(retValue <0) return retValue;
#endif

	return 0;
}


