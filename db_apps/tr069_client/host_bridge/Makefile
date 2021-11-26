#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_ALIBS     = CDCS static library path
# CDCS_SOLIBS    = CDCS dynamic library path
# MODE           = Build mode (for bovine: recovery, main)
# SBASE          = Source tree base directory

ifeq ($(PLATFORM),Platypus)
CONF_DIR=$(INSTALLDIR)/etc_ro
else
CONF_DIR=$(INSTALLDIR)/etc
endif

LUA_DIR=$(INSTALLDIR)/usr/lib/tr-069
VARIANT_DIR=src/platform/$(PLATFORM)/$(VARIANT)

.PHONY: all install uninstall clean distclean

all: install


install: $(VARIANT_DIR)/tr-069.conf $(VARIANT_DIR)/scripts $(VARIANT_DIR)/handlers
	mkdir -p $(CONF_DIR) $(LUA_DIR) $(LUA_DIR)/classes $(LUA_DIR)/handlers $(LUA_DIR)/scripts
	cp $(VARIANT_DIR)/tr-069.conf $(CONF_DIR)
	cp src/core/*.lua $(LUA_DIR)
	cp src/core/classes/*.lua $(LUA_DIR)/classes/
	cp src/core/handlers/*.lua $(LUA_DIR)/handlers/
	cp $(VARIANT_DIR)/core/*.lua $(LUA_DIR)
	cp $(VARIANT_DIR)/scripts/* $(LUA_DIR)/scripts/
	cp $(VARIANT_DIR)/handlers/*.lua $(LUA_DIR)/handlers/

uninstall:
	rm -rf $(CONF_DIR)/tr-069.conf
	rm -rf $(LUA_DIR)

clean:


distclean:

