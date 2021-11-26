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
FILE_DAEMONLIB := $(CDCS_LIB)/daemon_lib.a

ifdef DEBUG
	CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

PROJECT = atportmgr

ifdef CONFIG_RALINK_RT3052
	RTSDK = $(shell CURDIR=`pwd`; echo $${CURDIR%%/user/cdcs/atportmgr})
	LIBNV_BIN = libnvram.a
	LIBNV_DIR = $(RTSDK)/lib/libnvram

	LIBNV_FNAME = $(LIBNV_DIR)/$(LIBNV_BIN)

	CFLAGS +=-I $(LIBNV_DIR) -DCONFIG_RALINK_RT3052
	LDFLAGS += $(LIBNV_FNAME)
endif

# c sources
SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY: all
all: $(PROJECT)

ifdef CONFIG_RALINK_RT3052

libusb.a:       ../libusb/.libs/libusb.a
		ln -sf $^ $@

usb.h:          ../libusb/usb.h
		ln -sf $^ $@

detectinfo.h:   ../hotplug/detectinfo.h
		ln -sf $^ $@

$(SOURCES): usb.h detectinfo.h libusb.a
endif

$(PROJECT): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(FILE_DBLIB) $(FILE_DAEMONLIB) $(LIBNV_FNAME) libusb.a -o $@ -L .
	$(STRIP) $@

.PHONY: clean distclean
clean distclean:
	rm -f *.o $(PROJECT) *~ *.suo *.ncb *.vcproj.*.user
	rm -fr Debug Release
	chmod 644 *.[ch] *.sln *.vcproj

.PHONY: install
install: $(PROJECT)
	cp $(PROJECT) $(INSTALLDIR)/usr/bin/
	$(STRIP) $(INSTALLDIR)/usr/bin/$(PROJECT)

.PHONY: romfs
romfs: all
	$(ROMFSINST) /usr/bin/$(PROJECT)
