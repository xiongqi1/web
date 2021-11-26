#
# Expects the following external variables to be set
#
# PLATFORM	 = Platform name
# PATH           = Must include a path to the correct toolchain
# CDCS_LIB	 = CDCS library path
# CDCS_INCLUDE	 = CDCS include files, including RDB, libs, etc.
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install


TARGET  := tcp2diag

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= $(shell readlink -f ../../../romfs)
CDCS_INCLUDE  ?= $(shell readlink -f ../../../staging_l/include)
CDCS_LIB      ?= $(shell readlink -f ../../../staging_l/lib)
else ifeq ($(PLATFORM),Platypus2)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../staging
KERNELINC     ?= $(shell readlink -f ../../RT288x_SDK/source/linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
else ifeq ($(PLATFORM),Avian)
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= $(shell readlink -f ../../staging/cdcs)
CDCS_INCLUDE  ?= $(shell readlink -f ../../staging_l/include)
CDCS_LIB      ?= $(shell readlink -f ../../staging_l/lib)
PREFIXDIR     ?= $(INSTALLDIR)
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= $(shell readlink -f ../../staging)
CDCS_INCLUDE  ?= $(shell readlink -f ../../staging_l/include)
CDCS_LIB      ?= $(shell readlink -f ../../staging_l/lib)
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
CSOURCES := main.c rdb_comms.c

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


