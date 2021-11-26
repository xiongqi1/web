/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"
#include "serverdata.h"
#include "utils.h"
#include "filetransfer.h"
#include "paramaccess.h"
#include "callback.h"
#include "eventcode.h"
#include "option.h"
#include "voipParameter.h"
#include "ftcallback.h"
#include "hosthandler.h"
#include "vouchers.h"
#include "list.h"
#include "diagParameter.h"
#include "dim.nsmap"
#include "httpda.h"
#include "paramconvenient.h"
#include "kickedhandler.h"
#include "transfershandler.h"
#include "acshandler.h"
#include "timehandler.h"
#include "stunhandler.h"
#include "notificationhandler.h"

#include "luaCore.h"
#include "luaEvent.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <openssl/md5.h>
#include <time.h>

#if defined(PLATFORM_PLATYPUS)
#include <syslog.h>
#endif

#define		FINISH_TRANSACTION		9999
#define 	BROKEN_TRANSACTION  	9998
#define		CONTINUE_TRANSACTION	9997

typedef int (*fparsehdr)(struct soap*, const char*, const char*);

char buf401[SOAP_BUFLEN];

#if defined(PLATFORM_PLATYPUS)
int procId = 0;
char conf_path[256] = "/etc_ro/tr-069.conf";
#else
int procId = 8080;
char conf_path[256] = "/etc/tr-069.conf";
#endif
char host[256] = "0.0.0.0";
char proxy_host[256] = "";
int proxy_port = 0;
int last_hold_value = 0;
time_t time_in_sec = 0;
#define TIME_BUF_SIZE 64
char time_buf[TIME_BUF_SIZE];

extern struct ConfigManagement conf;
unsigned int acs_flag = false;

pthread_cond_t condGo = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutexGo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t paramLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t informLock = PTHREAD_MUTEX_INITIALIZER;

#ifdef HAVE_FILE
/*
	Define a download/upload callback function pointers
	it is handed to the download/upload handlers
	and called before/after a download/upload of the files
*/
DownloadCB downloadCBB = NULL;
UploadCB uploadCBB = NULL;
DownloadCB downloadCBA = NULL;
UploadCB uploadCBA = NULL;
#endif /* HAVE_FILE */

struct soap *globalSoap;
struct http_da_info info;

static fparsehdr soap_parse_header;
static bool connectionClose;
static long local_sendInform_count = 0;
static char authrealm[] = CONNECTION_REALM;
struct SOAP_ENV__Header tt = {"1",false_};

static int dimarkMain (struct soap *, unsigned short);
static void soapSetup (struct soap *);
static int sendEmptyBody (struct soap *);
static int sendInform (struct soap *, struct ArrayOfParameterValueStruct *, struct ArrayOfEventStruct *, struct DeviceId * );
static void sigpipe_handler (int);
static int mySoap_serve (struct soap *);
#ifdef	WITH_COOKIES
static int cookieHandler (struct soap *);
#endif /* WITH_COOKIES */
static void initParameters (bool);
static void freeMemory (struct soap *);
static int myMainHttpParseHeader( struct soap *, const char *, const char *);
static int sendFault (struct soap *);
static int clearGetRPCMethods (struct soap *);
/* for active notification */
extern int sendInformFromActiveNotification();
static int sendInformByActiveNotification(struct soap *soap);

static struct soap soap, hostSoap;
#ifdef HAVE_CONNECTION_REQUEST
static struct soap acsSoap;
#endif
#ifdef HAVE_FILE
static struct soap timeSoap;
#endif
#ifdef HAVE_KICKED
static struct soap kickedSoap;
#endif
#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
static struct soap transfersSoap;
#endif

pthread_t hostTid;
#ifdef HAVE_CONNECTION_REQUEST
pthread_t acsTid;
#endif
#ifdef HAVE_FILE
pthread_t timeTid;
#endif
#ifdef HAVE_KICKED
pthread_t kickedTid;
#endif
#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
pthread_t transfersTid;
#endif
#ifdef HAVE_UDP_ECHO
pthread_t udpTid;
#endif
#ifdef WITH_STUN_CLIENT
pthread_t stunTid;
#endif
#ifdef HAVE_NOTIFICATION
pthread_t passive_notification_id;   /* thread id for passive notification */
pthread_t active_notification_id;   /* thread id for active notification */
#endif

#ifdef CONNECTION_REQUEST_HACK
pthread_t connReqTid;
#endif

#if defined(PLATFORM_PLATYPUS)
static void sig_handler(int signum)
{

	switch (signum)
	{
		case SIGUSR1:
			setlogmask(LOG_UPTO(LOG_DEBUG));
			break;
			
		case SIGUSR2:
			setlogmask(LOG_UPTO(LOG_ERR));
			break;
	}
}
#endif


extern unsigned int rdb_signal;


static void rdb_handle_signal(int sig) {
	//dbglog (SVR_INFO, DBG_MAIN, "rdb_handle_signal\n" );
	rdb_signal ++;;
}


/**
 * Main	Function
 * this Function is normally called	by the HostSystem of the Modem
 */
