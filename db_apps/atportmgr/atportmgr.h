#ifndef __ATPORTMGR_H__
#define __ATPORTMGR_H__


#define ATPORTMGR_SELECT_INTERVAL			1				// seconds
#define ATPORTMGR_VERSION							"1.0"
#define ATPORTMGR_DAEMON_NAME					"atportmgr"
#define ATPORTMGR_RUN_AS_USER					"atportdaemon"
#define ATPORTMGR_ATCOMMAND_TIMEOUT		2				// seconds
#define ATPORTMGR_DBPREFIX						"wwan.%d.%s"
#define ATPORTMGR_ATCOMMAND_DELAY			3

#define ATPORTMGR_DB_MODULELOCK				"module.lock"
#define ATPORTMGR_DB_MODULELOCKED			"module.lock_result"

#define ATPORTMGR_DB_PORTWAIT					60

#endif
