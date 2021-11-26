#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# V_*            = Variant specifiers
#
# Copyright (C) 2018 NetComm Wireless Limited.

PLATFORM ?= Bovine
V_WEBIF_VERSION ?= v1
MODE ?= main

ifeq ($(PLATFORM),Platypus)
  CROSS_COMPILE ?= mipsel-linux-
  INSTALLDIR    ?= ../../../romfs
  CDCS_INCLUDE  ?= ../../../staging_l/include
  CDCS_LIB      ?= ../../../staging_l/lib
  STAGING_P     ?= ../../staging_p
else ifeq ($(PLATFORM),Platypus2)
  CROSS_COMPILE ?= mipsel-linux-
  INSTALLDIR    ?= ../../staging
  CDCS_INCLUDE  ?= ../../staging_l/include
  CDCS_LIB      ?= ../../staging_l/lib
  STAGING_P     ?= ../../staging_p
else # PLATFORM != Platypus[2]
  CROSS_COMPILE ?= arm-linux-
  INSTALLDIR    ?= ../../staging
  CDCS_INCLUDE  ?= ../../staging_l/include
  CDCS_LIB      ?= ../../staging_l/lib
  STAGING_P     ?= ../../staging_p
endif # PLATFORM != Platypus[2]

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := -lrdb -lcdcsbase64

ifndef CDCS_LIB_OPTS
	CDCS_LIB_OPTS:=-L$(CDCS_LIB)
endif
LDFLAGS := $(CDCS_LIB_OPTS)
ifndef CDCS_INCLUDE_OPTS
	CDCS_INCLUDE_OPTS:=-I$(CDCS_INCLUDE)
endif

ifdef DEBUG
  CFLAGS := -g -c -Wall $(CDCS_INCLUDE_OPTS)
  # This disables strip
  STRIP  := true
else # ! DEBUG
  CFLAGS := -c -Wall -Os $(CDCS_INCLUDE_OPTS)
endif # ! DEBUG

PRODUCT := $(shell echo $(V_PRODUCT) | sed -e 's/.\+_\(.\+\)/\1/')

CFLAGS += -DV_MANUAL_ROAMING_$(V_MANUAL_ROAMING) -DV_IPSEC_$(V_IPSEC) -DV_ETH_PORT_$(V_ETH_PORT) -DV_ADV_STATUS_$(V_ADV_STATUS) \
  -DV_MULTIPLE_WWAN_PROFILES_$(V_MULTIPLE_WWAN_PROFILES) -DV_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_$(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN) \
  -DMODE_$(MODE) -DV_CELL_NW_$(V_CELL_NW) \
  -DV_WEBIF_VERSION_$(V_WEBIF_VERSION) -DV_SMS_QMI_MODE_$(V_SMS_QMI_MODE) -DV_MODULE_$(V_MODULE) \
  -DV_FIREWALL_IP_PORT_BASIC_RDB_INTERPRETATION_$(V_FIREWALL_IP_PORT_BASIC_RDB_INTERPRETATION) -DV_WEBIF_SPEC_$(V_WEBIF_SPEC) -DV_ALIAS_IP_$(V_ALIAS_IP) \
  -DV_PARTITION_LAYOUT_$(V_PARTITION_LAYOUT) -DV_LOCK_IMS_SOS_PROFILE_$(V_LOCK_IMS_SOS_PROFILE) -DV_DDNS_WEBUI_$(V_DDNS_WEBUI) -DV_WEBUI_TWEAK_$(V_WEBUI_TWEAK) \
  -DV_BRIDGE_$(V_BRIDGE) -DPRODUCT_$(PRODUCT) \
  -DV_WATCHDOG_UI_$(V_WATCHDOG_UI) -DV_HELP_UI_$(V_HELP_UI) \
  -DV_CELL_INFO_WEBUI_$(V_CELL_INFO_WEBUI) \
  -DV_SYSTEM_CONFIG_UI_$(V_SYSTEM_CONFIG_UI) \
  -DV_WEBIF_SERVER_$(V_WEBIF_SERVER) \
  -DV_CONFIG_REMOTE_HTTP_FROM_WEBUI_$(V_CONFIG_REMOTE_HTTP_FROM_WEBUI) -DV_CONFIG_REMOTE_HTTPS_FROM_WEBUI_$(V_CONFIG_REMOTE_HTTPS_FROM_WEBUI) \
  -DV_CONFIG_REMOTE_PING_FROM_WEBUI_$(V_CONFIG_REMOTE_PING_FROM_WEBUI) \
  -DV_CONFIG_REMOTE_TELNET_FROM_WEBUI_$(V_CONFIG_REMOTE_TELNET_FROM_WEBUI) -DV_CONFIG_REMOTE_SSH_FROM_WEBUI_$(V_CONFIG_REMOTE_SSH_FROM_WEBUI) \
  -DV_CONFIG_LOCAL_HTTP_FROM_WEBUI_$(V_CONFIG_LOCAL_HTTP_FROM_WEBUI) -DV_CONFIG_LOCAL_HTTPS_FROM_WEBUI_$(V_CONFIG_LOCAL_HTTPS_FROM_WEBUI) \
  -DV_CONFIG_LOCAL_TELNET_FROM_WEBUI_$(V_CONFIG_LOCAL_TELNET_FROM_WEBUI) -DV_CONFIG_LOCAL_SSH_FROM_WEBUI_$(V_CONFIG_LOCAL_SSH_FROM_WEBUI) \
  -DV_CONFIG_TELNET_SSH_ACCOUNT_FROM_WEBUI_$(V_CONFIG_TELNET_SSH_ACCOUNT_FROM_WEBUI) \
  -DV_WIRELESS_WAN_WEBUI_$(V_WIRELESS_WAN_WEBUI) -DV_LAN_WEBUI_$(V_LAN_WEBUI) -DV_DHCP_RELAY_WEBUI_$(V_DHCP_RELAY_WEBUI) \
  -DV_BAND_SETTINGS_WEBUI_$(V_BAND_SETTINGS_WEBUI) -DV_PROFILE_ROUTING_WEBUI_$(V_PROFILE_ROUTING_WEBUI) \
  -DV_NTP_WEBUI_$(V_NTP_WEBUI) -DV_REMOTE_MGMT_WEBUI_$(V_REMOTE_MGMT_WEBUI) -DV_GPS_WEBUI_$(V_GPS_WEBUI) \
  -DV_CBRS_SAS_WEBUI_$(V_CBRS_SAS_WEBUI) \
  -DV_SYSTEM_CONFIG_WEBUI_$(V_SYSTEM_CONFIG_WEBUI) \
  -DV_SYSTEM_CONFIG_SETTINGS_WEBUI_$(V_SYSTEM_CONFIG_SETTINGS_WEBUI) -DV_SYSTEM_CONFIG_SETTINGS_BACKUP_RESTORE_WEBUI_$(V_SYSTEM_CONFIG_SETTINGS_BACKUP_RESTORE_WEBUI) \
  -DV_SYSTEM_CONFIG_UPLOAD_WEBUI_$(V_SYSTEM_CONFIG_UPLOAD_WEBUI) -DV_SYSTEM_CONFIG_PKG_MNGR_WEBUI_$(V_SYSTEM_CONFIG_PKG_MNGR_WEBUI) \
  -DV_SERVER_CERTIFICATE_WEBUI_$(V_SERVER_CERTIFICATE_WEBUI) \
  -DV_SSH_KEY_MGMT_WEBUI_$(V_SSH_KEY_MGMT_WEBUI) \
  -DV_FACTORY_RESET_TRIGGER_TYPE_$(V_FACTORY_RESET_TRIGGER_TYPE) \
  -DV_TRANSPARENT_BRIDGE_PPOE_WEBUI_$(V_TRANSPARENT_BRIDGE_PPOE_WEBUI) -DV_ROAMING_SETTINGS_WEBUI_$(V_ROAMING_SETTINGS_WEBUI)

