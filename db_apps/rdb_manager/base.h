#ifndef __BASE_H__
#define __BASE_H__

#ifndef NULL
#define NULL				0
#endif

#define FALSE				0
#define TRUE				1
//
//typedef u_int32_t u32;
//typedef u_int16_t u16;
//typedef u_int8_t u8;

#define COUNTOF(x)	(sizeof(x)/sizeof(x[0]))

typedef int BOOL;

#ifdef __uClinux__
#define fork vfork
#endif


/* Verison Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD	1

//
#define DAEMON_NAME "rdbmgr"

/* The user under which to run */
#define RUN_AS_USER "rdbdaemon"

// matches GEM limit for how long string can be stored in an RDB variable
// Refer to definitions in textedit.h
#define MAX_VALUE_LEN_RDB   4096

extern int g_fSigTerm;
extern int g_fSigRefresh;

#include <unistd.h>
#include <sys/times.h>

clock_t __getTicksPerSecond(void);
clock_t __getTickCount(void);
int __mkdirall(const char* szDir);
int __isExisting(const char* fileName);

typedef enum {
    RDB_MANAGER_MODE_CONFIG,
    RDB_MANAGER_MODE_STATS,
    RDB_MANAGER_MODE_TEMPLATE
} RdbMgrMode;

/// Minimum interval used by updateRdbWatchdog for updating the actual RDB
/// watchdog variables.
#define WATCHDOG_UPDATE_INTERVAL_SECS       5

/// Gets a mode-specific RDB ready variable.
///
/// @param mode The mode in which template manager is running
/// @return 1 -> ready, 0 -> not ready
int getRdbReady(RdbMgrMode mode);

/// Sets a mode-specific RDB ready variable.
///
/// @param mode The mode in which template manager is running
/// @param ready Ready state of template manager, with 0 indicating not ready
///        ("0" stored in RDB), and any other value indicating ready ("1"
///        stored in RDB)
void setRdbReady(RdbMgrMode mode, int ready);

/// Updates a mode-specific RDB watchdog variable.
///
/// @param mode The mode in which template manager is running
void updateRdbWatchdog(RdbMgrMode mode);

#endif
