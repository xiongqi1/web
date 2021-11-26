#CDCS_INCLUDE ?= /home/yong/src/products/build-Platypus-ts_3g38wv-trunk/Platypus_src/RT_sdk/staging_l/include
#CDCS_LIB ?= /home/yong/src/products/build-Platypus-ts_3g38wv-trunk/Platypus_src/RT_sdk/staging_l/lib
#CROSS_COMPILE ?= /opt/buildroot-gcc342/bin/mipsel-linux-uclibc-
#PLATFORM ?= Platypus
#V_BOARD ?= 3g38wv
#V_SKIN ?= ts

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
INSTALLDIR    ?= ../../../romfs
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a
FILE_MXML := $(CDCS_LIB)/libmxml.a
FILE_PASSGEN := $(CDCS_LIB)/libpassgen.a

ifdef DEBUG
   CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE)
   # This disables strip
   STRIP  := true
else
   CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

ROOTDIR ?= $(shell CURDIR=`pwd`; echo $${CURDIR%%/user/cdcs/connection_mgr/wwan_daemon})

NVDIR = $(ROOTDIR)/lib/libnvram
GPIODIR = $(CDCS_INCLUDE)
LINUXDIR = $(ROOTDIR)/linux-2.6.21.x/include
LDFLAGS += -L$(NVDIR) -lnvram
CFLAGS += -I$(NVDIR) -I$(GPIODIR) -I$(LINUXDIR)

ifeq ($(PLATFORM),Avian)
   CFLAGS += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
   CFLAGS += -D PLATFORM_BOVINE
else ifeq ($(PLATFORM),Platypus)
   CFLAGS += -D PLATFORM_PLATYPUS
else ifeq ($(PLATFORM),Serpent)
   CFLAGS += -D PLATFORM_SERPENT
else ifeq ($(MAKECMDGOALS),clean)
   # We let this pass
else ifeq ($(MAKECMDGOALS),distclean)
   # We let this pass
else
   $(error "Unknown Platform $(PLATFORM)")
endif

CFLAGS+=-DBOARD_$(V_BOARD)=y -DSKIN_$(V_SKIN)=y -DMEPLOCK_$(V_MEPLOCK)

ifneq ($(V_3GFAILOVERNOTICATION), none)
CFLAGS+=-D USE_3G_FAILOVER_NOTIFICATION
endif

PROJECT = wwand

# c sources
#SOURCES = daemon.c logger.c ping.c rearswitch.c tickcount.c wwan_daemon.c ../autoapn.c
SOURCES = daemon.c logger.c ping.c rearswitch.c tickcount.c wwan_daemon.c ../autoapn.c tnslookup.c 
OBJECTS = $(patsubst %.c,%.o,$(SOURCES)) 

.PHONY: all clean distclean install

all: $(PROJECT)


$(PROJECT): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(FILE_DBLIB) $(FILE_PASSGEN) $(NVDIR)/libnvram.a $(CDCS_LIB)/libgpio.a $(FILE_MXML) -lpthread -o $@
	$(STRIP) $@

install: all
	install -m 0755 -d $(INSTALLDIR)/usr/bin
	install -m 0755 $(PROJECT) $(INSTALLDIR)/usr/bin/$(PROJECT)
	install -m 0755 ../score_connection.sh $(INSTALLDIR)/usr/bin/score_connection.sh
	install -m 0755 -d $(INSTALLDIR)/etc_ro/cdcs/connectscripts
	install -m 755 connectscripts/* $(INSTALLDIR)/etc_ro/cdcs/connectscripts/

clean distclean:
	rm -f $(PROJECT) *.o *~

