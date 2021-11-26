#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -c -Wall -Werror -Os -I$(CDCS_INCLUDE)
LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB)
LDLIBS := -lrdb $(CDCS_LIB)/daemon_lib.a

TEMPINSTALL := $(shell /bin/pwd)/install
TEMPINSTALL_BIN := $(TEMPINSTALL)/usr/bin

PROJECT = nand_scrub_daemon
SOURCES = main.c util.c ubi.c mtd.c
DEPS = nand_scrub_daemon.h util.h ubi.h
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
PREFIX=/usr/bin
all: $(PROJECT)
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL_BIN)
	install -m 0755 $(PROJECT) $(TEMPINSTALL_BIN)

.PHONY : clean distclean
clean distclean:
	rm -f $(PROJECT) $(OBJECTS);
	rm -f *~ ~*;

$(OBJECTS) : $(DEPS)

$(PROJECT): $(OBJECTS)
	$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

.PHONY: package
package: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -b -l --ipk CONTROL -D /usr/local

.PHONY: install
install: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr
