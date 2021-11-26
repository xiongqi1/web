####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=magic magicserver.cgi mtmagicserver.cgi
SOAPHEADER=magic.h

include ../Makefile.defines
SOAP_NS=magic.nsmap magic.wsdl

include ../Makefile.cpp_rules

magic_SOURCES=magic.cpp $(SOAPHEADER) $(SOAP_CPP_CLIENT)
magicserver_cgi_SOURCES=magicserver.cpp $(SOAPHEADER) $(SOAP_CPP_SERVER)
mtmagicserver_cgi_SOURCES=mtmagicserver.cpp $(SOAPHEADER) $(SOAP_CPP_SERVER)
mtmagicserver_cgi_LDADD=$(SOAP_CPP_LIB) $(SAMPLE_EXTRA_LIBS) -lpthread
