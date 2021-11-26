/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef notification_handler_H
#define notification_handler_H

#include "globals.h"

#ifdef HAVE_NOTIFICATION

void *passiveNotificationHandler (void *);
void *activeNotificationHandler(void *);

#endif /* HAVE_NOTIFICATION */

#endif /* notification__H */
