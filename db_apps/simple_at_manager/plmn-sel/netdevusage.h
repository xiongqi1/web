#ifndef __NETDEVUSAGE_H__
#define __NETDEVUSAGE_H__ 

#include <string.h>
#include <limits.h>

#define NETDEVUSAGE_STAT_DEV "/proc/net/dev"

typedef struct 
{
	char achDev[PATH_MAX];

	int fValid;
	unsigned long long cbCurRecv;
	unsigned long long cbCurSent;

} netdevusage;

netdevusage* netdevusage_create();
void netdevusage_setDev(netdevusage* pN,const char* dev);
void netdevusage_destroy(netdevusage* pN);
int netdevusage_getUsageChg(netdevusage* pN, unsigned long long* pDeltaRecv, unsigned long long* pDeltaSent);

#endif
