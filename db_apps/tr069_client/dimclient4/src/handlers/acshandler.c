/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_CONNECTION_REQUEST

#include <pthread.h>

#include "eventcode.h"
#include "paramconvenient.h"

struct DigestData
{
	char username[256+1];
	char realm[32+1];
	char uri[32+1];
	char nonce[32+1];
	char nc[10+1];
	char cnonce[32+1];
	char qop[32+1];
	char response[256+1];
};

extern int procId;
extern pthread_mutex_t informLock;
extern char host[];
extern unsigned int acs_flag;

// used to inform the mainloop that the acsHandler could bind on the port.
//
pthread_cond_t acsHandlerStarted = PTHREAD_COND_INITIALIZER;
pthread_mutex_t acsHandlerMutexLock = PTHREAD_MUTEX_INITIALIZER;

static const char *myNonce;
static struct DigestData digestData;

static int myHttpGet (struct soap *);
static int myHttpParseHeader (struct soap *, const char *, const char *);
static int checkDigestResponse (struct soap *, const struct DigestData *);
static void storeDigestData (char *);
static void hex2Ascii (const unsigned char *, char *, int);

#ifdef CONNECTION_REQUEST_HACK
static int issue_connReq_event = 0;

#ifdef PLATFORM_PLATYPUS
int get_nvram (char *name, char *buffer, size_t buflen) {
	int ret;
	size_t len = 0;
	char *buf = NULL;

	if (buffer == NULL) return 1;

	*buffer = 0;

	ret = li_nvram_value_retrieve(name, &buf, &len);

	if(ret) return 1;
	
	if(len >= buflen) {
		memcpy(buffer, buf, (buflen-1));
		buffer[buflen] = 0;
	}
	else {
		memcpy(buffer, buf, len);
		buffer[len] = 0;
	}
	free(buf);

	return 0;
}
#endif

void * connReqHandler ()
{
	struct timespec delay, rem;
	delay.tv_sec = 1;
	delay.tv_nsec = 0;

	while(true) {
		if 	(issue_connReq_event == 1) {
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_ACS, "Adding CONNECTION REQUEST event.\n");
			)
			addEventCodeSingle (EV_CONNECT_REQ);

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_ACS, "Signalling main thread to do inform.\n");
			)
			setAsyncInform(true);
			issue_connReq_event = 0;
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_ACS, "Signal sent.\n");
			)
		}
		else {
			nanosleep( &delay, &rem );
		}
	}
	pthread_exit (NULL);
}
#endif

/** This function handles a call from the ACS
 * and triggers through the accept timeout the InformIntervall
*/
void *
acsHandler (void *localSoap)
{
	int ret = OK;
	int mutexStat = OK;
	struct soap *soap = (struct soap *) localSoap;
	int timeout = 0;
	struct timespec delay, rem;
	delay.tv_sec = 1;
	delay.tv_nsec = 0;
	int	requestCnt = 0;
	time_t actualTime = 0;
	time_t requestSlot = 0;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACS, "ACS Port: %d\n", (ACS_NOTIFICATION_PORT + (procId)));
	)

	soap->accept_timeout = timeout;
	soap->recv_timeout = 30;

	// set my one HttpGet and HttpParseHeader handler
	soap->fget = &myHttpGet;
	soap->fparsehdr = &myHttpParseHeader;

	// wait on port
	ret = soap_bind (soap, host, ACS_NOTIFICATION_PORT + (procId), 100);
	while (ret < SOAP_OK)
	{
		soap_closesock (soap);
		ret = soap_bind (soap, host, ACS_NOTIFICATION_PORT + (procId), 100);
		nanosleep( &delay, &rem );
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_ACS, "ACS port binds success\n");
	)
	// tell the mainloop we are ready to serve the ACS ConnectionRequests
	nanosleep( &delay, &rem );
	pthread_mutex_lock (&acsHandlerMutexLock);
	pthread_cond_signal (&acsHandlerStarted);
	pthread_mutex_unlock (&acsHandlerMutexLock);
	myNonce = NULL;
	ret = soap_accept (soap);
	DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_ACS, "ACS Accept ret: %d\n", ret);
	)

	// init DoS attack detection
	requestSlot = time(NULL);
	requestSlot += MAX_REQUEST_TIME;

	while (true)
	{
		/* count the number of requests in a defined period of time
		 * and reject any request if the number of request is above a maximum number
		 *
		 * 	requestSlot ( lower and upper time) is set
		 *  MAX_REQUEST_TIME defined
		 *  MAX_REQUESTS_PER_TIME defined
		 *  reqCounter set to zero
		 *
		 *  			requestSlot			AnzahlRequests
		 *	-------------------------------------------------------
		 *  OK  	        actTime in          <= MaxRequests
		 *  Fail		actTime in			> MaxRequests
		 *  OK  		actTime out			Reset reqCounter, recalc requestSlot
		 */


		unsigned int flag = false;
#ifndef CONNECTION_REQUEST_HACK
		pthread_mutex_lock (&informLock);
#endif
		flag = acs_flag;
#ifndef CONNECTION_REQUEST_HACK
		pthread_mutex_unlock (&informLock);
#endif
		if(flag)
			continue;

		memset (&digestData, '\0', sizeof (digestData));
		if (ret > 0)
		{
			// HTTP Request from Server
			ret = soap->fparse (soap);
			// Timeout: close socket and restart
			if ( ret == -1 ) {
				soap_closesock (soap);
				ret = soap_accept (soap);
				// reset nonce value to force a new Digest challenge next time
#ifndef CONNECTION_REQUEST_RESET_NONCE
				myNonce = NULL;
#endif
				continue;
			}

			// if no problems then SOAP_STOP is returned
			if (ret != SOAP_STOP)
			{
				DEBUG_OUTPUT (
						dbglog (SVR_ERROR, DBG_ACS, "ACS Notification failed: %d\n", ret );
				)
				//ret = soap->fresponse (soap, ret, 0);
				continue;
			}
			// check for DoS attack
			actualTime = time(NULL);
			if ( actualTime <= requestSlot ) {
				requestCnt++;
				if ( requestCnt > MAX_REQUESTS_PER_TIME ) {
					// Write a HTTP 503 Error message if we are not ready to serv the request
					soap->fresponse( soap, 503, 0 );
					soap_closesock (soap);
					ret = soap_accept (soap);
					continue;
				}
			} else {
				// recalc requestSlot value
				requestSlot = actualTime + MAX_REQUEST_TIME;
				requestCnt = 0;
			}

#if defined(CONNECTION_REQUEST_HACK)
			// reset nonce value to force a new Digest challenge next time
			myNonce = NULL;
			// Write a HTTP 200 OK message if OK
			soap->fresponse( soap, SOAP_OK, 0 );
			soap_closesock (soap);

			issue_connReq_event = 1;

			ret = soap_accept (soap);
			mutexStat = pthread_mutex_trylock (&informLock);
			// reset nonce value to force a new Digest challenge next time
			myNonce = NULL;
			if (mutexStat == OK)
			{
				pthread_mutex_unlock (&informLock);
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_ACS, "ACS Notification\n");
				)
				addEventCodeSingle (EV_CONNECT_REQ);
				setAsyncInform(true);
				// Write a HTTP 200 OK message if OK
				soap->fresponse( soap, SOAP_OK, 0 );
				soap_closesock (soap);
				ret = soap_accept (soap);
			} else {
				// Write a HTTP 503 Error message if we are not ready to serve the request
				soap->fresponse( soap, 503, 0 );
				soap_closesock (soap);
				ret = soap_accept (soap);
			}
#endif
		} else {
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_ACS, "ACS Accept: %d\n", errno);
			)
			nanosleep( &delay, &rem );
			ret = soap_accept (soap);
		}
	}
	pthread_exit (NULL);
}

