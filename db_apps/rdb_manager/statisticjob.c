#include "statisticjob.h"

#include <sys/select.h>
#include <time.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <libgen.h>
#include <linux/limits.h>

#include "base.h"
#include "netdevusage.h"
#include "historytracker.h"
#include "rdb_ops.h"

#define RDBMANAGER_STATISTIC_FREQ_SEC					1
#define RDBMANAGER_STATISTIC_FREQ_USEC				0

#define RDBMANAGER_STATISTIC_IFNAME				"wwan.0.netif"
#define RDBMANAGER_STATISTIC_DEFAULT_HOURS		24

#define RDBMANAGER_STATISTIC_WRITE_FREQ				60
#define RDBMANAGER_STATISTIC_WRITE_LEN				(1*1024*1024)

/* If not using /usr/local/cdcs, make sure that /usr/local/cdcs is a symlink
 * to the real location. */

#define RDBMANAGER_STATISTIC_FILENAME_CUR			"/usr/local/cdcs/conf/system.stat"
#define RDBMANAGER_STATISTIC_FILENAME_NEW			"/usr/local/cdcs/conf/system.stat.new"


#define RDBMANAGER_STATISTIC_DELAY						4

#define RDBMANAGER_STATISTIC_RECV							"statistics.recv"
#define RDBMANAGER_STATISTIC_SENT							"statistics.sent"
#define RDBMANAGER_STATISTIC_RECV_DETAIL			"statistics.recv_detail"
#define RDBMANAGER_STATISTIC_SENT_DETAIL			"statistics.sent_detail"

#define RDBMANAGER_STATISTIC_TOTAL_HOURS			"statistics.total_hours"


///////////////////////////////////////////////////////////////////////////////
static const char* _statVarNames[] = {RDBMANAGER_STATISTIC_RECV, RDBMANAGER_STATISTIC_SENT, RDBMANAGER_STATISTIC_RECV_DETAIL, RDBMANAGER_STATISTIC_SENT_DETAIL, NULL};
static netdevusage* _pUsage = NULL;
static historytracker* _pHist = NULL;
static int _fDatabaseOpen = 0;

