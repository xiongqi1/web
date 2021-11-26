/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"
#include "utils.h"
#include "parameter.h"
#include "voipParameter.h"

#define NUMBER_OF_LINES 	4
#define NUMBER_OF_PROFILES	3

/* This structure simulates ( a rough simulation ) of an VoIP Device */

typedef struct stats
{
	unsigned int packetsSent;
	unsigned int packetsReceived;
	unsigned int bytesReceived;
	unsigned int packetsLost;
	unsigned int overruns;
	unsigned int underruns;
	unsigned int incomingCallsReceived;
} Stats;

typedef struct line
{
	int number;
	char *enable;
	char *status;
	char *directoryNumber;
	volatile Stats stats;
} Line;

typedef struct profile
{
	int number;
	Line lines[4];
} Profile;

struct device
{
	int profileEntries;
	Profile profiles[NUMBER_OF_PROFILES];
} VoiceDevice =
{
	2,
	{
		{
			1,
			{
				{
				1, NULL, NULL},
				{
				2, NULL, NULL},
				{
				3, NULL, NULL},
				{
			4, NULL, NULL}}
		},
		{
			2,
			{
				{
				1, NULL, NULL},
				{
				2, NULL, NULL},
				{
				3, NULL, NULL},
				{
			4, NULL, NULL}}
		},
		{
			3,
			{
				{
				1, NULL, NULL, 0},
				{
				2, NULL, NULL, 0},
				{
				3, NULL, NULL, 0},
				{
			4, NULL, NULL, 0}}
	}}
};


static void resetStats (Line *);
static Line *getLine (const char *param);

 /** Initialize VoIP Parameters and devices
 */
int
initVoIP (void)
{
	char buf[20];
	int vpIdx, lIdx;

	for (vpIdx = 0; vpIdx < NUMBER_OF_PROFILES; vpIdx++)
	{
		Profile *profile = &VoiceDevice.profiles[vpIdx];
		for (lIdx = 0; lIdx < NUMBER_OF_LINES; lIdx++)
		{
			Line *line = &profile->lines[lIdx];
			line->enable = strnDup (line->enable, "Disabled", 8);
			sprintf (buf, "Line_%02d", (lIdx + 1));
			line->directoryNumber =
				strnDup (line->directoryNumber, buf,
					 strlen (buf));
			resetStats (line);
		}
	}

	return OK;
}

/** Checks the number of allowed LineObjects against the actual number of LineObjects.
 * The actual number is given as a integer in value. 
 * The number of allowed Instances is stored in parameter: 
 *	 	InternetGateWayDevice.VoiceDevice.1.Capabilities.MaxLineCount
 */
int
initVdMaxLine (const char *paramPath, const ParameterType type, ParameterValue *value)
{
	register int ret = OK;
	int maxCount = 1;
	int newCount = *(int *) value;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_VOIP, "VoIP Parameter: %s instance: %d\n", paramPath, *(int *) value);
	)

	ret = getParameter
		("InternetGatewayDevice.VoiceDevice.1.Capabilities.MaxLineCount",
		 &maxCount);
	if (ret == OK)
	{
		if (newCount <= maxCount)
			return ret;
		else
			return ERR_INVALID_PARAMETER_NAME;
	}
	else
		return ret;
	return OK;
}

/** Checks the number of allowed ProfileObjects against the actual number of ProfileObjects.
 * The actual number is given as a integer in value. 
 * The number of allowed Instances is stored in parameter: 
 *	 	InternetGateWayDevice.VoiceDevice.1.Capabilities.MaxProfileCount
 */
int
initVdMaxProfile (const char *paramPath, ParameterType type, ParameterValue *value)
{
	int ret = OK;
	int maxCount = 1;
	int newCount = *(int *) value;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_VOIP, "VoIP Parameter: %s instance: %d\n", paramPath, *(int *) value);
	)

	ret = getParameter("InternetGatewayDevice.VoiceDevice.1.Capabilities.MaxProfileCount",
		 &maxCount);
	if (ret == OK)
	{
		if (newCount <= maxCount)
		{
			ret = setParameter
				("InternetGatewayDevice.VoiceDevice.1.VoiceProfileNumberOfEntries",
				 &newCount);
			return ret;
		}
		else
			return ERR_INVALID_PARAMETER_NAME;
	}
	else
		return ret;
}

/** set the line enable parameter value 
	Shows how to access the instance numbers of the parameter objects VoiceProfile and Line
	..Line.x.Enable 
*/
int
setVdLineEnable (const char *paramPath, const ParameterType type, ParameterValue *value)
{
	int ret = OK;
	Line *line = NULL;
	char *newValue = *(char **) value;
	char **oldValue = NULL;

	line = getLine (paramPath);
	if (!line)
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		oldValue = &line->enable;
		if (*oldValue != NULL)
			efree (*oldValue);
		*oldValue = strnDup (*oldValue, (char *) newValue, strlen ((char *) newValue));
		// !!! Attention, the following code will produce trouble if newValue.length > oldValue.length !!!
		//strcpy( oldValue, newValue );
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_VOIP, "SetEnable: Line[%d] new: %s\n", line->number, line->enable);
		)
		line->stats.packetsSent += strlen (newValue);
		return ret;
	}
}

