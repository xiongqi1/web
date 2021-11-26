#
# A makefile for one shot timer utility
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install

# set compiler environment variables

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

RDB_LIB :=$(CDCS_LIB)/rdb.a
DAEMON_LIB :=$(CDCS_LIB)/daemon_lib.a
CFLAGS := -Wall -Os -I$(CDCS_INCLUDE)
PROJECT = one_shot_timer
SOURCES := $(shell ./detect_timerfd.sh $(CROSS_COMPILE)gcc)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(OBJECTS);
	rm -f *~ ~*;

$(PROJECT): $(OBJECTS) $(RDB_LIB) $(DAEMON_LIB)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) $(RDB_LIB) $(DAEMON_LIB) -lm -ldl -L$(CDCS_LIB) -o $@
	$(STRIP) $@

.PHONY: install
install: $(PROJECT)
	$(STRIP) $(PROJECT)
	install -m 755 $(PROJECT) $(INSTALLDIR)/usr/bin/$(PROJECT)
