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

CFLAGS := -Os -std=gnu99 -Wall -pedantic
LDFLAGS:=-Wall -pedantic -lrdb -lm

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

PROJECT:=batman
OBJS:=battmonitor.o power_nl_event.o battery_imx28.o

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: install
install: $(PROJECT)
	install -D $(PROJECT) $(INSTALLDIR)/sbin/
	$(STRIP) $(INSTALLDIR)/sbin/$(PROJECT)

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) $(TESTPROG) *.o *~

#dependencies
$(OBJS): $(wildcard *.h)
$(TESTOBJS): $(wildcard *.h)
