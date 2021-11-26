#include "base.h"
#include "rdb_ops.h"

#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <linux/limits.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
clock_t __getTicksPerSecond(void)
{
	return sysconf(_SC_CLK_TCK);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
clock_t __getTickCount(void)
{
	struct tms tm;

	return times(&tm);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int __isExisting(const char* fileName)
{
	struct stat statStruc;

	return stat(fileName,&statStruc)>=0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int __mkdirall(const char* szDir)
{
	char fullDir[PATH_MAX];
	char* savePtr;

	char dirName[PATH_MAX];
	char* token;

	strcpy(fullDir,szDir);

	// copy inital string
	dirName[0]=0;
	if(szDir[0]=='/')
		strcpy(dirName,"/");
	
	token=strtok_r(fullDir,"/",&savePtr);
	while(token)
	{
		strcat(dirName,token);
		
		if(!__isExisting(dirName))
		{
			if(mkdir(dirName,0755)<0)
				return -1;
		}

		strcat(dirName,"/");

		token=strtok_r(0,"/",&savePtr);
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
typedef struct {
    const char *p_readyName;
    const char *p_watchdogName;
    clock_t lastClock;
    uint32_t lastWatchdog;
} RdbManagerModeData;

static RdbManagerModeData s_modeData[] = {
    {"rdb_manager.cfg.ready", "rdb_manager.cfg.watchdog", 0, 0},            // RDB_MANAGER_MODE_CONFIG
    {"rdb_manager.stat.ready", "rdb_manager.stat.watchdog", 0, 0},          // RDB_MANAGER_MODE_STATS
    {"rdb_manager.template.ready", "rdb_manager.template.watchdog", 0, 0}   // RDB_MANAGER_MODE_TEMPLATE
};

static clock_t s_watchdogClockDelta = 0;

int getRdbReady(RdbMgrMode mode)
{
    if (mode < 0 || mode >= COUNTOF(s_modeData)) {
        return 0;
    }

    const char *p_rdbName = s_modeData[mode].p_readyName;
    int ready = 0;
    int stat = rdb_get_single_int(p_rdbName, &ready);

    if (stat < 0 && errno != ENOENT) {
        syslog(LOG_ERR, "failed to get value from %s - %s (%d)", p_rdbName, strerror(errno), errno);
    }

    /* ready should only be "1" when selecting mode is ready as setRdbReady only sets it to "1" */
    return (stat == 0 && ready == 1) ? 1 : 0;
}

void setRdbReady(RdbMgrMode mode, int ready)
{
    if (mode < 0 || mode >= COUNTOF(s_modeData)) {
        return;
    }

    const char *p_rdbName = s_modeData[mode].p_readyName;
    const char *p_ready = ready ? "1" : "0";
    int stat = rdb_set_single(p_rdbName, p_ready);

    if ((stat < 0) && (errno == ENOENT)) {
        stat = rdb_create_variable(p_rdbName, p_ready, 0, DEFAULT_PERM, 0, 0);
    }
    if (stat < 0) {
        syslog(LOG_ERR, "failed to set %s to %s - %s (%d)", p_rdbName, p_ready, strerror(errno), errno);
    }
    else {
        syslog(LOG_INFO, "set %s to %s", p_rdbName, p_ready);
    }
}

void updateRdbWatchdog(RdbMgrMode mode)
{
    if (mode < 0 || mode >= COUNTOF(s_modeData)) {
        return;
    }

    RdbManagerModeData *p_modeData = &s_modeData[mode];

    if (s_watchdogClockDelta == 0) {
        s_watchdogClockDelta = WATCHDOG_UPDATE_INTERVAL_SECS * __getTicksPerSecond();
    }

    clock_t clockNow = __getTickCount();
    clock_t clockElapsed = clockNow - p_modeData->lastClock;

    // Accommodate initial negative values or wrap-around of tick clock_t values
    if ((clockElapsed >= s_watchdogClockDelta)
        || (clockElapsed <= -s_watchdogClockDelta))
    {
        char buf[16];   // sufficient for 32-bits in unsigned decimal (NUL terminated)
        uint32_t nextWatchdog = p_modeData->lastWatchdog + 1;

        snprintf(buf, sizeof(buf), "%u", (unsigned)nextWatchdog);

        const char *p_rdbName = p_modeData->p_watchdogName;
        int stat = rdb_set_single(p_rdbName, buf);

        if ((stat < 0) && (errno == ENOENT)) {
            stat = rdb_create_variable(p_rdbName, buf, 0, DEFAULT_PERM, 0, 0);
        }
        if (stat < 0) {
            syslog(LOG_ERR, "failed to set %s to %s - %s (%d)", p_rdbName, buf, strerror(errno), errno);
        }
        else {
            syslog(LOG_DEBUG, "set %s to %s", p_rdbName, buf);
            p_modeData->lastClock = clockNow;
            p_modeData->lastWatchdog = nextWatchdog;
        }
    }
}

