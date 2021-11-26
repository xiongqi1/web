#include "templatemgr.h"

#include <errno.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "base.h"
#include "rdb_ops.h"

///////////////////////////////////////////////////////////////////////////////
void templatemgr_destroy(templatemgr* pT)
{
	if (!pT)
		return;

	objectlist_destroy(pT->pObjListVariables);

	free(pT);
}

typedef enum
{
	variabletype_none,
	variabletype_replace,
	variabletype_subscribe
} variabletype;

///////////////////////////////////////////////////////////////////////////////
static int __getVariable(const char* pParserPtr, char* pVariable, variabletype* pType, int* pStartIdx, const char** ppNext)
{
	variabletype variableType = variabletype_none;

	int fBracket = 0;
	int fPrevBracket = 0;
	int cbBracket = strlen(TEMPLATEMGR_DELIMITER_REPLACE_OPEN);
	int iStartIdx = -1;

	const char* pPtr = pParserPtr;

	while (*pPtr)
	{
		if (!fBracket)
		{
			if (!strncmp(pPtr, TEMPLATEMGR_DELIMITER_REPLACE_OPEN, cbBracket))
				variableType = variabletype_replace;
			else if (!strncmp(pPtr, TEMPLATEMGR_DELIMITER_SUBSCRIBE_OPEN, cbBracket))
				variableType = variabletype_subscribe;

			fBracket = variableType != variabletype_none;
		}
		else
		{
			if (!strncmp(pPtr, TEMPLATEMGR_DELIMITER_CLOSE, cbBracket))
				fBracket = 0;
		}

		int fIn = !fPrevBracket && fBracket;
		int fOut = fPrevBracket && !fBracket;
		fPrevBracket = fBracket;

		if (fIn)
			iStartIdx = pPtr - pParserPtr;

		if (fBracket && !(fOut || fIn))
			*pVariable++ = *pPtr;

		if (fOut || fIn)
			pPtr += cbBracket;
		else
			pPtr++;

		if (fOut)
			break;
	}

	*pType = variableType;
	*pVariable = 0;
	*pStartIdx = iStartIdx;
	*ppNext = pPtr;

	if (fBracket)
		return -1;

	if (variableType == variabletype_none)
		return 0;

	return 1;
}

