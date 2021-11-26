####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=quote4
SOAPHEADER=quote4.h

include ../Makefile.defines
SOAP_NS=
include ../Makefile.cpp_rules

quote4_SOURCES = quote4.cpp ../../dom.cpp soapC.cpp $(SOAPHEADER)

