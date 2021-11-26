/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "globals.h"
#include "paramaccess.h"
#include "parameterStore.h"
#include "utils.h"
#include "parameter.h"
#include "ethParameter.h"
#include "diagParameter.h"
#include "voipParameter.h"
#include "diagParameter.h"
// Profiles:
#include "profiles/ipping_profile.h"
#include "profiles/traceroute_profile.h"
#include "profiles/time_profile.h"

#define LAYER_3_FORWARD_SIZE 3

static int initDebug (const char *, ParameterType, ParameterValue *);
static int getInt (const char *, ParameterType, ParameterValue *);
static int getString (const char *, ParameterType, ParameterValue *);
static int initInt (const char *, ParameterType, ParameterValue *);
static int initString (const char *, ParameterType, ParameterValue *);
static int setString (const char *, ParameterType, ParameterValue *);
static int setInt (const char *, ParameterType, ParameterValue *);
static int getUptime (const char *, ParameterType, ParameterValue *);
static int getLocalNetworkData (const char *);
static int getMacAddr (const char *, ParameterType, ParameterValue *);
static int getCRU (const char *, ParameterType, ParameterValue *);
static int getOUI (const char *, ParameterType, ParameterValue *);
static int getManageableDeviceCount( const char *, ParameterType, ParameterValue *);
static int setLayer3ForwardingEnable( const char *, ParameterType, ParameterValue * );
static int getLayer3ForwardingEnable( const char *, ParameterType, ParameterValue * );
static int setLayer3ForwardingType( const char *, ParameterType, ParameterValue * );
static int getLayer3ForwardingType( const char *, ParameterType, ParameterValue * );
static int getLayer3ForwardingStatus (const char *, ParameterType, ParameterValue *);

extern int	setUDPEchoConfig(const char *, ParameterType, ParameterValue *);

Func initArray[] = {
	{1, 	&initParamValue},
	{2, 	&initString},
	{3, 	&initInt},
	{10000,	&initVdMaxProfile},
	{10001,	&initVdMaxLine}
};

static int initArraySize = sizeof (initArray) / sizeof (Func);

Func deleteArray[] = {
	{1, 	&initDebug},
	{2, 	&initString},
	{3, 	&initInt},
	{10000,	&initVdMaxProfile},
	{10001,	&initVdMaxLine}
};

static int deleteArraySize = sizeof (deleteArray) / sizeof (Func);