MENUFLAG+= -DV_MANUAL_ROAMING_$(V_MANUAL_ROAMING)  -DV_SSH_$(V_SSH) -DV_IPSEC_$(V_IPSEC) -DV_ETH_PORT_$(V_ETH_PORT) \
  -DV_ADV_STATUS_$(V_ADV_STATUS) -DV_IPWAN_$(V_IPWAN) -DV_LEDPW_SAVE_$(V_LEDPW_SAVE) -DV_HW_PUSH_BUTTON_SETTINGS_$(V_HW_PUSH_BUTTON_SETTINGS) -DV_BAND_$(V_BAND) -DV_TR069_$(V_TR069) \
  -DV_VPN_$(V_VPN) -DV_DMZ_$(V_DMZ) -DV_SYSCONFIG_$(V_SYSCONFIG) -DV_SHOW_PPPOE_$(V_SHOW_PPPOE) \
  -DV_SHOW_ACTIVE_CONNECTION_$(V_SHOW_ACTIVE_CONNECTION) -DV_SHOW_DIAGNOSTIC_PORT_$(V_SHOW_DIAGNOSTIC_PORT) \
  -DV_WEBIF_SPEC_$(V_WEBIF_SPEC) -DV_WEBIF_DEVTOOLS_$(V_WEBIF_DEVTOOLS) \
  -DV_MULTIPLE_WWAN_PROFILES_$(V_MULTIPLE_WWAN_PROFILES) -DV_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_$(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN) \
  -DV_CELL_NW_$(V_CELL_NW) -DV_DNP3_$(V_DNP3) -DV_IOMGR_$(V_IOMGR) \
  -DV_DATA_STREAM_SWITCH_$(V_DATA_STREAM_SWITCH) -DV_WIFI_HOTSPOT_$(V_WIFI_HOTSPOT) -DV_PRODUCT_$(V_PRODUCT) -DPRODUCT_$(PRODUCT) \
  -DV_MODULE_$(V_MODULE) -DV_WAN_INTERFACE_$(V_WAN_INTERFACE) -DV_LANGUAGE_FR_$(V_LANGUAGE_FR) \
  -DV_LANGUAGE_AR_$(V_LANGUAGE_AR) -DV_LANGUAGE_DE_$(V_LANGUAGE_DE) -DV_LANGUAGE_JP_$(V_LANGUAGE_JP) \
  -DV_WIFI_$(V_WIFI) -DV_WIFI_HW_$(V_WIFI_HW) -DV_WIFI_CLIENT_$(V_WIFI_CLIENT) \
  -DV_IOBOARD_$(V_IOBOARD) -DV_OMA_DM_LWM2M_$(V_OMA_DM_LWM2M) -DV_SERIAL_IO_MODE_$(V_SERIAL_IO_MODE) \
  -DV_SMS_QMI_MODE_$(V_SMS_QMI_MODE) -DV_MODULE_PRI_BASED_OPERATION_$(V_MODULE_PRI_BASED_OPERATION) -DV_SKIN_$(V_SKIN) \
  -DV_WIFI_HOTSPOT_DEPRIORITISATION_$(V_WIFI_HOTSPOT_DEPRIORITISATION) \
  -DV_QOS_CBQ_INIT_$(V_QOS_CBQ_INIT) -DV_SKIN_$(V_SKIN) -DV_SFTPC_$(V_SFTPC) -DV_TCP_KEEPALIVE_$(V_TCP_KEEPALIVE) \
  -DV_FIREWALL_IP_PORT_BASIC_RDB_INTERPRETATION_$(V_FIREWALL_IP_PORT_BASIC_RDB_INTERPRETATION) \
  -DV_FIXED_BAND_$(V_FIXED_BAND) -DV_PER_PROFILE_MTU_$(V_PER_PROFILE_MTU) \
  -DPLATFORM_$(PLATFORM) -DV_SPEED_TEST_$(V_SPEED_TEST) -DV_ODOMETER_$(V_ODOMETER) -DV_ALIAS_IP_$(V_ALIAS_IP) \
  -DV_CHECK_PASSWORD_STRENGTH_$(V_CHECK_PASSWORD_STRENGTH) -DV_ENFORCE_PASSWORD_POLICY_$(V_ENFORCE_PASSWORD_POLICY) \
  -DV_WEB_LOGIN_LOCKING_$(V_WEB_LOGIN_LOCKING) -DV_WEB_SESSION_TIMEOUT_$(V_WEB_SESSION_TIMEOUT) \
  -DV_NON_SECURE_WARNING_$(V_NON_SECURE_WARNING) -DV_PARTITION_LAYOUT_$(V_PARTITION_LAYOUT) \
  -DV_LOCK_IMS_SOS_PROFILE_$(V_LOCK_IMS_SOS_PROFILE) -DV_DDNS_WEBUI_$(V_DDNS_WEBUI) -DV_WEBUI_TWEAK_$(V_WEBUI_TWEAK) \
  -DV_BRIDGE_$(V_BRIDGE) \
  -DV_NETWORKING_UI_$(V_NETWORKING_UI) -DV_SERVICES_UI_$(V_SERVICES_UI) -DV_ADMINISTRATION_UI_$(V_ADMINISTRATION_UI) \
  -DV_WATCHDOG_UI_$(V_WATCHDOG_UI) -DV_HELP_UI_$(V_HELP_UI) \
  -DV_WEBIF_SERVER_$(V_WEBIF_SERVER) -DV_AUTO_POWERDOWN_$(V_AUTO_POWERDOWN) \
  -DV_CELL_INFO_WEBUI_$(V_CELL_INFO_WEBUI) -DV_IPV6_$(V_IPV6) \
  -DV_SYSTEM_CONFIG_UI_$(V_SYSTEM_CONFIG_UI) \
  -DV_CONFIG_REMOTE_HTTP_FROM_WEBUI_$(V_CONFIG_REMOTE_HTTP_FROM_WEBUI) -DV_CONFIG_REMOTE_HTTPS_FROM_WEBUI_$(V_CONFIG_REMOTE_HTTPS_FROM_WEBUI) \
  -DV_CONFIG_REMOTE_PING_FROM_WEBUI_$(V_CONFIG_REMOTE_PING_FROM_WEBUI) \
  -DV_CONFIG_REMOTE_TELNET_FROM_WEBUI_$(V_CONFIG_REMOTE_TELNET_FROM_WEBUI) -DV_CONFIG_REMOTE_SSH_FROM_WEBUI_$(V_CONFIG_REMOTE_SSH_FROM_WEBUI) \
  -DV_CONFIG_LOCAL_HTTP_FROM_WEBUI_$(V_CONFIG_LOCAL_HTTP_FROM_WEBUI) -DV_CONFIG_LOCAL_HTTPS_FROM_WEBUI_$(V_CONFIG_LOCAL_HTTPS_FROM_WEBUI) \
  -DV_CONFIG_LOCAL_TELNET_FROM_WEBUI_$(V_CONFIG_LOCAL_TELNET_FROM_WEBUI) -DV_CONFIG_LOCAL_SSH_FROM_WEBUI_$(V_CONFIG_LOCAL_SSH_FROM_WEBUI) \
  -DV_CONFIG_TELNET_SSH_ACCOUNT_FROM_WEBUI_$(V_CONFIG_TELNET_SSH_ACCOUNT_FROM_WEBUI) \
  -DV_WIRELESS_WAN_WEBUI_$(V_WIRELESS_WAN_WEBUI) -DV_LAN_WEBUI_$(V_LAN_WEBUI) -DV_DHCP_RELAY_WEBUI_$(V_DHCP_RELAY_WEBUI) \
  -DV_BAND_SETTINGS_WEBUI_$(V_BAND_SETTINGS_WEBUI) -DV_PROFILE_ROUTING_WEBUI_$(V_PROFILE_ROUTING_WEBUI) \
  -DV_NTP_WEBUI_$(V_NTP_WEBUI) -DV_REMOTE_MGMT_WEBUI_$(V_REMOTE_MGMT_WEBUI) -DV_GPS_WEBUI_$(V_GPS_WEBUI) \
  -DV_IP_HANDOVER_$(V_IP_HANDOVER) -DV_CBRS_SAS_WEBUI_$(V_CBRS_SAS_WEBUI) \
  -DV_SYSTEM_CONFIG_WEBUI_$(V_SYSTEM_CONFIG_WEBUI) \
  -DV_SYSTEM_CONFIG_SETTINGS_WEBUI_$(V_SYSTEM_CONFIG_SETTINGS_WEBUI) -DV_SYSTEM_CONFIG_SETTINGS_BACKUP_RESTORE_WEBUI_$(V_SYSTEM_CONFIG_SETTINGS_BACKUP_RESTORE_WEBUI) \
  -DV_SYSTEM_CONFIG_UPLOAD_WEBUI_$(V_SYSTEM_CONFIG_UPLOAD_WEBUI) -DV_SYSTEM_CONFIG_PKG_MNGR_WEBUI_$(V_SYSTEM_CONFIG_PKG_MNGR_WEBUI) \
  -DV_SERVER_CERTIFICATE_WEBUI_$(V_SERVER_CERTIFICATE_WEBUI) \
  -DV_SSH_KEY_MGMT_WEBUI_$(V_SSH_KEY_MGMT_WEBUI) \
  -DV_FACTORY_RESET_TRIGGER_TYPE_$(V_FACTORY_RESET_TRIGGER_TYPE) \
  -DV_TRANSPARENT_BRIDGE_PPOE_WEBUI_$(V_TRANSPARENT_BRIDGE_PPOE_WEBUI) -DV_ROAMING_SETTINGS_WEBUI_$(V_ROAMING_SETTINGS_WEBUI)

# These files are #included and not required on target
REMOVE_LIST+= dsm_ser_and_modem_ep.inc end_point_types.inc dsm_serial_ep.inc

ifeq ($(PLATFORM),Avian)
  CFLAGS += -D PLATFORM_AVIAN
else # PLATFORM != Avian
  CFLAGS += -D PLATFORM_$(PLATFORM) -DBOARD_$(V_BOARD)=y
endif # PLATFORM != Avian

#ifeq ($(V_MODULE),MC7304)
#  MENUFLAG+= -DENABLE_NETWORK_RAT_SEL
#else ifeq ($(V_MODULE),MC7354)
#  MENUFLAG+= -DENABLE_NETWORK_RAT_SEL
#endif

# This is to remove the warning
# <command-line>:0:10: warning: missing whitespace after the macro name
# that comes from -DV_SERIAL_/dev/ttyAPP4
# Only V_SERIAL_none is significant
ifeq ($(V_SERIAL),none)
  MENUFLAG+= -DV_SERIAL_none
else # V_Serial != none
  MENUFLAG+= -DV_SERIAL_somePort
endif # V_Serial != none

ifeq ($(V_WEBIF_VERSION),v2)
  ifeq ($(MODE),recovery)
    WWWSRC := V_WEBIF_VERSION_v2/recovery
    MENUFLAG+=-DMODE_recovery
  else # MODE != recovery
    WWWSRC := V_WEBIF_VERSION_v2
  endif # MODE != recovery
else # V_WEBIF_VERSION != v2
  ifeq ($(PLATFORM),Bovine)
    ifeq ($(MODE),recovery)
      WWWSRC := V_WEBIF_VERSION_v1/bovine_rhtml
      MENUFLAG+=-DMODE_recovery
    else # MODE != recovery
      WWWSRC := V_WEBIF_VERSION_v1/bovine_html
    endif # MODE != recovery
  else ifeq ($(PLATFORM),Antelope)
    ifeq ($(MODE),recovery)
      WWWSRC := V_WEBIF_VERSION_v1/bovine_rhtml
    else # MODE != recovery
      WWWSRC := V_WEBIF_VERSION_v1/bovine_html
    endif # MODE != recovery
  else ifeq ($(PLATFORM),Avian)
    ifeq ($(MODE),recovery)
      WWWSRC := V_WEBIF_VERSION_v1/finch_rhtml
    else # MODE != recovery
      WWWSRC := V_WEBIF_VERSION_v1/finch_html
    endif # MODE != recovery
  else ifeq ($(PLATFORM),Platypus2)
    WWWSRC := V_WEBIF_VERSION_v1/platypus2_html
  else # PLATFORM != Platypus2
    $(error Don`t know how to handle platform: $(PLATFORM))
  endif # PLATFORM != Platypus2
endif # V_WEBIF_VERSION != v2

ifeq ($(V_SMS),y)
  MENUFLAG+=-DV_SMS
endif

ifeq ($(V_GPS),y)
  MENUFLAG+=-DV_GPS
endif

ifeq ($(V_ODOMETER),y)
  MENUFLAG+=-DV_ODOMETER
endif

ifeq ($(V_HAS_SGPS),y)
  MENUFLAG+=-DV_HAS_SGPS
endif

MENUFLAG+=-DV_HAS_AGPS_$(V_HAS_AGPS)

ifeq ($(V_AUTODIAL),y)
  MENUFLAG+=-DV_AUTODIAL
endif

ifeq ($(V_USSD),y)
  MENUFLAG+=-DV_USSD
endif

ifeq ($(V_3GFAILOVERNOTIFICATION),y)
  MENUFLAG+=-DV_3GFAILOVERNOTIFICATION
  CFLAGS+=-DV_3GFAILOVERNOTIFICATION
endif

ifeq ($(V_WIZARD),y)
  MENUFLAG+=-DV_WIZARD
endif

ifeq ($(V_CALL_FORWARDING),y)
  MENUFLAG+=-DV_CALL_FORWARDING
  CFLAGS+=-DV_CALL_FORWARDING
endif

ifeq ($(V_MEPLOCK),y)
  MENUFLAG+=-DV_MEPLOCK
  CFLAGS+=-DV_MEPLOCK
endif

ifneq ($(V_WIFI),none)
  ifneq ($(V_WIFI),fullmac)
    MENUFLAG+=-DV_WIFI
    CFLAGS+=-DV_WIFI
    CFLAGS+=-DV_WIFI_$(V_WIFI)
  endif # V_WIFI != fullmac
endif # V_WIFI != none

ifneq ($(V_WIFI_CLIENT),none)
  MENUFLAG+=-DV_WIFI_CLIENT
  CFLAGS+=-DV_WIFI_CLIENT
  CFLAGS+=-DV_WIFI_CLIENT_$(V_WIFI_CLIENT)
endif

ifneq ($(V_WIFI_FOLLOW_STA),none)
  MENUFLAG+=-DV_WIFI_FOLLOW_STA
  CFLAGS+=-DV_WIFI_FOLLOW_STA
  CFLAGS+=-DV_WIFI_FOLLOW_STA_$(V_WIFI_FOLLOW_STA)
endif

ifeq ($(V_WIFI_MBSSID),y)
  MENUFLAG+=-DV_WIFI_MBSSID
  CFLAGS+=-DV_WIFI_MBSSID
endif

ifeq ($(V_WIFI_WDS),y)
  MENUFLAG+=-DV_WIFI_WDS
  CFLAGS+=-DV_WIFI_WDS
endif

ifneq ($(V_POWERSAVE),none)
  # Suppress power management page for ntc_8000c here for expediency
  # Might revisit this in the future.
  ifeq ($(V_POWERSAVE_NOGUI),none)
    MENUFLAG+=-DV_POWERSAVE_y
    CFLAGS+=-DV_POWERSAVE_y
  endif # V_POWERSAVE_NOGUI == none
endif # V_POWERSAVE != none

ifeq ($(V_POWERSAVE),nguni)
  MENUFLAG+=-DV_POWERSAVE_nguni
  CFLAGS+=-DV_POWERSAVE_nguni
endif

ifeq ($(V_SIMMGMT),y)
  MENUFLAG+=-DV_SIMMGMT_y
  CFLAGS+=-DV_SIMMGMT_y
endif

ifeq ($(V_SIMMGMT),v2)
  MENUFLAG+=-DV_SIMMGMT_v2
  CFLAGS+=-DV_SIMMGMT_v2
endif

ifeq ($(V_MODCOMMS),y)
  MENUFLAG+=-DV_MODCOMMS_y
  CFLAGS+=-DV_MODCOMMS_y
  REMOVE_LIST+=ModComms.inc
endif

ifeq ($(V_SFTPC),y)
  MENUFLAG+=-DV_SFTPC_y
  CFLAGS+=-DV_SFTPC_y
endif

ifeq ($(V_TCP_KEEPALIVE),y)
  MENUFLAG+=-DV_TCP_KEEPALIVE_y
  CFLAGS+=-DV_TCP_KEEPALIVE_y
endif

ifneq ($(V_SLIC),none)
  MENUFLAG+=-DV_SLIC
  CFLAGS+=-DV_SLIC
endif

ifeq ($(V_SERIAL_HAS_FC),y)
  MENUFLAG+=-DV_SERIAL_HAS_FC_y
  CFLAGS+=-DV_SERIAL_HAS_FC_y
endif

ifeq ($(V_DIAL_ON_DEMAND),y)
  MENUFLAG+=-DV_DIAL_ON_DEMAND
  CFLAGS+=-DV_DIAL_ON_DEMAND
endif

ifeq ($(V_KEEP_SINGLE_TCP_CONN),y)
  MENUFLAG+=-DV_KEEP_SINGLE_TCP_CONN
  CFLAGS+=-DV_KEEP_SINGLE_TCP_CONN
endif

ifeq ($(V_MULTIPLE_LANWAN_UI),y)
  MENUFLAG+=-DV_MULTIPLE_LANWAN_UI
  CFLAGS+=-DV_MULTIPLE_LANWAN_UI
endif

MENUFLAG+=-DV_NAS_$(V_NAS)
CFLAGS+=-DV_NAS_$(V_NAS)

MENUFLAG+=-DV_SCEP_CLIENT_$(V_SCEP_CLIENT)
CFLAGS+=-DV_SCEP_CLIENT_$(V_SCEP_CLIENT)

ifneq ($(V_USB_OTG_MANUAL_MODE_SELECTION),none)
  MENUFLAG+=-DV_USB_OTG_MANUAL_MODE_SELECTION
  CFLAGS+=-DV_USB_OTG_MANUAL_MODE_SELECTION
endif

ifeq ($(V_EVENT_NOTIFICATION),y)
  MENUFLAG+=-DV_EVENT_NOTIFICATION
  CFLAGS+=-DV_EVENT_NOTIFICATION
  REMOVE_LIST+=event_notif.inc
endif

ifeq ($(V_EMAIL_CLIENT),y)
  MENUFLAG+=-DV_EMAIL_CLIENT
  CFLAGS+=-DV_EMAIL_CLIENT
endif

MENUFLAG+=-DV_POWER_SOURCES_$(V_POWER_SOURCES)

ifneq ($(V_ROUTER_TERMINATED_PPPOE),none)
  MENUFLAG+=-DV_ROUTER_TERMINATED_PPPOE
  CFLAGS+=-DV_ROUTER_TERMINATED_PPPOE
endif

ifneq ($(V_CUSTOM_FEATURE_PACK),none)
  MENUFLAG+=-DV_CUSTOM_FEATURE_PACK_$(V_CUSTOM_FEATURE_PACK)
  CFLAGS+=-DV_CUSTOM_FEATURE_PACK_$(V_CUSTOM_FEATURE_PACK)
endif

ifneq ($(V_SUB_NETIF),none)
  ifneq ($(V_SUB_NETIF),)
    MENUFLAG+=-DV_SUB_NETIF
    CFLAGS+=-DV_SUB_NETIF
  endif
endif

MENUFLAG+=-DV_MODULE_$(V_MODULE)

ifneq ($(V_BLUETOOTH),none)
  MENUFLAG+=-DV_BLUETOOTH
  CFLAGS+=-DV_BLUETOOTH
endif

ifeq ($(V_DATA_STREAM_SWITCH_HAS_CSD),y)
  MENUFLAG+=-DV_DATA_STREAM_SWITCH_HAS_CSD
  CFLAGS+=-DV_DATA_STREAM_SWITCH_HAS_CSD
endif

ifeq ($(V_SINGLE_NETWORK),y)
  MENUFLAG+=-DV_SINGLE_NETWORK
  CFLAGS+=-DV_SINGLE_NETWORK
endif

ifneq ($(V_NETSNMP),none)
  MENUFLAG+=-DV_NETSNMP
  CFLAGS+=-DV_NETSNMP
endif

ifneq ($(V_QOS_CBQ_INIT),none)
  MENUFLAG+=-DV_QOS_CBQ_INIT
  CFLAGS+=-DV_QOS_CBQ_INIT
  CFLAGS+=-DV_QOS_CBQ_INIT_$(V_QOS_CBQ_INIT)
endif

ifneq ($(V_VRRP),none)
  MENUFLAG+=-DV_VRRP
  CFLAGS+=-DV_VRRP
endif

ifneq ($(V_RIP),none)
  MENUFLAG+=-DV_RIP
  CFLAGS+=-DV_RIP
endif

ifneq ($(V_COPROCESSOR),none)
  MENUFLAG+=-DV_COPROCESSOR_$(V_COPROCESSOR)
  CFLAGS+=-DV_COPROCESSOR_$(V_COPROCESSOR)
endif

ifneq ($(V_WATCHDOG_SETTING_UI),none)
  MENUFLAG+=-DV_WATCHDOG_SETTING_UI_y
  CFLAGS+=-DV_WATCHDOG_SETTING_UI_y
endif

ifneq ($(V_STARTUP_WATCHDOG),none)
  MENUFLAG+=-DV_STARTUP_WATCHDOG_y
  CFLAGS+=-DV_STARTUP_WATCHDOG_y
endif

ifneq ($(V_SHUTDOWN_WATCHDOG),none)
  MENUFLAG+=-DV_SHUTDOWN_WATCHDOG_y
  CFLAGS+=-DV_SHUTDOWN_WATCHDOG_y
endif

ifneq ($(V_ETHERNET_PACKET_PORT_WATCHDOG),none)
  MENUFLAG+=-DV_ETHERNET_PACKET_PORT_WATCHDOG_y
  CFLAGS+=-DV_ETHERNET_PACKET_PORT_WATCHDOG_y
endif

ifneq ($(V_USB_ENUMERATION_WATCHDOG),none)
  MENUFLAG+=-DV_USB_ENUMERATION_WATCHDOG_y
  CFLAGS+=-DV_USB_ENUMERATION_WATCHDOG_y
endif

ifeq ($(V_GPS_GEOFENCE),y)
  MENUFLAG+=-DV_GPS_GEOFENCE_y
endif

ifeq ($(V_VRRP_WAN_WATCHDOG),y)
  CFLAGS+=-DV_VRRP_WAN_WATCHDOG_y
  MENUFLAG+=-DV_VRRP_WAN_WATCHDOG_y
endif

# With implementing a new package installation during firmware upgrades,
# some ipks need another power-cycle after the ipks are configured.
# Currently, only 'nas' ipk triggers the configuration power-cycle via rdb varibale'system.postinstall.reboot'.
ifneq ($(V_INCLUDED_IPK),$(filter-out nas,$(V_INCLUDED_IPK)))
  MENUFLAG+=-DREBOOT_IPK_INSTALL
  CFLAGS+=-DREBOOT_IPK_INSTALL
endif

ifeq ($(V_IPBASED_VLAN),y)
  MENUFLAG+=-DV_IPBASED_VLAN_y
endif

ifeq ($(V_ONE_WIRE),y)
  CFLAGS+=-DV_ONE_WIRE
  MENUFLAG+=-DV_ONE_WIRE
endif

ifeq ($(V_TRIG_IO_PIN_ON_FAILOVER),y)
  CFLAGS+=-DV_TRIG_IO_PIN_ON_FAILOVER
  MENUFLAG+=-DV_TRIG_IO_PIN_ON_FAILOVER
endif

ifeq ($(V_DIAGNOSTIC_LOG),y)
  MENUFLAG+=-DV_DIAGNOSTIC_LOG
  CFLAGS+=-DV_DIAGNOSTIC_LOG
endif

ifeq ($(V_IPK_FW_SIGNING),y)
  MENUFLAG+=-DV_IPK_FW_SIGNING
  CFLAGS+=-DV_IPK_FW_SIGNING
endif

ifeq ($(V_REQUIRE_FACTORY_PASSWORD_CHANGE),y)
  MENUFLAG+=-DV_REQUIRE_FACTORY_PASSWORD_CHANGE_y
  CFLAGS+=-DV_REQUIRE_FACTORY_PASSWORD_CHANGE_y
endif

ifeq ($(V_CBRS_SAS),y)
  MENUFLAG+=-DV_CBRS_SAS_y
  CFLAGS+=-DV_CBRS_SAS_y
endif

ifeq ($(V_RUNTIME_CONFIG),y)
  MENUFLAG+=-DV_RUNTIME_CONFIG_y
  CFLAGS+=-DV_RUNTIME_CONFIG_y
endif

MENUFLAG+=-DV_ROUTING_UI_$(V_ROUTING_UI)

$(info ***Webif***************************************************************)
$(info product=$(V_PRODUCT))
$(info webif_version=$(V_WEBIF_VERSION))
$(info mode=$(MODE))
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
$(info cell_nw = $(V_CELL_NW))
$(info gps = $(V_GPS))
$(info odometer = $(V_ODOMETER))
$(info sgps = $(V_HAS_SGPS))
$(info agps = $(V_HAS_AGPS))
$(info serial_has_fc=$(V_SERIAL_HAS_FC))
$(info dial_on_demand=$(V_DIAL_ON_DEMAND))
$(info keep_single_tcp=$(V_KEEP_SINGLE_TCP_CONN))
$(info event_notification=$(V_EVENT_NOTIFICATION))
$(info email_client=$(V_EMAIL_CLIENT))
$(info custom_feature_pack=$(V_CUSTOM_FEATURE_PACK))
$(info bluetooth=$(V_BLUETOOTH))
$(info data_stream_switch_has_csd=$(V_DATA_STREAM_SWITCH_HAS_CSD))
$(info single_network=$(V_SINGLE_NETWORK))
$(info snmp=$(V_NETSNMP))
$(info vrrp=$(V_VRRP))
$(info rip=$(V_RIP))
$(info wifi_hw = $(V_WIFI_HW))
$(info modcomms = $(V_MODCOMMS))
$(info staging_p = $(STAGING_P))
$(info ******************************************************************)

RDBUTIL_SRC = rdb_util.c
RDBUTIL_OBJ = $(patsubst %.c,%.o,$(RDBUTIL_SRC))

HOTSPOTINIT_SRC = hotspotInit.c
HOTSPOTINIT_OBJ = $(patsubst %.c,%.o,$(HOTSPOTINIT_SRC))

HOTSPOTWIFI_SRC = hotspotWiFi.c
HOTSPOTWIFI_OBJ = $(patsubst %.c,%.o,$(HOTSPOTWIFI_SRC))

CHECKAPN_SRC = checkAPN.c
CHECKAPN_OBJ = $(patsubst %.c,%.o,$(CHECKAPN_SRC))

WEBCOMMON_SRC = webcommon.c
WEBCOMMON_OBJ = $(patsubst %.c,%.o,$(WEBCOMMON_SRC))

AJAX_SRC = ajax.c
AJAX_OBJ = $(patsubst %.c,%.o,$(AJAX_SRC))

APPUPLOAD_SRC = AppUpload.c
APPUPLOAD_OBJ = $(patsubst %.c,%.o,$(APPUPLOAD_SRC))

CDCSPROGRESS_SRC = CdcsProgress.c
CDCSPROGRESS_OBJ = $(patsubst %.c,%.o,$(CDCSPROGRESS_SRC))

ifeq ($(V_GPS),y)
  GPS_SRC = gps.c
  GPS_OBJ = $(patsubst %.c,%.o,$(GPS_SRC))

  GPS_CLKSYNC_SRC = gps_clksync.c
  GPS_CLKSYNC_OBJ = $(patsubst %.c,%.o,$(GPS_CLKSYNC_SRC))
endif

LOGFILE_SRC = logfile.c
LOGFILE_OBJ = $(patsubst %.c,%.o,$(LOGFILE_SRC))

WWANGETAPN_SRC = wwanGetAPN.c
WWANGETAPN_OBJ = $(patsubst %.c,%.o,$(WWANGETAPN_SRC))

TIMEZONELIST_SRC = timezoneList.c
TIMEZONELIST_OBJ = $(patsubst %.c,%.o,$(TIMEZONELIST_SRC))

MANUFACTURE_SRC = manufacture.c
MANUFACTURE_OBJ = $(patsubst %.c,%.o,$(MANUFACTURE_SRC))

CGI_SCRIPTS = ajax.cgi AppUpload.cgi CdcsProgress.cgi logfile wwanGetAPN.cgi check_apn hotspotInit.cgi hotspotWiFi.cgi timezoneList.cgi


ifeq ($(V_GPS),y)
  CGI_SCRIPTS += gps.cgi gps_clksync.cgi
endif

ifeq ($(V_WEBIF_MANUFACTURE),y)
  CGI_SCRIPTS += manufacture.cgi
endif

#---------------------------------------------------------------------------------------------------------
# Separate WEB page files for TOP menu and side menu that will be removed from staging folder
# after all compile and copying operation are done. V2b WEB page files are handled separately
# in its source files respectively.
#
# Note: The following logic is only for existing legacy v2 web pages, and should not be added to any more.
#       All new web pages should be created under the new v2b framework. When a legacy v2 page is converted
#       to v2b page, the corresponding item should be removed from the following list.
ifeq ($(V_NETWORKING_UI),none)
  NETWORKING_UI_FILES =  LAN Profile_Settings Profile_Name_List VLAN_List routing
  NETWORKING_UI_FILES += dod setband sim_management pinsettings DHCP wlanswitch
  NETWORKING_UI_FILES += wlan advanced wifimacblock wlstationlist wifihotspot
  NETWORKING_UI_FILES += wlan_sta wlan_sta_linux lanwan_switch ethwan vlan_ipbased
  NETWORKING_UI_FILES += pppoe wan_summary RIP VRRP NAT DMZ firewall
  NETWORKING_UI_FILES += port_filtering VPN_ipsec VPN_openvpn VPN_pptpc VPN_gre
  NETWORKING_UI_FILES += VPN_scep zigbeeConfig zigbeeStationList zigbeeData
  NETWORKING_UI_FILES += bluetooth_config bluetooth_devices qos_cbq_init
endif
ifeq ($(V_SERVICES_UI),none)
  SERVICES_UI_FILES =  NTP ddns snmp TR069 end_points data_stream
  SERVICES_UI_FILES += v250 padd LWM2M gps msb odometer gps_geofence
  SERVICES_UI_FILES += autodial ussd IO_configuration voice event_noti event_dest
  SERVICES_UI_FILES += email_client SMS_Setup SMS_New_Message SMS_Inbox SMS_Outbox
  SERVICES_UI_FILES += SMS_Diagnostics XEMS network_quality
endif
ifeq ($(V_ADMINISTRATION_UI),none)
  ADMINISTRATION_UI_FILES =  administration server_certificate SSH led_mode sftp_client
  ADMINISTRATION_UI_FILES += tcp_keepalive
endif
ifeq ($(V_WATCHDOG_UI),none)
  WATCHDOG_UI_FILES =  ltph startup_watchdog shutdown_watchdog ethernet_packet_port_watchdog
  WATCHDOG_UI_FILES += usb_enumeration_watchdog dhcp_server_watchdog
endif
ifeq ($(V_HELP_UI),none)
  HELP_UI_FILES =  help
endif
ifeq ($(V_CUSTOM_FEATURE_PACK),bellca)
  MISC_UI_FILES_TO_BE_REMOVED =  ipseclog eventnotilog AppUpload pkManager low_power_standby
  MISC_UI_FILES_TO_BE_REMOVED += usbotg nas button_settings
endif
ifeq ($(V_WEBIF_SPEC),lark)
  MISC_UI_FILES_TO_BE_REMOVED ?= ipseclog eventnotilog AppUpload pkManager low_power_standby usbotg nas button_settings
  MISC_UI_FILES_TO_BE_REMOVED += low_power_standby status wmm Reboot SaveLoadSettings logfile
endif

ifeq ($(V_CBRS_SAS),none)
  CBRS_SAS_UI_FILES =  CBRS_parameters speed_test CBRS_parameters_advanced_config CBRS_parameters_Cfg_Test
else
ifeq (, $(findstring eng, $(V_SKIN)))
  CBRS_SAS_UI_FILES += CBRS_parameters_advanced_config CBRS_parameters_Cfg_Test
endif
endif


ifeq ($(V_ROUTING_UI),none)
  V_ROUTING_UI_FILES =  RIP VRRP NAT DMZ firewall port_filtering
endif

FINAL_STAGE_CLEANUP_FILES =  $(NETWORKING_UI_FILES)
FINAL_STAGE_CLEANUP_FILES += $(SERVICES_UI_FILES)
FINAL_STAGE_CLEANUP_FILES += $(ADMINISTRATION_UI_FILES)
FINAL_STAGE_CLEANUP_FILES += $(WATCHDOG_UI_FILES)
FINAL_STAGE_CLEANUP_FILES += $(WATCHDOG_UI_FILES)
FINAL_STAGE_CLEANUP_FILES += $(HELP_UI_FILES)
FINAL_STAGE_CLEANUP_FILES += $(MISC_UI_FILES_TO_BE_REMOVED)
FINAL_STAGE_CLEANUP_FILES += $(CBRS_SAS_UI_FILES)
FINAL_STAGE_CLEANUP_LANG = en
ifeq ($(V_LANGUAGE_FR),y)
  FINAL_STAGE_CLEANUP_LANG += fr
endif
ifeq ($(V_LANGUAGE_AR),y)
  FINAL_STAGE_CLEANUP_LANG += ar
endif
ifeq ($(V_LANGUAGE_DE),y)
  FINAL_STAGE_CLEANUP_LANG += de
endif
ifeq ($(V_LANGUAGE_IT),y)
  FINAL_STAGE_CLEANUP_LANG += it
endif
ifeq ($(V_LANGUAGE_ES),y)
  FINAL_STAGE_CLEANUP_LANG += es
endif
ifeq ($(V_LANGUAGE_PT),y)
  FINAL_STAGE_CLEANUP_LANG += pt
endif
ifeq ($(V_LANGUAGE_CZ),y)
  FINAL_STAGE_CLEANUP_LANG += cz
endif
ifeq ($(V_LANGUAGE_NL),y)
  FINAL_STAGE_CLEANUP_LANG += nl
endif
ifeq ($(V_LANGUAGE_JP),y)
  FINAL_STAGE_CLEANUP_LANG += jp
endif
#---------------------------------------------------------------------------------------------------------

.PHONY : all
all:  $(CGI_SCRIPTS)

hotspotInit.cgi: $(HOTSPOTINIT_OBJ) $(WEBCOMMON_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(HOTSPOTINIT_OBJ) $(WEBCOMMON_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

hotspotWiFi.cgi: $(HOTSPOTWIFI_OBJ) $(WEBCOMMON_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(HOTSPOTWIFI_OBJ) $(WEBCOMMON_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

check_apn: $(CHECKAPN_OBJ)
	$(CC) $(CHECKAPN_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

ajax.cgi: $(AJAX_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(AJAX_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

AppUpload.cgi: $(APPUPLOAD_OBJ) $(WEBCOMMON_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(APPUPLOAD_OBJ) $(WEBCOMMON_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

CdcsProgress.cgi: $(CDCSPROGRESS_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(CDCSPROGRESS_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

logfile: $(LOGFILE_OBJ)
	$(CC) $(LOGFILE_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

gps.cgi: $(GPS_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(GPS_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

gps_clksync.cgi: $(GPS_CLKSYNC_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(GPS_CLKSYNC_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

wwanGetAPN.cgi: $(WWANGETAPN_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(WWANGETAPN_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

timezoneList.cgi: $(TIMEZONELIST_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(TIMEZONELIST_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

manufacture.cgi: $(MANUFACTURE_OBJ) $(RDBUTIL_OBJ)
	$(CC) $(MANUFACTURE_OBJ) $(RDBUTIL_OBJ) $(LDFLAGS) $(FILE_DBLIB) -o $@
	$(STRIP) $@

allweb: $(wildcard $(WWWSRC)/*)

ifeq ($(MODE),recovery)
  GCC_PREPROC_LIST ?= menu.html status.html port_status.htm port_status.js
  CGI_LIST ?= ajax.cgi CdcsProgress.cgi cgi-scripts/clearlog.cgi cgi-scripts/get_files.cgi cgi-scripts/file_action.cgi cgi-scripts/getInstallMsg.cgi cgi-scripts/default_details.cgi cgi-scripts/SaveSettings.cgi cgi-scripts/emergencyTXT.cgi cgi-scripts/system_reset.cgi cgi-scripts/logfile.cgi
else # MODE != recovery

  ifeq ($(V_WEBIF_VERSION),v2)
    GCC_PREPROC_LIST = util.js topinc_v2.inc net_util.js
    ifeq ($(V_WIFI_CLIENT),backports)
      GCC_PREPROC_LIST += wlan_sta_linux.js
    else ifeq ($(V_WIFI_CLIENT),qca_soc_lsdk)
      GCC_PREPROC_LIST += wlan_sta_linux.js
    endif
    REMOVE_LIST += net_util.js cgi-bin/killmgr.cgi cgi-bin/cportal_reason.cgi cgi-bin/failover_SMS_noti.cgi cgi-bin/failover_SMTP_noti.cgi cgi-bin/FusionPRL_file_action.cgi cgi-bin/get_FusionPRL_files.cgi cgi-bin/Prevision_Sierra_Airprime_MC.cgi cgi-bin/module_omadm.cgi cgi-bin/wwanGetAPN.cgi cgi-bin/emergencyTXT.cgi
  else # V_WEBIF_VERSION != v2
    GCC_PREPROC_LIST ?= status.html logfile.html wifimacblock.html content_filtering.html wlan_sta.html menu.html system_reset.html nat.html advanced.html wlanswitch.html gps.html
    ifneq ($(PLATFORM),Bovine)
      GCC_PREPROC_LIST += siteMap.html
    endif

    ifeq ($(PLATFORM),Platypus2)

      ifneq ($(V_BOARD),nhd1w)
        GCC_PREPROC_LIST += index.html GUI-login.html system_config.html wizard.html
      endif
      GCC_PREPROC_LIST += system_config.html 3gwwan.html wan.html setband.html GUI-wlan.html port_status.htm port_status.js NTP.html LAN.html snmp.html adv_snmp.html ddns.html statistics.html ltph.html
    endif # PLATFORM == Platypus2

    ifeq ($(PLATFORM),Bovine)
      GCC_PREPROC_LIST += snmp.html adv_snmp.html wwan.html 3gwwan.html ltph.html setband.html NTP.html LAN.html failover.html system_config.html ddns.html
    endif

  endif # V_WEBIF_VERSION != v2

endif # MODE != recovery

V_PRODUCT:=$(shell echo "$(V_PRODUCT)" | sed 's/-/_/g')

export V_SKIN
export V_PRODUCT

install: all allweb
	install -d $(INSTALLDIR)/www/images
	install -d $(INSTALLDIR)/www/cgi-bin

ifeq ($(MODE),recovery)
	copy_skip_svn.sh "$(WWWSRC)" "$(INSTALLDIR)/www/"

	# Copy cgi binaries
	for i in $(CGI_LIST); do \
		cp $$i  $(INSTALLDIR)/www/cgi-bin/; \
	done

  ifeq ($(V_WEBIF_VERSION),v1)
	ln -sf /var/log/messages $(INSTALLDIR)/www/logfile.txt
  else # V_WEBIF_VERSION_v2 below:
	ln -sf /var/log/messages $(INSTALLDIR)/etc/logfile.txt
  endif
	cp logfile $(INSTALLDIR)/usr/bin/logfile
	cp stuHover.js $(INSTALLDIR)/www

	-if [ "$(V_SKIN)" = "MOB" ]; then \
		cp $(INSTALLDIR)/www/images/mobImages/*.* $(INSTALLDIR)/www/images/; \
	else \
		cp -fr $(INSTALLDIR)/www/recovery/* $(INSTALLDIR)/www/; \
	fi

	-rm -fr $(INSTALLDIR)/www/images/mobImages

	-mkdir $(INSTALLDIR)/www/preproc
	for i in $(GCC_PREPROC_LIST); do \
		($(CC) -E -x c -P -nostdinc -DPLATFORM_$(PLATFORM) -DV_BOARD_$(V_BOARD)=y -DV_IOBOARD_$(V_IOBOARD)=y -DV_SKIN_$(V_SKIN)=y -DV_PRODUCT_$(V_PRODUCT)=y $(MENUFLAG) -C $(INSTALLDIR)/www/$$i >$(INSTALLDIR)/www/preproc/$$i); \
	done
	cp $(INSTALLDIR)/www/preproc/* $(INSTALLDIR)/www/
	-rm -fr $(INSTALLDIR)/www/preproc
	-rm -fr $(INSTALLDIR)/www/V_*
	-rm -f $(INSTALLDIR)/www/*.*~

else # MODE != recovery)

  ifeq ($(V_WEBIF_VERSION),v1)
	install -d $(INSTALLDIR)/www/hotspot/images
	ln -sf /tmp/usermenu.html $(INSTALLDIR)/www/usermenu.html; \
	ln -fs /opt/cdcs/doc/ $(INSTALLDIR)/www/

	copy_skip_svn.sh "$(WWWSRC)" "$(INSTALLDIR)/www/"
	if [ "$(PLATFORM)" = "Avian" ]; then \
		ln -sf /system/cdcs/etc/cdcs/conf/default.conf $(INSTALLDIR)/www/default.conf; \
	else \
		ln -sf /etc/cdcs/conf/default.conf $(INSTALLDIR)/www/default.conf; \
		copy_skip_svn.sh "V_WEBIF_VERSION_v1/common_html" "$(INSTALLDIR)/www/"; \
	fi

	if [ "$(V_WIFI)" != "none" -a "$(V_WIFI)" != "fullmac" ]; then \
		copy_skip_svn.sh "V_WEBIF_VERSION_v1/V_WIFI" "$(INSTALLDIR)/www/"; \
	fi

	if [ "$(V_WIFI_CLIENT)" != "none" ]; then \
		copy_skip_svn.sh "V_WEBIF_VERSION_v1/V_WIFI_CLIENT" "$(INSTALLDIR)/www/"; \
	fi

	if [ "$(V_WEBIF_DEVTOOLS)" = "y" ]; then \
		copy_skip_svn.sh "V_WEBIF_VERSION_v1/V_WEBIF_DEVTOOLS" "$(INSTALLDIR)/www/"; \
	fi

	# Copy cgi binaries
	cp V_WEBIF_VERSION_v1/cgi-scripts/*.cgi $(INSTALLDIR)/www/cgi-bin
	cp cgi-scripts/*.cgi $(INSTALLDIR)/www/cgi-bin
	# give the platform name to the cgi - TODO: we may use this feature to all CGIs as we do not compile CGIs
	sed -i 's/<<<PLATFORM>>>/$(PLATFORM)/g' "$(INSTALLDIR)/www/cgi-bin/routerservice.cgi"

	#copy .sh to /usr/bin
	cp cgi-scripts/upload_killall.sh $(INSTALLDIR)/usr/bin
	-if [ "$(V_SLIC)" != "none" ]; then \
		cp cgi-scripts/set_telephony_profile.sh $(INSTALLDIR)/usr/bin; \
		cp cgi-scripts/set_telephony_profile.sh $(INSTALLDIR)/www/cgi-bin; \
	fi

#	$(CC) -E -x c -P -nostdinc -DHASH=# -DV_BOARD_$(V_BOARD)=y -C cgi-scripts/file_action.cgi >$(INSTALLDIR)/www/cgi-bin/file_action.cgi
	cp CdcsProgress.cgi $(INSTALLDIR)/www/cgi-bin
	cp logfile $(INSTALLDIR)/usr/bin/logfile
	cp ajax.cgi $(INSTALLDIR)/www/cgi-bin
	cp wwanGetAPN.cgi $(INSTALLDIR)/www/cgi-bin
	if [ "$(PLATFORM)" = "Platypus2" ]; then \
		ln -sf /tmp/messages $(INSTALLDIR)/www/logfile.txt; \
		ln -sf /tmp/appweb $(INSTALLDIR)/www/tmp; \
		copy_skip_svn.sh "$(WWWSRC)/cgi-bin/" "$(INSTALLDIR)/www/cgi-bin/"; \
	else \
		ln -sf /var/log/messages $(INSTALLDIR)/www/logfile.txt; \
		ln -fs /opt/cdcs/doc/ $(INSTALLDIR)/www/doc; \
	fi
	cp stuHover.js $(INSTALLDIR)/www
	cp check_apn $(INSTALLDIR)/bin/wwan_check_apn

	-if [ "$(V_GPS)" = "y" ]; then \
		cp gps.cgi $(INSTALLDIR)/www/cgi-bin; \
		cp gps_clksync.cgi $(INSTALLDIR)/www/cgi-bin; \
	fi

	cp timezoneList.cgi $(INSTALLDIR)/www/cgi-bin

	mkdir -p $(INSTALLDIR)/www/hotspot
	mkdir -p $(INSTALLDIR)/www/usermenu
	cp hotspotInit.cgi $(INSTALLDIR)/www/hotspot/hotspotInit.cgi
	cp hotspotWiFi.cgi $(INSTALLDIR)/www/hotspot/hotspotWiFi.cgi
#	cp $(INSTALLDIR)/www/hotspot/ntcImages/*.* $(INSTALLDIR)/www/hotspot/images/
#	mv $(INSTALLDIR)/www/hotspot/ntcIndex.html $(INSTALLDIR)/www/hotspot/index.html
	if [ "$(PLATFORM)" = "Avian" ]; then \
		ln -sf /system/cdcs/www/cgi-bin $(INSTALLDIR)/www/hotspot/cgi-bin; \
		ln -sf /system/cdcs/www/MyZoneStyle.css $(INSTALLDIR)/www/hotspot/MyZoneStyle.css; \
		ln -sf /system/cdcs/www/BovineStyle.css $(INSTALLDIR)/www/hotspot/BovineStyle.css; \
		if [ "$(V_SKIN)" = "RET" ]; then \
			copy_skip_svn.sh "$(INSTALLDIR)/www/eti_pages/" "$(INSTALLDIR)/www/"; \
			ln -sf /system/cdcs/www/MyZoneStyle-ar.css $(INSTALLDIR)/www/hotspot/MyZoneStyle-ar.css; \
		elif [ "$(V_SKIN)" = "QEU" ]; then \
			copy_skip_svn.sh "$(INSTALLDIR)/www/qeuc_pages/" "$(INSTALLDIR)/www/"; \
		fi; \
	fi

	-if [ "$(V_SKIN)" = "MOB" ]; then \
		cp $(INSTALLDIR)/www/images/mobImages/*.* $(INSTALLDIR)/www/images/; \
	else \
		cp -fr $(INSTALLDIR)/www/V_WEBIF_SPEC_$(V_WEBIF_SPEC)/* $(INSTALLDIR)/www/; \
	fi

	-if [ "$(V_WIFI)" != "none" -a "$(V_WIFI)" != "fullmac" ]; then \
		cp -fr $(INSTALLDIR)/www/V_WIFI/* $(INSTALLDIR)/www/; \
	fi

	-if [ "$(V_WIFI_CLIENT)" != "none" ]; then \
		cp -fr $(INSTALLDIR)/www/V_WIFI_CLIENT/* $(INSTALLDIR)/www/; \
	fi

	-if [ "$(PLATFORM)" = "Bovine" ]; then \
		if [ "$(V_SERIAL)" != "none" ]; then \
			cp -fr $(INSTALLDIR)/www/V_SERIAL/* $(INSTALLDIR)/www/; \
		fi; \
	fi

	-rm -fr $(INSTALLDIR)/www/images/mobImages

	-mkdir $(INSTALLDIR)/www/preproc
	-for i in $(GCC_PREPROC_LIST); do \
		($(CC) -E -x c -P -nostdinc -DPLATFORM_$(PLATFORM) -DV_BOARD_$(V_BOARD)=y -DV_IOBOARD_$(V_IOBOARD)=y -DV_SKIN_$(V_SKIN)=y -DV_PRODUCT_$(V_PRODUCT)=y $(MENUFLAG) -C $(INSTALLDIR)/www/$$i >$(INSTALLDIR)/www/preproc/$$i); \
	done

	cp $(INSTALLDIR)/www/preproc/* $(INSTALLDIR)/www/

	-rm -fr $(INSTALLDIR)/www/preproc

	# Not to delete inc files involved in sdk.
	-mkdir $(INSTALLDIR)/www/sdkinc
	cp $(INSTALLDIR)/www/*_sdk.inc $(INSTALLDIR)/www/sdkinc/

	# remove all inc files
	-rm -f "$(INSTALLDIR)/www/"*".inc"

	cp $(INSTALLDIR)/www/sdkinc/* $(INSTALLDIR)/www/
	-rm -fr $(INSTALLDIR)/www/sdkinc/

	# copy some new v2 include files to replace removed CGI files
	cp V_WEBIF_VERSION_v2/V_WIFI/mbssid.inc $(INSTALLDIR)/www/
	cp V_WEBIF_VERSION_v2/V_VPN/vpnCommon.inc $(INSTALLDIR)/www/

  else # V_WEBIF_VERSION != v1
	# Copy cgi scripts
	cp cgi-scripts/*.cgi $(INSTALLDIR)/www/cgi-bin

	# Copy binaries
	cp CdcsProgress.cgi $(INSTALLDIR)/www/cgi-bin
	cp logfile $(INSTALLDIR)/usr/bin/logfile
	cp ajax.cgi $(INSTALLDIR)/www/cgi-bin
	cp wwanGetAPN.cgi $(INSTALLDIR)/www/cgi-bin
	cp timezoneList.cgi $(INSTALLDIR)/www/cgi-bin
	cp check_apn $(INSTALLDIR)/bin/wwan_check_apn

	if [ "$(V_WEBIF_MANUFACTURE)" = "y" ]; then \
		cp manufacture.cgi $(INSTALLDIR)/www/cgi-bin; \
	fi

	#copy .sh to /usr/bin
	cp cgi-scripts/upload_killall.sh $(INSTALLDIR)/usr/bin
	-if [ "$(V_SLIC)" != "none" ]; then \
		cp cgi-scripts/set_telephony_profile.sh $(INSTALLDIR)/usr/bin; \
		cp cgi-scripts/set_telephony_profile.sh $(INSTALLDIR)/www/cgi-bin; \
	fi

	-rm -fr $(INSTALLDIR)/www/usermenu
	copy_skip_svn.sh $(WWWSRC)/common $(shell readlink -f $(INSTALLDIR)/www/)
	copy_skip_svn.sh $(WWWSRC)/V_WEBIF_SPEC_$(V_WEBIF_SPEC) $(shell readlink -f $(INSTALLDIR)/www/)

	if [ "$(V_WIFI)" != "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_WIFI $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_WIFI_CLIENT)" != "none" ]; then \
		if [ "$(V_WIFI_CLIENT)" != "ralink" ]; then \
			copy_skip_svn.sh $(WWWSRC)/V_WIFI_CLIENT/common $(shell readlink -f $(INSTALLDIR)/www/); \
		fi; \
		copy_skip_svn.sh $(WWWSRC)/V_WIFI_CLIENT/$(V_WIFI_CLIENT) $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_WEBIF_DEVTOOLS)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_WEBIF_DEVTOOLS $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SMS)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SMS $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_VPN)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_VPN $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_CELL_NW)" = "cdma" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_CELL_NW_cdma $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SERIAL)" != "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SERIAL $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SSH)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SSH $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_WEBIF_DEVTOOLS)" = "y" ]; then \
		copy_skip_svn.sh "V_WEBIF_DEVTOOLS" $(shell readlink -f $(INSTALLDIR)/www/); \
		ln -fs /opt/cdcs/web/www.tar $(INSTALLDIR)/www/www.tar; \
		ln -fs /opt/cdcs/web/lang.tar $(INSTALLDIR)/www/lang.tar; \
		ln -fs /opt/cdcs/web/string_spec_$(V_WEBIF_SPEC).csv $(INSTALLDIR)/www/string_spec_$(V_WEBIF_SPEC).csv; \
	fi

	if [ "$(V_WEBIF_SPEC)" = "vdf" ]; then \
		ln -sf /www/vdf-lib/js/lib/lib.min.js $(INSTALLDIR)/www/jquery-1.6.min.js; \
	fi

	if [ "$(V_CHECK_PASSWORD_STRENGTH)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_CHECK_PASSWORD_STRENGTH $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_GPS)" = "y" ]; then \
		cp gps_clksync.cgi $(INSTALLDIR)/www/cgi-bin; \
		copy_skip_svn.sh $(WWWSRC)/V_GPS $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_ODOMETER)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_ODOMETER $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_IOMGR)" = "ioext4" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_IOMGR $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_IOMGR)" = "clarke" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_IOMGR $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_IOMGR)" = "nguni" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_IOMGR $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_IOMGR)" = "kudu" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_IOMGR $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_MODCOMMS)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_IOMGR $(shell readlink -f $(INSTALLDIR)/www/); \
		copy_skip_svn.sh $(WWWSRC)/V_MODCOMMS_y $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_DATA_STREAM_SWITCH)" = "y" ]; then \
		data_stream_destination_dir="$(shell readlink -f $(INSTALLDIR)/www/)"; \
		copy_skip_svn.sh $(WWWSRC)/V_DATA_STREAM_SWITCH/cgi-bin $${data_stream_destination_dir}/cgi-bin; \
		data_stream_files_to_copy=""; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/data_stream.html"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/end_points.html"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/end_point_types.inc"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/dsm_ser_and_modem_ep.inc"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/dsm_serial_ep.inc"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type1.html"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type2.html"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type3.html"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type4.html"; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type5.html"; \
		if [ "$(V_GPS)" = "y" -a "$(V_HAS_SGPS)" = "y" ]; then \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type6.html"; \
		fi; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type7.html"; \
		if [ "$(V_SERIAL_IO_MODE)" != "none" ]; then \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type8.html"; \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type9.html"; \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type10.html"; \
		fi; \
		if [ "$(V_SERIAL)" != "none" ]; then \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type11.html"; \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type11_adv.html"; \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type12.html"; \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type13.html"; \
		fi; \
		if [ "$(V_DATA_STREAM_SWITCH_HAS_CSD)" = "y" ]; then \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type14.html"; \
		fi; \
		data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type15.html"; \
		if [ "$(V_BLUETOOTH)" != "none" ]; then \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type16.html"; \
			data_stream_files_to_copy="$$data_stream_files_to_copy $(WWWSRC)/V_DATA_STREAM_SWITCH/End_Point_Type17.html"; \
		fi; \
		cp $$data_stream_files_to_copy $$data_stream_destination_dir; \
	fi

	if [ "$(V_WIFI_HOTSPOT)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_WIFI_HOTSPOT $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_CUSTOM_FEATURE_PACK)" = "hitachi_nedo" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_CUSTOM_FEATURE_PACK_hitachi_nedo $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_POWERSAVE)" != "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_POWERSAVE/common $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_POWERSAVE)" = "nguni" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_POWERSAVE/add_nguni $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_POWERSAVE)" = "kudu" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_POWERSAVE/add_kudu $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	if [ "$(V_POWERSAVE)" = "clarke" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_POWERSAVE/add_clarke $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SIMMGMT)" = "y" ]; then \
               copy_skip_svn.sh $(WWWSRC)/V_SIMMGMT_y $(shell readlink -f $(INSTALLDIR)/www/); \
	fi
	#if [ "$(V_AUTODIAL)" = "y" ]; then \
	#	copy_skip_svn.sh $(WWWSRC)/V_AUTODIAL $(shell readlink -f $(INSTALLDIR)/www/); \
	#fi This is now v2b

	if [ "$(V_SLIC)" != "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SLIC $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_MODULE)" = "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_MODULE_none $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_MULTIPLE_LANWAN_UI)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_MULTIPLE_LANWAN_UI $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_NAS)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_NAS $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_OMA_DM_LWM2M)" != "none" ]; then \
		if [ "$(V_OMA_DM_LWM2M)" = "v2" ]; then \
			copy_skip_svn.sh $(WWWSRC)/V_OMA_DM_LWM2M_v2 $(shell readlink -f $(INSTALLDIR)/www/); \
		else \
			copy_skip_svn.sh $(WWWSRC)/V_OMA_DM_LWM2M $(shell readlink -f $(INSTALLDIR)/www/); \
		fi \
	fi

	if [ "$(V_USB_OTG_MANUAL_MODE_SELECTION)" != "" -a "$(V_USB_OTG_MANUAL_MODE_SELECTION)" != "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_USB_OTG_MANUAL_MODE_SELECTION $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_EVENT_NOTIFICATION)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_EVENT_NOTIFICATION $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_EMAIL_CLIENT)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_EMAIL_CLIENT $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_ROUTER_TERMINATED_PPPOE)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_ROUTER_TERMINATED_PPPOE $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_BLUETOOTH)" != "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_BLUETOOTH $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_QOS_CBQ_INIT)" != "none" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_QOS_CBQ_INIT $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	-copy_skip_svn.sh $(WWWSRC)/V_PRODUCT_$(V_PRODUCT) $(shell readlink -f $(INSTALLDIR)/www/)

	if [ "$(V_WATCHDOG_SETTING_UI)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_WATCHDOG_SETTING_UI_y $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SFTPC)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SFTPC $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_TCP_KEEPALIVE)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_TCP_KEEPALIVE $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SCEP_CLIENT)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SCEP_CLIENT $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_GPS_GEOFENCE)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_GPS_GEOFENCE_y $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_ODOMETER)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_ODOMETER $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_IPBASED_VLAN)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_IPBASED_VLAN_y $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SPEED_TEST)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SPEED_TEST_y $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_IPK_FW_SIGNING)" = "y" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_IPK_FW_SIGNING_y $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

	if [ "$(V_SKIN)" = "VDF2" ]; then \
		copy_skip_svn.sh $(WWWSRC)/V_SKIN_VDF2 $(shell readlink -f $(INSTALLDIR)/www/); \
	fi

    ifeq ($(PLATFORM),Serpent)
	ln -sf /tmp/messages $(INSTALLDIR)/etc/logfile.txt
    else
	ln -sf /var/log/messages $(INSTALLDIR)/etc/logfile.txt
    endif
	ln -sf /var/log/ipseclog $(INSTALLDIR)/etc/ipseclog.txt
	ln -fs /opt/cdcs/doc/ $(INSTALLDIR)/www/doc
	mkdir -p $(INSTALLDIR)/www/usermenu

	-mkdir $(INSTALLDIR)/www/preproc
	for i in $(GCC_PREPROC_LIST); do \
		($(CC) -E -x c -P -nostdinc $(MENUFLAG) -C $(INSTALLDIR)/www/$$i >$(INSTALLDIR)/www/preproc/$$i); \
	done

	cd $(INSTALLDIR)/www/ && \
	for i in `find *.html -type f`; do \
		($(CC) -E -x c -P -nostdinc $(MENUFLAG) -C $(INSTALLDIR)/www/$$i >$(INSTALLDIR)/www/preproc/$$i); \
	done

	-cp $(INSTALLDIR)/www/preproc/* $(INSTALLDIR)/www/
	rm -fr $(INSTALLDIR)/www/preproc

	-for i in $(REMOVE_LIST); do \
		rm  $(INSTALLDIR)/www/$$i; \
	done

	# These files are copied to staging_p folder for NAS package
	if [ "$(V_NAS)" = "y" ]; then \
		mkdir -p $(STAGING_P)/nas/usr/local/www/cgi-bin; \
		mv $(INSTALLDIR)/www/nas.html $(STAGING_P)/nas/usr/local/www/; \
		mv $(INSTALLDIR)/www/cgi-bin/nas.cgi $(STAGING_P)/nas/usr/local/www/cgi-bin/; \
	fi

	# Build the new Javascipt driven WebUI
	pushd ./V_WEBIF_VERSION_v2b;MENUFLAG='$(MENUFLAG)' HBASE='$(HBASE)' ./build.sh && popd

  endif # V_WEBIF_VERSION != v1
	-if [ "$(V_TR069)" = "bovine" -o "$(V_TR069)" = "serpent" ]; then \
		ln -sf /usr/lib/tr-069/parmalist/tr-069parameterlist.html $(INSTALLDIR)/www/tr-069parameterlist.html; \
		ln -sf /usr/lib/tr-069/parmalist/tr069_sdm_1.xml $(INSTALLDIR)/www/tr069_sdm_1.xml; \
	fi

	# generate mib
	./build_platform_mib.sh > $(INSTALLDIR)/www/snmp.mib

	-rm -fr $(INSTALLDIR)/www/V_*
	-rm -f $(INSTALLDIR)/www/*.*~

	#-----------------------------------------------------------
	# Clean-up unnecessary WEB page files from staging folder
	# depending on relative Vvariables for security
	# This cleaning should be done here because the order of WEBUI
	# compilation is;
	# webif --> v2b webif --> internationalization
	# and the target WEB page files and language .xml files should
	# be removed at the last stage of this compilation.
	-for i in $(FINAL_STAGE_CLEANUP_FILES); do \
		rm -f $(INSTALLDIR)/www/$$i.html 2>/dev/null; \
		rm -f $(INSTALLDIR)/www/$$i.htmlv2b 2>/dev/null; \
		for j in $(FINAL_STAGE_CLEANUP_LANG); do \
			rm -f $(INSTALLDIR)/www/lang/$$j/$$i.xml 2>/dev/null; \
		done \
	done
	-rm -fr $(INSTALLDIR)/www/obsolete 2>/dev/null
	#-----------------------------------------------------------

	touch ../Internationalization/BuildFiles.c

endif # MODE != recovery)

ifeq ($(V_WEBIF_VERSION),v1)
COMMON_HTML_LIST:=$(glob V_WEBIF_VERSION_v1/common_html)
endif

clean distclean:
	-rm -f *.o *.cgi logfile check_apn
ifeq ($(V_WEBIF_VERSION),v1)
	( cd V_WEBIF_VERSION_v1/bovine_html && rm -f $(COMMON_HTML_LIST) )
endif
