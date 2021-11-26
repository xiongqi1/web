#ifndef __HISTORY_TRACKER__
#define __HISTORY_TRACKER__

#include "list.h"
#include <time.h>

typedef struct
{
	struct list_head list;

	time_t tmTraffic;

	unsigned long long cbRecv;
	unsigned long long cbSent;


} historyent;


typedef struct
{
	struct list_head listHdr;

	int cHistory;
	unsigned long long cbTotalRecv;
	unsigned long long cbTotalSent;

	int cMaxHours;

} historytracker;


historytracker* historytracker_create(int nHourCnt);
void historytracker_addTraffic(historytracker* pT,time_t tmCur,unsigned long long cbRecv,unsigned long long cbSent);
int historytracker_loadFile(historytracker* pT, const char* szFile);
int historytracker_saveFile(historytracker* pT, const char* szFile);
void historytracker_destroy(historytracker* pT);
unsigned long long historytracker_getTotalTraffic(historytracker* pT);
void historytracker_getTrafficDetail(historytracker* pT,time_t tmCur,char* achRecv,char* achSent);
int historytracker_getPeriod(historytracker* pT);
void historytracker_changePeriod(historytracker* pT,int nHourCnt);

#endif
