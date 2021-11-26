#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# GEM_API_LIB       = CDCS library path

CDCS_INCLUDE  ?= ../../staging_l/include

# this allows to do: export LOCAL_COMPILE=Yes
# followed by make or make unit_test from /cdcs_libs/rdb_lib
# of course this will only work on targets that use arm compiler below
ifeq ($(LOCAL_COMPILE),Yes)
    CROSS_COMPILE ?= /opt/cross/gcc-4.3.4-glibc-2.9/bin/arm-cdcs-linux-gnueabi-
else
    CROSS_COMPILE ?= arm-linux-
endif


# set compiler environment variables

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CDCS_LIB    ?= ../../staging_l/lib
RDB_LIB :=$(CDCS_LIB)/rdb.a
GEM_API_LIB :=$(CDCS_LIB)/gem_api.a

CFLAGS := -Wall -Os -I$(CDCS_INCLUDE)
LDFLAGS += -L$(CDCS_LIB) -lcdcsbase64

PROJECT = gem_tool
SOURCES = gem_tool.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)

.PHONY : clean distclean
clean distclean:
	rm -f gem_tool;
	rm -f gem_tool.o;

$(PROJECT): $(OBJECTS) $(GEM_API_LIB) $(RDB_LIB)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) $(GEM_API_LIB) $(RDB_LIB) -lm -ldl -L$(CDCS_LIB) -o $@
	$(STRIP) $@

.PHONY: install
install: $(PROJECT)
	$(STRIP) $(PROJECT)
	install -m 755 $(PROJECT) $(INSTALLDIR)/usr/bin/$(PROJECT)
