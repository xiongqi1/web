####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=quotex
SOAPHEADER=quotex.h

include ../Makefile.defines
SOAP_NS=ns1.nsmap ns2.nsmap ns1.wsdl ns2.wsdl
include ../Makefile.cpp_rules

quotex_SOURCES = quotex.cpp $(SOAPHEADER) $(SOAP_CPP_CLIENT) soapServer.cpp
