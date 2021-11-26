#ifndef __DEBUGMSG_H__
#define __DEBUGMSG_H__

#include <stdio.h>
#include <syslog.h>

#include "utils.h"

void syslog_init(const char* szIdent,int iPriority, BOOL fErrOut);
void syslog_fini(void);


#endif
