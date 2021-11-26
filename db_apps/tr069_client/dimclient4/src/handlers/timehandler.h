/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef timehandler_H
#define timehandler_H

#include "globals.h"

#ifdef HAVE_FILE

#ifdef HAVE_SCHEDULE_INFORM

void initSchedule (void);
int execSchedule (struct soap *, 	xsd__unsignedInt, xsd__string);
int clearDelayedSchedule (struct soap *);

#endif /* HAVE_SCHEDULE_INFORM */

#ifdef HAVE_REQUEST_DOWNLOAD

#define		FIRST_TIME_REQUEST_DOWNLOAD		1111111//120 		//2 min

extern time_t	StartRequestDownloadTime;
extern bool isRequestDownload;

int clearRequestDownload (struct soap *);

#endif /* HAVE_REQUEST_DOWNLOAD */

void *timeHandler (void *);

#endif /* HAVE_FILE */

#endif /* timehandler_H */
