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

ifdef DEBUG
	CFLAGS := -g -c -O0 -Wall -Werror -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include -DDEBUG
	STRIP  := true
else
	CFLAGS := -c -Wall -Werror -Os -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include
endif

DAEMON_LIB = $(CDCS_LIB)/daemon_lib.a

LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB) -lrdb -lbluetooth -lglib-2.0 -lgio-2.0 -lpthread -lm $(CDCS_LIB)/daemon_lib.a

TEMPINSTALL := $(shell /bin/pwd)/install
ifeq ($(MAKECMDGOALS),package)
prefix=usr/local/bin
else
prefix=usr/bin
endif

PROJECT = btmgr
SOURCES = btmgr.c bluez_support.c btmgr_rpc.c
DEPS = bluez_support.h btmgr_priv.h
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/$(prefix)/
	install -m 755 $(PROJECT) $(TEMPINSTALL)/$(prefix)/
	install -m 755 launch_btmgr.sh $(TEMPINSTALL)/$(prefix)/

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(PROJECT).a $(OBJECTS);
	rm -f *~ ~*;

$(OBJECTS) : $(DEPS)

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.PHONY: package
package: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)/btmgr" "$(CDCS_LIBSTAGE)" -b -l --ipk CONTROL -D /usr/local
.PHONY: install
install: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr
