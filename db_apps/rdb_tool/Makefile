#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# MOCK_INCLUDE   = include path for the mock RDB head files
# MOCK_LIB       = library path for the mock RDB library

#
# TODO:
#	Build system will provide mock staging directories. When it does, the following lines need to be removed
#

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

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
	CROSS_COMPILE ?= mipsel-linux-
	INSTALLDIR    ?= ../../../romfs
	CDCS_INCLUDE  ?= ../../../staging_l/include
	CDCS_LIB      ?= ../../../staging_l/lib
	MOCK_INCLUDE  ?= ../../../cdcs_libs/mock_rdb_lib/mock_include
	MOCK_LIB      ?= ../../../cdcs_libs/mock_rdb_lib/mock_lib
else
	ifeq ($(PLATFORM),Host)
		CROSS_COMPILE :=
	else
		CROSS_COMPILE ?= arm-linux-
	endif

	INSTALLDIR    ?= ../../staging
	CDCS_INCLUDE  ?= ../../staging_l/include
	CDCS_LIB      ?= ../../staging_l/lib
	MOCK_INCLUDE  ?= ../../cdcs_libs/mock_rdb_lib/mock_include
	MOCK_LIB      ?= ../../cdcs_libs/mock_rdb_lib/mock_lib
endif



# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

MFILE_DBLIB := $(MOCK_LIB)/mock_rdb.a
FILE_DBLIB := $(CDCS_LIB)/rdb.a

CFLAGS := -c -Wall
ifdef CDCS_INCLUDE_OPTS
        CFLAGS += $(CDCS_INCLUDE_OPTS)
else
        CFLAGS += -I$(CDCS_INCLUDE)
endif

ifdef DEBUG
	CFLAGS += -g
	# This disables strip
	STRIP  := true
else
	CFLAGS += -Os
endif

ifeq ($(PLATFORM),Arachnid)
	CFLAGS += -D PLATFORM_ARACHNID $(PLATFORM_CFLAGS)
endif

ifeq ($(PLATFORM),Host)
	CFLAGS += -I$(MOCK_INCLUDE)
endif

# Some parts are installed in prefix /, others in /usr. Set
# PREFIXDIR to move the /usr stuff somewhere else.
PREFIXDIR ?= $(INSTALLDIR)/usr
TEST_DIR  ?= ./test

RDBTOOL = rdb_tool
RDBTOOL_SRC = rdb_tool.c
RDBTOOL_OBJ = $(patsubst %.c,%.o,$(RDBTOOL_SRC))

RDBTOOL_MTEST = ./test/rdb_tool
RDBTOOL_MTEST_SRC = rdb_tool.c
RDBTOOL_MTEST_OBJ = $(patsubst %.c,%.o,$(RDBTOOL_SRC))

RUNONTIME = runontime
RUNONTIME_SRC = runontime.c
RUNONTIME_OBJ = $(patsubst %.c,%.o,$(RUNONTIME_SRC))

RUNONTERM = runonterm
RUNONTERM_SRC = runonterm.c
RUNONTERM_OBJ = $(patsubst %.c,%.o,$(RUNONTERM_SRC))

REDIRECT = redirect
REDIRECT_SRC = redirect.c
REDIRECT_OBJ = $(patsubst %.c,%.o,$(REDIRECT_SRC))

.PHONY: all
all:	$(RDBTOOL) $(RUNONTERM) $(REDIRECT) $(RUNONTIME) 

$(REDIRECT): $(REDIRECT_OBJ)
	$(CC) $^ $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

$(RUNONTIME): $(RUNONTIME_OBJ)
	$(CC) $^ $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

$(RUNONTERM): $(RUNONTERM_OBJ)
	$(CC) $^ $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

$(RDBTOOL): $(RDBTOOL_OBJ)
	echo `env`
	$(CC) $^ $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@


.PHONY: test
test:	$(MFILE_DBLIB) $(RDBTOOL_MTEST)

$(MFILE_DBLIB):
	cd $(MOCK_LIB)/..; make install

$(RDBTOOL_MTEST): $(RDBTOOL_MTEST_OBJ)
	echo `env`
	$(CC) $^ $(LDFLAGS) $(MFILE_DBLIB) -o $@
	$(STRIP) $@
	mv *.o $(TEST_DIR)/
	cp $(RDBTOOL_MTEST) $(TEST_DIR)/rdb_get
	ln -sf rdb_get $(TEST_DIR)/rdb_set
	ln -sf rdb_get $(TEST_DIR)/rdb_del
	ln -sf rdb_get $(TEST_DIR)/rdb_wait
	ln -sf rdb_get $(TEST_DIR)/rdb_set_wait
	# install binaries to mock staging
	install -m 0755 -D $(RDBTOOL_MTEST) $(MOCK_STAGING)/rdb_tool
	ln -sf rdb_tool $(MOCK_STAGING)/rdb_get
	ln -sf rdb_tool $(MOCK_STAGING)/rdb_set
	ln -sf rdb_tool $(MOCK_STAGING)/rdb_del
	ln -sf rdb_tool $(MOCK_STAGING)/rdb_wait
	ln -sf rdb_tool $(MOCK_STAGING)/rdb_set_wait


.PHONY: clean distclean
clean distclean:
	rm -f *.o $(REDIRECT) $(RDBTOOL) $(RUNONTERM) $(RUNONTIME) $(RDBTOOL_MTEST) DD_ioctl.h *~ *.suo *.ncb *.vcproj.*.user $(TEST_DIR)/rdb_* $(TEST_DIR)/*.o
	rm -fr Debug Release
	chmod 644 *.[ch] *.sln *.vcproj

.PHONY: install
install: all
	cp $(RDBTOOL) $(PREFIXDIR)/bin/rdb_get
	ln -sf rdb_get $(PREFIXDIR)/bin/rdb_set
	ln -sf rdb_get $(PREFIXDIR)/bin/rdb_wait
	ln -sf rdb_get $(PREFIXDIR)/bin/rdb_del
	ln -sf rdb_get $(PREFIXDIR)/bin/rdb_set_wait
	cp $(RUNONTERM) $(PREFIXDIR)/bin/
	cp $(RUNONTIME) $(PREFIXDIR)/bin/
	cp $(REDIRECT) $(PREFIXDIR)/bin/
	cp pidof-runontime $(PREFIXDIR)/bin/


.PHONY: instest
instest: test
	cp $(RDBTOOL_MTEST) $(TEST_DIR)/rdb_get
	ln -sf rdb_get $(TEST_DIR)/rdb_set
	ln -sf rdb_get $(TEST_DIR)/rdb_del
	ln -sf rdb_get $(TEST_DIR)/rdb_wait
	ln -sf rdb_get $(TEST_DIR)/rdb_set_wait

.PHONY: romfs
romfs:
	$(ROMFSINST) $(RDBTOOL) /usr/bin/rdb_get
	$(ROMFSINST) -s /usr/bin/rdb_get /usr/bin/rdb_set
	$(ROMFSINST) -s /usr/bin/rdb_get /usr/bin/rdb_wait
	$(ROMFSINST) -s /usr/bin/rdb_get /usr/bin/rdb_del
	$(ROMFSINST) -s /usr/bin/rdb_get /usr/bin/rdb_wait_set
	$(ROMFSINST) /usr/bin/$(RUNONTERM)
	$(ROMFSINST) /usr/bin/$(RUNONTIME)
	$(ROMFSINST) /usr/bin/$(REDIRECT)