/** This function is called after the header is parsed.
 If the digest information is not found in digestData
 an 401 error is returned to the server, to initiate an authorization challenge.
 Last modification: San, 15 July 2011
*/
static int
myHttpGet (struct soap *soap)
{
	int ret;

#ifndef ACS_REGMAN
#ifndef PLATFORM_PLATYPUS
	char *connectionURL;

	if ((ret = getConnectionURL(&connectionURL)) != OK)
	{
		return ret;
	}

	/* moved code from below digest check to here and fixed */
	/* return typo from "OK" to "404". Add soap_send() msg  */
	/* 20081111 0000133 */
	// The endpoint must be the same value we read from connection URL

	if ((soap->endpoint != NULL) && (connectionURL != NULL))
	{
//		ret = strcmp (soap->endpoint, connectionURL);
	}
	else ret = 404;

	if ( ret != 0) {
	  soap_send (soap, "HTTP/1.1 404 Not Found\r\n");
	  return 404;
	}
#endif
#endif

#ifdef PLATFORM_PLATYPUS
	char connectionUsername[256+1];
	if ((ret = get_nvram("tr069.conreq.username", connectionUsername, sizeof(connectionUsername))) != 0)
	{
		return ret;
	}
#else
	char * connectionUsername;
	if ((ret = getConnectionUsername(&connectionUsername)) != OK)
	{
		return ret;
	}
#endif

	// no auth. if no username is provided in the parameter file
	if ( (connectionUsername != NULL) && (strlen(connectionUsername) != 0) ) {  // HH Sphairon 17.2.
		if (strlen (digestData.username) == 0 ||
			strlen (digestData.response) == 0 ||
			myNonce == NULL ||
			strncmp (myNonce, digestData.nonce, strlen (myNonce)) ||
			checkDigestResponse (soap, &digestData) ||
			strncmp (digestData.username, connectionUsername,
				 (sizeof (digestData.username) - 1)) != 0)
		{
			myNonce = createNonceValue();
			sprintf (soap->tmpbuf,
				 "WWW-Authenticate: Digest realm=\"Dimark\", qop=\"auth\", nonce=\"%s\"\r\nServer: gSOAP/2.7\r\nContent-Type: text/xml; charset=utf-8\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n",
				 myNonce);
			soap_send (soap, "HTTP/1.1 401 Unauthorized\r\n");
			soap_send (soap, soap->tmpbuf);
			return 401;
		}
	}
	DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_ACS, "user: %s pass: %s\n", soap->userid, soap->passwd);
	)
	return OK;
}

/** Only parse the Host
 * and the Authorization Digest
 */
static int
myHttpParseHeader (struct soap *soap, const char *key, const char *val)
{
	char *valPtr;
	char *headerValue;

	DEBUG_OUTPUT (
		dbglog(SVR_DEBUG, DBG_ACS, "HTTP Header: %s: '%s'\n", key, val);
	)

	if (!soap_tag_cmp (key, "Host"))
	{
		strcpy (soap->endpoint, "http://");
		strncat (soap->endpoint, val, (sizeof (soap->endpoint) - 8));
		soap->endpoint[sizeof (soap->endpoint) - 1] = '\0';
		DEBUG_OUTPUT (
			dbglog(SVR_DEBUG, DBG_ACS, "Endpoint: '%s'\n", soap->endpoint);
		)
		
	}
	else if (!soap_tag_cmp (key, "Authorization"))
	{
		if (!soap_tag_cmp (val, "digest *"))
		{
			valPtr = (char *) &val[7];
			strncpy (soap->tmpbuf, valPtr, sizeof (soap->tmpbuf));
			valPtr = soap->tmpbuf;
			// Split the authorization string into the single parts
			//              username, realm, nonce, uri, response
			while (valPtr != NULL)
			{
				headerValue = strsep (&valPtr, ",");
				DEBUG_OUTPUT (
						dbglog (SVR_DEBUG, DBG_ACS, "Digest: '%s'\n", headerValue);
				)
				storeDigestData (headerValue);
			}
			return SOAP_OK;
		}
	}
	return SOAP_OK;
}

static int
checkDigestResponse (struct soap *soap, const struct DigestData *digest)
{
		int ret;
        EVP_MD_CTX ctx;
        static unsigned char a1Digest[17];
        static unsigned char a2Digest[17];
        static unsigned char a3Digest[17];
        unsigned char a1[600];
        unsigned char a2[200];
        unsigned char a3[255];
        unsigned int size;

#ifdef PLATFORM_PLATYPUS
	char connectionUsername[256+1];
	if ((ret = get_nvram("tr069.conreq.username", connectionUsername, sizeof(connectionUsername))) != 0)
	{
		return ret;
	}
#else
        char * connectionUsername;
    	if ((ret = getConnectionUsername(&connectionUsername)) != OK)
    	{
		DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_ACS, "checkDigestResponse(): getConnectionUsername() failed.\n");
		)
    		return ret;
    	}
