/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_HOST

#include <signal.h>
#include <pthread.h>

#include "hosthandler.h"
#include "globals.h"
#include "option.h"
#include "parameter.h"
#include "voipParameter.h"

/* this define is used to control the access to the client notification
 * if LOCALHOST is set to NULL an access over the net is allowed
 * if LOCALHOST is set to "127.0.0.1" or "localhost", the access is restricted to CPE local
 */
#define LOCALHOST NULL

#define 	GET_CMD_STR					"get"
#define 	SET_CMD_STR					"set"
#define 	ADD_CMD_STR					"add"
#define 	DEL_CMD_STR					"del"
#define 	CLEAR_OPTION_CMD_STR		"opt"
#define 	VENDOR_INFO_				'v'
#define 	GET_VENDOR_INFO_CMD_STR		"vg"
#define 	SET_VENDOR_INFO_CMD_STR		"vs"
#define 	DEVICE_INFO_				'd'
#define 	ADD_DEVICE_INFO_CMD_STR		"da"
#define 	REMOVE_DEVICE_INFO_CMD_STR	"dr"
#define 	GATEWAY_INFO_				'g'
#define 	GET_GATEWAY_INFO_CMD_STR	"gg"
#define 	SET_GATEWAY_INFO_CMD_STR	"gs"
#define		DHCP_						'h'
#define		DHCP_DISCOVERY_STR			"hd"
#define		DHCP_REQUEST_STR			"hr"
#define		DHCP_INFORM_STR				"hi"
#define		DHCP_OFFER_STR				"ho"
#define		DHCP_ACK_STR				"ha"

#define 	CMD						0
#define 	GET_CMD					1
#define 	SET_CMD					2
#define 	CLEAR_OPTION_CMD		3
#define 	GET_VENDOR_INFO_CMD		4
#define 	SET_VENDOR_INFO_CMD		5
#define 	ADD_DEVICE_INFO_CMD		6
#define 	REMOVE_DEVICE_INFO_CMD	7
#define 	GET_GATEWAY_INFO_CMD	8
#define 	SET_GATEWAY_INFO_CMD	9
#define 	ADD_OBJECT_CMD			10
#define 	DELETE_OBJECT_CMD		11
#define		DHCP_DISCOVERY_CMD		12
#define		DHCP_REQUEST_CMD		13
#define		DHCP_INFORM_CMD			14
#define		DHCP_OFFER_CMD			15
#define		DHCP_ACK_CMD			16

// Define the maximal length of the parameters
#define MANUFACTURER_OUI_LENGTH		6
#define SERIAL_NUMBER_LENGTH		64
#define PRODUCT_CLASS_LENGTH		64

#define BUFFER_SIZE				1024

// used to inform the mainloop that the hostHandler could bind on the port.
// and the DHCP client could
//
pthread_cond_t hostHandlerStarted = PTHREAD_COND_INITIALIZER;
pthread_mutex_t hostHandlerMutexLock = PTHREAD_MUTEX_INITIALIZER;

extern pthread_mutex_t paramLock;
extern char host[];

static char buf[BUFFER_SIZE + 1];
static	struct soap *soap;
extern	int procId;

// Function prototypes
static int cmdstr2int( char * );
static int readLine( char *, int );

#ifdef TR_111_DEVICE
static const char *getGwManufacturerOUIPath( void );
static const char *getGwSerialNumberPath( void );
static const char *getGwProductClassPath( void );
static int setVendorData( const char *, char *, bool * );
#endif

static int getVendorData( char **, char **, char **);
#if defined( TR_111_DEVICE ) || defined( TR_111_GATEWAY )
static const char *getDvManufacturerOUIPath( void );
static const char *getDvSerialNumberPath( void );
static const char *getDvProductClassPath( void );
#endif

static int sendInt( int );
static int sendUInt( unsigned int );
static int sendEOL( void );
static int sendEOM( void );
static int sendChar( const char * );

// Buffers to store the values coming from outside
#ifdef TR_111_GATEWAY
static char deviceManufacturerOUI[MANUFACTURER_OUI_LENGTH+1];
static char deviceSerialNumber[SERIAL_NUMBER_LENGTH+1];
static char deviceProductClass[PRODUCT_CLASS_LENGTH+1];

static int removeClientData( void );
static int addClientData( bool * );
static	int	IfIsNotaddClientData( bool * );
static const char *getManageableDevicePath( void );
#endif

#if defined( TR_111_DEVICE ) && !defined( TR_111_DEVICE_WITHOUT_GATEWAY )

static void	DHCP_Discover( void * );
static void	DHCP_Request( void * );

void	DHCP_Discover_dimarkMain( void )
{
	DHCP_Discover( soap );
}

