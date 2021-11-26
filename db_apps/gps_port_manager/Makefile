#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# MODE           = Build mode (for bovine: recovery, main)
# SBASE          = Source tree base directory

PLATFORM 	  ?= Platypus
VARIANT ?= $(shell cat "../../../variant.last_build" 2> /dev/null )

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
	CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

CFLAGS := -I$(CDCS_INCLUDE)
LDFLAGS := -L$(CDCS_LIB)

ifeq ($(V_MODCOMMS),y)
CFLAGS += -DV_MODCOMMS
endif

# ModComm UBX GPs needs latest gpsd v3.16 which has static libgpsd.
ifeq ($(V_GPS_HW),ubx)
LIBS := -lm -l:$(FILE_DBLIB) -lgps
else
LIBS := -lm $(FILE_DBLIB) -lgps -lgpsd
endif

ifeq ($(PLATFORM),Platypus)
	RTSDK = $(shell CURDIR=`pwd`; echo $${CURDIR%%/user/cdcs/gps_port_manager})
	LIBNV_BIN = libnvram.a
	LIBNV_DIR = $(RTSDK)/lib/libnvram
	LIBNV_FNAME = $(LIBNV_DIR)/$(LIBNV_BIN)
	CFLAGS += -I $(LIBNV_DIR)
	LIBS += -l:$(LIBNV_FNAME)
endif

TEST_LIBS := $(LIBS) $(CDCS_LIB)/unit_test.a $(FILE_DBLIB)

MAIN_APPLICATION := gps_port_manager
GPSD_CLIENT_APPL := gpsd_client
GPSD_LUA_CLIENT_APPL := gpsd_client.lua

SRC := $(MAIN_APPLICATION).c gps_common.c $(wildcard util/*.c)
OBJ := $(patsubst %.c,%.o,$(SRC))

GPSDC_SRC := $(GPSD_CLIENT_APPL).c gps_common.c $(wildcard util/*.c)
GPSDC_OBJ := $(patsubst %.c,%.o,$(GPSDC_SRC))

TEST_SRC := $(wildcard test/*.c)
TEST_OBJ := $(patsubst %.c,%.o, util/gps.c util/rdb_util.c $(TEST_SRC))

all : $(MAIN_APPLICATION) $(GPSD_CLIENT_APPL) unit_test

$(MAIN_APPLICATION): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@ $(LIBS)

$(GPSD_CLIENT_APPL): $(GPSDC_OBJ)
	$(CC) $(LDFLAGS) $(GPSDC_OBJ) -o $@ $(LIBS)

unit_test: $(TEST_OBJ)
	$(CC) $(LDFLAGS) $(TEST_OBJ) -o $@ $(TEST_LIBS)

install: all
	install -m 755 $(MAIN_APPLICATION) $(INSTALLDIR)/usr/bin/
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(MAIN_APPLICATION)"
	install -m 755 $(GPSD_CLIENT_APPL) $(INSTALLDIR)/usr/bin/
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(GPSD_CLIENT_APPL)"
	install -m 755 $(GPSD_LUA_CLIENT_APPL) $(INSTALLDIR)/usr/bin/

romfs: all
	$(STRIP) $(MAIN_APPLICATION) $(GPSD_CLIENT_APPL)
	$(ROMFSINST) /usr/bin/$(MAIN_APPLICATION)
	$(ROMFSINST) /usr/bin/$(GPSD_CLIENT_APPL)

clean distclean:
	rm -f $(OBJ) $(MAIN_APPLICATION) $(GPSD_CLIENT_APPL) unit_test $(GPSDC_OBJ) $(TEST_OBJ)
	rm -f ../$(MAIN_APPLICATION).dep ../$(MAIN_APPLICATION).done
