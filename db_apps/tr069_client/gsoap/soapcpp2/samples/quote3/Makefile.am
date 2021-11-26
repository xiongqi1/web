####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=quote3
SOAPHEADER=quote3.h

include ../Makefile.defines
SOAP_NS=Quote.nsmap Quote.wsdl
include ../Makefile.cpp_rules

quote3_SOURCES = quote3.cpp $(SOAPHEADER) $(SOAP_CPP_CLIENT)

