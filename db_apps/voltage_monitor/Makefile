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

CFLAGS := -Os -Wall -DBOARD_$(V_BOARD)=y -DADCMAP_$(V_ADCMAP)
LDFLAGS:=-Wall

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

LIBS:=$(CDCS_LIB)/rdb.a -lpthread

PROJECT:=voltage_monitor
OBJS:=voltage_monitor.o

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: install
install: $(PROJECT)
	install -D $(PROJECT) $(INSTALLDIR)/usr/bin/
	$(STRIP) $(INSTALLDIR)/usr/bin/$(PROJECT)

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o *~

#dependencies
$(OBJS): $(wildcard *.h)
