/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/** Useful functions for access the Modem data */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "paramaccess.h"
#include "serverdata.h"
#include "list.h"

// the digest nonce length must fit the nonce length
#define DIGEST_NONCE_LENGTH		24
#define DIGEST_NONCE_FORMAT		"%8.8x%8.8x%8.8x"

// External defined Soap for access to gsoap functions
extern struct soap *globalSoap;

typedef struct faultMessage {
		int		faultCode;
		char	*faultString;
} FaultMessage;

static FaultMessage faultMessages[] = {
		{ 9000, "Method not Supported" },
		{ 9001, "Requst Denied" },
		{ 9002, "Internal Error" },
		{ 9003, "Invalid Argument" },
		{ 9004, "Resource exceed" },
		{ 9005, "Invalid Parametername" },
		{ 9006, "Invalid Parametertype" },
		{ 9007, "Invalid Parametervalue" },
		{ 9008, "Parameter readonly" },
		{ 9009, "Parameter writeonly" },
		{ 9010, "Download failure" },
		{ 9011, "Upload failure" },
		{ 9012, "Trans auth failure" },
		{ 9013, "No Trans Protocol" }
	};
// Calculate the number of entries in faultMessages
static int faultMessagesSize = sizeof( faultMessages ) / sizeof( FaultMessage );	

// Status which is returned by SetParamterValues
static unsigned int parameterReturnStatus = 0;

// Pointer to a SetParameterValuesFault Array
static bool	initListDone = false;
static List	parameterFaultList;
	
static SetParameterFaultStruct	**parameterFault = NULL;
static int						parameterFaultSize = 0;
	
// Session informations
static time_t		sessionStarttime = 0;
static time_t		sessionId = 0;
static time_t 		sessionEndtime = 0;
static time_t 		sessionRuntime = 0;
static unsigned int	sessionCallCount = 0;
static char			sessionInfoMsg[1024];
	
// Async start of the inform message
static bool	asyncInform = false;
	
// Count allocated chunks	
static int memoryAllocCount = 0;
// Counts all memory allocated in one session
static long sessionAllocCount = 0;

static long tempMemAllocCount = 0L;
// Count allocated memory
static long tempMemAllocBytes = 0L;	
// Maximum of allocated Bytes between two efreeTemp during one session
static long tempMaxAllocBytes = 0L;

// Count memory allocations for Session Memory
static long sessMemAllocCount = 0L;
static long sessMemAllocBytes = 0L;

// Structure to hold a pointer to the allocated Memory which is freed at the 
// end of the Client Mainloop
typedef struct AllocatedMemory {
	void *memory;
	unsigned int  size;
	struct AllocatedMemory *next;
} AllocatedMemory;

static AllocatedMemory *tmpMemListFirst = NULL;
static AllocatedMemory *tmpMemListLast = NULL;
static int tmpMemListSize = 0;

// Buffer to store memory chunks allocated during session usage
static AllocatedMemory *sessMemListFirst = NULL;
static AllocatedMemory *sessMemListLast = NULL;
static int sessMemListSize = 0;

static void *_emalloc( unsigned int );
static bool doReboot = false;
static bool bootstrap = false;
static char nonceValue[DIGEST_NONCE_LENGTH];

/** Session handling.
  A Session is the communication between a CPE and an ACS.
  The Session starts with the Inform message and ends when the connection is broken
*/
void sessionStart( void )
{
	sessionCallCount = 0;
	sessionStarttime = time( NULL );
	sessionId = sessionStarttime;
}

time_t getSessionId( void )
{
	return sessionId;	
}

bool isNewSession( time_t checkSessionId )
{
	if ( sessionId != 0 )
		return sessionId != checkSessionId;
	else
		return true;
}

void sessionEnd( void )
{
	sessionEndtime = time( NULL );
	sessionRuntime = sessionEndtime - sessionStarttime;
	sessionId = -1;
	server_url = NULL;
}

// Builds a string with some session info
const char *sessionInfo( void )
{
	sprintf( sessionInfoMsg, "Runtime: %ld Calls: %d\n", sessionRuntime, sessionCallCount );
	return sessionInfoMsg;
}

// Increments the call counter every time the ACS calls a CPE function
void sessionCall( void )
{
	sessionCallCount ++;
}

/** Handles the reboot flag, which can be set by different functions
 ex. setParameterValue(), addObject() etc.
 It just sets a flag which is used in the mainloop, after no more requests are received
 the system is doing a reboot
*/
void setReboot( void )
{
	doReboot = true;  // !doReboot;
}

bool isReboot( void )
{
	return doReboot;
}

bool isBootstrap( void )
{
	return bootstrap;
}

void setBootstrap(bool flag)
{
	bootstrap = flag;
}

void setAsyncInform( const bool async )
{
	asyncInform = async;
}

bool getAsyncInform( void )
{
	return asyncInform;
}

/** Get the Header and fill the ServerData Structure
 * The HoldRequests must be clear because the CPE is not allowed to send this flag
*/
struct SOAP_ENV__Header *analyseHeader( struct SOAP_ENV__Header *header )
{
	if ( header != NULL ) {
		setSdCurrentId( header->cwmp__ID );
		
		if ( header->cwmp__HoldRequests != 0 ) {
			setSdHoldRequests( (int)header->cwmp__HoldRequests );
			header->cwmp__HoldRequests = 0;  // NULL
		} else {
			// If the Flag is not found in the header it is reseted to false
			setSdHoldRequests( 0 );
		}
// NoMoreRequest is deprecated. see TR121
//  "uncomment" the following lines to support tr069 (no ammendments)
		if ( header->cwmp__NoMoreRequests != 0 ) {
			setSdNoMoreRequests( (int)header->cwmp__NoMoreRequests );
			header->cwmp__NoMoreRequests = 0;  // NULL;
		} 

	} else {
		// If no header is found it is reseted to false
		setSdHoldRequests( 0 );
	}

	return header;
}

/** Set the returnstatus for SetParameterValue
 * 
 * \param value new value only 0 or 1 allowed 
 */
void
setParameterReturnStatus( unsigned int value )
{
	if ( value > 0 )
		parameterReturnStatus = 1;
	else
		parameterReturnStatus = value;
}
 
int 
getParameterReturnStatus( void )
{
	return parameterReturnStatus;
}

/*
Post reboot session retry count			Wait interval range (min-max seconds)
	#1										5-10
	#2										10-20
	#3										20-40
	#4										40-80
	#5										80-160
	#6										160-320
	#7										320-640
	#8										640-1280
	#9										1280-2560
	#10 and subsequent						2560-5120
*/
int expWait( unsigned long power )
{
	power = (1 << power) * 10;

	return (power>5120) ? 5120 : power;
}

/** Mallocs a memory for permanent usage after the mainloop
 *  If size n is 0, then alloc 1 byte for the EOS.
 */
void *emalloc(unsigned int n)
{ 
	void  *p;
	if ( n != 0 ) {
		p = _emalloc( n );
		memoryAllocCount ++;
	} else {
		p = _emalloc(1);
	}

 	return p;
}

static void *_emalloc(unsigned int n)
{ 
	void  *p;
	if ((p = (void*)calloc(1,n)) == NULL && n != 0 )
    	sysError("Out of memory");
 	// this is for the gcc 2.95.x compiler
	// changed call to calloc(), otherwise,  need the memset()
	// memset( p, '\0', n );

	return p;
}

void efree( void *mem )
{
	free( mem );
	memoryAllocCount --;
}

/** Mallocs memory which is freed after leaving the mainloop
*/
void *emallocTemp( unsigned int n )
{
        AllocatedMemory *am;

        void *p = _emalloc( n );
        if ( p == NULL )
                return p;


        am = (AllocatedMemory*)_emalloc( sizeof(AllocatedMemory));
        am->size = n;
        am->memory = p;
        am->next = NULL;
        if ( tmpMemListFirst == NULL ) {
                tmpMemListFirst = am;
                tmpMemListLast = am;
        } else {
                tmpMemListLast->next = am;
                tmpMemListLast = am;
        }
        tmpMemListSize++;
        tempMemAllocCount ++;
        tempMemAllocBytes += n;
        sessionAllocCount += n;

        return p;
}

/** Mallocs memory which is freed after leaving the mainloop
*/
void *emallocSession( unsigned int n )
{
        AllocatedMemory *am;

        void *p = _emalloc( n );
        if ( p == NULL ) {
      	  DEBUG_OUTPUT (
          		  dbglog (SVR_ERROR, DBG_MEMORY, "emallocSession: _emalloc(%d) FAILED\n", n);
      	  )

          return p;
	}

        am = (AllocatedMemory*)_emalloc( sizeof(AllocatedMemory));
        am->size = n;
        am->memory = p;
        am->next = NULL;
        if ( sessMemListFirst == NULL ) {
                sessMemListFirst = am;
                sessMemListLast = am;
        } else {
                sessMemListLast->next = am;
                sessMemListLast = am;
        }
        sessMemListSize++;
        sessMemAllocCount ++;
        sessMemAllocBytes += n;

        return p;
}

void efreeTemp( void )
{
	AllocatedMemory *aml = tmpMemListFirst;
	AllocatedMemory *tmp = tmpMemListFirst;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_MEMORY,
					"Before Freeing MemallocCount: %d TempMemAllocCount: %d TempMemAllocBytes: %ld\n",
					memoryAllocCount, tempMemAllocCount, tempMemAllocBytes );
	)

	// save statistics
	if ( tempMaxAllocBytes < tempMemAllocBytes )
		tempMaxAllocBytes = tempMemAllocBytes;

	while( aml != NULL ) {
		free( aml->memory );
		tempMemAllocBytes -= aml->size;
//		memoryAllocCount --;
		tmp = aml->next;
		free( aml );
//		memoryAllocCount --;
		aml = tmp;
		tmpMemListSize--;
		tempMemAllocCount--;
	}

	tmpMemListFirst = NULL;
	tmpMemListSize = 0;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_MEMORY,
					"After Freeing MemallocCount: %d TempMemAllocCount: %d TempMemAllocBytes: %ld\n",
					memoryAllocCount, tempMemAllocCount, tempMemAllocBytes);
	)
}
		
void efreeSession( void )
{
        AllocatedMemory *aml = sessMemListFirst;
        AllocatedMemory *tmp = sessMemListFirst;

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_MEMORY, "Before Freeing Session\n");
        		dbglog (SVR_INFO, DBG_MEMORY, "PermMem: %ld AllTempMemDuringSession: %ld MaxTempMem: %ld SessMemAllocCount: %d SessMemAllocBytes: %ld\n",
        				memoryAllocCount, sessionAllocCount, tempMaxAllocBytes,  sessMemAllocCount, sessMemAllocBytes);
        )

        while( aml != NULL ) {
                free( aml->memory );
                sessMemAllocBytes -= aml->size;
//              memoryAllocCount --;
                tmp = aml->next;
                free( aml );
//              memoryAllocCount --;
                aml = tmp;
                sessMemListSize--;
                sessMemAllocCount--;

        }

        sessMemListFirst = NULL;
        sessMemListSize = 0;
        sessionAllocCount = 0L;
        tempMaxAllocBytes = 0L;
}

void strnCopy( char *dest, const char *src, int len )
{
	if ( src != NULL )
		strncpy( dest, src, (len+1) );
	else
		memset( dest, '\0', (len+1) );
}

/** Create a copy of src, and returns a pointer to the copy
 do not free the allocated memory, this is done in efreeTemp()
*/
char *strnDupTemp( char *dest, const char *src, int len )
{
	if ( src != NULL ) {
		dest = emallocTemp( len + 1 );
		strncpy( dest, src, (len+1) );
		return dest;
	} else
		return NULL;
}

/** Create a copy of src, and returns a pointer to the copy
 do not free the allocated memory, this is done in efreeSession()
*/
char *strnDupSession( char *dest, const char *src, int len )
{
        if ( src != NULL ) {
                dest = emallocSession( len + 1 );
                strncpy( dest, src, (len+1) );
                return dest;
        } else {
                return NULL;
        }
}


char *strnDup( char *dest, const char *src, int len )
{
	if ( src != NULL ) {
		dest = emalloc( len + 1 );
		strncpy( dest, src, (len + 1));
		return dest;
	} else
		return NULL;
}

/** Compares two char * which also have to have the same length
	\return true 	val1 and val2 have same length and value
					false val1 differs from val2 in length or value
*/
bool strCmp( const char *val1, const char *val2 )
{
	return ( val1 && val2 && strlen( val1 ) == strlen( val2) && strcmp( val1, val2 ) == 0 );
}

/** Compares two char * 
	\return true 	val1 equal val2 for len characters
					false val1 differs from val2
*/
bool strnStartsWith( const char *val1, const char *val2, int len )
{
	return ( strncmp( val1, val2, len ) == 0 );
}

void sysError( const char *mesg )
{
	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_MEMORY, "%s\n", mesg);
	)
}

void clearFault( void )
{
	SetParameterFaultStruct *paramFault;
	
	if ( parameterFault != NULL ) {
		efree( parameterFault );
		parameterFault = NULL;
	}
	
	while (( paramFault = freeList( &parameterFaultList )) !=  NULL ) {
		efree( paramFault->ParameterName );
		efree( paramFault );
	}
	parameterFaultSize = 0;
}

SetParameterFaultStruct **getParameterFaults( void )
{
	SetParameterFaultStruct *paramFault;
	int cnt;

	parameterFaultSize = getListSize( &parameterFaultList );
	parameterFault = (SetParameterFaultStruct **)emalloc(( sizeof (SetParameterFaultStruct *)) * parameterFaultSize );
	ListEntry *le = iterateList( &parameterFaultList, NULL );
	cnt = 0;
	while ( le ) {
		paramFault = (SetParameterFaultStruct *)le->data;
		parameterFault[cnt] = paramFault;
		cnt ++;
		le = iterateList( &parameterFaultList, le );
	}
	return parameterFault;
}

/** Create a new SetParameterValuesFault entry
 */
int addFault( const char *fault, const char *name, int code )
{
	SetParameterFaultStruct *paramFault;

	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_PARAMETER, "SetParameterValuesFault: %s %s %d\n", fault, name, code );
	)

	if ( !initListDone ) {
		initList( &parameterFaultList );
		initListDone = true;
	}
	paramFault = (SetParameterFaultStruct *)emalloc( sizeof ( SetParameterFaultStruct ));
	if ( !paramFault )
		return ERR_RESOURCE_EXCEED;

	paramFault->ParameterName = strnDup( paramFault->ParameterName, name, strlen( name ));
	paramFault->FaultString = getFaultString( code );
	paramFault->FaultCode = code;
	
	addEntry( &parameterFaultList, paramFault );

	return code;
}

void
createFault (struct soap *soap, int errCode)
{
	Fault *detail;
	soap_sender_fault (soap, "CWMP fault", NULL);

	detail = (Fault *) soap_malloc (soap, sizeof (Fault));
	
	// Set the ErrorCode and ErrorMessage as defined in utils.c
	detail->FaultCode = errCode;
	detail->FaultString = (char *) getFaultString (errCode);
	// Append the SetParameterValuesFault messages
	detail->SetParameterValuesFault = getParameterFaults();
	// the size is calculated during getParameterFaults() 
	detail->__sizeParameterValuesFault = parameterFaultSize;
	
	soap->fault->detail = (struct SOAP_ENV__Detail *) soap_malloc (soap, sizeof (struct SOAP_ENV__Detail));
	soap->fault->detail->cwmp__Fault = detail;
//	soap->fault->detail->__any = NULL;	// parameterFault; /* no other XML data      Status = 0;*/
	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_SOAP, "Fault detected %d %s\n", errCode, detail->FaultString);
	)
}

char *getFaultString( int faultCode )
{
	register int i = 0;
	for ( i = 0; i != faultMessagesSize; i ++ ) {
		if ( faultMessages[i].faultCode == faultCode )
			return faultMessages[i].faultString;
	}

	return NULL;
}

long getTime( void )
{
    struct timeval now;

    gettimeofday( &now, NULL );

    return (long)(now.tv_sec*1000 + now.tv_usec/1000);
}

/** Get the instance number of a named object given in paramPath.
	If the name appears more then once, the first hit is taken to get the instance number.

	ParamPath =     Internet.Object.1.Test.2.Enable
	Name =          Test
	Returns 2
*/
int getIdxByName( const char *paramPath, const char *name )
{
	const	register char *ecp , *scp = paramPath;
	char buf[10];

	memset( buf, 0, sizeof( buf ));
	while( true ) {
		ecp = strchr( scp, '.' );
		if ( strncmp( scp, name, ecp-scp ) == 0 ) {
			ecp ++;
			scp = ecp;
			ecp = strchr( scp, '.' );
			strncpy( buf, scp, ecp - scp );
			return a2i( buf );
		} else {
			// skip dot
			ecp ++;
			scp = ecp;
		}
	}
}
	
