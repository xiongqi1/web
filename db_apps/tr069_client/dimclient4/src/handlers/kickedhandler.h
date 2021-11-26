/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef kickedhandler_H
#define kickedhandler_H

#include "globals.h"

#ifdef HAVE_KICKED

/* Time we wait until the kickedHandler must bind otherwise there is a problem
*/
#define KICKED_HANDLER_WAIT_SECS	120

extern pthread_cond_t kickedHandlerStarted;
extern pthread_mutex_t kickedHandlerMutexLock;
extern	bool isKicked;

int clearKicked (struct soap *);
void *kickedHandler (void *);

#endif /* HAVE_KICKED */

#endif /* kickedhandler_H */
