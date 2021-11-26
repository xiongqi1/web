###########################################################################
#   Copyright (C) 2004-2010 by Dimark Software Inc.                       #
#   support@dimark.com                                                    #
###########################################################################

How Parameterhandling works:

Parameters are one of the key elements in the TR-069

Therefor is a lot of work done in the ParameterHandling part.

1. Storage of parameter in memory

The parameters have a hierarchical structure. In the client the parameters are also stored in an tree where the parents are
the nodes and the children are the leaves.

Ex.: InternetGatewayDevice.DeviceInfo.Manufacturer
InternetGatewayDevice. is the parent of the entry DeviceInfo
DeviceInfo. is the parent of Manufacturer

Every parameter is stored in an object of the structure ParameterEntry ( see. parameter.c )
which has 4 pointers to handle the tree.

    child	is a pointer to the first child of this entry
    next	is a pointer to the next sister of this entry ( NULL means there is none )
    prev	is a pointer to the previous server of this entry ( NULL means there is none )
    parent	is a pointer to the parent ( node ) of this entry 

With this pointers has every entry access to all information it needs.
There is one rootEntry which is for management reason. It owns the children of the highest parameter level.

Example with  !! NULL pointers are left off for easier reading !!

	InternetGatewayDevice.DeviceInfo.Manufacturer
	InternetGatewayDevice.DeviceInfo.ModelName
    
    rootEntry
	.parent = NULL
	.next   = NULL
	.prev   = NULL
	.child	-> InternetGatewayDevice.
			    .parent ->  rootEntry
			    .child  ->	DeviceInfo.
					    .parent -> InternetGatewayDevice.
					    .child  -> Manufacturer
							    .parent -> DeviceInfo.
							    .next -> ModelName
									.parent -> DeviceInfo.
									.prev   -> Manufacturer

Special handling is made for the MultiObjectTypes, which are nodes that can have more then one instance.
The instances are numbered from 1 to n. The instance is handled a node. The numbering is made in the 
parent node of the instance.

Parameter value or default value

Action						read only	readWrite	value	default value
-------------------------------------------------------------------
setParameterValue				-			x				
getParameterValue
writeToFile
readFromFile
AddObject						x			x					x


2. Parameter preset

To build the tree and preset the parameter with values, there is a initialization file.
The file pathname is defined in parameter.c as DEFAULT_PARAMETER_FILE.
This file should be in a read only file system ex. ./etc/config/data-model.xml

3. Parameter persistence

Some parameters are defined to be writable through the server or a LAN interface.
These parameters are stored in files in a flash file system.
The path to the directory is defined in parameter.c as PERSISTENT_PARAMETER_DIR
For every parameter is a single file with the parameter path as filename.
The internal structure is the same as the initial setup file.
The directory must exist. It can also be in a ram file system, but than the parameters are reseted at reboot.
The files are read after the initial setup file is read.

4. Parameter access to host system

To inform the host system about a changed parameter or to get a parameter value from the host system, there are access functions
defined. In the parameter structure are 3 indexes into arrays of access functions. These arrays are defined in paramaccess.c.
These indexes are 

    int initDataIdx	index to the init and delete function
    int getDataIdx	index to the get function,
    int setDataIdx;	index to the set function

Two special values are defined for the index:
    -1	function not allowed
    0	get or set value from the parameter, no call to the host system

The index must be a unique integer.
The access to the host system is done be two convenient functions.

    int getAccess( int idx, const char *name, ParameterType type, ParameterValue *value );
    int setAccess( int idx, const char *name, ParameterType type, ParameterValue *value );

The function prototype is defined as 

	typedef int (*accessParam)(const char* , ParameterType, ParameterValue *);

Example:
    
    Parameter: InternetGatewayDevice.WANDevice.1.WANCommonInterfaceConfig.TotalBytesSent
		.initDataIdx = -1    
		.getDataIdx = 817
		.setDataIdx = 817

    There are two access functions defined by the host system, which are set in the arrays

    Func getArray[] = {
	{ 817 , &getETH0SentBytes }

    };

    Func setArray[] = {
	{ 817 , &setETH0SentBytes }

    };


5. Callback Feature

To execute a function after setting a value, the access function are used. But if you want to execute a function after
the communication is done, the callback has to be used.
The callback feature gives you the possibility to register a callback function which is called after the communication 
with the server has finished. 
The callback functions are stored in a list are executed as a FIFO ( first in first out ).
The callback function is only called once, and there is no persistence of the callback list.

There is a function to register a callback 
    
    int registerCallback( Callback cb )
 
