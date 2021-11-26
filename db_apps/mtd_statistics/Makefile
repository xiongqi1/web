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
SBASE	      ?= $(shell readlink -f ../../)
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

# Detect GCC version to control compiler options
GCC_MAJOR := $(shell $(CC) -dumpversion 2>&1 | cut -d'.' -f1)
GCC_MINOR := $(shell $(CC) -dumpversion 2>&1 | cut -d'.' -f2)

$(info GCC version $(GCC_MAJOR).$(GCC_MINOR) )

ifeq ($(GCC_MAJOR),2)
	CFLAGS := -Wall -W
else
	CFLAGS := -pedantic -Wall -Wsequence-point -W -Wno-variadic-macros
endif

ifdef DEBUG
	CFLAGS += -g -c
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Os -fPIE -fstack-protector-strong
	LFLAGS += -pie
endif
CFLAGS += -I$(CDCS_INCLUDE)

$(info PLATFORM=$(PLATFORM) )
$(info V_MAX_USAGE_HISTORY=$(V_MAX_USAGE_HISTORY) )

CFLAGS += -D PLATFORM_$(PLATFORM) -DBOARD_$(V_BOARD)=y -DSKIN_$(V_SKIN)=y -DV_MAX_USAGE_HISTORY_$(V_MAX_USAGE_HISTORY)=y

#PROJECTS = mtd_statistics wwan_monitor

# c sources
ifeq ($(V_MULTIPLE_WWAN_PROFILES),y)
MTD_STATISTICS_SOURCES = flash_statistics.c mtd_statistics2.c
WWAN_USAGE_SOURCES = wwan_monitor2.c daemon.c
else
MTD_STATISTICS_SOURCES = flash_statistics.c mtd_statistics.c
WWAN_USAGE_SOURCES = wwan_monitor.c daemon.c
endif

MTD_STATISTICS_OBJECTS = $(patsubst %.c,%.o,$(MTD_STATISTICS_SOURCES))

WWAN_USAGE_OBJECTS = $(patsubst %.c,%.o,$(WWAN_USAGE_SOURCES))

.PHONY: all
ifeq ($(V_MULTIPLE_WWAN_PROFILES),y)
all: mtd_statistics2 wwan_monitor2
else
all: mtd_statistics wwan_monitor
endif

ifeq ($(V_MULTIPLE_WWAN_PROFILES),y)
mtd_statistics2: $(MTD_STATISTICS_OBJECTS)
else
mtd_statistics: $(MTD_STATISTICS_OBJECTS)
endif
	$(CC) $(LFLAGS) $(MTD_STATISTICS_OBJECTS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

ifeq ($(V_MULTIPLE_WWAN_PROFILES),y)
wwan_monitor2: $(WWAN_USAGE_OBJECTS)
else
wwan_monitor: $(WWAN_USAGE_OBJECTS)
endif
	$(CC) $(LFLAGS) $(WWAN_USAGE_OBJECTS) flash_statistics.o $(FILE_DBLIB) -o $@
	$(STRIP) $@

.PHONY: clean distclean
clean distclean:
	rm -f mtd_statistics wwan_monitor *.o
	rm -f mtd_statistics2 wwan_monitor2 *.o

.PHONY: install
install: all
	mkdir -p $(INSTALLDIR)/bin
ifeq ($(PLATFORM),Platypus)
	cp mtd_statistics $(INSTALLDIR)/bin/
	cp wwan_monitor $(INSTALLDIR)/bin/usage_monitor
else ifeq ($(PLATFORM),Platypus2)
	cp mtd_statistics $(INSTALLDIR)/bin/
	cp wwan_monitor $(INSTALLDIR)/bin/usage_monitor
else ifeq ($(V_MULTIPLE_WWAN_PROFILES),y)
	cp mtd_statistics2 $(INSTALLDIR)/bin/mtd_statistics
	cp wwan_monitor2 $(INSTALLDIR)/bin/wwan_monitor
else
	cp mtd_statistics $(INSTALLDIR)/bin/
	cp wwan_monitor $(INSTALLDIR)/bin/
endif
	$(STRIP) $(INSTALLDIR)/bin/mtd_statistics

.PHONY: romfs
romfs:
	$(ROMFSINST) /bin/mtd_statistics