Func getArray[] = {
	{1,	&retrieveParamValue},
	{2, 	&getInt},
	{3,		&getString},
	{112, 	&getOUI},
	{116, 	&getMacAddr},
	{117, 	&getCRU},
	{118, 	&getUdpCRU},
	{124, 	&getUptime},
	{200, 	&getManageableDeviceCount},

	{811, 	&getETH0EnabledForInternet},
	{812, 	&getETH0WANAccessType},
	{813, 	&getETH0Layer1UpstreamMaxBitRate},
	{817, 	&getETH0SentBytes},
	{818, 	&getETH0ReceivedBytes},
	{819, 	&getETH0SentPackets},
	{820, 	&getETH0ReceivedPackets},

	{2000,	&getLayer3ForwardingEnable},
	{2001, 	&getLayer3ForwardingStatus},
	{2002, 	&getLayer3ForwardingType},

	{10100, &getVdLineEnable},
	{10101, &getVdLineDirectoryNumber},
	{10200, &getVdLinePacketsSent},
	// San. 07 may 2011: IPPing profile getters:
	{16000, &get_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState },
	{16001, &get_InternetGatewayDevice_IPPingDiagnostics_Interface },
	{16002, &get_InternetGatewayDevice_IPPingDiagnostics_Host },
	{16003, &get_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions },
	{16004, &get_InternetGatewayDevice_IPPingDiagnostics_Timeout },
	{16005, &get_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize },
	{16006, &get_InternetGatewayDevice_IPPingDiagnostics_DSCP },
	{16007, &get_InternetGatewayDevice_IPPingDiagnostics_SuccessCount },
	{16008, &get_InternetGatewayDevice_IPPingDiagnostics_FailureCount },
	{16009, &get_InternetGatewayDevice_IPPingDiagnostics_AverageResponseTime },
	{16010, &get_InternetGatewayDevice_IPPingDiagnostics_MinimumResponseTime },
	{16011, &get_InternetGatewayDevice_IPPingDiagnostics_MaximumResponseTime },
	// gonchar. 12 may 2011: TraceRoute profile getters:
	{16500, &get_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState },
	{16501, &get_InternetGatewayDevice_TraceRouteDiagnostics_Interface },
	{16502, &get_InternetGatewayDevice_TraceRouteDiagnostics_Host },
	{16503, &get_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries },
	{16504, &get_InternetGatewayDevice_TraceRouteDiagnostics_Timeout },
	{16505, &get_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize },
	{16506, &get_InternetGatewayDevice_TraceRouteDiagnostics_DSCP },
	{16507, &get_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount },
	{16508, &get_InternetGatewayDevice_TraceRouteDiagnostics_ResponseTime },
	{16509, &get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHopsNumberOfEntries },
	{16510, &get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHost },
	{16511, &get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHostAddress },
	{16512, &get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopErrorCode },
	{16513, &get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopRTTimes },
	// gonchar. 18 may 2011: Time profile getters:
	{16600, &get_InternetGatewayDevice_Time_Enable },
	{16601, &get_InternetGatewayDevice_Time_Status },
	{16602, &get_InternetGatewayDevice_Time_NTPServer1 },
	{16603, &get_InternetGatewayDevice_Time_NTPServer2 },
	{16604, &get_InternetGatewayDevice_Time_NTPServer3 },
	{16605, &get_InternetGatewayDevice_Time_NTPServer4 },
	{16606, &get_InternetGatewayDevice_Time_NTPServer5 },
	{16607, &get_InternetGatewayDevice_Time_CurrentLocalTime },
	{16608, &get_InternetGatewayDevice_Time_LocalTimeZone },
	{16609, &get_InternetGatewayDevice_Time_LocalTimeZoneName },
	{16610, &get_InternetGatewayDevice_Time_DaylightSavingsUsed },
	{16611, &get_InternetGatewayDevice_Time_DaylightSavingsStart },
	{16612, &get_InternetGatewayDevice_Time_DaylightSavingsEnd }
};

static int getArraySize = sizeof (getArray) / sizeof (Func);

Func setArray[] = {
	{1, 	&storeParamValue},
	{2, 	&setInt},
	{3,		&setString},
	{020, 	&setATMF5Diagnostics},
	{811, 	&setETH0EnabledForInternet},
	{1000, 	&setIPPingDiagnostics},
	{1010, 	&setWANDSLDiagnostics},
	{1020, 	&setATMF5Diagnostics},
	{2000, 	&setLayer3ForwardingEnable},
	{2002, 	&setLayer3ForwardingType},
	{3000, 	&setDownloadDiagnostics},
	{4000, 	&setUploadDiagnostics},
	{5000, 	&setUDPEchoConfig},
	{10100,	&setVdLineEnable},
	{10101,	&setVdLineDirectoryNumber},
	// San. 07 may 2011: IPPing profile setters:
	{16000, &set_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState },
	{16001, &set_InternetGatewayDevice_IPPingDiagnostics_Interface },
	{16002, &set_InternetGatewayDevice_IPPingDiagnostics_Host },
	{16003, &set_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions },
	{16004, &set_InternetGatewayDevice_IPPingDiagnostics_Timeout },
	{16005, &set_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize },
	{16006, &set_InternetGatewayDevice_IPPingDiagnostics_DSCP },
	// gonchar. 12 may 2011: TraceRoute profile setters:
	{16500, &set_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState },
	{16501, &set_InternetGatewayDevice_TraceRouteDiagnostics_Interface },
	{16502, &set_InternetGatewayDevice_TraceRouteDiagnostics_Host },
	{16503, &set_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries },
	{16504, &set_InternetGatewayDevice_TraceRouteDiagnostics_Timeout },
	{16505, &set_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize },
	{16506, &set_InternetGatewayDevice_TraceRouteDiagnostics_DSCP },
	{16507, &set_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount },
	// gonchar. 18 may 2011: Time profile setters:
	{16600, &set_InternetGatewayDevice_Time_Enable },
	{16602, &set_InternetGatewayDevice_Time_NTPServer1 },
	{16603, &set_InternetGatewayDevice_Time_NTPServer2 },
	{16604, &set_InternetGatewayDevice_Time_NTPServer3 },
	{16605, &set_InternetGatewayDevice_Time_NTPServer4 },
	{16606, &set_InternetGatewayDevice_Time_NTPServer5 },
	{16608, &set_InternetGatewayDevice_Time_LocalTimeZone },
	{16609, &set_InternetGatewayDevice_Time_LocalTimeZoneName },
	{16610, &set_InternetGatewayDevice_Time_DaylightSavingsUsed },
	{16611, &set_InternetGatewayDevice_Time_DaylightSavingsStart },
	{16612, &set_InternetGatewayDevice_Time_DaylightSavingsEnd }
};

