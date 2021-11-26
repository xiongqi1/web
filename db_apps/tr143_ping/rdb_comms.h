/* ----------------------------------------------------------------------------
RDB interface program

Lee Huang<leeh@netcomm.com.au>

*/

#ifndef __RDB_COMMS_H
#define __RDB_COMMS_H


#include "rdb_ops.h"




///Diagnostics.UDPEcho. 	object 	readonly 	none 	present 		UDPEchoPlus diagnostic object.
#define Diagnostics_UDPEcho "udpecho."

///Diagnostics.UDPEcho.DiagnosticsState 	string 	readwrite 	none 	None
///Upload diagnostic subsystem state.
///    * "None" - service is idle waiting for a request.
///   * "Requested" - set by ACS after configuring test to request CPE to begin the test.
///    * "Completed" - set by CPE to indicate successful test completion.
///    * "Error_<description>" - set by CPE to indicate failure of test.
#define Diagnostics_UDPEcho_DiagnosticsState "udpecho.%i.diagnosticsstate"

/// force to reload all parameters
#define Diagnostics_UDPEcho_Changed "udpecho.%i.changed"

///Diagnostics.UDPEcho.StartTest	int 	readwrite 	none 	0	Start Console udpecho test.
#define Diagnostics_UDPEcho_StartTest	"udpecho.%i.starttest"

///Diagnostics.UDPEcho.SmartEdgeAddress 	string 	readwrite 	none 	0.0.0.0 		IP address of the CPG to establish test AVC to.
#define Diagnostics_UDPEcho_SmartEdgeAddress  "udpecho.%i.smartedgeaddress"

///Diagnostics.UDPEcho.MPLSTag 	uint 	readwrite 	none 	0 		MPLS label for the test AVC.
#define Diagnostics_UDPEcho_MPLSTag  "udpecho.%i.mplstag"

///Diagnostics.UDPEcho.CoS 	uint 	readwrite 	none 	0 		CoS for the test traffic.
#define Diagnostics_UDPEcho_CoS  "udpecho.%i.cos"

///Diagnostics.UDPEcho.CoStoEXP 	string 	readwrite 	none 	0,1,2,3,4,5,6,7 		CoS to MPLS experimental bits mapping for the test AVC (only the utilised CoS value mapping need be non-zero).
#define Diagnostics_UDPEcho_CoStoEXP  "udpecho.%i.costoexp"

///Diagnostics.UDPEcho.CoStoDSCP 	string 	readwrite 	none 	0,0,0,16,32,40,0,0 		CoS to IP-GRE DSCP mapping for the test AVC (only the utilised CoS value mapping need be non-zero).
#define Diagnostics_UDPEcho_CoStoDSCP  "udpecho.%i.costodscp"

///Diagnostics.UDPEcho.InterfaceAddress 	string 	readwrite 	none 	0.0.0.0 		IPv4 address to assign to test interface.
#define Diagnostics_UDPEcho_InterfaceAddress  "udpecho.%i.interfaceaddress"

///Diagnostics.UDPEcho.InterfaceNetmask 	string 	readwrite 	none 	0.0.0.0 		IPv4 address mask to assign to test interface.
#define Diagnostics_UDPEcho_InterfaceNetmask  "udpecho.%i.interfacenetmask"

///Diagnostics.UDPEcho.ServerAddress 	string 	readwrite 	none 	0.0.0.0 		IPv4 address of the UDPEchoPlus server to send pings to. Server must be reachable via the test interface.
#define Diagnostics_UDPEcho_ServerAddress  "udpecho.%i.serveraddress"

///Diagnostics.UDPEcho.ServerPort 	uint 	readwrite 	none 	0 		UDP port address of the UDPEchoPlus server to send pings to.
#define Diagnostics_UDPEcho_ServerPort  "udpecho.%i.serverport"


///Diagnostics.UDPEcho.PacketCount 	unit 	readwrite 	none 	0 		Number of ping packets to send. Limited by memory considerations, assignment may return 9004 - resources exceeded.
#define Diagnostics_UDPEcho_PacketCount  "udpecho.%i.packetcount"


///Diagnostics.UDPEcho.PacketSize 	unit 	readwrite 	none 	20 		Size of packets to send in bytes. This is the payload size, not including the IP and UDP header, the minimum is 20 (to fit UDPEchoPlus fields), the maximum is 65,503 (limited by IPv4).
#define Diagnostics_UDPEcho_PacketSize "udpecho.%i.packetsize"

