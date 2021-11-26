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
CC := $(CROSS_COMPILE)gcc -std=gnu99
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS :=
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
INSTALLDIR_LIB := $(INSTALLDIR_BIN)/../lib/dm


# cdcs path
ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/omadmclient -I./lextras -I./logger
endif
ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB)
endif

LIBS := -lcurl \
	-ldmclient \
	-lwbxml2 \
	-llua \
	-lcrypto \
	-lssl \
	-lrdb \
	-lz

project := dm_client

src :=  log.c \
	lua_att.c \
	lua_class.c \
	lua_enum.c \
	lua_dm_path.c \
	lua_object.c \
	lua_rdb.c \
	lua_sched.c \
	lua_util.c \
	lua_wbxml.c \
	main.c \
	options.c \
	util.c \
	lextras/lextras.c \
	logger/logger.c \
	logger/logger-lua.c

obj := $(patsubst %.c,%.o,$(src))
obj_dir := obj-$(PLATFORM)
obj_platform := $(addprefix $(obj_dir)/,$(obj))

.PHONY: all test install clean distclean

all: $(obj_dir)/$(project)

test: all

$(obj_dir)/$(project): $(obj_platform)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(obj_dir)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(obj_platform): | $(obj_dir)_create

$(obj_dir)_create:
	mkdir -p $(obj_dir)/lextras
	mkdir -p $(obj_dir)/logger

install: all
	mkdir -p $(INSTALLDIR_BIN)
	# install the main
	install -m 0755 $(obj_dir)/$(project) $(INSTALLDIR_BIN)/$(project)
	# install additional modules
	mkdir -p $(INSTALLDIR_LIB)/modules
	cp ./modules/* $(INSTALLDIR_LIB)/modules
	mkdir -p $(INSTALLDIR_LIB)/objects
	cp ./objects/* $(INSTALLDIR_LIB)/objects
	# install template
	mkdir -p $(INSTALLDIR)/etc/cdcs/conf/mgr_templates
	cp ./dm_client.template $(INSTALLDIR)/etc/cdcs/conf/mgr_templates
	cp ./dm_client_default_apn.template $(INSTALLDIR)/etc/cdcs/conf/mgr_templates
	# strip binaries
	$(STRIP) $(INSTALLDIR_BIN)/$(project)

clean distclean:
	# delete test outputs
	rm -fr ./obj-*
	rm -f *.o *~

#dependencies
$(obj_platform): $(wildcard *.h)
