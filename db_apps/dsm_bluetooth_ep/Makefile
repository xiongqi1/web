#
# Copyright (C) 2016 NetComm Wireless limited.
#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for Platypus)
CROSS_COMPILE ?= arm-ntc-linux-gnueabi-
INSTALLDIR    ?= ../../romfs
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include -I$(CDCS_INCLUDE)/gio-unix-2.0 -Werror

# Uncomment the next line to enable debugs and asserts
DEBUG=1
#
ifdef DEBUG
CFLAGS += -DDEBUG
else
# disable asserts
CFLAGS += -DNDEBUG
endif

LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB) -lrdb -lglib-2.0 -lgio-2.0

TEMPINSTALL := $(shell /bin/pwd)/install

PROJECT = dsm_bluetooth_ep
SOURCES = dsm_bluetooth_ep.c dsm_bluetooth_device.c dsm_btep_rpc.c dsm_gatt_rpc.c dsm_bt_utils.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

DEPS = dsm_bluetooth_ep.h dsm_btep_rpc.h dsm_gatt_rpc.h dsm_bt_utils.h
LAUNCHER = dsm_bluetooth_ep.sh

export PATH
export CROSS_COMPILE
export CDCS_INCLUDE
export CDCS_LIB

TEST_SUBDIR = test

.PHONY : all
all: $(PROJECT) $(CLIENT_PROJECT)
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin/
	install -m 755 $(PROJECT) $(TEMPINSTALL)/usr/bin/
	install -m 755 $(LAUNCHER) $(TEMPINSTALL)/usr/bin/
	$(MAKE) -C $(TEST_SUBDIR) all

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(PROJECT).a $(OBJECTS);
	rm -f *~ ~*;
	$(MAKE) -C $(TEST_SUBDIR) clean

$(OBJECTS) : $(DEPS)

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.PHONY: install
install: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr
