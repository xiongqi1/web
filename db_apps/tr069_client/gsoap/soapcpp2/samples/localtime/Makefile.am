####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=localtime
SOAPHEADER=localtime.h

include ../Makefile.defines
SOAP_NS=localtime.nsmap localtime.wsdl
include ../Makefile.c_rules

localtime_SOURCES = localtime.c $(SOAPHEADER) $(SOAP_C_CLIENT)

