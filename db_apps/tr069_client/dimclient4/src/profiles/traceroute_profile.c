/***************************************************************************
 *    Copyright (C) 2004-2011 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/
/*
 * traceroute_profile.c
 *
 *  Created on: May 12, 2011
 *      Author: gonchar
 */

#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "globals.h"
#include "../paramaccess.h"
#include "parameterStore.h"
#include "utils.h"
#include "parameter.h"
#include "ethParameter.h"
#include "diagParameter.h"
#include "voipParameter.h"

// TraceRoute Profile. Getters:

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 1
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1000
 * Access List:
 * Default value: None
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.Interface
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 1
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_Interface: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.Host
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 1
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_Host: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.NumberOfTries
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.Timeout
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_Timeout: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.DataBlockSize
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.DSCP
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_DSCP: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.MaxHopCount
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: -1
 * Get Idx: 0
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.ResponseTime
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: -1
 * Get Idx: 0
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_ResponseTime(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_ResponseTime: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.RouteHopsNumberOfEntries
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: -1
 * Get Idx: 0
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHopsNumberOfEntries(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHopsNumberOfEntries: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.{i}.HopHost
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: -1
 * Get Idx: 0
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHost(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHost: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.{i}.HopHostAddress
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: -1
 * Get Idx: 0
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHostAddress(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHostAddress: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.{i}.HopErrorCode
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: -1
 * Get Idx: 0
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopErrorCode(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopErrorCode: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.{i}.HopRTTimes
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: -1
 * Get Idx: 0
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopRTTimes(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopRTTimes: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}






/****************************************************************************************************************************************************/
// TraceRoute Profile. Setters:
/****************************************************************************************************************************************************/
/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 1
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1000
 * Access List:
 * Default value: None
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	ret = setTraceRouteDiagnostics(name, type,value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState: setTraceRouteDiagnostics() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.Interface
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 1
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_Interface: storeParamValue() error1\n");
		)
		return ret;
	}

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_Interface: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.Host
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 1
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_Host: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_Host: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.NumberOfTries
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfRepetitions: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.Timeout
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_Timeout: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_Timeout: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.DataBlockSize
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.DSCP
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_DSCP: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_DSCP: storeParamValue() error2\n");
		)
		return ret;
	}


	return OK;
}


/**
 * Parameter Name: InternetGatewayDevice.TraceRouteDiagnostics.MaxHopCount
 * Type ID: 9
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 1
 * Set Idx: 1
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount: storeParamValue() error2\n");
		)
		return ret;
	}


	return OK;
}
