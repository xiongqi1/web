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

CFLAGS := -c -Wall -Werror

ifdef DEBUG
	CFLAGS += -g -O0 -DMDEBUG
	STRIP  := echo
else
	CFLAGS += -Os
	STRIP := $(CROSS_COMPILE)strip
endif

ifdef CDCS_INCLUDE_OPTS
	CFLAGS += $(CDCS_INCLUDE_OPTS)
else
	CFLAGS += -I$(CDCS_INCLUDE)
endif

ifeq ($(PLATFORM),Cassini)
	CFLAGS += -DLINK_POLICY_PREFIX=\"link.policy\" -DLINK_POLICY_IFACE=\"ifdev\"
else
	CFLAGS += -DLINK_POLICY_PREFIX=\"link.profile\" -DLINK_POLICY_IFACE=\"interface\"
endif

ifdef CDCS_LIB_OPTS
LDFLAGS := $(CDCS_LIB_OPTS) -Wl,-rpath-link,$(INSTALLDIR)/usr/lib
else
LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB)
endif

LDLIBS := -lrdb $(CDCS_LIB)/daemon_lib.a
LDLIBS_PPING := -lrdb $(CDCS_LIB)/daemon_lib.a -lpcap

PROJECT = throughput_measure_daemon

SOURCES = link_profile_meas.c \
	  meas_bot.c \
	  proc_net_dev.c \
	  rdb.c \
	  meas_history.c \
	  throughput_measure_daemon.c \
	  tick_clock.c \
	  store_bot.c \
	  cellular_meas.c

DEPS = link_profile_meas.h \
       meas_bot.h \
       proc_net_dev.h \
       rdb.h \
       meas_history.h \
       tick_clock.h \
       store_bot.h \
       cellular_meas.h

OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

PROJECT_PPING = passive_rtt_measure

SOURCES_PPING = passive_rtt_measure.c \
		rdb.c

DEPS_PPING = uthash.h \
	     rdb.h

OBJECTS_PPING = $(patsubst %.c,%.o,$(SOURCES_PPING))

TEST_SUBDIR = tests

# common part
#
.PHONY: all

all: $(PROJECT) $(PROJECT_PPING)
	$(MAKE) -C $(TEST_SUBDIR) all

.PHONY: clean distclean

clean distclean:
	rm -f $(PROJECT) $(PROJECT_PPING) $(OBJECTS) *~ ~*
	$(MAKE) -C $(TEST_SUBDIR) clean

# project - throughput_measure_daemon

$(OBJECTS) : $(DEPS)

$(PROJECT): $(OBJECTS)
	$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

# project - passive_rtt_measure
#
$(OBJECTS_PPING) : $(DEPS_PPING)

$(PROJECT_PPING): $(OBJECTS_PPING)
	$(CC) $^ $(LDFLAGS) $(LDLIBS_PPING) -o $@

.PHONY: install

install: all
	install -m 0755 -d "$(INSTALLDIR)/usr/bin/"
	install -m 0755 "$(PROJECT)" "$(INSTALLDIR)/usr/bin/"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(PROJECT)"
	install -m 0755 "$(PROJECT_PPING)" "$(INSTALLDIR)/usr/bin/"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(PROJECT_PPING)"