static void	DHCP_Discover( void *localSoap )
{
	soap = (struct soap *) localSoap;
	int ret = OK;
	bool notify = false;
	bool notifyTmp = false;
	struct timespec delay;
	delay.tv_sec = 1;
	delay.tv_nsec = 0;
	char *manuOUI, *serialNumber, *productClass;
	char *tmp;

	DEBUG_OUTPUT (
		dbg (DBG_HOST, "put_msg: DHCP_Discover\n");
	)

	getParameter( DEFAULT_GATEWAY, &tmp );
	soap_connect (soap, tmp, "");
	soap_begin_send(soap);

	sendChar( "hd" );sendEOL();
	getVendorData( &manuOUI, &serialNumber, &productClass );

	DEBUG_OUTPUT (
			dbg (DBG_HOST, "VendorInfo: %s %s %s\n", manuOUI, serialNumber, productClass);
	)

	sendChar( manuOUI ); sendEOL();
	sendChar( serialNumber ); sendEOL();
	sendChar( productClass ); sendEOL();
	soap_end_send (soap);

	readLine( buf, BUFFER_SIZE ); /* Error */

	DEBUG_OUTPUT (
			dbg (DBG_HOST, "Buf: %s\n", buf);
	)

	readLine( buf, BUFFER_SIZE ); /* New Line */

	DEBUG_OUTPUT (
			dbg (DBG_HOST, "Buf: %s\n", buf);
	)

	ret = readLine( buf, BUFFER_SIZE );

	DEBUG_OUTPUT (
			dbg (DBG_HOST, "Buf: %s\n", buf);
	)

	if (cmdstr2int( buf ) == DHCP_OFFER_CMD)
	{
		notify = false;

		if ( readLine( buf, BUFFER_SIZE ) == OK ) {
			DEBUG_OUTPUT (
				dbg (DBG_HOST, "GatewayInfo: %s %s\n", getGwManufacturerOUIPath(), buf);
			)

			ret = setParameter2Host( getGwManufacturerOUIPath(), &notifyTmp, buf );

			if ( ret == OK ) {
				notify |= notifyTmp;
			}
		}

		if ( readLine( buf, BUFFER_SIZE ) == OK ) {
			DEBUG_OUTPUT (
				dbg (DBG_HOST, "GatewayInfo: %s %s\n", getGwSerialNumberPath(), buf);
			)
			ret = setParameter2Host( getGwSerialNumberPath(), &notifyTmp, buf );
			if ( ret == OK ) {
				notify |= notifyTmp;
			}
		}

		if ( readLine( buf, BUFFER_SIZE ) == OK ) {
			DEBUG_OUTPUT (
				dbg (DBG_HOST, "GatewayInfo: %s %s\n", getGwProductClassPath(), buf);
			)
			ret = setParameter2Host( getGwProductClassPath(), &notifyTmp, buf );
			if ( ret == OK ) {
				notify |= notifyTmp;
			}
		}

		DEBUG_OUTPUT (
			dbg (DBG_HOST, "GatewayInfo Notify: %d\n", notifyTmp );
		)
		if ( readLine( buf, BUFFER_SIZE ) == OK ) {
			DEBUG_OUTPUT (
				dbg (DBG_HOST, "ACS URL: %s\n", buf);
			)
			ret = setParameter2Host( MANAGEMENT_SERVER_URL, &notifyTmp, buf );
			if ( ret == OK ) {
				notify |= notifyTmp;
			}
		}
		DEBUG_OUTPUT (
			dbg (DBG_HOST, "GatewayInfo Notify: %d\n", notifyTmp );
		)
		sendInt( ret );sendEOL();
		sendEOL();
	}
}