int
main (int argc, char **argv)
{
	int ret = OK;
	unsigned short delayCnt;
	unsigned int periodicIntervalDelay;
#if defined(PLATFORM_PLATYPUS)
	unsigned int nextIntervalTime;
#endif
	unsigned int periodicIntervalTime;
	int delta;
	struct timespec handlerWait;
	struct timespec retryDelay, retryRemain;
	struct timespec periodicInterval;
	time_t now;
	int ch;
	unsigned int notifacation_flag = false;
	struct sigaction sa;

	while ((ch=getopt( argc, argv, "b:p:i:o:h:d:f:" )) != EOF) {
		switch (ch)	{
			case 'b':
				procId = atoi (optarg);
				break;

			case 'p':
				; //strcpy (tmp_path, optarg);
				break;

			case 'i':
				strcpy (proxy_host, optarg);
				break;

			case 'o':
				proxy_port = atoi (optarg);
				break;

			case 'd':
				strcpy (host, optarg);
				break;
			case 'f':
				; //strcpy (conf_path, optarg);
				break;
			case 'h':
			case '?':
			default:
				printf ("-h - Help\n");
				printf ("-d - IP address for binding Connection Request handler\n");
				printf ("-b - Base port for binding Connection Request handler\n");
				printf ("-p - Not Support: Temporary folder for runtime DB (must be writable)\n");
				printf ("-i - Proxy host \n");
				printf ("-o - Proxy port \n");
				printf ("-f - Not Support: Configuration file (ManagementServer.URL, ManagementServer.Username, ManagementServer.Password, etc.)\n");
				exit(0);
		}
	}

	printf ("------------------------------------------------------------\n");
	printf ("CWMP 1.1\n");
	printf ("Base port: %d\n", procId);
//	printf ("Temporary folder: %s\n", tmp_path);
	printf ("Proxy host: %s\n", proxy_host);
	printf ("Proxy port: %d\n", proxy_port);
	printf ("Host: %s\n", host);
	printf ("\n");

#ifdef _DEBUG
	printf ("_DEBUG\n");
#endif /* _DEBUG */

#ifdef TR_111_GATEWAY
	printf ("TR_111_GATEWAY\n");
#endif /* TR_111_GATEWAY */

#ifdef TR_111_DEVICE
	printf ("TR_111_DEVICE\n");
#endif /* TR_111_DEVICE */

#ifdef TR_111_DEVICE_WITHOUT_GATEWAY
	printf ("TR_111_DEVICE_WITHOUT_GATEWAY\n");
#endif /* TR_111_DEVICE_WITHOUT_GATEWAY */

#ifdef WITH_STUN_CLIENT
	printf ("WITH_STUN_CLIENT\n");
#endif /* WITH_STUN_CLIENT */

#ifdef ACS_REGMAN
	printf ("ACS_REGMAN\n");
#endif /* ACS_REGMAN */

#ifdef WITH_OPENSSL
	printf ("WITH_OPENSSL\n");
#endif /* WITH_OPENSSL */

#ifdef WITH_SSLAUTH
	printf ("WITH_SSLAUTH\n");
#endif /* WITH_SSLAUTH */

#ifdef WITH_COOKIES
	printf ("WITH_COOKIES\n");
#endif /* WITH_COOKIES */

#ifdef WITH_SOAPDEFS_H
	printf ("WITH_SOAPDEFS_H\n");
#endif /* WITH_SOAPDEFS_H */

	printf ("------------------------------------------------------------\n");

#ifdef _DEBUG
   #if defined(PLATFORM_PLATYPUS)
   signal(SIGUSR1, sig_handler);
   signal(SIGUSR2, sig_handler);
   openlog("tr069_client", LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_LOCAL5);
   setlogmask(LOG_UPTO(LOG_ERR));
	loadConfFile ("/etc_ro/tr-069.log.conf");
   #else
	loadConfFile ("/etc/tr-069.log.conf");
   #endif
#endif /* _DEBUG */

#ifdef HAVE_REQUEST_DOWNLOAD
	StartRequestDownloadTime = time (NULL) + FIRST_TIME_REQUEST_DOWNLOAD;
#endif


	/* setup rdb signal handler, get notified when rdb changed*/
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = rdb_handle_signal;
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);
	sigaction(SIGPWR, &sa, NULL);

	//signal(SIGPWR, rdb_handle_signal);
	//dbglog (SVR_INFO, DBG_MAIN, "set SIGPWR %p\n", rdb_handle_signal);

	/** Initialize the callback lists */
	initCallbackList();
	
	/* init LUA interface - after callback lists exist */
	li_init();

	/* Set an EventCode, This is normally done by the HostSystem
	 */
	loadEventCode ();
	initServerData ();

	/* load and create the Parameters */
	initParameters ( isBootstrap() );
	efreeTemp();

	/** execute startup callback list
	 * this is only called once at this time 
	 */
	executeCallback(initParametersDoneCbList);

	/** Initialize parameters from the config */
	//printf("conf_path = %s\n", conf_path);
	if ((ret = initConfStruct(conf_path)) != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "dimclient->main(): Initialization Configure File error. initConfStruct() returns %i\n",ret);
		)
		return ret;
	}

	/* load the options if available */
#ifdef HAVE_VOUCHERS_OPTIONS
	loadOptions ();
	efreeTemp();
#endif /* HAVE_VOUCHERS_OPTIONS */

#ifdef HAVE_FILE
	 downloadCBB = &fileDownloadCallbackBefore;
	 downloadCBA = &fileDownloadCallbackAfter;
	 uploadCBB = &fileUploadCallbackBefore;
	 uploadCBA = &fileUploadCallbackAfter;

	/* load transferList */
	initFiletransfer (downloadCBB, uploadCBB, downloadCBA, uploadCBA);
	efreeTemp();
#endif /* HAVE_FILE */

#ifdef HAVE_SCHEDULE_INFORM
	/* load scheduleList */
	initSchedule ();
	efreeTemp();
#endif

	/* Initialize Voice over IP data */
	initVoIP ();
	efreeTemp();

	soapSetup (&soap);
#ifdef WITH_SSLAUTH
	// Code delivered by Finepoint
	if (
			soap_ssl_client_context (&soap,
				SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION
			   /*SOAP_SSL_DEFAULT*/,
			   "client.pem", /* keyfile: required only when client must authenticate to server (see SSL docs on how to obtain this file) */
			   "password", /* password to read the key file (not used with GNUTLS) */
			   "cacerts.pem", /* cacert file to store trusted certificates (needed to verify server) */
			   NULL, /* capath to directory with trusted certificates */
			   NULL /* if randfile!=NULL: use a file with random data to seed randomness */
			)
//			 soap_ssl_client_context (&soap,
//					 	  SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION,
//						  NULL,  // keyfile, SSL_CTX_use_certificate_chain_file
//						  NULL,  // password, SSL_CTX_set_default_passwd_cb
//						  "dimark.pem",  // cafile, SSL_CTX_load_verify_locations
//						  NULL,  // capath, SSL_CTX_load_verify_locations
//						  NULL   // randfile
//				                )
	  )
	{
		soap_print_fault(&soap, stderr);
		exit(1);
	}
#endif

	soap_init (&hostSoap);
#ifdef HAVE_CONNECTION_REQUEST
	soap_init (&acsSoap);
#endif
#ifdef HAVE_FILE
	soap_init (&timeSoap);
#endif
#ifdef HAVE_KICKED
	soap_init (&kickedSoap);
#endif
#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
	soap_init (&transfersSoap);
#endif

	// NOTE: STUN process does not make use of SOAP; i.e. no
	// soap_init() for STUN

#ifdef HAVE_HOST
	pthread_mutex_lock (&hostHandlerMutexLock);
#endif
#ifdef HAVE_CONNECTION_REQUEST
	pthread_mutex_lock (&acsHandlerMutexLock);
#endif
#ifdef HAVE_KICKED
	pthread_mutex_lock (&kickedHandlerMutexLock);
#endif
#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
	pthread_mutex_lock (&transfersHandlerMutexLock);
