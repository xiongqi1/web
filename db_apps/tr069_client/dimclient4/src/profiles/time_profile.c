/***************************************************************************
 *    Copyright (C) 2004-2011 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/
/*
 * time_profile.c
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
#include <sys/time.h>

#include "globals.h"
#include "../paramaccess.h"
#include "parameterStore.h"
#include "utils.h"
#include "parameter.h"
#include "ethParameter.h"
#include "diagParameter.h"
#include "voipParameter.h"
#include "ftp_var.h"

#define _PATH_ETC_LOCALTIME "/etc/localtime"
#define	 InternetGatewayDevice_Time_Status	"InternetGatewayDevice.Time.Status"

#define	 Time_Status_Disabled "Disabled"
#define	 Time_Status_Unsynchronized "Unsynchronized"
#define	 Time_Status_Synchronized "Synchronized"
#define	 Time_Status_Error_FailedToSynchronize "Error_FailedToSynchronize"
#define	 Time_Status_Error "Error (OPTIONAL)"



static char strTimezone[10] = {'\0'};


// Time Profile. Getters:

/**
 * Parameter Name: InternetGatewayDevice.Time.Enable
 * Type ID: 18
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18000
 * Set Idx: 18000
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_Enable(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_Enable: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}


/**
 * Parameter Name: InternetGatewayDevice.Time.Status
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18001
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_Status(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_Status: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer1
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18002
 * Set Idx: 18001
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_NTPServer1(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_NTPServer1: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer2
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18003
 * Set Idx: 18002
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_NTPServer2(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_NTPServer2: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer3
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18004
 * Set Idx: 18003
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_NTPServer3(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_NTPServer3: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer4
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18005
 * Set Idx: 18004
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_NTPServer4(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_NTPServer4: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer5
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18006
 * Set Idx: 18005
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_NTPServer5(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_NTPServer5: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.CurrentLocalTime
 * Type ID: 11
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18007
 * Set Idx: -1
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_CurrentLocalTime(const char *name, ParameterType type, ParameterValue *value)
{
	ParameterValue temp_value;
	time_t now;
	int ret;

	time(&now);

	if (now == -1)
	{
		temp_value.in_cval = Time_Status_Disabled;
	}
	else
	{
		temp_value.in_cval = Time_Status_Synchronized;
	}

	ret = storeParamValue(InternetGatewayDevice_Time_Status, StringType, &temp_value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_CurrentLocalTime: storeParamValue(InternetGatewayDevice_Time_Status) error\n");
		)
		return ret;
	}


	value->out_timet = now;
	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.LocalTimeZone
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18008
 * Set Idx: 18006
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_LocalTimeZone(const char *name, ParameterType type, ParameterValue *value)
{
	int hoursTimezone = 0;
	int minutesTimezone = 0;
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);

	minutesTimezone = abs(tz.tz_minuteswest) % 60;
	hoursTimezone = (abs(tz.tz_minuteswest) - minutesTimezone) / 60;

	if (tz.tz_minuteswest > 0)
	{
		sprintf(strTimezone, "+%02d:%02d", hoursTimezone, minutesTimezone);
	}
	else
	{
		sprintf(strTimezone, "-%02d:%02d", hoursTimezone, minutesTimezone);
	}

	value->out_cval = strTimezone;



	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.LocalTimeZoneName
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18009
 * Set Idx: 18007
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_LocalTimeZoneName(const char *name, ParameterType type, ParameterValue *value)
{

	int ret;
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "IntelGet: %s\n", name);
	)
	FILE * fp = popen("tail -n 1 /etc/localtime", "r");
	if(!fp)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_LocalTimeZoneName() error1\n");
		)
		return -1;
	}
	if (!fgets(line, sizeof(line), fp))
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_LocalTimeZoneName() error2\n");
		)
		return -1;
	}

	pclose(fp);

	value->out_cval = line;

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.DaylightSavingsUsed
 * Type ID: 18
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18010
 * Set Idx: 18008
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_DaylightSavingsUsed(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_DaylightSavingsUsed: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.DaylightSavingsStart
 * Type ID: 11
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18011
 * Set Idx: 18009
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_DaylightSavingsStart(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_DaylightSavingsStart: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.DaylightSavingsEnd
 * Type ID: 11
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18012
 * Set Idx: 18010
 * Access List:
 * Default value:
 */
