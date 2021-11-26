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

CFLAGS = -c -Wall -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/asn1c/ -I$(PROPRIETARY_INCLUDE) -I$(PROPRIETARY_INCLUDE)/qc
LDFLAGS = -L $(CDCS_LIB) -L $(PROPRIETARY_LIB)

ifdef DEBUG
	CFLAGS += -g -O0
	STRIP  := true
else
	CFLAGS += -Os
endif


CELLTRACKING_PLATFORM_O = celltracking_$(V_CELLTRACKING).o

CFLAGS += -D V_CELLTRACKING_$(V_CELLTRACKING)

.PHONY: all install clean

all:
	echo "do not call directly, use Netcomm build tool"

rdb.o: rdb.h

celltracking.o: rdb.h log.h celltracking.h

$(CELLTRACKING_PLATFORM_O): rdb.h log.h  celltracking.h

celltracking: $(CELLTRACKING_PLATFORM_O) celltracking.o rdb.o
	$(CC) $^ $(LDFLAGS) -lrdb -o $@
	$(STRIP) $@

install: celltracking
	install -m 0755 celltracking $(INSTALLDIR)/usr/bin/celltracking
	$(STRIP) $(INSTALLDIR)/usr/bin/celltracking
	# install startup scripts
	install -m 0755 init.d/rc.d/celltracking $(INSTALLDIR)/etc/init.d/rc.d/celltracking
	cp --no-dereference --preserve=links init.d/rc3.d/S920celltracking $(INSTALLDIR)/etc/init.d/rc3.d/

distclean clean:
	# remove targets
	rm -f celltracking *.o *~ init.d/*~
