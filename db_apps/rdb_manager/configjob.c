
#include "configjob.h"

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <sys/times.h>
#include <time.h>
#include <errno.h>
#include <libgen.h>
#include <linux/limits.h>

#include "base.h"

#include "dbenum.h"
#include "textedit.h"
#include "rdb_ops.h"
#include "DD_ioctl.h"


#define STRLEN(str)	(sizeof(str)-1)

static char rdb_value_buf[RDBMANAGER_DATABASE_VARIABLE_LENGTH];
static char rdb_name_buf[MAX_NAME_LENGTH];

static const char szCfgWriteCntHdr[]="# Total write count : ";

static dbenum* _pEnum = NULL;
int _fCfgChg = 0;
extern int _fPatched;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int saveDbToTextEdit(int nDefFlags)
{
	int nUser;
	int nGroup;
	int nPerm;
	int nFlags;
	int ret;

	texteditline* pLine;
	texteditline* pNextLine = textedit_findFirst();
	int iLine = 0;

	while ((pLine = pNextLine) != 0)
	{
		iLine++;
		pNextLine = textedit_findNext();

		if (!pLine->fComment)
		{
			if (readCfgFormat(pLine, rdb_name_buf, &nUser, &nGroup, &nPerm, &nFlags, rdb_value_buf ,sizeof(rdb_value_buf)) >= 0)
			{
				// take PERSIST if not existing
				if (!nFlags)
					nFlags = nDefFlags;

				if ((ret = rdb_get_single(rdb_name_buf, rdb_value_buf, sizeof(rdb_value_buf))) < 0)
				{
					textedit_deleteLine(pLine);
					syslog(LOG_DEBUG, "line %d removed for the deleted variable (%s) because of %s.", iLine, rdb_name_buf, strerror(errno));
				}
				else
				{
					writeCfgFormat(pLine, rdb_name_buf, nUser, nGroup, nPerm, nFlags, rdb_value_buf);
				}
			}
		}
	}

	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int saveTextEditToDb(int nFlags)
{
	const char* szCfgHdr = "# NetComm Configuration written by ";

	int nUser;
	int nGroup;
	int nPerm;
	int nCfgFlags;

	int stat;

	texteditline* pLineDate = NULL;
	texteditline* pLineWriteCount = NULL;

	texteditline* pLine;
	texteditline* pNextLine = textedit_findFirst();
	int iLine = 0;

	char* pContent;

	// lock
	#ifndef DEBUG
	if (rdb_database_lock(0) < 0)
		syslog(LOG_CRIT, "failed to lock database");
	#endif

	while ((pLine = pNextLine) != 0)
	{
		iLine++;

		pNextLine = textedit_findNext();

		pContent=textedit_get_line_ptr(pLine);

		if(!pContent) {
			syslog(LOG_ERR,"NULL content pointer detected (line=%d)",iLine);
			continue;
		}
		// do nothing if empty line
		if (!strlen(pContent))
		{
			pLine->fComment = 1;
			continue;
		}
		// remeber the line if it is header
		if (!strncmp(pContent, szCfgHdr, strlen(szCfgHdr)))
		{
			pLine->fComment = 1;
			pLineDate = pLine;
			continue;
		}
		if (!strncmp(pContent, szCfgWriteCntHdr, STRLEN(szCfgWriteCntHdr))) {
			pLine->fComment = 1;
			pLineWriteCount = pLine;
			continue;
		}
		// do nothing if comment line
		else if (*pContent == '#')
		{
			pLine->fComment = 1;
			continue;
		}

		// read the line and write it into database
		if (readCfgFormat(pLine, rdb_name_buf, &nUser, &nGroup, &nPerm, &nCfgFlags, rdb_value_buf, sizeof(rdb_value_buf)) < 0)
		{
			syslog(LOG_ERR, "commented out for having an error (line=%d)", iLine);

			char achBuf[TEXTEDIT_MAX_LINE_LENGTH];

			strcpy(achBuf, pContent);
			textedit_sprintf_line(pLine,"# error line : %s", achBuf);

			pLine->fComment = 1;
			continue;
		}

		pLine->fComment = 0;
		// create the variable in database
		if ((stat = rdb_create_variable(rdb_name_buf, rdb_value_buf, nCfgFlags, nPerm, nUser, nGroup)) < 0)
		{
			//syslog(LOG_INFO, "failed to create variable (name=%s,flags=0x%04x,stat=%d) - %s", rdb_name_buf, nFlags, stat,strerror(errno));
			/* Update the existing RDB with the new attributes.
			 * This makes it possible to let the new definition override the former.
			*/
			if ((stat = rdb_set_single_privilege(rdb_name_buf, rdb_value_buf, nCfgFlags, nPerm, nUser, nGroup)) < 0)
			{
				syslog(LOG_ERR, "failed to create and update database variable (line=%d,name=%s,stat=%d)", iLine, rdb_name_buf, stat);
			}
		}
		else
		{
			syslog(LOG_DEBUG, "varaible created (line=%d,name=%s,flags=0x%04x", iLine, rdb_name_buf, nFlags);
		}
	}

	#ifndef DEBUG
	// unlock
	if (rdb_database_unlock() < 0)
		syslog(LOG_CRIT, "failed to unlock database");
	#endif

	//////////////////////////////////////////////////
	// put version information in textedit memory
	//////////////////////////////////////////////////

	// add date line if it doesn't exist
	if (!pLineDate)
	{
		pLineDate = textedit_addHead();
		pLineDate->fComment = 1;

		iLine++;

		// tick the change flag
		_fCfgChg = 1;
	}

	// get current time
	char achDate[256];
	time_t timeCur = time(0);
	ctime_r(&timeCur, achDate);

	// clear carriage return
	achDate[strlen(achDate)-1] = 0;

	// put a new config date
	// Iwo: I have switched the date stamp off, as it is often the only part
	// of the config file that changes. This upsets the config backup mechanism.
	//sprintf(pLineDate->achContent, "%s version %d.%d.%d at %s", szCfgHdr, VER_MJ, VER_MN, VER_BLD, achDate);
	textedit_sprintf_line(pLineDate,"%s version %d.%d.%d", szCfgHdr, VER_MJ, VER_MN, VER_BLD);

	/* add write count */
	if(!pLineWriteCount) {
		pLineWriteCount = textedit_addNext(pLineDate);
		pLineWriteCount->fComment = 1;
		iLine++;
	}
	
	textedit_sprintf_line(pLineWriteCount,"%s%u", szCfgWriteCntHdr,1);

	syslog(LOG_DEBUG, "total %d line(s) processed to save to database", iLine);

	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int textedit_save_callback(int nLine,texteditline* pLine)
{
	char* pContent;
	unsigned int nWriteCnt;
	
	pContent=textedit_get_line_ptr(pLine);

	/* bypass if null content */
	if(!pContent)
		return -1;

	/* bypass if not header */
	if (strncmp(pContent, szCfgWriteCntHdr, STRLEN(szCfgWriteCntHdr)))
		return 0;
	
	nWriteCnt=(unsigned int)atoll(pContent+STRLEN(szCfgWriteCntHdr));
	nWriteCnt++;
	textedit_sprintf_line(pLine,"%s%u", szCfgWriteCntHdr,nWriteCnt);
	
	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int saveTextEditToFile(const char* szCur, const char* szNew)
{
	// save
	if (!textedit_save(szNew,textedit_save_callback))
	{
		syslog(LOG_CRIT, "failed to write a new config file (name=%s)", szNew);
	}
	else
	{
		syslog(LOG_DEBUG, "%s saved", szNew);

		// copy
		sync();
		if (rename(szNew, szCur) < 0)
		{
			syslog(LOG_CRIT, "filed to rename to running config (name=%s) - %s", szCur,strerror(errno));
		}
		else
		{
			syslog(LOG_DEBUG, "renamed to %s", szCur);
		}

		syslog(LOG_DEBUG, "working configuration file updated");
	}

	_fCfgChg = 0;

	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int updatePersisDbInTextEdit(int fSubscribe)
{
	int nFlags;
	int nUser;
	int nGroup;
	int nPerm;

	int cbVarLen;
	int stat;

	dbenumitem* pItem;

	////////////////////////////////////////////////
	// check new persistant variables in database //
	////////////////////////////////////////////////

	// get all persistant names from database
	int cVariable;
	if ((cVariable = dbenum_enumDb(_pEnum)) < 0)
		syslog(LOG_CRIT, "failed to get persistant variable names from databaase driver");

	syslog(LOG_DEBUG, "total %d variable(s) found", cVariable);

	pItem = dbenum_findFirst(_pEnum);
	while (pItem)
	{
		syslog(LOG_DEBUG, "database varible %s found", pItem->szName);
		pItem = dbenum_findNext(_pEnum);
	}

	/////////////////////////////
	// delete duplicated lines //
	/////////////////////////////

	texteditline* pLine;
	texteditline* pNextLine = textedit_findFirst();
	int iLine = 0;


	while ((pLine = pNextLine) != 0)
	{
		iLine++;

		pNextLine = textedit_findNext();

		if (pLine->fComment)
			continue;

		if (readCfgFormat(pLine, rdb_name_buf, &nUser, &nGroup, &nPerm, NULL, rdb_value_buf,sizeof(rdb_value_buf)) < 0)
		{
			syslog(LOG_ERR, "cannot parse line %d", iLine);
			continue;
		}

		if (NULL == (pItem = dbenum_getItem(_pEnum , rdb_name_buf)))
		{
			syslog(LOG_DEBUG, "variable %s might have the wrong flag. flag 0x%08x needed", rdb_name_buf, _pEnum->nFlags);
			continue;
		}

		texteditline* pDupLine = pItem->pUserPtr;

		if (pDupLine)
		{
			textedit_sprintf_line(pDupLine,"%s",textedit_get_line_ptr(pLine));
			textedit_deleteLine(pLine);

			syslog(LOG_INFO, "duplicated line detected (line=%d,name=%s)", iLine, rdb_name_buf);

			_fCfgChg = 1;
		}
		else
		{
			pItem->pUserPtr = pLine;

			if (fSubscribe)
			{
				syslog(LOG_DEBUG, "registering notification (name=%s)", rdb_name_buf);
				rdb_subscribe_variable(rdb_name_buf);
			}
		}
	}

	int cNewLine = 0;

	pItem = dbenum_findFirst(_pEnum);

	while (pItem)
	{
		if (!pItem->pUserPtr)
		{
			cbVarLen = 0;
			// get info
			stat = rdb_get_info(pItem->szName, &cbVarLen, &nFlags, &nPerm, &nUser, &nGroup);
			if (stat == -EOVERFLOW)
			{
				if(cbVarLen > sizeof(rdb_value_buf))
				{
					syslog(LOG_ERR, "failed to get database variable (name=%s) since its length(%d) is greater than %d", pItem->szName, cbVarLen, sizeof(rdb_value_buf));
				}
				else
				{
					if ((stat = rdb_get_single(pItem->szName, rdb_value_buf, sizeof(rdb_value_buf) - 1)) < 0)
					{
						syslog(LOG_ERR, "failed to get database variable (name=%s, Error: %s)", pItem->szName, strerror(errno));
					}
					else
					{
						texteditline* pAddLine = textedit_addTail();

						writeCfgFormat(pAddLine, pItem->szName, nUser, nGroup, nPerm, nFlags, rdb_value_buf);
						syslog(LOG_INFO, "new line added (name=%s,value=%s)", pItem->szName, rdb_value_buf);

						// tick the change flag
						_fCfgChg = 1;

						syslog(LOG_DEBUG, "registering notification (name=%s)", pItem->szName);
						// subscribe item
						rdb_subscribe_variable(pItem->szName);

						cNewLine++;
					}
				}
			}
			else
			{
				syslog(LOG_DEBUG, "failed to get database variable information (name=%s, Error: %s)", pItem->szName, strerror(errno));
			}
		}

		pItem = dbenum_findNext(_pEnum);
	}

	if (cNewLine == 0)
		syslog(LOG_DEBUG, "no new persistant variable found in database");

	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int loadTextEditFromFile(const char* szCfg)
{
	int stat;

	textedit_unload();
	stat = textedit_load(szCfg);

	// load configuration file into memory
	if (stat < 0)
		syslog(LOG_CRIT, "failed to read Default configuration file (name=%s)", szCfg);
	else
		syslog(LOG_DEBUG, "configuration file loaded (name=%s)", szCfg);

	return stat;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int doConfigJobPrepare(const char* szCur, const char* szNew, const char* szDef, const char* szAdd, int nFlags, int nSaveDelay)
{
	int stat = -1;

	if (rdb_open_db() < 0)
	{
		syslog(LOG_ERR, "can't open device");
		goto fini2;
	}

	// create directory structure if needed
	char achDir[PATH_MAX];
	strcpy(achDir,RDBMANAGER_CFG_FILENAME_CUR);

	char* szConfDir=dirname(achDir);
	if(!__isExisting(szConfDir))
	{
		if(__mkdirall(szConfDir)<0)
			syslog(LOG_CRIT, "failed to mkdir %s",szConfDir);
	}

	// create an enumerator
	if (NULL == (_pEnum = dbenum_create(nFlags << 16 | nFlags)))
	{
		syslog(LOG_CRIT, "failed to create an enumerator for persistant database variable names");
		goto fini;
	}


	// load text edit from disk
	if (szDef)
	{
		syslog(LOG_DEBUG, "loading text edit from default file");
		loadTextEditFromFile(szDef);

		// save text edit to db
		syslog(LOG_DEBUG, "writing configuration into database");
		saveTextEditToDb(nFlags);
	}


	// load text edit from disk
	if (szCur)
	{
		syslog(LOG_DEBUG, "loading text edit from current file");
		loadTextEditFromFile(szCur);

		// save text edit to db
		syslog(LOG_DEBUG, "writing configuration into database");
		saveTextEditToDb(nFlags);
	}

	if (szAdd)
	{
		syslog(LOG_DEBUG, "loading text edit from addon file");
		loadTextEditFromFile(szAdd);

		// save text edit to db
		syslog(LOG_DEBUG, "writing addon configuration into database");
		saveTextEditToDb(nFlags);
	}

	return 0;

fini:
	textedit_unload();
	dbenum_destroy(_pEnum);

	// close database
	rdb_close_db();

fini2:
	return stat;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void _dump_triggered_vars() {
	char *buf = NULL, *sp, *vn, *vns;
	int size = 1024, len, ret;

	do {
		if(!(buf = realloc(buf, size))) {
			syslog(LOG_ERR, "realloc(): %s\n", strerror(errno));
			return;
		}
		memset(buf, 0, size);
		len = size - 1;
		errno = 0;
		ret = rdb_get_names("", buf, &len, TRIGGERED);
		buf[len] = 0;
		size <<= 1;
	} while(ret && errno == EOVERFLOW);

	syslog(LOG_DEBUG, "triggered variables:");
	vns = buf;
	sp = NULL;
	while((vn = strtok_r(vns, "&", &sp))) {
		syslog(LOG_DEBUG, "%s", vn);
		vns = NULL;
	}
	syslog(LOG_DEBUG, "\n");

	free(buf);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int doConfigJob(const char* szCur, const char* szNew, const char* szDef, int nFlags, unsigned int nSaveDelay, RdbMgrMode mode)
{
	int stat = -1;


	////////////////////////////////////////////////
	// starting select loop
	////////////////////////////////////////////////
	clock_t tickPerSec = __getTicksPerSecond();
	clock_t tickUpdate=0;
	clock_t tickTrigger;
	int fdDb = rdb_get_fd();

	int fSubscribe = 1;
	int force_to_save = 0;
	int prev_cfg_chg = 0;

	tickTrigger = __getTickCount();

	while (1)
	{
		clock_t tickCur = __getTickCount();

		// search all persistance variables in database to add
		if (g_fSigRefresh || fSubscribe || !tickUpdate || ( (tickCur - tickUpdate) / tickPerSec >= RDBMANAGER_DATABASE_READ_FREQ) )
		{
			syslog(LOG_DEBUG, "searching for persistant variables in database");
			updatePersisDbInTextEdit(fSubscribe);

			// runs once only at most during the first iteration of the loop
			// force to immediately update the file with RDB variables that have
			// been patched (which takes place to convert different RDB
			// names used in different versions of firmware)
			if (_fPatched)
			{
				/* set force flag and change flag to force to write */
				force_to_save = _fCfgChg = 1;
				_fPatched = 0;
			}

			tickUpdate = tickCur;
			fSubscribe = 0;

			g_fSigRefresh=0;
		}

		/* detect raising edge */
		if(!prev_cfg_chg && _fCfgChg)
			tickTrigger = __getTickCount();
		prev_cfg_chg = _fCfgChg;
		
		// save
		if (_fCfgChg){
			if (force_to_save || (tickCur - tickTrigger) / tickPerSec >= nSaveDelay ) {
				
				syslog(LOG_DEBUG, "update config file (fname=%s,force_to_save=%d,nSaveDelay=%d)",szCur,force_to_save,nSaveDelay);
				saveTextEditToFile(szCur, szNew);

				/* suspend write-back for another delay period */
				tickTrigger = __getTickCount();

				/* clear force flag */
				force_to_save=0;
			}
		}

		updateRdbWatchdog(mode);

		// prepare for selecting
		fd_set fdsetR;
		FD_ZERO(&fdsetR);
		FD_SET(fdDb, &fdsetR);
		struct timeval tv = {RDBMANAGER_SELECT_TIMEOUT, 0};

		// select
		errno = 0;
		stat = select(fdDb + 1, &fdsetR, 0, 0, &tv);

		// terminate gracefully if signal detected
		if (g_fSigTerm)
		{
			syslog(LOG_ERR, "signal detected");
			break;
		}

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
		else if (stat == 0)
		{
			syslog(LOG_DEBUG, "select timeout");
		}
		else
		{
			syslog(LOG_DEBUG, "database notification detected (stat=%d, errno=%d, err=%s)", stat, errno, strerror(errno));

			_dump_triggered_vars();

			saveDbToTextEdit(nFlags);
			_fCfgChg = 1;
		}
	}

	syslog(LOG_DEBUG, "searching for persistant variables in database");
	updatePersisDbInTextEdit(FALSE);

	// save
	if (_fCfgChg)
	{
		syslog(LOG_DEBUG, "update config file (fname=%s)",szCur);

		saveTextEditToFile(szCur, szNew);
	}

	stat = 0;

	textedit_unload();
	dbenum_destroy(_pEnum);

	// close database
	//rdb_close_db();

	return stat;
}
