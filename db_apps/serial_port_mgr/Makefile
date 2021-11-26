#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# CLI_DEBUG      = Run the program in CLI mode for debug purpose
# LOOP_TEST      = Loop message sent to layer 3 service back

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -Os -Wall -pedantic -std=gnu99 -Werror
CXXFLAGS := -I. -Wall -pedantic -std=c++11 -Wextra -Os
LDFLAGS:= -lrdb

ifneq ($(CLI_DEBUG),)
CFLAGS += -DCLI_DEBUG
CXXFLAGS += -DCLI_DEBUG
endif

ifneq ($(LOOP_TEST),)
CFLAGS += -DLOOP_TEST
CXXFLAGS += -DLOOP_TEST
endif


ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
CXXFLAGS += -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

PROJECT:=serial_port_mgr

OBJS:=main.o crc.o serial_process.o fifo_process.o

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

TEMPINSTALL := $(shell /bin/pwd)/install

.PHONY: install
install: $(PROJECT)
	mkdir -p $(TEMPINSTALL)/sbin
	install -m 0755 $(PROJECT) $(TEMPINSTALL)/sbin
	scatter_install.sh -l -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -b

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o
	rm -rf $(TEMPINSTALL)