static int setArraySize = sizeof (setArray) / sizeof (Func);

struct Layer3Forward
{
	bool enable;
	char *string;
	char *type;
} layer3Forward[LAYER_3_FORWARD_SIZE];

/** Calls the indexed function with the given parameters 
   the call is made through the initArray[] 
*/
int
initAccess (int idx, const char *name, ParameterType type, ParameterValue *value)
{
	register int i = 0;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "InitAccess: %d %s\n", idx, name);
	)

	for (i = 0; i != initArraySize; i++)
	{
		if (initArray[i].idx == idx)
			return initArray[i].func (name, type, value);
	}

	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "InitAccess: %d %s\n", idx, name);
	)

	return ERR_INTERNAL_ERROR;
}

/** Calls the indexed function with the given parameters 
   the call is made through the deleteArray[] 
*/
int
deleteAccess (int idx, const char *name, ParameterType type, ParameterValue *value)
{
	register int i = 0;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "DeleteAccess: %d %s\n", idx, name);
	)

	for (i = 0; i != deleteArraySize; i++)
	{
		if (deleteArray[i].idx == idx)
			return deleteArray[i].func (name, type, value);
	}

	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "DeleteAccess: %d %s\n", idx, name);
	)

	return ERR_INTERNAL_ERROR;
}

/** Calls the indexed function with the given parameters 
   the call is made through the getArray[] 
*/
int
getAccess (int idx, const char *name, ParameterType type, ParameterValue *value)
{
	register int i = 0;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "GetAccess: %d %s %p\n", idx, name, value);
	)

	for (i = 0; i != getArraySize; i++)
	{
		if (getArray[i].idx == idx)
			return getArray[i].func (name, type, value);
	}

	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "GetAccess: %d %s %p\n", idx, name, value);
	)

	return ERR_INTERNAL_ERROR;
}

/** Calls the indexed function with the given parameters 
   the call is made through the setArray[] 
*/
int
setAccess (int idx, const char *name, ParameterType type, ParameterValue *value)
{
	register int i = 0;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "SetAccess: %d %s %p\n", idx, name, value);
	)

	for (i = 0; i != setArraySize; i++)
	{
		if (setArray[i].idx == idx)
			return setArray[i].func (name, type, value);
	}

	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "SetAccess: %d %s %p\n", idx, name, value);
	)

	return ERR_INTERNAL_ERROR;
}

/** Debug function, do nothing then to print the parameter name and type
*/
int
initDebug (const char *name, ParameterType type, ParameterValue *value)
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "InitDebug: %s %d\n", name, type);
	)

	setParameterReturnStatus(PARAMETER_CHANGES_NOT_APPLIED);

	return OK;
}

/** Sample Functions
*/
static int
getString (const char *name, ParameterType type, ParameterValue *value)
{
	value->out_cval = "Test";

	return OK;
}

static int
getInt (const char *name, ParameterType type, ParameterValue *value)
{
	int x = 1234;
	value->out_int = x;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "GetInt %s %d\n", name, value->out_int);
	)

	return OK;
}

