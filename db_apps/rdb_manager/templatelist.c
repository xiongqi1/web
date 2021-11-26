#include "templatelist.h"

#include <stdlib.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sched.h>

#include "rdb_ops.h"
#include "templatemgr.h"

///////////////////////////////////////////////////////////////////////////////
void templatelist_destroy(templatelist* pL)
{
	if (!pL)
		return;

	objectlist_destroy(pL->pObjListMgr);
	dbenum_destroy(pL->pEnum);

	free(pL);
}

///////////////////////////////////////////////////////////////////////////////
templatelist* templatelist_create(void)
{
	templatelist* pL;

	// create instance
	pL = malloc(sizeof(*pL));
	if (!pL)
	{
		syslog(LOG_ERR, "failed to allocate for templatelist");
		goto error;
	}

	// create objectlist for templatemgr
	pL->pObjListMgr = objectlist_create();
	if (!pL->pObjListMgr)
	{
		syslog(LOG_ERR, "failed to create an object list for template managers");
		goto error;
	}

	pL->pEnum = dbenum_create(TRIGGERED);
	if (!pL->pEnum)
	{
		syslog(LOG_ERR, "failed to create an enumerator for notification");
		goto error;
	}

	return pL;

error:
	templatelist_destroy(pL);
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
int templatelist_procNotification(templatelist* pL, int fInit)
{
	char achValue[MAX_VALUE_LEN_RDB]; /* buffer for RDB values */

	///////////////////////////////
	// clear ticks
	///////////////////////////////
	templatemgr* pT = (templatemgr*)objectlist_findFirst(pL->pObjListMgr);
	while (pT)
	{
		pT->fUserTick = 0;
		pT = (templatemgr*)objectlist_findNext(pL->pObjListMgr);
	}

	//////////////////////////////////////
	// enumerate notification variables
	//////////////////////////////////////

	// get notification name list from database
	dbenum_enumDb(pL->pEnum);

	//////////////////////////////////////
	// tick template manager to launch
	//////////////////////////////////////

	dbenumitem* pE = dbenum_findFirst(pL->pEnum);
	while (pE)
	{
		const char* szVariable = pE->szName;

		templatemgr* pNextT = (templatemgr*)objectlist_findFirst(pL->pObjListMgr);
		while ((pT = pNextT) != NULL)
		{
			pNextT = (templatemgr*)objectlist_findNext(pL->pObjListMgr);
			if (pT->fUserTick)
				continue;

			if (templatemgr_isIncluded(pT, szVariable) < 0)
				continue;

			/* immediately clear triggered notification */
			if (rdb_get_single(szVariable, achValue, sizeof(achValue)) < 0) {
				syslog(LOG_ERR, "failed to clear notification in variable '%s'", szVariable);
			}

			pT->fUserTick = 1;
		}

		pE = dbenum_findNext(pL->pEnum);
	}

	//////////////////////////////////////
	// execute template manager
	//////////////////////////////////////
	templatemgr* pNextT = (templatemgr*)objectlist_findFirst(pL->pObjListMgr);
	while ((pT = pNextT) != NULL)
	{
		pNextT = (templatemgr*)objectlist_findNext(pL->pObjListMgr);

		if (!fInit && !pT->fUserTick)
			continue;

		if (templatemgr_buildConf(pT) < 0)
		{
			syslog(LOG_ERR, "failed to parse template file %s", pT->achTemplate);
			continue;
		}

		syslog(LOG_DEBUG, "launching template manager script %s", pT->achScriptMgr);
		if (templatemgr_execMgr(pT,fInit) < 0)
		{
			syslog(LOG_ERR, "template %s script returned failure %s", pT->achTemplate, pT->achScriptMgr);
		}

		if (tempaltemgr_unlinkConf(pT) < 0)
		{
			syslog(LOG_CRIT, "failed to unlink %s", pT->achOutput);
		}

		syslog(LOG_DEBUG, "generated configuration file %s unlinked", pT->achOutput);
	}


	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int templatelist_subscribteTemplateVaribles(templatelist* pL)
{
	objectlist* pObjListV = objectlist_create();
	if (!pObjListV)
		return -1;

	// foreach in templates
	templatemgr* pNextT = objectlist_findFirst(pL->pObjListMgr);
	templatemgr* pT;
	while ((pT = pNextT) != NULL)
	{
		pNextT = objectlist_findNext(pL->pObjListMgr);

		// foreach in variables
		char* szN = objectlist_findFirst(pT->pObjListVariables);
		char* szVariable;
		while ((szVariable = szN) != NULL)
		{
			szN = objectlist_findNext(pT->pObjListVariables);

			// don't subscribe if already done
			if (objectlist_lookUp(pObjListV, szVariable, objectlist_compareString) != NULL)
				continue;


			int stat;

			// try to subscribe
			stat = rdb_subscribe_variable(szVariable);
			if (stat < 0)
			{
				syslog(LOG_INFO, "unable to subscribe variable %s. trying to create (stat=%d)", szVariable, stat);
				if ((stat = rdb_create_variable(szVariable, NULL, 0, DEFAULT_PERM, 0, 0)) < 0)
				{
					syslog(LOG_CRIT, "failed to create (variable=%s,stat=%d)", szVariable, stat);
					continue;
				}

				stat = rdb_subscribe_variable(szVariable);
				if (stat < 0)
				{
					syslog(LOG_CRIT, "failed to subscribte (variable=%s,stat=%d)", szVariable, stat);
				}
			}

			// add the added variable in V list
			objectlist_addObj(pObjListV, szVariable, objectlist_destructorNull);
			syslog(LOG_DEBUG, "variable %s subscribed", szVariable);
		}
	}

	objectlist_destroy(pObjListV);

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int templatelist_buildTemplates(templatelist* pL)
{
	templatemgr* pT = objectlist_findFirst(pL->pObjListMgr);
	while (pT)
	{
		if (templatemgr_buildVaribles(pT) < 0)
			syslog(LOG_ERR, "failed to gether variables in use by template %s", pT->achTemplate);

		pT = objectlist_findNext(pL->pObjListMgr);
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int templatelist_loadTemplates(templatelist* pL)
{
	// scan template directory
	struct dirent **ppNameList;
	/* use alphasort() argument to give a priority in template file calling order */
	int cNameList = scandir(TEMPLATELIST_DIR_TEMPLATES, &ppNameList, NULL, alphasort);
	if (cNameList < 0)
	{
		syslog(LOG_ERR, "failed to enumerate template directory (name=%s)", TEMPLATELIST_DIR_TEMPLATES);
		return -1;
	}

	int cTemplates = 0;

	///////////////////////////////////////////////
	// create template manager
	///////////////////////////////////////////////

	// go through all names
	int iName;
	for (iName = 0;iName < cNameList;iName++)
	{
		const char* szName = ppNameList[iName]->d_name;

		char szFullName[PATH_MAX];
		sprintf(szFullName, "%s/%s", TEMPLATELIST_DIR_TEMPLATES, szName);

		// bypass if directory
		DIR* pDir = opendir(szFullName);
		if (pDir)
		{
			closedir(pDir);

			syslog(LOG_DEBUG, "directory %s ignored in Template folder", szFullName);

			continue;
		}

		// bypass if not accessible
		if (access(szFullName, R_OK) < 0)
		{
			syslog(LOG_ERR, "template %s not accessible", szFullName);
			continue;
		}


		// get template file extention
		char szOrgName[PATH_MAX];
		strcpy(szOrgName, szName);

		int iExt;
		iExt = strlen(szOrgName) - strlen(TEMPLATELIST_FILE_EXT);
		if (iExt < 0)
		{
			syslog(LOG_ERR, "template %s ignored for not having a proper extention", szFullName);
			continue;
		}

		char* pExt = szOrgName + iExt;
		if (strcasecmp(pExt, TEMPLATELIST_FILE_EXT))
		{
			syslog(LOG_ERR, "template %s ignored for not having a proper extention", szFullName);
			continue;
		}

		// cut the file extention
		*pExt = 0;

		// get a corresponding manager script
		char szScript[PATH_MAX];
		sprintf(szScript, "%s/%s%s", TEMPLATELIST_DIR_SCRIPTS, szOrgName, TEMPLATELIST_POSTFIX_SCRIPTS);
		char* pScript;

		pScript = szScript;

		// bypass if not accessible
		if (!access(szFullName, X_OK))
		{
			pScript = NULL;
		}
		// bypass if the script not executable
		else if (access(szScript, X_OK) < 0)
		{
			syslog(LOG_ERR, "template %s ignored by not existing of a corresponding manager script %s", szName, szScript);
			continue;
		}

		// create a template manager
		templatemgr* pT = templatemgr_create(szFullName, pScript);
		if (!pT)
		{
			syslog(LOG_ERR, "failed to create a new template manager for %s", szFullName);
			continue;
		}

		objectlistelement* pE = objectlist_addObj(pL->pObjListMgr, pT, (objectlist_destructor*)templatemgr_destroy);
		if (!pE)
		{
			templatemgr_destroy(pT);

			syslog(LOG_ERR, "failed to add a new template manager object in the list %s", szFullName);
			continue;
		}

		cTemplates++;

		syslog(LOG_INFO, "template %s added with manager script %s", szFullName, szScript);
	}

	syslog(LOG_INFO, "total %d template(s) taken", cTemplates);

	free(ppNameList);


	return 0;
}
