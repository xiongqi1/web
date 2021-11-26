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

CFLAGS := -I ./include
LDFLAGS :=

ifdef CDCS_INCLUDE_OPTS
CFLAGS += $(CDCS_INCLUDE_OPTS)
endif

ifdef CDCS_LIB_OPTS
LDFLAGS += $(CDCS_LIB_OPTS)
endif

# Debug
ifneq ($(DEBUG),1)
CFLAGS += -Os -Wall
else
CFLAGS += -g -O0 -Wall
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

LIBS := -ldl -lrdb -lestd -lpthread -lstdc++fs

project := ookla_mgr

srcpp := Main.cpp OoklaOutputType.cpp SpeedTest.cpp SpeedTestTypes.cpp

ifeq ($(V_SPEED_TEST_CONFIG),bellca)
srcpp += Calculation_bellca.cpp
else
srcpp += Calculation_default.cpp
endif

obj := $(patsubst %.c,%.o,$(src)) $(patsubst %.cpp,%.o,$(srcpp))
obj_dir := output
obj_platform := $(addprefix $(obj_dir)/,$(obj))

.PHONY: all test install clean distclean

all: $(obj_dir)/$(project)

test: all

$(obj_dir)/$(project): $(obj_platform)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(obj_dir)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(obj_dir)/%.o: %.cpp
	$(CXX) $(CFLAGS) -std=c++17 -Wno-psabi -c $< -o $@

$(obj_platform): | $(obj_dir)_create

$(obj_dir)_create:
	mkdir -p ./$(obj_dir)

install: all
	mkdir -p $(INSTALLDIR_BIN)
	# install project
	install -m 0755 ./$(obj_dir)/$(project) $(INSTALLDIR_BIN)/$(project)
	# strip binaries
	$(STRIP) $(INSTALLDIR_BIN)/$(project)

clean distclean:
	rm -f $(obj_dir)/$(project)
	rm -f output/* *.o *~
	rmdir output 2> /dev/null; true

#dependencies
$(obj_platform): $(wildcard *.h)
