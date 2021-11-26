/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/*
 * Server definitions
 */
#ifndef server_H
#define server_H

#include "utils.h"

#define NEXT_NO_ACTION 		0
#define NEXT_BOOT_ACTION 	1

/* Initialize the ServerData with reasonable Values
*/
void initServerData( void );

/** Servers MaxEnvelopes we always use one */
void setSdMaxEnvelopes( int me );

/** Our RetryCounter */
void setSdRetryCount( unsigned int rc );
void incSdRetryCount( void );
void clearSdRetryCount( void );
unsigned int getSdRetryCount( void );

/** Stores the Next Action after we have handled all Requests */
void setSdNextAction( int na );
int getSdNextAction( void );

/** A Command which was given by the ACS */
void setSdLastCommandKey( char *ck );
const char *getSdLastCommandKey( void );

/** Current Header ID  Used by ACS */
void setSdCurrentId( const char *ci );
char *getSdCurrentId( void );

/** Holdrequest from ACS */
void setSdHoldRequests( int );
int getSdHoldRequests( void );

/** NoMoreRequests from ACS */
void setSdNoMoreRequests( int );
int getSdNoMoreRequests( void );

#endif /* server_H */
