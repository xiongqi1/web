/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/** Handles the callback functions */

#include "globals.h"
#include "callback.h"
#include "utils.h"
#include "list.h"

List *initParametersDoneCbList;
List *preSessionCbList;
List *postSessionCbList;
List *cleanUpCbList;

static int preSessionCbOnce (void);
static int preSessionCbCont (void);

void initCallbackList (void)
{
	initParametersDoneCbList = (List*)emalloc( sizeof (List) );
	initList( initParametersDoneCbList );
	preSessionCbList = (List*)emalloc( sizeof (List) );
	initList( preSessionCbList );
	postSessionCbList = (List*)emalloc( sizeof (List) );
	initList( postSessionCbList );
 	cleanUpCbList = (List*)emalloc( sizeof (List) );
	initList( cleanUpCbList );
	
	// Example of preSession Callbacks, these can also be used for post session
	// or cleanup
	// See the return codes for one shot or continuous callbacks
	addCallback( preSessionCbOnce, preSessionCbList );
	addCallback( preSessionCbCont, preSessionCbList );
    //addCallback( preSessionCbOnce, initParametersDoneCbList );
}

void addCallback( Callback cb, List* list )
{
	addEntry( list, cb );
}

/** Executes the callback from the given list
 * the callbacks are called from the head of the list to the end
 * If a callback returns CALLBACK_STOP = 0 the callback is removed from the list
 * if it returns CALLBACK_REPEAT = 1 the callback stays in the list and is called
 * again executeCallback is called.
 * 
 */
int executeCallback( List* list )
{
	int ret = OK;
	ListEntry *entry = NULL;
	Callback cb = NULL;

	entry = iterateList (list, NULL);
	while( entry != NULL ) {
		cb = (Callback)entry->data;
		ret = cb ();
		if ( ret == CALLBACK_STOP )	
			entry = iterateRemove(list, entry);
		else
			entry = iterateList(list, entry);
	}
	return OK;
}

/** Example of a callback function only called once
 */
static int
preSessionCbOnce (void)
{
	DEBUG_OUTPUT (
		dbglog (SVR_INFO, DBG_CALLBACK, "PreSession Callback only once\n");
	)

	return CALLBACK_STOP;
}

/** Example of a callback function called every time the callback list is
 *  processed
 */
static int
preSessionCbCont (void)
{
	DEBUG_OUTPUT (
		dbglog (SVR_INFO, DBG_CALLBACK, "PreSession Callback continuous\n");
	)

	return CALLBACK_REPEAT;
}
