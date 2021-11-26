#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR        = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

LUA_DIR ?= /usr/share/lua/5.1

.PHONY: all install clean distclean

all install:
	install -m 755 -D check_crl.sh $(INSTALLDIR)/bin/check_crl.sh
	install -m 644 -D check_crl_support.lua $(INSTALLDIR)/$(LUA_DIR)/check_crl_support.lua

clean distclean:
