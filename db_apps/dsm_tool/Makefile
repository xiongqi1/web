#
# A makefile for one dsm_tool (data stream manager tool)
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install

# set compiler environment variables
ifeq ($(PLATFORM),Host)
    CROSS_COMPILE :=
else ifeq ($(LOCAL_COMPILE),Yes)
    CROSS_COMPILE ?= /opt/cross/gcc-4.3.4-glibc-2.9/bin/arm-cdcs-linux-gnueabi-
endif

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

RDB_LIB :=$(CDCS_LIB)/rdb.a
MOCK_RDB_LIB :=../../cdcs_libs/mock_rdb_lib/mock_rdb.a
ifeq ($(PLATFORM),Host)
CFLAGS := -Wall -Os -I../../cdcs_libs/mock_rdb_lib
else
CFLAGS := -Wall -Os -I$(CDCS_INCLUDE)
endif
PROJECT = dsm_tool
SOURCES = dsm_tool.c dsm_validations.c dsm_generate_cmd.c dsm_print.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

.PHONY : all
all: $(PROJECT)

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(OBJECTS);
	rm -f *~ ~*;

dsm_tool.o : dsm_tool.h

ifeq ($(PLATFORM),Host)
$(PROJECT): $(OBJECTS) $(MOCK_RDB_LIB)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) $(MOCK_RDB_LIB) -lm -ldl -o $@
	$(STRIP) $@
else
$(PROJECT): $(OBJECTS) $(RDB_LIB)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) $(RDB_LIB) -lm -ldl -L$(CDCS_LIB) -o $@
	$(STRIP) $@
endif

ifeq ($(PLATFORM),Host)

else
.PHONY: install
install: $(PROJECT)
	$(STRIP) $(PROJECT)
	install -m 755 $(PROJECT) $(INSTALLDIR)/usr/bin/$(PROJECT)
endif
