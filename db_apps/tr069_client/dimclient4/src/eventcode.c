/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include <pthread.h>
#include "eventcode.h"
#include "globals.h"
#include "utils.h"
#include "debug.h"
#include "host/eventStore.h"

#define EVENT_BUFSIZE	200

pthread_mutex_t eventLock = PTHREAD_MUTEX_INITIALIZER;

typedef struct EventStructList {
		struct EventStruct 		*data;
		struct EventStructList 	*next;
} EventStructList;

static EventStructList *evtListLast = NULL;
static int evtListSize = 0;

// Inserts an eventCode into the List of EventCodes
// does no persistent storage
static int insertEventCode( const char *, const char * );

static int addNewEvent (char *);
static bool findEventCode( const char * );

// Write the EventCode into the persistent Database
static int writeEventCodes (void);

/** Returns an ArrayOfEventStruct with all event codes found in the eventCodelist
*/
int getEventCodeList( struct ArrayOfEventStruct *evList )
{
	int i = 0;
	struct EventStructList *el;
	cwmp__EventStruct **ev = (cwmp__EventStruct **)emallocTemp( sizeof( cwmp__EventStruct[evtListSize] ));

	if ( ev == NULL ) 
		return ERR_RESOURCE_EXCEED;

	pthread_mutex_lock(&eventLock);
	
	// Copy list evtListLast entries into array ev[] 
	el = evtListLast;
	
	for ( i = 0; i != evtListSize; i++ ) {
		ev[i] = el->data;
		el = el->next;
	}
	evList->__ptrEventStruct = ev;
	evList->__size = evtListSize;

	pthread_mutex_unlock(&eventLock);

	return OK;
}

/** Frees the entries in the EventList 
 * call it after successful transfer of Data
 */
void freeEventList( void )
{
	struct EventStructList *el;
	struct EventStructList *tmp;
#ifdef _DEBUG
	int i = 0;
#endif /* _DEBUG */
	

	pthread_mutex_lock(&eventLock);

	el = evtListLast;
	
	while( el != NULL ) {

		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_EVENTCODE, "Freeing: %d %s\n", i++, el->data->EventCode);
		)

		efree( el->data->EventCode );
		efree( el->data->CommandKey );
		efree( el->data );
		tmp = el;
		el = el->next;
		efree( tmp );
	}

	evtListLast = NULL;
	evtListSize = 0;

	pthread_mutex_unlock(&eventLock);

	// CLI24
	writeEventCodes();

}

/** Adds an EventCode into the EventCode list

	\param ec 	EventCode
	\param ck	Commandkey 

	\returns ErrorCode
*/
int addEventCodeMultiple( const char *ec, const char *ck )
{
	int ret = OK;
	
	ret = insertEventCode( ec, ck );
	if ( ret == OK )
		ret = writeEventCodes();
	return ret;
}

/** Adds an EventCode into the EventCode list
	Only eventCodes which are not in the list are inserted

	\param ec 	EventCode
	\param ck	Commandkey 

	\returns ErrorCode
*/

int addEventCodeSingle( const char *ec )
{
	int ret = OK;
	
	if ( findEventCode( ec ) )
		return OK;

	ret = insertEventCode( ec, "" );

	if ( ret == OK )
		ret = writeEventCodes();

	return ret;
}

/** Clear all event codes and delete the event code files.
	Called by the factory reset function
 */
int resetEventCodes( void )
{
	int ret = OK;
	// ignore the return values, can't help us
	deleteBootstrapMarker();
	clearEventStorage();

	return ret;
}