The Callback prototype
    
    typedef int (*Callback)(struct soap* );

    Soap is the access to the soap functions. Maybe useful.

For example see:

    diagParameter.c and diagParameter.h


6. Using HTTP BasicAuthentication

Username and password are taken from Parameter:

	InternetGatewayDevice.ManagementServer.Username
	InternetGatewayDevice.ManagementServer.Password
	
These parameter can be changed by the server.
The username is the OUI which are the 24 bits of the MACAddress a hyphen and the serial number.
The password is the the serial number, a hyphen and the OUI.



Appendix A:

This to do for Parameter Access,

1. Implement the Access to the Operating System

2. Declare the Functions in an Header file

3. Include the header file in paramaccess.c

4. Add the getFunctions to the getArray[]
    ( Caution!! use unique numbers )

5. Add the setFunctions to the setArray[]

6. Create an entry in the Default parameter file. The filename is defined in parameter.c
   The file can be in a RO file system.

7. After the change of writable parameter is a file written with the value of the 
   parameter. This filename is equal the parameter name. The directory name is defined in parameter.c
   This directory must live in an RW file system, which is persistent.
   These parameter files are read after the load of the default parameter file, and overwrites the 
   default value of the parameter.


END OF SECTION



Default Parameter file structure:

notification = notify the server if the value of this parameter is changed through the LAN interface
		0 = NotificationNone 
		1 = NotificationPassive 	( included in the next regular inform schedule )
		2 = NotificationActive		( changes of this parameter initiate an immediate inform schedule )
		4 = NotificationAllways 	( always included in the inform schedule )
		
notificationMax = maximum allowed notification type 
		0 = NotificationMaxNone 
		1 = NotificationMaxPassive 	( included in the next regular imform schedule )
		2 = NotificationMaxActive	( changes of this parameter initiate an immediate inform schedule )
		3 = NotificationMaxAllways 	( always included in the inform schedule )
		4 = NotificationNotChangeable ( changes are not allowed )
		
reboot = Reboot the system if the value of this parameter has changed
		0 = Reboot
		1 = NoReboot

initIdx = -1 , not used yet, value is taken from default value
getIdx = index in the getArray[] or 
	-1 if no support for the get() means WriteOnly
	0 get the Value from the memory
	1 get the Value from the persistent storage
	n get the Value from a customer specific implementation

setIdx = index into the setArray[] or 
	-1 if no support for the set() means ReadOnly
	0 store the Value in memory, lost after a reboot
	1 store the Value in the persistent storage
	n store the Value in a customer specific implementation

!!setIdx and getIdx should always have the same value, you could not mix 0 and 1 values for one param!!
	
access list = collection of the access listen tries, delimited by '|'

default value = setup value, must be casted to void *


--------
 Debug

For environments where there is no debugging functionality available there is the option 
to enable output of the client to help diagnose issues. 
Those are defined in debug.h and implemented in debug.c

To minimize performance penalties during debugging output the debugging output can be removed 
completely from the program code. This is managed through the WITHOUT_DEBUG=TRUE in Makefile.

Each line in log.config has the next format:

subsystem;level

[SOAP|MAIN|PARAMETER|TRANSFER|STUN|ACCESS|MEMORY|EVENTCODE|SCHEDULE|ACS|VOUCHERS|OPTIONS|
DIAGNOSTIC|VOIP|KICK|CALLBACK|HOST|REQUEST|DEBUG|AUTH];[DEBUG|INFO|WARN|ERROR]

For example:

SOAP;DEBUG
MAIN;INFO
PARAMETER;WARN
TRANSFER;ERROR
STUN;DEBUG
ACCESS;DEBUG
MEMORY;DEBUG
EVENTCODE;DEBUG
SCHEDULE;DEBUG
ACS;DEBUG
VOUCHERS;DEBUG
OPTIONS;DEBUG
DIAGNOSTIC;DEBUG
VOIP;DEBUG
KICK;DEBUG
CALLBACK;DEBUG
HOST;DEBUG
REQUEST;DEBUG
DEBUG;DEBUG
AUTH;DEBUG


--------
Functionality: Download

Description:
	Downloads one file per ACS request from a known server.
	The file is specified by an URL.
	ACS can specify an delay of the download function. 
	A value > 0 means the download should not be done inside the actual session.
	After a successful download, a callback function is called.
	The callback function is registered in dimclient.c and must be set at the startup of dimclient.
	If the value of the callbackfunction is NULL, no callback is initiated.
	See ftcallback.c for an example.
	
	The Function declaration of the Callback function:
		int DownloadCB( char *targetFilename, char *fileType )
	
	targetFilename is the pathname of the loaded file.
	fileType is the type, defined in the TR-069 spec.
	
	Return code should be one of codes defined in globals.h
	
