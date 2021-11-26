#include "dbenum.h"

#include <syslog.h>
#include <stdlib.h>
#include <string.h>

#include <asm/errno.h>

#include "rdb_ops.h"



/**
 * destroys a database name enumerator object
 * \param pEnum
 */
void dbenum_destroy(dbenum* pEnum)
{
	if (!pEnum)
		return;

	growingmem_destroy(pEnum->pDbNameStrMem);
	growingmem_destroy(pEnum->pDbNameArrayMem);

	free(pEnum);
}

/**
 * creates a database name enumerator object
 * \return The result value is an oboject pointer if it succeeds. Otherwise, it returns NULL
 */
dbenum* dbenum_create(int nFlags)
{
	dbenum* pEnum = NULL;

	if (!(pEnum = malloc(sizeof(*pEnum))))
		goto error;

	memset(pEnum, 0, sizeof(*pEnum));

	if (!(pEnum->pDbNameStrMem = growingmem_create(DBENUM_GROWING_NAME_BUFFER)))
		goto error;

	if (!(pEnum->pDbNameArrayMem = growingmem_create(DBENUM_GROWING_NAME_COUNT * sizeof(dbenumitem))))
		goto error;

	pEnum->nFlags = nFlags;

	return pEnum;

error:
	dbenum_destroy(pEnum);
	return NULL;
}

// seperators for database getnames
static const char _szSeperators[] = ";&";


dbenumitem* dbenum_findNext(dbenum* pEnum)
{
	dbenumitem* pVarEl = (dbenumitem*)growingmem_getMem(pEnum->pDbNameArrayMem);

	if (pEnum->iCurSearch < pEnum->cDbNameArray)
		return &pVarEl[pEnum->iCurSearch++];

	return NULL;
}

dbenumitem* dbenum_findFirst(dbenum* pEnum)
{
	pEnum->iCurSearch = 0;
	return dbenum_findNext(pEnum);
}

dbenumitem* dbenum_getItem(dbenum* pEnum, const char* szVarName)
{
	int i;
	char* szStr;

	dbenumitem* pVarEl = (dbenumitem*)growingmem_getMem(pEnum->pDbNameArrayMem);

	for (i = 0;i < pEnum->cDbNameArray;i++)
	{
		if ((szStr = pVarEl[i].szName) != NULL)
		{
			if (!strcasecmp(szStr, szVarName))
				return &pVarEl[i];
		}
	}

	return 0;
}

/**
 * seperate a source string to a string array by using seperators
 * \param pEnum pointer to a database enumerator object
 * \return the return value is the number of string that the function copies to the destination
 */
int dbenum_chopToArray(dbenum* pEnum, BOOL fCountOnly)
{
	int cVariable = 0;
	int ch;
	int cbName;

	char* pPtr;

	dbenumitem* pVarEl = (dbenumitem*)growingmem_getMem(pEnum->pDbNameArrayMem);
	char* szDbNameStr = (char*)growingmem_getMem(pEnum->pDbNameStrMem);

	if (!fCountOnly)
		memset(pVarEl, 0, pEnum->pDbNameArrayMem->cbMem);

	cbName = 0;
	pPtr = szDbNameStr;

	BOOL fReadOnly;

	while (TRUE)
	{
		ch = *szDbNameStr;

		// if seperator
		if (ch == 0 || strchr(_szSeperators, ch))
		{
			// do not write if read only
			fReadOnly = fCountOnly || !(cVariable < pEnum->cDbNameArray);

			// set NULL termination in the seperator
			if (!fReadOnly)
				*szDbNameStr = 0;

			// store if not empty variable name
			if (cbName)
			{
				if (!fReadOnly)
				{
					pVarEl->szName = pPtr;
					pVarEl++;
				}
				cVariable++;
			}

			// reset byte count for a variable name
			cbName = 0;
			// keep the next pointer as a start pointer to the variable name
			pPtr = szDbNameStr + 1;
		}
		else
		{
			cbName++;
		}

		szDbNameStr++;

		if (ch == 0)
			break;
	}

	return cVariable;
}

/**
 * get a string count that can be seperated by seperators
 * \return the return value is total number of strings that can be seperated by seperators
 */
int dbenum_getVarCount(dbenum* pEnum)
{
	return dbenum_chopToArray(pEnum, TRUE);
}


/**
 * get all the database persistant variable names and store in a string array
 * \param pEnum pointer to a database enumerator object
 * \return the return value is total number of persistant variables in the array
 */
int dbenum_enumDb(dbenum* pEnum)
{
	int stat;
	// get length of persistant variables
	int cbLen = 0;

	stat = rdb_get_names("", NULL, &cbLen, pEnum->nFlags);

	// bypass if no entry
	if (stat == -ENOENT)
	{
		pEnum->cDbNameArray = 0;
		return 0;
	}

	if (stat != -EOVERFLOW)
	{
		syslog(LOG_ERR, "failed to enumerate persistant variables (stat=%d)", stat);
		return -1;
	}

	// reallocate if bigger than before
	if (!growingmem_growIfNeeded(pEnum->pDbNameStrMem, cbLen + 1))
	{
		syslog(LOG_ERR, "failed to allocate memory for persistant variable names (cbLen=%d)", cbLen);
		return -1;
	}

	// get persistant variable names
	char* szDbNameStr = (char*)growingmem_getMem(pEnum->pDbNameStrMem);
	stat = rdb_get_names("", szDbNameStr, &cbLen, pEnum->nFlags);

	if (cbLen < 0 || pEnum->pDbNameStrMem->cbMem <= cbLen)
		cbLen = 0;
	szDbNameStr[cbLen] = 0;

	syslog(LOG_DEBUG, "database search result : stat=%d,flags=0x%04x,cbLen=%d,str=%s", stat, pEnum->nFlags, cbLen, szDbNameStr);

	// get count of variable names
	pEnum->cDbNameArray = 0;
	int cNames = dbenum_getVarCount(pEnum);
	if (!growingmem_growIfNeeded(pEnum->pDbNameArrayMem, cNames*sizeof(dbenumitem)))
	{
		syslog(LOG_ERR, "failed to allocate memory for persistant variable name array (cName=%d)", cNames);
		return -1;
	}

	pEnum->cDbNameArray = cNames;


	// convert database variable names to array
	return dbenum_chopToArray(pEnum, FALSE);
}


