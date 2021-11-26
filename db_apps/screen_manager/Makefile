#
# Screen Manager (scrm) Makefile
#
# Source files are included into the scrm build based on the following rules:
#    - Core: All source files in src/core are always built.
#    - Plugin: All source files in src/V_SCREEN_MANAGER/$(V_SCREEN_MANAGER) are
#      always built. That is, only one of the subdirectories in
#      src/V_SCREEN_MANAGER is used for a particular variant.
#    - Features: All source files in each src/feature/<V_Variable> is built
#      if the <V_Variable> value is anything except 'none'.
#
# Expects the following variables to be available:
#
# CDCS_INCLUDE  = Where to install include files
# CDCS_LIB      = Where to install libraries
# CROSS_COMPILE = toolchain prefix
# PLATFORM      = Sim, Platypus, Bovine, Elephant

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar
STRIP := $(CROSS_COMPILE)strip

# Uncomment the next line to enable debugs and asserts
#DEBUG=1
#
ifdef DEBUG
CFLAGS += -DDEBUG
else
# disable asserts
CFLAGS += -DNDEBUG
endif

CFLAGS += -g -Os -Wall -W -Werror
CFLAGS += -Iinclude/ -I$(CDCS_INCLUDE)

LDFLAGS += -g -Os -pedantic -Wall -W -Werror -rdynamic
LDFLAGS += -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB) -lngt -lrdb -ldl

ifneq ($(V_WIFI),none)
CFLAGS+= -DV_WIFI
CFLAGS+= -DV_WIFI_$(V_WIFI)
endif

ifneq ($(V_WIFI_CLIENT),none)
CFLAGS+= -DV_WIFI_CLIENT
CFLAGS+= -DV_WIFI_CLIENT_$(V_WIFI_CLIENT)
endif

ifneq ($(V_POWER_SCREEN_UI),none)
ifneq ($(V_BATTERY),none)
CFLAGS+= -DV_BATTERY_$(V_BATTERY)
endif
endif

ifneq ($(V_LEDFUN),none)
ifeq ($(V_DISP_DAEMON),none)
	$(error screen_manager LED support requires dispd)
endif
endif

ifneq ($(V_MODULE),none)
CFLAGS+= -DV_MULTIPLE_WWAN_PROFILES_$(V_MULTIPLE_WWAN_PROFILES)
endif

PROJ := scrmd

SOURCEDIR := src
COREDIR := $(SOURCEDIR)/core
FEATUREDIR := $(SOURCEDIR)/feature

# Get the plugin directory. The plugin directory is taken from
# the value of V_SCREEN_MANAGER.
PLUGINDIR := $(wildcard $(SOURCEDIR)/V_SCREEN_MANAGER/$(V_SCREEN_MANAGER))

ifeq ($(PLUGINDIR),)
    $(error Screen Manager plugin source directory, \
	    $(SOURCEDIR)/V_SCREEN_MANAGER/$(V_SCREEN_MANAGER), does not exit. \
	    Please check that V_SCREEN_MANAGER is set correctly.)
endif

# Get all the V feature directories
V_DIRS := $(notdir $(wildcard $(FEATUREDIR)/V_*))

# Filter the V directories removing all the "none" ones
V_DIRS_ENABLED := $(foreach V, $(V_DIRS), \
	$(if $(filter none, $($V)), , $(FEATUREDIR)/$(V)))

# Build all the core, plugin and enabled V feature source files
SRCS := $(shell find $(COREDIR) -name '*.c')
SRCS += $(shell find $(PLUGINDIR) -name '*.c')
SRCS +=  $(foreach V_ENABLED, $(V_DIRS_ENABLED), \
	$(shell find $(V_ENABLED) -name '*.c'))

OBJS := $(patsubst %.c,%.o,$(SRCS))

PUBLIC_HDRS := include/scrm_ops.h

.PHONY: all
all: $(PROJ)

$(PROJ): $(OBJS)
	$(CC) -o $(@) $(OBJS) $(LDFLAGS)

# Dependency files
-include $(OBJS:.o=.d)

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	$(CC) -MM $(CFLAGS) $*.c > $*.d
	mv -f $*.d $*.d.tmp
	sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	rm $*.d.tmp

TEMPINSTALL := $(shell /bin/pwd)/install

.PHONY: clean, distclean
clean distclean:
	rm -f $(PROJ)
	rm -rf $(TEMPINSTALL)
	find . -name "*.o" | xargs rm -f
	find . -name "*.d" | xargs rm -f

install: $(PROJ)
	mkdir -p $(TEMPINSTALL)/usr/bin
	mkdir -p $(TEMPINSTALL)/usr/lib/screen_manager
	mkdir -p $(TEMPINSTALL)/include
	install -m 0755 $(PROJ) $(TEMPINSTALL)/usr/bin
	install -m 0644 $(PUBLIC_HDRS) $(TEMPINSTALL)/include
	scatter_install.sh -l -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -b
