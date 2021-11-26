#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../romfs
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -g -c -Wall -Wstrict-prototypes -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -Wstrict-prototypes -I$(CDCS_INCLUDE) -fPIE -fstack-protector-strong
	LDFLAGS := -pie
endif

.PHONY: all
all: pppoe-serial

pppoe-serial : pppoe-serial.o if.o debug.o common.o md5.o
	$(CC) $^ $(LDFLAGS) -o $@ -lpthread $(FILE_DBLIB)
	$(STRIP) pppoe-serial

# dependencies
md5.o: md5.c md5.h
pppoe-serial.o: pppoe-serial.c pppoe.h
if.o: if.c pppoe.h
common.o: common.c pppoe.h
debug.o: debug.c pppoe.h

.PHONY: clean distclean
clean distlean:
	find -type f -name "*.[ch]" | xargs chmod 644; chmod 644 Makefile
	rm -f *.o pppoe-serial

.PHONY: install
install: pppoe-serial
	cp pppoe-serial $(INSTALLDIR)/usr/sbin/
