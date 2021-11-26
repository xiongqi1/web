#
# Copyright (C) 2016 NetComm Wireless limited.
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
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE) -I../custom -Werror -DUNIT_TEST

LDFLAGS := $(CDCS_LIB)/unit_test.a

TEST_PROJECT = qmi_unit_test
TEST_SOURCES = qmi_unit_test.c ../bandsel.c custom_test.c
TEST_OBJECTS = $(patsubst %.c,%.o,$(TEST_SOURCES))

DEPS = qmi_unit_test.h ../minilib.h ../qmidef.h ../bandsel.h ../custom/custom.h ../custom/_default.c

.PHONY : all
all: $(TEST_PROJECT)

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(TEST_PROJECT).*; \
	rm -f $(TEST_PROJECT) $(TEST_PROJECT).a $(TEST_OBJECTS); \
	rm -f *~ ~*;

$(TEST_OBJECTS) : $(DEPS)

$(TEST_PROJECT): $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LDFLAGS) -o $@
