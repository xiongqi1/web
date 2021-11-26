####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=mtom
SOAPHEADER=mtom-test.h

include ../Makefile.defines
SOAP_NS=mtom_test.nsmap mtom_test.wsdl
include ../Makefile.c_rules

mtom_SOURCES=mtom-test.c $(SOAPHEADER) $(SOAP_C_SRC)
mtom_LDADD=$(SOAP_C_LIB) $(SAMPLE_EXTRA_LIBS)


