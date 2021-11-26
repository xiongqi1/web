/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/
/*
 * ipping_profile.c
 *
 *  Created on: May 6, 2011
 *      Author: San
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
#include "diagParameter.h"

// IPPing Profile. Getters:

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState
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
int get_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.Interface
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
int get_InternetGatewayDevice_IPPingDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_Interface: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.Host
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
int get_InternetGatewayDevice_IPPingDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_Host: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.NumberOfRepetitions
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
int get_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.Timeout
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
int get_InternetGatewayDevice_IPPingDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_Timeout: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.DataBlockSize
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
int get_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.DSCP
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
int get_InternetGatewayDevice_IPPingDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_DSCP: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.SuccessCount
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
int get_InternetGatewayDevice_IPPingDiagnostics_SuccessCount(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_SuccessCount: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.FailureCount
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
int get_InternetGatewayDevice_IPPingDiagnostics_FailureCount(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_FailureCount: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.AverageResponseTime
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
int get_InternetGatewayDevice_IPPingDiagnostics_AverageResponseTime(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_AverageResponseTime: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.MinimumResponseTime
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
int get_InternetGatewayDevice_IPPingDiagnostics_MinimumResponseTime(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_MinimumResponseTime: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.MaximumResponseTime
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
int get_InternetGatewayDevice_IPPingDiagnostics_MaximumResponseTime(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_IPPingDiagnostics_MaximumResponseTime: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}





/****************************************************************************************************************************************************/
// IPPing Profile. Setters:
/****************************************************************************************************************************************************/
/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState
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
int set_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	ret = setIPPingDiagnostics(name, type,value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState: setIPPingDiagnostics() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.Interface
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
int set_InternetGatewayDevice_IPPingDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_Interface: storeParamValue() error1\n");
		)
		return ret;
	}

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_Interface: storeParamValue() error2\n");
		)
		return ret;
	}

	//TODO: If the test is in progress - test being terminated !!!!!


	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.Host
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
int set_InternetGatewayDevice_IPPingDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_Host: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_Host: storeParamValue() error2\n");
		)
		return ret;
	}

	//TODO: If the test is in progress - test being terminated !!!!!

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.NumberOfRepetitions
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
int set_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions: storeParamValue() error2\n");
		)
		return ret;
	}

	//TODO: If the test is in progress - test being terminated !!!!!

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.Timeout
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
int set_InternetGatewayDevice_IPPingDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_Timeout: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_Timeout: storeParamValue() error2\n");
		)
		return ret;
	}

	//TODO: If the test is in progress - test being terminated !!!!!

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.DataBlockSize
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
int set_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize: storeParamValue() error2\n");
		)
		return ret;
	}

	//TODO: If the test is in progress - test being terminated !!!!!

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.IPPingDiagnostics.DSCP
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
int set_InternetGatewayDevice_IPPingDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState = "None"
	ParameterValue value_None;
	value_None.in_cval = "None";
	ret = storeParamValue("InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState", StringType, &value_None);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_DSCP: storeParamValue() error1\n");
		)
		return ret;
	}
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_DSCP: storeParamValue() error2\n");
		)
		return ret;
	}

	//TODO: If the test is in progress - test being terminated !!!!!

	return OK;
}