#endif

#ifdef HAVE_HOST
	pthread_create (&hostTid, NULL, hostHandler, (void *) &hostSoap);
	// Wait for x seconds the hostHandler could bind the port
	handlerWait.tv_sec = time(NULL) + HOST_HANDLER_WAIT_SECS;
	handlerWait.tv_nsec = 0;
	if ( pthread_cond_timedwait( &hostHandlerStarted, &hostHandlerMutexLock, &handlerWait ) == ETIMEDOUT ) {
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_MAIN, "Host handler binds failed\n" );
		) ; // ; used if DEBUG_OUTPUT is not defined
	} else {
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_MAIN, "Host handler binds successfully\n" );
		)
	}
#endif

#ifdef HAVE_CONNECTION_REQUEST
	pthread_create (&acsTid, NULL, acsHandler, (void *) &acsSoap);
	// Wait for x seconds the acsHandler could bind the port
	handlerWait.tv_sec = time(NULL) + ACS_HANDLER_WAIT_SECS;
	handlerWait.tv_nsec = 0;
	if ( pthread_cond_timedwait( &acsHandlerStarted, &acsHandlerMutexLock, &handlerWait ) == ETIMEDOUT ) {
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_MAIN, "ACS handler binds failed\n" );
		); 
	} else {
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_MAIN, "ACS handler binds successfully\n" );
		);
	}
#endif

#ifdef HAVE_KICKED
	pthread_create (&kickedTid, NULL, kickedHandler, (void *) &kickedSoap);
	// Wait for x seconds the kickedHandler could bind the port
	handlerWait.tv_sec = time(NULL) + KICKED_HANDLER_WAIT_SECS;
	handlerWait.tv_nsec = 0;
	if ( pthread_cond_timedwait( &kickedHandlerStarted, &kickedHandlerMutexLock, &handlerWait ) == ETIMEDOUT ) {
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_MAIN, "Kick handler binds failed\n" );
		);
	} else {
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_MAIN, "Kick handler binds successfully\n" );
		);
	}
#endif

#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
	pthread_create (&transfersTid, NULL, transfersHandler, (void *) &transfersSoap);
	// Wait for x seconds the transfersHandler could bind the port
	handlerWait.tv_sec = time(NULL) + TRANSFERS_HANDLER_WAIT_SECS;
	handlerWait.tv_nsec = 0;
	if ( pthread_cond_timedwait( &transfersHandlerStarted, &transfersHandlerMutexLock, &handlerWait ) == ETIMEDOUT ) {
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_MAIN, "Transfer handler binds failed\n" );
		);
	} else {
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_MAIN, "Transfer handler binds successfully\n" );
		);
	}
#endif

#ifdef HAVE_FILE
	// Init as late as possible otherwise, otherwise there is a run condition with the 
	// clearing of delayed file transfers.
	pthread_create (&timeTid, NULL, timeHandler, (void *) &timeSoap);
#endif /* HAVE_FILE */

#ifdef WITH_STUN_CLIENT
	pthread_create (&stunTid, NULL, stunHandler, NULL);
#endif

#ifdef HAVE_UDP_ECHO
#ifndef PLATFORM_PLATYPUS
	pthread_create (&udpTid, NULL, udpHandler, NULL);
#endif
#endif

#ifdef CONNECTION_REQUEST_HACK
	pthread_create (&connReqTid, NULL, connReqHandler, NULL);
#endif
	// free all temporary allocated memory during setup 
	efreeTemp();
	soap.header = &tt;

	for (;;)
	{

		/* Initial Call to Server
		 */
		delayCnt = 0;
		ret = OK;
		pthread_mutex_lock (&informLock);

#if defined(PLATFORM_PLATYPUS)
// Sometimes, issuing Inform method without any event could happen, resulting in 8003 Fault from ACS. 
// That is because of activeNotificationHandler that consumes every event.
// In this case, dimclient is never recovered.
if ( 0 < sizeOfevtList() ) {
#endif 
		while (true)
		{
			pthread_mutex_lock (&paramLock);
			getServerURL();
			ret = dimarkMain (&soap, delayCnt);
#if defined( TR_111_DEVICE ) && !defined( TR_111_DEVICE_WITHOUT_GATEWAY )
			if( ret != SOAP_OK )	{
				DHCP_Discover_dimarkMain();
			}
#endif
			soap_end(&soap);
			soap_done(&soap);
			soap_closesock(&soap);
			pthread_mutex_unlock (&paramLock);

			if (ret != OK)
			{
				incSdRetryCount ();
				retryDelay.tv_sec = expWait (delayCnt++);
				retryDelay.tv_nsec = 0;

				DEBUG_OUTPUT (
				    dbglog (SVR_DEBUG, DBG_MAIN, "DelayCnt: %d ret=%d delay_sec=%d\n", delayCnt,ret,retryDelay.tv_sec);
				)

				if ( nanosleep (&retryDelay, &retryRemain) == -1 ) {
					DEBUG_OUTPUT (
					    dbglog (SVR_ERROR, DBG_MAIN, "main: nanosleep() exit -1 errno = %d\n", errno);
					)
				}
			}
			else
				break;
		} /* while */

#if defined(PLATFORM_PLATYPUS)
}
#endif
		pthread_mutex_unlock (&informLock);

#ifdef HAVE_NOTIFICATION
		if(notifacation_flag == false)
		{
			notifacation_flag = true;
			if(conf.notificationTime != 0)
			{
				/* Initialize active and passive notification handler */
//				pthread_create(&passive_notification_id, NULL, passiveNotificationHandler, NULL);	/*thread for passive notification*/
				pthread_create(&active_notification_id, NULL, activeNotificationHandler, NULL);		/*thread for active notification*/

				DEBUG_OUTPUT (
						dbglog (SVR_DEBUG, DBG_MAIN, "Notification Enabled\n");
				)
			}
			else
			{
				DEBUG_OUTPUT (
						dbglog (SVR_DEBUG, DBG_MAIN, "Notification Disabled\n");
				)
			}
		}
#endif

		if (isReboot () == true)
		{
			DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_MAIN, "Do Reboot\n");
			)
			li_event_reboot();
			exit (1);
		}

		pthread_mutex_lock (&paramLock);
		periodicIntervalDelay = getPeriodicInterval();
		periodicIntervalTime = getPeriodicTime();
		pthread_mutex_unlock (&paramLock);

		/* Handle PeriodicIntervalTime lt. TR121v4
		 * 
		 * at 	= actual time in seconds
		 * pit 	= periodicInformTime in seconds
		 * pi 	= periodicInformInterval
		 * npi  = time in seconds to next periodic inform call
		 * 
		 * 	abs( at - pit ) < pi -> npi = pi - abs( at - pit )
		 *  abs( at - pit ) >= pi -> npi = pi - (abs( at - pit ) % pi )
		 *  
		 */ 
