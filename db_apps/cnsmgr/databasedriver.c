#include "databasedriver.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* databasedriver_getKey(databasedriver* pDb, const char* szKey, const char* szPrefix)
{
	static char achKey[MAX_NAME_LENGTH]={0,};

	if(szPrefix)
		strcpy(achKey, szPrefix);
	else
		strcpy(achKey, pDb->achKeyPrefix);

	strcat(achKey, szKey);

	return achKey;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void databasedriver_destroy(databasedriver* pDb)
{
	__bypassIfNull(pDb);

	databasedriver_closeSession(pDb);

	__free(pDb);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
databasedriver* databasedriver_create(void)
{
	databasedriver* pDb;

	// allocate object
	__goToErrorIfFalse(pDb = __allocObj(databasedriver));

	pDb->achKeyPrefix[0] = 0;

	return pDb;

error:
	databasedriver_destroy(pDb);
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedriver_setKeyPrefix(databasedriver* pDb, const char* szKeyPrefix)
{
	strcpy(pDb->achKeyPrefix, szKeyPrefix);

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedriver_cmdSetNotify(databasedriver* pDb, const char* szKey,const char* szPrefix)
{
	const char* szFullKey=databasedriver_getKey(pDb,szKey,szPrefix);

	return rdb_subscribe_variable(szFullKey)>=0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedriver_cmdGetLength(databasedriver* pDb, const char* szKey, int* pcbValue,const char* szPrefix)
{
	const char* szFullKey=databasedriver_getKey(pDb,szKey,szPrefix);

	rdb_get_info(szFullKey,pcbValue,0,0,0,0);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedriver_cmdGetSingle(databasedriver* pDb, const char* szKey, char* szValue, int* pcbValue,const char* szPrefix)
{
	const char* szFullKey=databasedriver_getKey(pDb,szKey,szPrefix);

	if(rdb_get_single(szFullKey,szValue,*pcbValue)<0)
	{
		*pcbValue=0;
		return FALSE;
	}

	*pcbValue=strlen(szValue);

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedriver_cmdDelSingle(databasedriver* pDb, const char* szKey,const char* szPrefix)
{
	const char* szFullKey=databasedriver_getKey(pDb,szKey,szPrefix);

	return rdb_delete_variable(szFullKey)>=0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedriver_cmdSetSingle(databasedriver* pDb, const char* szKey, char* szValue, BOOL fCreate, BOOL fFifo,const char* szPrefix)
{
	static char* szDummyPtr = "";

	int nFlags = (fCreate ? CREATE : 0) | (fFifo ? FIFO : 0);

	char* szCookValue = szValue ? szValue : szDummyPtr;

	const char* szFullKey=databasedriver_getKey(pDb,szKey,szPrefix);

	if(fCreate)
		return rdb_create_variable(szFullKey,szCookValue,nFlags,DEFAULT_PERM,0,0)>=0;
	else
		return rdb_set_single(szFullKey,szCookValue)>=0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int databasedriver_getHandle(databasedriver* pDb)
{
	return rdb_get_fd();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void databasedriver_onRead(databasedriver* pDb)
{
	// call notify handler
	DATABASEDRIVER_NOTIFYHANDLER* lpfnNotifyHandler;
	if (__isAssigned(lpfnNotifyHandler = (DATABASEDRIVER_NOTIFYHANDLER*)pDb->lpfnNotifyHandler))
		lpfnNotifyHandler(pDb);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void databasedriver_closeSession(databasedriver* pDb)
{
	if(rdb_get_fd()<0)
		return;

	rdb_close_db();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedriver_openSession(databasedriver* pDb)
{

	// open database port
	if(rdb_open_db()<0)
		goto error;

	return TRUE;

error:
	databasedriver_closeSession(pDb);
	return FALSE;

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL databasedirver_setNotifyHandler(databasedriver* pDb, DATABASEDRIVER_NOTIFYHANDLER* lpfnNotifyHandler)
{
	pDb->lpfnNotifyHandler = lpfnNotifyHandler;

	return TRUE;
}
