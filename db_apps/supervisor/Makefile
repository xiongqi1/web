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
	CFLAGS := -g -c -pedantic -Wall -Wno-variadic-macros -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -pedantic -Os -Wno-variadic-macros -I$(CDCS_INCLUDE)
endif

# Extract relevant variant info
ifeq ($(V_WIFI),ralink)
CFLAGS+= -DV_WIFI_ralink=1
endif

CFLAGS+= -DV_USB_EP0_TIMEOUT_WATHCDOG_$(V_USB_EP0_TIMEOUT_WATHCDOG)

ifeq ($(PLATFORM),Avian)
	CFLAGS += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
	CFLAGS += -D PLATFORM_BOVINE
else ifeq ($(PLATFORM),Platypus)
	CFLAGS += -D PLATFORM_PLATYPUS
else ifeq ($(PLATFORM),Platypus2)
	CFLAGS += -D PLATFORM_PLATYPUS2
endif

LDFLAGS:=-Wall -pedantic

OBJS:=supervisor.o cbuf.o static.o

.PHONY: all
all: supervisor cbuf_test

cbuf_test: cbuf.c cbuf.h
	$(CC) $(CFLAGS) -DTEST -o $@ cbuf.c

supervisor: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(FILE_DBLIB)

$(OBJS): cbuf.h

.PHONY: install
install: supervisor
	cp supervisor $(INSTALLDIR)/sbin/
	$(STRIP) $(INSTALLDIR)/sbin/supervisor

.PHONY: clean distclean
clean distclean:
	rm -f supervisor cbuf_test *.o
