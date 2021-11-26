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
	CFLAGS := -g -O0 -Wall -fPIC
	STRIP  := true
else
	CFLAGS := -Os -Wall
endif

LDFLAGS:=-Wall -pedantic -lrdb -lm -lasound

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif

CFLAGS += -D DIALPLAN_$(V_DIALPLAN)

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

PROJECT:=chan_qmirdb.so
OBJS:=chan_qmirdb.o qmirdbctrl.o dbhash.o rwpipe.o strarg.o indexed_rdb.o

.PHONY: all unit_tests
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(LDFLAGS) -pthread -lrt -shared -o $@ $^

sound_playback: sound_playback.o
	$(CC) $(LDFLAGS) -o $@ $^

unit_tests:
	$(CC) -D CONFIG_UNIT_TEST -I$(CDCS_INCLUDE) -g -O0 -Wall -lpthread rwpipe.c -o unit_tests/rwpipe
	
.PHONY: install
install: $(PROJECT)
	install -D $(PROJECT) $(INSTALLDIR)/lib/asterisk/modules/
	$(STRIP) $(INSTALLDIR)/lib/asterisk/modules/$(PROJECT)

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) test_units *.o *~

#dependencies
$(OBJS): $(wildcard *.h)
