
#include <sys/select.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

#include "templatelist.h"
#include "rdb_ops.h"

///////////////////////////////////////////////////////////////////////////////
int doTemplateJob(void)
{
	int stat = -1;

	templatelist* pL = NULL;

	dbenum* pDbEnumTrigger = dbenum_create(TRIGGERED);

	// open database
	if (rdb_open_db() < 0)
	{
		syslog(LOG_ERR, "failed to open database driver");
		goto fini;
	}

	// clear ready state
	setRdbReady(RDB_MANAGER_MODE_TEMPLATE, 0);

	// create template list
	pL = templatelist_create();
	if (!pL)
	{
		syslog(LOG_ERR, "failed to create template list");
		goto fini;
	}

	// load templates
	if (templatelist_loadTemplates(pL) > 0)
	{
		syslog(LOG_ERR, "failed to load templates");
		goto fini;
	}

	// build template variables
	if (templatelist_buildTemplates(pL) < 0)
	{
		syslog(LOG_ERR, "failed to build tmeplate variables");
		goto fini;
	}

	// subscribe notifications
	if (templatelist_subscribteTemplateVaribles(pL) < 0)
	{
		syslog(LOG_ERR, "failed to subscribe variables");
		goto fini;
	}

	// set ready state
	setRdbReady(RDB_MANAGER_MODE_TEMPLATE, 1);

	int fdDb = rdb_get_fd();
	syslog(LOG_DEBUG, "initial template execution");

	// ru template
	templatelist_procNotification(pL, 1);

	syslog(LOG_DEBUG, "going to select loop");

	while (1)
	{
		updateRdbWatchdog(RDB_MANAGER_MODE_TEMPLATE);

		struct timeval tv = {1, 0};

		fd_set fdsetR;
		FD_ZERO(&fdsetR);
		FD_SET(fdDb, &fdsetR);

		int stat = select(fdDb + 1, &fdsetR, NULL, NULL, &tv);

		if (g_fSigTerm)
			break;

		if (stat < 0) {
			if(errno==EINTR) {
				syslog(LOG_ERR, "signal was caught");
			}
			else {
				syslog(LOG_ERR, "unknown error occured from select (stat=%d) - %s", stat,strerror(errno));
				break;
			}
		}

		if (stat == 0)
			continue;

		syslog(LOG_DEBUG, "notification detected (stat=%d)", stat);

		if (templatelist_procNotification(pL, 0) < 0)
			syslog(LOG_ERR, "failed to process database notification");

		// check notification left-ver
		int cLeftover = dbenum_enumDb(pDbEnumTrigger);
		syslog(LOG_DEBUG, "left-over notified variable count - %d", cLeftover);
		dbenumitem* pItem = dbenum_findFirst(pDbEnumTrigger);
		while (pItem)
		{
			syslog(LOG_DEBUG, "triggers left over - %s", pItem->szName);
			pItem = dbenum_findNext(pDbEnumTrigger);
		}
	}

	stat = 0;

fini:
	templatelist_destroy(pL);

	dbenum_destroy(pDbEnumTrigger);

	if (rdb_get_fd() >= 0) {
		setRdbReady(RDB_MANAGER_MODE_TEMPLATE, 0);
		rdb_close_db();
	}

	return stat;
}
