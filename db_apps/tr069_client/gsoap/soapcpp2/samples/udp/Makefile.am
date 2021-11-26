####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=udpclient udpserver

AM_CFLAGS=-D$(platform) -DWITH_UDP
AM_CXXFLAGS=-D$(platform) -DWITH_UDP

INCLUDES=-I$(top_builddir)/soapcpp2
SOAP=$(top_builddir)/soapcpp2/src/soapcpp2
SOAP_FLAGS=-I$(top_builddir)/soapcpp2/import

SOAP_NS_DEF=ns.nsmap ns.wsdl

SOAP_H_FILES=soapH.h soapStub.h 

SOAP_C_CLIENT=soapClient.c soapC.c $(top_builddir)/soapcpp2/stdsoap2.c
SOAP_C_SERVER=soapServer.c soapC.c $(top_builddir)/soapcpp2/stdsoap2.c

SOAPHEADER=udp.h
SOAP_NS=udp.nsmap
include ../Makefile.c_rules

udpclient_SOURCES=udpclient.c $(SOAPHEADER) $(SOAP_C_CLIENT)
udpserver_SOURCES=udpserver.c $(SOAPHEADER) $(SOAP_C_SERVER)

