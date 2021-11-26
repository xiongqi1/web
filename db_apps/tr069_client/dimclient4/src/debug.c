/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#if defined(PLATFORM_PLATYPUS)
#include <syslog.h>
#endif

#include "globals.h"
#include "debug.h"

#ifdef _DEBUG

#define DEBUG_CHANNELS 21

struct	dbg_stuct sev_m[DEBUG_CHANNELS] = {
		{"SOAP", 1, "INFO"},
		{"MAIN", 1, "INFO"},
		{"PARAMETER", 1, "INFO"},
		{"TRANSFER", 1, "INFO"},
		{"STUN", 1, "INFO"},
		{"ACCESS", 1, "INFO"},
		{"MEMORY", 1, "INFO"},
		{"EVENTCODE", 1, "INFO"},
		{"SCHEDULE", 1, "INFO"},
		{"ACS", 1, "INFO"},
		{"VOUCHERS", 1, "INFO"},
		{"OPTIONS", 1, "INFO"},
		{"DIAGNOSTIC", 1, "INFO"},
		{"VOIP", 1, "INFO"},
		{"KICK", 1, "INFO"},
		{"CALLBACK", 1, "INFO"},
		{"HOST", 1, "INFO"},
		{"REQUEST", 1, "INFO"},
		{"DEBUG", 1, "INFO"},
		{"AUTH", 1, "INFO"},
		{"LUA", 1, "INFO"},
};

int loadConfFile (char *filename)
{
	int ret = OK;
	char buf[MAX_PATH_NAME_SIZE + 1];
	FILE *file;
	int	i;
	char *s;

	file = fopen (filename, "r");
	if (file == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_DEBUG, "loadReadConfFile: file not found %s\n", filename);
		)

		return ERR_INTERNAL_ERROR;
	}

	while (fgets (buf, MAX_PATH_NAME_SIZE, file) != NULL)
	{
		if ( buf[0] == '#' || buf[0] == ' ' || buf[0] == '\n' || buf[0] == '\0' )
			continue;
		buf[strlen (buf) - 1] = '\0';	/* remove trailing EOL */

		s = strtok (buf, ";");
		for (i = 0; i < DEBUG_CHANNELS; i++) {
			if ( !strcmp(sev_m[i].sys_str, s) ) {
				strcpy(sev_m[i].level_str, strtok (NULL, "\n"));

				if ( !strcmp(sev_m[i].level_str, "DEBUG") ) {
					sev_m[i].level_int = 0;
				} else {
					if ( !strcmp(sev_m[i].level_str, "INFO") ) {
						sev_m[i].level_int = 1;
					} else {
						if ( !strcmp(sev_m[i].level_str, "WARN") ) {
							sev_m[i].level_int = 2;
						} else {
							sev_m[i].level_int = 3;
						}
					}
				}

				break;
			}
		}
	}

	for (i = 0; i < DEBUG_CHANNELS; i++) {
		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_DEBUG,
						"%s->%d->%s\n",
						sev_m[i].sys_str, sev_m[i].level_int, sev_m[i].level_str);
		)
	}

	fclose (file);

	return ret;
}

#if defined(PLATFORM_PLATYPUS)
void vdbglog(unsigned int severity, unsigned int modul, const char *format, va_list ap) {

	if (sev_m[modul].level_int <= severity) {

		switch (severity)
		{
			case SVR_DEBUG:
				vsyslog(LOG_DEBUG, format, ap);
			break;

			case SVR_INFO:
				vsyslog(LOG_INFO, format, ap);
			break;

			case SVR_WARN:
				vsyslog(LOG_WARNING, format, ap);
			break;

			case SVR_ERROR:
				vsyslog(LOG_ERR, format, ap);
			break;

		    default:
			break;
		}
	}
}
#else
void vdbglog(unsigned int severity, unsigned int modul, const char *format, va_list ap) {
	time_t time_of_day;
	char buffer[ 80 ];

	if (sev_m[modul].level_int <= severity) {
		time_of_day = time( NULL );
		strftime (buffer, 80, "%c", localtime( &time_of_day ));
		//printf ("%s %ld [", buffer, (unsigned long)syscall(224));
		printf ("%s [", buffer);
		switch (severity)
		{
			case SVR_DEBUG:
				printf ("%s", "DEBUG");
			break;

			case SVR_INFO:
				printf ("%s", "INFO");
			break;

			case SVR_WARN:
				printf ("%s", "WARN");
			break;

			case SVR_ERROR:
				printf ("%s", "ERROR");
			break;

		    default:
			break;
		}

		printf ("] %s - ", sev_m[modul].sys_str);
		vprintf (format, ap);
	}
	fflush(stdout);
}
#endif

void dbglog( unsigned int severity, unsigned int modul, const char *format, ... )
{
	va_list ap;
	va_start(ap, format);
	vdbglog(severity, modul, format, ap);
	va_end(ap);
}

#endif /* _DEBUG */