#endif

#ifdef PLATFORM_PLATYPUS
	char connectionPassword[256+1];
	if ((ret = get_nvram("tr069.conreq.password", connectionPassword, sizeof(connectionPassword))) != 0)
	{
		return ret;
	}
#else
        char * connectionPassword;
    	if ((ret = getConnectionPassword(&connectionPassword)) != OK)
    	{
		DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_ACS, "checkDigestResponse(): getConnectionPassword() failed.\n");
		)
    		return ret;
    	}
#endif

    	if ( (connectionUsername == NULL) || (connectionPassword == NULL) )
    	{
            DEBUG_OUTPUT (
            		dbglog (SVR_ERROR, DBG_ACS, "checkDigestResponse(): connectionUsername or connectionPassword == NULL\n");
            )
            return ERR_AUTHENTICATION;
    	}

        DEBUG_OUTPUT (
        		dbglog (SVR_DEBUG, DBG_ACS, "user: %s pass: %s nonce= %s\n", connectionUsername, connectionPassword, myNonce);
        )

        strncpy ((char*)a1, connectionUsername, 256);
        strcat ((char*)a1, ":");
        strcat ((char*)a1, CONNECTION_REALM);
        strcat ((char*)a1, ":");
        strncat ((char*)a1, connectionPassword, 256);

        strcpy ((char*)a2, "GET");
        strcat ((char*)a2, ":");
        strncat ((char*)a2, digest->uri, ( sizeof(a2)-strlen((char*)a2)-1));

        EVP_MD_CTX_init((EVP_MD_CTX*)&ctx);
        EVP_DigestInit(&ctx, EVP_md5());
        EVP_DigestUpdate (&ctx, a1, strlen ((char*)a1));
        EVP_DigestFinal(&ctx, a1Digest, &size);

        hex2Ascii (a1Digest, (char*)a1, size);
        EVP_DigestInit(&ctx, EVP_md5());
        EVP_DigestUpdate (&ctx, a2, strlen ((char*)a2));
        EVP_DigestFinal(&ctx, a2Digest, &size);

        hex2Ascii (a2Digest, (char*)a2, size);
        strcpy ((char*)a3, (char*)a1);
        strcat ((char*)a3, ":");
        strcat ((char*)a3, myNonce);
        if ( digest->qop && *digest->qop ) {
                strcat ((char*)a3, ":");
                strcat ((char*)a3, digest->nc);
                strcat ((char*)a3, ":");
                strcat ((char*)a3, digest->cnonce);
                strcat ((char*)a3, ":");
                strcat ((char*)a3, digest->qop);
        }
        strcat ((char*)a3, ":");
        strcat ((char*)a3, (char*)a2);
        EVP_DigestInit(&ctx, EVP_md5());
        EVP_DigestUpdate (&ctx, (unsigned char *) a3, strlen ((char*)a3));
        EVP_DigestFinal(&ctx, a3Digest, &size);
        hex2Ascii (a3Digest, (char*)a3, size);
        DEBUG_OUTPUT (
        		dbglog (SVR_DEBUG, DBG_ACS, "Digest-Auth: response acs: %s client: %s\n", digest->response, a3 );
        )

	ret = strcmp ((char*)a3, digest->response);
	if(ret) {
        	DEBUG_OUTPUT (
       			dbglog (SVR_DEBUG, DBG_ACS, "Digest-Auth: does not match.\n");
        	)
	} else {
        	DEBUG_OUTPUT (
       			dbglog (SVR_DEBUG, DBG_ACS, "Digest-Auth: matches.\n");
        	)
	}

        return ret;
}

