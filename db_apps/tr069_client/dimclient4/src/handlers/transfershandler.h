/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef transfershandler_H
#define transfershandler_H

#include "globals.h"

#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
/* Time we wait until the transfersHandler must bind otherwise there is a problem
*/
#define TRANSFERS_HANDLER_WAIT_SECS		120

extern pthread_cond_t transfersHandlerStarted;
extern pthread_mutex_t transfersHandlerMutexLock;
//extern	bool isKicked;

//int clearKicked (struct soap *);
void *transfersHandler (void *);

#endif /* HAVE_GET_ALL_QUEUED_TRANSFERS */

#endif /* transfershandler_H */