/** Get the instance of an object in the parameterPath.
 the name of the object must be equal up to the instance number.
 ex.
	ParamPath =     Internet.Object.1.Test.2.Enable
	Name =          Internet.Object.1.Test
	Returns 2
*/
int getIdx( const char *paramPath, const char *name )
{
	register int nameLen, i;
	const	register char *ecp , *scp = paramPath;
	char buf[10];
	
	nameLen = strlen( name );
	scp += nameLen;
	// skip dot
	scp ++;
	ecp = strchr( scp, '.' );
	// between scp and ecp is the number we search 
	i = 0;
	while ( scp != ecp )
		buf[i++] = *scp++;
	buf[i] = '\0';

	return a2i( buf );
}

/** Get the instance of an object in the parameterPath.
	The access method is different to getIdx(). 
	In getRevIdx() the part of the path is given from rear to front, until the instance number.

 ex.
	ParamPath =     Internet.Object.1.Test.2.Enable
	Name =          Test.2.Enable
	Returns 1
*/
int getRevIdx( const char *paramPath, const char *name )
{
	register int pathLen, nameLen, i;
	const	register char *ecp , *scp;
	char buf[256];
	
	pathLen = strlen( paramPath );
	nameLen = strlen( name );
	scp = paramPath + pathLen;
	scp -= nameLen;
	// skip dot
	scp --;
	ecp = scp;
	while ( *--scp != '.' && scp != paramPath ) {
		;
	}
	// between scp and ecp is the number we search 
	i = 0;
	while ( ++scp != ecp )
		buf[i++] = *scp;
	buf[i] = '\0';
	return a2i( buf );
}

/** Create a random integer value
 * use your own random generator available on your system
 */
const int createRandomValue( void )
{
	return rand();
}

/** Create a random nonce value for Digest Authentication
 * 
 */
const char *createNonceValue( void )
{
	sprintf( nonceValue, DIGEST_NONCE_FORMAT, createRandomValue(), memoryAllocCount, (int)time(NULL));

	return nonceValue;
}

/** Convert a string into a int type value
 */
int a2i( const char *cp )
{
	return atoi(cp);
}
 
/** Convert a string into a long type value
*/
long a2l( const char *cp )
{
	return atol(cp);
}

static char dateTimeBuf[DATE_TIME_LENGHT];

/** Convert a time into a SOAP String.
 *  Create a copy of the returned string, cause it's overwritten by the next call.
 */
char *dateTime2s( time_t time )
{
	struct tm *tm;
	
	tm = localtime(&time);
	if (strftime(dateTimeBuf, sizeof(dateTimeBuf),  "%Y-%m-%dT%H:%M:%SZ", tm) == 0)
		return UNKNOWN_TIME;
	else
		return dateTimeBuf;
}
 
 /** Converts the string in timeStr into a time_t type
  * which is written into *time.
  * Uses gSoap Function
  */
const int s2dateTime( const char *timeStr, time_t *time )
{
	return soap_s2dateTime( globalSoap, timeStr, time );
}


extern pthread_cond_t condGo;
extern pthread_mutex_t mutexGo;

void requestInform() {
	DEBUG_OUTPUT (
			dbglog(SVR_INFO, DBG_MAIN, "Requesting INFORM - locking GO mutex\n");
	)
	
	pthread_mutex_lock (&mutexGo);
	DEBUG_OUTPUT (
			dbglog(SVR_INFO, DBG_MAIN, "Got GO condition mutex\n");
	)
	pthread_cond_signal (&condGo);
	DEBUG_OUTPUT (
			dbglog(SVR_INFO, DBG_MAIN, "Signalled GO condition\n");
	)
	pthread_mutex_unlock (&mutexGo);

	DEBUG_OUTPUT (
			dbglog(SVR_INFO, DBG_MAIN, "INFORM Requested - GO mutex released\n");
	)
}