///Diagnostics.UDPEcho.PacketInterval 	unit 	readwrite 	none 	1000 		Interval between packets sent in milliseconds. Zero means to send as fast as possible.
#define Diagnostics_UDPEcho_PacketInterval "udpecho.%i.packetinterval"

///Diagnostics.UDPEcho.StragglerTimeout 	unit 	readwrite 	none 	5000 		Time in milliseconds to wait after last packet is sent for replies to arrive.
#define Diagnostics_UDPEcho_StragglerTimeout "udpecho.%i.stragglertimeout"

///Diagnostics.UDPEcho.BytesSent 	uint 	readonly 	none 	0 		Bytes sent.
#define Diagnostics_UDPEcho_BytesSent "udpecho.%i.bytessent"

///Diagnostics.UDPEcho.BytesReceived 	uint 	readonly 	none 	0 		Bytes received.
#define Diagnostics_UDPEcho_BytesReceived "udpecho.%i.bytesreceived"

///Diagnostics.UDPEcho.PacketsSent 	uint 	readonly 	none 	0 		Packets sent to server.
#define Diagnostics_UDPEcho_PacketsSent "udpecho.%i.packetssent"

///Diagnostics.UDPEcho.PacketsSendLoss 	uint 	readonly 	none 	0 		loss Packets in  sending
#define Diagnostics_UDPEcho_PacketsSendLoss "udpecho.%i.packetssendloss"

///Diagnostics.UDPEcho.PacketsReceived 	uint 	readonly 	none 	0 		Packets received from server.
#define Diagnostics_UDPEcho_PacketsReceived "udpecho.%i.packetsreceived"

///Diagnostics.UDPEcho.PacketsReceiveLoss 	uint 	readonly 	none 	0 		Packets loss in receiving
#define Diagnostics_UDPEcho_PacketsReceiveLoss "udpecho.%i.packetsreceiveloss"

///Diagnostics.UDPEcho.PacketsLossPercentage 	string 	readonly 	none 	0 		Packets loss percentage (decimal as string).
#define Diagnostics_UDPEcho_PacketsLossPercentage "udpecho.%i.packetslosspercentage"

///Diagnostics.UDPEcho.RTTAverage 	string 	readonly 	none 	0 		Average round-trip time in milliseconds (decimal as string).
#define Diagnostics_UDPEcho_RTTAverage "udpecho.%i.rttaverage"

///Diagnostics.UDPEcho.RTTMax 	string 	readonly 	none 	0 		Maximum round-trip time in milliseconds (decimal as string).
#define Diagnostics_UDPEcho_RTTMax "udpecho.%i.rttmax"

///Diagnostics.UDPEcho.RTTMin 	string 	readonly 	none 	0 		Minimum round-trip time in milliseconds (decimal as string).
#define Diagnostics_UDPEcho_RTTMin "udpecho.%i.rttmin"



///Diagnostics.UDPEcho.SendPathDelayDeltaJitterAverage 	string 	readonly 	none 	0 		Average packet delay deltajitter in the up-stream direction (decimal milliseconds as string).
#define Diagnostics_UDPEcho_SendPathDelayDeltaJitterAverage "udpecho.%i.sendpathdelaydeltajitteraverage"

///Diagnostics.UDPEcho.SendPathDelayDeltaJitterMin 	string 	readonly 	none 	0 		Minimum packet delay deltajitter in the up-stream direction (decimal milliseconds as string).
#define Diagnostics_UDPEcho_SendPathDelayDeltaJitterMin "udpecho.%i.sendpathdelaydeltajittermin"

///Diagnostics.UDPEcho.SendPathDelayDeltaJitterMax 	string 	readonly 	none 	0 		Maximum packet delay deltajitter in the up-stream direction (decimal milliseconds as string).
#define Diagnostics_UDPEcho_SendPathDelayDeltaJitterMax "udpecho.%i.sendpathdelaydeltajittermax"



///Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterAverage 	string 	readonly 	none 	0 		Average packet delay deltajitter in the down-stream direction (decimal milliseconds as string).
#define Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterAverage "udpecho.%i.receivepathdelaydeltajitteraverage"

///Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterMin 	string 	readonly 	none 	0 		Minimum packet delay deltajitter in the down-stream direction (decimal milliseconds as string).
#define Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMin "udpecho.%i.receivepathdelaydeltajittermin"

///Diagnostics.UDPEcho.ReceivePathDelayDeltaJitterMax 	string 	readonly 	none 	0 		Maximum packet delay deltajitter in the down-stream direction (decimal milliseconds as string).
#define Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMax "udpecho.%i.receivepathdelaydeltajittermax"


