/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef utils_H
#define utils_H

#include "soapH.h" 

typedef enum Bool { true = 1, false = 0 } bool;

bool isReboot( void );
void setReboot( void );

/** Returns true if the system made a bootstrap boot 
 * and false all the other time.
 * The flag is set by the event handling
 */
bool isBootstrap( void );
void setBootstrap( bool );

void setAsyncInform( const bool );
bool getAsyncInform( void );
long getTime( void );
void sysError( const char * );
void clearFault( void );
int  addFault ( const char *, const char *, int );
void createFault( struct soap *, int );
SetParameterFaultStruct **getParameterFaults( void );
char *getFaultString( int );
void *emalloc( unsigned int );
void *emallocTemp( unsigned int );
char *strnDupTemp( char *, const char *, int );
void efreeTemp( void );
void efree( void * );
void *emallocSession( unsigned int );
char *strnDupSession( char *, const char *, int );
void efreeSession( void );

/** String functions */
void strnCopy( char *, const char *, int );
char *strnDup( char *, const char *, int );
bool strCmp( const char *, const char * );
bool strnStartsWith( const char *, const char *, int );

struct SOAP_ENV__Header *setHeader( struct SOAP_ENV__Header * );
struct SOAP_ENV__Header *analyseHeader( struct SOAP_ENV__Header * );

int expWait( unsigned long );

/** Access to the instance number in an object pathname */
int getIdx( const char *, const char * );
int getIdxByName( const char *, const char * );
int getRevIdx( const char *, const char * );

/** Handles the Status return code for SetParameterValue */
#define PARAMETER_CHANGES_APPLIED		0
#define PARAMETER_CHANGES_NOT_APPLIED	1

void setParameterReturnStatus( unsigned int );
int getParameterReturnStatus( void );

/** ACS-CPE Session monitoring */
/** call at start of a ACS session, after the inform message is successfully returned */
void sessionStart( void );
/** get the start time as a session id, this can be used to identify a new session */
time_t getSessionId( void );
/** returns true if the sessionId differs from the given parameter */
bool isNewSession( time_t );
/** call at end of an ACS session, after the empty message is received */
void sessionEnd( void );
/** build a session info string */
const char *sessionInfo( void );
/** Increments the call counter every time the ACS calls a CPE function */
void sessionCall( void );

/** creates a random nonce value for Digest */
const char *createNonceValue( void );

/** creates a random integer value */
const int createRandomValue( void );

int a2i( const char *);
long a2l( const char *);
char *dateTime2s( time_t );
const int s2dateTime( const char *, time_t * );


/* My Locking centralisation */

void requestInform();

#endif /* utils_H */
