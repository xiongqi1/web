#
# TR143 ping make file
#
#Lee Huang<leeh@netcomm.com.au>


TARGET  := tr143_http


#BUILD_OPT ?=-D_DEBUG
BUILD_OPT ?=-D_PLATFORM_$(PLATFORM)



CDCS_INCLUDE ?=../../staging_l/include

CDCS_LIB    ?= ../../staging_l/lib




# Our tools.
CROSS_COMPILE ?=

CC    := $(CROSS_COMPILE)gcc
LD    := $(CROSS_COMPILE)gcc
AS    := $(CROSS_COMPILE)as
CP    := $(CROSS_COMPILE)objcopy





LUA_HOME=../../gnu_apps/lua

LUA_INC:=-I$(LUA_HOME)/src

LUA_LIB:=-lm -llua -ldl -L$(CDCS_LIB) -rdynamic -lcurl -lssl -lcrypto -lz

LIBS :=  -lgcc -lrdb -lm $(LUA_LIB) $(CDCS_LIB)/cdcs_syslog_lib.a


# Commandline options for each tool.
#CFLAGS   := -Os -Wall -c -g
#CFLAGS   := -Wall -c -ggdb  $(LUA_INC) -I$(CDCS_INCLUDE) $(BUILD_OPT)
CFLAGS   := -Os -Wall -c  -ggdb   $(LUA_INC) -I$(CDCS_INCLUDE) $(BUILD_OPT)

#AFLAGS   := -ahls -mapcs-32
LDFLAGS  := -v

#CPFLAGS  := --output-target=ihex



vpath %.h  $(CDCS_INCLUDE)

# C source files.
CSOURCES := getopt.c main.c main_loop.c rdb_comms.c utils.c rdb_event.c luaCall.c \
			http.c comms.c

COBJS   := $(CSOURCES:.c=.o)


# C header files
HSOURCES := $(wildcard *.h)

# Libraries.
#LIBS     := -lc

.PHONY: all clean distclean install

#TEST_TARGET=http_test

all:  $(TARGET) $(TEST_TARGET)
	touch $<

%.o: %.c $(HSOURCES)
	$(CC)  -o $@ $< $(CFLAGS)


clean distclean:
	rm -f *.o  *.map *.lst core *~
	rm -f $(TARGET)


$(TARGET): $(COBJS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(COBJS) $(LIBS)


$(TEST_TARGET): http_test.o
	$(LD) $(LDFLAGS) -o http_test http_test.o $(LIBS)


install: all
	cp $(TARGET) $(INSTALLDIR)/usr/bin/
	cp tr143_ifup.lua $(INSTALLDIR)/usr/bin/
	cp tr143_ifdown.lua $(INSTALLDIR)/usr/bin/
	#cp tr143_mgr.lua $(INSTALLDIR)/usr/bin/