// Another present from stupid Dimark.
// Original routine has a fault when PeriodicInformTime is set to the future.
#if defined(PLATFORM_PLATYPUS)
		if ( periodicIntervalTime >= 0 && periodicIntervalDelay > 0 ) {
			now = time(NULL);
			delta = abs((now - periodicIntervalTime));
			nextIntervalTime = periodicIntervalDelay;

			if (now >= (time_t) periodicIntervalTime) {
				if ( delta < (int)periodicIntervalDelay )
					nextIntervalTime -= delta; 
				else
					nextIntervalTime -= (delta % periodicIntervalDelay);
			}
			else {
				if ( delta < (int)periodicIntervalDelay )
					nextIntervalTime = delta; 
				else
					nextIntervalTime = (delta % periodicIntervalDelay);				
			}
			if(nextIntervalTime == 0)
				nextIntervalTime = periodicIntervalDelay;
		}
		else {
			nextIntervalTime = periodicIntervalDelay;
		}
#else
		if ( periodicIntervalTime > 0 ) {
			now = time(NULL);
			delta = abs((now - periodicIntervalTime));
			if ( delta < (int)periodicIntervalDelay )
				periodicIntervalDelay -= delta; 
			else
				periodicIntervalDelay -= (delta % periodicIntervalDelay);
		}
#endif

		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_MAIN, "Locking GO mutex.\n");
		)
		if(pthread_mutex_trylock(&mutexGo) != EBUSY) {
			DEBUG_OUTPUT (
					dbglog (SVR_DEBUG, DBG_MAIN, "Got GO mutex.\n");
			)
		} else {
			DEBUG_OUTPUT (
					dbglog (SVR_DEBUG, DBG_MAIN, "GO mutex already locked!\n");
			)
		}
		// periodicIntervalDelay > 0 periodicInform is enabled
		if ( periodicIntervalDelay > 0 ) {
#if defined(PLATFORM_PLATYPUS)
			periodicInterval.tv_sec = time (NULL) + nextIntervalTime;
#else
			periodicInterval.tv_sec = time (NULL) + periodicIntervalDelay;
#endif
			periodicInterval.tv_nsec = 0;
			if (pthread_cond_timedwait
				(&condGo, &mutexGo, &periodicInterval) == ETIMEDOUT)
			{
				pthread_mutex_unlock(&mutexGo);
				addEventCodeSingle (EV_PERIODIC);

				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_MAIN, "Inform activation by timeout\n");
				)
			}
			else
			{
				pthread_mutex_unlock(&mutexGo);
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_MAIN, "Inform activation by signal\n");
				)
			}
		} else {
			// PeriodicInform is disabled, just wait for signals from ACS
			if (pthread_cond_wait ( &condGo, &mutexGo ) ) 
			{
				pthread_mutex_unlock(&mutexGo);
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_MAIN, "Inform activation by signal\n" );
				)
			}				
		} /* else */
	} /* for */
#if defined(PLATFORM_PLATYPUS)
	closelog();
#endif
	return 0;
}

