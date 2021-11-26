#ifndef __DATABASEDRIVER_H__
#define __DATABASEDRIVER_H__

#include <stdlib.h>
#include <linux/limits.h>

#include "base.h"

#include "rdb_ops.h"

typedef struct
{
	void* lpfnNotifyHandler;

	char achKeyPrefix[MAX_NAME_LENGTH];

} databasedriver;


typedef void (DATABASEDRIVER_NOTIFYHANDLER)(databasedriver* pDb);

databasedriver* databasedriver_create(void);
void databasedriver_destroy(databasedriver* pDb);

BOOL databasedirver_setNotifyHandler(databasedriver* pDb, DATABASEDRIVER_NOTIFYHANDLER* lpfnNotifyHandler);
BOOL databasedriver_setKeyPrefix(databasedriver* pDb, const char* szKeyPrefix);

BOOL databasedriver_openSession(databasedriver* pDb);
void databasedriver_closeSession(databasedriver* pDb);

BOOL databasedriver_cmdGetSingle(databasedriver* pDb, const char* szKey, char* szValue, int* pcbValue,const char* szPrefix);
BOOL databasedriver_cmdSetSingle(databasedriver* pDb, const char* szKey, char* szValue, BOOL fCreate, BOOL fFifo,const char* szPrefix);
BOOL databasedriver_cmdSetNotify(databasedriver* pDb, const char* szKey,const char* szPrefix);
BOOL databasedriver_cmdDelSingle(databasedriver* pDb, const char* szKey,const char* szPrefix);

int databasedriver_getHandle(databasedriver* pDb);
void databasedriver_onRead(databasedriver* pDb);

BOOL databasedriver_cmdGetLength(databasedriver* pDb, const char* szKey, int* pcbValue,const char* szPrefix);

#endif
