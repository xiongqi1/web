#ifndef __GLOBALCONFIG_H__
#define __GLOBALCONFIG_H__

#include "base.h"

#define CNS_MANAGER_VERSION							"0.1.0"
#define CNS_DAEMON_NAME									"cns-manager"
#define CNS_RUN_AS_USER									"cnsdaemon"

typedef struct
{
	int iDbgPrior;

	const char* szCurProcName;
	const char* szCnsDevName;
	const char* szLastDevName;

	int iInstance;

	BOOL fDaemon;
	BOOL fSmsDisabled;

} cnsmgr_config;

#define GLOBALCONFIG_RUNLEVEL_HIPTEST				0
#define GLOBALCONFIG_RUNLEVEL_CNSTEST				1
#define GLOBALCONFIG_RUNLEVEL_NORMALOP			2

extern cnsmgr_config _globalConfig;


BOOL globalconfig_init(int cArg, char** pArg);
void globalconfig_printUsage(void);

#endif