/**	Mainloop for one transaction with the ACS 
*/
static int
dimarkMain (struct soap *soap, unsigned short delayCnt)
{
	int ret;
	int digest_init_done = 0;
	int hold = 0;

	last_hold_value = 0;

	// Register Digest Authentication Plugin
	soap_register_plugin (soap, http_da);

	struct ArrayOfParameterValueStruct *informs =	(struct ArrayOfParameterValueStruct *)
														emallocTemp (sizeof (struct ArrayOfParameterValueStruct));
	struct ArrayOfEventStruct *eventList = (struct ArrayOfEventStruct *)
														emallocTemp (sizeof (struct ArrayOfEventStruct));
	struct DeviceId *dev = getDeviceId ();

	ret = getInformParameters (informs);
	ret = getEventCodeList (eventList);
	
	// Reset the soap->error variable because it is not cleared by GSOAP
	soap->error = SOAP_OK;
	soap_errno = SOAP_OK;
	
	/* Call the preSessionCallbacks */
	executeCallback(preSessionCbList);
	/* reset connectionClose flag */
	connectionClose = false;
	/* this function parses the HTTP Header, to look for Connection: close conditions */
	soap_parse_header = soap->fparsehdr;
	soap->fparsehdr = myMainHttpParseHeader;

	if (sendInform (soap, informs, eventList, dev ) != SOAP_OK)
	{
		if(soap->error == 400)
			acs_flag = true;
		else
			acs_flag = false;

		// Client is not authorized 
		if (soap->error == 401)
		{
	        if ( connectionClose )
	        	soap_end(soap);
			// Plugin handles Basic and Digest Authentication
			http_da_save (soap, &info, ( soap->authrealm ? soap->authrealm : authrealm ),
						  getUsername (), getPassword ());

			if (sendInform (soap, informs, eventList, dev ) == SOAP_OK)
			{
				soap_end (soap);
				http_da_restore (soap, &info);
				digest_init_done = 1;
			} else {
			    sessionEnd();
				freeMemory(soap);
				soap_done(soap);
				return ERR_NO_INFORM_DONE;
			}
		}
		else
		{
			sessionEnd();
			freeMemory (soap);  //gork
			/* Reset: close master/slave sockets and remove callbacks
			 */
			soap_done (soap);

			return ERR_NO_INFORM_DONE;
		}
	}
	sessionStart();
#ifdef	WITH_COOKIES
	cookieHandler (soap);
#endif
	hold = getSdHoldRequests();
	last_hold_value = getSdHoldRequests();

	/* Clear the delayed file transfers and send TransferComplete Message
	 * clear the delayed schedules and send Inform Message
	 * Send Kicked Message
	 * Send RequestDownload Message
	 * Send GetRPCMethods Message
	 * before we send an EmptyBodyMessage
	 * but only if the HoldRequest Flag is not set to 1
	 */
	if ( getSdHoldRequests() == 0 )
	{
#ifdef HAVE_RPC_METHODS
		clearGetRPCMethods (soap);
#endif
#ifdef HAVE_FILE
		clearDelayedFiletransfers (soap);
#endif
#ifdef HAVE_SCHEDULE_INFORM
		clearDelayedSchedule (soap);
#endif
#ifdef HAVE_KICKED
		clearKicked (soap);
#endif
#ifdef HAVE_REQUEST_DOWNLOAD
		clearRequestDownload (soap);
#endif
	}
	
	if (sendEmptyBody (soap) != SOAP_OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_MAIN, "Inform completed error: %d\n", soap->error);
		)

		freeMemory (soap);
		/* Reset: close master/slave sockets and remove callbacks
		 */
		soap_done (soap);
		li_event_informComplete(0); // failure
	}
	else
	{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_MAIN, "Inform completed successfully\n");
		)
		li_event_informComplete(1); // success

		/* Loops until all Requests from the ACS are completed
		 */
		while (1)
		{
			ret = mySoap_serve (soap);
			if (ret == BROKEN_TRANSACTION)
			{
				// Send an emptyBody CLI9
				sendEmptyBody (soap);
				// free some memory
				soap_free (soap);
//				efreeTemp ();
				continue;
			}
			DEBUG_OUTPUT (
					dbglog (SVR_DEBUG, DBG_MAIN, "(1)soap_serv ret: %d %d\n", ret, ((struct soap *) soap)->error);
			)
#if defined(PLATFORM_PLATYPUS)
			//discard ACS request when the session is terminated unexpectedly
			if (ret == SOAP_FAULT) {
				if(soap->error == SOAP_EOF) {
					// Send an emptyBody CLI9
					sendEmptyBody (soap);
					// free some memory
					soap_free (soap);
					continue;
				}
			}
#endif
#if 1 //See TR-069 Amendment3 -3.7.1.3 Outgoing Requests for details
#ifdef HAVE_FILE
		clearDelayedFiletransfers (soap);
#endif
#ifdef HAVE_SCHEDULE_INFORM
		clearDelayedSchedule (soap);
#endif
#ifdef HAVE_KICKED
		clearKicked (soap);
#endif
#ifdef HAVE_REQUEST_DOWNLOAD
		clearRequestDownload (soap);
#endif

#endif

			if (ret == SOAP_FAULT)
			{
				// soap_send_fault (soap);
				sendFault (soap);
				clearFault ();
			}
			/* In case of Error break connection
			 */
			if (ret == FINISH_TRANSACTION)
			{
				break;
			}
			if (ret == CONTINUE_TRANSACTION)
			{
				continue;
			}
			// San: 05.07.2011
			if (ret < 0)
			{
				if(last_hold_value == 0)
				{
					break;
				} // end if last_hold_value == 0
				else
				{
					// if last_hold_value == 1
					if (sendEmptyBody (soap) != SOAP_OK)
					{
						DEBUG_OUTPUT (
							dbglog (SVR_ERROR, DBG_MAIN, "sendEmptyBody() error: %d\n", soap->error);
						)
						freeMemory (soap);
						// Reset: close master/slave sockets and remove callbacks
						soap_done (soap);
						break;
					}
					else
					{
						last_hold_value = 0;
						continue;
					}
				} // end if last_hold_value == 1

			}

			efreeTemp ();
		}

		/* execute registered Callbacks
		 */
		executeCallback( postSessionCbList );

		/* Clear the delayed file transfers, schedules, Kicked, RequestDownload, GetRPCMethods if we
		 * couldn't do it after the inform message
		 * but only if HoldRequest is not set to 1
		 */
		if ( getSdHoldRequests() == 0 )
		{
#ifdef HAVE_RPC_METHODS
			clearGetRPCMethods (soap);
#endif
#ifdef HAVE_FILE
			clearDelayedFiletransfers (soap);
#endif
#ifdef HAVE_SCHEDULE_INFORM
			clearDelayedSchedule (soap);
#endif
#ifdef HAVE_KICKED
			clearKicked (soap);
#endif
#ifdef HAVE_REQUEST_DOWNLOAD
			clearRequestDownload (soap);
#endif
		}

		/* free the Digest data 
		 */
		if (digest_init_done)
			http_da_release (soap, &info);

		sessionEnd();

		printf ("%s", sessionInfo());

		/* soap_closesock( (struct soap*)soap );
		 */
		soap_destroy (soap);
		/* Remove temporary data and deserialized data
		 */

		 // Move after dimarkCall because of Problems with too fast ConnectionRequests
		soap_end (soap);

		/* Reset: close master/slave sockets and remove callbacks
		 */
//		soap_done (soap);

		executeCallback( cleanUpCbList );
		/* free the memory allocated with emallocTemp()
		 */
		efreeTemp ();
		efreeSession();

		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_MAIN, "soap_serve end error: %d tag:%s\n", ((struct soap *) soap)->error, ((struct soap *) soap)->tag);
		)
	}

	return OK;
}

int sendInformFromActiveNotification()
{
	int ret = OK;
	pthread_mutex_lock (&informLock);
	pthread_mutex_lock (&paramLock);
	getServerURL();

	ret = sendInformByActiveNotification(&soap);

	soap_end(&soap);
	soap_done(&soap);
	soap_closesock(&soap);

	pthread_mutex_unlock (&paramLock);
	pthread_mutex_unlock (&informLock);

	return ret;
}

