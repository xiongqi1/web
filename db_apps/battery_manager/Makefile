#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
#
# V_BATTERY_CHARGER = battery charger ic model: fan54005 or max17050
# V_BATTERY_MONITOR = fuel gauge ic model: max17050 or max17201

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

# this is needed by max17xxx.c
CFLAGS += -DV_BATTERY_MONITOR_$(V_BATTERY_MONITOR) -DV_BATTERY_CHARGER_$(V_BATTERY_CHARGER)

PROJECT:=battery_manager

# EVT1: V_BATTERY_CHARGER=fan54005 V_BATTERY_MONITOR=max17050
# EVT2: V_BATTERY_CHARGER=bq25601 V_BATTERY_MONITOR=max17201
OBJS:=main.o i2c_linux.o battery_rdb.o $(V_BATTERY_CHARGER).o max17xxx.o

DEPS = $(wildcard *.h)

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c Makefile $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

TEMPINSTALL := $(shell /bin/pwd)/install

.PHONY: install
install: $(PROJECT)
	mkdir -p $(TEMPINSTALL)/sbin
	install -m 0755 $(PROJECT) $(TEMPINSTALL)/sbin
	scatter_install.sh -l -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -b

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o
	rm -rf $(TEMPINSTALL)
