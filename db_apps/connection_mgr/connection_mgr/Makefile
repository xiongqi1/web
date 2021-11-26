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

# Some parts are installed in prefix /, others in /usr. Set
# PREFIXDIR to move the /usr stuff somewhere else.
PREFIXDIR ?= $(INSTALLDIR)/usr

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE)
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

PROJECT = connection_mgr

ifeq ($(PLATFORM),Avian)
   CFLAGS += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
   CFLAGS += -D PLATFORM_BOVINE
else ifeq ($(PLATFORM),Antelope)
   CFLAGS += -D PLATFORM_ANTELOPE
else ifeq ($(PLATFORM),Platypus)
   CFLAGS += -D PLATFORM_PLATYPUS
else ifeq ($(PLATFORM),Platypus2)
   CFLAGS += -D PLATFORM_PLATYPUS2
else ifeq ($(PLATFORM),Serpent)
   CFLAGS += -D PLATFORM_SERPENT
endif

CFLAGS += -DMODULE_$(V_MODULE) -DBOARD_$(V_BOARD) -DMODULETYPE_$(V_MODULETYPE) -DCELL_NW_$(V_CELL_NW) -DV_MODULE_ERROR_RECOVERY_$(V_MODULE_ERROR_RECOVERY)

ifeq ($(V_DISP_DAEMON),y)
CFLAGS += -DV_DISP_DAEMON
endif

ifeq ($(V_SECONDARY_3GWWAN),y)
CFLAGS += -DV_SECONDARY_3GWWAN
endif

ifeq ($(V_MEPLOCK),y)
CFLAGS += -DV_MEPLOCK
endif

ifeq ($(V_VODAFONE_SPEC),y)
CFLAGS += -DV_VODAFONE_SPEC
endif

ifeq ($(V_CUSTOM_FEATURE_PACK),Santos)
CFLAGS += -DV_TELSTRA_SPEC_Santos
else ifeq ($(V_CUSTOM_FEATURE_PACK),$(filter $(V_CUSTOM_FEATURE_PACK), Telstra_6200 Select_Solutions))
CFLAGS += -DV_TELSTRA_SPEC_Cinterion_module
else ifeq ($(V_CUSTOM_FEATURE_PACK),$(filter $(V_CUSTOM_FEATURE_PACK), Telstra_140 Telstra_145w))
CFLAGS += -DV_TELSTRA_SPEC_Sierra_LTE_module
endif

ifeq ($(V_MODULE_PRI_BASED_OPERATION),y)
	CFLAGS += -D MODULE_PRI_BASED_OPERATION
endif

ifeq ($(V_USE_DCCD),y)
CFLAGS += -D USE_DCCD
endif

CFLAGS += -DV_MANUAL_ROAMING_$(V_MANUAL_ROAMING)

# c sources
SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)

$(PROJECT): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(FILE_DBLIB) $(CDCS_LIB)/libpassgen.a -o $@
	$(STRIP) $@

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o

.PHONY: install
install: $(PROJECT)
	mkdir -p $(PREFIXDIR)/bin/
	cp $(PROJECT) $(PREFIXDIR)/bin/
	$(STRIP) $(PREFIXDIR)/bin/$(PROJECT)
	install -d $(INSTALLDIR)/etc/cdcs/connectscripts
	cp -d connectscripts/* $(INSTALLDIR)/etc/cdcs/connectscripts/