///////////////////////////////////////////////////////////////////////////////
static int templatemgr_parseTemplate(templatemgr* pT, FILE* streamOut)
{
	if (!streamOut)
		objectlist_clear(pT->pObjListVariables);

	FILE* streamIn;

	// open input
	streamIn = fopen(pT->achTemplate, "rt");
	if (!streamIn)
	{
		syslog(LOG_ERR, "failed to open %s for reading", pT->achTemplate);
		return -1;
	}

	int iLine = 0;
	int cVariables = 0;

	char achLineBuf[1024];
	while (fgets(achLineBuf, sizeof(achLineBuf), streamIn) != NULL)
	{
		iLine++;

		const char* pParsePtr = NULL;
		char achVariable[MAX_NAME_LENGTH];
		variabletype variableType;
		int iStartIdx;
		const char* pNextParsePtr = achLineBuf;

		while (1)
		{
			pParsePtr = pNextParsePtr;
			int stat = __getVariable(pParsePtr, achVariable, &variableType, &iStartIdx, &pNextParsePtr);

			if (stat == 0)
			{
				break;
			}
			else if (stat < 0)
			{
				syslog(LOG_ERR, "bracket error found at line %d in file %s", iLine, pT->achTemplate);
				break;
			}

			int cbVariableLen = strlen(achVariable);

			// read-only mode
			if (!streamOut)
			{
				if (objectlist_lookUp(pT->pObjListVariables, achVariable, objectlist_compareString))
				{
					syslog(LOG_DEBUG, "duplicated %s variable skipped in line %d", achVariable, iLine);
				}
				else
				{
					objectlistelement* pE = objectlist_addDummyObj(pT->pObjListVariables, cbVariableLen + 1);
					memcpy(pE->pM, achVariable, cbVariableLen + 1);

					syslog(LOG_DEBUG, "%s variable (type:%d) added from line %d in file %s", achVariable, variableType, iLine, pT->achTemplate);
					cVariables++;
				}

				continue;
			}

			// build mode - replace or remove
			char achValue[sizeof(achLineBuf)];
			if (fwrite(pParsePtr, sizeof(char), iStartIdx, streamOut) != iStartIdx)
			{
				syslog(LOG_CRIT, "disk write failure occured while writing a configuration file");
				continue;
			}

			if (rdb_peek_single(achVariable, achValue, sizeof(achValue)) < 0)
			{
				syslog(LOG_CRIT, "database error occured while putting variable %s at line %d in template %s", achVariable, iLine, pT->achTemplate);
				continue;
			}

			if (variableType == variabletype_subscribe)
			{
				// remove the whole line
				if (*pNextParsePtr == '\n' && iStartIdx == 0)
				{
					pParsePtr = NULL;
					break;
				}
			}
			else
			{
				fputs(achValue, streamOut);
				syslog(LOG_DEBUG, "variable %s(%s) stored at line %d in template %s", achVariable, achValue, iLine, pT->achTemplate);
			}

		}

		if (streamOut && pParsePtr)
			fputs(pParsePtr, streamOut);
	}

	syslog(LOG_DEBUG, "%d line(s) of %s file parsed for subscribing %d variable(s) ", iLine, pT->achTemplate, cVariables);

	if (streamIn)
		fclose(streamIn);

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int templatemgr_buildVaribles(templatemgr* pT)
{
	int dbStat;

	dbStat = rdb_database_lock(0);
	if (dbStat < 0)
		syslog(LOG_CRIT, "database locking failure!! Theoretically, this should not happen. (dbStat=%d)", dbStat);

	clock_t tickS = __getTickCount();

	int stat = templatemgr_parseTemplate(pT, NULL);

	clock_t tickE = __getTickCount();

	syslog(LOG_DEBUG, "total time spending for database locking : %ld msec", (tickE - tickS)*1000 / __getTicksPerSecond());

	dbStat = rdb_database_unlock();
	if (dbStat < 0)
		syslog(LOG_CRIT, "database unlocking failure!! Theoretically, this should not happen. (dbStat=%d)", dbStat);


	return stat;
}

///////////////////////////////////////////////////////////////////////////////
static int __waitUntilTerminate(pid_t pID, int* pStat, int nTimeOutSec)
{
	// wait until the child terminates
	clock_t tickPerSec = __getTicksPerSecond();
	clock_t tickS = __getTickCount();

	while ((__getTickCount() - tickS) / tickPerSec <= nTimeOutSec)
	{
	    // note this does NOT block when called with WHOHANG argument
		if (waitpid(pID, pStat, WNOHANG) > 0)
			break;

		//
		// Avoid busy waiting which was previously causing for template manager to take up to 30-40% of CPU in some instances
		// Sleep value of 100 ms which seems like a good number, meaning it will not delay the execution of the next template by more
		// than 100 ms when the previous child process exits.
		//
		usleep(100000);
	}

	return waitpid(pID, pStat, WNOHANG);
}
///////////////////////////////////////////////////////////////////////////////
static int __executeSync(const char* szCmdLine, int nTimeOutSec)
{
	int succ = -1;

	pid_t pID = fork();

	// if child
	if (!pID)
	{
		char achPath[1024];
		char* pPath;

		int  iArgVal = 0;
		char* aArgVal[256];

		// copy
		strcpy(achPath, szCmdLine);

		pPath = achPath;

		// splite - chopping and getting each pointer
		while (iArgVal < sizeof(aArgVal) - 1)
		{
			aArgVal[iArgVal++] = pPath;

			// until a space or null
			while (*pPath && (*pPath != ' '))
				pPath++;

			if (!*pPath)
				break;

			*pPath++ = 0;
		}

		// null for over-sized args
		aArgVal[iArgVal] = 0;

		// launch
		execv(aArgVal[0], aArgVal);
	}
	// if parent
	else if (pID > 0)
	{
		int stat;

		if ((__waitUntilTerminate(pID, &stat, nTimeOutSec)) == 0)
		{
			syslog(LOG_ERR, "script timeout occured (%d seconds). sending SIGINT (command=%s)", TEMPLATEMGR_SCRIPT_TIMEOUT, szCmdLine);

			kill(pID, SIGKILL);
			waitpid(pID, &stat, 0);
		}

		if (WIFEXITED(stat))
			succ = WEXITSTATUS(stat);
	}

	return succ;
}

///////////////////////////////////////////////////////////////////////////////
int templatemgr_execMgr(templatemgr* pT, int fInit)
{
	char achCmdLine[1024];

	int fSelfExec = !strlen(pT->achScriptMgr);

	if (fSelfExec)
		sprintf(achCmdLine, "%s %s", pT->achOutput, pT->achTemplate);
	else
		sprintf(achCmdLine, "%s %s", pT->achScriptMgr, pT->achOutput);

	// if it is init
	if(fInit) {
		strcat(achCmdLine," init");
	}

	syslog(LOG_DEBUG, "launching script manager %s", achCmdLine);

	return __executeSync(achCmdLine, TEMPLATEMGR_SCRIPT_TIMEOUT);
}
///////////////////////////////////////////////////////////////////////////////
int tempaltemgr_unlinkConf(templatemgr* pT)
{
	return unlink(pT->achOutput);
}
///////////////////////////////////////////////////////////////////////////////
int templatemgr_buildConf(templatemgr* pT)
{
	int stat;
	FILE* streamOut = NULL;

	// open output
	streamOut = fopen(pT->achOutput, "wt");
	if (!streamOut)
	{
		syslog(LOG_ERR, "failed to open %s for writing", pT->achOutput);
		return -1;
	}

	int fSelfExec = !strlen(pT->achScriptMgr);

	if (fSelfExec)
	{
		int nMode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
		int fno = fileno(streamOut);
		stat = fchmod(fno, nMode);
		if (stat < 0)
			syslog(LOG_ERR, "failed to change %s mode to executable permission", pT->achOutput);
	}

	stat = templatemgr_parseTemplate(pT, streamOut);

	fclose(streamOut);


	return stat;
}
///////////////////////////////////////////////////////////////////////////////
int templatemgr_isIncluded(templatemgr* pT, const char* szVariable)
{
	if (objectlist_lookUp(pT->pObjListVariables, szVariable, objectlist_compareString) == NULL)
		return -1;

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
templatemgr* templatemgr_create(const char* szTemplate, const char* szMgr)
{
	char* pDupTemplate = NULL;

	templatemgr* pT;

	pT = malloc(sizeof(*pT));
	if (!pT)
	{
		syslog(LOG_ERR, "template manager allocation failed");
		goto error;
	}

	// store template file name
	strcpy(pT->achTemplate, szTemplate);

	// get base name
	pDupTemplate = strdup(szTemplate);
	if (!pDupTemplate)
	{
		syslog(LOG_ERR, "failed to duplicate a string %s", szTemplate);
		goto error;
	}
	char* szBase = basename(pDupTemplate);

	// gen. a temporary file
	sprintf(pT->achOutput, "%s/%s-XXXXXX", TEMPLATEMGR_DIR_OUTPUT, szBase);
	int tmpFile = mkstemp(pT->achOutput);
	if (tmpFile < 0)
	{
		syslog(LOG_ERR, "failed to create a temporary file for template output (name=%s)", pT->achOutput);
		goto error;
	}

	free(pDupTemplate);
	close(tmpFile);

	// store script mgr
	pT->achScriptMgr[0] = 0;
	if (szMgr)
		strcpy(pT->achScriptMgr, szMgr);

	// create objectlist for variables
	pT->pObjListVariables = objectlist_create();

	return pT;

error:
	if (pDupTemplate)
		free(pDupTemplate);

	templatemgr_destroy(pT);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
