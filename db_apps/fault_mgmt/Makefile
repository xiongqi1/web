#
# Expects the following external variables to be set
#
# PATH		   = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR	 = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB	   = CDCS library path

include $(HBASE)/variant.mk

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -Werror -DDEBUG=1
	export DEBUG
else
	CFLAGS := -c -Wall -Werror -Os
endif

CFLAGS+= -I$(CDCS_INCLUDE) -I./inc/
LDFLAGS := -fPIC -shared

# libfm
FAULT_MGMT_LIB := libfm.so
FAULT_MGMT_ARCHIVE := fm.a
MGMT_LIB_SRC := src/fault_mgmt.c \
				src/fm_supported_alarm.c \
				src/fm_rdb.c \
				src/fm_util.c \
				src/fm_event.c \
				src/fm_current_alarm.c
MGMT_LIB_OBJ := $(MGMT_LIB_SRC:.c=.o)

# fmctl
FAULT_MGMT_CTL_EXE := fmctl
FAULT_MGMT_CTL_SRC := ./src/fmctl/

# init script
INIT_SCRIPT := fault_mgmt.sh

# config files
CONFIG_SUPPORTED_ALARM := $(V_RPU)/supported_alarm.conf

# header files
FAULT_MGMT_HEADER := fault_mgmt.h


.PHONY: all $(FAULT_MGMT_ARCHIVE) $(FAULT_MGMT_CTL_EXE) install clean distclean test unit_tests

all : $(FAULT_MGMT_ARCHIVE) $(FAULT_MGMT_LIB) $(FAULT_MGMT_CTL_EXE) unit_tests

$(FAULT_MGMT_ARCHIVE): $(MGMT_LIB_OBJ)
	$(AR) rsv $@ $^

$(FAULT_MGMT_LIB): $(MGMT_LIB_OBJ)
	$(CC) $(LDFLAGS) $(MGMT_LIB_OBJ) $(FILE_DBLIB) -o $@


$(FAULT_MGMT_CTL_EXE): $(FAULT_MGMT_LIB)
	make -C $(FAULT_MGMT_CTL_SRC) \
			FAULT_MGMT_LIB=$(FAULT_MGMT_LIB) \
			FAULT_MGMT_CTL_EXE="`realpath .`/$(FAULT_MGMT_CTL_EXE)"


%.o: %.c
	$(CC) -fPIC $(CFLAGS) -c $< -o $@


install: all
	install -m 0755 $(FAULT_MGMT_LIB) "$(INSTALLDIR)/lib/$(FAULT_MGMT_LIB)"
	install -m 0755 $(FAULT_MGMT_CTL_EXE) "$(INSTALLDIR)/usr/bin/$(FAULT_MGMT_CTL_EXE)"
	install -m 0755 $(INIT_SCRIPT) "$(INSTALLDIR)/etc/init.d/$(INIT_SCRIPT)"
	ln -sf ../init.d/$(INIT_SCRIPT) "$(INSTALLDIR)/etc/rc.d/S45fault_mgmt"
	install -m 0644 $(CONFIG_SUPPORTED_ALARM) "$(INSTALLDIR)/etc/supported_alarm.conf"

	install -m 0755 $(FAULT_MGMT_ARCHIVE) "$(INSTALLDIR)_l/lib/$(FAULT_MGMT_ARCHIVE)"
	install -m 0755 $(FAULT_MGMT_LIB) "$(INSTALLDIR)_l/lib/$(FAULT_MGMT_LIB)"
	install -m 0644 ./inc/$(FAULT_MGMT_HEADER) "$(INSTALLDIR)_l/include/$(FAULT_MGMT_HEADER)"
ifndef DEBUG
	$(STRIP) "$(INSTALLDIR)/lib/$(FAULT_MGMT_LIB)"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(FAULT_MGMT_CTL_EXE)"
	$(STRIP) "$(INSTALLDIR)_l/lib/$(FAULT_MGMT_LIB)"
else
	install -m 0755 fm_rm_data "$(INSTALLDIR)/usr/bin/fm_rm_data"
	install -m 0755 fm_show_events "$(INSTALLDIR)/usr/bin/fm_show_events"
endif

clean distclean:
	rm -f $(FAULT_MGMT_LIB) $(FAULT_MGMT_ARCHIVE) $(MGMT_LIB_OBJ)
	rm -rf tests/build
	make -C $(FAULT_MGMT_CTL_SRC) clean \
			FAULT_MGMT_LIB=$(FAULT_MGMT_LIB) \
			FAULT_MGMT_CTL_EXE="`realpath .`/$(FAULT_MGMT_CTL_EXE)"

test:
	sshpass -p "admin" scp $(FAULT_MGMT_LIB) root@192.168.1.1:/lib/$(FAULT_MGMT_LIB)
	sshpass -p "admin" scp $(FAULT_MGMT_CTL_EXE) root@192.168.1.1:/usr/bin/$(FAULT_MGMT_CTL_EXE)
	sshpass -p "admin" scp $(CONFIG_SUPPORTED_ALARM) root@192.168.1.1:/etc/supported_alarm.conf
	sshpass -p "admin" scp $(INIT_SCRIPT) root@192.168.1.1:/etc/init.d/$(INIT_SCRIPT)
	sshpass -p "admin" ssh root@192.168.1.1 "chmod +x /usr/bin/$(FAULT_MGMT_CTL_EXE)"
ifdef DEBUG
	sshpass -p "admin" scp fm_rm_data root@192.168.1.1:/usr/bin/fm_rm_data
	sshpass -p "admin" scp fm_show_events root@192.168.1.1:/usr/bin/fm_show_events
	sshpass -p "admin" ssh root@192.168.1.1 "chmod +x /usr/bin/fm_rm_data; chmod +x /usr/bin/fm_show_events"
endif

unit_tests:
	hostincludes.sh enable 2>/dev/null || true
	cmake -E make_directory tests/build
	LDFLAGS= CFLAGS="$(CFLAGS) -D__386__" cmake -E chdir tests/build cmake ..
	+cmake -E chdir tests cmake --build build -- VERBOSE=1
	hostincludes.sh disable 2>/dev/null || true
