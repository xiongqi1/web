#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for Platypus)
CROSS_COMPILE ?= arm-ntc-linux-gnueabi-
INSTALLDIR    ?= ../../romfs
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -g -c -O0 -Wall -Werror -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include -DDEBUG
	STRIP  := true
else
	CFLAGS := -c -Wall -Werror -Os -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include
endif

LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB) -lbluetooth -lglib-2.0 -lcurl

TEMPINSTALL := $(shell /bin/pwd)/install

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

PROJECT = bt_sensord
SOURCES = main.c td2551.c gatt/att.c gatt/gatt.c gatt/uuid.c gatt/crypto.c gatt/gattrib.c gatt/btio.c gatt/log.c gatt/utils.c non3230.c mgh.c serial_support.c healthpoint.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin/
	install -m 755 $(PROJECT) $(TEMPINSTALL)/usr/bin/
	install -m 755 bt_pair.sh $(TEMPINSTALL)/usr/bin/
	install -m 755 bt_sensord.sh $(TEMPINSTALL)/usr/bin/

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(PROJECT).a $(OBJECTS);
	rm -f *~ ~*;

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(FILE_DBLIB) -o $@

.PHONY: install
install: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr
