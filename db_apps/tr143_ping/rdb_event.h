
/* ----------------------------------------------------------------------------
RDB/Indoor event handling program

Lee Huang<leeh@netcomm.com.au>

*/

#ifndef __RDB_EVENT_H
#define __RDB_EVENT_H
#include "rdb_comms.h"
#include "ping.h"



// requrest
static inline int  open_rdb(int session_id, int subs_group)
{
    int retValue = rdb_open(RDB_DEVNAME, &g_rdb_session);
    if(retValue <0) return retValue;

    retValue = subscribe(session_id, subs_group);
    if(retValue <0) return retValue;

    return retValue;
}




static inline void close_rdb()
{
    rdb_close(&g_rdb_session);
    g_rdb_session=NULL;
}
///    * "None" - service is idle waiting for a request.
///   * "Requested" - set by ACS after configuring test to request CPE to begin the test.
///    * "Completed" - set by CPE to indicate successful test completion.
///    * "Error_<description>" - set by CPE to indicate failure of test.


#define DiagnosticsState_Code_None 					0	//"None"
#define DiagnosticsState_Code_Request 				1//"Requested"
#define DiagnosticsState_Code_Completed 			2 //"Completed"
#define DiagnosticsState_Code_Cancelled				3 //"Cancelled"

#define DiagnosticsState_Error_InitConnectionFailed -1 //"Error_InitConnectionFailed"
#define DiagnosticsState_Error_NoResponse 			-2	//"Error_NoResponse
#define DiagnosticsState_Error_TransferFailed 		-3	//"Error_TransferFailed"
#define DiagnosticsState_Error_PasswordRequestFailed -4	//"Error_PasswordRequestFailed"
#define DiagnosticsState_Error_LoginFailed 			-5//"Error_LoginFailed"
#define DiagnosticsState_Error_NoTransferMode 		-6//"Error_NoTransferMode"
#define DiagnosticsState_Error_NoPASV 				-7//"Error_NoPASV"
#define DiagnosticsState_Error_IncorrectSize 		-8//"Error_IncorrectSize"
#define DiagnosticsState_Error_Timeout 				-9//"Error_Timeout"
#define DiagnosticsState_Error_Invalid_URL			-10//"Error_Invalid_URL"
#define DiagnosticsState_Error_Cancelled			-11//"Error_Canceled"

/// get state code  from udpecho.diagnosticsstate
int get_ping_state(int session_id);

/// set udpecho.diagnosticsstate by state code
void set_ping_state(int session_id, int state);

/// get udpecho.starttest
/// $>=0 -- success
/// $<0 --- error
int get_ping_starttest(int session_id);



//$ ==0 update
//$<0 -- error code
int update_all_session(TConnectSession *TConnectSession, int update_file);

//$ ==0 update
//$<0 -- error code
int load_session_rdb(TConnectSession *pParameter);

// save server address into RDB variable
static inline void packetcount2rdb(int session_id,  int  packetCount)
{
	if(packetCount >=0)
		rdb_set_i_uint(Diagnostics_UDPEcho_PacketCount, session_id, packetCount);
}
// save server address into RDB variable
static inline void packetsize2rdb( int session_id, int  packetSize)
{
	if(packetSize >=0)
		rdb_set_i_uint(Diagnostics_UDPEcho_PacketSize,  session_id, packetSize);
}

static inline void host2rdb(int session_id, const char*hostname)
{
	if(hostname)
		rdb_set_i_str(Diagnostics_UDPEcho_ServerAddress,  session_id, hostname);
}

static inline void port2rdb(int session_id, int port)
{
	if(port >=0)
		rdb_set_i_uint(Diagnostics_UDPEcho_ServerPort,session_id, port);
}
static inline void interval_timeout2rdb(int session_id, int timeout)
{
	if(timeout >= 0)
		rdb_set_i_uint(Diagnostics_UDPEcho_PacketInterval, session_id, timeout);
}

static inline void straggler_timeout2rdb(int session_id, int timeout)
{
	if(timeout >= 0)
		rdb_set_i_uint(Diagnostics_UDPEcho_StragglerTimeout, session_id, timeout);
}

static inline void if_ip2rdb(int session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_UDPEcho_InterfaceAddress, session_id, ip);
}


static inline void if_mask2rdb(int session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_UDPEcho_InterfaceNetmask, session_id, ip);
}

static inline void if_routr2rdb(int session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_UDPEcho_SmartEdgeAddress, session_id, ip);
}




static inline void if_MPLSTag2rdb(int session_id, int tag)
{
	if(tag >=0)
		rdb_set_i_uint(Diagnostics_UDPEcho_MPLSTag, session_id, tag);
}

static inline void if_Cos2rdb(int session_id, int cos)
{
	if(cos >=0)
		rdb_set_i_uint(Diagnostics_UDPEcho_CoS, session_id, cos);
}

#endif
