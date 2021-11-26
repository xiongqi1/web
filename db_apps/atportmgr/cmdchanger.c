#include "cmdchanger.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <syslog.h>

#include "utils.h"


///////////////////////////////////////////////////////////////////////////////
void cmdchanger_destroy(cmdchanger* pC)
{
	if (!pC)
		return;

	if (pC->pAtGetBuf)
		free(pC->pAtGetBuf);

	if (pC->pAtGetAnsBuf)
		free(pC->pAtGetAnsBuf);

	if (pC->pAtGetAnsPosBuf)
		free(pC->pAtGetAnsPosBuf);

	free(pC);
}

///////////////////////////////////////////////////////////////////////////////
cmdchanger* cmdchanger_create(const char* szPrimModuleName, const char* szSecModuleName, int cbMaxAns)
{
	cmdchanger* pC;

	// allocate object
	pC = malloc(sizeof(*pC));
	if (!pC)
	{
		syslog(LOG_ERR, "failed to allocate memory for a commahd changer object");
		goto error;
	}
	memset(pC, 0, sizeof(*pC));

	// looking for matching module information
	const moduleinfo* pModuleInfo = moduleInfoTbl;
	while (pModuleInfo->szModuleName)
	{
		// get primary module info
		if (!strcmp(pModuleInfo->szModuleName, szPrimModuleName))
		{
			pC->moduleType = pModuleInfo->moduleType;
			pC->pAtTransTbl = pModuleInfo->pAtTransTbl;
		}
		// get secondary module info.
		else if (szSecModuleName && !strcmp(pModuleInfo->szModuleName, szSecModuleName))
		{
			pC->pDefTransTbl = pModuleInfo->pAtTransTbl;
		}

		pModuleInfo++;
	}

	// error if not found
	if (!pC->pAtTransTbl)
	{
		syslog(LOG_ERR, "unable to handle primary module (module name=%s)", szPrimModuleName);
		goto error;
	}

	// error if not found
	if (szSecModuleName && !pC->pDefTransTbl)
	{
		syslog(LOG_ERR, "unable to handle secondary module (module name=%s)", szSecModuleName);
		goto error;
	}


	// max answer
	pC->cbMaxAns = cbMaxAns;

	// allocate buffer 1
	pC->pAtGetBuf = malloc(pC->cbMaxAns);
	if (!pC->pAtGetBuf)
		goto error;

	// allocate buffer 2
	pC->pAtGetAnsBuf = malloc(pC->cbMaxAns);
	if (!pC->pAtGetAnsBuf)
		goto error;

	// allocate buffer 3
	pC->pAtGetAnsPosBuf = malloc(pC->cbMaxAns);
	if (!pC->pAtGetAnsPosBuf)
		goto error;


	return pC;

error:
	cmdchanger_destroy(pC);
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
static const attranstbl* getAtCmdTableFromInitTbl(const attranstbl* pInitTbl, const char* szDbVarName)
{
	const attranstbl* pAtCmds = pInitTbl;

	while (pAtCmds->szDbVarName)
	{
		if (!strcmp(pAtCmds->szDbVarName, szDbVarName))
			return pAtCmds;

		pAtCmds++;
	}

	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
static const attranstbl* getAtCmdTable(cmdchanger* pC, const char* szDbVarName)
{
	// try its own at command table first
	const attranstbl* pAtCmd = getAtCmdTableFromInitTbl(pC->pAtTransTbl, szDbVarName);
	if (pAtCmd)
		return pAtCmd;

	// try generic table if does not exist
	if (pC->pDefTransTbl)
		return getAtCmdTableFromInitTbl(pC->pDefTransTbl, szDbVarName);

	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
static int getFmtStr(const char* szTokenFmt, const char* szStr, char* pOut, int cbOut)
{

	// get headpart of regtokenex
	char* pH = strstr(szTokenFmt, CMDCHANGER_BRACKET_BEGIN);
	if (!pH)
		goto error;

	// get tailpart of regtokenex
	char* pT = strstr(szTokenFmt, CMDCHANGER_BRACKET_END);
	if (!pT)
		goto error;
	pT += strlen(CMDCHANGER_BRACKET_END);

	// get length
	int cbH = pH - szTokenFmt;
	int cbT = szTokenFmt + strlen(szTokenFmt) - pT;

	// check the length
	int cbTotal = cbH + cbT + strlen(szStr);
	int cbNeed = cbTotal + 1;
	if (cbOut < cbNeed)
	{
		syslog(LOG_ERR, "output buffer too small (token format=%s, need size=%d)", szTokenFmt, cbNeed);
		goto error;
	}

	*pOut = 0;

	strncpy(pOut, szTokenFmt, cbH);
	strcat(pOut, szStr);
	strncat(pOut, pT, cbT);

	pOut[cbTotal] = 0;
	return cbNeed;

error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
const char* cmdchanger_getAtGetAns(cmdchanger* pC, const char* szDbVarName, int iIdx, const char* szAtResult)
{
	int stat;

	// get atcmd
	const attranstbl* pAtCmd = getAtCmdTable(pC, szDbVarName);
	if (!pAtCmd)
	{
		syslog(LOG_ERR, "database variable doesn't exist in at command table (dbvarname=%s)", szDbVarName);
		goto error;
	}

	//szAtResult = "\n+COPS: (2,\"CA.IPW.com\", \"CA\",\"0023\"), (2,\"NJ.IPW.com\", \"NJ\",\"0064\")\n";

	// do get regtokenex
	stat = __getRegTokenEx(pAtCmd->szGetAnsRegTokenEx, szAtResult, pC->pAtGetAnsBuf, pC->cbMaxAns);
	if (stat < 0)
	{
		syslog(LOG_INFO, "regular expression failed on answer (RegEx=%s,AtResult=%s)", pAtCmd->szGetAnsRegTokenEx, __convCtrlToCStyle(szAtResult));
		goto error;
	}

	char* pOut = pC->pAtGetAnsBuf;
	int cbOutLen = strlen(pOut);

	// do post process
	attranstbl_callback* lpfnGetPostProcess = pAtCmd->lpfnGetPostProcess;
	if (lpfnGetPostProcess)
	{
		stat = lpfnGetPostProcess(szDbVarName, iIdx, pOut, pC->pAtGetAnsPosBuf, pC->cbMaxAns);
		if (stat < 0)
		{
			syslog(LOG_ERR, "post process returns error (dbvarname=%s,ans=%s)", szDbVarName, pOut);
			goto error;
		}

		strcpy(pOut, pC->pAtGetAnsPosBuf);
	}

	// do post process - format
	if (cbOutLen && pAtCmd->szGetFormat)
	{
		stat = getFmtStr(pAtCmd->szGetFormat, pOut, pC->pAtGetAnsPosBuf, pC->cbMaxAns);
		if (stat < 0)
		{
			syslog(LOG_ERR, "format error found (dbvarname=%s,fmt=%s)", szDbVarName, pAtCmd->szGetFormat);
			goto error;
		}

		strcpy(pOut, pC->pAtGetAnsPosBuf);
	}


	return pOut;

error:
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
const char* cmdchanger_getAtGetQuery(cmdchanger* pC, const char* szDbVarName, int iIdx)
{
	const attranstbl* pAtCmd = getAtCmdTable(pC, szDbVarName);

	if (!pAtCmd)
		goto error;

	snprintf(pC->pAtGetBuf, pC->cbMaxAns, pAtCmd->szGetAt, iIdx);
	strcat(pC->pAtGetBuf, "\n");

	return pC->pAtGetBuf;

error:
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
const char* cmdchanger_getNextSupportedDbVarName(cmdchanger* pC)
{
	if (!pC->pAtTransTbl[pC->iSearchIdx].szDbVarName)
		return NULL;

	return pC->pAtTransTbl[pC->iSearchIdx++].szDbVarName;
}
///////////////////////////////////////////////////////////////////////////////
const char* cmdchanger_getFirstSupportedDbVarName(cmdchanger* pC)
{
	pC->iSearchIdx = 0;

	return cmdchanger_getNextSupportedDbVarName(pC);
}

///////////////////////////////////////////////////////////////////////////////
const char* cmdchanger_getNextGenericSupportedDbVarName(cmdchanger* pC)
{
	if (!pC->pDefTransTbl)
		return NULL;

	if (!pC->pDefTransTbl[pC->iGenericSearchIdx].szDbVarName)
		return NULL;

	return pC->pDefTransTbl[pC->iGenericSearchIdx++].szDbVarName;
}
///////////////////////////////////////////////////////////////////////////////
const char* cmdchanger_getFirstGenericSupportedDbVarName(cmdchanger* pC)
{
	pC->iGenericSearchIdx = 0;
	return cmdchanger_getNextGenericSupportedDbVarName(pC);
}
