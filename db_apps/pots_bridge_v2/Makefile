#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# NTC-40wv DC-DC converter settings
#SI3217X_C_BB := 1
#export SI3217X_C_BB

PROSLIC_SDK_DIR = ./sdk
SI3217X_C_LCQC3W := 1
SPIDEV := 1

export SI3217X_C_LCQC3W
export SPIDEV

# include proslic
include $(PROSLIC_SDK_DIR)/demo/proslic_api_options.mk \
	$(PROSLIC_SDK_DIR)/demo/proslic_api_core.mk


# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld

ifdef DEBUG
binary_suffix := _DBG
else
binary_suffix :=
endif

export CC
export LD

SDK_SRCS := $(PROSLIC_SRC_CORE) \
	$(PROSLIC_CHIPSET_SRC) \
	$(PROSLIC_CFG_SRC) \
	$(DAA_CFG_SRC) \
	$(PROSLIC_PATCH_SRC)

# SLIC source files
SRCS := pots_bridge.c \
	pbx_common.c \
	netcomm_proslic.c

# peripheral files
SRCS += fsm.c \
	call_prefix.c \
	call_track.c \
	callback_timer.c \
	utils.c \
	dialplan.c \
	softtonegen.c

# QMI IPC source files
SRCS += rwpipe.c \
	dbhash.c \
	strarg.c \
	indexed_rdb.c \
	qmirdbctrl.c \
	ezrdb.c \
	call_history.c \
	block_calls.c

# FXS test source
SRCS_TEST := slic_test.c \
	netcomm_proslic.c \
	pbx_common.c \
	utils.c

SRCS_PCM_KEEPER := pcm_keeper.c

CFLAGS := -Wall -Werror -I ./custom -I $(PROSLIC_SDK_DIR)/inc -I$(CDCS_INCLUDE) $(PROSLIC_CFLAGS)

PROSLIC_CONSTANTS_HDR = $(PROSLIC_CFG_SRC:.c=.h)
CFLAGS += -DFXS_CONSTANTS_HDR=\"$(PROSLIC_CONSTANTS_HDR)\"

# verify V variable
ifeq ($(V_SLIC_SPIDEV),)
	$(error V_SLIC_SPIDEV not defined)
endif

ifeq ($(V_SLIC_SPIDEV),none)
	$(error V_SLIC_SPIDEV must be configured)
endif

ifeq ($(V_SLIC_RESET_GPIO),)
	V_SLIC_RESET_GPIO_NONE := 1
endif

ifeq ($(V_SLIC_RESET_GPIO),none)
	V_SLIC_RESET_GPIO_NONE := 1
endif

# define SPIDEV and GPIO
DEF_SILABS_SPIDEV := -DSILABS_SPIDEV=\"/dev/$(V_SLIC_SPIDEV)\"
ifneq ($(V_SLIC_RESET_GPIO_NONE),1)
	DEF_LINUX_GPIO := -DLINUX_GPIO=\"/sys/class/gpio/gpio$(V_SLIC_RESET_GPIO)/value\"
endif
DEF_SILAB_CONFIG := $(DEF_SILABS_SPIDEV) $(DEF_LINUX_GPIO)
CFLAGS += $(DEF_SILAB_CONFIG)

ifdef DEBUG
	CFLAGS += -g -O0 -DDEBUG
	STRIP  := echo
else
	CFLAGS += -Werror -Os
	STRIP := $(CROSS_COMPILE)strip
endif

LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB) -lasound

LIBS := -lrdb -lm
OBJS := $(patsubst %.c,%.o,$(SRCS) $(SDK_SRCS))
DEPS := $(patsubst %.c,%.d,$(SRCS) $(SDK_SRCS))

OBJS_TEST := $(patsubst %.c,%.o,$(SRCS_TEST) $(SDK_SRCS))
DEPS_TEST := $(patsubst %.c,%.d,$(SRCS_TEST) $(SDK_SRCS))

OBJS_PCM_KEEPER := $(patsubst %.c,%.o,$(SRCS_PCM_KEEPER))
DEPS_PCM_KEEPER := $(patsubst %.c,%.d,$(SRCS_PCM_KEEPER))

PRJ = pots_bridge
PRJ_TEST := slic_test
PRJ_PCM_KEEPER := pcm_keeper

vpath %.c $(PROSLIC_SDK_DIR)/src
vpath %.c ./custom
vpath %.c ./sdk/patch_files


# directories
UPBX_BUILD_DIR := ./sdk/demo/uPbx/build
RDWR_BUILD_DIR := ./sdk/demo/rdwr/build
API_DEMO_BUILD_DIR := ./sdk/demo/api_demo/build

.PHONY: all

all: rdwr_build api_demo_build $(PRJ) $(PRJ_TEST) $(PRJ_PCM_KEEPER)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ -MD $<

$(PRJ): $(OBJS)
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

$(PRJ_TEST): $(OBJS_TEST)
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

$(PRJ_PCM_KEEPER): $(OBJS_PCM_KEEPER)
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

.PHONY: clean distclean

clean distclean:
	make clean -C $(RDWR_BUILD_DIR)
	make clean -C $(API_DEMO_BUILD_DIR)
	rm -f $(PRJ) $(PRJ_TEST) $(OBJS) $(OBJS_TEST) $(DEPS) $(DEPS_TEST) $(PRJ_PCM_KEEPER) $(OBJS_PCM_KEEPER) $(DEPS_PCM_KEEPER) *~ ~*


.PHONY: rdwr_build api_demo_build

rdwr_build:
	PROSLIC_CFLAGS='$(DEF_SILAB_CONFIG)' make -C $(RDWR_BUILD_DIR)

.PHONY: api_demo_build

api_demo_build:
	PROSLIC_CFLAGS='$(DEF_SILAB_CONFIG)' make -C $(API_DEMO_BUILD_DIR)

.PHONY: install

install: all
	install -m 0755 -d "$(INSTALLDIR)/usr/bin/"
	# proslic demo
	install -m 0755 "$(RDWR_BUILD_DIR)/rdwr$(binary_suffix)" "$(INSTALLDIR)/usr/bin/proslic_rdwr"
	install -m 0755 "$(API_DEMO_BUILD_DIR)/$(OUT_FILE_PREFIX)$(binary_suffix)" "$(INSTALLDIR)/usr/bin/proslic_demo"
	# pots bridge
	install -m 0755 "$(PRJ)" "$(INSTALLDIR)/usr/bin/"
	# test
	install -m 0755 "$(PRJ_TEST)" "$(INSTALLDIR)/usr/bin/"
	# pcm keeper
	install -m 0755 "$(PRJ_PCM_KEEPER)" "$(INSTALLDIR)/usr/bin/"
	# test script - place mo calls
	install -m 0755 "place_mo_calls.lua" "$(INSTALLDIR)/usr/bin/"


-include $(DEPS)