int get_InternetGatewayDevice_Time_DaylightSavingsEnd(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Retrieving value
	ret = retrieveParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_DaylightSavingsEnd: retrieveParamValue() error\n");
		)
		return ret;
	}

	return OK;
}







/****************************************************************************************************************************************************/
// Time Profile. Setters:
/****************************************************************************************************************************************************/
/**
 * Parameter Name: InternetGatewayDevice.Time.Enable
 * Type ID: 18
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18000
 * Set Idx: 18000
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_Enable(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_Enable: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer1
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18002
 * Set Idx: 18001
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_NTPServer1(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_NTPServer1: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}


/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer2
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18003
 * Set Idx: 18002
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_NTPServer2(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_NTPServer2: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer3
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18004
 * Set Idx: 18003
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_NTPServer3(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_NTPServer3: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}


/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer4
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18005
 * Set Idx: 18004
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_NTPServer4(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_NTPServer4: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.NTPServer5
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18006
 * Set Idx: 18005
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_NTPServer5(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_NTPServer5: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.LocalTimeZone
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18008
 * Set Idx: 18006
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_LocalTimeZone(const char *name, ParameterType type, ParameterValue *value)
{

	int ret;
	int hoursTimezone = 0;
	int minutesTimezone = 0;
	int newTimeZoneMinutes = 0;
	struct timeval tv;
	struct timezone tz;


	gettimeofday(&tv, &tz);


	sscanf(value->in_cval, "%d:%d", &hoursTimezone, &minutesTimezone);

	newTimeZoneMinutes = hoursTimezone * 60 + minutesTimezone;
	if (strstr(value->in_cval, "-"))
	{
		newTimeZoneMinutes *= -1;
	}

	// change time according new timezone
	if (newTimeZoneMinutes > tz.tz_minuteswest)
	{
		tv.tv_sec += abs(newTimeZoneMinutes - tz.tz_minuteswest) * 60;
	}
	else
	{
		tv.tv_sec -= abs(tz.tz_minuteswest - newTimeZoneMinutes) * 60;
	}


	tz.tz_minuteswest = newTimeZoneMinutes;
	tz.tz_dsttime=1;

	int result;
	result = settimeofday(&tv, &tz);


	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_LocalTimeZone: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.LocalTimeZoneName
 * Type ID: 6
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18009
 * Set Idx: 18007
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_LocalTimeZoneName(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	char lastLine[100] = "";
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "IntelGet: %s\n", name);
	)

	// getting last line
	char command[100] = "";
	FILE * fp = popen("tail -n 1 /etc/localtime", "r");
	if(!fp)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_LocalTimeZoneName() error1\n");
		)
		return OK;
	}
	if (!fgets(line, sizeof(line), fp))
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "get_InternetGatewayDevice_Time_LocalTimeZoneName() error2\n");
		)
		return -1;
	}

	pclose(fp);
	strcat(lastLine, line);


	FILE * temp = fopen("/etc/localtime", "r");
	if(!fp)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_LocalTimeZoneName() error1\n");
		)
		return -1;
	}
	while (fgets(line, sizeof(line), fp)) {
		if (!strstr(lastLine, line)) {

		}
	}


	fclose(fp);

	value->out_cval = line;

	return OK;


}

/**
 * Parameter Name: InternetGatewayDevice.Time.DaylightSavingsUsed
 * Type ID: 18
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 2
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18010
 * Set Idx: 18008
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_DaylightSavingsUsed(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_DaylightSavingsUsed: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.DaylightSavingsStart
 * Type ID: 11
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18011
 * Set Idx: 18009
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_DaylightSavingsStart(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;

	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_DaylightSavingsStart: storeParamValue() error2\n");
		)
		return ret;
	}

	return OK;
}

/**
 * Parameter Name: InternetGatewayDevice.Time.DaylightSavingsEnd
 * Type ID: 11
 * Instance: 0
 * Notification: 0
 * Max Notitfication: 0
 * Reboot: 1
 * Init Idx: 1
 * Get Idx: 18012
 * Set Idx: 18010
 * Access List:
 * Default value:
 */
int set_InternetGatewayDevice_Time_DaylightSavingsEnd(const char *name, ParameterType type, ParameterValue *value)
{
	int ret;
	//Storing value
	ret = storeParamValue(name, type, value);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_Time_DaylightSavingsEnd: storeParamValue() error2\n");
		)
		return ret;
	}


	return OK;
}

