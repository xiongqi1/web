#
# TR143 ping make file
#
#Lee Huang<leeh@netcomm.com.au>


TARGET  := tr143_ping

BUILD_OPT ?=-D_DEBUG -D_PLATFORM_$(PLATFORM)
#BUILD_OPT ?=-D_PLATFORM_$(PLATFORM) -D_KERNEL_UDPECHO_TIMESTAMP_


CDCS_INCLUDE ?=../../staging_l/include

CDCS_LIB    ?= ../../staging_l/lib




# Our tools.
CROSS_COMPILE ?=

CC    := $(CROSS_COMPILE)gcc
LD    := $(CROSS_COMPILE)gcc
AS    := $(CROSS_COMPILE)as
CP    := $(CROSS_COMPILE)objcopy
STRIP := $(CROSS_COMPILE)strip




LUA_HOME=../../gnu_apps/lua

LUA_INC:=-I$(LUA_HOME)/src

LUA_LIB:=-lm -llua -ldl -L$(CDCS_LIB) -rdynamic -pthread

LIBS := -lgcc $(CDCS_LIB)/rdb.a $(LUA_LIB) $(CDCS_LIB)/cdcs_syslog_lib.a


# Commandline options for each tool.
#CFLAGS   := -Os -Wall -c -g
#CFLAGS   := -Wall -c -ggdb  $(LUA_INC) -I$(CDCS_INCLUDE) $(BUILD_OPT)
CFLAGS   := -O2 -Wall -c  $(LUA_INC) -I$(CDCS_INCLUDE) $(BUILD_OPT)

#AFLAGS   := -ahls -mapcs-32
LDFLAGS  := -v

#CPFLAGS  := --output-target=ihex



vpath %.h  $(CDCS_INCLUDE)

# C source files.
CSOURCES := ping.c  getopt.c main.c main_loop.c rdb_comms.c utils.c rdb_event.c luaCall.c\
			comms.c

COBJS   := $(CSOURCES:.c=.o)


# C header files
HSOURCES := $(wildcard *.h)

# Libraries.
#LIBS     := -lc

.PHONY: all clean distclean install


all:  $(TARGET) udpserver
	touch $<

%.o: %.c $(HSOURCES)
	$(CC)  -o $@ $< $(CFLAGS)


clean distclean:
	rm -f *.o  *.map *.lst core *~
	rm -f $(TARGET)
	rm -f udpserver

$(TARGET): $(COBJS)
	$(LD) $(LDFLAGS)  -o $(TARGET)  $(COBJS) $(LIBS)



udpserver: udpserver.c
	$(CC) -O2 -Wall -o$@ $<



install: all
	cp $(TARGET) $(INSTALLDIR)/usr/bin/
	cp udpserver $(INSTALLDIR)/usr/bin/
	#cp tr143_ping_ifup.lua $(INSTALLDIR)/usr/bin/
	#cp tr143_ping_ifdown.lua $(INSTALLDIR)/usr/bin/


