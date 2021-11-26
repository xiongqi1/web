#ifndef __TEMPLATELIST_H__
#define __TEMPLATELIST_H__

#include "objectlist.h"
#include "dbenum.h"


#define TEMPLATELIST_SUBSCRIBE_DELAY			5

#define TEMPLATELIST_DIR_TEMPLATES				"/etc/cdcs/conf/mgr_templates"
#define TEMPLATELIST_DIR_SCRIPTS					"/etc/cdcs/conf/mgr_scripts"

#define TEMPLATELIST_FILE_EXT							".template"
#define TEMPLATELIST_POSTFIX_SCRIPTS			".sh"

///////////////////////////////////////////////////////////////////////////////
typedef struct
{

	objectlist* pObjListMgr;
	dbenum* pEnum;

} templatelist;

///////////////////////////////////////////////////////////////////////////////

templatelist* templatelist_create(void);
void templatelist_destroy(templatelist* pL);
int templatelist_procNotification(templatelist* pL, int fInit);
int templatelist_subscribteTemplateVaribles(templatelist* pL);
int templatelist_buildTemplates(templatelist* pL);
int templatelist_loadTemplates(templatelist* pL);

///////////////////////////////////////////////////////////////////////////////

#endif
