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
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

RDB_LIB := $(CDCS_LIB)/rdb.a
DAEMON_LIB := $(CDCS_LIB)/daemon_lib.a
GEMAPI_LIB := $(CDCS_LIB)/gem_api.a

#DEBUG=1

# add CDCS_LIB to the default lib path
LDFLAGS := -L $(CDCS_LIB)

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := echo
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif
CFLAGS += -Wno-unused-but-set-variable -Wno-deprecated-declarations

LIBS := $(DAEMON_LIB) $(GEMAPI_LIB) $(RDB_LIB)

MAIN_APPLICATION := w1_monitor

SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,%.o,$(SRC))

.PHONY: all
all : $(MAIN_APPLICATION)

$(MAIN_APPLICATION): $(OBJ) $(GEMAPI_LIB) $(RDB_LIB)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
	$(STRIP) $@

install: all
	install -m 0755 -d "$(INSTALLDIR)/usr/bin"
	install -m 0755 $(MAIN_APPLICATION) "$(INSTALLDIR)/usr/bin/$(MAIN_APPLICATION)"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(MAIN_APPLICATION)"

clean distclean:
	rm -f $(MAIN_APPLICATION) $(OBJ) *~
