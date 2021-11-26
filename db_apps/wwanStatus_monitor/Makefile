#
# Expects the following external variables to be set
#
# PLATFORM	 = Platform name
# PATH           = Must include a path to the correct toolchain
# CDCS_LIB	 = CDCS library path
# CDCS_INCLUDE	 = CDCS include files, including RDB, libs, etc.
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install


TARGET  := wwanStatus_monitor

ifeq ($(PLATFORM),Bovine)
CROSS_COMPILE ?= arm-linux-
CDCS_LIB ?= ../../../staging_l/lib
endif

CC    := $(CROSS_COMPILE)gcc
LD    := $(CROSS_COMPILE)gcc

LIBS := -lgcc -pthread -rdynamic
FILE_DBLIB := $(CDCS_LIB)/rdb.a

# WITH_DEBUG=TRUE

ifdef WITH_DEBUG
CFLAGS   := -Os -Wall -c -D_DEBUG -I$(CDCS_INCLUDE) -L$(CDCS_LIB)
else
CFLAGS   := -Os -Wall -c -I$(CDCS_INCLUDE) -L$(CDCS_LIB)
endif

LDFLAGS  := -v

vpath %.h  $(CDCS_INCLUDE)

# C source files.
CSOURCES := main.c main_loop.c 

COBJS   := $(CSOURCES:.c=.o)


# C header files
HSOURCES := $(wildcard *.h)

.PHONY: all clean distclean install


all:  $(TARGET)
	touch $<

%.o: %.c $(HSOURCES)
	$(CC)  -o $@ $< $(CFLAGS)


distclean:	clean

clean:
	rm -f *.o  *.map *.lst core *~
	rm -f $(TARGET)


$(TARGET): $(COBJS)
	$(LD) $(LDFLAGS)  -o $(TARGET)  $(COBJS) $(LIBS) $(FILE_DBLIB)


install: all
	cp $(TARGET) $(INSTALLDIR)/usr/bin/


