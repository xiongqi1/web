####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=xml-rpc-currentTime xml-rpc-weblogs
SOAPHEADER=xml-rpc.h

include ../Makefile.defines
SOAP_NS=$(SOAP_NS_DEF)
include ../Makefile.c_rules

xml_rpc_currentTime_SOURCES = xml-rpc-currentTime.c $(SOAPHEADER) soapC.c
xml_rpc_weblogs_SOURCES = xml-rpc-weblogs.c $(SOAPHEADER) soapC.c

