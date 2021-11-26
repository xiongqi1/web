#
# Expects the following external variables to be set
#
# PLATFORM	 = Platform name
# PATH           = Must include a path to the correct toolchain
# CDCS_LIB	 = CDCS library path
# CDCS_INCLUDE	 = CDCS include files, including RDB, libs, etc.
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install

ifeq ($(PLATFORM),Bovine)
CROSS_COMPILE ?= arm-linux-
CDCS_LIB ?= ../../../staging_l/lib
endif

INSTALL_BIN=$(INSTALLDIR)/usr/bin
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)gcc
ifndef CDCS_LIB_OPTS
	CDCS_LIB_OPTS:=-L$(CDCS_LIB)
endif
ifndef CDCS_INCLUDE_OPTS
	CDCS_INCLUDE_OPTS:=-I$(CDCS_INCLUDE)
endif
CFLAGS=-g -Wno-format $(CDCS_INCLUDE_OPTS) $(CDCS_LIB_OPTS) -DDEBUGS_ON
DEPS=
OBJ= cwmp-crd.o http_auth.o http_parse.o
LIBS=-lssl -lcrypto -pthread
ifeq ($(PLATFORM),Earwig)
	LIBS += -ldl
endif

FILE_DBLIB := -lrdb
TEMPINSTALL := $(shell /bin/pwd)/install

.PHONY: all build clean distclean

all: build

install: build
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin
	install -m 0755 cwmp-crd $(TEMPINSTALL)/usr/bin
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" \
	-f /usr/bin/cwmp-crd

package: build
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin
	install -m 0755 cwmp-crd $(TEMPINSTALL)/usr/bin
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" \
	-f /usr/bin/cwmp-crd --ipk CONTROL

build: cwmp-crd cwmp-crd-testclient

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

cwmp-crd: $(OBJ)
	$(LD) -o $@ $^ $(CFLAGS) $(LIBS) $(FILE_DBLIB)

cwmp-crd-testclient: cwmp-crd-testclient.o http_auth.o http_parse.o
	$(LD) -o $@ $^ $(CFLAGS) $(LIBS) $(FILE_DBLIB)

clean:
	rm -f cwmp-crd cwmp-crd-testclient *.o

distclean: clean
