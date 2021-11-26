####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=luclient luserver.cgi
SOAPHEADER=lu.h

include ../Makefile.defines
SOAP_NS=lu.nsmap lu.wsdl
include ../Makefile.cpp_rules

luclient_CXXFLAGS=-DWITH_COOKIES -D$(platform)
luclient_SOURCES=luclient.cpp lumat.cpp $(SOAPHEADER) $(SOAP_CPP_CLIENT)
luclient_LDADD=$(SOAP_CPP_LIB_CK) $(SAMPLE_EXTRA_LIBS)

luserver_cgi_CXXFLAGS=-DWITH_COOKIES -D$(platform)
luserver_cgi_SOURCES=luserver.cpp lumat.cpp $(SOAPHEADER) $(SOAP_CPP_SERVER)
luserver_cgi_LDADD=$(SOAP_CPP_LIB_CK) $(SAMPLE_EXTRA_LIBS)


