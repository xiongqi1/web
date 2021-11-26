#include "moduledatabase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "detectinfo.h"

#define DATABASE_VARIABLE_SIMSTATUS						"sim.status.status"
#define DATABASE_VARIABLE_NETWORKNAME					"system_network_status.network"
#define DATABASE_VARIABLE_PROVIDER						"service_provider_name"

#define DATABASE_VARIABLE_SIGNALSTRENGTH			"radio.information.signal_strength"
#define	DATABASE_VARIABLE_SERVICETYPE					"system_network_status.service_type"
#define	DATABASE_VARIABLE_CURRENTBAND					"system_network_status.current_band"
#define	DATABASE_VARIABLE_IMEI								"imei"

#define DATABASE_VARIABLE_PIN_REMAINING				"sim.status.retries_remaining"
#define DATABASE_VARIABLE_PUK_REMAINING				"sim.status.retries_puk_remaining"


#define DATABASE_VARIABLE_MCC									"imsi.plmn_mcc"
#define DATABASE_VARIABLE_MNC									"imsi.plmn_mnc"


#define	DATABASE_VARIABLE_MODEL								"model"
#define	DATABASE_VARIABLE_MANUFACTURE					"manufacture"
#define	DATABASE_VARIABLE_FIRMWARE						"firmware_version"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// database query table
////////////////////////////////////////////////////////////////////////////////
const dbquery dbQueries[] =
{
	{DATABASE_VARIABLE_PIN_REMAINING,				-1,		callback_entry_type_permanent},
	{DATABASE_VARIABLE_PUK_REMAINING,				-1,		callback_entry_type_permanent},
	
	{DATABASE_VARIABLE_MODEL,								-1,		callback_entry_type_permanent},
	{DATABASE_VARIABLE_MANUFACTURE,					-1,		callback_entry_type_permanent},
	{DATABASE_VARIABLE_FIRMWARE,						-1,		callback_entry_type_permanent},

	{DATABASE_VARIABLE_SIGNALSTRENGTH,			-1,		callback_entry_type_permanent},
	{DATABASE_VARIABLE_IMEI,								-1,		callback_entry_type_permanent},
	{DATABASE_VARIABLE_MCC,								-1,		callback_entry_type_permanent},
	{DATABASE_VARIABLE_MNC,								-1,		callback_entry_type_permanent},

	{DATABASE_VARIABLE_NETWORKNAME,					-1,		callback_entry_type_permanent},
	{DATABASE_VARIABLE_PROVIDER,					-1,		callback_entry_type_permanent},

	{DATABASE_VARIABLE_SERVICETYPE,					-1,		callback_entry_type_permanent},

	{DATABASE_VARIABLE_CURRENTBAND,					-1,		callback_entry_type_permanent},


	/*
		{DATABASE_VARIABLE_SIMSTATUS,						-1,		callback_entry_type_permanent},
	*/

	/*
		{"radio.temperature",										-1,		callback_entry_type_permanent},
	*/

	{NULL,								-1}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// default device information
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const devinfo genericDevInfo =
    {PHONE_MODULE_NAME_GENERIC, 0x0000, 0x0000, 0, 0, 0, "/dev/ttyUSB0", "/dev/ttyUSB0", "/dev/ttyUSB0"};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sierra AT command answer string - service type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
STRCONVERT_TABLE_BEGIN(SierraServiceDomain)
{
	"No service",
	"Circuit-switched service",
	"Packed-swtiched service",
	"Combined service",
	"Any"
}
STRCONVERT_TABLE_END(SierraServiceDomain)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sierra AT command answer string - sim status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
STRCONVERT_TABLE_BEGIN(SierraSimStatus)
{
	"SIM not available",
	"SIM OK"
}
STRCONVERT_TABLE_END(SierraSimStatus)


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic AT command answer string - get MNC
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STRCONVERT_FUNCTION_NAME(GenericGetMNC)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf)
{

	int iValue = atoi(szValue);

	if (strlen(szValue))
		snprintf(pBuf, cbBuf, "%d", iValue);
	else
		strcpy(pBuf, "");

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic AT command answer string - get MNC
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STRCONVERT_FUNCTION_NAME(grlMobileGetMNC)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf)
{

	int iValue = atoi(szValue);

	// if Voodafone, force to use crazy john MNC
	if (iValue == 3)
		iValue = 38;

	if (strlen(szValue))
		snprintf(pBuf, cbBuf, "%d", iValue);
	else
		strcpy(pBuf, "");

	return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic AT command answer string - signal strength
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STRCONVERT_FUNCTION_NAME(GenericSignalStrength)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf)
{
	int iValue = atoi(szValue);

	if (iValue > 63)
		iValue = 0;

	snprintf(pBuf, cbBuf, "%d", iValue - 110);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic AT command answer string - signal strength
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
int STRCONVERT_FUNCTION_NAME(GenericPinStatus)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf)
{
	const char* szSIMOK = "SIM OK";
	const char* pSrc = szValue;

	if (!strcmp(szValue, "READY"))
		pSrc = szSIMOK;

	strncpy(pBuf, pSrc, cbBuf);
	pBuf[cbBuf-1] = 0;

	return 0;
}
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic AT command answer string - service type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
STRCONVERT_TABLE_BEGIN(GenericServiceTypeDirect)
{
	"GSM",
	"GSM Compact",
	"UMTS",
	"EGPRS"
	"HSDPA"
	"HSUPA",
	"HSDPA/HSUPA",
	"E-UMTS"
}
STRCONVERT_TABLE_END(GenericServiceTypeDirect)

