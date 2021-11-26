#
# Expects the following external variables to be set
#
# PLATFORM	 = Platform name
# PATH           = Must include a path to the correct toolchain
# CDCS_LIB	 = CDCS library path
# CDCS_INCLUDE	 = CDCS include files, including RDB, libs, etc.
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install


TARGET  := psoc_loader

ifeq ($(PLATFORM),Bovine)
CROSS_COMPILE ?= arm-linux-
CDCS_LIB ?= ../../../staging_l/lib
endif

CC    := $(CROSS_COMPILE)g++
LD    := $(CROSS_COMPILE)g++

LIBS := -lgcc -pthread -rdynamic
FILE_DBLIB := $(CDCS_LIB)/libusb.a $(CDCS_LIB)/rdb.a $(CDCS_LIB)/cdcs_misc_lib.a

# WITH_DEBUG=TRUE

#ifdef WITH_DEBUG
#CFLAGS   := -Os -Wall -c -D_DEBUG -I$(CDCS_INCLUDE) -L$(CDCS_LIB)
#else
CFLAGS   := -Os -Wall -c -I$(CDCS_INCLUDE) -I../../drivers/ModComms -L$(CDCS_LIB)
#endif

LDFLAGS  := -v  -L$(CDCS_LIB)
#-Llibusb-1.0.so

vpath %.h  $(CDCS_INCLUDE)

# C source files.
CSOURCES := main.cpp cybtldr_api.cpp cybtldr_api2.cpp cybtldr_parse.cpp cybtldr_command.cpp

COBJS   := $(CSOURCES:.cpp=.o)


# C header files
HSOURCES := $(wildcard *.h)

.PHONY: all clean distclean install

all:  $(TARGET)
	touch $<

%.o: %.cpp $(HSOURCES)
	$(CC)  -o $@ $< $(CFLAGS)


distclean:	clean

clean:
	rm -f *.o  *.map *.lst core *~
	rm -f $(TARGET)


$(TARGET): $(COBJS)
	$(LD) $(LDFLAGS) -o $(TARGET)  $(COBJS) $(LIBS) $(FILE_DBLIB)


install: all
	cp $(TARGET) $(INSTALLDIR)/usr/bin/


