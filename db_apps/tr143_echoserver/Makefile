#
# TR143 ping make file
#
#Lee Huang<leeh@netcomm.com.au>


TARGET  := tr143_echoserver


BUILD_OPT ?=



CDCS_INCLUDE ?=../../../staging_l/include
CDCS_LIB    ?= ../../../staging_l/lib
INSTALLDIR    ?= ../../../romfs
CDCS_SOLIBS   ?= $(shell readlink -f ../../../romfs/lib)

# Our tools.
CROSS_COMPILE ?= mipsel-linux-


CC    := $(CROSS_COMPILE)gcc
LD    := $(CROSS_COMPILE)gcc
AS    := $(CROSS_COMPILE)as
CP    := $(CROSS_COMPILE)objcopy
STRIP := $(CROSS_COMPILE)strip




LUA_HOME=../gnu_apps/lua

LUA_INC:=-I$(LUA_HOME)/src

LUA_LIB:=-lm -L$(CDCS_SOLIBS) -llua -ldl -L$(CDCS_LIB) -rdynamic

LIBS := -lgcc $(CDCS_LIB)/rdb.a $(LUA_LIB)

NVRAM_DIR := ../../../lib/libnvram
LIBS += $(NVRAM_DIR)/libnvram.a

# Commandline options for each tool.
#CFLAGS   := -Os -Wall -c -g
#CFLAGS   := -Wall -c -ggdb  $(LUA_INC) -I$(CDCS_INCLUDE) $(BUILD_OPT)

# WITH_DEBUG=TRUE

ifdef WITH_DEBUG
CFLAGS   := -Os -Wall -c -D_DEBUG $(LUA_INC) -I$(CDCS_INCLUDE) -I$(NVRAM_DIR) $(BUILD_OPT)
else
CFLAGS   := -Os -Wall -c  $(LUA_INC) -I$(CDCS_INCLUDE) -I$(NVRAM_DIR) $(BUILD_OPT)
endif

ifeq ($(PLATFORM),Platypus)
CFLAGS += -D PLATFORM_PLATYPUS
endif

#AFLAGS   := -ahls -mapcs-32
LDFLAGS  := -v

#CPFLAGS  := --output-target=ihex



vpath %.h  $(CDCS_INCLUDE)

# C source files.
CSOURCES := main.c main_loop.c rdb_comms.c rdb_event.c utils.c

COBJS   := $(CSOURCES:.c=.o)


# C header files
HSOURCES := $(wildcard *.h)

# Libraries.
#LIBS     := -lc

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
	$(LD) $(LDFLAGS)  -o $(TARGET)  $(COBJS) $(LIBS)


install: all
	cp $(TARGET) $(INSTALLDIR)/usr/bin/


