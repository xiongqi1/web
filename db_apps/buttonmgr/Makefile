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

export CC
export LD

# source files
SRCS := buttonexec.c  buttonmgr.c  evdev.c  timer.c iomutex.c
SRCS += buttonaction_$(V_KEYBOARD_GPIO).c

CFLAGS := -Wall -I$(CDCS_INCLUDE)

ifdef DEBUG
	CFLAGS += -g -O0
	STRIP  := echo
else
	CFLAGS += -Werror -Os
	STRIP := $(CROSS_COMPILE)strip
endif

LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB)

LIBS := -lrdb
OBJS := $(patsubst %.c,%.o,$(SRCS))
DEPS := $(patsubst %.c,%.d,$(SRCS))

PRJ = buttonmgr

.PHONY: all

all: $(PRJ)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ -MD $<

$(PRJ): $(OBJS)
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

.PHONY: clean distclean

clean distclean:
	rm -f $(PRJ) $(OBJS) $(DEPS) *~ ~*


.PHONY: install

install: all
	install -m 0755 "$(PRJ)" "$(INSTALLDIR)/usr/bin/"

-include $(DEPS)