static int sendInformByActiveNotification(struct soap *soap)
{
	int ret;
	int digest_init_done = 0;
	int hold = 0;

	last_hold_value = 0;

	// Register Digest Authentication Plugin
	soap_register_plugin (soap, http_da);
	struct ArrayOfParameterValueStruct *informs =	(struct ArrayOfParameterValueStruct *)emallocTemp (sizeof (struct ArrayOfParameterValueStruct));
	struct ArrayOfEventStruct *eventList = (struct ArrayOfEventStruct *)emallocTemp (sizeof (struct ArrayOfEventStruct));
	struct DeviceId *dev = getDeviceId ();

	ret = getInformParameters (informs);
	ret = getEventCodeList (eventList);

	// Reset the soap->error variable because it is not cleared by GSOAP
	soap->error = SOAP_OK;
	soap_errno = SOAP_OK;

	/* Call the preSessionCallbacks */
	executeCallback(preSessionCbList);
	/* reset connectionClose flag */
	connectionClose = false;
	/* this function parses the HTTP Header, to look for Connection: close conditions */
	soap_parse_header = soap->fparsehdr;
	soap->fparsehdr = myMainHttpParseHeader;

	if(sendInform (soap, informs, eventList, dev ) != SOAP_OK){

		if(soap->error == 400)
			acs_flag = true;
		else
			acs_flag = false;

		// Client is not authorized
		if(soap->error == 401){
			if(connectionClose)
				soap_end(soap);
			// Plugin handles Basic and Digest Authentication
			http_da_save (soap, &info, ( soap->authrealm ? soap->authrealm : authrealm ), getUsername (), getPassword ());
			if (sendInform (soap, informs, eventList, dev ) == SOAP_OK){
				soap_end (soap);
				http_da_restore (soap, &info);
				digest_init_done = 1;
			}else{
			    sessionEnd();
				freeMemory(soap);
				soap_done(soap);
				return ERR_NO_INFORM_DONE;
			}
		}else{
			sessionEnd();
			freeMemory (soap);  //gork
			/* Reset: close master/slave sockets and remove callbacks */
			soap_done (soap);
			return ERR_NO_INFORM_DONE;
		}
	}
	sessionStart();
#ifdef	WITH_COOKIES
	cookieHandler (soap);
#endif
	hold = getSdHoldRequests();
	last_hold_value = getSdHoldRequests();

	/* Clear the delayed file transfers and send TransferComplete Message
	 * clear the delayed schedules and send Inform Message
	 * Send Kicked Message
	 * Send RequestDownload Message
	 * Send GetRPCMethods Message
	 * before we send an EmptyBodyMessage
	 * but only if the HoldRequest Flag is not set to 1
	 */
	if(getSdHoldRequests() == 0){
#ifdef HAVE_RPC_METHODS
		clearGetRPCMethods (soap);
#endif
#ifdef HAVE_FILE
		clearDelayedFiletransfers (soap);
#endif
#ifdef HAVE_SCHEDULE_INFORM
		clearDelayedSchedule (soap);
#endif
#ifdef HAVE_KICKED
		clearKicked (soap);
#endif
#ifdef HAVE_REQUEST_DOWNLOAD
		clearRequestDownload (soap);
#endif
	}

	if(sendEmptyBody(soap) != SOAP_OK){
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_MAIN, "Active notification -Inform completed error: %d\n", soap->error);
		)

		freeMemory (soap);
		/* Reset: close master/slave sockets and remove callbacks */
		soap_done (soap);
	}else{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_MAIN, "Active notification - Inform completed successfully\n");
		)

		/* Loops until all Requests from the ACS are completed */
		while(1)
		{
			ret = mySoap_serve (soap);
			if (ret == BROKEN_TRANSACTION){
				// Send an emptyBody CLI9
				sendEmptyBody (soap);
				// free some memory
				soap_free (soap);
//				efreeTemp ();
				continue;
			}
			DEBUG_OUTPUT (
					dbglog (SVR_DEBUG, DBG_MAIN, "soap_serv ret: %d %d\n", ret, ((struct soap *) soap)->error);
			)
#if defined(PLATFORM_PLATYPUS)
			//discard ACS request when the session is terminated unexpectedly
			if (ret == SOAP_FAULT) {
				if(soap->error == SOAP_EOF) {
					// Send an emptyBody CLI9
					sendEmptyBody (soap);
					// free some memory
					soap_free (soap);
					continue;
				}
			}
#endif
#if 1 //See TR-069 Amendment3 -3.7.1.3 Outgoing Requests for details
#ifdef HAVE_FILE
		clearDelayedFiletransfers (soap);
#endif
#ifdef HAVE_SCHEDULE_INFORM
		clearDelayedSchedule (soap);
#endif
#ifdef HAVE_KICKED
		clearKicked (soap);
#endif
#ifdef HAVE_REQUEST_DOWNLOAD
		clearRequestDownload (soap);
#endif

#endif

			if(ret == SOAP_FAULT){
				// soap_send_fault (soap);
				sendFault (soap);
				clearFault ();
			}
			/* In case of Error break connection */
			if (ret == FINISH_TRANSACTION)
			{
				break;
			}
			if (ret == CONTINUE_TRANSACTION)
			{
				continue;
			}
			// San: 05.07.2011
			if (ret < 0)
			{
				if(last_hold_value == 0)
				{
					break;
				} // end if last_hold_value == 0
				else
				{
					// if last_hold_value == 1
					if (sendEmptyBody (soap) != SOAP_OK)
					{
						DEBUG_OUTPUT (
							dbglog (SVR_ERROR, DBG_MAIN, "sendInformByActiveNotification()->sendEmptyBody() error: %d\n", soap->error);
						)
						freeMemory (soap);
						// Reset: close master/slave sockets and remove callbacks
						soap_done (soap);
						break;
					}
					else
					{
						last_hold_value = 0;
						continue;
					}
				} // end if last_hold_value == 1

			}

			efreeTemp ();
		}
		/* execute registered Callbacks*/
		executeCallback( postSessionCbList );

		/* Clear the delayed file transfers, schedules, Kicked, RequestDownload, GetRPCMethods if we
		 * couldn't do it after the inform message
		 * but only if HoldRequest is not set to 1
		 */
		if ( getSdHoldRequests() == 0 )
		{
#ifdef HAVE_RPC_METHODS
			clearGetRPCMethods (soap);
#endif
#ifdef HAVE_FILE
			clearDelayedFiletransfers (soap);
#endif
#ifdef HAVE_SCHEDULE_INFORM
			clearDelayedSchedule (soap);
#endif
#ifdef HAVE_KICKED
			clearKicked (soap);
#endif
#ifdef HAVE_REQUEST_DOWNLOAD
			clearRequestDownload (soap);
#endif
		}

		/* free the Digest data */
		if (digest_init_done)
			http_da_release (soap, &info);

		sessionEnd();
		printf ("%s", sessionInfo());

		/* soap_closesock( (struct soap*)soap );*/
		soap_destroy (soap);
		/* Remove temporary data and deserialized data */
		 // Move after dimarkCall because of Problems with too fast ConnectionRequests
		soap_end (soap);
		/* Reset: close master/slave sockets and remove callbacks */
//		soap_done (soap);

		executeCallback( cleanUpCbList );
		/* free the memory allocated with emallocTemp()*/
		efreeTemp ();
		efreeSession();

		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_MAIN, "soap_serve end error: %d tag:%s\n", ((struct soap *) soap)->error, ((struct soap *) soap)->tag);
		)
	}
	return OK;
}

/* Initialize Soap Environment
*/
static void
soapSetup (struct soap *soap)
{
	signal (SIGPIPE, sigpipe_handler);
	soap_init (soap);
	soap->socket_flags = MSG_NOSIGNAL;
#if defined(PLATFORM_PLATYPUS)
	soap_set_omode (soap,
			SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_XML_INDENT | SOAP_IO_STORE);
#else
	soap_set_omode (soap,
			SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_IO_STORE);
#endif
	soap_set_imode (soap, SOAP_IO_KEEPALIVE);

	soap->recv_timeout = 300;
	soap->send_timeout = 60;
	/* We send no Header in our Requests
	 */
	soap->header = NULL;
	soap->actor = NULL;
/* [v4.0.1c2] */
	/* Proxy */
	soap->proxy_host = proxy_host[0]?proxy_host:NULL;
	soap->proxy_port = proxy_port;
}

/** Send a SOAP Message with an empty body part
 * This signals the ACS, after the inform message, to start the transactions from the ACS.
 */
static int
sendEmptyBody (struct soap *soap)
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_MAIN, "Send empty body\n");
	)
	soap->encodingStyle = NULL;

	/* Send an empty HTTP Post Request to the Server,
	 * this will tell the server to send its requests
	 */
//	soap->userid = (char *) getUsername ();
//	soap->passwd = (char *) getPassword ();
//	soap->authrealm = authrealm;

	// CLI4 Set content-length to 0 value
	soap->keep_alive = 1;

// 	if (soap_begin_send(soap))
//		return soap->error;

	return (soap_connect (soap, getServerURL (), "")
			||
			soap_end_send (soap));
// soap->fpost( soap, getServerURL(), "", 0, "", "", 0 );
}

/**	Send a Inform Command to the ACS  and wait for the response
*/
static int
sendInform (struct soap *soap, struct ArrayOfParameterValueStruct *informs, struct ArrayOfEventStruct *eventList, struct DeviceId *dev )
{
	int ret = OK;
	int maxEnvelopes;
	int idx;
	time_t timet;
	struct tm *timeofday;

	local_sendInform_count++;
	time(&timet);
	timeofday = localtime(&timet);
	DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_MAIN, "sendInform() enter count = %5d | %s", local_sendInform_count, asctime(timeofday));
	)

	maxEnvelopes = 0;
	idx = 0;
	for (idx = 0; idx != informs->__size; idx++)
	{
#ifndef ACS_REGMAN
		if (informs->__ptrParameterValueStruct[idx]->__typeOfValue == StringType) {
			DEBUG_OUTPUT (
					dbglog (SVR_DEBUG, DBG_MAIN, "InfP: %d %s %s\n",
							idx, informs->__ptrParameterValueStruct[idx]->Name,
							informs->__ptrParameterValueStruct[idx]->Value);
			)
		} else {
			DEBUG_OUTPUT (
					dbglog (SVR_DEBUG, DBG_MAIN, "InfP: %d %s %d\n",
							idx, informs->__ptrParameterValueStruct[idx]->Name,
							*(int *)informs->__ptrParameterValueStruct[idx]->Value);
			)
		}
#endif
	}
	soap->keep_alive = 1;

	ret = soap_call_cwmp__Inform (soap, getServerURL (), "", dev,
				      eventList, 1, time (NULL),
				      getSdRetryCount (), informs,
				      &maxEnvelopes);

	if (ret != SOAP_OK)
	{
	    DEBUG_OUTPUT (
	    		dbglog (SVR_DEBUG, DBG_MAIN, "Exit sendInform count=%d ret=%d soap->errnum=%d FAIL\n",
	    				local_sendInform_count, ret, soap->errnum);
	      )

	    return ret;
	}
	else
	{
		if (soap->header )
		{
			if ( soap->header->cwmp__HoldRequests != 0 )
				setSdHoldRequests( (int)soap->header->cwmp__HoldRequests );
		}		
		clearSdRetryCount ();
		setSdMaxEnvelopes (maxEnvelopes);
		resetParameterStatus ();
		freeEventList ();
		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_MAIN, "SendInform result: %d\n", maxEnvelopes);
		)
	}
	DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_MAIN, "Exit sendInform count = %d ret = SOAP_OK\n", local_sendInform_count);
	)

	return SOAP_OK;
}

static void
sigpipe_handler (int x)
{
	DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_MAIN, "Pipe broke %d\n", x);
	)
}

/** Mainloop to handle ACS SOAP requests.
 * As long as the connection is alive, the requests is executed and the response is sent back.
 * If the connection is closed by the ACS,
 *	by sending an empty request
 *	or a "Connection: close" in the HTTP header, a BROKEN_TRANSACTION is returned.
 *
	"Connection: close"				= close
	"Transfer-Encoding: chunked" 	= chunk
	"Soaplength: 0"					= noMsg
	"Soaplength: >0"				= msg

							close		noMsg		msg		chunk		noChunk
	-----------------------------------------------------------------------------
	FINISH_TRANSACTION		 t|f	      t								    t
	BROKEN_TRANSACTION		 t								   t
 */
static int
mySoap_serve (struct soap *soap)
{
	do
	{
#ifdef _DEBUG
		long startTime = getTime ();
#endif /* _DEBUG */

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_SOAP, "SoapServe st: %ld %d\n", startTime);
		)

		// free unused memory
		soap_destroy(soap);
		efreeTemp();

		soap->error = OK;
/* [v4.0.0c10] */
		soap->length = 0xFFFF;
/**/
		int ret;
		ret = soap_begin_recv(soap);
		if (ret)
		{
			if (soap->error < SOAP_STOP)
			{
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_SOAP, "SoapStatus begin_recv: %d\n", soap->error);
				)
				if (soap->error == 401) {
					http_da_save (soap, &info, ( soap->authrealm ? soap->authrealm : authrealm ),
								  getUsername (), getPassword ());
					soap_connect (soap, getServerURL (), "");
					soap_send( soap, buf401 );
					soap_end_send (soap);

					return CONTINUE_TRANSACTION;
				} else {
					if ( (last_hold_value == 1) && (soap->error == 204) )
					{
						// Don't close soap_closesock, if in previous soap-packet was holdrequest=true and we must to send "HTTP POST empty".
						soap->keep_alive = 1;
						return -1;
					}
					soap_closesock (soap);
					soap->keep_alive = 1;
					return -1;
				}
			}
		}
		/* To identify an empty HTTP Header     take the Content-Length which is 0
		 * Does not work, because the server does not set the Content-Length when handling a normal Request
		 */
		// if "Content-length: 0" is found
		// and no "Transfer-Encoding: chunked" is found
		//      return a FINISH_TRANSACTION
		// keep_alive dosn't matter
		// CLI9
/* [v4.0.0c10] */
		if ( (soap->length == 0 || soap->length == 0xFFFF)
				&& !(( soap->mode & SOAP_IO ) == SOAP_IO_CHUNK))
//		if (soap->length == 0  && !(( soap->mode & SOAP_IO ) == SOAP_IO_CHUNK))
/**/
		{
			soap_closesock (soap);
			return FINISH_TRANSACTION;
		}
		// if "Connection: close" in http header, keep_alive is set to 0 by gsoap
		// and no "Transfer-Encoding: chunked" is found
		//      return a BROKEN_TRANSACTION
		if (soap->keep_alive == 0
		    && !((soap->mode & SOAP_IO) == SOAP_IO_CHUNK))
		{
			soap_closesock (soap);
			soap->keep_alive = 1;
			return BROKEN_TRANSACTION;
		}
		if (soap_envelope_begin_in (soap)
		    || soap_recv_header (soap)
		    || soap_body_begin_in (soap)
			|| soap_serve_request (soap))
		{
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_SOAP, "SoapStatus envelope_begin_in: %d, Tag: %s:\n", soap->error, soap->tag);
			)

			// check for an empty body message and finish the transaction
			if (soap->error == SOAP_NO_METHOD
			    && soap->tag[0] == '\0')
			{
				soap_end_recv (soap);
				soap_closesock (soap);
				return FINISH_TRANSACTION;
			}
			else
				return SOAP_FAULT;
		}

		// San. 05 July 2011:
		last_hold_value = getSdHoldRequests();
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_SOAP, "Exit SoapServe lz: %ld\n", getTime () - startTime);
		)
	}
	while (soap->keep_alive);
	return SOAP_OK;
}

#ifdef	WITH_COOKIES
/** Handles cookies
 * 	Modify the server cookies otherwise the server can't identify us
*/
static int
cookieHandler (struct soap *soap)
{
/*	 clear the cookie path extension, other	the	server can't recognize the client */
	struct soap_cookie *cookie;
	if (soap->cookies != NULL)
	{
		/* Set a cookie for the charCode conversion
		 */
		cookie = (struct soap_cookie *) soap_set_cookie (soap,
								 "CharCode",
								 "293a1567c77f25780de94981d4b8b907ba280ee2baa0c4",
								 NULL,
								 "/");
		cookie->expire = 0L;
		// clear the cookie path
		cookie = soap->cookies;
		while( cookie != NULL ) {
			cookie->path = NULL;
			cookie = cookie->next;
//			soap->cookies->path = NULL;
		}
	}

	return OK;
}
#endif

/* Parameter Initialization
*/
static void
initParameters (bool bootstrap)
{
	loadParameters (bootstrap);
}

/* Frees temporary allocated memory from gSOAP and Client
*/
static void
freeMemory (struct soap *soap)
{
	/* Remove temporary data and deserialized data
	 */
	soap_end (soap);
	/* free the memory allocated with emallocTemp()
	 */
	efreeTemp ();
	/** free session memory
	 */
	efreeSession();
}

/** Only parse the Host
 * and the Authorization Digest
 */
static int
myMainHttpParseHeader (struct soap *soap, const char *key, const char *val)
{
	// Check for Connection: close in http header. if found we have to close
	// connection after command done
	if (!soap_tag_cmp(key, "Connection")) {
    	if (!soap_tag_cmp(val, "close"))
      		connectionClose = true;
  	}

	return soap_parse_header(soap, key, val);
}

static int
sendFault( struct soap *soap )
{
	soap_set_fault(soap);
//	soap_closesock(soap);
	soap->keep_alive = 1;
	soap_serializeheader(soap);
	soap_serializefault(soap);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{
		soap_envelope_begin_out(soap);
	  	soap_putheader(soap);
	  	soap_body_begin_out(soap);
	  	soap_putfault(soap);
	  	soap_body_end_out(soap);
	  	soap_envelope_end_out(soap);
	}
	soap_end_count(soap);

	if (soap_connect( soap, getServerURL(), "" )
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_putfault(soap)
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap))
	  return soap_closesock(soap);
	soap_end_send(soap);

	return OK;
}

#ifdef HAVE_RPC_METHODS
static int
clearGetRPCMethods (struct soap *server)
{
	static	bool first_rpc = true;
	int i;
	void *_ = NULL;
	struct	ArrayOfString MethodList;

	if (first_rpc)
	{
#ifdef HAVE_KICKED
		isKicked = false;
#endif /* HAVE_KICKED */

#ifdef HAVE_REQUEST_DOWNLOAD
		isRequestDownload = false;
#endif /* HAVE_REQUEST_DOWNLOAD */

#ifdef	 HAVE_AUTONOMOUS_TRANSFER_COMPLETE
		isAutonomousTransferComplete = false;
#endif /* HAVE_AUTONOMOUS_TRANSFER_COMPLETE */

		soap_call_cwmp__GetRPCMethods(server, getServerURL (), "",
										_,
										&MethodList);

		for (i= 0; i < MethodList.__size; i++)
		{
#ifdef	 HAVE_AUTONOMOUS_TRANSFER_COMPLETE
			if (!strcmp ("AutonomousTransferComplete", MethodList.__ptrstring[i]))
				isAutonomousTransferComplete = true;
#endif /* HAVE_AUTONOMOUS_TRANSFER_COMPLETE */

#ifdef HAVE_REQUEST_DOWNLOAD
			if (!strcmp ("RequestDownload", MethodList.__ptrstring[i]))
				isRequestDownload = true;
#endif /* HAVE_REQUEST_DOWNLOAD */

#ifdef HAVE_KICKED
			if (!strcmp ("Kicked", MethodList.__ptrstring[i]))
				isKicked = true;
#endif /* HAVE_KICKED */
		}
	}
	first_rpc = false;

	return OK;
}
#endif /* HAVE_RPC_METHODS */

/** Authorize a client against an ACS which needs the Authorization Header
*   every time the clients sends a request
*/
int
authorizeClient( struct soap *soap, struct http_da_info *info )
{
	soap->userid = (char *)getUsername();
	soap->passwd = (char *)getPassword();

	http_da_restore( soap, info );
	// CLI9
	return SOAP_OK;
}

/** De/Serializers for special type xsd__boolean_
 * This type is used for the SOAP header structure.
 * The value can be read, but will never be written.
 */
enum _Enum_1 *soap_in_xsd__boolean_(struct soap *soap, const char *tag, enum _Enum_1 *a, const char *type)
{
	return soap_in_xsd__boolean(soap, tag, a, type);
}

int soap_out_xsd__boolean_(struct soap *soap, const char *tag, int id, const enum _Enum_1 *a, const char *type)
{
	// clear the mustUnderstand flag
	soap->mustUnderstand = 0;
	return SOAP_OK; // soap_out_xsd__boolean(soap, tag, id, a, type);
}

void soap_default_xsd__boolean_(struct soap *soap, enum _Enum_1 *a)
{
	soap_default_xsd__boolean(soap, a);
}