/** Insert an eventCode into the list 

	\param ec 	EventCode
	\param ck	Commandkey 

	\returns ErrorCode
*/
static int insertEventCode( const char *ec, const char *ck )
{
	struct EventStructList *el;
	struct EventStruct *es = (struct EventStruct*)emalloc( sizeof( struct EventStruct));
    if ( es == NULL )
        return ERR_RESOURCE_EXCEED;
	es->EventCode = strnDup( es->EventCode, ec, strlen( ec ));
    if ( es->EventCode == NULL )
        return ERR_RESOURCE_EXCEED; 

	es->CommandKey = strnDup( es->CommandKey, ck, strlen( ck));
    if ( es->CommandKey == NULL )
        return ERR_RESOURCE_EXCEED;

	el = (struct EventStructList*)emalloc( sizeof( struct EventStructList));
    if ( el == NULL )
        return ERR_RESOURCE_EXCEED;

    if (evtListSize == MAX_EVENT_LIST_NUM)	{

		DEBUG_OUTPUT (
				dbglog( SVR_DEBUG, DBG_EVENTCODE, "Max event list number (%d) is over by event |%s|%s|\n", MAX_EVENT_LIST_NUM, es->EventCode, es->CommandKey );
		)

		struct EventStructList *em = evtListLast;
		struct EventStructList *before_kill = NULL, *kill = NULL, *tmp = NULL;

    	while( em != NULL ) {
    		if( es->EventCode[0] == 'M')	{
    			before_kill = tmp;
    			kill = em;
    		}
			tmp = em;
    		em = em->next;
    	}

   		DEBUG_OUTPUT (
   				dbglog( SVR_DEBUG, DBG_EVENTCODE, "Event |%s|%s| is taken out from event list\n", kill->data->EventCode, kill->data->CommandKey );
		)

    	before_kill->next = kill->next;
		efree( kill->data->EventCode );
		efree( kill->data->CommandKey );
		efree( kill->data );
		efree( kill );
		evtListSize--;
    }

	el->data = es;

	pthread_mutex_lock(&eventLock);
	el->next = evtListLast;
	evtListLast = el;
	evtListSize++;
	pthread_mutex_unlock(&eventLock);

	return OK;
}

/** Creates an EventCode depending on

 		PersistentFile			Created EventCode
			Absent					0 BOOTSTRAP
			Present					1 BOOT
*/
int loadEventCode( void )
{
	int ret = OK;

	if ( !isBootstrapMarker() ) {
		// A bootstrap is identified
		setBootstrap(true);
		ret = insertEventCode (EV_BOOTSTRAP, "");
		if ( ret != OK )
			return ret;
		ret = createBootstrapMarker();
		return ret;
	}
	// A boot is identified
	ret = insertEventCode (EV_BOOT, "");
	if ( ret != OK )
		return ret;
	ret = readEvents( (newEvent *)&addNewEvent );
	return ret;
}

int addNewEvent (char *data)
{
	char *ptrBuf;
	char *eventCode;
	char *commandKey;
	
	ptrBuf = data;
	eventCode = strsep( &ptrBuf, "|" );
	commandKey = strsep( &ptrBuf, "|" );
	insertEventCode( eventCode, commandKey );

	return OK;
}

static bool findEventCode( const char *ec )
{
	bool ret = false;
	struct EventStructList *el;

	pthread_mutex_lock(&eventLock);
	el = evtListLast;
	while( el != NULL ) {
		if ( strncmp( ec, el->data->EventCode, strlen( el->data->EventCode )) == 0 ) {
			ret = true;
			break;
		}
		el = el->next;
	}
	pthread_mutex_unlock(&eventLock);
	return ret;
}

/** Write all event code from the list into the persistent file
*/
static int writeEventCodes( void )
{
	int ret = OK;
	
	struct EventStructList *el;
	char buf[EVENT_BUFSIZE+1];
	
	pthread_mutex_lock(&eventLock);

	el = evtListLast;

//	CLI24 
	// remove all pending events before we write the complete list again
	clearEventStorage();

	while( el != NULL ) {

		DEBUG_OUTPUT (
				dbglog( SVR_DEBUG, DBG_EVENTCODE, "Save Event: %s CommandKey: %s\n", el->data->EventCode, el->data->CommandKey );
		)

		sprintf( buf, "%s|%s|", el->data->EventCode, el->data->CommandKey );
		insertEvent(buf);
		el = el->next;
	}		

	pthread_mutex_unlock(&eventLock);
	return ret;
}

int sizeOfevtList( void )
{
	int ret;
	pthread_mutex_lock(&eventLock);
	ret = evtListSize;
	pthread_mutex_unlock(&eventLock);
	return ret;
}