/** Split the dataSet and store the value
  in digestData

FFS: Dimark can't write code!

static void
storeDigestData (char *dataSet)
{
	char *keyPtr, *key;
	char *value;

	DEBUG_OUTPUT (
		dbglog (SVR_DEBUG, DBG_ACS, "Digest Data: '%s'\n", dataSet);
	)

	keyPtr = strsep (&dataSet, "=");
	// skip trailing spaces
	while (*keyPtr == ' ')
	{
		keyPtr++;
	}
	key = keyPtr;
	value = strsep (&dataSet, "=");
	if ( value != NULL && strlen( value ) != 0 )
	{
		// check for quotes
		if ( value[0] == '"' )
			value[strlen (value) - 1] = '\0';
	}
	char * value_first_symbol = value+1;
	DEBUG_OUTPUT (
		dbglog (SVR_DEBUG, DBG_ACS, "Key: %s Value: %s\n", key, value);
	)
	if (strcmp (key, "username") == 0)
	{
		strncpy (digestData.username, value_first_symbol,
			 (sizeof (digestData.username) - 1));
	}
	else if (strcmp (key, "realm") == 0)
	{
		strncpy (digestData.realm, value_first_symbol,
			 (sizeof (digestData.realm) - 1));
	}
	else if (strcmp (key, "uri") == 0)
	{
		strncpy (digestData.uri, value_first_symbol,
			 (sizeof (digestData.uri) - 1));
	}
	else if (strcmp (key, "nonce") == 0)
	{
		strncpy (digestData.nonce, value_first_symbol,
			 (sizeof (digestData.nonce) - 1));
	}
	else if (strcmp (key, "nc") == 0)
	{
		strncpy (digestData.nc, value_first_symbol,
			 (sizeof (digestData.nc) - 1));
	}
	else if (strcmp (key, "response") == 0)
	{
		strncpy (digestData.response, value_first_symbol,
			 (sizeof (digestData.response) - 1));
	}
	else if (strcmp (key, "cnonce") == 0)
	{
		strncpy (digestData.cnonce, value_first_symbol,
			 (sizeof (digestData.cnonce) - 1));
	}
	else if (strcmp (key, "qop") == 0)
	{
		if ( value[0] == '"' )
			strncpy (digestData.qop, value_first_symbol,
				 (sizeof (digestData.qop) - 1));
		else
			strncpy (digestData.qop, &value[0],
				 (sizeof (digestData.qop) - 1));
	}
}
*/
static void
storeDigestData (char *dataSet)
{
	char *key, *value;

	/* Parse: LWS? key "=" (value | '"' value '"') */

	DEBUG_OUTPUT (
		dbglog (SVR_DEBUG, DBG_ACS, "Digest Data: '%s'\n", dataSet);
	)

	// get key
	key = strsep (&dataSet, "=");
	if(key == NULL) return;
	// strip leading spaces from key
	while (*key && *key == ' ') key++;

	value = strsep (&dataSet, "=");
	if(value == NULL) return;
	while(*value && *value == ' ') value++;
	if(*value == '"') {
		value++;
		value[strlen(value) - 1] = 0;
	}

	DEBUG_OUTPUT (
		dbglog (SVR_DEBUG, DBG_ACS, "Key: '%s' Value: '%s'\n", key, value);
	)

	/* stuff them into struct field */

	if (strcmp (key, "username") == 0)
	{
		strncpy (digestData.username, value, (sizeof (digestData.username) - 1));
	}
	else if (strcmp (key, "realm") == 0)
	{
		strncpy (digestData.realm, value, (sizeof (digestData.realm) - 1));
	}
	else if (strcmp (key, "uri") == 0)
	{
		strncpy (digestData.uri, value, (sizeof (digestData.uri) - 1));
	}
	else if (strcmp (key, "nonce") == 0)
	{
		strncpy (digestData.nonce, value, (sizeof (digestData.nonce) - 1));
	}
	else if (strcmp (key, "nc") == 0)
	{
		strncpy (digestData.nc, value, (sizeof (digestData.nc) - 1));
	}
	else if (strcmp (key, "response") == 0)
	{
		strncpy (digestData.response, value, (sizeof (digestData.response) - 1));
	}
	else if (strcmp (key, "cnonce") == 0)
	{
		strncpy (digestData.cnonce, value, (sizeof (digestData.cnonce) - 1));
	}
	else if (strcmp (key, "qop") == 0)
	{
		strncpy (digestData.qop, value, (sizeof (digestData.qop) - 1));
	}
}

static void
hex2Ascii (const unsigned char *hex, char *ascii, int len)
{
	char *aptr = ascii;
	register int i;
	for (i = 0; i != len; i++)
	{
		sprintf (aptr, "%2.2x", hex[i]);
		aptr++;
		aptr++;
	}
}

#endif /* HAVE_CONNECTION_REQUEST */
