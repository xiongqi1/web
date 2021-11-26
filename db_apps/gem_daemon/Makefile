#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# GEM_API_LIB       = CDCS library path

CDCS_INCLUDE  ?= ../../staging_l/include

#
# by doing export LOCAL_COMPILE=Yes you can compile from the directory
# of the project
#
ifeq ($(PLATFORM),Host)
    CROSS_COMPILE :=
else ifeq ($(LOCAL_COMPILE),Yes)
    CROSS_COMPILE ?= /opt/cross/gcc-4.3.4-glibc-2.9/bin/arm-cdcs-linux-gnueabi-
else
    CROSS_COMPILE ?= arm-linux-
endif


# set compiler environment variables

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CDCS_LIB    ?= ../../staging_l/lib
MOCK_RDB_LIB :=../../cdcs_libs/mock_rdb_lib/mock_rdb.a
RDB_LIB :=$(CDCS_LIB)/rdb.a
DAEMON_LIB :=$(CDCS_LIB)/daemon_lib.a
#GEM_API_LIB :=../../cdcs_libs/gem_api/gem_api.a
GEM_API_LIB :=$(CDCS_LIB)/gem_api.a
LUA_INCPATH := /usr/include/lua5.1

ifeq ($(PLATFORM),Host)
LUA_LIB := lua5.1
else
LUA_LIB:=-lm -llua -ldl -L$(CDCS_LIB) -rdynamic
endif

CFLAGS := -Wall -Os -I$(CDCS_INCLUDE)

ifeq ($(PLATFORM),Host)
CFLAGS += -I/usr/include/lua5.1
CFLAGS += -D V_HOST
else
CFLAGS += -I../../gnu_apps/lua/src/src
endif

LDFLAGS += -L$(CDCS_LIB) -lcdcsbase64

PROJECT = gem_daemon
SOURCES = gem_exec_lua.c gem_main.c gem_exec_templates.c gem_trig_timer.c gem_exec_mgr.c \
    gem_stats_mgr.c gem_trig_inotify.c gem_trig_rdb.c gem_control.c #daemon.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(OBJECTS);
	rm -f *~ ~*;

ifeq ($(PLATFORM),Host)
$(PROJECT): $(OBJECTS) $(GEM_API_LIB) $(MOCK_RDB_LIB)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDFLAGS) $(GEM_API_LIB) $(MOCK_RDB_LIB) -l$(LUA_LIB) -o $@
	$(STRIP) $@
else
$(PROJECT): $(OBJECTS) $(GEM_API_LIB) $(RDB_LIB)
	$(CC) $(LDFLAGS) $(CFLAGS) $(OBJECTS) $(LDFLAGS) $(GEM_API_LIB) $(RDB_LIB) $(DAEMON_LIB) -lm -llua -ldl -L$(CDCS_LIB) -o $@
	$(STRIP) $@
endif

.PHONY: install
install: $(PROJECT)
	$(STRIP) $(PROJECT)
	install -m 755 $(PROJECT) $(INSTALLDIR)/usr/bin/$(PROJECT)
