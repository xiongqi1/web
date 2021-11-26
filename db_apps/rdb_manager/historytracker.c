#include "historytracker.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
void historytracker_destroy(historytracker* pT)
{
	if (!pT)
		return;

	free(pT);
}

///////////////////////////////////////////////////////////////////////////////
static time_t historytracker_getHourlyTime(time_t tmCur)
{
	struct tm stmHour;

	localtime_r(&tmCur, &stmHour);
	stmHour.tm_sec = 0;
	stmHour.tm_min = 0;

	return mktime(&stmHour);
}

///////////////////////////////////////////////////////////////////////////////
static void historytracker_removeSpecificHistoryEnt(historytracker* pT, historyent* pE)
{
	list_del(&pE->list);
	free(pE);
}
/////////////////////////////////////////////////////////////////////////////////
//static void historytracker_removeTailHistoryEnt(historytracker* pT)
//{
//	if(list_empty(&pT->listHdr))
//		return;
//
//	historyent* pE=list_entry(pT->listHdr.prev,historyent,list);
//	historytracker_removeSpecificHistoryEnt(pT,pE);
//
//	pT->cHistory--;
//}
///////////////////////////////////////////////////////////////////////////////
static historyent* historytracker_getHistoryEnt(historytracker* pT)
{
	if (list_empty(&pT->listHdr))
		return NULL;

	return list_entry(pT->listHdr.next, historyent, list);
}
///////////////////////////////////////////////////////////////////////////////
static historyent* historytracker_addHistoryEntInt(historytracker* pT, time_t tmTraffic, int fHead)
{
	historyent* pE = malloc(sizeof(*pE));
	if (!pE)
		return 0;

	memset(pE, 0, sizeof(*pE));
	pE->tmTraffic = tmTraffic;

	if (fHead)
		list_add(&pE->list, &pT->listHdr);
	else
		list_add_tail(&pE->list, &pT->listHdr);

	pT->cHistory++;

	return pE;
}
///////////////////////////////////////////////////////////////////////////////
static historyent* historytracker_addHistoryEnt(historytracker* pT, time_t tmTraffic)
{
	return historytracker_addHistoryEntInt(pT, tmTraffic, 1);
}
///////////////////////////////////////////////////////////////////////////////
static historyent* historytracker_addHistoryEntTail(historytracker* pT, time_t tmTraffic)
{
	return historytracker_addHistoryEntInt(pT, tmTraffic, 0);
}
///////////////////////////////////////////////////////////////////////////////
void historytracker_getTrafficDetail(historytracker* pT, time_t tmCur, char* achRecv, char* achSent)
{
	achRecv[0] = 0;
	achSent[0] = 0;

	historyent* pE;
	pE = historytracker_getHistoryEnt(pT);

	// get start hour
	time_t tmHour = historytracker_getHourlyTime(tmCur);

	char achBuf[256];
	unsigned long long cbRecv;
	unsigned long long cbSent;

	int iHour;

	for (iHour = 0;iHour < pT->cMaxHours;iHour++)
	{
		if (!pE || tmHour != pE->tmTraffic)
		{
			cbRecv = 0;
			cbSent = 0;
		}
		else
		{
			cbRecv = pE->cbRecv;
			cbSent = pE->cbSent;

			if (pE->list.next == &pT->listHdr)
				pE = NULL;
			else
				pE = list_entry(pE->list.next, historyent, list);
		}

		sprintf(achBuf, "%lld ", cbRecv);
		strcat(achRecv, achBuf);
		sprintf(achBuf, "%lld ", cbSent);
		strcat(achSent, achBuf);

		tmHour -= 1 * 60 * 60;
	}
}

///////////////////////////////////////////////////////////////////////////////
static void historytracker_updateTotalTraffic(historytracker* pT)
{
	// reset
	pT->cHistory = 0;
	pT->cbTotalRecv = 0;
	pT->cbTotalSent = 0;

	if (list_empty(&pT->listHdr))
		return;

	historyent* pFE = list_entry(pT->listHdr.next, historyent, list);
	time_t tmMaxOld = pFE->tmTraffic - pT->cMaxHours * 60 * 60;

	// accumulate
	historyent* pE;
	historyent* pN;
	list_for_each_entry_safe(pE, pN, &pT->listHdr, list)
	{
		if (pE->tmTraffic <= tmMaxOld || pE->tmTraffic > pFE->tmTraffic)
		{
			historytracker_removeSpecificHistoryEnt(pT, pE);
			continue;
		}

		pT->cHistory++;

		pT->cbTotalRecv += pE->cbRecv;
		pT->cbTotalSent += pE->cbSent;
	}
}
///////////////////////////////////////////////////////////////////////////////
static void __tm2a(time_t tm, char* achBuf)
{
	struct tm stm;

	localtime_r(&tm, &stm);

	sprintf(achBuf, "%02d-%02d-%04d %02d:%02d:%02d", stm.tm_mday, stm.tm_mon + 1, stm.tm_year + 1900, stm.tm_hour, stm.tm_min, stm.tm_sec);
}
///////////////////////////////////////////////////////////////////////////////
static time_t __a2tm(const char* szDate)
{
	struct tm stm;

	memset(&stm, 0, sizeof(stm));

	sscanf(szDate, "%02d-%02d-%04d %02d:%02d:%02d", &stm.tm_mday, &stm.tm_mon, &stm.tm_year, &stm.tm_hour, &stm.tm_min, &stm.tm_sec);
	stm.tm_year -= 1900;
	stm.tm_mon -= 1;

	return mktime(&stm);
}
///////////////////////////////////////////////////////////////////////////////
int historytracker_saveFile(historytracker* pT, const char* szFile)
{
	// open file
	FILE* pF = fopen(szFile, "w");
	if (!pF)
		return -1;

	char achDate[128];

	historyent* pEnt;
	list_for_each_entry(pEnt, &pT->listHdr, list)
	{
		__tm2a(pEnt->tmTraffic, achDate);

		fprintf(pF, "%s %lld %lld\n", achDate, pEnt->cbRecv, pEnt->cbSent);
	}

	// close
	fclose(pF);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int historytracker_loadFile(historytracker* pT, const char* szFile)
{
	// open file
	FILE* pF = fopen(szFile, "r");
	if (!pF)
		return -1;

	char achDate[128];
	char achTime[128];
	char achDateTime[256];

	unsigned long long cbRecv;
	unsigned long long cbSent;
	int cInput;

	char achBuf[256];

	while (fgets(achBuf, sizeof(achBuf), pF))
	{
		cInput = sscanf(achBuf, "%s %s %lld %lld", achDate, achTime, &cbRecv, &cbSent);
		if (cInput != 4)
			continue;

		sprintf(achDateTime, "%s %s", achDate, achTime);

		time_t tmTraffic = __a2tm(achDateTime);
		historyent* pE = historytracker_addHistoryEntTail(pT, tmTraffic);

		pE->cbRecv = cbRecv;
		pE->cbSent = cbSent;
	}

	// close
	fclose(pF);

	historytracker_updateTotalTraffic(pT);

	return pT->cHistory;
}

///////////////////////////////////////////////////////////////////////////////
unsigned long long historytracker_getTotalTraffic(historytracker* pT)
{
	return pT->cbTotalRecv + pT->cbTotalSent;
}
///////////////////////////////////////////////////////////////////////////////
void historytracker_addTraffic(historytracker* pT, time_t tmCur, unsigned long long cbRecv, unsigned long long cbSent)
{
	// bypass if empty traffic
	if (!cbRecv && !cbSent)
		return;

	// get hourly time
	time_t tmTraffic = historytracker_getHourlyTime(tmCur);

	historyent* pE;

	int fAddedEnt = 0;

	// add a new entry if empty
	if (list_empty(&pT->listHdr))
	{
		pE = historytracker_addHistoryEnt(pT, tmTraffic);
		fAddedEnt = 1;
	}
	else
	{
		pE = historytracker_getHistoryEnt(pT);

		// add a new entry if past
		if (pE->tmTraffic != tmTraffic)
		{
			pE = historytracker_addHistoryEnt(pT, tmTraffic);
			fAddedEnt = 1;
		}
	}

	if (fAddedEnt)
		historytracker_updateTotalTraffic(pT);

	pE->cbRecv += cbRecv;
	pE->cbSent += cbSent;

	pT->cbTotalRecv += cbRecv;
	pT->cbTotalSent += cbSent;
}
///////////////////////////////////////////////////////////////////////////////
void historytracker_changePeriod(historytracker* pT,int nHourCnt)
{
	pT->cMaxHours = nHourCnt;
}
///////////////////////////////////////////////////////////////////////////////
int historytracker_getPeriod(historytracker* pT)
{
	return pT->cMaxHours;
}
///////////////////////////////////////////////////////////////////////////////
historytracker* historytracker_create(int nHourCnt)
{
	historytracker* pT = malloc(sizeof(*pT));
	if (!pT)
		goto error;
	memset(pT, 0, sizeof(*pT));

	pT->cMaxHours = nHourCnt;
	INIT_LIST_HEAD(&pT->listHdr);

	return pT;

error:
	historytracker_destroy(pT);
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