///Diagnostics.UDPEcho.TC1_CIR		uint 	readonly	none 	1500000 		TC1_CIR  rate
#define Diagnostics_UDPEcho_TC1_CIR	"udpecho.%i.tc1_cir"

///Diagnostics.UDPEcho.TC4_PIR		uint 	readonly	none 	10000000 		TC4_PIR  rate
#define Diagnostics_UDPEcho_TC4_PIR	"udpecho.%i.tc4_pir"

// TC2_PIR rate
#define Diagnostics_UDPEcho_TC2_PIR	"udpecho.%i.tc2_pir"



///Diagnostics.UDPEcho.StatsName 	string 	readonly 	none 	0 		Maximum packet delay delta in the down-stream direction (decimal milliseconds as string).
#define Diagnostics_UDPEcho_StatsName	"udpecho.%i.statsname"

//#define Diagnostics_UDPEcho_RXBytes	"udpecho.%i.rxbytes"
//#define Diagnostics_UDPEcho_TXBytes	"udpecho.%i.txbytes"
#define Diagnostics_UDPEcho_RXPkts	"udpecho.%i.rxpkts"
#define Diagnostics_UDPEcho_TXPkts	"udpecho.%i.txpkts"

//#define Diagnostics_UDPEcho_SmartedgeRXBytes "udpecho.%i.smartedgerxbytes"
//#define Diagnostics_UDPEcho_SmartedgeTXBytes "udpecho.%i.smartedgetxbytes"
#define Diagnostics_UDPEcho_RXPktsSmartedge	"udpecho.%i.rxpktssmartedge"
#define Diagnostics_UDPEcho_TXPktsSmartedge	"udpecho.%i.txpktssmartedge"




#define RDB_VAR_NONE 		0
#define RDB_VAR_SET_IF 		0x01 // used by IF interface

#define RDB_VAR_SET_TEST_IF 0x04 // rdb used only by test console

#define RDB_VAR_SUBCRIBE_TEST	0x40 // subscribed only test console
#define RDB_VAR_SUBCRIBE		0x80 // subscribed

#define RDB_STATIC			0x100

#define RDB_DEVNAME  		"/dev/cdcs_DD"

#define RDB_MAX_LEN          128

typedef struct TRdbNameList
{
    char* szName;
    int bcreate; // create if it does not exit
    int attribute;	//RDB_VAR_XXXX
    char*szValue; // default value
} TRdbNameList;


extern const TRdbNameList g_rdbNameList[];

extern  struct rdb_session *g_rdb_session;

////////////////////////////////////////////////////////////////////////////////
// initilize rdb variable
// session_id (in) -- <=0 no session  id in the rdb variable
// fCreate (in) -- force create all the variable
extern int rdb_init(int session_id, int fCreate);


extern void rdb_end(int session_id,  int remove_rdb);

////////////////////////////////////////////////////////////////////////////////
extern int subscribe(int session_id, int subs_group);

////////////////////////
extern int rdb_get_boolean(const char* name, int *value);
////////////////////////////////////////
extern int rdb_set_boolean(const char* name, int value);
//////////////////////////////////////////
extern int rdb_get_uint(const char* name, unsigned int *value);
//////////////////////////////////////////////
extern int rdb_set_uint(const char* name, unsigned int value);

extern int rdb_set_int(const char* name, int value);

extern int rdb_set_float(const char*fmt, const char* name, double value);

static inline int rdb_set_str(const char* name, const char* value)
{
	//NTCLOG_DEBUG("%s =%s", name, value);
	return rdb_set_string(g_rdb_session, name, value);

}

extern char* rdb_get_i_name(char *buf, const char *namefmt, int i);

extern int rdb_get_i_uint(const char* namefmt, int i, unsigned int *value);
//////////////////////////////////////////////
extern int rdb_set_i_uint(const char* namefmt, int i, unsigned int value);

extern int rdb_get_i_str(const char* namefmt, int i, char* value, int len);

extern int rdb_set_i_str(const char* namefmt, int i, const char* value);

extern int rdb_set_i_float(const char*fmt, const char* namefmt, int i,  double value);

extern int rdb_get_i_boolean(const char* namefmt, int i,  int *value);

extern int rdb_set_i_boolean(const char* namefmt, int i, int value);

// poll any rdb changed
// $ >0   --- rdb changed
// $ 0 --- rdb not changed
// $ <0 -- rdb error
extern int poll_rdb(int timeout_sec, int timeout_usec);

#endif
