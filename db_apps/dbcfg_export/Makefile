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

# TODO: merge this project into rdb_manager
RDB_MGR_PATH := ../rdb_manager

FILE_DBLIB := $(CDCS_LIB)/rdb.a

CFLAGS := -c -Wall
ifdef CDCS_INCLUDE_OPTS
	CFLAGS += $(CDCS_INCLUDE_OPTS)
else
	CFLAGS += -I$(CDCS_INCLUDE)
endif

ifdef DEBUG
	CFLAGS += -g
	# This disables strip
	STRIP  := true
else
	CFLAGS += -Os
endif

LDFLAGS += -L$(CDCS_LIB)
LDFLAGS+=$(CDCS_LIB_OPTS)
ifeq ($(PLATFORM),Earwig)
	LDFLAGS += -ldl
endif
CFLAGS += -DPLATFORM_$(PLATFORM)=y -I $(RDB_MGR_PATH)

# For variants which support WiFi, a longer RDB variable length is needed for holding very long certificates.
# The definition should be aligned with that defined in Makefile for rdb_manager.
ifneq ($(V_WIFI), "none")
	CFLAGS += -DRDBMANAGER_DATABASE_VARIABLE_LENGTH=8192
else
	CFLAGS += -DRDBMANAGER_DATABASE_VARIABLE_LENGTH=4096
endif

PROJECT = dbcfg_export
SOURCES = main.c base.c base64.c sslencrypt.c crc32.c
EXT_SOURCES = growingmem.c textedit.c  dbenum.c  uricodec.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES)) $(patsubst %.c,%.o,$(EXT_SOURCES))

.PHONY: all install clean distclean

all: $(PROJECT)

$(PROJECT): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(FILE_DBLIB) -lssl -lcrypto -o $@
	$(STRIP) $@

# object files from RDB manager

uricodec.o: $(RDB_MGR_PATH)/uricodec.c $(RDB_MGR_PATH)/uricodec.h
	$(CC) $(CFLAGS) -c $< -o $@

growingmem.o: $(RDB_MGR_PATH)/growingmem.c $(RDB_MGR_PATH)/growingmem.h
	$(CC) $(CFLAGS) -c $< -o $@

textedit.o: $(RDB_MGR_PATH)/textedit.c $(RDB_MGR_PATH)/textedit.h $(RDB_MGR_PATH)/list.h
	$(CC) $(CFLAGS) -c $< -o $@

dbenum.o: $(RDB_MGR_PATH)/dbenum.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(PROJECT)
	install -D -m 755 dbcfg_export $(INSTALLDIR)/bin/dbcfg_export
	ln -sf /bin/dbcfg_export $(INSTALLDIR)/bin/dbcfg_import
	if [ "$(PLATFORM)" = "Platypus2" ]; then \
		install -D -m 755 dbcfg_default_platypus2 $(INSTALLDIR)/bin/dbcfg_default; \
	elif [ "$(PLATFORM)" = "Serpent" ]; then \
		install -D -m 755 dbcfg_default_serpent $(INSTALLDIR)/bin/dbcfg_default; \
	elif [ "$(PLATFORM)" = "Juno" ]; then \
		install -D -m 755 dbcfg_default_juno $(INSTALLDIR)/bin/dbcfg_default; \
	elif [ "$(PLATFORM)" = "Cassini" ]; then \
		install -D -m 755 dbcfg_default_cassini $(INSTALLDIR)/bin/dbcfg_default; \
	else \
		install -D -m 755 dbcfg_default $(INSTALLDIR)/bin/; \
	fi

clean distclean:
	rm -f $(PROJECT) *~ *.o
