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

#CFLAGS := -Os -Wall -pedantic -std=gnu99 -Werror
CFLAGS := -Os -Wall -pedantic -std=gnu99 -fPIE -fstack-protector-strong

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif

#CFLAGS += -DDEBUG
LDFLAGS += -pie

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

#LDFLAGS += -l json

LIBS := $(CDCS_LIB)/daemon_lib.a $(CDCS_LIB)/rdb.a $(CDCS_LIB)/libjson-c.a

PROJECT = rdb_bridge
SOURCES = rdb_bridge.c options.c rbtreehash-dlist.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

TEMPINSTALL := $(shell /bin/pwd)/install
TEMPIA := $(TEMPINSTALL)/usr/share/installation_assistant

.PHONY: install
install: $(PROJECT)
	mkdir -p $(TEMPINSTALL)/usr/bin
	install -m 0755 $(PROJECT) $(TEMPINSTALL)/usr/bin
	mkdir -p $(TEMPINSTALL)/etc
	cp rdb_bridge_$(V_RDB_BRIDGE).conf $(TEMPINSTALL)/etc/rdb_bridge.conf
	if [ -n "$(V_INSTALLATION_ASSISTANT_UI_MODEL_LIST)" -a "$(V_INSTALLATION_ASSISTANT_UI_MODEL_LIST)" != "none" ]; then \
		for i in $(V_INSTALLATION_ASSISTANT_UI_MODEL_LIST); do \
			echo "install lark config for $$i" ; \
			install -m 0755 -d $(TEMPIA)/$$i ; \
			cp rdb_bridge_lark_$$i.conf $(TEMPIA)/$$i/rdb_bridge.conf ; \
		done ; \
	fi
	scatter_install.sh -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D etc -D usr

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o
	rm -rf $(TEMPINSTALL)
