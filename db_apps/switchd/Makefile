#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for Platypus)
MODE ?= main

ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../../romfs
KERNELINC     ?= $(shell readlink -f ../../../linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib
else ifeq ($(PLATFORM),Platypus2)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../staging
KERNELINC     ?= $(shell readlink -f ../../RT288x_SDK/source/linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
SBASE 		  ?= $(shell readlink -f ../../)
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE) -Wsequence-point -fPIE -fstack-protector-strong
	LDFLAGS := -pie
endif

ifeq ($(MODE),recovery)
	CFLAGS+=-DMODE_recovery
endif

ifneq ($(shell echo "$(V_ETH_PORT)" | grep '^[0-9]\+$$'),)
	CFLAGS += -DV_ETH_PORT=$(V_ETH_PORT) -DUSE_SYSFS
else
	CFLAGS += -DV_ETH_PORT_$(V_ETH_PORT)
endif

CFLAGS += -DV_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_$(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN) -DV_IOBOARD_$(V_IOBOARD)

$(info ******************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info eth_port = $(V_ETH_PORT))
$(info ******************************************************************)

PROJECT = switchd
SOURCES = switchd.c daemon.c logger.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all:	$(PROJECT)

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

.PHONY : clean distclean
clean distclean:
	rm -f $(PROJECT) *.o

.PHONY: install
install: $(PROJECT)
	cp $(PROJECT) $(INSTALLDIR)/usr/bin/$(PROJECT)