Files:
	filetransfer.h
	filetransfer.c
Functions:
	doDownload
	handleDelayedFiletransfers
	clearDelayedFiletransfers
	
Misc:

Example of a callback function implementation.

	static int downloadCallback( char *filename, char *filetype )
	{
		dbg( DBG_ALL, "** Download Callback Dummy **\nFile: %s  Type: %s \n", filename, filetype );
		return OK;
	}


----
Functionality: Client Notification

Description:
To read and write the client parameters by the host system ( not the ACS ), there is a 
thread spawned in the dimclient. 
The thread is listening at port 8081 for a command to get or to set a parameter.

GET:
	send	get
	send	<parameterpath>
	receive	<type>			( see parameter.h )
	receive <value>			all values are returned as string
					in case of an error type and value has same value
	receive <emptyline>		

SET:
	send	set
	send	<parameterpath>
	send	<parametervalue>
	receive	<returncode>
	receive <emptyline>		

ADDOBJECT:
	send	add
	send	<objectpath>		
	receive <instanceno>		instance number of new object
					in case of an error a 0 is returned
	receive <emptyline>		

DELOBJECT:
	send	del
	send	<objectpath>		
	receive <errorcode>		0 = noError
	receive <emptyline>		

CLEAROPTION:
	send	opt
	send	<voucherSN>
	send	<remove>
	receive <errorcode>		0 = noError
	receive <emptyline>

GETVENDORINFO
	send	vg
	receive <errorcode>		0 = noError
	receive	<manufactorerOUI>
	receive	<serialNumber>
	receive	<productClass>
	receive <emptyline>
		
SETVENDORINFO
	send	vs
	send	<manufactorerOUI>
	send	<serialNumber>
	send	<productClass>
	receive	<errorcode>		0 = noError
	receive	<emptyline>
	
ADDDEVICEINFO	
	send	da
	send	<manufactorerOUI>
	send	<serialNumber>
	send	<productClass>
	receive	<errorcode>		0 = noError
	receive	<emptyline>

REMOVEDEVICEINFO
	send	dr
	send	<manufactorerOUI>
	send	<serialNumber>
	send	<productClass>
	receive	<errorcode>		0 = noError
	receive	<emptyline>

DHCPDISCOVERY
	send	hd
	send	<manufactorerOUI>
	send	<serialNumber>
	send	<productClass>
	receive	ho
	receive	<manufactorerOUI>
	receive	<serialNumber>
	receive	<productClass>
	receive	<URL>
	receive <emptyline>

DHCPREQUEST
	send	gr
	send	<manufactorerOUI>
	send	<serialNumber>
	send	<productClass>
	receive	ho
	receive	<manufactorerOUI>
	receive	<serialNumber>
	receive	<productClass>
	
For testing purposes the access is not delimited to the localhost.


----
Functionality: Server Notification

Description:
The ACS can notify the CPE to initiate a inform message.
The Communication is started by a HTTP Request from the ACS to port 8082 of the CPE.
The used authentication is Digest. The following Parameters are checked before a connection is allowed:

	InternetGatewayDevice.ManagementServer.ConnectionRequestURL  		must be identical with the HTTP Request
	InternetGatewayDevice.ManagementServer.ConnectionRequestUsername	must be the same used by the ACS
	InternetGatewayDevice.ManagementServer.ConnectionRequestPassword 	must be the same used by the ACS
	
The Connection timing is delimited to one connection every 60 seconds ( not implemented yet )
After the connection is approved the socket is closed and an inform Message is sent to ACS.
If the CPE is in an transaction with the ACE the call is ignored.

----
Communication keep-alive

Jboss Tomcat has maxKeepAliveRequests Variable in the deploys/jbossweb-tomcat50.sar/server.xml file.
This parameter must be set to a value > 100 default = 20

The parameter is found in <Connector .. maxKeepAliveRequests="1000" .. >
If the value is not set, the connection is closed after x Requests from the client
Don't forget to restart jboss

----
Digest Authentication

To use the Digest authentication, gSOAP Version 2.7 and the httpda.* plugin is necessary.
The first inform call is made with Basic authentication, if this fails with return Code 401
the Digest authentication is used. All following calls are made with the setup of this
authentication. The next inform call starts with Basic authentication again.

----
Voip Abstraction Layer

tbd.


----
TR-111