static void	DHCP_Request( void *localSoap )
{
		soap = (struct soap *) localSoap;
		int ret = OK;
		bool notify = false;
		bool notifyTmp = false;
		struct timespec delay, rem;
		delay.tv_sec = 1;
		delay.tv_nsec = 0;
		char *manuOUI, *serialNumber, *productClass;
		char *tmp;

	DEBUG_OUTPUT (
		dbg (DBG_HOST, "put_msg: DHCP_Request\n");
	)
	getParameter( DEFAULT_GATEWAY, &tmp );
	soap_connect (soap, tmp, "");
	soap_begin_send(soap);

	sendChar( "hr");sendEOL();
	getVendorData( &manuOUI, &serialNumber, &productClass );
	DEBUG_OUTPUT (
		dbg (DBG_HOST, "VendorInfo: %s %s %s\n", manuOUI, serialNumber, productClass);
	)
	sendChar( manuOUI ); sendEOL();
	sendChar( serialNumber ); sendEOL();
	sendChar( productClass ); sendEOL();
	soap_end_send (soap);

	readLine( buf, BUFFER_SIZE ); /* Error */

	DEBUG_OUTPUT (
			dbg (DBG_HOST, "Buf: %s\n", buf);
	)

	readLine( buf, BUFFER_SIZE ); /* New Line */

	DEBUG_OUTPUT (
			dbg (DBG_HOST, "Buf: %s\n", buf);
	)

	ret = readLine( buf, BUFFER_SIZE );

	DEBUG_OUTPUT (
			dbg (DBG_HOST, "Buf: %s\n", buf);
	)

	if (cmdstr2int( buf ) == DHCP_ACK_CMD)
	{

	notify = false;
	if ( readLine( buf, BUFFER_SIZE ) == OK ) {
		DEBUG_OUTPUT (
			dbg (DBG_HOST, "GatewayInfo: %s %s\n", getGwManufacturerOUIPath(), buf);
		)
		ret = setParameter2Host( getGwManufacturerOUIPath(), &notifyTmp, buf );
		if ( ret == OK ) {
			notify |= notifyTmp;
		}
	}
	if ( readLine( buf, BUFFER_SIZE ) == OK ) {
		DEBUG_OUTPUT (
			dbg (DBG_HOST, "GatewayInfo: %s %s\n", getGwSerialNumberPath(), buf);
		)
		ret = setParameter2Host( getGwSerialNumberPath(), &notifyTmp, buf );
		if ( ret == OK ) {
			notify |= notifyTmp;
		}
	}
	if ( readLine( buf, BUFFER_SIZE ) == OK ) {
		DEBUG_OUTPUT (
			dbg (DBG_HOST, "GatewayInfo: %s %s\n", getGwProductClassPath(), buf);
		)
		ret = setParameter2Host( getGwProductClassPath(), &notifyTmp, buf );
		if ( ret == OK ) {
			notify |= notifyTmp;
		}
	}
	DEBUG_OUTPUT (
		dbg (DBG_HOST, "GatewayInfo Notify: %d\n", notifyTmp );
	)
	sendInt( ret );sendEOL();
	sendEOL();
	}

	// release the mainloop
	nanosleep( &delay, &rem );
	pthread_mutex_lock (&hostHandlerMutexLock);
	pthread_cond_signal (&hostHandlerStarted);
	pthread_mutex_unlock (&hostHandlerMutexLock);
}
#endif

void *
hostHandler (void *localSoap)
{
	char paramPath[257];
	char *manuOUI, *serialNumber, *productClass;
	int ret = OK;
	ParameterType type;
	AccessType access;
	void *data;
	bool notify = false;
	bool notifyTmp = false;
	struct timespec delay, rem;
	delay.tv_sec = 1;
	delay.tv_nsec = 0;
	soap = (struct soap *) localSoap;
	int cmd = CMD;
	int instance;
	int status;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_HOST, "HostPort: %d\n", (HOST_NOTIFICATION_PORT + (procId)));
	)

	soap->accept_timeout = 0;
	soap->recv_timeout = 1;
	soap->keep_alive = 1;
	ret = soap_bind (soap, host, HOST_NOTIFICATION_PORT + (procId), 100);

	while (ret < SOAP_OK)
	{
		soap_closesock (soap);
		ret = soap_bind (soap, host, HOST_NOTIFICATION_PORT + (procId), 100);
		nanosleep( &delay, &rem );
	}

//	TR-111. Part 1. Device.
#if defined( TR_111_DEVICE ) && !defined( TR_111_DEVICE_WITHOUT_GATEWAY )
	char *tmp;

	getParameter( MANAGEMENT_SERVER_URL, &tmp );

	if ( !*tmp )
		DHCP_Discover( soap );

	DHCP_Request( soap );
#endif

#if defined( TR_111_DEVICE ) == false || defined( TR_111_DEVICE_WITHOUT_GATEWAY )
	// tell the mainloop we are ready to serve the Host ConnectionRequests

	// do a short nap to give the mainloop time to setup the mutex
	nanosleep( &delay, &rem );
	pthread_mutex_lock (&hostHandlerMutexLock);
	pthread_cond_signal (&hostHandlerStarted);
	pthread_mutex_unlock (&hostHandlerMutexLock);
#endif
		
	while (true)
	{
		//soap_closesock (soap);	
		while (true)  
		{
			bzero(buf, sizeof( buf ));
			ret = soap_accept (soap);
			if ( ret < 0  ) {
				soap_closesock(soap);
				continue;
			}
			ret = readLine( buf, BUFFER_SIZE );
			if ( ret < 0 )
				break;

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_HOST, "Buf: %s\n", buf);
			)

			cmd = cmdstr2int( buf );
			
			switch ( cmd ) {
				/* HostSystem tries to get the value of a parameter */
				/* The parameter is requested by a
				 * get NL
				 * <parameterpath> NL
				 * and delivered as
				 * <type> NL
				 * <parametervalue as string>
				 */
				case GET_CMD:
					if (readLine( paramPath, 256) != SOAP_EOF)
					{
						DEBUG_OUTPUT (
								dbglog (SVR_INFO, DBG_HOST, "get: %s\n", buf);
						)
						ret = getParameter2Host (paramPath, &type, &access, &data);
						if (ret == OK)
						{
							sendInt (type); sendEOL();
							switch (type)
							{
								case StringType:
								case Base64Type:
									DEBUG_OUTPUT (
											dbglog (SVR_INFO, DBG_HOST, "Type: %d %s\n", type, (char*)data);
									)
									sendChar ((const char*) data);
									break;
								case DateTimeType:
								case UnsignedIntType:
									DEBUG_OUTPUT (
											dbglog (SVR_INFO, DBG_HOST, "Type: %d %u\n", type, *(unsigned int*)data);
									)
									sendUInt(*(unsigned int*)data);
									break;
								default:
									DEBUG_OUTPUT (
											dbglog (SVR_INFO, DBG_HOST, "Type: %d %d\n", type, *(int*)data);
									)
									sendInt(*(int*)data);
							}
						}
						else
						{
							sendInt (ret);
						}
						sendEOL ();
						sendEOM ();
						notify = false;
					}
					break;
				case SET_CMD:
					/* the HostSystem tries to change a parameter value */
					/* The change is requested by
					 * set NL
					 * <parameterpath> NL
					 * <parametervalue as string> NL
					 * A notification is sent to the ACS if necessary.
					 * The parameter value is restricted to 1024 chars
					 */
					if (readLine (paramPath, 256) == OK)
					{
						if (readLine (buf, BUFFER_SIZE) == OK)
						{
							DEBUG_OUTPUT (
									dbglog (SVR_INFO, DBG_HOST, "set: %s %s\n", paramPath, buf);
							)
							pthread_mutex_lock(&paramLock);
							ret = setParameter2Host(paramPath, &notifyTmp, buf);
							pthread_mutex_unlock(&paramLock);
							sendInt (ret);
							sendEOL();
							sendEOM();
							notify |= notifyTmp;
							DEBUG_OUTPUT (
									dbglog (SVR_INFO, DBG_HOST, "set notify: %d\n", notify);
							)
//							continue;
						}
					}
					break;
				case ADD_OBJECT_CMD:
					/* the HostSystem want's to add new instance for an object
					 * add NL
					 * <objectpath> NL
					 * The instance number of the new created object is returned, or
					 * if an error occurred a 0 is returned.
					 */
					if (readLine (paramPath, 256) == OK)
					{
						DEBUG_OUTPUT (
								dbglog (SVR_INFO, DBG_HOST, "add: %s\n", paramPath);
						)
						pthread_mutex_lock(&paramLock);
						ret = addObjectIntern(paramPath, &instance);
						pthread_mutex_unlock(&paramLock);
						if ( ret != OK )
							sendInt ( 0 );
						else
							sendInt (instance);
						sendEOL();
						sendEOM();
//						continue;
					}
					break;				
				case DELETE_OBJECT_CMD:
					/* the HostSystem want's to delete an instance object
					 * del NL
					 * <objectpath> NL
					 * if no error occurred a OK is returned.
					 */
					if (readLine ( paramPath, 256) == OK)
					{
						DEBUG_OUTPUT (
								dbglog (SVR_INFO, DBG_HOST, "del: %s\n", paramPath);
						)
						pthread_mutex_lock(&paramLock);
						ret = deleteObject(paramPath, &status);
						pthread_mutex_unlock(&paramLock);
						sendInt ( ret );
						sendEOL();
						sendEOM();
//						continue;
					}
					break;				
#ifdef HAVE_VOUCHERS_OPTIONS					
				case CLEAR_OPTION_CMD:
					/* the HostSystem want delete an option which has timed out */
					/* request
					 * opt  NL
					 * <voucherSN> NL
					 * remove NL
					 */
					if (readLine(paramPath, 256) == OK)
					{
						if (readLine(buf, BUFFER_SIZE) == OK)
						{
							DEBUG_OUTPUT (
									dbglog (SVR_INFO, DBG_HOST, "opt: %s %s\n", paramPath, buf);
							)
							ret = handleHostOption(paramPath, buf);
							sendInt (ret);
							sendEOL();
							sendEOM();
							continue;
						}
					}
					break;
#endif /* HAVE_VOUCHERS_OPTIONS */
				/** Get the vendor info of this device
				* the vendor info is stored in manuOUI, serialNumber and productClass
				* this CMD is used by the DHCP to get the DEVICE or GATEWAY Vendor information
				* Sends:
				*	<returncode><EOL>
				*	<manufactorerOUI><EOL>
				*	<serialNumbe1><EOL>
				*	<productClass><EOL>
				*	<EOL>
				*/
#if defined(TR_111_DEVICE) || defined( TR_111_GATEWAY )
				case GET_VENDOR_INFO_CMD:
					ret = getVendorData( &manuOUI, &serialNumber, &productClass );
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "VendorInfo: %s %s %s\n", manuOUI, serialNumber, productClass);
					) 
					sendInt( ret ); sendEOL();
					sendChar( manuOUI ); sendEOL();
					sendChar(  serialNumber ); sendEOL();
					sendChar(  productClass ); sendEOL();
					sendEOM();
					break;
#endif
				/** Set the gateway vendor info for this device.
				* only useful if this is a device and TR_111_DEVICE is set.
				* Gets the ManufacturerOUI, SerialNumber and the ProductClass from the Gateway 
				* and stores it in 
				*	Device.GatewayInfo.ManufacturerOUI
				*	Device.GatewayInfo.SerialNumber
				*	Device.GatewayInfo.ProductClass
				* The DHCP Server always must send a line even if the value is empty.
				*
				* Sends:
				*	<returncode><EOL>
				*	<EOL>
				*
				*/
#ifdef TR_111_DEVICE
				case SET_VENDOR_INFO_CMD:
					notify = false;
					if ( readLine( buf, BUFFER_SIZE ) == OK ) {
						DEBUG_OUTPUT (
								dbglog (SVR_INFO, DBG_HOST, "GatewayInfo: %s %s\n", getGwManufacturerOUIPath(), buf);
						)
						ret = setVendorData( getGwManufacturerOUIPath(), buf, &notifyTmp );
						if ( ret == OK ) {
							notify |= notifyTmp;
						}
					}
					if ( readLine( buf, BUFFER_SIZE ) == OK ) {
						DEBUG_OUTPUT (
								dbglog (SVR_INFO, DBG_HOST, "GatewayInfo: %s %s\n", getGwSerialNumberPath(), buf);
						)
						ret = setVendorData( getGwSerialNumberPath(), buf, &notifyTmp );
						if ( ret == OK ) {
							notify |= notifyTmp;
						}
					}
					if ( readLine( buf, BUFFER_SIZE ) == OK ) {
						DEBUG_OUTPUT (
								dbglog (SVR_INFO, DBG_HOST, "GatewayInfo: %s %s\n", getGwProductClassPath(), buf);
						)
						ret = setVendorData( getGwProductClassPath(), buf, &notifyTmp );
						if ( ret == OK ) {
							notify |= notifyTmp;
						}
					}
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "GatewayInfo Notify: %d\n", notifyTmp );
					)
					sendInt( ret );sendEOL();
					sendEOM();
					// release the mainloop
					nanosleep( &delay, &rem );
					pthread_mutex_lock (&hostHandlerMutexLock);
					pthread_cond_signal (&hostHandlerStarted);
					pthread_mutex_unlock (&hostHandlerMutexLock);
					break;
#endif
				/** Add the device vendor info into a gateway device parameter database.
				*
				*/
#ifdef TR_111_GATEWAY
					case ADD_DEVICE_INFO_CMD:
					// ManufacturerOUI
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceManufacturerOUI, buf, MANUFACTURER_OUI_LENGTH );
					// SerialNumber
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceSerialNumber, buf, SERIAL_NUMBER_LENGTH );
					// ProductClasetParameter2Hostss
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceProductClass, buf, PRODUCT_CLASS_LENGTH );
					ret = addClientData( &notifyTmp );
					if ( ret == OK ) {
						notify |= notifyTmp;
					}
					sendInt( ret ); sendEOL();
					sendEOM();
					break;
#endif
				/** Remove the device from the ManageableDevice List in a gateway
				*/
#ifdef TR_111_GATEWAY
				case REMOVE_DEVICE_INFO_CMD:
					// ManufacturerOUI
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceManufacturerOUI, buf, MANUFACTURER_OUI_LENGTH );
					// SerialNumber
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceSerialNumber, buf, SERIAL_NUMBER_LENGTH );
					// ProductClass
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceProductClass, buf, PRODUCT_CLASS_LENGTH );
					ret = removeClientData();
					sendInt( ret ); sendEOL();
					sendEOM();
					break;
#endif

#if defined( TR_111_GATEWAY )
				case DHCP_DISCOVERY_CMD:
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "get_msg: DHCP_Discover\n");
					)
					// ManufacturerOUI
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceManufacturerOUI, buf, MANUFACTURER_OUI_LENGTH );
					// SerialNumber
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceSerialNumber, buf, SERIAL_NUMBER_LENGTH );
					// ProductClass
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceProductClass, buf, PRODUCT_CLASS_LENGTH );
					sendInt( ret ); sendEOL();
					sendEOM();

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "put_msg: DHCP_Offer\n");
					)
					sendChar( "ho" );sendEOL();
					getVendorData( &manuOUI, &serialNumber, &productClass );
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "VendorInfo: %s %s %s\n", manuOUI, serialNumber, productClass);
					)
					sendChar( manuOUI ); sendEOL();
					sendChar( serialNumber ); sendEOL();
					sendChar( productClass ); sendEOL();
					char *url;
					getParameter (MANAGEMENT_SERVER_URL, &url);
					sendChar( url ); sendEOL();
				break;

				case DHCP_REQUEST_CMD:
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "get_msg: DHCP_Request\n");
					)
					// ManufacturerOUI
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceManufacturerOUI, buf, MANUFACTURER_OUI_LENGTH );
					// SerialNumber
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceSerialNumber, buf, SERIAL_NUMBER_LENGTH );
					// ProductClass
					if ( readLine( buf, BUFFER_SIZE ) == OK )
						strncpy( deviceProductClass, buf, PRODUCT_CLASS_LENGTH );

					ret = IfIsNotaddClientData( &notifyTmp );

					if ( ret == OK ) {
						notify |= notifyTmp;
					}
					sendInt( ret ); sendEOL();
					sendEOM();

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "put_msg: DHCP_Ack\n");
					)
					sendChar( "ha" );sendEOL();
					getVendorData( &manuOUI, &serialNumber, &productClass );
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_HOST, "VendorInfo: %s %s %s\n", manuOUI, serialNumber, productClass);
					)
					sendChar( manuOUI ); sendEOL();
					sendChar( serialNumber ); sendEOL();
					sendChar( productClass ); sendEOL();
				break;

				case DHCP_INFORM_CMD:

				break;
#endif
				default:
					DEBUG_OUTPUT (
							dbglog (SVR_ERROR, DBG_HOST, "Leave loop\n");
					)
					break;			
			}

//#ifdef TR_111_GATEWAY
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_HOST, "Host Notify: %d\n", notify);
			)
			if (notify)
			{
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_HOST, "Create inform message\n");
				)
				setAsyncInform(true);
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_HOST, "Create inform message done \n");
				)
				notify = false;
			}
		}
//#endif
	}

	pthread_exit (NULL);
}

static
int cmdstr2int( char *buf )
{
	int cmd = 0;
	// First convert the command string into a number for better handling
	if ( strncasecmp (buf, GET_CMD_STR, 3) == 0 ) {
		cmd = GET_CMD;
	} else
	if ( strncasecmp (buf, SET_CMD_STR, 3) == 0 ) {
		cmd = SET_CMD;
	} else
	if ( strncasecmp (buf, ADD_CMD_STR, 3) == 0 ) {
		cmd = ADD_OBJECT_CMD;
	} else
	if ( strncasecmp (buf, DEL_CMD_STR, 3) == 0 ) {
		cmd = DELETE_OBJECT_CMD;
	} else
	if ( strncasecmp (buf, CLEAR_OPTION_CMD_STR, 3) == 0 ) {
		cmd = CLEAR_OPTION_CMD;
	} else
	if ( buf[0] == VENDOR_INFO_ ) {
		if ( strncasecmp( buf, GET_VENDOR_INFO_CMD_STR, 2 ) == 0 )
			cmd = GET_VENDOR_INFO_CMD;
		else
		if ( strncasecmp( buf, SET_VENDOR_INFO_CMD_STR, 2 ) == 0 )
			cmd = SET_VENDOR_INFO_CMD;
	} else 
	if ( buf[0] == DEVICE_INFO_ ) {
		if ( strncasecmp( buf, ADD_DEVICE_INFO_CMD_STR, 2 ) == 0 )
			cmd = ADD_DEVICE_INFO_CMD;
		else
		if ( strncasecmp( buf, REMOVE_DEVICE_INFO_CMD_STR, 2 ) == 0 )
			cmd = REMOVE_DEVICE_INFO_CMD;
		
	} else
	if ( buf[0] == GATEWAY_INFO_ ) {
		if ( strncasecmp( buf, GET_GATEWAY_INFO_CMD_STR, 2 ) == 0 )
			cmd = GET_GATEWAY_INFO_CMD;
		else
		if ( strncasecmp( buf, SET_GATEWAY_INFO_CMD_STR, 2 ) == 0 )
			cmd = SET_VENDOR_INFO_CMD;
	} else
		if ( buf[0] == DHCP_ ) {
			if ( strncasecmp( buf, DHCP_DISCOVERY_STR, 2 ) == 0 )
				cmd = DHCP_DISCOVERY_CMD;
			else
			if ( strncasecmp( buf, DHCP_REQUEST_STR, 2 ) == 0 )
				cmd = DHCP_REQUEST_CMD;
			else
			if ( strncasecmp( buf, DHCP_INFORM_STR, 2 ) == 0 )
				cmd = DHCP_INFORM_CMD;
			else
			if ( strncasecmp( buf, DHCP_OFFER_STR, 2 ) == 0 )
				cmd = DHCP_OFFER_CMD;
			else
			if ( strncasecmp( buf, DHCP_ACK_STR, 2 ) == 0 )
				cmd = DHCP_ACK_CMD;

		} else
			cmd = CMD;

	return cmd;
}

