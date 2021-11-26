#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

#
# TODO:
#	Build system will provide mock staging directories. When it does, the following lines need to be removed
#

# do Host as a default platform
ifndef PLATFORM
	PLATFORM := Host
endif

ifndef CDCS_INCLUDE
CDCS_INCLUDE := ../../staging_l/include
endif

ifndef CDCS_LIB
CDCS_LIB = ../../staging_l/lib
endif

# add MOCK directories
ifndef MOCK_INCLUDE
MOCK_INCLUDE := ../../staging_test_l/include
endif
ifndef MOCK_LIB
MOCK_LIB := ../../staging_test_l/lib
endif
ifndef MOCK_STAGING
MOCK_STAGING := ../../staging_test
endif

# host compile
ifeq ($(PLATFORM),Host)
	CROSS_COMPILE :=
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -DV_PRODUCT_$(V_PRODUCT) -DV_PROCESSOR_$(V_PROCESSOR)
LDFLAGS :=

# debug
ifneq ($(DEBUG),1)
CFLAGS += -Os -Wall
else
CFLAGS += -g -O0 -Wall
endif

# mock path
ifeq ($(PLATFORM),Host)
	CFLAGS += -I$(MOCK_INCLUDE) -I$(CDCS_INCLUDE)
	LDFLAGS += -L$(MOCK_LIB)
	INSTALLDIR_BIN := $(MOCK_STAGING)
else
	# taget path
	INSTALLDIR_BIN := $(INSTALLDIR)/usr/bin
endif

# cdcs path
ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif
ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB) -L$(CDCS_LIB)/../usr/lib
endif

LIBS := -lrdb -lpthread -lsysrepo -lyang -lpcre -lev -lm -lredblack -lprotobuf-c $(CDCS_LIB)/cdcs_misc_lib.a

project := sysrepo_client

srcpp := sysrepo_client.cpp timer.cpp Sysrepo.cpp yangdefs.cpp

obj := $(patsubst %.c,%.o,$(src)) $(patsubst %.cpp,%.o,$(srcpp))
obj_dir := obj-$(PLATFORM)
obj_platform := $(addprefix $(obj_dir)/,$(obj))

.PHONY: all test install clean distclean

makeDefinitions.js: makeDefinitions.ts
	nodejs -v
	tsc -v
	npm install @types/node
	tsc

yangdefs.cpp: makeDefinitions.js
	nodejs makeDefinitions.js
	touch Sysrepo.cpp

all: yangdefs.cpp $(obj_dir)/$(project)

test: all

$(obj_dir)/$(project): $(obj_platform)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(obj_dir)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(obj_dir)/%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

$(obj_platform): | $(obj_dir)_create

$(obj_dir)_create:
	mkdir -p $(obj_dir)

install: all
	mkdir -p $(INSTALLDIR_BIN)
	# install the main
	install -m 0755 $(obj_dir)/$(project) $(INSTALLDIR_BIN)/$(project)
	# install additional binaries
	# strip binaries
	$(STRIP) $(INSTALLDIR_BIN)/$(project)
	cp *.yang $(INSTALLDIR)/etc/sysrepo/yang

clean distclean:
	# delete test outputs
	rm -f obj-*/* *.o *~
	rmdir obj-* 2> /dev/null; true

#dependencies
$(obj_platform): $(wildcard *.h)
