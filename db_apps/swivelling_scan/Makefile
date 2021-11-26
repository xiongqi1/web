#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

ifndef CDCS_INCLUDE
CDCS_INCLUDE := ../../staging_l/include
endif

ifndef CDCS_LIB
CDCS_LIB = ../../staging_l/lib
endif

# Set compiler environment variables
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -I ./include -std=c++17
LDFLAGS :=

ifdef CDCS_INCLUDE_OPTS
CFLAGS += $(CDCS_INCLUDE_OPTS)
endif

ifdef CDCS_LIB_OPTS
LDFLAGS += $(CDCS_LIB_OPTS)
endif

ifneq ($(DEBUG),1)
CFLAGS += -Os -Wall -Wno-psabi
else
CFLAGS += -g -O0 -Wall -Wno-psabi
endif

# DESTDIR is a variable prepended to each installed target file
# but still support NetComm Meta build creation
INSTALLDIR := $(if $(INSTALLDIR),$(INSTALLDIR),$(DESTDIR))

INSTALLDIR_BIN := $(INSTALLDIR)/usr/bin

# CDCS path
ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif
ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

LIBS := -lrdb -ldl -lestd -lpthread -lstdc++fs
LIBS += -lstm32_motor

project := swivelling_scan

srcpp := main.cpp elogger.cpp udp_client.cpp user_interface.cpp manager.cpp status.cpp configuration.cpp scan_controller.cpp
srcpp += usb_udc_check.cpp
srcpp += casa_scan_controller.cpp
srcpp += motor_driver_factory.cpp
srcpp += stm32_motor_driver.cpp

obj := $(patsubst %.cpp,%.o,$(srcpp))
obj_dir := obj
objs := $(addprefix $(obj_dir)/,$(obj))

.PHONY: all install clean distclean

all: $(obj_dir)/$(project)

DEPS := $(objs:.o=.d)

-include $(DEPS)

$(obj_dir)/$(project): $(objs)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(obj_dir)/%.o: %.cpp
	$(CXX) $(CFLAGS) -MM -MT $@ -MF $(patsubst %.o,%.d,$@) $<
	$(CXX) $(CFLAGS) -c $< -o $@

$(objs): | $(obj_dir)_create

$(obj_dir)_create:
	mkdir -p ./$(obj_dir)

install: all
	mkdir -p $(INSTALLDIR_BIN)
	# install project
	install -m 0755 ./$(obj_dir)/$(project) $(INSTALLDIR_BIN)/$(project)
	# strip binaries
	$(STRIP) $(INSTALLDIR_BIN)/$(project)

clean distclean:
	rm -rf $(obj_dir)

