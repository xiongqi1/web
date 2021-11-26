#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_ALIBS     = CDCS static library path
# CDCS_SOLIBS    = CDCS dynamic library path
# MODE           = Build mode (for bovine: recovery, main)
# SBASE          = Source tree base directory

export PLATFORM

LUA_DIR=$(INSTALLDIR)/usr/lib/tr-069
CONF_DIR=$(INSTALLDIR)/etc

ifeq ($(PLATFORM),Platypus)
	CONF_DIR=$(INSTALLDIR)/etc_ro
endif

$(info **TR069****************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info slic=$(V_SLIC))
$(info voip=$(V_VOIP))
$(info autodial=$(V_AUTODIAL))
$(info ussd = $(V_USSD))
$(info sms = $(V_SMS))
$(info 3gfailovernoti = $(V_3GFAILOVERNOTIFICATION))
$(info wizard = $(V_WIZARD))
$(info callforwarding = $(V_CALL_FORWARDING))
$(info meplock = $(V_MEPLOCK))
$(info ipsec = $(V_IPSEC))
$(info eth_port = $(V_ETH_PORT))
$(info advanced_status = $(V_ADV_STATUS))
$(info ipwan = $(V_IPWAN))
$(info band = $(V_BAND))
$(info tr069 = $(V_TR069))
$(info vpn = $(V_VPN))
$(info dmz = $(V_DMZ))
$(info system_config = $(V_SYSCONFIG))
$(info webif_spec = $(V_WEBIF_SPEC))
$(info V_PRODUCT = $(V_PRODUCT))
$(info V_IDENTITY = $(V_IDENTITY))
$(info V_CLASS = $(V_CLASS))
$(info RELEASE = $(RELEASE))
$(info V_TR069_XVENDOR = $(V_TR069_XVENDOR))
$(info ******************************************************************)

MENUFLAG+=-DPLATFORM_$(PLATFORM) -DV_BOARD_$(V_BOARD) -DV_IOBOARD_$(V_IOBOARD) -DV_SKIN_$(V_SKIN) -DV_PRODUCT_$(V_PRODUCT) -DV_X_NETCOMM_WEBUI_$(V_X_NETCOMM_WEBUI)
MENUFLAG+=-D_V_PRODUCT_="$(V_PRODUCT)" -D_V_IDENTITY_="$(V_IDENTITY)" -D_V_CLASS_="$(V_CLASS)" -D_V_TR069_XVENDOR_=$(V_TR069_XVENDOR)

ifeq ($(V_SMS),y)
MENUFLAG+=-DV_SMS
endif

ifeq ($(V_GPS),y)
MENUFLAG+=-DV_GPS
endif

ifeq ($(V_ODOMETER),y)
MENUFLAG+=-DV_ODOMETER
endif

ifneq ($(V_WIFI),none)
MENUFLAG+=-DV_WIFI
CFLAGS+=-DV_WIFI
endif

ifneq ($(V_WIFI_CLIENT),none)
MENUFLAG+=-DV_WIFI_CLIENT
CFLAGS+=-DV_WIFI_CLIENT
endif

ifeq ($(V_WIFI_MBSSID),y)
MENUFLAG+=-DV_WIFI_MBSSID
CFLAGS+=-DV_WIFI_MBSSID
endif

ifeq ($(V_POWERSAVE),y)
MENUFLAG+=-DV_POWERSAVE_y
CFLAGS+=-DV_POWERSAVE_y
endif

ifneq ($(V_IPSEC),none)
MENUFLAG+=-DV_IPSEC
endif

ifneq ($(V_DIAL_ON_DEMAND),none)
MENUFLAG+=-DV_DIAL_ON_DEMAND
endif

ifeq ($(V_ENVIRONMENTAL_SENSOR),y)
MENUFLAG+=-DV_ENVIRONMENTAL_SENSOR
endif

.PHONY: all build install install_core install_platform uninstall clean distclean

all: install

build:


install: build install_core install_platform

install_core:
	mkdir -p $(LUA_DIR) $(LUA_DIR)/handlers
	cp src/*.lua $(LUA_DIR)
	chmod a+rx $(LUA_DIR)/cwmpd.lua $(LUA_DIR)/cwmp-xfrd.lua
	./install_classes.sh src/classes $(LUA_DIR)/classes
	cp src/handlers/*.lua $(LUA_DIR)/handlers

install_platform:
ifneq (,$(filter $(PLATFORM),Bovine Serpent Earwig Cassini Juno))
	./install_platform_Bovine.sh $(HBASE)/variant.sh ../variants $(INSTALLDIR)

	gcc -E -x c -P $(MENUFLAG) $(CONF_DIR)/tr-069.conf > $(CONF_DIR)/tr-069.conftmp
	mv -f $(CONF_DIR)/tr-069.conftmp $(CONF_DIR)/tr-069.conf
	mkdir -p $(LUA_DIR) $(LUA_DIR)/parmalist
	./install_parameterlist.sh $(HBASE)/variant.sh '$(RELEASE)' $(CONF_DIR)/tr-069.conf $(LUA_DIR)/parmalist/tr-069parameterlist.html
	./install_datamodelxml.sh $(HBASE)/variant.sh  $(CONF_DIR)/tr-069.conf $(LUA_DIR)/parmalist/tr069_sdm_1.xml
	cp $(LUA_DIR)/parmalist/tr-069parameterlist.html $(IDIR)/$(V_PRODUCT)_$(RELEASE)_tr069_sdm.html
	cp $(LUA_DIR)/parmalist/tr069_sdm_1.xml $(IDIR)/$(V_PRODUCT)_$(RELEASE)_tr069_sdm.xml
else
	./install_platform.sh $(HBASE)/variant.sh ../variants $(INSTALLDIR)
endif

uninstall:
	rm -rf $(CONF_DIR)/tr-069.conf
	rm -rf $(LUA_DIR)

clean:


distclean:

