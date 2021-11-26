#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../../romfs
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib
KERNELINC     ?= $(shell readlink -f ../../../linux-2.6.21.x/include)
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../../staging
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib
KERNELINC     ?= $(shell readlink -f ../../kernel/include)
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

ifdef DEBUG
	CFLAGS := -g -c -pedantic -Wall -Wno-variadic-macros -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -pedantic -Os -Wno-variadic-macros -I$(CDCS_INCLUDE)
endif

ifdef CDCS_INCLUDE_OPTS
CFLAGS += -I$(CDCS_INCLUDE_OPTS)
endif

CFLAGS += -DV_STARTUP_WATCHDOG_$(V_STARTUP_WATCHDOG) -DV_RESET_LOGGER_$(V_RESET_LOGGER)

LDFLAGS:=-L$(CDCS_LIB) -lrdb

OBJS:=watchdog_main.o watchdog_logger.o

.PHONY: all
all: rdb_watchdog

rdb_watchdog: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(FILE_DBLIB)

.PHONY: install
install: rdb_watchdog
	mkdir -p $(INSTALLDIR)/usr/bin/
	mkdir -p $(INSTALLDIR)/sbin/
	cp wdt_commit $(INSTALLDIR)/usr/bin/
	cp rdb_watchdog $(INSTALLDIR)/sbin/
	$(STRIP) $(INSTALLDIR)/sbin/rdb_watchdog
# Platforms prior to Cassini supplied the startup scripts and config from fsextra
# Going forward it would be best to add those support files below to maintain modularity
ifeq ($(V_STARTUP_TYPE),systemd)
	mkdir -p $(INSTALLDIR)/lib/systemd/system/multi-user.target.wants
	cp rdb_watchdog.service $(INSTALLDIR)/lib/systemd/system
	ln -sf /lib/systemd/system/rdb_watchdog.service $(INSTALLDIR)/lib/systemd/system/multi-user.target.wants/rdb_watchdog.service
endif
	# V_VARs should be in the make environment
	mkdir -p $(INSTALLDIR)/etc
	./rdb_watchdog.conf__fsx.sh > $(INSTALLDIR)/etc/rdb_watchdog.conf

.PHONY: clean distclean
clean distclean:
	rm -f rdb_watchdog *.o
