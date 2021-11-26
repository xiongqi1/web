#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

CDCS_INCLUDE  ?= ../../staging_l/include

# this allows to do: export LOCAL_COMPILE=Yes
# followed by make or make unit_test from /cdcs_libs/rdb_lib
# of course this will only work on targets that use arm compiler below
ifeq ($(LOCAL_COMPILE),Yes)
    CROSS_COMPILE ?= /opt/cross/gcc-4.3.4-glibc-2.9/bin/arm-cdcs-linux-gnueabi-
else
    CROSS_COMPILE ?= arm-linux-
endif

# set compiler environment variables

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CDCS_LIB    ?= ../../staging_l/lib
RDB_LIB :=$(CDCS_LIB)/rdb.a

CFLAGS := -Wall -Os -I$(CDCS_INCLUDE) -DHAVE_RDB -fPIC
LDFLAGS += -shared -L$(CDCS_LIB) -lwebsockets -lz -lssl -lcrypto -luv -lcap

PROJECT = libnetc_netstat.so
SOURCES = netc_netstat.c netstat.c rdbstat.c ds_store.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
TEMPINSTALL := install

.PHONY : all
all: $(PROJECT)

.PHONY : clean distclean
clean distclean:
	rm -f *.o *.so;

$(PROJECT): $(OBJECTS) $(RDB_LIB)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) $(RDB_LIB) -L$(CDCS_LIB) -o $@


.PHONY: install
install: $(PROJECT)
	$(STRIP) $(PROJECT)
	mkdir -p $(TEMPINSTALL)/lib
	mkdir -p $(TEMPINSTALL)/share/lwsws/plugins
	mkdir -p $(TEMPINSTALL)/share/lwsws/netc_netstat
	mkdir -p $(TEMPINSTALL)/etc/lwsws/conf.d
	cp $(PROJECT) $(TEMPINSTALL)/share/lwsws/plugins
	cp static/* $(TEMPINSTALL)/share/lwsws/netc_netstat
	cp etc/lwsws/conf $(TEMPINSTALL)/etc/lwsws/conf
	cp etc/lwsws/conf.d/* $(TEMPINSTALL)/etc/lwsws/conf.d
	scatter_install.sh -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D share/lwsws -D etc -b -l
