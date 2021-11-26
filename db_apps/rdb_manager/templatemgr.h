#ifndef __TEMPLATEMGR_H__
#define __TEMPLATEMGR_H__

#include <linux/limits.h>

#define TEMPLATEMGR_DIR_OUTPUT					"/tmp"

#define TEMPLATEMGR_DELIMITER_REPLACE_OPEN			"?<"
#define TEMPLATEMGR_DELIMITER_SUBSCRIBE_OPEN		"!<"
#define TEMPLATEMGR_DELIMITER_CLOSE							">;"
#define TEMPLATEMGR_SCRIPT_TIMEOUT							30

#include "objectlist.h"

typedef struct
{
	char achTemplate[PATH_MAX];
	char achOutput[PATH_MAX];
	char achScriptMgr[PATH_MAX];

	objectlist* pObjListVariables;

	int fUserTick;

} templatemgr;

templatemgr* templatemgr_create(const char* szTemplate, const char* szMgr);
void templatemgr_destroy(templatemgr* pT);

int templatemgr_isIncluded(templatemgr* pT, const char* szVariable);
int templatemgr_buildConf(templatemgr* pT);

int templatemgr_execMgr(templatemgr* pT, int fInit);
int templatemgr_buildVaribles(templatemgr* pT);
int tempaltemgr_unlinkConf(templatemgr* pT);



#endif

