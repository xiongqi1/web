#include "atportmgr.h"

#define _GNU_SOURCE

// standard headers
#include <syslog.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include "usb.h"

// include nvram
#ifdef CONFIG_RALINK_RT3052
#include <nvram.h>
#endif

// library headers
#include "daemon.h"
#include "rdb_ops.h"

// own headers
#include "utils.h"
#include "lowserial.h"
#include "cmdchanger.h"
#include "funccaller.h"
#include "moduledatabase.h"
#include "atcmdqueue.h"
#include "detectinfo.h"
#include "dbenum.h"



////////////////////////////////////////////////////////////////////////////////
typedef int (dbcmdfunc)(const char* szValue);

struct dbcmdinfo
{
	const char* dbVarName;
	dbcmdfunc* lpfnHandler;
};


////////////////////////////////////////////////////////////////////////////////

int dbCmdSimCardPin(const char* szValue);


////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetAns_Pin(funccaller* pF, callback_entry* pEnt);
int funcCallerCallBackCmdChangerGetQuery_Pin(funccaller* pF, callback_entry* pEnt);

////////////////////////////////////////////////////////////////////////////////
static const struct dbcmdinfo dbCmdInfoTbl[] =
{
	{"sim.cmd.command",   dbCmdSimCardPin},
	{"module.lock",				NULL},
	{NULL, NULL}
};

////////////////////////////////////////////////////////////////////////////////

static lowserial* _pS = NULL;
static cmdchanger* _pC = NULL;
static funccaller* _pF = NULL;
static dbenum* _pE = NULL;

int fTermSigDetected = 0;


////////////////////////////////////////////////////////////////////////////////
struct
{
	int fDisableDaemon;
	int iInst;

	const char* szFullName;
	const char* szBaseName;

	const char* szAtPort;
	const char* szLastPort;
	const char* szModuleName;
	int fSharedPort;

