####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=googleapi
SOAPHEADER=googleapi.h

include ../Makefile.defines
SOAP_NS=googleapi.nsmap googleapi.wsdl
include ../Makefile.c_rules

googleapi_SOURCES=googleapi.c $(SOAPHEADER) $(SOAP_C_CLIENT)

