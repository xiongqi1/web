#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := echo
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

LIBS := -lm $(FILE_DBLIB)
CFLAGS+= -DGPS_$(V_GPS)

MAIN_APPLICATION := odometerd
SRC := odometer.c daemon.c logger.c
OBJ := $(patsubst %.c,%.o,$(SRC))

.PHONY: all
all : $(MAIN_APPLICATION)

$(MAIN_APPLICATION): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
	$(STRIP) $@

install: all
	install -m 0755 -d "$(INSTALLDIR)/sbin"
	install -m 0755 $(MAIN_APPLICATION) "$(INSTALLDIR)/sbin/$(MAIN_APPLICATION)"
	$(STRIP) "$(INSTALLDIR)/sbin/$(MAIN_APPLICATION)"

clean distclean:
	rm -f $(MAIN_APPLICATION) $(OBJ) *~