Device:
	Requirements:
	- Dimclient must be started before the DHCP client starts.
	- The mainloop in dimclient must wait until the hostsystem has finished the DHCP Session.
	
	Procedure:
	- Get the device vendor specific informations ( ManufacturerOUI, SerialNumber, ProductClass )
	from dimclient. Use HostInterface with a convenient access function.
		Device.Info.ManufacturerOUI
		Device.Info.SerialNumber
		Device.Info.ProductClass
		
	DHCP Client uses the information for getting the GatewayDevice informations
	
	Store the GatewayDevice vendor specific informations in 
		Device.GatewayInfo.ManufacturerOUI
		Device.GatewayInfo.SerialNumber
		Device.GatewayInfo.ProductClass

	the mainloop is unlocked, and the first inform message is sent to the ACS.
	
	The DHCP Client uses the host interface to set the parameters 
	if the lease is released or expires without renewal to empty values
	or to the new values.
	
	
Gateway:
	Requirements:
	- Dimclient must run before the first request can be handled by the DHCP server.
	- The mainloop must not wait for the hostsystem is finished
	
	The DHCP Server reads the data at the first clientrequest.
	
	The DHCP Server stores the client data via hostinterface in dimclient. The server must
	deliver all 3 values, even one of them is empty.
	
	If a new client dataset is coming from the hostinterface, the data is added or replaces the old one.
	The clientdataset is stored in
		InternetGatewayDevice.ManagementServer.ManageableDevice.{i}.ManufacturerOUI
		InternetGatewayDevice.ManagementServer.ManageableDevice.{i}.SerialNumber
		InternetGatewayDevice.ManagementServer.ManageableDevice.{i}.ProductClass
	The 
		InternetGatewayDevice.ManagementServer.ManageableDeviceNumberOfEntries 
	Parameter is updated every time a new ManageableDevice is created or deleted.
	Every time the ManageableDeviceNumberOfEntries changes and the last change is inside
	the ManageableDevicNotificationLimit time, there is an Informmessage to the ACS.

	If the Lease of a client is released the dataset is removed from the ManageableDevice table and 
	the ManageableDeviceNumberOfEntries is decreased by one.
	
	Dataflow:
	1. The DHCP Server sends the ClientData line by line ( OUI, SerialNumber, ProductClass )
	to the hostHandler with Cmd "da" 
	2. HostHandler buffers the ClientData
	3.

Stun Client Requirements from TR111

1. Determine Public IP address and port for UDP Connection Requests listener
primary source port 	UDPConnectionRequests are awaited
secondary source port	Used for binding timeout recovery


- Stun is enabled by STUNEnable Parameter.
	Erweiterung in parameter.c zum Lesen der Parameter
		STUNEnable, STUNServerAddress, STUNServerPort (uint), 
		STUNMaximumKeepAlivePeriod ( uint ),  STUNMinimumKeepAlivePeriod (uint)
		STUNUsername, STUNPassword, NATDetected ( = false wenn STUNEnable false )
		UDPConnectionRequestAddressNotificationLimit ( uint )
		UDPConnectionRequestAddress ( String )
	
- If no StunServerAddress use the ACS Serveraddress
- Binding requests send from my IP Address and Portno defined for UDPConnectionRequests
  ACS_UDP_NOTIFICATION_PORT 
- Addressmapping 
	Stun Attribute MAPPED-ADRESS: public IP and Port
- If STUNUsername and STUNPassword is given and Returncode is 401
	Use USERNAME and MESSAGE-INTEGRITY must be sent and received. Otherwise the Values are not valid.
	( Send only MESSAGE-INTEGRITY if Stunserver Response is 401 )
- No support for CHANGE-REQUEST, CHANGED-ADDRESS, SOURCE-ADDRESS, REFLECTED-FROM Attributes
  and no SharedSecret exchange.
- if local IP Address changes ( DHCP ) the binding procedure must be repeated.

	
2. Discover the NAT binding timeout and send STUN Bindings requests to keep alive binding
- 2 Parameters STUNMinimumKeepAlivePeriod and STUNMaximumKeepAlivePeriod control the
timeout for the Keepalive Binding requests. If the Values are not equal, discover the longest
keepalive period.

- 2 methods to tell the ACS about the binding address.
  first, add CONNECTION-REQUEST-BINDING Attribute into every StunRequest with my IP and Portno.
  if StunPassword is not empty send it in USERNAME Attribute.
  do not use it in the keepalive tests.
  
3. Handle the UDPConnectionRequestAddress Parameter if binding changes.

4. Listen for UDP Connection Requests messages

#end