####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

noinst_PROGRAMS=listing
SOAPHEADER=listing.h

include ../Makefile.defines
SOAP_NS=$(SOAP_NS_DEF)
include ../Makefile.cpp_rules

listing_SOURCES = listing.cpp $(SOAPHEADER) $(SOAP_CPP_CLIENT)

