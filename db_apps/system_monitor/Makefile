#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for PC)
CROSS_COMPILE ?= 
INSTALLDIR    ?= install

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -g -c -Wall -pedantic -Wsequence-point -Wno-variadic-macros -fPIE -fstack-protector-strong
LDFLAGS:=-Wall -pedantic -lm -pie

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

LIBS:=-l:rdb.a

PROJECT:=sysmon
OBJS:=utils.o battery.o sysmon.o netdevices.o

TESTPROG:=battery_test
TESTOBJS:=utils.o battery_test.o

.PHONY: all
all: $(PROJECT)

.PHONY: test
test: $(TESTPROG)

$(TESTPROG): $(TESTOBJS)
	$(CC) $(LDFLAGS) -o $@ $^

battery_test.o: battery.c utils.h
	$(CC) $(CFLAGS) -DTEST -o $@ $<

$(PROJECT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: install
install: $(PROJECT)
	install -D $(PROJECT) $(INSTALLDIR)/sbin/
	$(STRIP) $(INSTALLDIR)/sbin/$(PROJECT)
	install -D measurements/bat_sample.sh $(INSTALLDIR)/sbin/

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) $(TESTPROG) *.o *~

#dependencies
$(OBJS): $(wildcard *.h)
$(TESTOBJS): $(wildcard *.h)
