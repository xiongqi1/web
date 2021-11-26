#include "syslog.h"

#include <stdarg.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void syslog_fini(void)
{
	// print termination message
	syslog(LOG_NOTICE, "terminated");

	closelog();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void syslog_init(const char* szIdent,int iPriority, BOOL fErrOut)
{
	printf("szIdent=%s,iPriority=%d,fErrOut=%d\n",szIdent,iPriority,fErrOut);

	// initial debugging
	openlog(szIdent, LOG_PID | (fErrOut ? LOG_PERROR : 0), LOG_LOCAL5);
	setlogmask(LOG_UPTO(iPriority));

	// print start message
	syslog(LOG_ERR, "start - LOG_ERR");
	syslog(LOG_INFO, "start - LOG_INFO");
	syslog(LOG_NOTICE, "start - LOG_NOTICE");
	syslog(LOG_DEBUG, "start - LOG_DEBUG");

}
