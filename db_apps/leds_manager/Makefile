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

CFLAGS += -DV_IOBOARD_$(V_IOBOARD)

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

PROJECT:=leds_manager
OBJS:=main.o event_loop.o leds_model.o leds_model_base.o leds_ops.o rdb_helper.o setup_leds_mode.o

# select the LEDs model to be compiled in the program in config_leds_model.mk
include config_leds_model.mk

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c Makefile
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
