#include "netdevusage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
static int netdevusage_readUsage(netdevusage* pN, unsigned long long* pRecv, unsigned long long* pSent)
{
	int stat = -1;

	// open proc
	FILE* pF = fopen(NETDEVUSAGE_STAT_DEV, "r");
	if (!pF)
		return -1;

	char achDev[256];
	char achBuf[256];

	unsigned long long cbRecv;
	unsigned long long cbSent;
	int nDummy;

	while (fgets(achBuf, sizeof(achBuf), pF))
	{
		char* pCh = strchr(achBuf, ':');
		if (pCh)
			*pCh = ' ';

		// read columns
		int cInput = sscanf(achBuf, "%s %lld %d %d %d %d %d %d %d %lld %d %d %d %d %d %d %d", achDev, &cbRecv, &nDummy, &nDummy, &nDummy, &nDummy, &nDummy, &nDummy, &nDummy, &cbSent, &nDummy, &nDummy, &nDummy, &nDummy, &nDummy, &nDummy, &nDummy);

		// bypass if less input
		if (cInput != 17)
			continue;

		// bypass if dev not matched
		if (strcmp(pN->achDev, achDev))
			continue;

		*pRecv = cbRecv;
		*pSent = cbSent;

		stat = 0;

		break;
	}

	fclose(pF);

	return stat;
}
///////////////////////////////////////////////////////////////////////////////
int netdevusage_getUsageChg(netdevusage* pN, unsigned long long* pDeltaRecv, unsigned long long* pDeltaSent)
{
	unsigned long long cbNewRecv;
	unsigned long long cbNewSent;

	// reset delta
	*pDeltaRecv = 0;
	*pDeltaSent = 0;

	// get current usage
	int stat = netdevusage_readUsage(pN, &cbNewRecv, &cbNewSent);
	if (stat < 0)
		return -1;

	if (pN->fValid)
	{
		if(pN->cbCurRecv>cbNewRecv)
			pN->cbCurRecv=cbNewRecv;

		if(pN->cbCurSent>cbNewSent)
			pN->cbCurSent=cbNewSent;

		*pDeltaRecv = cbNewRecv - pN->cbCurRecv;
		*pDeltaSent = cbNewSent - pN->cbCurSent;
	}
	else
	{
		*pDeltaRecv = 0;
		*pDeltaSent = 0;
	}

	pN->cbCurRecv = cbNewRecv;
	pN->cbCurSent = cbNewSent;

	pN->fValid = 1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
void netdevusage_destroy(netdevusage* pN)
{
	if (!pN)
		return;

	free(pN);
}
///////////////////////////////////////////////////////////////////////////////
netdevusage* netdevusage_create(const char* szDev)
{
	netdevusage* pN;

	pN = malloc(sizeof(*pN));
	if (!pN)
		goto error;
	memset(pN, 0, sizeof(*pN));

	strcpy(pN->achDev, szDev);

	return pN;

error:
	netdevusage_destroy(pN);
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
