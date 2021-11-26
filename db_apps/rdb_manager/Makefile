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
INSTALLDIR    ?= ../../romfs
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

CFLAGS := -c -Wall
ifdef CDCS_INCLUDE_OPTS
        CFLAGS += $(CDCS_INCLUDE_OPTS)
else
        CFLAGS += -I$(CDCS_INCLUDE)
endif

ifdef DEBUG
	CFLAGS += -g -O0 -DDEBUG
	STRIP  := true
else
	CFLAGS += -Os
endif

# Some parts are installed in prefix /, others in /usr. Set
# PREFIXDIR to move the /usr stuff somewhere else.
PREFIXDIR ?= $(INSTALLDIR)/usr

ifeq ($(PLATFORM),Avian)
	CFLAGS += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
	CFLAGS += -D PLATFORM_BOVINE
else ifeq ($(PLATFORM),Platypus)
	CFLAGS += -D PLATFORM_PLATYPUS
else ifeq ($(PLATFORM),Platypus2)
	CFLAGS += -D PLATFORM_PLATYPUS2
else ifeq ($(PLATFORM),Arachnid)
	CFLAGS += -D PLATFORM_ARACHNID $(PLATFORM_CFLAGS) -DRDBMANAGER_CFG_ROOT_DIR=\"/etc/cdcs/conf/\" -DRDBMANAGER_CFG_BASENAME=\"arachnid\"
endif

# For variants which support WiFi, a longer RDB variable length is needed for holding very long certificates.
ifneq ($(V_WIFI), "none")
	CFLAGS += -DRDBMANAGER_DATABASE_VARIABLE_LENGTH=8192
else
	CFLAGS += -DRDBMANAGER_DATABASE_VARIABLE_LENGTH=4096
endif

PROJECT = rdb_manager
SOURCES = textedit.c  dbenum.c growingmem.c uricodec.c daemon.c  logger.c  main.c objectlist.c templatejob.c configjob.c templatemgr.c templatelist.c base.c
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
	$(STRIP) rdb_manager
	install -m 755 $(PROJECT) $(PREFIXDIR)/bin/$(PROJECT)
	install -m 755 run-template.sh $(PREFIXDIR)/bin/run-template.sh

ifeq ($(PLATFORM),Cassini)
	# Install dev header files, qdiag needs them
	cp -f ./growingmem.h $(CDCS_INCLUDE)
	cp -f ./uricodec.h $(CDCS_INCLUDE)
	cp -f ./textedit.h $(CDCS_INCLUDE)
	cp -f ./list.h $(CDCS_INCLUDE)
	cp -f ./base.h $(CDCS_INCLUDE)
endif



