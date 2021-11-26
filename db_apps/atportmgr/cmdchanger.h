#ifndef __CMDTALKER_H__
#define __CMDTALKER_H__

#include "moduledatabase.h"


typedef struct
{
	const attranstbl* pAtTransTbl;
	const attranstbl* pDefTransTbl;
	cmdchanger_module_type moduleType;

	int cbMaxAns;

	char* pAtGetBuf;				// return buffer for cmdchanger_getAtGetQuery
	char* pAtGetAnsBuf;			// return buffer for cmdchanger_getAtGetAns
	char* pAtGetAnsPosBuf;	// return buffer for cmdchanger_getAtGetAns after post process

	int iSearchIdx;
	int iGenericSearchIdx;
} cmdchanger;


const char* cmdchanger_getAtGetQuery(cmdchanger* pC, const char* szDbVarName, int iIdx);
const char* cmdchanger_getAtGetAns(cmdchanger* pC, const char* szDbVarName, int iIdx, const char* szAtResult);
cmdchanger* cmdchanger_create(const char* szPrimModuleName, const char* szSecModuleName, int cbMaxAns);
void cmdchanger_destroy(cmdchanger* pC);

const char* cmdchanger_getNextSupportedDbVarName(cmdchanger* pC);
const char* cmdchanger_getFirstSupportedDbVarName(cmdchanger* pC);

const char* cmdchanger_getNextGenericSupportedDbVarName(cmdchanger* pC);
const char* cmdchanger_getFirstGenericSupportedDbVarName(cmdchanger* pC);

#endif
