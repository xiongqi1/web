###########################################################################
#   Copyright (C) 2004-2010 by Dimark Software Inc.                       #
#   support@dimark.com                                                    #
###########################################################################

The files in this directory implement TR-111 support for the Dimark 
TR-069 Client.

1. STATUS OF IMPLEMENTATION

TR-111 is divided into two parts:

a. PART 1 - DHCP Client / Server

   Implemented

b. PART 2 - STUN (RFC3489) 

i. Binding Requests over UDP

   All TR-111 Part 2 functionality has been implemented except as noted
below for Server and Client.

ii. Shared Secret Requests

   Server: Optional and not implemented
   Client: Not implemented.

iii. DNS SRV Record Support

   Client: not implemented

   NOTE: STUN server connection information is provided via TR-069 / TR-111 
parameters as set by default in the client or as specified by the ACS.

iv. MESSAGE-INTEGRITY

   Server: optional, not implemented.
   Client: not implemented.

v.  Discovery of NAT binding timeout

   Server and Client: not implemented.

vi. CONNECTION-REQUEST-BINDING

   Server and Client: not implemented.

vii. UDPConnectionRequestAddressNotificationLimit

   Client: not implemented.  Parameter accepted, but value current
   not used.

viii. STUNMaximumKeepAlivePeriod

   Client: not implemented.  Parameter accepted, but value current
   not used.

iX. Server secondary source IP address

   Server: optional, not implemented.

X. TR-111 Section 2.2.2.2.1 STUN-based Approach

   Server: not implemented.  System uses Section 2.2.2.2.2 
   Notification-based Approach only.

   NOTE on last bullet in Section 2.2.2.2.2, server always attempts 
   TCP-based connection requests first.

XI. Under 2.2.1.4 UDP Connection Requests

   The STUN implementation has two phases if Stun is enabled with
   the ManagementServer.STUNEnable parameter: the client exchanges
   STUN binding information with the STUN server, and then listens
   for incoming UDP Connection Requests.

   If STUN is not enabled with this parameter, the client only
   listens for incoming UDP Connection Requests.

   Overview of Details:

   1. Exchange binding information with STUN server

   STUN Client/Server protocol as per RFC 3489. One iteration
   with timeout, sufficient to client to obtain mapped
   address information.  NOTE: the STUN procedure does not
   recognize an incoming TR-111 UDP Connection Request.

   2. Listen for incoming UDP Connection requests

   Client enters a loop with a UDP recvfrom() call with 10 
   second timeouts.  Loop continues until the minimum time
   interval in STUNMinimumKeepAlivePeriod is met. Then
   client tests to see if STUN is enabled and repeats.

2. METHOD OF OPERATION

a. PART 1 - DHCP Client / Server

   Implemented

b. PART 2 - STUN (RFC3489)

   The STUN client is available in either a TR-098 (Internet Gateway) or 
TR-106 (Device) configuration. The data model parameter prefix "<pre>" 
for TR-098 is:

    "InternetGatewayDevice"

and for TR-106:

    "Device"

The selection of either is set at compile time.

STUN parameters sent by the ACS are:

<pre>.ManagementServer.
   "    STUNEnable
   "    STUNServerAddress
   "    STUNServerPort
   "    STUNUsername
   "    STUNPassword
   "    STUNMaximumKeepAlivePeriod
   "    STUNMinimumKeepAlivePeriod
   "    UDPConnectionRequestAddressNotificationLimit

STUN parameters sent by the client using Inform:

<pre>.ManagementServer.
   "    NATDetected
   "    UDPConnectionRequestAddress

See Table 6 (page 26) of TR-111 for a full description of these parameters.

   See the format of the actual parameters in the  data-model.xml 
file for use by the Dimark client.

   NOTE: The following parameters are currently ignored by this version of 
the STUN client:

   STUNMaximumKeepAlivePeriod
   UDPConnectionRequestAddressNotificationLimit

NOTE: The internal default setting is 100 seconds for 
   
   STUNMinimumKeepAlivePeriod

   Setting the value of this parameter greater than 0 will override the 
default with the specified value.


METHOD OF OPERATION - SERVER

   Simply, Dimark provides a stand-alone STUN server that runs in parallel 
to the ACS on the same server. The STUN server accepts BINDING Requests 
and issues BINDING Responses as per RFC3478. 

   In addition, the STUN server has been augmented to act as a relay for 
ACS UDP Connection Requests.

   There are two types of Connection Requests that may be issued by the 
ACS: TCP and UDP.  The ACS will always attempt a TCP connection request 
first. If a non-response is experienced, the ACS will see if 
<pre>.UDPConnectionRequestAddress exists for the device and that it has 
a non-null value.  If non-null, the ACS will construct a UDP Connection 
Request and pass it to the STUN server on "localhost" using UDP port 16000. 
The STUN server will then relay the UDP packet to the client device.  As 
per TR-111, the STUN server will send multiple copies of the UDP connection
request.  Default is 2 copies.

   NOTE: in this method, there is no communication from the STUN server to 
the ACS.  All UDP address information is obtained from the client.  This 
is the TR-111 Section 2.2.2.2.2 Notification-based Approach.


METHOD OF OPERATION - CLIENT

   The TR-111 STUN client support replaces previous UDP connection request 
support in the client. The compiler directive "WITH_UDP" has been deprecated 
and now "WITH_STUN_CLIENT" is used to include compile time support.

On startup, the Dimark client creates a process thread for "stunHandler".  

NOTE: This procedure is in the main dimclient.c file, otherwise all 
TR-111 support is under the "tr111/" directory.

The stunHander() process is responsible for:

1. Examining STUNEnable, as set by the ACS. 

If STUN Enabled:
2. Calling the stun_client procedure for exchanging BINDING information 
with the STUN server.
3. Examining results and updating Inform parameters for notification
if the information has recently changed.

If STUN Disabled:
4. Updating Inform parameters with local connection information

5. Calling the UDP Connection Request handling process for a period 
of time.  If a validated UDP connection request is received, scheuling 
a signaled Inform.

If either Enabled or Disabled, then
6. Sleeping, and returning to 1.

#end
