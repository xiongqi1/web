#include "utils.h"

#include <string.h>

static char _achBuf[ATPORTMGR_ATANS_LEN*2+1];

////////////////////////////////////////////////////////////////////////////////
const char* __convCtrlToCStyle(const char* szRaw)
{
	if (!szRaw)
	{
		strcpy(_achBuf, "(NULL)");
	}
	else
	{
		char ch;

		char* pD = _achBuf;
		char* pEnd = _achBuf + sizeof(_achBuf) - 2;

		while (pD < pEnd)
		{
			ch = *szRaw++;
			if (!ch)
				break;

			if (ch == '\n')
			{
				*pD++ = '\\';
				ch = 'n';
			}
			else if (ch == '\r')
			{
				*pD++ = '\\';
				ch = 'r';
			}

			*pD++ = ch;
		}

		*pD++ = 0;
	}

	return _achBuf;
}
///////////////////////////////////////////////////////////////////////////////
int __getRegPatternEx(const char* szPattern, const char* szStr, regmatch_t* pRegMatch)
{
	int fRegInit = 0;

	regex_t reEx;
	int stat;

	// compile
	stat = regcomp(&reEx, szPattern, REG_EXTENDED | REG_NEWLINE);
	if (stat < 0)
		goto error;

	fRegInit = 1;

	regmatch_t tmpRegMatch;
	if (!pRegMatch)
		pRegMatch = &tmpRegMatch;

	// exec
	int nEFlag = 0;
	stat = regexec(&reEx, szStr, 1, pRegMatch, nEFlag);
	if (stat != 0)
		goto error;

	regfree(&reEx);
	return 0;

error:
	if (fRegInit)
		regfree(&reEx);

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int __getRegTokenEx(const char* szRegTokenEx, const char* szStr, char* pOut, int cbOut)
{
	char achH[256];
	char achT[256];
	char achToken[256];
	char achMidBuf[ATPORTMGR_ATANS_LEN+1];

	// get headpart of regtokenex
	char* pH = strstr(szRegTokenEx, CMDCHANGER_BRACKET_BEGIN);
	if (!pH)
		goto error;

	// get tailpart of regtokenex
	char* pT = strstr(szRegTokenEx, CMDCHANGER_BRACKET_END);
	if (!pT)
		goto error;

	// get token regex
	const char* pToken = pH + strlen(CMDCHANGER_BRACKET_BEGIN);
	int cbToken = pT - pToken;
	strncpy(achToken, pToken, cbToken);
	achToken[cbToken] = 0;

	// copy H regex
	int cbH = pH - szRegTokenEx;
	memcpy(achH, szRegTokenEx, cbH);
	achH[cbH] = 0;

	// copy T regex
	pT += strlen(CMDCHANGER_BRACKET_END);
	strcpy(achT, pT);

	int stat = -1;

	const char* pNewSrc = szStr;
	const char* pSrc;

	while (1)
	{
		pSrc = pNewSrc;

		// get H of string
		regmatch_t regMatchH = {0, };
		stat = __getRegPatternEx(achH, pSrc, &regMatchH);
		if (stat < 0)
			break;

		const char* pOutH = pSrc + regMatchH.rm_eo;

		pNewSrc = pOutH + 1;

		// get T of string
		regmatch_t regMatchT = {0, };
		stat = __getRegPatternEx(achT, pOutH, &regMatchT);
		if (stat < 0)
			continue;

		const char* pOutT = pOutH + regMatchT.rm_so;

		// get token length of string
		int cbOutHT = pOutT - pOutH;
		if (cbOut < cbOutHT)
			continue;

		// get token
		if (cbToken)
		{
			memcpy(achMidBuf, pOutH, cbOutHT);
			achMidBuf[cbOutHT] = 0;

			// get token
			regmatch_t regMatchToken = {0, };
			stat = __getRegPatternEx(achToken, achMidBuf, &regMatchToken);
			if (stat < 0)
				continue;

			cbOutHT = regMatchToken.rm_eo - regMatchToken.rm_so;
			pOutH = achMidBuf + regMatchToken.rm_so;
		}

		// copy
		if (cbOutHT)
			memcpy(pOut, pOutH, cbOutHT);
		pOut[cbOutHT] = 0;
		break;

	}

	return stat;

error:
	return -1;
}
