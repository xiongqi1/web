#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -Os -Wall -pedantic -std=gnu99 -Werror
LDFLAGS:= -lrdb -lrt

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

LIBS:=$(CDCS_LIB)/libwpa_ctrl.a

TEMPINSTALL := $(shell /bin/pwd)/install
ifeq ($(MAKECMDGOALS),package)
prefix=usr/local/bin
else
prefix=usr/bin
endif

PROJECT:=wifi_client_monitor
OBJS:=main.o event_loop.o wpa_supp.o wpa_rdb.o

.PHONY: all
all: $(PROJECT)
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/$(prefix)/
	install -m 755 $(PROJECT) $(TEMPINSTALL)/$(prefix)/

$(PROJECT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: install
install: $(PROJECT)
	install -D $(PROJECT) $(INSTALLDIR)/sbin/
	$(STRIP) $(INSTALLDIR)/sbin/$(PROJECT)

.PHONY: package
package: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)/$(PROJECT)" "$(CDCS_LIBSTAGE)" -b -l --ipk CONTROL -D /usr

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o *~

#dependencies
$(OBJS): $(wildcard *.h)
