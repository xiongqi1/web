#CDCS_INCLUDE ?= /home/yong/src/products/build-Platypus-int_3g39w-trunk/Platypus_src/RT_sdk/staging_l/include
#CDCS_LIB ?= /home/yong/src/products/build-Platypus-int_3g39w-trunk/Platypus_src/RT_sdk/staging_l/lib
#CROSS_COMPILE ?= /opt/buildroot-gcc342/bin/mipsel-linux-uclibc-

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip
CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -I$(CDCS_INCLUDE) -DDEBUG
	STRIP  := echo
endif

#CFLAGS += -D CONFIG_DISABLE_SCHEDULE_GENERIC_QMI

# platform defines
ifeq ($(PLATFORM),Avian)
	CFLAGS += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
	CFLAGS += -D PLATFORM_BOVINE -DSKIN_$(V_SKIN)
else ifeq ($(PLATFORM),Arachnid)
	CFLAGS += -D PLATFORM_ARACHNID $(PLATFORM_CFLAGS)
else ifeq ($(PLATFORM),Platypus)
	CFLAGS += -D PLATFORM_PLATYPUS
endif

# platypus board and skin defines
ifeq ($(PLATFORM),Platypus)
	CFLAGS+=-D BOARD_$(V_BOARD) -DSKIN_$(V_SKIN)
endif

ifeq ($(V_QMI_PROTO),linux)
	CFLAGS += -D CONFIG_LINUX_QMI_DRIVER
endif

ifeq ($(V_MODULE),cinterion)
	CFLAGS += -D MODULE_cinterion
endif


# These modules require 10 seconds stabilization period at startup. Otherwise, the module occasionally becomes irresponsible
ifeq ($(V_MODULE),MC7304)
	CFLAGS += -D CONFIG_INIT_DELAY=10 -D CONFIG_IGNORE_INVALID_PROFILE_NUMBER
endif

ifeq ($(V_MODULE),MC7354)
	CFLAGS += -D CONFIG_INIT_DELAY=10 -D CONFIG_IGNORE_INVALID_PROFILE_NUMBER
endif


ifeq ($(V_MODULE_PRI_BASED_OPERATION),y)
	CFLAGS += -D MODULE_PRI_BASED_OPERATION
endif

ifneq ($(V_SUB_NETIF),none)
 ifneq ($(V_SUB_NETIF),)
	CFLAGS += -D SUB_NETIF
 endif
endif

ifeq ($(V_CUSTOM_FEATURE_PACK),$(filter $(V_CUSTOM_FEATURE_PACK), Santos Telstra_140 Telstra_145w Telstra_AU Telstra_6200 Select_Solutions))
	CFLAGS += -DV_TELSTRA_SPEC
endif

ifeq ($(V_CUSTOM_FEATURE_PACK),Santos)
	CFLAGS += -DV_TELSTRA_SPEC_Santos
endif

# libraries
SRCS := featurehash.c binqueue.c dbhash.c generictree.c linkedlist.c minilib.c port.c qmiservtran.c resourcetree.c funcschedule.c \
	main.c msgqueue.c qmimsg.c qmiuniclient.c strqueue.c qmi_cell_info.c bandsel.c svcver.c ezqmi_uim.c ezqmi_loc.c \
	ezqmi.c custom/custom.c

ifeq ($(V_QMI_VOICE),y)
	SRCS += ezqmi_voice.c qmirdbctrl.c rwpipe.c strarg.c indexed_rdb.c
endif

CFLAGS += -D V_SMS_QMI_MODE_$(V_SMS_QMI_MODE)
CFLAGS += -D MODULE_$(V_MODULE)
CFLAGS += -D QMI_VOICE_$(V_QMI_VOICE)
CFLAGS += -D V_HAS_AGPS_$(V_HAS_AGPS)
CFLAGS += -D V_MANUAL_ROAMING_$(V_MANUAL_ROAMING)
CFLAGS += -D V_MULTI_SELBAND_$(V_MULTI_SELBAND)
CFLAGS += -D V_PRODUCT_$(V_PRODUCT)

ifeq ($(V_QMI_PROTO),linux)
	SRCS += qmimux.c
endif

ifeq ($(V_SMS_QMI_MODE),y)
	SRCS += ezqmi_sms.c gsm.c
endif

PROJECT=qmimgr

#SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))
LIBS := -lgcc -lm $(CDCS_LIB)/rdb.a

ifeq ($(V_SMS_QMI_MODE),y)
	LIBS += -L $(CDCS_LIB) -liconv
endif

TEST_SUBDIR = test
DEPS = $(wildcard *.h) $(wildcard custom/_*.c) $(wildcard custom/*.h)

.PHONY: all install distclean clean

all: $(PROJECT)
	$(MAKE) -C $(TEST_SUBDIR) all

$(PROJECT): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
	$(STRIP) $@

install: all
	install -m 755 $(PROJECT) $(INSTALLDIR)/usr/bin/

distclean clean:
	rm -f $(PROJECT) *.o *~ custom/*.o custom/*~
	$(MAKE) -C $(TEST_SUBDIR) clean

$(OBJS): $(DEPS)
