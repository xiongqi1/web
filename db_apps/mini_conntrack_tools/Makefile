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

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := echo
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

LIBS := -L$(CDCS_LIB) -lnetfilter_conntrack

MAIN_APPLICATION := flush_conntrack_cache

SRC := flush_conntrack_cache.c
#SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,%.o,$(SRC))

.PHONY: all
all : $(MAIN_APPLICATION)

$(MAIN_APPLICATION): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
	$(STRIP) $@

install: all
	install -m 0755 -d "$(INSTALLDIR)/bin"
	install -m 0755 $(MAIN_APPLICATION) "$(INSTALLDIR)/bin/$(MAIN_APPLICATION)"
	$(STRIP) "$(INSTALLDIR)/bin/$(MAIN_APPLICATION)"

clean distclean:
	rm -f $(MAIN_APPLICATION) $(OBJ) *~
