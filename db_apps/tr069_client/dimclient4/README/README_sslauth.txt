###########################################################################
#   Copyright (C) 2004-2010 by Dimark Software Inc.                       #
#   support@dimark.com                                                    #
###########################################################################

DIMARK CLIENT SSL AUTHENTICATION NOTES - OpenSSL Version Only

The Dimark client can be compiled to enable the checking of SSL certificates.

NOTE: The TCP Connection Request (ACS -> client) mechanism does not use SSL.  
The contents of this document only refer to main (client -> ACS) communicates when 
that  connection is established using SSL (e.g. https://....).

The compilation flag is "WITH_SSLAUTH".  Default is to not include support.

1. WHEN NOT INCLUDED, the SSL support simply checks the server's SSL certificate 
to make sure that it is a valid certificate.  Valid certificate checks are very 
simple and straightforward:

a) Is it a valid SSL certificate and is it signed.  NOTE: that self-signed 
certificates are also accepted, do virtually anyone may put in place an ACS with 
a "valid" certificate, and

b) The date on the client is between the "Issued On" and "Expires On" dates of 
the certificate.

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NOTE: the most common reason for SSL connection failure is that the date/time 
on the client is not set correctly.  Use a battery backed real-time clock or 
an NTP (Network Time Protocol) client on the TR-069 client.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

2) WHEN INCLUDED, there are two main options that can be used to more thoroughly 
validate certificates:

  - Client validates Server's certificate
  - Server validates Client's certificate(s)

gSoap provides the soap_ssl_client_context() to setup up validation credentials 
for the normal main ACS conversations.  The arguments to the call are:

  soap_ssl_client_context(&soap, SOAP_SSL_DEFAULT, keyfile, password,
                           cafile, capath, randfile);

This call is coded (located) in the src/dimclient.c file.

RECOMMENDATION: Unless the customer/integrator has more detailed knowledge of
OpenSSL operation and coding (or other cryptographic requirements) the use of 
Method A) below using the "cafile" option only, is the preferred implementation
approach for the client to validate the server's SSL credentials.

A) CLIENT VALIDATES SERVER'S CERTIFICATE

With this method, the client has one or more "Root Certificate Authority",
also called "Root CA" certificates.  The idea is that the server's SSL
certificate is signed by a known authority, such as Verisign, Thawte, etc.
Each authority has their own Root Certificate used to sign server 
certificates.  These root certificates are made publicly available and
can be used by clients to verify that a server's certificate was indeed
signed officially by that authority. For information on the trusted
hierarchy created by this system, see:

  http://en.wikipedia.org/wiki/Root_certificate
  http://www.tech-faq.com/root-certificate.shtml

To actually download root certificates, there are several sources on
the web for this. One is at Verisign:

  https://www.verisign.com/support/roots.html

You will need to fill out information and then will obtain the latest root 
certificates from Verign, Thawte, and GeoTrust.

Also see the OpenSSL SSL_CTX_load_verify_locations() call page at: 
http://www.openssl.org/docs/ssl/SSL_CTX_load_verify_locations.html

In order for this method to be successful, the client MUST have a local copy 
of the Root CA for the server's certificate; e.g. if the server's certificate 
is signed by a Verisign Root CA certificate, the client must have that 
certificate available locally.  If the server's certificate is privately signed, 
then the client must have a copy of the private root ca used to sign the server's 
certificate.

NOTE: Dimark self-signs the SSL server certificate for test.dimark.com.
We provide a copy of the Dimark Root CA certificate for testing.  It is
available as "dimark.pem" in the ./test directory of this release.

This methods also means that if the TR-069 client is expected to verify one 
of many ACS's, then the client might need to have many of the common root 
server certificates pre-loaded, or it must be able to Have a root ca locally 
installed.

There are two ways to specify the location of root ca certificates using 
the above call.  First is to specify a single file "cafile" that contains 
one or more certs, or a "capath" that points to a directory that contains 
one or more certificates.

The "cafile" methods is the most common.  The "cafile" is a file name of 
a single file containing one or more PEM encoded root ca certificates.  
A PEM (privacy enhanced mail) format certificate is the most common and 
refers to a Base-64 encoded X.509 certifiate. 

See http://en.wikipedia.org/wiki/X.509 for more information.

NOTE: "cafile" could be coded as a string variable that is used to point 
to one of several files, each file containing one or more certificates.  
The coding of the call is up to the integrator of the Dimark client.

The "capath" method is provided by OpenSSL that allows a directory to be 
used to store a single certificate per file.  The details on setting up 
the directory are in the OpenSSL man page reference earlier.

Note that "cafile" and "capath" may both be used, OpenSSL searches
"cafile" first.

DIMARK supplies it's private root ca in file "dimark.pem". If the file 
was located in the same directory as the diclient binary, then a 
possible coding of the call would be:

 soap_ssl_client_context( &soap,
  		          SOAP_SSL_DEFAULT,
			  NULL,  // keyfile, SSL_CTX_use_certificate_chain_file
			  NULL,  // password, SSL_CTX_set_default_passwd_cb
			  "dimark.pem",  // cafile, SSL_CTX_load_verify_locations
			  NULL,  // capath, SSL_CTX_load_verify_locations
			  NULL   // randfile
	                )

NOTE: gSoap is coded to expect the following matching condition: the text
host name in the URL address (endpoint address) must match the name of server
in the certificate:  Therefore, the URL must be coded as:

   https://test.dimark.com:8443/dps/TR069
   https://test.dimark.com:8443/dps-basic/TR069
   https://test.dimark.com:8443/dps-digest/TR069

because "test.dimark.com" is encoded in the server SSL certificate.  The
IP address may not be substituted or else gSoap will reject the connection.

B) SERVER VALIDATES CLIENT'S CERTIFICATE(S)

This is a little used feature. It allows the client to have it's own set of 
certificates, much like the server SSL certificate. The client's certificates
are signed and all levels of certificates in the chain must be present as
required to permit the server to validate the client's certificate.  More
information can be found at:

  http://www.openssl.org/docs/ssl/SSL_CTX_use_certificate.html
  https://www.openssl.org/docs/ssl/SSL_CTX_set_default_passwd_cb.html

NOTE: this is a very complicated setup to administer as the certificates 
must be unique per device.  It is not used in practice.

C) RANDFILE

gSoap allows the SSL function RAND_load_file() to be called using the "randfile"
name to allow using a different random number seed startup process.  For more 
information, see: 

  https://www.openssl.org/docs/crypto/rand.html
  https://www.openssl.org/docs/crypto/RAND_load_file.html

gSoap essentially calls the function with the file name and "-1" as the max_bytes
argument value: e.g. RAND_load_file(randfile, -1);

In practice and the nature of TR-069, using "randfile" is not used.

#end
