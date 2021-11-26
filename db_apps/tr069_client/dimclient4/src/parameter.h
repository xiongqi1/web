/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef		parameter_H
#define		parameter_H

#include "utils.h"

#define INFO_PARENT_NOT_FOUND (1)

/* Definition of some important Parameter names */
#if defined(TR_111_DEVICE)
//
//	We don't provide Gateway functionality. This device is just a
//	plain device connected to a network. Treat it as such by
//	naming parameters Device and not InternetGatewayDevice
//
#define 	PARAMETER_KEY  					     			"Device.ManagementServer.ParameterKey"
#define 	DEVICE_INFO				    		 			"Device.DeviceInfo."
#define		DEVICE_INFO_MANUFACTURER			 			"Device.DeviceInfo.Manufacturer"
#define		DEVICE_INFO_MANUFACTURER_OUI 		 			"Device.DeviceInfo.ManufacturerOUI"
#define 	DEVICE_INFO_PRODUCT_CLASS		     			"Device.DeviceInfo.ProductClass"
#define		DEVICE_INFO_SERIAL_NUMBER		     			"Device.DeviceInfo.SerialNumber"
#define 	PERIODIC_INFORM_ENABLE			     			"Device.ManagementServer.PeriodicInformEnable"
#define		PERIODIC_INFORM_INTERVAL		     			"Device.ManagementServer.PeriodicInformInterval"
#define		PERIODIC_INFORM_TIME			     			"Device.ManagementServer.PeriodicInformTime"
#define		MANAGEMENT_SERVER_URL			     			"Device.ManagementServer.URL"
#define 	MANAGEMENT_SERVER_USERNAME		     			"Device.ManagementServer.Username"
#define		MANAGEMENT_SERVER_PASSWORD		     			"Device.ManagementServer.Password"
#define 	CONNECTION_REQUEST_URL			     			"Device.ManagementServer.ConnectionRequestURL"
#define		CONNECTION_REQUEST_USERNAME		     			"Device.ManagementServer.ConnectionRequestUsername"
#define		CONNECTION_REQUEST_PASSWORD		     			"Device.ManagementServer.ConnectionRequestPassword"
#define 	UDPCONNECTIONREQUESTADDRESS                  	"Device.ManagementServer.UDPConnectionRequestAddress"
#define 	NATDETECTED                                  	"Device.ManagementServer.NATDetected"
#define 	UDPCONNECTIONREQUESTADDRESSNOTIFICATIONLIMIT 	"Device.ManagementServer.UDPConnectionRequestAddressNotificationLimit"
#define 	STUNENABLE                                   	"Device.ManagementServer.STUNEnable"
#define 	STUNSERVERADDRESS                            	"Device.ManagementServer.STUNServerAddress"
#define 	STUNSERVERPORT                               	"Device.ManagementServer.STUNServerPort"
#define 	STUNUSERNAME                                 	"Device.ManagementServer.STUNUsername"
#define 	STUNPASSWORD                                 	"Device.ManagementServer.STUNPassword"
#define 	STUNMAXIMUMKEEPALIVEPERIOD                   	"Device.ManagementServer.STUNMaximumKeepAlivePeriod"
#define 	STUNMINIMUMKEEPALIVEPERIOD                   	"Device.ManagementServer.STUNMinimumKeepAlivePeriod"
#define		DEFAULT_GATEWAY									"Device.LAN.DefaultGateway"
#define		KICKURL											"Device.ManagementServer.KickURL"
#define		TRANSFERURL										"Device.Dimark.TransferURL"
#else
//
// We are actually providing a Gateway, so we need to
// name our parameters InternetGatewayDevice
//
#define 	PARAMETER_KEY  			  		      	 		"InternetGatewayDevice.ManagementServer.ParameterKey"
#define 	DEVICE_INFO			            		 		"InternetGatewayDevice.DeviceInfo."
#define		DEVICE_INFO_MANUFACTURER	             		"InternetGatewayDevice.DeviceInfo.Manufacturer"
#define		DEVICE_INFO_MANUFACTURER_OUI 	         		"InternetGatewayDevice.DeviceInfo.ManufacturerOUI"
#define 	DEVICE_INFO_PRODUCT_CLASS	             		"InternetGatewayDevice.DeviceInfo.ProductClass"
#define		DEVICE_INFO_SERIAL_NUMBER	             		"InternetGatewayDevice.DeviceInfo.SerialNumber"
#define 	PERIODIC_INFORM_ENABLE		             		"InternetGatewayDevice.ManagementServer.PeriodicInformEnable"
#define		PERIODIC_INFORM_INTERVAL	             		"InternetGatewayDevice.ManagementServer.PeriodicInformInterval"
#define		PERIODIC_INFORM_TIME		             		"InternetGatewayDevice.ManagementServer.PeriodicInformTime"
#define		MANAGEMENT_SERVER_URL		             		"InternetGatewayDevice.ManagementServer.URL"
#define 	MANAGEMENT_SERVER_USERNAME	             		"InternetGatewayDevice.ManagementServer.Username"
#define		MANAGEMENT_SERVER_PASSWORD	 	   		 		"InternetGatewayDevice.ManagementServer.Password"
#define 	CONNECTION_REQUEST_URL			  				"InternetGatewayDevice.ManagementServer.ConnectionRequestURL"
#define		CONNECTION_REQUEST_USERNAME		     			"InternetGatewayDevice.ManagementServer.ConnectionRequestUsername"
#define		CONNECTION_REQUEST_PASSWORD		     			"InternetGatewayDevice.ManagementServer.ConnectionRequestPassword"
#define 	UDPCONNECTIONREQUESTADDRESS                  	"InternetGatewayDevice.ManagementServer.UDPConnectionRequestAddress"
#define 	NATDETECTED                                  	"InternetGatewayDevice.ManagementServer.NATDetected"
#define 	UDPCONNECTIONREQUESTADDRESSNOTIFICATIONLIMIT 	"InternetGatewayDevice.ManagementServer.UDPConnectionRequestAddressNotificationLimit"
#define 	STUNENABLE                                   	"InternetGatewayDevice.ManagementServer.STUNEnable"
#define 	STUNSERVERADDRESS                            	"InternetGatewayDevice.ManagementServer.STUNServerAddress"
#define 	STUNSERVERPORT                               	"InternetGatewayDevice.ManagementServer.STUNServerPort"
#define 	STUNUSERNAME                                 	"InternetGatewayDevice.ManagementServer.STUNUsername"
#define 	STUNPASSWORD                                 	"InternetGatewayDevice.ManagementServer.STUNPassword"
#define 	STUNMAXIMUMKEEPALIVEPERIOD                   	"InternetGatewayDevice.ManagementServer.STUNMaximumKeepAlivePeriod"
#define 	STUNMINIMUMKEEPALIVEPERIOD                   	"InternetGatewayDevice.ManagementServer.STUNMinimumKeepAlivePeriod"
#define		KICKURL											"InternetGatewayDevice.ManagementServer.KickURL"
#define		TRANSFERURL										"InternetGatewayDevice.Dimark.TransferURL"
#endif

/* Kind	of Parameter
*/
typedef	enum parameterType { DefaultType		= 97,
							 ObjectType			= 98,											/* old */ /*new+1*/
							 MultiObjectType	= 99,
							 StringType			= SOAP_TYPE_xsd__string,						/*  6  */ /*  7  */
							 DefStringType		= (DefaultType + SOAP_TYPE_xsd__string),		/* 103 */ /* 104 */
							 IntegerType		= SOAP_TYPE_xsd__int,							/*  7  */ /*  8  */
							 DefIntegerType		= (DefaultType + SOAP_TYPE_xsd__int),			/* 104 */ /* 105 */
							 UnsignedIntType	= SOAP_TYPE_xsd__unsignedInt,					/*  9  */ /*  10 */
							 DefUnsignedIntType	= (DefaultType + SOAP_TYPE_xsd__unsignedInt),	/* 106 */ /* 107 */
							 BooleanType		= SOAP_TYPE_xsd__boolean,						/*  18 */ /*  19 */
							 DefBooleanType		= (DefaultType + SOAP_TYPE_xsd__boolean),		/* 115 */ /* 116 */
							 DateTimeType		= SOAP_TYPE_xsd__dateTime,						/*  11 */ /*  12 */
							 DefDateTimeType	= (DefaultType + SOAP_TYPE_xsd__dateTime),		/* 108 */ /* 109 */
							 Base64Type			= SOAP_TYPE_xsd__base64Binary,					/*  12 */ /*  13 */
							 DefBase64Type		= (DefaultType + SOAP_TYPE_xsd__base64Binary)	/* 109 */ /* 110 */
						} ParameterType;

