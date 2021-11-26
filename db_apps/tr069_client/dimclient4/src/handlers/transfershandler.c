/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS

#include <pthread.h>

#include "filetransfer.h"

#define 	BUF_SIZE_TRANSFERS		1024
#define 	BUF_SIZE_COMMAND		256
#define		TRANSFER_ACCEPTED		"Transfer accepted"
#define		TRANSFER_NOT_ACCEPTED	"Transfer not Accepted"

extern int procId;
extern pthread_mutex_t informLock;
extern char host[];

// used to inform the mainloop that the transfersHandler could bind on the port.
//
pthread_cond_t transfersHandlerStarted = PTHREAD_COND_INITIALIZER;
pthread_mutex_t transfersHandlerMutexLock = PTHREAD_MUTEX_INITIALIZER;

static char	buf_transfers[BUF_SIZE_TRANSFERS + 1];

static	char	Tcommand[BUF_SIZE_COMMAND];
static	char	Ttype[BUF_SIZE_COMMAND];
static	char	Turl[BUF_SIZE_COMMAND];
static	char	Tusername[BUF_SIZE_COMMAND];
static	char	Tpassword[BUF_SIZE_COMMAND];
static	char	Tdelay[BUF_SIZE_COMMAND];

static	char	Tsize[BUF_SIZE_COMMAND];
static	char	Ttarget[BUF_SIZE_COMMAND];
static	char	Tsuccess[BUF_SIZE_COMMAND];
static	char	Tfailure[BUF_SIZE_COMMAND];

static	char from_hex( char );
static	char *url_decode( char * );
static	void StrFind( const char *, const char *, char * );

/* Converts a hex character to its integer value */
static	char
from_hex( char ch )
{
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Returns a url-decoded version of str */
static	char *
url_decode( char *str )
{
	char	s[BUF_SIZE_COMMAND];
	char	*pstr = str, *pbuf = s;

	while (*pstr) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		} else if (*pstr == '+') {
			*pbuf++ = ' ';
		} else {
			*pbuf++ = *pstr;
		}
		pstr++;
	}
	*pbuf = '\0';
	strcpy( str, s );

	return str;
}

static	void
StrFind (const char *bstr, const char *cstr, char *tstr)
{
	char	localbuf[BUF_SIZE_TRANSFERS + 1];
	char	*s = localbuf;

	tstr[0] = 0;
	strcpy (s, bstr);
	s = strstr (s, cstr);
	if (s) {
		s = strtok (s, "=");
		if(s) {
			s = strtok (NULL,"& ");
			if(s) {
				strcpy (tstr,s);
			}
		}
	}
}

/** This function handles AllQueuedTransfers
*/
void *
transfersHandler (void *localSoap)
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
			dbglog (SVR_INFO, DBG_TRANSFER, "AllQueuedTransfers Port: %d\n", (TRANSFERS_NOTIFICATION_PORT + (procId)));
	)

	soap->accept_timeout = timeout;
	soap->recv_timeout = 30;

	// wait on port
	ret = soap_bind (soap, host, TRANSFERS_NOTIFICATION_PORT + (procId), 100);
	while (ret < SOAP_OK)
	{
		soap_closesock (soap);
		ret = soap_bind (soap, host, TRANSFERS_NOTIFICATION_PORT + (procId), 100);
		nanosleep( &delay, &rem );
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Port bind success\n");
	)
	// tell the mainloop we are ready to serve the Kicked
	nanosleep( &delay, &rem );
	pthread_mutex_lock (&transfersHandlerMutexLock);
	pthread_cond_signal (&transfersHandlerStarted);
	pthread_mutex_unlock (&transfersHandlerMutexLock);

	ret = soap_accept (soap);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Transfers Accept ret: %d\n", ret);
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
				soap_getline(soap, buf_transfers, BUF_SIZE_TRANSFERS);

				//http://192.168.15.17:8096/
				//	command=Download&
				//	type=1 Firmware Upgrade Image&
				//	url=http://www.insart.com/images/stories/content/homepage_logo.jpg&
				//	username=user&
				//	password=name&
				//	delay=10&
				//	size=24024&
				//	target=target&
				//	success=success&
				//	failure=failure

				//http://192.168.15.17:8096/
				//	command=Upload&
				//	type=2 Vendor Log File&
				//	url=http://www.insart.ua/post&
				//	username=user&
				//	password=name&
				//	delay=10

				StrFind (buf_transfers, "command",	Tcommand);
				StrFind (buf_transfers, "type",		Ttype);
				StrFind (buf_transfers, "url",		Turl);
				StrFind (buf_transfers, "username",	Tusername);
				StrFind (buf_transfers, "password",	Tpassword);
				StrFind (buf_transfers, "delay",	Tdelay);

				StrFind (buf_transfers, "size",		Tsize);
				StrFind (buf_transfers, "target",	Ttarget);
				StrFind (buf_transfers, "success",	Tsuccess);
				StrFind (buf_transfers, "failure",	Tfailure);

				char	*TransfersURL, *TransfersSiteURL;
				bool	flag_out_transfer = false;

				/* find X-Forwarded-For */
				do{
					soap_getline(soap, buf_transfers, BUF_SIZE_TRANSFERS);
					if( strstr(buf_transfers,"X-Forwarded-For") )
					{
						TransfersSiteURL = strchr(buf_transfers,' ');
						while( *TransfersSiteURL != 0 && *TransfersSiteURL == ' ')
							TransfersSiteURL++;
						if(TransfersSiteURL)
						{
							ret = getParameter (TRANSFERURL, &TransfersURL);
							if	(ret != OK || inet_addr (TransfersURL) != inet_addr (TransfersSiteURL))
							{
								break;
							}
							else
							{
								flag_out_transfer = true;
								break;
							}
						}
						else
						{
							break;
						}
					}
				}while(*buf_transfers);

				/* Go to end of HTTP/MIME header*/
				/* empty line: end of HTTP/MIME header */
				while(*buf_transfers)
					soap_getline(soap, buf_transfers, BUF_SIZE_TRANSFERS);

				if(!flag_out_transfer || !Turl[0] || (strcmp(Tcommand, "Download") && strcmp(Tcommand, "Upload")) )
				{
					pthread_mutex_unlock (&informLock);

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_TRANSFER, "Transfer. Not accepted\n");
					)

					// Write a HTTP 503 Error message
					soap->fresponse( soap, 503, 0 );
					soap_closesock (soap);
					ret = soap_accept (soap);
					continue;
				}
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_TRANSFER, "Transfer. Accepted\n");
				)

				if (!strcmp(Tcommand, "Download")) {
					cwmp__DownloadResponse response;

					ret = execDownload (	soap,	"",
											url_decode (Ttype),
											url_decode (Turl),
											url_decode (Tusername),
											url_decode (Tpassword),
											atoi (Tsize),
											url_decode (Ttarget),
											atoi (Tdelay),
											url_decode (Tsuccess),
											url_decode (Tfailure),
											NOTACS,
											TransfersURL,
											Turl,
											&response);
				} else {
					cwmp__UploadResponse response;

					ret = execUpload (		soap, "",
											url_decode (Ttype),
											url_decode (Turl),
											url_decode (Tusername),
											url_decode (Tpassword),
											atoi (Tdelay),
											NOTACS,
											TransfersURL,
											Turl,
											&response);
				}

				pthread_mutex_unlock (&informLock);

				soap_send (soap, ret ? TRANSFER_NOT_ACCEPTED : TRANSFER_ACCEPTED);
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
					dbglog (SVR_ERROR, DBG_TRANSFER, "Transfers Accept error: %d\n", errno);
			)
			nanosleep( &delay, &rem );
		}
	}
	pthread_exit (NULL);
}

#endif /* HAVE_GET_ALL_QUEUED_TRANSFERS */
