/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef EVENTSTORE_H
#define EVENTSTORE_H

#include "utils.h" 
#include "eventcode.h"

/** Deletes all entries in the event data storage
 */
int clearEventStorage( void );

/** Insert the event data in the char * at the end of the storage
 * \param data  the complete event as NULL terminated string
 */
int insertEvent( const char * );

/** Get alle events from the storage and calls newEvent() to process it in dimclient
 */
int readEvents( newEvent * );

/** Creates a file in the RAM file system.
 */
int createBootstrapMarker( void );

int deleteBootstrapMarker( void );

/** Returns true if bootstrapMarker is found
 *  else false
 */
bool isBootstrapMarker( void );

#endif /*EVENTSTORE_H*/
