/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef acshandler_H
#define acshandler_H

#include "globals.h"

#ifdef HAVE_CONNECTION_REQUEST

/* Time we wait until the acsHandler must bind otherwise there is a problem
*/
#define ACS_HANDLER_WAIT_SECS  120

extern pthread_cond_t acsHandlerStarted;
extern pthread_mutex_t acsHandlerMutexLock;

void *acsHandler (void *);

#ifdef CONNECTION_REQUEST_HACK
void * connReqHandler ();
#endif
#endif /* HAVE_CONNECTION_REQUEST */

#endif /* acshandler_H */
