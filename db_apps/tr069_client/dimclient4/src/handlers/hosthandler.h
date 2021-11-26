/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef hosthandler_h
#define hosthandler_h

#include "globals.h"

#ifdef HAVE_HOST

// Timeout value for host handler binding on port and
// Device has connected to Gateway for data exchange
#define HOST_HANDLER_WAIT_SECS  300

extern pthread_cond_t hostHandlerStarted;
extern pthread_mutex_t hostHandlerMutexLock;

void *hostHandler (void *);

#if defined( TR_111_DEVICE ) && !defined( TR_111_DEVICE_WITHOUT_GATEWAY )
void	DHCP_Discover_dimarkMain( void );
#endif

#endif /* HAVE_HOST */

#endif /* hosthandler_h */