static int
initString(const char *name, ParameterType type, ParameterValue *value)
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "Init string: %s %d\n", name, type);
	)

	return OK;
}

static int
initInt(const char *name, ParameterType type, ParameterValue *value)
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "Init int: %s %d\n", name, type);
	)

	return OK;
}

static int
setString (const char *name, ParameterType type, ParameterValue *value)
{
#ifdef _DEBUG
	char *in = value->in_cval;
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "SetString %s %s\n", name, in);
	)

	return OK;
}

static int
setInt (const char *name, ParameterType type, ParameterValue *value)
{
#ifdef _DEBUG
	int in = value->in_int;
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACCESS, "SetInt %s %d\n", name, in);
	)

	return OK;
}

static int
getUptime (const char *name, ParameterType type, ParameterValue *value)
{
	struct sysinfo info;

	sysinfo (&info);
	value->out_uint = (unsigned int) info.uptime;

	return OK;
}

static char macAddr[13] = { '\0' };
static char ip[256] = { '\0' };

static int
getLocalNetworkData (const char *interfaceName)
{
	int sfd;
	unsigned char *u;
	struct ifreq ifr;
	struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;

	memset (&ifr, 0, sizeof ifr);

	if (0 > (sfd = socket (AF_INET, SOCK_STREAM, 0)))
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "getLocalNetworkData socket failure\n");
		)

		return ERR_INTERNAL_ERROR;
	}

	strcpy (ifr.ifr_name, interfaceName);
	sin->sin_family = AF_INET;

	/*if (0 > ioctl (sfd, SIOCGIFADDR, &ifr))
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "getLocalNetworkData: getIPAddr\n");
		)

		return ERR_INTERNAL_ERROR;
	}
	else
	{
		strncpy (ip, inet_ntoa (sin->sin_addr), (sizeof (ip) - 1));
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_ACCESS, "IPAddress: %s length: %d\n", ip, strlen ( ip ));
		)
	}*/

	if (0 > ioctl (sfd, SIOCGIFHWADDR, &ifr))
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "getLocalNetworkData: getMacAddr\n");
		)

		return ERR_INTERNAL_ERROR;
	}
	else
	{
		u = (unsigned char *) &ifr.ifr_addr.sa_data;
		if (u[0] + u[1] + u[2] + u[3] + u[4] + u[5])
		{
			sprintf (macAddr, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", u[0], u[1], u[2], u[3], u[4], u[5]);
		}
	}

	return OK;
}

static int
getMacAddr (const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	// If the MacAddress is already there ( macAddr[0] != '\0' ) we get it from
	// the buffer
	if (macAddr[0] == '\0') {
		ret = getLocalNetworkData ("eth0");
		if (ret != OK) {
			ret = getLocalNetworkData ("eth2");
			if ( ret != OK ) 
				return ret;
		}
	}
	value->out_cval = macAddr;

	return OK;
}

static char cru[40] = { '\0' };
extern int procId;
extern char host[256];

/** Get the ConnectionRequestURL, which is the URL the ACS is using to trigger the CPE
* Form:  http://<cpe ip addr>/acscall 
*/
static int
getCRU (const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	if(strcmp(host, "0.0.0.0"))
	{
		/* no host != 0.0.0.0 */
		sprintf(cru, "http://%s:%d/%s", host , ACS_NOTIFICATION_PORT + (procId), ACS_NOTIFICATION_PARAM);
		strcpy(ip, host);
		value->out_cval = cru;
	}
	else
	{
		/* yes  host == 0.0.0.0 */ /*get CRU from data model */
		int ret = retrieveParamValue(name, type, value);
		if (ret != OK)
		{
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "setUDPEchoConfig()->retrieveParamValue() returns error = %i\n",ret);
			)
			return ret;
		}
		if (!value)	return ERR_INVALID_PARAMETER_VALUE;
		if (!value->out_cval) return ERR_INVALID_PARAMETER_VALUE;

		sscanf(value->out_cval, "http://%[^:]*", &ip);
		strcpy(cru, value->out_cval);
		if(!strcmp(ip,"" ))
		{
			DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "getCRU: Not found ip address\n");
			)
		}
	}

	return OK;
}