///////////////////////////////////////////////////////////////////////////////
void finiStatisticJob(void)
{
	netdevusage_destroy(_pUsage);
	historytracker_destroy(_pHist);

	if (_fDatabaseOpen)
	{
		// delete variables
		const char** ppName = &_statVarNames[0];
		while (*ppName)
			rdb_delete_variable(*ppName++);

		rdb_close_db();
	}
}
///////////////////////////////////////////////////////////////////////////////
int getTotalPeriodInHoursFromDb()
{
	// get total period
	char achTotalHours[64]={0,};
	if(rdb_get_single(RDBMANAGER_STATISTIC_TOTAL_HOURS,achTotalHours,sizeof(achTotalHours))<0)
	{
		syslog(LOG_ERR, "failed to read %s - %s. applying default hours", RDBMANAGER_STATISTIC_TOTAL_HOURS, strerror(errno));
		achTotalHours[0]=0;
	}

	int nTotalHours=atoi(achTotalHours);

	if(nTotalHours<=0)
		nTotalHours=RDBMANAGER_STATISTIC_DEFAULT_HOURS;

	return nTotalHours;
}
///////////////////////////////////////////////////////////////////////////////
void saveStatisticFile(time_t tmCur)
{
	int stat;

	// write into new
	stat = historytracker_saveFile(_pHist, RDBMANAGER_STATISTIC_FILENAME_NEW);
	if (stat < 0)
	{
		syslog(LOG_ERR, "write statistic information into %s - %s", RDBMANAGER_STATISTIC_FILENAME_NEW, strerror(errno));
	}
	else
	{
		// rename into cur
		sync();
		stat = rename(RDBMANAGER_STATISTIC_FILENAME_NEW, RDBMANAGER_STATISTIC_FILENAME_CUR);
		if (stat < 0)
			syslog(LOG_ERR, "rename statistic information into %s - %s", RDBMANAGER_STATISTIC_FILENAME_CUR, strerror(errno));
	}

	if (_fDatabaseOpen)
	{
		char achBuf[256];

		char achRecvDetail[256];
		char achSentDetail[256];

		historytracker_getTrafficDetail(_pHist, tmCur, achRecvDetail, achSentDetail);

		#ifndef DEBUG
		// lock database
		stat = rdb_database_lock(0);
		if (stat < 0)
			syslog(LOG_ERR, "cannot lock database - err#%d", stat);
		#endif

		// write recv into database
		sprintf(achBuf, "%lld", _pHist->cbTotalRecv);
		stat = rdb_set_single(RDBMANAGER_STATISTIC_RECV, achBuf);
		if (stat < 0)
			stat = rdb_create_variable(RDBMANAGER_STATISTIC_RECV, achBuf, STATISTICS, DEFAULT_PERM, 0, 0);
		if (stat < 0)
			syslog(LOG_ERR, "failed to write recv in database err(name=%s,value=%s) - %s", RDBMANAGER_STATISTIC_RECV, achBuf, strerror(errno));

		// write sent into database
		sprintf(achBuf, "%lld", _pHist->cbTotalSent);
		stat = rdb_set_single(RDBMANAGER_STATISTIC_SENT, achBuf);
		if (stat < 0)
			stat = rdb_create_variable(RDBMANAGER_STATISTIC_SENT, achBuf, STATISTICS, DEFAULT_PERM, 0, 0);
		if (stat < 0)
			syslog(LOG_ERR, "failed to write sent in database err(name=%s,value=%s) - %s", RDBMANAGER_STATISTIC_SENT, achBuf, strerror(errno));

		// write recv detail into database
		stat = rdb_set_single(RDBMANAGER_STATISTIC_RECV_DETAIL, achRecvDetail);
		if (stat < 0)
			stat = rdb_create_variable(RDBMANAGER_STATISTIC_RECV_DETAIL, achRecvDetail, STATISTICS, DEFAULT_PERM, 0, 0);
		if (stat < 0)
			syslog(LOG_ERR, "failed to write detail into database err(name=%s,value=%s) - %s", RDBMANAGER_STATISTIC_RECV_DETAIL, achRecvDetail, strerror(errno));

		// write sent detail into database
		stat = rdb_set_single(RDBMANAGER_STATISTIC_SENT_DETAIL, achSentDetail);
		if (stat < 0)
			stat = rdb_create_variable(RDBMANAGER_STATISTIC_SENT_DETAIL, achSentDetail, STATISTICS, DEFAULT_PERM, 0, 0);
		if (stat < 0)
			syslog(LOG_ERR, "failed to write detail into database err(name=%s,value=%s) - %s", RDBMANAGER_STATISTIC_SENT_DETAIL, achSentDetail, strerror(errno));

		#ifndef DEBUG
		// unlock database
		stat = rdb_database_unlock();
		if (stat < 0)
			syslog(LOG_ERR, "cannot unlock database - err#%d", stat);
		#endif
	}
}
///////////////////////////////////////////////////////////////////////////////
const char* get3GIfName()
{
	#ifdef PLATFORM_Avian
		return "rmnet0";
	#elif defined PLATFORM_Bovine
		static char achNetIf[256];
	
		if(rdb_get_single(RDBMANAGER_STATISTIC_IFNAME,achNetIf,sizeof(achNetIf))<0)
		{
			syslog(LOG_ERR, "failed to read %s - %s", RDBMANAGER_STATISTIC_IFNAME, strerror(errno));
			return "wwan0";
		};
	
		strcat(achNetIf,"0");
	
		return achNetIf;
	#else
		return "ppp0";
	#endif

}
///////////////////////////////////////////////////////////////////////////////
int initStatisticJob(void)
{
	if (rdb_open_db() < 0)
	{
		syslog(LOG_CRIT, "cannot open database - %s", strerror(errno));
		return -1;
	}
	else
	{
		_fDatabaseOpen = TRUE;
	}

	if (_fDatabaseOpen)
	{
		// create statistic variables
		const char** ppName = &_statVarNames[0];
		while (*ppName)
		{
			if (rdb_create_variable(*ppName, NULL, STATISTICS, DEFAULT_PERM, 0, 0) < 0)
				syslog(LOG_ERR, "failed to create database variable %s - %s", *ppName, strerror(errno));

			ppName++;
		}
	}

	const char* szNetIf=get3GIfName();
	_pUsage = netdevusage_create(szNetIf);
	if (!_pUsage)
		return -1;

	int nTotalPeriodInHour=getTotalPeriodInHoursFromDb();
	_pHist = historytracker_create(nTotalPeriodInHour);
	if (!_pHist)
		return -1;

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int doStatisticJob(void)
{
	if (initStatisticJob() < 0)
		goto error;

	// create directory structure if needed
	char achDir[PATH_MAX];
	strcpy(achDir,RDBMANAGER_STATISTIC_FILENAME_CUR);

	char* szConfDir=dirname(achDir);
	if(!__isExisting(szConfDir))
	{
		if(__mkdirall(szConfDir)<0)
			syslog(LOG_CRIT, "failed to mkdir %s",szConfDir);
	}

	// load statistic file and get the total traffic
	unsigned long long cbTrafficWritten;
	historytracker_loadFile(_pHist, RDBMANAGER_STATISTIC_FILENAME_CUR);
	cbTrafficWritten = historytracker_getTotalTraffic(_pHist);

	// set written clock
	clock_t clkPerSec = __getTicksPerSecond();
	clock_t clkWritten = __getTickCount();

	time_t tmCur = time(0);

	saveStatisticFile(tmCur);

	while (1)
	{
		struct timeval tv = {RDBMANAGER_STATISTIC_FREQ_SEC, RDBMANAGER_STATISTIC_FREQ_USEC};
		int stat = select(0, NULL, NULL, NULL, &tv);

		tmCur = time(0);
		clock_t clkCur = __getTickCount();


		// terminate gracefully if signal detected
		if (g_fSigTerm)
		{
			syslog(LOG_ERR, "signal detected in select");
			break;
		}

		// break if unknown error occurs
		if (stat < 0)
		{
			if(errno==EINTR) {
				syslog(LOG_ERR, "signal was caught");
			}
			else {
				syslog(LOG_ERR, "unknown error occured from select (stat=%d) - %s", stat,strerror(errno));
				break;
			}
		}

		// get different traffic
		unsigned long long cbDeltaRecv;
		unsigned long long cbDeltaSent;
		netdevusage_getUsageChg(_pUsage, &cbDeltaRecv, &cbDeltaSent);

		// add traffic into history
		historytracker_addTraffic(_pHist, tmCur, cbDeltaRecv, cbDeltaSent);
		unsigned long long cbCurTraffic = historytracker_getTotalTraffic(_pHist);

		int fPeriodChg=0;

		int nNewPeriod=getTotalPeriodInHoursFromDb();
		fPeriodChg=nNewPeriod!=historytracker_getPeriod(_pHist);

		if(fPeriodChg)
		{
			syslog(LOG_DEBUG, "period change detected - %d",nNewPeriod);
			historytracker_changePeriod(_pHist,nNewPeriod);
		}

		// bypass if no traffic
		if ( (cbTrafficWritten == cbCurTraffic) && !fPeriodChg)
			continue;

		unsigned long long cbTrafficExceeded = cbCurTraffic - cbTrafficWritten;
		clock_t clkTimeExceeded = (clkCur - clkWritten) / clkPerSec;

		// get status to write
		int fTrafficExceeded = cbTrafficExceeded >= RDBMANAGER_STATISTIC_WRITE_LEN;
		int fTimeExceeded = clkTimeExceeded >= RDBMANAGER_STATISTIC_WRITE_FREQ;

		syslog(LOG_DEBUG, "traffic in queue to write - cbCurTraffic=%lld (len=%lldkb,sec=%ld)", cbCurTraffic, fTrafficExceeded ? 0 : (RDBMANAGER_STATISTIC_WRITE_LEN - cbTrafficExceeded) / 1024, fTimeExceeded ? 0 : (RDBMANAGER_STATISTIC_WRITE_FREQ - clkTimeExceeded));

		// bypass if not needed to write
		if (!fTrafficExceeded && !fTimeExceeded && !fPeriodChg)
			continue;

		syslog(LOG_DEBUG, "writing action triggered - traffic=%d,time=%d", fTrafficExceeded, fTimeExceeded);

		// set written point
		cbTrafficWritten = cbCurTraffic;
		clkWritten = clkCur;

		saveStatisticFile(tmCur);
	}

	saveStatisticFile(tmCur);

	finiStatisticJob();
	return 0;

error:
	finiStatisticJob();
	return -1;
}

