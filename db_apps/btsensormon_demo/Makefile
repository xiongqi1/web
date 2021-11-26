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

CFLAGS := -c -Wall -Werror -Os -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include -I$(CDCS_INCLUDE)/gio-unix-2.0

#CFLAGS += -DDEBUG

LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB) -lrdb -lglib-2.0 -lgio-2.0 -lpthread -lm

TEMPINSTALL := $(shell /bin/pwd)/install

PROJECT = btsensormon
CLIENT_SRC := taidoc.c nonin.c
CLIENT_DEPS := taidoc.h
SOURCES = btsensormon.c btsm_device.c btsm_client.c $(CLIENT_SRC)
DEPS = btsensormon.h $(CLIENT_DEPS)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)
	mkdir -p "$(TEMPINSTALL)"
	mkdir -p "$(TEMPINSTALL)/usr/bin/"
	mkdir -p "$(TEMPINSTALL)/etc/init.d/rc.d"
	mkdir -p "$(TEMPINSTALL)/etc/init.d/rc3.d"
	mkdir -p "$(TEMPINSTALL)/www"

	install -m 0755 $(PROJECT) "$(TEMPINSTALL)/usr/bin/"
	install -m 0755 btsensormon_rc "$(TEMPINSTALL)/etc/init.d/rc.d/$(PROJECT)"
	cp -r www "$(TEMPINSTALL)/"
	ln -sf "../rc.d/$(PROJECT)" "$(TEMPINSTALL)/etc/init.d/rc3.d/Sbbb_btsensormon"

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(PROJECT).a $(OBJECTS);
	rm -f *~ ~*;

$(OBJECTS) : $(DEPS)

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.PHONY: install
install: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr -D /etc -D /www

package: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)/$(PROJECT)" "$(CDCS_LIBSTAGE)" -D /usr -D /etc -D /www --ipk CONTROL
