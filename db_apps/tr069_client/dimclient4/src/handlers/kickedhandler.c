/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_KICKED

#include <pthread.h>

#include "eventcode.h"
#include "paramconvenient.h"

#define		BUF_SIZE	1024
#define		NEXT_URL	"NextURL"
#define		FAULT_URL	"FaultURL"

extern int procId;
extern pthread_mutex_t informLock;
extern char host[];

// used to inform the mainloop that the kickedHandler could bind on the port.
//
pthread_cond_t kickedHandlerStarted = PTHREAD_COND_INITIALIZER;
pthread_mutex_t kickedHandlerMutexLock = PTHREAD_MUTEX_INITIALIZER;

bool	isKicked = true;

static char	buf[BUF_SIZE + 1];
static char	Command[SOAP_TAGLEN];
static char	Referer[SOAP_TAGLEN];
static char	Arg[SOAP_TAGLEN];
static char	Next[SOAP_TAGLEN];
static	char	NextURL[SOAP_TAGLEN] = "";
static	char	*NextURLPtr;
static	bool	KickedFlag = false;

/** For Kicked a Kicked Message is sent to the ACS
*/
int
clearKicked (struct soap *server)
{
	int ret = SOAP_OK;

	if( KickedFlag )
	{
		if (isKicked)	{

			ret = soap_call_cwmp__Kicked(server, getServerURL (), "",
											Command,
											Referer,
											Arg,
											Next,
											&NextURLPtr);
		} else {
			ret = SOAP_NO_METHOD;
		}
		if (ret != OK || strcmp(NextURLPtr, Next))
		{
			strcpy(NextURL, FAULT_URL);
		}
		else
		{
			strcpy(NextURL, NextURLPtr);
		}

		KickedFlag = false;

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_KICK, "Call Kicked ret: %d\n", ret);
		)

		pthread_mutex_unlock (&kickedHandlerMutexLock);
	}
	return ret;
}

/** This function handles a call from the Kicked
*/
void *
kickedHandler (void *localSoap)
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
			dbglog (SVR_INFO, DBG_KICK, "Kicked Port: %d\n", (KICKED_NOTIFICATION_PORT + (procId)));
	)

	soap->accept_timeout = timeout;
	soap->recv_timeout = 30;

	// wait on port
	ret = soap_bind (soap, host, KICKED_NOTIFICATION_PORT + (procId), 100);
	while (ret < SOAP_OK)
	{
		soap_closesock (soap);
		ret = soap_bind (soap, host, KICKED_NOTIFICATION_PORT + (procId), 100);
		nanosleep( &delay, &rem );
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_KICK, "Port bind success\n");
	)
	// tell the mainloop we are ready to serve the Kicked
	nanosleep( &delay, &rem );
	pthread_mutex_lock (&kickedHandlerMutexLock);
	pthread_cond_signal (&kickedHandlerStarted);
	pthread_mutex_unlock (&kickedHandlerMutexLock);

	ret = soap_accept (soap);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_KICK, "Kicked Accept ret: %d\n", ret);
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
		 *  			requestSlot			AnalysisRequests
		 *	-------------------------------------------------------
		 *  OK  	 	actTime in          <= MaxRequests
		 *  Fail		actTime in			> MaxRequests
		 *  OK  		actTime out			Reset reqCounter, recalc requestSlot
		 */
		if (ret > 0)
		{
			// check for DoS attack
			actualTime = time(NULL);
			if ( actualTime <= requestSlot ) {
				requestCnt++;
				if ( requestCnt > MAX_REQUESTS_PER_TIME ) {
					// Write a HTTP 503 Error message if we are not ready to serve the request
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

			mutexStat = pthread_mutex_trylock (&informLock);
			if (mutexStat == OK)
			{
				soap_getline(soap, buf, BUF_SIZE);

				//Check FaultURL site
				if(strstr(buf, FAULT_URL))
				{
					pthread_mutex_unlock (&informLock);

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_KICK, "FaultURL site\n");
					)

					/* Go to end of HTTP/MIME header*/
					/* empty line: end of HTTP/MIME header */
					while(*buf)
						soap_getline(soap, buf, BUF_SIZE);

					// Go to FaultURL site
					soap_send( soap, "FaultURL site.");
					soap_closesock (soap);
					ret = soap_accept (soap);
					continue;
				}

				//Check NextURL site
				if (strstr(buf, NEXT_URL) && !strstr(buf, "command"))
				{
					pthread_mutex_unlock (&informLock);

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_KICK, "NextURL site\n");
					)

					/* Go to end of HTTP/MIME header*/
					/* empty line: end of HTTP/MIME header */
					while(*buf)
						soap_getline(soap, buf, BUF_SIZE);

					// Go to NextURL site
					soap_send( soap, "NextURL site.");
					soap_closesock (soap);
					ret = soap_accept (soap);
					continue;
				}

				// http://cpe-host-name/kick.html?command=<#>&arg=<arg>&next=<url>
				// http://192.168.15.17:8084/command=1&arg=2&next=NextURL

				char	*s;

				Command[0] = 0;
				strncpy( Referer, soap->href, sizeof(Referer));
				Arg[0] = 0;
				Next[0] = 0;

				s = strtok(buf, "=");
				if(s)
				{
					s = strtok(NULL,"&");
					if(s)
					{
						strcpy(Command,s);
						s = strtok(NULL, "=");
						if(s)
						{
							s = strtok(NULL,"&");
							if(s)
							{
								strcpy(Arg,s);
								s = strtok(NULL, "=");
								if(s)
								{
									s = strtok(NULL, " ");
									if(s)
									{
										strcpy(Next,s);
									}
								}
							}
						}
					}
				}

				char	*KickURL, *SiteURL;
				bool	flag_out = false;

				/* find X-Forwarded-For */
				do{
					soap_getline(soap, buf, BUF_SIZE);
					if( strstr(buf,"X-Forwarded-For") )
					{
						SiteURL = strchr(buf,' ');
						while( *SiteURL != 0 && *SiteURL == ' ')
							SiteURL++;
						if(SiteURL)
						{
							ret = getParameter (KICKURL, &KickURL);
							if	(ret != OK || inet_addr (KickURL) != inet_addr (SiteURL))
							{
								break;
							}
							else
							{
								flag_out = true;
								break;
							}
						}
						else
						{
							break;
						}
					}
				}while(*buf);

				/* Go to end of HTTP/MIME header*/
				/* empty line: end of HTTP/MIME header */
				while(*buf)
					soap_getline(soap, buf, BUF_SIZE);

				if(!flag_out)
				{
					pthread_mutex_unlock (&informLock);

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_KICK, "Kicked. Not accepted\n");
					)

					// Write a HTTP 503 Error message
					soap->fresponse( soap, 503, 0 );
					soap_closesock (soap);
					ret = soap_accept (soap);
					continue;
				}
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_KICK, "Kicked. Accepted\n");
				)

				KickedFlag = true;
				pthread_mutex_unlock (&informLock);

				addEventCodeSingle (EV_KICKED);
				setAsyncInform(true);

				nanosleep( &delay, &rem );
				pthread_mutex_lock (&kickedHandlerMutexLock);
				pthread_cond_signal (&kickedHandlerStarted);
				pthread_mutex_unlock (&kickedHandlerMutexLock);

				strncpy( soap->endpoint, NextURL, sizeof(soap->endpoint));

				//302 Found
				soap->fresponse( soap, 302, 0 );
				soap_closesock (soap);
				ret = soap_accept (soap);
			} else {
				// Write a HTTP 503 Error message if we are not ready to serve the request
				soap->fresponse( soap, 503, 0 );
				soap_closesock (soap);
				ret = soap_accept (soap);
			}
		} else {
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_KICK, "Kicked Accept error: %d\n", errno );
			)

			nanosleep( &delay, &rem );
		}
	}
	pthread_exit (NULL);
}

#endif /* HAVE_KICKED */
