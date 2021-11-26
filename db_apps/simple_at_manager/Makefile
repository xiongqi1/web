#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../../romfs
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib
else ifeq ($(PLATFORM),Platypus2)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../staging
KERNELINC     ?= $(shell readlink -f ../../RT288x_SDK/source/linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
else ifeq ($(PLATFORM),Avian)
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging/cdcs
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
PREFIXDIR 	  ?= $(INSTALLDIR)
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

#DEBUG=1

# add CDCS_LIB to the default lib path
LDFLAGS:=-L$(CDCS_LIB)

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := echo
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif



# Some parts are installed in prefix /, others in /usr. Set
# PREFIXDIR to move the /usr stuff somewhere else.
PREFIXDIR ?= $(INSTALLDIR)/usr

ifeq ($(PLATFORM),Avian)
	CFLAGS += -D PLATFORM_AVIAN
	# fixme: build system does not have SLIC variable - avian platform does not have SLIC
	V_SLIC := none
else ifeq ($(PLATFORM),Bovine)
	CFLAGS += -D PLATFORM_BOVINE
else ifeq ($(PLATFORM),Antelope)
	CFLAGS += -D PLATFORM_ANTELOPE
else ifeq ($(PLATFORM),Platypus)
	CFLAGS += -D PLATFORM_PLATYPUS
	CFLAGS += -D USE_SIM_AS_SMS_STORAGE
else ifeq ($(PLATFORM),Platypus2)
	CFLAGS += -D PLATFORM_PLATYPUS2
	CFLAGS += -D USE_SIM_AS_SMS_STORAGE
else ifeq ($(PLATFORM),Serpent)
	CFLAGS += -D PLATFORM_SERPENT
endif

PRODUCT := $(shell echo $(V_PRODUCT) | sed -e 's/.\+_\(.\+\)/\1/')

CFLAGS += -DV_PRODUCT_$(V_PRODUCT)
CFLAGS += -DPRODUCT_$(PRODUCT)

CFLAGS += -DV_MANUAL_ROAMING_$(V_MANUAL_ROAMING)
CFLAGS += -DV_CELL_NW_$(V_CELL_NW)
CFLAGS += -DV_MODULE_ERROR_RECOVERY_$(V_MODULE_ERROR_RECOVERY)
CFLAGS += -DV_PHS8P_NETWORK_PRIORITY_FIXUP_$(V_PHS8P_NETWORK_PRIORITY_FIXUP)
CFLAGS += -DV_CUSTOM_FEATURE_PACK_$(V_CUSTOM_FEATURE_PACK)
CFLAGS += -DV_FIXED_BAND_$(V_FIXED_BAND)
CFLAGS += -D FORCED_REGISTRATION
ifeq ($(V_MODULE_PRI_BASED_OPERATION),y)
	CFLAGS += -D MODULE_PRI_BASED_OPERATION
endif

#ifeq ($(V_MODULE),MC7304)
#	CFLAGS += -DENABLE_NETWORK_RAT_SEL
#else ifeq ($(V_MODULE),MC7354)
#	CFLAGS += -DENABLE_NETWORK_RAT_SEL
#endif

#ifeq ($(V_MODULE),MC7304)
#	CFLAGS += -DENABLE_NETWORK_RAT_SEL
#else ifeq ($(V_MODULE),MC7354)
#	CFLAGS += -DENABLE_NETWORK_RAT_SEL
#endif

ifeq ($(PLATFORM),Platypus)
	# platypus
	LIBS := -lm $(FILE_DBLIB)
	TEST_LIBS := $(LIBS) $(CDCS_LIB)/unit_test.a
	RTSDK = $(shell CURDIR=`pwd`; echo $${CURDIR%%/user/cdcs/simple_at_manager})
	LIBNV_BIN = libnvram.a
	LIBNV_DIR = $(RTSDK)/lib/libnvram
	LIBNV_FNAME = $(LIBNV_DIR)/$(LIBNV_BIN)
	#PLATYPUS_CC_OPT=-I $(LIBNV_DIR)
	# TODO: Neither V_BOARD nor V_SKIN are suitable variables for this
	CFLAGS+=-I $(LIBNV_DIR) -DBOARD_$(V_BOARD) -DSKIN_$(V_SKIN)
	#PLATYPUS_LD_OPT=$(LIBNV_FNAME)
	LIBS+=$(LIBNV_FNAME)
else
	# bovine / Avian
	LIBS := $(FILE_DBLIB)
	TEST_LIBS := $(LIBS) $(CDCS_LIB)/unit_test.a
	# TODO: Neither V_BOARD nor V_SKIN are suitable variables for this
	CFLAGS+= -DBOARD_$(V_BOARD) -DSKIN_$(V_SKIN)
endif

CFLAGS+=-Wno-deprecated-declarations

CFLAGS+=-DCELL_NW_$(V_CELL_NW)

ifeq ($(V_SLIC),$(filter $(V_SLIC),none fixme ''))
else
	CFLAGS+= -DHAS_VOICE_DEVICE
endif

# enable ussd support for all model in simple_at_manager
ifeq ($(V_USSD), y)
CFLAGS+= -DUSSD_SUPPORT
endif
CFLAGS+= -DMODULETYPE_$(V_MODULETYPE)
CFLAGS+= -DMODULE_$(V_MODULE)

ifeq ($(V_GPS),y)
	CFLAGS+= -DGPS
endif
ifeq ($(V_GPS_ON_AT),y)
	CFLAGS+= -DGPS_ON_AT

	# add curl to downlaod xtra.bin
	LDFLAGS+=-rdynamic -lcurl -lssl -lcrypto -lz
endif

ifeq ($(V_IOBOARD), nhd1w)
    CFLAGS+= -DMHS_ENABLED
endif

ifneq ($(V_TR069), none)
    CFLAGS+= -DV_TR069
endif

ifeq ($(V_GCF_NETREG_WORKAROUND), y)
    CFLAGS+= -DV_GCF_NETREG_WORKAROUND
endif

CFLAGS += -DV_BUILTIN_UART_$(V_BUILTIN_UART)

ifneq ($(V_PORT_OPEN_DELAY), none)
    CFLAGS+= -DV_PORT_OPEN_DELAY="$(V_PORT_OPEN_DELAY)"
endif

CFLAGS += -DV_SUB_NETIF_$(V_SUB_NETIF)

ifeq ($(V_SIMMGMT), y)
    CFLAGS+= -DV_SIMMGMT
endif

$(info ******************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info slic=$(V_SLIC))
$(info voip=$(V_VOIP))
$(info autodial=$(V_AUTODIAL))
$(info gps_on_at=$(V_GPS_ON_AT))
$(info builtin_uart=$(V_BUILTIN_UART))
$(info port_open_delay=$(V_PORT_OPEN_DELAY))
$(info ******************************************************************)


MAIN_APPLICATION := simple_at_manager

SRC := $(MAIN_APPLICATION).c dyna.c  daemon.c featurehash.c\
		at/at.c  at/queue.c \
		model/model.c \
		\
		models/model_4031.c models/model_cdma_alcatel.c models/model_cdma_default.c models/model_cdma_huawei.c  \
		models/model_CyfrowyPolsat.c models/model_default.c models/model_ericsson.c models/model_fusion.c models/model_ipw.c  \
		models/model_ipw_lte.c models/model_longsung.c models/model_mv200.c models/model_option.c models/model_sierra.c  \
		models/model_zte_k3805Z.c models/suppl.c models/tagrule.c models/model_huawei.c models/model_cinterion.c models/model_quanta.c \
		models/model_sequans.c models/model_quectel.c \
		util/parse_response.c util/sprintf_buffer.c util/at_util.c util/cfg_util.c util/rdb_util.c util/scheduled.c util/string_utils.c

# add vodafone manual roaming procedure

ifeq ($(V_MANUAL_ROAMING),vdfglobal)
SRC += plmn-sel/plmn_struc.c \
	plmn-sel/simaccess.c \
	plmn-sel/plmn.c \
	plmn-sel/netdevusage.c
endif

ifeq ($(PLATFORM),Platypus2)
	LIBS+=$(CDCS_LIB)/libcrypto.a
	SRC+=models/model_ac754s.c
endif

SMS_SRC := $(wildcard sms/*.c)
SRC += $(SMS_SRC)

OBJ := $(patsubst %.c,%.o,$(SRC))

TEST_SRC := $(wildcard test/*.c)
TEST_OBJ := $(patsubst %.c,%.o, at/queue.c $(TEST_SRC))

.PHONY: all
all : $(MAIN_APPLICATION) unit_test atcmd_direct

atcmd_direct: atcmd_direct.o
	$(CC) $(LDFLAGS) $^ -o $@

$(MAIN_APPLICATION): $(OBJ)
	@if [ -z "$(V_SLIC)" ]; then \
		echo "V_SLIC V variable is not inhertied or not defined for simple_at_manager"; \
		false; \
	fi
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
	$(STRIP) $@

unit_test: $(TEST_OBJ)
	$(CC) $(LDFLAGS) $(TEST_OBJ) $(TEST_LIBS) -o unit_test
	$(STRIP) unit_test

install: all
	$(STRIP) $(MAIN_APPLICATION) atcmd_direct
	rm -f $(PREFIXDIR)/bin/$(MAIN_APPLICATION)
	#cp $(MAIN_APPLICATION) $(MAIN_APPLICATION)2
	install -m 755 $(MAIN_APPLICATION) $(PREFIXDIR)/bin/
	#install -m 755 $(MAIN_APPLICATION) $(PREFIXDIR)/bin/$(MAIN_APPLICATION)2
	install -m 755 atcmd $(PREFIXDIR)/bin/
	install -m 755 atcmd_term.sh $(PREFIXDIR)/bin/
	install -m 755 atcmd_direct $(PREFIXDIR)/bin/
	if [ "$(V_MANUAL_ROAMING)" = "vdfglobal" ]; then \
		mkdir -p "$(INSTALLDIR)/etc/cdcs/conf/"; \
		cp ./plmn-sel/plmn-*.csv.factory "$(INSTALLDIR)/etc/cdcs/conf/"; \
	fi

romfs: all
	$(ROMFSINST) /usr/bin/$(MAIN_APPLICATION)
	#$(ROMFSINST) /usr/bin/$(MAIN_APPLICATION)2
	$(ROMFSINST) /usr/bin/atcmd
	$(ROMFSINST) /usr/bin/atcmd_term.sh

clean distclean:
	rm -f $(OBJ) $(MAIN_APPLICATION) unit_test $(OBJ) $(TEST_OBJ) plmn-sel/*.o
	rm -f ../$(MAIN_APPLICATION).dep ../$(MAIN_APPLICATION).done atcmd_direct
	#rm -f $(MAIN_APPLICATION)2
