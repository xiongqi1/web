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
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
KERNELINC     ?= $(shell readlink -f ../../kernel/include)
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -g -c -Wall -O0 -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

ifeq ($(PLATFORM),Platypus2)
	CFLAGS += -D PLATFORM_Platypus2
endif

ifeq ($(V_GPS_ON_AT),y)
	CFLAGS+= -DGPS_ON_AT
endif

ifeq ($(V_GPS),y)
    CFLAGS+= -DGPS_ENABLED
endif

PROJECT = cnsmgr
# c sources
SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(PROJECT).a $(OBJECTS);
	rm -f *~ ~*;
	rm -f  ~*$(PROJECT).vcproj $(PROJECT).vcproj.*.user $(PROJECT).{suo,ncb};
	rm -fr Debug Release;
	rm -fr latex;

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

.PHONY: install
install: $(PROJECT)
	cp $(PROJECT) $(INSTALLDIR)/usr/bin/
	$(STRIP) $(INSTALLDIR)/usr/bin/$(PROJECT)

.PHONY: romfs
romfs: $(PROJECT)
	$(ROMFSINST) /usr/bin/$(PROJECT)
