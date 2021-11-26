#
# Ethernet OAM make file
#
#Lee Huang<leeh@netcomm.com.au>


TARGET  := ethoamd

#BUILD_OPT ?=-D_DEBUG
#BUILD_OPT ?=



CDCS_INCLUDE ?=../../staging_l/include

CDCS_LIB    ?= ../../staging_l/lib


ETHOAM_INCLUDE ?= ../../drivers/ethoam

TMSAPI := tmsAPI

# Our tools.
CROSS_COMPILE ?=

CC    := $(CROSS_COMPILE)gcc
LD    := $(CROSS_COMPILE)gcc
AS    := $(CROSS_COMPILE)as
CP    := $(CROSS_COMPILE)objcopy
STRIP := $(CROSS_COMPILE)strip


LUA_LIB:=-lm -llua -ldl -L$(CDCS_LIB) -rdynamic


LIBS := -lgcc -lpthread $(CDCS_LIB)/rdb.a $(LUA_LIB) $(CDCS_LIB)/cdcs_syslog_lib.a $(CDCS_LIB)/daemon_lib.a

EXTRA_FLAGS := -D_AVCID_MAC
#NBN requirement: The function to extract MAC from AVCID is not availible in Arachnid
ifeq ($(PLATFORM),Arachnid)
	EXTRA_FLAGS :=
endif

# Commandline options for each tool.
#CFLAGS   := -Os -Wall -c -g
#CFLAGS   := -Wall -c -ggdb  $(LUA_INC) -I$(CDCS_INCLUDE) $(BUILD_OPT)
CFLAGS   := -Wall -c -g -I$(CDCS_INCLUDE) -I$(ETHOAM_INCLUDE) \
			$(BUILD_OPT) -DETH_PACKAGE  -DNCI_ETH1AG  -DNCIXLATE -D__32BIT__\
			-DETH_CCM  -D_MULTI_MEP -DNCI_Y1731  $(EXTRA_FLAGS) \
			-D_PLATFORM_$(PLATFORM)

#CFLAGS   := -O2 -Wall -c  -I$(CDCS_INCLUDE) -I$(ETHOAM_INCLUDE) \
#			$(BUILD_OPT) -DETH_PACKAGE  -DNCI_ETH1AG  -DNCIXLATE -D__32BIT__\
#			-DETH_CCM -DNCI_Y1731 \
#			-D_PLATFORM_$(PLATFORM)

#CFLAGS   := -O2 -Wall -c  -I$(CDCS_INCLUDE) -I$(ETHOAM_INCLUDE) \
#			$(BUILD_OPT) -DETH_PACKAGE  -DNCI_ETH1AG

#AFLAGS   := -ahls -mapcs-32
LDFLAGS  := -v -g

#CPFLAGS  := --output-target=ihex



vpath %.h  $(CDCS_INCLUDE)

# C source files.
CSOURCES := getopt.c main.c  rdb_comms.c  utils.c session.c\
			tms.c main_loop.c tms_1agsetupendpoints.c \
			tmsLinkUser.c tmsXlateUser.c \
			rdb_lbm.c rdb_ltm.c rdb_rmp.c rdb_lmp.c rdb_ltr.c rdb_ccm.c \
			rdb_y1731_lmp.c rdb_y1731_mda.c rdb_y1731_pdu.c
#			rdbrpc.c

COBJS   := $(CSOURCES:.c=.o)



TMS_COBJS   := $(TMSAPI_CSOURCES:.c=.o)


# C header files
HSOURCES := $(wildcard *.h)

# Libraries.
#LIBS     := -lc

.PHONY: all clean distclean install


all:  $(TARGET)
	touch $<


%.o: %.c $(HSOURCES) tmsXlate.c
	$(CC)  -o $@ $< $(CFLAGS)

#$(TMSAPI)/%.o: $(TMSAPI)/%.c
#	$(CC)  -o $@ $< $(CFLAGS)


clean distclean:
	rm -f *.o  *.map *.lst core *~
	rm -f $(TARGET)
	(cd $(TMSAPI); rm -f *.o)

$(TARGET): $(COBJS) $(TMS_COBJS)
	$(LD) $(LDFLAGS)  -o $@  $^ $(LIBS)


install: all
	mkdir -p $(INSTALLDIR)/usr/bin/
	install -m 0755 $(TARGET)  $(INSTALLDIR)/usr/bin/$(TARGET)
	$(STRIP) $(INSTALLDIR)/usr/bin/$(TARGET)


