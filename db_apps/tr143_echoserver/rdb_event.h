
/* ----------------------------------------------------------------------------
RDB/Indoor event handling program

Lee Huang<leeh@netcomm.com.au>

*/

#ifndef __RDB_EVENT_H
#define __RDB_EVENT_H
#include "rdb_comms.h"
#include "comms.h"

#define TR143_IPTABLE_TRIGGER    "tr143.echoserver.trigger"

// requrest
static inline int  open_rdb()
{
    int retValue = rdb_open_db();
    if(retValue <0) return retValue;

    retValue = subscribe();
    if(retValue <0) return retValue;

    return retValue;
}




static inline void close_rdb()
{
    rdb_close_db();
}

int is_UDPEchoConfigEnabled();
int is_EchoPlusEnabled();

//$ ==0 update
//$<0 -- error code
int update_all_session(TConnectSession *TConnectSession);

//$ ==0 update
//$<0 -- error code
int load_session_rdb(TConnectSession *pParameter);

static inline void enable2rdb( int value)
{
	if(value == TRUE || value == FALSE)
		rdb_set_boolean(UDPEchoConfig_Enable, value);
}

static inline void interface2rdb( const char* iface)
{
	if(iface)
		rdb_set_str(UDPEchoConfig_Interface, iface);
}

static inline void sourceIPAddress2rdb( const char* ipAddr)
{
	if(ipAddr)
		rdb_set_str(UDPEchoConfig_SourceIPAddress, ipAddr);
}

static inline void udpPort2rdb(unsigned int port)
{
		rdb_set_int(UDPEchoConfig_UDPPort, port);
}

static inline void echoPlusEnabled2rdb(int value)
{
	if(value == TRUE || value == FALSE)
		rdb_set_boolean(UDPEchoConfig_EchoPlusEnabled, value);
}

static inline void packetsReceived2rdb(unsigned int num)
{
		rdb_set_int(UDPEchoConfig_PacketsReceived, num);
}

static inline void packetsResponded2rdb(unsigned int num)
{
		rdb_set_int(UDPEchoConfig_PacketsResponded, num);
}

static inline void bytesReceived2rdb(unsigned int num)
{
		rdb_set_int(UDPEchoConfig_BytesReceived, num);
}

static inline void bytesResponded2rdb(unsigned int num)
{
		rdb_set_int(UDPEchoConfig_BytesResponded, num);
}

static inline void timeFirstPacketReceivedr2rdb(const char*str_time)
{
	if(str_time)
		rdb_set_str(UDPEchoConfig_TimeFirstPacketReceived, str_time);
}

static inline void timeLastPacketReceived2rdb(const char*str_time)
{
	if(str_time)
		rdb_set_str(UDPEchoConfig_TimeLastPacketReceived, str_time);
}

#endif
