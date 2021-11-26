####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=factory
SOAPHEADER=factory.h

include ../Makefile.defines
SOAP_NS=factory.nsmap factory.wsdl
include ../Makefile.cpp_rules

factory_SOURCES = factory.cpp $(SOAPHEADER) $(SOAP_CPP_SERVER)