#if defined(TR_111_DEVICE) || defined(TR_111_GATEWAY)
static int getVendorData( char **manuOUI, char **serialNumber, char **productClass )
{
	ParameterType type;
	AccessType access;
	int ret = OK;

	ret = getParameter2Host( getDvManufacturerOUIPath(), &type, &access, manuOUI );

	if ( ret > OK )
		return ret;

	ret = getParameter2Host( getDvSerialNumberPath(), &type, &access, serialNumber );

	if ( ret > OK )
		return ret;

	ret = getParameter2Host( getDvProductClassPath(), &type, &access, productClass );

	return ret;	
}
#endif

#ifdef TR_111_DEVICE
static int setVendorData( const char *path, char *value, bool *notifyTmp )
{
	int ret = OK;

	pthread_mutex_lock(&paramLock);
	ret = setParameter2Host( path, notifyTmp, value );
	pthread_mutex_unlock(&paramLock);

	return ret;
}
#endif

#ifdef TR_111_GATEWAY
static int IfIsNotaddClientData(bool *notifyTmp )
{
	int ret = OK;
	int *count = 0;
	ParameterType type;
	AccessType access;
	char *data;
	int i;

	ret = getParameter2Host( "InternetGatewayDevice.ManagementServer.ManageableDeviceNumberOfEntries", &type, &access, &count );
	for( i = 1; i <= *count; i ++ ) {
		sprintf( buf, "%s%d.%s", getManageableDevicePath(), i, "ManufacturerOUI" );
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_HOST, "ManageableDevice: %s %s\n", buf, deviceManufacturerOUI);
		)
		ret = getParameter2Host( buf, &type, &access, &data );
		if ( ret == OK ) {
			if ( strncmp( data, deviceManufacturerOUI, MANUFACTURER_OUI_LENGTH ) == 0 ) {
				sprintf( buf, "%s%d.%s", getManageableDevicePath(), i, "SerialNumber" );
				ret = getParameter2Host( buf, &type, &access, &data );
				if ( ret == OK ) {
					if ( strncmp( data, deviceSerialNumber, SERIAL_NUMBER_LENGTH ) == 0 ) {
						sprintf( buf, "%s%d.%s", getManageableDevicePath(), i, "ProductClass" );
						ret = getParameter2Host( buf, &type, &access, &data );
						if ( ret == OK ) {
							if ( strncmp( data, deviceProductClass, PRODUCT_CLASS_LENGTH ) == 0 ) {
								return	ret;
							}
						}
					}
				}
			}
		}
	}

	return		addClientData( notifyTmp );
}
#endif

#ifdef TR_111_GATEWAY
static int addClientData( bool *notifyTmp )
{
	int ret = OK;
	int instance = 0;

	ret = addObjectIntern( getManageableDevicePath(), &instance );
	if ( ret == OK ) {
		sprintf( buf, "%s%d.%s", getManageableDevicePath(), instance, "ManufacturerOUI" );
		DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_HOST, "ManageableDevice: %s %s\n", buf, deviceManufacturerOUI);
		)
		ret = setParameter2Host( buf, notifyTmp, deviceManufacturerOUI );
	}
	if ( ret == OK ) {
		sprintf( buf, "%s%d.%s", getManageableDevicePath(), instance, "SerialNumber" );
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_HOST, "ManageableDevice: %s %s\n", buf, deviceSerialNumber);
		)
		ret = setParameter2Host( buf, notifyTmp, deviceSerialNumber );
	}
	if ( ret == OK ) {
		sprintf( buf, "%s%d.%s", getManageableDevicePath(), instance, "ProductClass" );
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_HOST, "ManageableDevice: %s %s\n", buf, deviceProductClass);
		)
		ret = setParameter2Host( buf, notifyTmp, deviceProductClass );
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_HOST, "ManageableDevice: ret %d instance %d\n", ret, instance);
	)
	return ret;
}
#endif

