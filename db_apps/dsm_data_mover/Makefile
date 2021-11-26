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
else ifeq ($(PLATFORM),Avian)
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging/cdcs
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
PREFIXDIR     ?= $(INSTALLDIR)
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

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

FILE_DBLIB := $(CDCS_LIB)/rdb.a
LIBS := $(FILE_DBLIB) $(CDCS_LIB)/libgpio.a -lpthread -lrt

ifdef DEBUG
	CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE)
	#CFLAGS := -g -c -o -Wall -pedantic -Os -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
	#CFLAGS := -c -o -Wall -pedantic -Os -I$(CDCS_INCLUDE)
endif

ifeq ($(V_SERIAL),/dev/ttyAPP1)
	CFLAGS += -DSERIAL_PORT_TTYAPP1
else ifeq ($(V_SERIAL),/dev/ttyAPP2)
	CFLAGS += -DSERIAL_PORT_TTYAPP2
else ifeq ($(V_SERIAL),/dev/ttyAPP3)
	CFLAGS += -DSERIAL_PORT_TTYAPP3
else ifeq ($(V_SERIAL),/dev/ttyAPP4)
	CFLAGS += -DSERIAL_PORT_TTYAPP4
endif

CFLAGS += -DV_GPIO_STYLE_$(V_GPIO_STYLE) -DV_SERIAL_HAS_FC_$(V_SERIAL_HAS_FC) -DV_SUPPRESS_RESPONSE_$(V_SUPPRESS_RESPONSE)
CFLAGS += -DV_KEEP_SINGLE_TCP_CONN_$(V_KEEP_SINGLE_TCP_CONN) -DV_ME_LOCAL_CMD_$(V_ME_LOCAL_CMD)

$(info ******************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info gpio_stype=$(V_GPIO_STYLE))
$(info serial_port=$(V_SERIAL))
$(info serial_has_fc=$(V_SERIAL_HAS_FC))
$(info suppress_response=$(V_SUPPRESS_RESPONSE))
$(info keep_single_tcp=$(V_KEEP_SINGLE_TCP_CONN))
$(info me_local_cmd=$(V_ME_LOCAL_CMD))
$(info ******************************************************************)

PROJECT = dsm_data_mover

# c sources
SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))


.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	$(STRIP) $@

$(OBJECTS) : dsm_data_mover.h

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o

.PHONY: install
install: all
	cp $(PROJECT) $(INSTALLDIR)/usr/bin/
	$(STRIP) $(INSTALLDIR)/usr/bin/$(PROJECT)
