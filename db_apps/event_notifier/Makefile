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
PREFIXDIR     ?= $(INSTALLDIR)
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
endif

# compile tools
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

#DEBUG:=1

# compile and link flags
ifdef DEBUG
CFLAGS := -c -Wall -O0 -g -I$(CDCS_INCLUDE)
LFLAGS := -g
STRIP := echo
else
CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
LFLAGS :=
endif

CFLAGS+= -DV_MULTIPLE_LANWAN_UI_$(V_MULTIPLE_LANWAN_UI) -DV_WIFI_$(V_WIFI) -DV_WIFI_MBSSID_$(V_WIFI_MBSSID) -DV_IOMGR_$(V_IOMGR) -DV_SINGLE_NETWORK_$(V_SINGLE_NETWORK) -DV_HW_PUSH_BUTTON_SETTINGS_$(V_HW_PUSH_BUTTON_SETTINGS) -DV_IOBOARD_$(V_IOBOARD)
CFLAGS+= -DV_MODCOMMS_$(V_MODCOMMS) -DV_VRRP_$(V_VRRP)

ifneq ($(V_USB_OTG_BUS),none)
	CFLAGS+=-DV_USB_OTG_BUS_$(V_USB_OTG_BUS)
	CFLAGS+=-DUSB_OTG_BUS=$(V_USB_OTG_BUS)
endif

# set project
EMONIPRJ = event_monitor
ENOTIPRJ = event_notifier
ECLRPRJ  = event_clear

COMMONSRC = event_rdb_util.c event_util.c

EMONISRC := event_monitor.c $(COMMONSRC)
ENOTISRC := event_notifier.c $(COMMONSRC)
ECLRSRC  := event_clear.c $(COMMONSRC)

EMONIOBJ := $(patsubst %.c,%.o,$(EMONISRC))
ENOTIOBJ := $(patsubst %.c,%.o,$(ENOTISRC))
ECLROBJ  := $(patsubst %.c,%.o,$(ECLRSRC))
RDB_LIB := $(CDCS_LIB)/rdb.a
DAEMON_LIB :=$(CDCS_LIB)/daemon_lib.a

$(info ******************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info ******************************************************************)


.PHONY:	all install clean distclean

all: $(EMONIPRJ) $(ENOTIPRJ) $(ECLRPRJ)
	# done

install: all
	install -D -m 0755 $(EMONIPRJ) "$(INSTALLDIR)/usr/bin/$(EMONIPRJ)"
	install -D -m 0755 $(ENOTIPRJ) "$(INSTALLDIR)/usr/bin/$(ENOTIPRJ)"
	install -D -m 0755 $(ECLRPRJ) "$(INSTALLDIR)/usr/bin/$(ECLRPRJ)"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(EMONIPRJ)"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(ENOTIPRJ)"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(ECLRPRJ)"
	install -D -m 0755 elogger "$(INSTALLDIR)/usr/bin/elogger"
	install -D -m 0755 enotifier "$(INSTALLDIR)/usr/bin/enotifier"
	install -D -m 0755 ethdevno_monitor "$(INSTALLDIR)/usr/bin/ethdevno_monitor"
	install -D -m 0755 event_notifier.template "$(INSTALLDIR)/etc/cdcs/conf/mgr_templates/50_event_notifier.template"
	install -D -m 0755 webui_session_timeout.template "$(INSTALLDIR)/etc/cdcs/conf/mgr_templates/51_webui_session_timeout.template"
	install -D -m 0755 assoc_sta_info.lua "$(INSTALLDIR)/usr/bin/assoc_sta_info.lua"

clean distclean:
	rm -f $(EMONIOBJ) $(EMONIPRJ) $(ENOTIOBJ) $(ENOTIPRJ) $(ECLROBJ) $(ECLRPRJ)

# on 40wv and others using older glibc -lrt is required for clock_gettime()
# it doesn't seem to hurt on newer toolchains
$(EMONIPRJ): $(EMONIOBJ)
	$(CC) $(LFLAGS) $(EMONIOBJ) $(RDB_LIB) $(DAEMON_LIB) -lrt -o $@

$(ENOTIPRJ): $(ENOTIOBJ)
	$(CC) $(LFLAGS) $(ENOTIOBJ) $(RDB_LIB) $(DAEMON_LIB) -o $@

$(ECLRPRJ): $(ECLROBJ)
	$(CC) $(LFLAGS) $(ECLROBJ) $(RDB_LIB) $(DAEMON_LIB) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