#ifdef TR_111_GATEWAY
static int removeClientData (void)
{
	int ret = OK;
	int *count = 0;
	ParameterType type;
	int status;
	AccessType access;
	char *data;
#ifdef _DEBUG
	int instance = 0;
#endif /* _DEBUG */
	int i;

	/**Discover the InstanceNumber entry in the ManageableDevice list.
       If instance's values that are found match, the instance should be removed with
       the help of DeleteObject () function.
	*/
	ret = getParameter2Host( "InternetGatewayDevice.ManagementServer.ManageableDeviceNumberOfEntries", &type, &access, &count );

	for( i = 1; i <= *count; i ++ ) {
		sprintf( buf, "%s%d.%s", getManageableDevicePath(), i, "ManufacturerOUI" );
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_HOST, "ManageableDevice: %s %s\n", buf, deviceManufacturerOUI);
		)
		ret = getParameter2Host( buf, &type, &access, &data );
		if ( ret == OK ) {
			if ( strncmp( data, deviceManufacturerOUI, MANUFACTURER_OUI_LENGTH ) == 0 ) {
				sprintf( buf, "%s%d.%s", getManageableDevicePath(), i, "SerialNumber" );
				ret = getParameter2Host( buf, &type, &access, &data );
				if ( ret == OK ) {
					if ( strncmp( data, deviceSerialNumber, SERIAL_NUMBER_LENGTH ) == 0 ) {
						sprintf( buf, "%s%d.%s", getManageableDevicePath(), i, "ProductClass" );
						ret = getParameter2Host( buf, &type, &access, &data );
						if ( ret == OK ) {
							if ( strncmp( data, deviceProductClass, PRODUCT_CLASS_LENGTH ) == 0 ) {
								// Delete the Object
								sprintf( buf, "%s%d.", getManageableDevicePath(), i );
								ret = deleteObject( buf, &status );
							}
						} else 
							return ret;
					}
				} else 
					return ret;
			}
		} else 
			return ret;
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_HOST, "ManageableDevice: ret %d instance %d\n", ret, instance);
	)
	return ret;
}
#endif

static int readLine( char *buf, int len )
{
	int i = len;
	int c = 0;
	
	bzero(buf, len);
	while( --i > 0 ) {
		c = soap_getchar(soap);
		if (c == '\r' || c == '\n')
        	break;
      	if ((int)c == EOF)
        	return SOAP_EOF;
      	*buf++ = (char)c;
    }
	if (c != '\n')
      c = soap_getchar(soap); /* got \r, now get \n */
    if (c == '\n') 
    	*buf = '\0';
    else if ((int)c == EOF)
      return SOAP_EOF;
    
    return OK;
//	return soap_getline (soap, buf, len);
}

int sendEOL()
{
	return sendChar( "\n");
}

/** EndOfMessage is an empty line 
 */
int sendEOM()
{
	return sendEOL();
}

int sendInt( int value )
{
	return sendChar (soap_int2s (soap, value));
}

int sendUInt( unsigned int value )
{
	return sendChar (soap_unsignedInt2s (soap, value));
}

int sendChar( const char *buf )
{
	return soap_send( soap, buf );
}

#ifdef TR_111_DEVICE
static const char *getGwManufacturerOUIPath( void )
{
	return "Device.GatewayInfo.ManufacturerOUI";
}
#endif

#ifdef TR_111_DEVICE
static const char *getGwSerialNumberPath( void )
{
	return "Device.GatewayInfo.SerialNumber";
}
#endif

#ifdef TR_111_DEVICE
static const char *getGwProductClassPath( void )
{
	return "Device.GatewayInfo.ProductClass";
}
#endif

#ifdef TR_111_DEVICE
static const char *getDvManufacturerOUIPath( void )
{
	return "Device.Info.ManufacturerOUI";
}
#endif
#ifdef TR_111_GATEWAY
static const char *getDvManufacturerOUIPath( void )
{
	return "InternetGatewayDevice.DeviceInfo.ManufacturerOUI";
}
#endif

#ifdef TR_111_DEVICE
static const char *getDvSerialNumberPath( void )
{
	return "Device.Info.SerialNumber";
}
#endif

#ifdef TR_111_GATEWAY
static const char *getDvSerialNumberPath( void )
{
	return "InternetGatewayDevice.DeviceInfo.SerialNumber";
}
#endif

#ifdef TR_111_DEVICE
static const char *getDvProductClassPath( void )
{
	return "Device.Info.ProductClass";
}
#endif

#ifdef TR_111_GATEWAY
static const char *getDvProductClassPath( void )
{
	return "InternetGatewayDevice.DeviceInfo.ProductClass";
}
#endif

#ifdef TR_111_GATEWAY
static const char *getManageableDevicePath( void )
{
	return "InternetGatewayDevice.ManagementServer.ManageableDevice.";
}
#endif

#endif /* HAVE_HOST */
