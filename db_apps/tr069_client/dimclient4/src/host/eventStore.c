/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/**
 * All event data is stored in the file system. Also the boot recognition is
 * implemented as a file in the file system.
 */

#include "eventStore.h"
#include "globals.h"
#include "debug.h"
#include "unistd.h"
#include "storage.h"

#include "luaEvent.h"

/** Deletes all entries in the event data storage
 *  ignore a negative return code from remove call, this can happen if
 *  the event store does not exist.
 */
int clearEventStorage( void )
{
	return li_event_deleteAll();
}

/** Insert the event data in the char * at the end of the storage
 * 
 * \param data  the complete event as NULL terminated string
 * 
 */
int insertEvent( const char *data )
{
	return li_event_add(data);
}

/** Get all events from the storage and calls newEvent() to process it in dimclient
 * A trailing EOL is replaced by a EOS char.
 * 
 */
int readEvents( newEvent *func )
{
	return li_event_getAll(func);
}

/** Creates a file in the RAM file system.
 */
int createBootstrapMarker( void )
{
	return li_event_bootstrap_set();
}

int deleteBootstrapMarker( void )
{
	return li_event_bootstrap_unset();
}

/** Returns true if bootstrapMarker is found
 *  else false
 */
bool isBootstrapMarker( void )
{
	return li_event_bootstrap_get();
}
