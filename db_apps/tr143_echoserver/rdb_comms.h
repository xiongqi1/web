/* ----------------------------------------------------------------------------
RDB interface program

Lee Huang<leeh@netcomm.com.au>

*/

#ifndef __RDB_COMMS_H
#define __RDB_COMMS_H


#include "rdb_ops.h"

///UDPEchoConfig. 	object 	readonly 	none 	present 		UDPEchoPlus diagnostic object.
#define UDPEchoConfig "UDPEchoConfig."

///UDPEchoConfig.Enable 	bool 	readwrite 	none 	0
#define UDPEchoConfig_Enable "UDPEchoConfig.Enable"

///UDPEchoConfig.InterfaceInternal 	string 	readwrite 	none 	""      -> IP-layer interface over which the CPE MUST listen and receive UDP echo requests on
#define UDPEchoConfig_Interface  "UDPEchoConfig.InterfaceInternal"

///UDPEchoConfig.SourceIPAddress 	string 	readwrite 	none 	"0.0.0.0" -> The Source IP address of the UDP echo packet.
#define UDPEchoConfig_SourceIPAddress  "UDPEchoConfig.SourceIPAddress"

///UDPEchoConfig.UDPPort 	uint 	readwrite 	none 	0.0.0.0 -> The UDP port on which the UDP server MUST listen and respond to UDP echo requests.
#define UDPEchoConfig_UDPPort  "UDPEchoConfig.UDPPort"

///UDPEchoConfig.EchoPlusEnabled 	bool 	readwrite 	none 	1 -> If True the CPE will perform necessary packet processing for UDP Echo Plus packets.
#define UDPEchoConfig_EchoPlusEnabled  "UDPEchoConfig.EchoPlusEnabled"

///UDPEchoConfig.PacketsReceived 	uint 	readonly 	none 	0.0.0.0 -> Incremented upon each valid UDP echo packet received.
#define UDPEchoConfig_PacketsReceived  "UDPEchoConfig.PacketsReceived"

///UDPEchoConfig.PacketsResponded 	uint 	readonly 	none 	0.0.0.0 -> Incremented for each UDP echo response sent.
#define UDPEchoConfig_PacketsResponded  "UDPEchoConfig.PacketsResponded"

///UDPEchoConfig.BytesReceived 	uint 	readonly 	none 	0.0.0.0 -> The number of UDP received bytes including payload and UDP header after the UDPEchoConfig is enabled.
#define UDPEchoConfig_BytesReceived  "UDPEchoConfig.BytesReceived"

///UDPEchoConfig.BytesResponded 	uint 	readonly 	none 	0.0.0.0 -> The number of UDP responded bytes, including payload and UDP header sent after the UDPEchoConfig is enabled.
#define UDPEchoConfig_BytesResponded  "UDPEchoConfig.BytesResponded"

///UDPEchoConfig.TimeFirstPacketReceived 	string 	readonly 	none 	0.0.0.0 -> The time that the server receives the first UDP echo packet after the UDPEchoConfig is enabled.
#define UDPEchoConfig_TimeFirstPacketReceived  "UDPEchoConfig.TimeFirstPacketReceived"

///UDPEchoConfig.TimeLastPacketReceived 	string 	readonly 	none 	0.0.0.0 -> The time that the server receives the most recent UDP echo packet.
#define UDPEchoConfig_TimeLastPacketReceived  "UDPEchoConfig.TimeLastPacketReceived"




#define RDB_VAR_NONE 		0
#define RDB_VAR_SET_IF 		0x01 // used by IF interface
#define RDB_VAR_PREFIX		0x10 // it is prefix of RDB variable
#define RDB_VAR_SUBCRIBE	0x80 // subscribed

typedef struct TRdbNameList
{
    const char* szName;
    int bcreate; // create if it does not exit
    int attribute;	//RDB_VAR_XXXX
    const char*szValue; // default value
} TRdbNameList;

extern const TRdbNameList g_rdbNameList[];

////////////////////////////////////////////////////////////////////////////////
// initilize rdb variable
// fCreate (in) -- force create all the variable
extern int rdb_init(int fCreate);
////////////////////////////////////////////////////////////////////////////////
extern int subscribe(void);

////////////////////////
extern int rdb_get_boolean(const char* name, int *value);
////////////////////////////////////////
extern int rdb_set_boolean(const char* name, int value);
//////////////////////////////////////////
extern int rdb_getint(const char* name, unsigned int *value);
//////////////////////////////////////////////
extern int rdb_set_int(const char* name, unsigned int value);

extern int rdb_set_sint(const char* name, int value);


static inline int rdb_set_str(const char* name, const char* value)
{
#ifdef _DEBUG
	SYSLOG_DEBUG("%s =%s", name, value);
#endif
	return rdb_set_single(name, value);

}

#endif
