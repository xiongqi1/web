#
# Expects the following external variables to be set
#
# INSTALLDIR     = Root of staging tree for install
# V_*            = Variant specifiers

TEMPINSTALL := $(shell /bin/pwd)/install

# Directory for installing lua modules
LUA_MOD_DIR = $(TEMPINSTALL)/usr/share/lua/5.1/rds
ifeq ($(PLATFORM), Skunkape)
LUA_MOD_DIR = $(TEMPINSTALL)/usr/share/lua/rds
endif

HANDLERS_DIR = $(TEMPINSTALL)/usr/share/rds

VARIANTS = $(notdir $(wildcard variants/*)) # available variant dirs

.PHONY : all
all: main_files $(VARIANTS)

.PHONY : main_files
main_files:
	mkdir -p $(LUA_MOD_DIR)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin/
	install -m 755 rds.lua $(TEMPINSTALL)/usr/bin/
	install -m 0755 -d $(LUA_MOD_DIR)
	install -m 644 logging.lua util.lua $(LUA_MOD_DIR)

# Stage variant files only if their folder matches their V_variable and its value
# e.g. ./variants/V_SMS/y/
.PHONY : $(VARIANTS)
$(VARIANTS):
	mkdir -p $(HANDLERS_DIR)
	install -m 644 handlers.lua $(HANDLERS_DIR)/handlers.lua; \
	if [ -n "${$@}" ] && [ -d variants/$@/${$@} ]; then \
		install -m 644 variants/$@/${$@}/add/* $(LUA_MOD_DIR); \
		install -m 644 variants/$@/${$@}/handlers.lua $(HANDLERS_DIR)/$@handlers.lua; \
	fi

.PHONY : install
install: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr

.PHONY : clean
clean:
	rm -rf $(TEMPINSTALL)
