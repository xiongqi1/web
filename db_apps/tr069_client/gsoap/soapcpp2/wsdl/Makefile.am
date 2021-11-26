####### This is the input file for automake, which will generate Makefile.in ##########


## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

INCLUDES=-I$(top_builddir)/soapcpp2
LDFLAGS=$(INCLUDES)
SOAP=$(top_builddir)/soapcpp2/src/soapcpp2
SOAP_CPP_LIB=$(top_builddir)/soapcpp2/libgsoap++.a
SOAP_CPP_SRC=wsdlC.cpp
SOAPHEADER=wsdl.h
SOAP_FLAGS=-SC -pwsdl
BUILT_SOURCES=$(SOAP_CPP_SRC)

$(SOAP_CPP_SRC) : $(SOAPHEADER)
	$(SOAP) $(SOAP_FLAGS) $(SOAPHEADER)

#LIBS=

bin_PROGRAMS=wsdl2h

wsdl2h_CFLAGS=$(C_DEBUG_FLAGS) $(SOAPCPP2_NONAMESPACES) -D$(platform)
wsdl2h_CPPFLAGS=$(C_DEBUG_FLAGS) $(SOAPCPP2_NONAMESPACES) -D$(platform)
wsdl2h_SOURCES=wsdl2h.cpp wsdl.cpp schema.cpp types.cpp service.cpp soap.cpp mime.cpp
wsdl2h_LDADD=$(SOAP_CPP_SRC) $(SOAP_CPP_LIB) $(SAMPLE_EXTRA_LIBS)

CLEANFILES= *~ *C.cpp *Client.cpp *ClientLib.cpp *H.h *Server.cpp *ServerLib.cpp *Stub.h *.xsd