	int nDbgLevel;
} _globalEnv;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int dbGetCmdIdx(const char* pCmdList[], const char* szCmd)
{
	int i = 0;

	while (*pCmdList)
	{
		if (!strcasecmp(*pCmdList, szCmd))
			return i;

		i++;
		pCmdList++;
	}

	return -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int dbGetUnknownCmdErr(const char* pCmdList[], char* pBuf)
{
	strcat(pBuf, "[error] unknown command - only ");

	char* pDst;
	pDst = pBuf + strlen(pBuf);

	int i = 0;

	while (*pCmdList)
	{
		if (i++)
			strcat(pDst, " ");

		strcat(pDst, *pCmdList++);
	}

	return i;
}
////////////////////////////////////////////////////////////////////////////////
const char* rdbGetPrefixDbVarName(const char* szDbVarName)
{
	static char achCookDbVarName[MAX_NAME_LENGTH];
	snprintf(achCookDbVarName, sizeof(achCookDbVarName), ATPORTMGR_DBPREFIX, _globalEnv.iInst, szDbVarName);

	return achCookDbVarName;
}
////////////////////////////////////////////////////////////////////////////////
int rdbSubscribe(const char* szDbVarName)
{
	const char* szPrefixDbVarName = rdbGetPrefixDbVarName(szDbVarName);
	return rdb_subscribe_variable(szPrefixDbVarName);
}
////////////////////////////////////////////////////////////////////////////////
int rdbGetSingle(const char* szDbVarName, char* szValue, int cbValue)
{
	const char* szPrefixDbVarName = rdbGetPrefixDbVarName(szDbVarName);
	return rdb_get_single(szPrefixDbVarName, szValue, cbValue);
}
////////////////////////////////////////////////////////////////////////////////
int rdbSetSingle(const char* szDbVarName, const char* szValue)
{
	const char* szPrefixDbVarName = rdbGetPrefixDbVarName(szDbVarName);

	int stat = rdb_set_single(szPrefixDbVarName, szValue);
	if (stat < 0)
	{
		stat = rdb_create_variable(szPrefixDbVarName, szValue, CREATE, DEFAULT_PERM, 0, 0);
		if (stat < 0)
			syslog(LOG_ERR, "failed to write to databaase - name=%s,value=%s,err=%s", szPrefixDbVarName, szValue, strerror(errno));
	}

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
int dbCmdSimCardPin(const char* szValue)
{
	const char* pszCmds[] = {"enable", "disable", "change", "verifypin", "verifypuk", "check", NULL};
	int iCmd = dbGetCmdIdx(pszCmds, szValue);

	switch (iCmd)
	{
			// enable and disable
		case 0:
		case 1:
		{
			//char achPin[256];
			//int cbPin = sizeof(achPin);
			//int fEn = iCmd == 0;

			//// get read pin
			//if (rdbGetSingle("sim.cmd.param.pin",achPin,sizeof(achPin)))
			//{
			//	rdbSetSingle("sim.cmd.status","[error] failed to get sim.cmd.param.pin");
			//	goto error;
			//}

			//atcmdEnChv1(fEn, achPin));

			rdbSetSingle("sim.cmd.status", "[error] not supported yet");
			break;
		}

		// check
		case 5:
		{
			//if(!cnsGetEnChv1())
			//{
			//	sprintf(achErr, "CnS communication failure");
			//	__goToError();
			//}

			rdbSetSingle("sim.cmd.status", "[error] not supported yet");
			goto error;
		}

		// verifypuk
		case 4:
		{
			//char achNewPin[CNSMGR_MAX_VALUE_LENGTH];
			//int cbNewPin = sizeof(achNewPin);

			//char achPuk[CNSMGR_MAX_VALUE_LENGTH];
			//int cbPuk = sizeof(achPuk);

			//// read newpin
			//if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 4, achNewPin, &cbNewPin))
			//{
			//	sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 4));
			//	__goToError();
			//}

			//// read puk
			//if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 5, achPuk, &cbPuk))
			//{
			//	sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 5));
			//	__goToError();
			//}

			//__goToErrorIfFalse(cnsVeriChvCode(0x0003, achNewPin, achPuk));

			rdbSetSingle("sim.cmd.status", "[error] not supported yet");
			goto error;
		}

		// verify pin
		case 3:
		{
			//char achPin[CNSMGR_MAX_VALUE_LENGTH];
			//int cbPin = sizeof(achPin);

			//// get read pin
			//if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 3, achPin, &cbPin))
			//{
			//	sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 3));
			//	__goToError();
			//}

			//__goToErrorIfFalse(cnsVeriChvCode(0x0001, achPin, NULL));
			//break;

			rdbSetSingle("sim.cmd.status", "[error] not supported yet");
			goto error;
		}

		// change
		case 2:
		{
			//char achOld[CNSMGR_MAX_VALUE_LENGTH];
			//int cbOld = sizeof(achOld);
			//char achNew[CNSMGR_MAX_VALUE_LENGTH];
			//int cbNew = sizeof(achNew);

			//// get read pin
			//if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 3, achOld, &cbOld))
			//{
			//	sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 3));
			//	__goToError();
			//}
			//// get read new pin
			//if (!dbGetStr(CNSMGR_DB_KEY_SIMCARDPIN, 4, achNew, &cbNew))
			//{
			//	sprintf(achErr, "cannot read [%s]", dbGetKey(CNSMGR_DB_KEY_SIMCARDPIN, 4));
			//	__goToError();
			//}

			//__goToErrorIfFalse(cnsChgChvCodes(1, achOld, achNew));


			rdbSetSingle("sim.cmd.status", "[error] not supported yet");
			goto error;
		}

		default:
		{
			char achCmds[256];
			dbGetUnknownCmdErr(pszCmds, achCmds);

			rdbSetSingle("sim.cmd.status", achCmds);
			goto error;
		}
	}

	return 0;

error:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
const struct dbcmdinfo* dbCmdGetInfo(const char* szDbVarName)
{
	const struct dbcmdinfo* pCmdInfo;

	for (pCmdInfo = dbCmdInfoTbl;pCmdInfo->dbVarName;pCmdInfo++)
	{
		if (!strcmp(szDbVarName, pCmdInfo->dbVarName))
			return pCmdInfo;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
int dbCmdMain()
{
	int cNoti = dbenum_enumDb(_pE);
	syslog(LOG_INFO, "%d items notified", cNoti);

	const struct dbcmdinfo* pDbCmdInfo = NULL;

	// get dbcmdinfo
	dbenumitem* pItem;
	for (pItem = dbenum_findFirst(_pE);pItem;pItem = dbenum_findNext(_pE))
		pDbCmdInfo = dbCmdGetInfo(pItem->szName);

	if (!pDbCmdInfo)
	{
		syslog(LOG_ERR, "couldn't find the corresponding database command information");
		goto error;
	}

	// read the variable
	char achValue[256];
	achValue[0] = 0;
	if (rdbGetSingle(pDbCmdInfo->dbVarName, achValue, sizeof(achValue) < 0))
	{
		syslog(LOG_ERR, "couldn't read database variable - %s", pDbCmdInfo->dbVarName);
		goto error;
	}

	// bypass if no handler
	if (!pDbCmdInfo->lpfnHandler)
		return 0;

	// call handler
	pDbCmdInfo->lpfnHandler(achValue);

	return 0;

error:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int rdbCreate(const char* szDbVarName)
{
	// get current uid and gid
	__uid_t uUID = getuid();
	__gid_t uGID = getgid();

	const char* szPrefixDbVarName = rdbGetPrefixDbVarName(szDbVarName);
	return rdb_create_variable(szPrefixDbVarName, NULL, CREATE, DEFAULT_PERM, uUID, uGID);
}
////////////////////////////////////////////////////////////////////////////////
int rdbDelete(const char* szDbVarName)
{
	const char* szPrefixDbVarName = rdbGetPrefixDbVarName(szDbVarName);
	return rdb_delete_variable(szPrefixDbVarName);
}
////////////////////////////////////////////////////////////////////////////////
int deleteOwnDbVariables()
{
	rdbDelete(ATPORTMGR_DB_MODULELOCK);
	rdbDelete(ATPORTMGR_DB_MODULELOCKED);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
void finiObjs(void)
{
	cmdchanger_destroy(_pC);
	lowserial_destroy(_pS);
	funccaller_destroy(_pF);

	deleteOwnDbVariables();

	dbenum_destroy(_pE);

	rdb_close_db();

	finiTickCount();

	// disable daemoon
	daemon_fini();
}

////////////////////////////////////////////////////////////////////////////////
int createOwnDbVariables()
{
	// create module lock
	rdbCreate(ATPORTMGR_DB_MODULELOCK);
	if (rdbSubscribe(ATPORTMGR_DB_MODULELOCK) < 0)
	{
		syslog(LOG_ERR, "cannot subscribe database variable (name=%s)", ATPORTMGR_DB_MODULELOCK);
		goto error;
	}

	// create module locked
	rdbCreate(ATPORTMGR_DB_MODULELOCKED);

	return 0;

error:
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
static int waitForPort(const char* szPort, int ms)
{
	struct stat portStat;

	tick tckS = getTickCountMS();
	int res = -1;

	while (res < 0)
	{
		if (fTermSigDetected)
			break;

		tick tckC = getTickCountMS();

		if (tckC > tckS + ms)
			break;

		res = stat(szPort, &portStat);
	}

	return res;
}
////////////////////////////////////////////////////////////////////////////////
void sigDummyHandler(int iSig)
{
	const char* szSig = strsignal(iSig);

	switch (iSig)
	{
		case SIGTERM:
			fprintf(stderr, "%s detected\n", szSig);

			fTermSigDetected = 1;
			break;

		default:
			fprintf(stderr, "%s ignored\n", szSig);
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////
int initObjs(void)
{
	// daemonize
	daemon_init(ATPORTMGR_DAEMON_NAME, NULL, _globalEnv.fDisableDaemon, _globalEnv.nDbgLevel);
	syslog(LOG_INFO, "daemonized");

	// ignore otherwise getting killed - platypus platform sends SIGHUP instread of SIGUSR1
	signal(SIGHUP, sigDummyHandler);
	signal(SIGINT, sigDummyHandler);
	signal(SIGTERM, sigDummyHandler);

	// init tick count
	initTickCount();

	// wait for usb device
	const char* szDev = _globalEnv.szAtPort;
	syslog(LOG_DEBUG, "waiting for the at port port up - %s", szDev);
	if (waitForPort(szDev, ATPORTMGR_DB_PORTWAIT*1000) < 0)
		syslog(LOG_ERR, "first-port-wait timeout");

	// open database
	int hDb = rdb_open_db();
	if (hDb < 0)
	{
		syslog(LOG_ERR, "failed to open database driver");
		goto error;
	}

	// create database enumerator
	_pE = dbenum_create(TRIGGERED);
	if (!_pE)
	{
		syslog(LOG_ERR, "failed to create dbenum - out of memory");
		goto error;
	}

	const char* szModuleName = _globalEnv.szModuleName;

	const char* szLast = _globalEnv.szLastPort;

	syslog(LOG_DEBUG, "waiting for all the last port up - %s", szLast);
	if (waitForPort(szLast, ATPORTMGR_DB_PORTWAIT*1000) < 0)
		syslog(LOG_ERR, "last-port-wait timeout");

	syslog(LOG_DEBUG, "starting job");

	// create own database variables
	createOwnDbVariables();

	// create serial
	_pS = lowserial_create(szDev, ATPORTMGR_ATANS_LEN * 4, ATPORTMGR_ATANS_LEN);
	if (!_pS)
		goto error;

	// get second module name
	const char* szSecModule = NULL;
	if (strcmp(szModuleName, PHONE_MODULE_NAME_GENERIC))
		szSecModule = PHONE_MODULE_NAME_GENERIC;

	// create cmdchanger
	_pC = cmdchanger_create(szModuleName, szSecModule, ATPORTMGR_ATANS_LEN);
	if (!_pC)
		goto error;

	// create function caller
	_pF = funccaller_create(ATPORTMGR_ATCOMMAND_TIMEOUT * 1000);
	if (!_pF)
		goto error;

	return 0;


error:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetAns(funccaller* pF, callback_entry* pEnt)
{
	const dbquery* pDbQuery = pEnt->pRef;

	// read answer from serial
	const char* szAns = lowserial_read(_pS);

	syslog(LOG_DEBUG, "read from serial (msg=%s)", __convCtrlToCStyle(szAns));

	// change the answer to db value
	const char* szDbValue = NULL;
	if (strlen(szAns))
		szDbValue = cmdchanger_getAtGetAns(_pC, pDbQuery->szDbVarName, pDbQuery->iIdx, szAns);

	if (!szDbValue)
		syslog(LOG_INFO, "the return message from module is not correct format (dbvarname=%s,msg=%s)", pDbQuery->szDbVarName, __convCtrlToCStyle(szAns));

	syslog(LOG_INFO, "write into database (name=%s,value=%s)", pDbQuery->szDbVarName, szDbValue);

	// write the db value
	int stat = rdbSetSingle(pDbQuery->szDbVarName, szDbValue ? szDbValue : "");
	if (stat < 0)
		syslog(LOG_ERR, "failed to write database variable (name=%s,value=%s)", pDbQuery->szDbVarName, szDbValue);

	stat = 0;
	if (!szDbValue)
		stat = -1;

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
int getPin(char* achBuf, int cbBuf)
{
	char achSimPin[256];
	achSimPin[0] = 0;

#ifdef	CONFIG_RALINK_RT3052
	//nvram_init(2860);
	char* szWwanPin = nvram_get(RT2860_NVRAM, "wwan_pin");
	//nvram_close(2860);

	if (szWwanPin)
		strcpy(achSimPin, szWwanPin);

#else
	char achAutoPin[256];
	char achPin[256];

	if (rdbGetSingle("sim.autopin", achAutoPin, sizeof(achAutoPin)) >= 0)
	{
		if (rdbGetSingle("sim.pin", achPin, sizeof(achPin)) >= 0)
		{
			if (atol(achAutoPin))
				strcpy(achSimPin, achPin);
		}
	}
#endif

	strncpy(achBuf, achSimPin, sizeof(achSimPin));
	achBuf[cbBuf-1] = 0;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetAns_UnlockPin(funccaller* pF, callback_entry* pEnt)
{
	const char* szAns = lowserial_read(_pS);

	syslog(LOG_DEBUG, "read from serial (msg=%s)", __convCtrlToCStyle(szAns));

	if (!szAns)
		return -1;

	if (__getRegPatternEx("\nOK\n", szAns, NULL) < 0)
	{
		char achResult[256];
		if (__getRegTokenEx("\\+CME ERROR: ([])\n", szAns, achResult, sizeof(achResult)) < 0)
			strcpy(achResult, "SIM negotiation failed");

		syslog(LOG_DEBUG, "error result detected - %s", achResult);

		rdbSetSingle("sim.status.status", achResult);
	};

	funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_Pin, funcCallerCallBackCmdChangerGetAns_Pin, callback_entry_type_deleteonfinish, NULL, pEnt);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetQuery_UnlockPin(funccaller* pF, callback_entry* pEnt)
{
	char szAt[256];
	char achPin[256];

	static char achPrevPin[256] = {0, };

	getPin(achPin, sizeof(achPin));

	// don't try if the pin is the same
	if (!strcmp(achPrevPin, achPin))
		sprintf(szAt, "at+cpin?\n");
	else
		sprintf(szAt, "at+cpin=%s\n", achPin);

	// write to serial
	int stat = lowserial_write(_pS, szAt, strlen(szAt));
	syslog(LOG_DEBUG, "written into serial (msg=%s)", __convCtrlToCStyle(szAt));

	if (stat < 0)
		syslog(LOG_ERR, "failed to write an AT command (at cmd=%s,err=%s)", __convCtrlToCStyle(szAt), strerror(errno));

	strcpy(achPrevPin, achPin);

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetQuery_Pin(funccaller* pF, callback_entry* pEnt)
{
	const char* szAt = "at+cpin?\n";

	// write to serial
	int stat = lowserial_write(_pS, szAt, strlen(szAt));
	syslog(LOG_DEBUG, "written into serial (msg=%s)", __convCtrlToCStyle(szAt));

	if (stat < 0)
		syslog(LOG_ERR, "failed to write an AT command (at cmd=%s,err=%s)", __convCtrlToCStyle(szAt), strerror(errno));

	return stat;
}

////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetQuery_SetCFUN(funccaller* pF, callback_entry* pEnt)
{
	syslog(LOG_DEBUG, "setting CFUN");

	const char* szAt = "at+cfun=1\n";

	// write to serial
	int stat = lowserial_write(_pS, szAt, strlen(szAt));

	if (stat < 0)
		syslog(LOG_ERR, "failed to write an AT command (at cmd=%s,err=%s)", __convCtrlToCStyle(szAt), strerror(errno));

	return stat;
}

int funcCallerCallBackCmdChangerGetAns_CFUN(funccaller* pF, callback_entry* pEnt);
int funcCallerCallBackCmdChangerGetQuery_CFUN(funccaller* pF, callback_entry* pEnt);

////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetAns_SetCFUN(funccaller* pF, callback_entry* pEnt)
{
	syslog(LOG_DEBUG, "getting result of setting CFUN");

	const char* szAns = lowserial_read(_pS);

	// get result
	if (!strstr(szAns, "OK\n"))
	{
		syslog(LOG_ERR, "informal pin result found - %s", __convCtrlToCStyle(szAns));
		goto error;
	}

	funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_CFUN, funcCallerCallBackCmdChangerGetAns_CFUN, callback_entry_type_deleteonfinish, NULL, pEnt);
	return 0;

error:
	funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_CFUN, funcCallerCallBackCmdChangerGetAns_CFUN, callback_entry_type_deleteonfinish, NULL, pEnt);
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetQuery_COPS(funccaller* pF, callback_entry* pEnt)
{
	syslog(LOG_DEBUG, "setting COPS");

	const char* szAt = "at+cops=3,0\n";

	// write to serial
	int stat = lowserial_write(_pS, szAt, strlen(szAt));

	if (stat < 0)
		syslog(LOG_ERR, "failed to write an AT command (at cmd=%s,err=%s)", __convCtrlToCStyle(szAt), strerror(errno));

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetQuery_CFUN(funccaller* pF, callback_entry* pEnt)
{
	syslog(LOG_DEBUG, "querying CFUN");

	const char* szAt = "at+cfun?\n";

	// write to serial
	int stat = lowserial_write(_pS, szAt, strlen(szAt));

	if (stat < 0)
		syslog(LOG_ERR, "failed to write an AT command (at cmd=%s,err=%s)", __convCtrlToCStyle(szAt), strerror(errno));

	return stat;
}

////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetAns_CFUN(funccaller* pF, callback_entry* pEnt)
{
	syslog(LOG_DEBUG, "getting result of querying CFUN");

	// read answer from serial
	const char* szAns = lowserial_read(_pS);
	char achResult[256];

	// get result
	if (__getRegTokenEx("\\+CFUN: ([])\n", szAns, achResult, sizeof(achResult)) < 0)
	{
		syslog(LOG_ERR, "informal pin result found - %s", __convCtrlToCStyle(szAns));
		goto error;
	}

	int iCFUN = atoi(achResult);

	syslog(LOG_DEBUG, "current CFUN = %d", iCFUN);

	if (!iCFUN)
		funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_SetCFUN, funcCallerCallBackCmdChangerGetAns_SetCFUN, callback_entry_type_deleteonfinish, NULL, pEnt);

	return 0;

error:
	// do pin status
	funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_CFUN, funcCallerCallBackCmdChangerGetAns_CFUN, callback_entry_type_deleteonfinish, NULL, pEnt);

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetAns_COPS(funccaller* pF, callback_entry* pEnt)
{
	syslog(LOG_DEBUG, "getting result of querying COPS");

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetAns_Pin(funccaller* pF, callback_entry* pEnt)
{
	// read answer from serial
	const char* szAns = lowserial_read(_pS);
	char achResult[256];

	syslog(LOG_DEBUG, "read from serial (msg=%s)", __convCtrlToCStyle(szAns));

	// get result
	if (__getRegTokenEx("(\\+CPIN|\\+CME ERROR): ([])\n", szAns, achResult, sizeof(achResult)) < 0)
	{
		syslog(LOG_ERR, "informal pin result found - %s", __convCtrlToCStyle(szAns));
		goto error;
	}

	syslog(LOG_DEBUG, "pin result - %s", achResult);

	// if SIM READY
	if (!strcmp(achResult, "READY") || !strcmp(achResult, "USIM READY"))
	{
		rdbSetSingle("sim.status.status", "SIM OK");
		// do pin status
		funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_Pin, funcCallerCallBackCmdChangerGetAns_Pin, callback_entry_type_deleteonfinish, NULL, pEnt);
	}
	// if SIM PIN
	else if (!strcmp(achResult, "SIM PIN"))
	{
		// get auto pin
		char szSimPin[256];
		getPin(szSimPin, sizeof(szSimPin));

		syslog(LOG_DEBUG, "sim pin in nvram - %s", szSimPin);

		int fPin = strlen(szSimPin);
		if (fPin)
		{
			// do the next step - try pin
			funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_UnlockPin, funcCallerCallBackCmdChangerGetAns_UnlockPin, callback_entry_type_deleteonfinish, NULL, pEnt);
		}
		else
		{
			rdbSetSingle("sim.status.status", "SIM PIN Required");
			// do pin status
			funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_Pin, funcCallerCallBackCmdChangerGetAns_Pin, callback_entry_type_deleteonfinish, NULL, pEnt);
		}
	}
	// if others
	else
	{
		rdbSetSingle("sim.status.status", achResult);
		// do pin status
		funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_Pin, funcCallerCallBackCmdChangerGetAns_Pin, callback_entry_type_deleteonfinish, NULL, pEnt);
	}

	return 0;

error:
	// do pin status
	funccaller_insert_before(_pF, funcCallerCallBackCmdChangerGetQuery_Pin, funcCallerCallBackCmdChangerGetAns_Pin, callback_entry_type_deleteonfinish, NULL, pEnt);

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int funcCallerCallBackCmdChangerGetQuery(funccaller* pF, callback_entry* pEnt)
{
	const dbquery* pDbQuery = pEnt->pRef;

	const char* szAt = cmdchanger_getAtGetQuery(_pC, pDbQuery->szDbVarName, pDbQuery->iIdx);
	if (!szAt)
	{
		syslog(LOG_INFO, "not supported by current module (dbvarname=%s)", pDbQuery->szDbVarName);

		return -1;
	}

	int cbAt = strlen(szAt);
	int stat = lowserial_write(_pS, szAt, cbAt);

	syslog(LOG_DEBUG, "written into serial (msg=%s)", __convCtrlToCStyle(szAt));

	if (stat < 0)
		syslog(LOG_ERR, "failed to write an AT command (at cmd=%s,err=%s)", __convCtrlToCStyle(szAt), strerror(errno));

	return stat;
}

typedef enum
{
	select_stage_idle,
	select_stage_query,
	select_stage_answer
} select_stage;

#define __max(x,y)			((x)>(y)?(x):(y))


////////////////////////////////////////////////////////////////////////////////
int isModuleLocked()
{
	char achValue[1024];
	int stat = -1;

	stat = rdbGetSingle(ATPORTMGR_DB_MODULELOCK, achValue, sizeof(achValue));
	if (stat < 0)
	{
		syslog(LOG_ERR, "failed to read database variable (name=%s)", ATPORTMGR_DB_MODULELOCK);
		goto error;
	}

	if (!strcmp(achValue, "1") || !strcmp(achValue, "lock"))
		return 1;

	return 0;

error:
	return	0;
}

////////////////////////////////////////////////////////////////////////////////
int selectObjs()
{
	int fLockDone = 0;
	int stat = 0;

	select_stage selStage = select_stage_idle;

	fd_set fdR;
	fd_set fdW;

	int fSharePort = _globalEnv.fSharedPort;

	int fOpenStatNeeded = !fSharePort || !isModuleLocked();

	if (fOpenStatNeeded)
		syslog(LOG_DEBUG, "need to open at port");
	else
		syslog(LOG_DEBUG, "don't need to open at port");

	// get nfds
	int hDb = rdb_get_fd();
	int nFds;

	callback_entry* pE;

	// put generic database query
	const dbquery* pDbQuery = dbQueries;
	while (pDbQuery->szDbVarName)
	{
		pE = funccaller_append(_pF, funcCallerCallBackCmdChangerGetQuery, funcCallerCallBackCmdChangerGetAns, pDbQuery->entType, pDbQuery);
		if (!pE)
			syslog(LOG_ERR, "unable to append an entry to function caller (db variable name=%s)", pDbQuery->szDbVarName);
		pDbQuery++;
	}

	// put pin query
	funccaller_append(_pF, funcCallerCallBackCmdChangerGetQuery_CFUN, funcCallerCallBackCmdChangerGetAns_CFUN, callback_entry_type_deleteonfinish, NULL);
	funccaller_append(_pF, funcCallerCallBackCmdChangerGetQuery_Pin, funcCallerCallBackCmdChangerGetAns_Pin, callback_entry_type_deleteonfinish, NULL);
	funccaller_append(_pF, funcCallerCallBackCmdChangerGetQuery_COPS, funcCallerCallBackCmdChangerGetAns_COPS, callback_entry_type_deleteonfinish, NULL);

	tick tckLastWrite = 0;
	int fWrapped = 0;

	while (1)
	{
		struct timeval tv = {ATPORTMGR_SELECT_INTERVAL, 0};
		int hSerial	= lowserial_getDev(_pS);

		// is serial locked?
		int fSLocked = hSerial < 0;

		FD_ZERO(&fdR);
		FD_ZERO(&fdW);

		// trigers - database
		FD_SET(hDb, &fdR);

		// trigers - serial port
		if (!fSLocked)
		{
			FD_SET(hSerial, &fdR);

			// only set if it has someothing to write
			if (lowserial_getWriteQueueLen(_pS))
				FD_SET(hSerial, &fdW);
		}

		// select
		nFds = __max(hSerial, hDb) + 1;
		stat = select(nFds, &fdR, &fdW, NULL, &tv);


		// get current tick
		tick tckCur = getTickCountMS();

		if (fTermSigDetected)
		{
			syslog(LOG_ERR, "preparing to terminate");
			break;
		}

		// select - interrupted
		if (stat < 0)
		{
			syslog(LOG_ERR, "select punk");
			break;
		}
		else
		{
			callback_entry* pEnt;

			if (!fSLocked)
			{
				// triggered - serial read
				if (FD_ISSET(hSerial, &fdR))
				{
					// gather in serial
					lowserial_result lsRes;
					int gatherStat = lowserial_gather(_pS, &lsRes);

					// if in answer stage
					if (gatherStat > 0 && selStage == select_stage_query)
					{
						pEnt = funccaller_getEntry(_pF);

						if (pEnt)
						{
							funccaller_callAnsCallback(_pF, pEnt);
							funccaller_moveOn(_pF, lsRes == lowserial_result_ok, &fWrapped);

							selStage = select_stage_answer;
						}
					}
				}

				// if in answer stage or idle stage
				int fEmpty = !lowserial_getWriteQueueLen(_pS);
				if (fEmpty && (selStage == select_stage_idle || selStage == select_stage_answer))
				{
					pEnt = funccaller_getEntry(_pF);
					int fDelayOver = tckCur - tckLastWrite > ATPORTMGR_ATCOMMAND_DELAY * 1000;

					if (pEnt && ((fDelayOver && fWrapped) || !fWrapped))
					{
						tckLastWrite = tckCur;

						// clear serial
						lowserial_clearReadQueue(_pS);
						// call query callback
						if (!(funccaller_callQueryCallback(_pF, pEnt, tckCur) < 0))
						{
							// set timeout
							funccaller_setTimeOut(_pF, pEnt, tckCur);

							selStage = select_stage_query;
						}
						else
						{
							funccaller_moveOn(_pF, lowserial_result_ok, &fWrapped);
						}
					}
				}

				// triggered - serial write
				if (FD_ISSET(hSerial, &fdW))
				{
					int cbWritten = lowserial_flushWrite(_pS);

					if (cbWritten < 0)
					{
						syslog(LOG_ERR, "write error occured. closing the port");
						lowserial_close(_pS);
					}
				}
			}

			// triggered - database
			fLockDone = 0;
			if (FD_ISSET(hDb, &fdR))
			{
				syslog(LOG_DEBUG, "database change detected");

				// TODO : eventually, dbCmdMain will be doing notification but now we have only module.lock subscribed
				//dbCmdMain();

				int fNewLock = isModuleLocked();

				fOpenStatNeeded = !fSharePort || !fNewLock;
				fLockDone = 1;
			}
		}

		// check timeout if query stage
		if (selStage == select_stage_query)
		{
			callback_entry* pEnt = funccaller_getEntry(_pF);

			if (funccaller_isTimeOut(_pF, pEnt, tckCur))
			{
				//const dbquery* pDbQuery = pEnt->pRef;
				syslog(LOG_ERR, "module doesn't respond (timeout=%d secs)", ATPORTMGR_ATCOMMAND_TIMEOUT);

				funccaller_callAnsCallback(_pF, pEnt);
				funccaller_moveOn(_pF, 0, &fWrapped);

				selStage = select_stage_answer;
			}
		}

		// open if needed
		if (fOpenStatNeeded && !lowserial_isOpen(_pS))
		{
			stat = lowserial_open(_pS);
			if (stat < 0)
				syslog(LOG_ERR, "failed to open at command port (dev=%s)", lowserial_getDevName(_pS));
			else
			{
				syslog(LOG_DEBUG, "at port opened");
				lowserial_eatGarbage(_pS);
			}

			selStage = select_stage_idle;
		}
		// close if needed
		else if (!fOpenStatNeeded && lowserial_isOpen(_pS))
		{
			if (lowserial_isOpen(_pS))
				lowserial_eatGarbage(_pS);

			lowserial_close(_pS);

			selStage = select_stage_idle;

			syslog(LOG_DEBUG, "at port closed");
		}

		// submit database variable
		if (fLockDone)
		{
			stat = rdbSetSingle(ATPORTMGR_DB_MODULELOCKED, "1");
			if (stat < 0)
				syslog(LOG_ERR, "fail to write in database - notify lock (dbvarname=%s)", ATPORTMGR_DB_MODULELOCKED);

			fLockDone = 0;
		}
	}

	return stat;
}


////////////////////////////////////////////////////////////////////////////////
void registerOrUnregisterDb(int fRegister)
{
	int stat;

	if (!fRegister)
	{
		rdbSetSingle("sim.status.status", "");
		rdbDelete("sim.status.status");
	}

	// create or delete db variables that are supported by command chanager
	const char* pDbVarName = cmdchanger_getFirstSupportedDbVarName(_pC);
	while (pDbVarName)
	{
		if (fRegister)
		{
			stat = rdbCreate(pDbVarName);
			if (stat < 0)
				syslog(LOG_ERR, "cannot create db variable - (name=%s,error=%s)", pDbVarName, strerror(errno));
		}
		else
		{
			rdbSetSingle(pDbVarName, "");
			stat = rdbDelete(pDbVarName);
			if (stat < 0)
				syslog(LOG_ERR, "failed to delete db variable - (name=%s,error=%s)", pDbVarName, strerror(errno));
		}

		pDbVarName = cmdchanger_getNextSupportedDbVarName(_pC);
	}

}

////////////////////////////////////////////////////////////////////////////////
int createDb()
{
	// create database variables
	registerOrUnregisterDb(1);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
void deleteDb()
{
	// delete database variables
	registerOrUnregisterDb(0);
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	usb_init();

	int stat;
	int res = -1;

	// set default environemtn variables
	memset(&_globalEnv, 0, sizeof(_globalEnv));

	_globalEnv.nDbgLevel = LOG_ERR;
	_globalEnv.szFullName = strdup(argv[0]);
	_globalEnv.szBaseName = strdup(basename(argv[0]));

	_globalEnv.szAtPort = "/dev/ttyUSB0";
	_globalEnv.szLastPort = 0;
	_globalEnv.szModuleName = PHONE_MODULE_NAME_GENERIC;
	_globalEnv.fSharedPort = 1;

	// command line options
	int opt;
	while ((opt = getopt(argc, argv, "Vdi:vp:hs:l:m:")) >= 0)
	{
		switch (opt)
		{
			case 'V':
				printf("Call-Direct AT port manager(%s) version %s\n", _globalEnv.szBaseName, ATPORTMGR_VERSION);
				exit(0);
				break;

			case 'd':
				_globalEnv.fDisableDaemon = 1;
				break;

			case 'i':
				_globalEnv.iInst = atol(optarg);
				break;

			case 'v':
				_globalEnv.nDbgLevel++;
				break;

			case 'p':
				_globalEnv.szAtPort = optarg;
				break;

			case 'l':
				_globalEnv.szLastPort = optarg;
				break;

			case 'm':
				_globalEnv.szModuleName = optarg;
				break;

			case 's':
				_globalEnv.fSharedPort = atol(optarg);
				break;

			case 'h':
				printf(
				    "Usage: %s [OPTION]...\n"
				    "Call-Direct AT port manager version %s\n"
				    "\n"
				    "\t-V                 \t output version information and exit\n"
				    "\t-d                 \t Disable daemonization\n"
				    "\t-i <instance no.>  \t select instance number for database keys (default %d)\n"
				    "\t-v                 \t increase debugging messsage level (default - ERR#3, max. 4 times WARN#4,NOTI#5,INFO#6,DBG#7)\n"
				    "\t-p <AT port>       \t specify AT port name (default - /dev/ttyUSB0)\n"
				    "\t-l <Last port>     \t specify Last port name (default - /dev/ttyUSB0)\n"
				    "\t-s <0 or 1>        \t specify if it shares PPP and AT ports (default - 1)\n"
				    "\t-m <module name>		\t speicify module name (default - geneirc)\n"
				    "\n"
				    "For maximum debugging display, run as folowing:\n"
				    "\t# %s -vvvv\n"
				    "\n",
				    _globalEnv.szBaseName, ATPORTMGR_VERSION, _globalEnv.iInst, _globalEnv.szBaseName
				);
				exit(0);
				break;

			case '?':
				fprintf(stderr,
				        "%s: Unrecognized option: -%c\n"
				        "Try `%s -h for more information\n",
				        _globalEnv.szBaseName, optopt, _globalEnv.szBaseName
				       );
				exit(-1);
				break;

			case ':':
				fprintf(stderr,
				        "%s: option -%c requires an operand\n"
				        "Try `%s -h for more information\n",
				        _globalEnv.szBaseName, optopt, _globalEnv.szBaseName
				       );
				exit(-1);
				break;

			default:
				fprintf(stderr,
				        "%s: invalid option -- %c\n"
				        "Try `%s -h for more information\n",
				        _globalEnv.szBaseName, optopt, _globalEnv.szBaseName
				       );
				exit(-1);
				break;
		}
	}


	if (!_globalEnv.szLastPort)
		_globalEnv.szLastPort = _globalEnv.szAtPort;

	// init objs
	stat = initObjs();
	if (stat < 0)
		goto fini;

	// activate db
	stat = createDb();
	if (stat < 0)
		goto fini;

	// select loop
	stat = selectObjs();
	if (stat < 0)
		goto fini;

	// tick everything is done ok
	res = 0;

fini:
	lowserial_close(_pS);

	// deactivate db
	deleteDb();
	// fini objs
	finiObjs();

	syslog(LOG_INFO, "finished");

	return res;
}