int STRCONVERT_FUNCTION_NAME(GenericServiceType)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf)
{
	pBuf[0] = 0;

	// bypass if nothing
	if (!strlen(szValue))
		return 0;

	int iVal = atoi(szValue);
	if (0 <= iVal && iVal <= 7)
		STRCONVERT_FUNCTION_NAME(GenericServiceTypeDirect)(szDbVarName, iIdx, szValue, pBuf, cbBuf);
	else
		snprintf(pBuf, cbBuf, "unknown #%d", iVal);

	return 0;
}


STRCONVERT_TABLE_BEGIN(IPWLServiceTypeDirect)
{
	"unknown",
	"available",
	"currenct",
	"forbidden"
}
STRCONVERT_TABLE_END(IPWLServiceTypeDirect)

int STRCONVERT_FUNCTION_NAME(IPWLServiceType)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf)
{
	pBuf[0] = 0;

	// bypass if nothing
	if (!strlen(szValue))
		return 0;

	int iVal = atoi(szValue);
	if (0 <= iVal && iVal <= 3)
		STRCONVERT_FUNCTION_NAME(IPWLServiceTypeDirect)(szDbVarName, iIdx, szValue, pBuf, cbBuf);
	else
		snprintf(pBuf, cbBuf, "unknown #%d", iVal);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic AT command tables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const attranstbl genericAtCommandTable[] =
{
	{DATABASE_VARIABLE_MANUFACTURE,
		"at+cgmi", "at\\+cgmi\n([])\n", NULL, NULL},

	{DATABASE_VARIABLE_MODEL,
		"at+cgmm", "at\\+cgmm\n([])\n", NULL, NULL},

	{DATABASE_VARIABLE_FIRMWARE,
		"at+cgmr", "at\\+cgmr\n([])\n", NULL, NULL},

	{DATABASE_VARIABLE_NETWORKNAME,
		"at+cops?", "\\+COPS: .*,\"([])\".*\n", NULL, NULL},

	{DATABASE_VARIABLE_PROVIDER,
		"at+cops?", "\\+COPS: .*,\"([])\".*\n", NULL, NULL},

	{DATABASE_VARIABLE_SIGNALSTRENGTH,
		"at+csq", "\\+CSQ: ([]),.*\n", STRCONVERT_FUNCTION_NAME(GenericSignalStrength), "([])dBm"},

	{DATABASE_VARIABLE_IMEI,
		"at+cgsn", "[\n ]([[0-9]+])\n", NULL, NULL},

	{DATABASE_VARIABLE_MCC,
		"at+cimi", "[\n ]([[0-9][0-9][0-9]])[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]\n", NULL, NULL},

	// 001010123456063
	{DATABASE_VARIABLE_MNC,
		"at+cimi", "[\n ][0-9][0-9][0-9]([[0-9][0-9]])[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]\n", STRCONVERT_FUNCTION_NAME(GenericGetMNC), NULL},

	/*
		{DATABASE_VARIABLE_SIMSTATUS,
		 "at+cpin?", "(\\+CPIN|\\+CME ERROR): ([])\n", STRCONVERT_FUNCTION_NAME(GenericPinStatus), NULL},
	*/

	{DATABASE_VARIABLE_SERVICETYPE,
		"at+cops?", "\\+COPS: .*\",([])\n", STRCONVERT_FUNCTION_NAME(GenericServiceType), NULL},

	 // ^CPIN: READY,10,10,3,6,0
	{DATABASE_VARIABLE_PIN_REMAINING,
		"at^cpin?", "\\^CPIN: [^,]*,[0-9]*,[0-9]*,([[0-9]*]),.*\n", NULL, NULL},

	{DATABASE_VARIABLE_PUK_REMAINING,
		"at^cpin?", "\\^CPIN: [^,]*,[0-9]*,([[0-9]*]),.*\n", NULL, NULL},

	{NULL,
	 NULL, NULL, NULL}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sierra AT command tables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const attranstbl sierraAtCommandTable[] =
{
	{DATABASE_VARIABLE_CURRENTBAND,
		"at!getband?", "!GETBAND: ([])\n", NULL, NULL},

	{NULL,
	 NULL, NULL, NULL}
};

const attranstbl grlMobileAtCommandTable[] =
{
	// 505060005361353
	{DATABASE_VARIABLE_MNC,
		"at+cimi", "[\n ][0-9][0-9][0-9]([[0-9][0-9]])[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]\n", STRCONVERT_FUNCTION_NAME(grlMobileGetMNC), NULL},

	{NULL,
	 NULL, NULL, NULL}
};

const attranstbl ipwirelessPEMCommandTable[] =
{
	{DATABASE_VARIABLE_MANUFACTURE,
		"at+cgmi", "at\\+cgmi\n.*: ([])\n", NULL, NULL},

	{DATABASE_VARIABLE_MODEL,
	 "at+cgmm", "at\\+cgmm\n.*: ([])\n", NULL, NULL},

	{DATABASE_VARIABLE_FIRMWARE,
	 "at+cgmr", "at\\+cgmr\n.*: ([])\n", NULL, NULL},

	// +COPS: (2,"CA.IPW.com", "CA","0023"), (2,"NJ.IPW.com", "NJ","0064")
	{DATABASE_VARIABLE_NETWORKNAME, // not done
	 "at+cops?", "\\+COPS: .*[0-9]+,\"([])\".*\n", NULL, NULL},

	{DATABASE_VARIABLE_PROVIDER, // not done
	 "at+cops?", "\\+COPS: .*[0-9]+,\"([])\".*\n", NULL, NULL},

	{DATABASE_VARIABLE_SIGNALSTRENGTH,
	 "at+csq", "\\+CSQ: ([]),.*\n", STRCONVERT_FUNCTION_NAME(GenericSignalStrength), "([])dBm"},

	{DATABASE_VARIABLE_IMEI,
	 "at+cgsn", "\n.*: ([[0-9]+])\n", NULL, NULL},

  // 001010123456063
	{DATABASE_VARIABLE_MCC,
	 "at+cimi", "\n.*: ([[0-9][0-9][0-9]])[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]\n", NULL, NULL},

	// 001010123456063
	{DATABASE_VARIABLE_MNC,
	 "at+cimi", "\n.*: [0-9][0-9][0-9]([[0-9][0-9]])[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]\n", STRCONVERT_FUNCTION_NAME(GenericGetMNC), NULL},

	/*
		{DATABASE_VARIABLE_SIMSTATUS,
		 "at+cpin?", "(\\+CPIN|\\+CME ERROR): ([])\n", STRCONVERT_FUNCTION_NAME(GenericPinStatus), NULL},
	*/

	{DATABASE_VARIABLE_SERVICETYPE,
	 "at+cops?", "\\+COPS: .*\\(([]),.*\n", STRCONVERT_FUNCTION_NAME(IPWLServiceType), NULL},

	{NULL,
	 NULL, NULL, NULL}
};


///////////////////////////////////////////////////////////////////////////////
const moduleinfo moduleInfoTbl[] =
{
	{PHONE_MODULE_NAME_GENERIC,							genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_SIERRA_MC8780,				sierraAtCommandTable,			cmdchanger_module_type_MC8780},
	{PHONE_MODULE_NAME_SIEMENS_HC25,				genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_QUALCOMM_29,					genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_VODAFONE_E220,				genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_OPTUS_E220,					genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_3HUAWEI_E160G,				genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_TELSTRA_USB3_8521,		genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_VODAFONE_K3715,			genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_VODAFONE_K3765,			genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_VIRGIN_K3715,			  genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_TELSTRA_MF626,				genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_TELSTRA_MF636,				genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_3_MF627,							genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_SIERRA_MC8790V_NET,	genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_3_MF668,							genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_K3565Z,							genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_TELSTRA_BP3EXT,			genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_3HUAWEI_E180,				genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_GLRMODULE_U6T900GRL,	grlMobileAtCommandTable,	cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_IPWIRELESS,					ipwirelessPEMCommandTable, cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_SIMCOM,	            genericAtCommandTable,		cmdchanger_module_type_Generic},
	{PHONE_MODULE_NAME_LONGWAY,	            genericAtCommandTable,		cmdchanger_module_type_Generic},


	{NULL, NULL, 0}
};


///////////////////////////////////////////////////////////////////////////////