/* Available access types
*/
typedef	enum accessType	{ ReadOnly = 0,
						  ReadWrite = 1,
						  WriteOnly = 2 } AccessType;

/* Available Reboot types
 If Parameter is set to Reboot a reboot has to initiated after all parameters are set during a SetParameterValues() call
*/
typedef	enum rebootType	{ Reboot, NoReboot } RebootType;

/* Available notification types as bit coded values
*/
typedef	enum notificationType {	NotificationNone = 0 ,
								NotificationPassive = 1,
								NotificationActive = 2,
								NotificationAllways = 4 } NotificationType;

/* Available notification change types
 */
typedef	enum notificationMax {	NotificationMaxNone = 0 ,
								NotificationMaxPassive = 1,
								NotificationMaxActive = 2,
								NotificationMaxAllways = 3,
								NotificationNotChangeable = 4 } NotificationMax;

/* Available status
*/
typedef	enum statusType	{ ValueNotChanged =	0,
						  ValueModifiedIntern = 1,
						  ValueModifiedExtern = 2 } StatusType;

/* Available persistence types
*/
typedef	enum persitenceType	{ ValuePersistent, ValueTransient }	PersitenceType;

/**	Returns	a array	of parameters in parameters	*/
int	getParameters( const struct ArrayOfString *, struct ArrayOfParameterValueStruct * );
int	setParameters( struct ArrayOfParameterValueStruct *, int * );

int	setParameter( const	char *,	void * );
int	getParameter( const	char *,	void * );

/**	Returns	a array	of parameters in parameters	*/
int	getParameterNames( const xsd__string, xsd__boolean, struct ArrayOfParameterInfoStruct *);

/**	Returns	a array	of parameters in parameters	*/
int	getInformParameters( struct	ArrayOfParameterValueStruct *);
void resetParameterStatus( void );

int	setParametersAttributes( struct	ArrayOfSetParameterAttributesStruct * );
int	getParametersAttributes( const struct ArrayOfString *, struct ArrayOfParameterAttributeStruct * );
int getParameter2Host( const char *, ParameterType *, AccessType *, void * );
int setParameter2Host( const char *, bool *, char * );
int addObjectIntern( const char *, int *, int );
int	addObject( const char * , struct cwmp__AddObjectResponse * );
int	deleteObject( const	char *,	int	* );
int countInstances( const char *, int * );

/**	Returns	the	number of parameters in	the	parameter list */
int	getParameterCount( void );
void printModifiedParameterList( void );

/** Load the Parameters from the init file and the written parameter files */
int	loadParameters( bool );
/** Delete all Parameters from disk, used by factory reset function */
int resetAllParameters( void );
/* Check Passive Notification */
void checkPassiveNotification();
/* Check Active Notification */
void checkActiveNotification();

// San. 31 may 2011. getNumberOfEntries
int getNumberOfEntries(char * , const char *);

#endif /* parameter_H */
