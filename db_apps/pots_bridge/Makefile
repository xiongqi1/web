#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# MODE           = Build mode (for bovine: recovery, main)
# SBASE          = Source tree base directory

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../../romfs
KERNELINC     ?= $(shell readlink -f ../../../linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib
else ifeq ($(PLATFORM),Platypus2)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../staging
KERNELINC     ?= $(shell readlink -f ../../RT288x_SDK/source/linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
SBASE 		  ?= $(shell readlink -f ../../)
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := true
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

ifeq ($(PLATFORM),Avian)
	CFLAGS += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
	CFLAGS += -D PLATFORM_BOVINE
else ifeq ($(PLATFORM),Platypus)
	CFLAGS += -D PLATFORM_PLATYPUS
else ifeq ($(PLATFORM),Platypus2)
	CFLAGS += -D PLATFORM_PLATYPUS2
endif

CFLAGS+=-DBOARD_$(V_BOARD)=y -DSKIN_$(V_SKIN)=y 

ifeq ($(PLATFORM),Platypus)
	# platypus
	SLIC_SRC = slic_control/slic_control.c slic_control/tone_pattern.c
	LIBS := -lm $(FILE_DBLIB)
	TEST_LIBS := $(LIBS) $(CDCS_LIB)/unit_test.a -lpthread
	#LDFLAGS :=
	CFLAGS += -I$(KERNELINC)
	# slic cal data save/restore feature
	RTSDK = $(shell CURDIR=`pwd`; echo $${CURDIR%%/user/cdcs/pots_bridge})
	LIBNV_BIN = libnvram.a
	LIBNV_DIR = $(RTSDK)/lib/libnvram
	LIBNV_FNAME = $(LIBNV_DIR)/$(LIBNV_BIN)
	CFLAGS += -I$(LIBNV_DIR)
	LDFLAGS += -L$(LIBNV_DIR) -lnvram
	LIBS += $(LIBNV_FNAME)
else ifeq ($(PLATFORM),Platypus2)
	# platypus
	SLIC_SRC = slic_control/slic_control.c slic_control/tone_pattern.c
	LIBS := -lm $(FILE_DBLIB)
	TEST_LIBS := $(LIBS) $(CDCS_LIB)/unit_test.a -lpthread
	#LDFLAGS :=
	CFLAGS += -I$(KERNELINC)
else
	# bovine / Avian
	ifeq ($(MODE),recovery)
		MODE_SUFFIX:=_r
	else
		MODE_SUFFIX:=
	endif

	SLIC_SRC = $(wildcard slic_control/*.c)
	LIBS := -lm -lasound $(FILE_DBLIB)
	TEST_LIBS := $(LIBS) $(CDCS_LIB)/unit_test.a -lpthread
ifeq ($(V_BOARD),maisy)
	CFLAGS += -I$(CDCS_INCLUDE)
else	
	CFLAGS += -I$(CDCS_INCLUDE) -DUSE_ALSA
endif	
	LDFLAGS := -L$(CDCS_LIB)
endif

ifeq ($(V_SLIC),si322x)
	CFLAGS += -DPROSLIC_SI322X -DMULTI_CHANNEL_SLIC -DTWO_CHANNEL_LOOPBACK
else ifeq ($(V_SLIC),si3217x)

ifeq ($(PLATFORM),Bovine)
	CFLAGS += -DPROSLIC_SI3217X
else
	CFLAGS += -DPROSLIC_SI3217X -DONE_CHANNEL_LOOPBACK
endif
	
endif

ifeq ($(V_VOIP),asterisk)
	CFLAGS += -DHAS_VOIP_FEATURE
endif

ifeq ($(V_AUTODIAL),y)
	CFLAGS += -DAUTOMATED_EMERGENCY_DIALING
ifeq ($(PLATFORM),Platypus)
	AD_TGT = $(INSTALLDIR)/etc_ro/www/adm/autodial.asp
else
	AD_TGT = $(INSTALLDIR)/www/autodial.html
endif
endif

ifeq ($(V_STUTTER_DIAL_TONE),y)
	CFLAGS += -DHAS_STUTTER_DIAL_TONE
endif

ifeq ($(V_USE_PREDEF_VMBOX_NO),y)
	CFLAGS += -DUSE_PREDEF_VMBOX_NO
endif

$(info ******************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info slic=$(V_SLIC))
$(info voip=$(V_VOIP))
$(info autodial=$(V_AUTODIAL))
$(info stutter_dialtone=$(V_STUTTER_DIAL_TONE))
$(info predef_vmbox_no=$(V_USE_PREDEF_VMBOX_NO))
$(info ******************************************************************)

SRC := pots_bridge.c rdb_operations.c daemon.c telephony_profile.c fsk/packet.c $(wildcard calls/*.c) $(SLIC_SRC)
OBJ := $(patsubst %.c,%.o,$(SRC))
SLIC_TEST_SRC := rdb_operations.c fsk/packet.c $(wildcard calls/*.c) $(SLIC_SRC)
SLIC_TEST_OBJ := $(patsubst %.c,%.o,slic_test.c $(SLIC_TEST_SRC))
#TEST_SRC := rdb_operations.c fsk/packet.c $(wildcard calls/*.c) $(SLIC_SRC) $(wildcard test/*.c)
TEST_SRC := rdb_operations.c fsk/packet.c $(wildcard calls/*.c) $(SLIC_SRC) test/test.c
TEST_OBJ := $(patsubst %.c,%.o,$(TEST_SRC))
FXS_TEST_SRC := rdb_operations.c fsk/packet.c $(wildcard calls/*.c) $(SLIC_SRC)
FXS_TEST_OBJ := $(patsubst %.c,%.o,fxs_test.c $(FXS_TEST_SRC))

ifneq ($(PLATFORM),Avian)
SRC += calibration.c
OBJ += calibration.o
SLIC_TEST_SRC += calibration.c
SLIC_TEST_OBJ += calibration.o
FXS_TEST_SRC += calibration.c
FXS_TEST_OBJ += calibration.o
endif

ifeq ($(V_SLIC),none)
all install romfs:
	# No SLIC on this variant
else

.PHONY: all
all : pots_bridge slic_test unit_test fxs_test

.PHONY: install
install: all
	$(STRIP) pots_bridge
	$(STRIP) slic_test
	$(STRIP) fxs_test
	install -m 755 pots_bridge $(INSTALLDIR)/usr/bin/
	install -m 755 slic_test $(INSTALLDIR)/usr/bin/
	install -m 755 fxs_test $(INSTALLDIR)/usr/bin/
	install -m 755 scripts/pots_reload.sh $(INSTALLDIR)/usr/bin/
	install -m 755 scripts/pots_bridge.sh $(INSTALLDIR)/sbin/
	install -m 755 scripts/voicecallenv.sh $(INSTALLDIR)/sbin/
ifeq ($(PLATFORM),Platypus)
	mkdir -p $(INSTALLDIR)/etc_ro/cdcs/conf/mgr_templates
	install -m 755 templates/pots_bridge.template $(INSTALLDIR)/etc_ro/cdcs/conf/mgr_templates/
else ifeq ($(PLATFORM),Platypus2)
	mkdir -p $(INSTALLDIR)/etc/cdcs/conf/mgr_templates
	# in Platypus2, simple_at_manager directly lanuches pots_bridge to save boot-up time for the factory
	#install -m 755 templates/pots_bridge.template $(INSTALLDIR)/etc/cdcs/conf/mgr_templates/
	install -m 755 scripts/pots_init.sh $(INSTALLDIR)/sbin/
else
	mkdir -p $(INSTALLDIR)/etc/cdcs/conf/mgr_templates
	install -m 755 templates/pots_bridge.template $(INSTALLDIR)/etc/cdcs/conf/mgr_templates/
	install -m 755 scripts/pots_init.sh $(INSTALLDIR)/sbin/
endif
ifeq ($(V_AUTODIAL),y)
	mkdir -p $(INSTALLDIR)/www
ifeq ($(PLATFORM),Platypus)
	mkdir -p $(INSTALLDIR)/www/adm
endif
	gcc -E -x c -P -D PLATFORM_$(PLATFORM) -C html/autodial.html > $(AD_TGT)
endif
	$(shell echo "platform=$(PLATFORM)" > $(INSTALLDIR)/etc/platform.txt)

.PHONY: romfs
romfs: all
	$(ROMFSINST) /usr/bin/pots_bridge
	$(ROMFSINST) /usr/bin/fxs_test
	$(ROMFSINST) /usr/bin/slic_test
	$(ROMFSINST) scripts/pots_reload.sh /usr/bin/pots_reload.sh
	$(ROMFSINST) scripts/pots_bridge.sh /sbin/pots_bridge.sh
	$(ROMFSINST) scripts/voicecallenv.sh /sbin/voicecallenv.sh
ifeq ($(PLATFORM),Platypus)
	$(ROMFSINST) templates/pots_bridge.template /etc_ro/cdcs/conf/mgr_templates/pots_bridge.template
else
	$(ROMFSINST) templates/pots_bridge.template /etc/cdcs/conf/mgr_templates/pots_bridge.template
	$(ROMFSINST) scripts/pots_init.sh /sbin/pots_init.sh
endif
ifeq ($(V_AUTODIAL),y)
	mkdir -p $(INSTALLDIR)/www
ifeq ($(PLATFORM),Platypus)
	mkdir -p $(INSTALLDIR)/www/adm
endif
	gcc -E -x c -P -D PLATFORM_$(PLATFORM) -C html/autodial.html > $(AD_TGT)
endif
	$(shell echo "platform=$(PLATFORM)" > $(INSTALLDIR)/etc/platform.txt)
ifeq ($(PLATFORM),Platypus)
	$(shell echo "platform=$(PLATFORM)" > $(INSTALLDIR)/etc_ro/platform.txt)
endif

endif

slic_test: $(SLIC_TEST_OBJ)
	$(CC) $(LDFLAGS) $(SLIC_TEST_OBJ) $(LIBS) -o $@
	$(STRIP) $@

pots_bridge: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
	$(STRIP) $@
	
unit_test: $(TEST_OBJ)
	$(CC) $(LDFLAGS) $(TEST_OBJ) $(TEST_LIBS) -o $@

fxs_test: $(FXS_TEST_OBJ)
	$(CC) $(LDFLAGS) $(FXS_TEST_OBJ) $(LIBS) -o $@
	$(STRIP) $@

.PHONY: clean distclean
clean distclean:
	rm -f *.o fsk/*.o test/*.o slic_control/*.o calls/*.o pots_bridge slic_test unit_test fxs_test $(OBJ) $(TEST_OBJ)
	rm -f ../pots_bridge.dep ../pots_bridge.done
