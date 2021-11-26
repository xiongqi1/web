/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef debug_H
#define debug_H

#include <stdarg.h>

#ifdef _DEBUG
# define DEBUG_OUTPUT(x) {x}
#else
# define DEBUG_OUTPUT(x)
#endif

#ifdef _DEBUG

enum dbg_sys_enum {
	DBG_SOAP,
	DBG_MAIN,
	DBG_PARAMETER,
	DBG_TRANSFER,
	DBG_STUN,
	DBG_ACCESS,
	DBG_MEMORY,
	DBG_EVENTCODE,
	DBG_SCHEDULE,
	DBG_ACS,
	DBG_VOUCHERS,
	DBG_OPTIONS,
	DBG_DIAGNOSTIC,
	DBG_VOIP,
	DBG_KICK,
	DBG_CALLBACK,
	DBG_HOST,
	DBG_REQUEST,
	DBG_DEBUG,
	DBG_AUTH,
	DBG_LUA
};

enum dbg_level_enum
{
	SVR_DEBUG,
	SVR_INFO,
	SVR_WARN,
	SVR_ERROR,
};

struct	dbg_stuct {
	char	sys_str[20];
	enum dbg_level_enum level_int;
	char	level_str[20];
};

int loadConfFile (char *);
void dbglog( unsigned int, unsigned int, const char *, ... );
void vdbglog( unsigned int, unsigned int, const char *, va_list );

#endif /* _DEBUG */

#endif /* debug_H */