/** get the line enable parameter value 
	Shows how to access the instance numbers of the parameter objects VoiceProfile and Line
	..Line.x.Enable 
*/
int
getVdLineEnable (const char *paramPath, const ParameterType type, ParameterValue *value)
{
	int ret = OK;
	Line *line = NULL;


	line = getLine (paramPath);
	if (!line)
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		*(char **) value = line->enable;

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_VOIP, "VoIP GetEnable: Line[%d] Value: %s\n", line->number, line->enable);
		)

		line->stats.packetsSent += strlen ((char *)value);
		return ret;
	}
}

/** set the line enable parameter value 
	Shows how to access the instance numbers of the parameter objects VoiceProfile and Line
	..Line.x.Enable 
*/
int
setVdLineDirectoryNumber (const char *paramPath, const ParameterType type, ParameterValue *value)
{
	int ret = OK;
	int vpIdx, lineIdx;
	Line *line = NULL;
	char *newValue = *(char **) value;
	char **oldValue = NULL;

	vpIdx = getIdxByName (paramPath, "VoiceProfile");
	lineIdx = getRevIdx (paramPath, "DirectoryNumber");
	if (vpIdx <= 0 || lineIdx <= 0 || vpIdx > NUMBER_OF_PROFILES
	    || lineIdx > NUMBER_OF_LINES)
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		line = (Line *) & VoiceDevice.profiles[(vpIdx - 1)].
			lines[(lineIdx - 1)];
		oldValue = &line->directoryNumber;
		if (*oldValue != NULL)
			efree (*oldValue);
		*oldValue =
			strnDup (*oldValue, (char *) newValue,
				 strlen ((char *) newValue));
		// !!! Attention, the following code will produce trouble if newValue.length > oldValue.length !!!
		//strcpy( oldValue, newValue );
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_VOIP, "SetDirectoryNumber: Line[%d] new: %s\n", line->number, line->directoryNumber);
		)

		line->stats.packetsSent = getTime ();
		return ret;
	}
}

/** get the line enable parameter value 
	Shows how to access the instance numbers of the parameter objects VoiceProfile and Line
	..Line.x.Enable 
*/
int
getVdLineDirectoryNumber (const char *paramPath, const ParameterType type, ParameterValue *value)
{
	int ret = OK;
	int vpIdx, lineIdx;
	Line *line = NULL;

	vpIdx = getIdxByName (paramPath, "VoiceProfile");
	lineIdx = getRevIdx (paramPath, "DirectoryNumber");

	if (vpIdx <= 0 || lineIdx <= 0 || vpIdx > NUMBER_OF_PROFILES || lineIdx > NUMBER_OF_LINES)
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		line = (Line *) & VoiceDevice.profiles[(vpIdx - 1)].lines[(lineIdx - 1)];
		value = (ParameterValue *)&line->directoryNumber;

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_VOIP, "GetDirectoryNumber: Line[%d] Value: %s\n", line->number, line->directoryNumber);
		)

		line->stats.packetsSent = getTime ();

		return ret;
	}
}


/** set the line enable parameter value 
	Shows how to access the instance numbers of the parameter objects VoiceProfile and Line
	..Line.x.Enable 
*/
int
getVdLinePacketsSent (const char *paramPath, const ParameterType type, ParameterValue *value)
{
	int ret = OK;
	Line *line = NULL;
//      char **newValue = *(char**)value;
//      char **oldValue = NULL;

	line = getLine (paramPath);
	if (!line)
	{
		return ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		*(unsigned int *) value = line->stats.packetsSent;

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_VOIP, "GetEnable: Line[%d] old: %u\n", line->number, line->stats.packetsSent);
		);

		line->stats.packetsSent = getTime ();
		return ret;
	}
}

/** Extracts the VoiceProfile and the Line index from paramPath and returns
 * the line object
*/
static Line *
getLine (const char *paramPath)
{
	int vpIdx;
	int lineIdx;

	vpIdx = getIdxByName (paramPath, "VoiceProfile");
	lineIdx = getIdxByName (paramPath, "Line");
	if (vpIdx <= 0 || lineIdx <= 0 || vpIdx > NUMBER_OF_PROFILES
	    || lineIdx > NUMBER_OF_LINES)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_VOIP, "Error: vpIdx %d  lineIdx %d\n", vpIdx, lineIdx);
		)

		return NULL;
	}
	else
	{
		return (Line *) & VoiceDevice.profiles[(vpIdx - 1)].
			lines[(lineIdx - 1)];
	}
}

static void
resetStats (Line * line)
{
	line->stats.packetsSent = 0;
	line->stats.packetsReceived = 0;
	line->stats.bytesReceived = 0;
	line->stats.packetsLost = 0;
	line->stats.overruns = 0;
	line->stats.underruns = 0;
	line->stats.incomingCallsReceived = 0;
}
