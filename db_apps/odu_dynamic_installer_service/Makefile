#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR        = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

CDCS_INCLUDE_OPTS ?= -I$(CDCS_INCLUDE)
CDCS_LIB_OPTS ?= -L$(CDCS_LIB)
LUA_DIR ?= /usr/share/lua/5.1

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -Wall -g -Os -fPIC -std=gnu99 -Werror
LDFLAGS := -Wall -pedantic -fPIC $(CDCS_LIB_OPTS) -llua -Werror
LDFLAGS += -lrdb -lcurl
LDFLAGS += -lsnextra

SUPPORT_LIB_TARGET := lua_odu_web_client_helper.so

.PHONY: all install clean distclean

all: $(SUPPORT_LIB_TARGET)

$(SUPPORT_LIB_TARGET): client_helper.o
	$(CC) $(LDFLAGS) -shared $^ -o $@

%.o: %.c Makefile
	$(CC) $(CFLAGS) $(CDCS_INCLUDE_OPTS) -c $< -o $@

install: $(SUPPORT_LIB_TARGET)
	install -m 755 -D odu_files_services.lua $(INSTALLDIR)/usr/bin/odu_files_services.lua
	mkdir -p $(INSTALLDIR)/$(LUA_DIR)
	mkdir -p $(INSTALLDIR)/usr/lib/lua/5.1
	cp $(SUPPORT_LIB_TARGET) $(INSTALLDIR)/usr/lib/lua/5.1
	$(STRIP) $(INSTALLDIR)/usr/lib/lua/5.1/$(SUPPORT_LIB_TARGET)
	cp -r odu_dynamic_installer $(INSTALLDIR)/$(LUA_DIR)/

clean distclean:
	rm -f *.o *.so
