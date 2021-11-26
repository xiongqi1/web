#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# CLI_DEBUG      = Run the program in CLI mode for debug purpose

# set compiler environment variables
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CXXFLAGS := -I. -Wall -pedantic -std=c++11 -Wextra -Os -fPIE -fstack-protector-strong
LDFLAGS := -lrdb -pie

ifneq ($(CLI_DEBUG),)
CFLAGS += -DCLI_DEBUG
CXXFLAGS += -DCLI_DEBUG
endif

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
CXXFLAGS += -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

PROJECT:= scomm_socket_server

OBJS:= file_buffer.o socket_server.o socket_message.o main.o

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