static char udp_cru[40]  = { '\0' };
static char udp_ip[40]   = { '\0' };
static char udp_port[40] = { '\0' };

/*
 * Get the UDPConnectionRequestAddress, which is the URL the ACS is using to trigger the CPE
 * Form:  <cpe ip addr>:port 
 */
int
getUdpCRU (const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	ParameterType  t = StringType;
	ParameterValue v;

	// If the URL is already there ( udp_cru[0] != '\0' ) we get it from
	// the buffer
	if (udp_cru[0] == '\0')
	{
		getCRU(CONNECTION_REQUEST_URL, t,  &v);
		sprintf( udp_cru,  "%s:%d", ip , ACS_UDP_NOTIFICATION_PORT);
		sprintf( udp_ip,   "%s"   , ip);
		sprintf( udp_port, "%d"   , ACS_UDP_NOTIFICATION_PORT);
	}

	value->out_cval = udp_cru;

	return OK;
}

/* 
 * Test IP address to see if matches udp_ip address previously found
 * by call to getUdpCRU
 *
 */

int 
is_IP_local( char *ip_addr )
{
  if ( strcmp( udp_ip, ip_addr ) == 0 ) 
    return(0);

  return(-1);
}

static char oui[6] = { '\0' };

/** The OUI are the first 24 bits ( 6 chars ) of the MAC Address
*/
static int
getOUI (const char *name, ParameterType type, ParameterValue *value)
{
	char *dummy;
	int ret;

	if (oui[0] == '\0')
	{
		ret = getMacAddr (name, type, (ParameterValue *)&dummy);
		if (ret == OK)
			strncpy (oui, dummy, 6);
	}
	value->out_cval = oui;

	return OK;
}

static int
getManageableDeviceCount( const char *name, ParameterType type, ParameterValue *value )
{
	int ret = OK;
	int count;
	
	ret = countInstances( "InternetGatewayDevice.ManagementServer.ManageableDevice", &count );
	if ( ret == OK )
		value->out_int = count;

	return ret;
}

static int
setLayer3ForwardingEnable (const char *name, ParameterType type, ParameterValue *value)
{
	register int idx;

	idx = getRevIdx (name, "Enable");
	if (idx > 0 && idx <= LAYER_3_FORWARD_SIZE)
	{
		// align index for faster access
		idx--;
		layer3Forward[idx].enable = value->in_int;
		return OK;
	}
	else
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
}

static int
getLayer3ForwardingEnable (const char *name, ParameterType type, ParameterValue *value)
{
	register int idx;

	idx = getRevIdx (name, "Enable");
	if (idx > 0 && idx <= LAYER_3_FORWARD_SIZE)
	{
		idx--;
		BOOL_GET value = layer3Forward[idx].enable;
		return OK;
	}
	else
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
}

int
getLayer3ForwardingStatus (const char *name, ParameterType type, ParameterValue *value)
{
	return OK;
}

static int
setLayer3ForwardingType (const char *name, ParameterType type, ParameterValue *value)
{
	register int idx = 0;
	register char **l3type;

	idx = getRevIdx (name, "Type");
	if (idx > 0 && idx <= LAYER_3_FORWARD_SIZE)
	{
		idx--;
		l3type = &layer3Forward[idx].type;
		if (*l3type != NULL)
			efree (*l3type);
		*l3type =
			strnDup (*l3type, value->in_cval, strlen (value->in_cval));
		return OK;
	}
	else
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
}

static int
getLayer3ForwardingType (const char *name, ParameterType type, ParameterValue *value)
{
	register int idx = 0;
	// register char **l3type;

	idx = getRevIdx (name, "Type");
	if (idx > 0 && idx <= LAYER_3_FORWARD_SIZE)
	{
		idx--;
		STRING_GET value = layer3Forward[idx].type;
		return OK;
	}
	else
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
}
