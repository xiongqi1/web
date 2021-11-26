/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_NOTIFICATION

#include <pthread.h>
#include "parameter.h"
#include "notificationhandler.h"

extern pthread_mutex_t paramLock;
extern int sendInformFromActiveNotification();
extern struct ConfigManagement conf;

unsigned int sendActiveNofi = false;

unsigned int rdb_signal	=0;


void * passiveNotificationHandler (void *localVariables)
{
	while(true)
	{
		sleep(conf.notificationTime);

		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_MAIN, "Start passive notification handler\n");
		)

		pthread_mutex_lock(&paramLock);
		checkPassiveNotification();
		pthread_mutex_unlock(&paramLock);

		DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_MAIN, "Finish passive notification handler\n");
		)
	}
	pthread_exit (NULL);
}

void *activeNotificationHandler(void *localVariables)
{
	while(true)
	{
		sleep(conf.notificationTime);

		pthread_mutex_lock (&paramLock);
		checkActiveNotification();
		pthread_mutex_unlock (&paramLock);
		if (sendActiveNofi == true) {
			DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_MAIN, "Active notification required.\n");
			)
			sendActiveNofi = false;
 			addEventCodeSingle(EV_VALUE_CHANGE);
 			setAsyncInform(true);
			DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_MAIN, "Active notification inform requested.\n");
			)
		}
	}

	pthread_exit (NULL);
}


#endif /* HAVE_NOTIFICATION */


