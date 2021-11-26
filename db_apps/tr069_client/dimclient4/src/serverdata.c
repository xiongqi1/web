/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/** This Module defines and implements a data structure which holds all
 * informations about the server ( ACS or DS ), which are used to control the
 * work.
 */
#include "serverdata.h"

#define CURRENT_ID_SIZE 	128
#define COMMANDKEY_SIZE 	32

typedef struct ServerData {
	// Number of possible Envelopes/per Message
	int maxEnvelopes;
	// Number of retries to connect the Server
	unsigned int retryCount;
	// Storage for the CommandKey sent with the Reboot Command
	char lastCommandKey[COMMANDKEY_SIZE+1];
	// the next action we execute after ending the transaction with the server
	int nextAction;
	// Current Header ID 
	// Mandatory: If used in an Request MUST be resent in the Response
	char currentId[CURRENT_ID_SIZE+1];
	// Boolean Value only sent from ACS if it wants freeze the connection
	// Optional:   0 = false  1 = true Default: 0 
	int holdRequests;
	// Boolean value to indicate that the sender has no more requests
	// Optional: Boolean Value 0 = false 1 = true  Default: 0 
	int noMoreRequests;
} ServerData;

ServerData serverData;

void initServerData( void )
{
	// Initalise the Server Data
	serverData.nextAction = NEXT_NO_ACTION;
	serverData.retryCount = 0;
	serverData.maxEnvelopes = 1;
	serverData.holdRequests = 0;
	serverData.noMoreRequests = 0;
}

void setSdMaxEnvelopes( int me )
{
	serverData.maxEnvelopes = me;
}

void setSdCurrentId( const char *ci )
{
	memset( serverData.currentId, 0, CURRENT_ID_SIZE );
	strnCopy( serverData.currentId, ci, CURRENT_ID_SIZE );
}

char *getSdCurrentId( void )
{
	return serverData.currentId;
}

void setSdNextAction( int na )
{
	serverData.nextAction = na;
}

int getSdNextAction( void )
{
	return serverData.nextAction;
}

void setSdRetryCount( unsigned int rc )
{
	serverData.retryCount = rc;
}

void incSdRetryCount( void )
{
	serverData.retryCount ++;
}

void clearSdRetryCount( void )
{
	serverData.retryCount = 0;
}

unsigned int getSdRetryCount( void )
{
	return serverData.retryCount;
}

void setSdLastCommandKey( char *ck )
{
	memset( serverData.lastCommandKey, 0, COMMANDKEY_SIZE );
	strncpy( serverData.lastCommandKey, ck, COMMANDKEY_SIZE );
}

const char *getSdLastCommandKey( void )
{
	return serverData.lastCommandKey;
}

void setSdHoldRequests( int hr )
{
	serverData.holdRequests = hr;
}

int getSdHoldRequests( void )
{
	return serverData.holdRequests;
}

void setSdNoMoreRequests( int nmr )
{
	serverData.noMoreRequests = nmr;
}

int getSdNoMoreRequests( void )
{
	return serverData.noMoreRequests;
}
