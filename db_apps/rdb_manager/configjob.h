#ifndef __CONFIGJOB_H__
#define __CONFIGJOB_H__

#include "base.h"

/* The default location for static files (default config, templates)
 * is /etc/cdcs/conf, the default location for dynamic files (current
 * conf, statistics) is /usr/local/cdcs/conf
 *
 * On platforms that want to store the dynamic config files elsewhere,
 * please arrange for /usr/local/cdcs to be a symlink to the correct
 * location.
 *
 * Legacy alert: Platypus2 config files were called platypus.conf,
 * Arachnid config files were called arachnid.conf and all others
 * were called bovine.conf.
 *
 * They are now all called system.conf. There are boot scripts in each
 * platform that rename those config files in case a legacy system
 * gets updated.
 */

#define RDBMANAGER_CFG_FILENAME_DEF		"/etc/cdcs/conf/default.conf"

#ifndef RDBMANAGER_CFG_ROOT_DIR
#define RDBMANAGER_CFG_ROOT_DIR			"/usr/local/cdcs/conf/"
#endif
#define RDBMANAGER_CFG_FILENAME_OVERRIDE	RDBMANAGER_CFG_ROOT_DIR "override.conf"
/* addon file -
* This file is used to change the values of persistent rdb variables on software upgrade.
* (i.e to override a user configuration with a software upgrade)
* On software upgrade the rdb variables in the addon file are updated and the addon file
* is deleted so that it is not applied on every reboot
*
* On Antelope and Arachnid - where the tr069 conf files are stored on unionfs - the same
* addon file cannot be used on more than one software upgrade (as it is marked as deleted
* by unionfs). Hence a new file (addon1.conf, addon2.conf and so on) has to be used every time
*
* There can be only one addon file at a time(for a release i.e.)
* as only the first one in the directory listing will be considered.
* Their names should begin with "addon" and they should reside in /etc/cdcs/conf folder
* For e.g. - addon1.conf, addon2.conf and so on
*
* IMP NOTE - addon files can only be used on platforms where /etc/cdcs/conf is not readonly
* since it relies on the fact that the addon file is deleted after a software upgrade
* so that it is not applied at every reboot
*/
#define RDBMANAGER_CFG_ADDON_DIR		"/etc/cdcs/conf/"
#define RDBMANAGER_CFG_FILENAME_ADDON_BEGIN	"addon"
#ifndef RDBMANAGER_CFG_BASENAME
#define RDBMANAGER_CFG_BASENAME			"system"
#endif
#define RDBMANAGER_CFG_FILENAME_CUR		RDBMANAGER_CFG_ROOT_DIR RDBMANAGER_CFG_BASENAME ".conf"
#define RDBMANAGER_CFG_FILENAME_NEW		RDBMANAGER_CFG_ROOT_DIR RDBMANAGER_CFG_BASENAME ".conf.new"

#define RDBMANAGER_STATISTIC_FILENAME_CUR	RDBMANAGER_CFG_ROOT_DIR RDBMANAGER_CFG_BASENAME ".stat"
#define RDBMANAGER_STATISTIC_FILENAME_NEW	RDBMANAGER_CFG_ROOT_DIR RDBMANAGER_CFG_BASENAME ".stat.new"
#define RDBMANAGER_CFG_WRITEBACK_DELAY		1
#define RDBMANAGER_STATISTIC_WRITEBACK_DELAY	30

#define RDBMANAGER_SELECT_TIMEOUT		1
#define RDBMANAGER_DATABASE_READ_FREQ		30


int doConfigJob(const char* szCur, const char* szNew, const char* szDef, int nFlags, unsigned int nSaveDelay, RdbMgrMode mode);
int doConfigJobPrepare(const char* szCur, const char* szNew, const char* szDef, const char* szAdd, int nFlags, int nSaveDelay);

#endif
