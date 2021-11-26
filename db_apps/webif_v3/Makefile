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
# Copyright (C) 2020 Casa Systems.

CROSS_COMPILE ?= arm-casa-linux-gnueabi-

INSTALLDIR ?= $(STAGING)
CDCS_LIBSTAGE ?= $(STAGING)_l

TEMPINSTALL := $(shell /bin/pwd)/temp_install
TEMPBUILD := $(shell /bin/pwd)/temp_build

BUILDERDIR := $(shell /bin/pwd)/builder
SRCDIR := $(shell /bin/pwd)/src

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

include $(HBASE)/variant.mk

# CFLAGS will be needed for C-preprocessing and/or compile some files.
CFLAGS :=
# Adding -DV_... to CFLAGS
CFLAGS += -DV_WEBIF_SERVER_$(V_WEBIF_SERVER) -DV_WEBIF_SPEC_$(V_WEBIF_SPEC)
CFLAGS += -DV_CHECK_PASSWORD_STRENGTH_$(V_CHECK_PASSWORD_STRENGTH)
CFLAGS += -DV_WMMD_GPS_$(V_WMMD_GPS)
CFLAGS += -DV_SOFTWARE_UPGRADE_UI_$(V_SOFTWARE_UPGRADE_UI)
CFLAGS += -DV_LOCK_IMS_SOS_PROFILE_$(V_LOCK_IMS_SOS_PROFILE)
CFLAGS += -DV_READONLY_WWAN_PROFILE_$(V_READONLY_WWAN_PROFILE)
CFLAGS += -DV_CUSTOM_FEATURE_PACK_$(V_CUSTOM_FEATURE_PACK)
CFLAGS += -DV_NTP_SERVER_ADDR_NUMBER=$(V_NTP_SERVER_ADDR_NUMBER)
CFLAGS += -DV_LARGE_LOGFILE_$(V_LARGE_LOGFILE)
CFLAGS += -DV_DA_NIT_IF_$(V_DA_NIT_IF)
CFLAGS += -DV_PER_PROFILE_MTU_$(V_PER_PROFILE_MTU)
CFLAGS += -DV_RUNTIME_CONFIG_$(V_RUNTIME_CONFIG)
CFLAGS += -DV_TR069_MANAGEMENT_APN_$(V_TR069_MANAGEMENT_APN)


include $(HBASE)/variant.mk

DIRS_FILES_IN = $(1) $(shell find $(1) -type d) $(shell find $(1) -type f)

.PHONY : all install clean distclean NTC_UI_js
all: prepare.done build.done

prepare.done: package.json
	nodejs -v
	npm install
	mkdir -p $(TEMPINSTALL)/www/js
	mkdir -p $(TEMPINSTALL)/www/cgi-bin
	mkdir -p $(TEMPBUILD)/theme_gen
	mkdir -p $(TEMPBUILD)/gen
	mkdir -p $(TEMPBUILD)/src_pages
	mkdir -p $(TEMPBUILD)/src_cgi_bin
	mkdir -p $(TEMPBUILD)/pages
	mkdir -p $(TEMPINSTALL)
	touch prepare.done

build.done: $(TEMPBUILD)/variant.json pages_cgi_bin.done $(TEMPINSTALL)/www/js/generatedMenuEntries.js www.done languages.done server.done rootfs.done
	touch $@

$(TEMPBUILD)/variant.json: $(HBASE)/variant.sh $(BUILDERDIR)/genVariantJson.ts
	npx ts-node $(BUILDERDIR)/genVariantJson.ts $(HBASE)/variant.sh $@

theme_gen.done: $(HBASE)/variant.sh $(call DIRS_FILES_IN,$(SRCDIR)/theme_gen)
	$(BUILDERDIR)/install_files_v_vars.sh $(HBASE)/variant.sh $(SRCDIR)/theme_gen $(TEMPBUILD)/theme_gen
	touch $@

pages_cgi_bin.done: $(HBASE)/variant.sh $(call DIRS_FILES_IN,builder) $(call DIRS_FILES_IN,$(SRCDIR)/common) $(call DIRS_FILES_IN,$(SRCDIR)/pages) $(call DIRS_FILES_IN,$(SRCDIR)/cgi-bin) theme_gen.done
	TEMPINSTALL='$(TEMPINSTALL)' TEMPBUILD='$(TEMPBUILD)' CFLAGS='$(CFLAGS)' HBASE='$(HBASE)' \
	SRCDIR='$(SRCDIR)' BUILDERDIR='$(BUILDERDIR)' $(BUILDERDIR)/build_pages_cgi_bin.sh
	touch $@

$(TEMPINSTALL)/www/js/generatedMenuEntries.js: $(TEMPBUILD)/variant.json $(call DIRS_FILES_IN,$(SRCDIR)/entries_def)
	npx ts-node $(BUILDERDIR)/genEntriesDef.ts $(SRCDIR)/entries_def $(TEMPBUILD)/variant.json $@

www.done: $(HBASE)/variant.sh $(call DIRS_FILES_IN,$(SRCDIR)/www)
	$(BUILDERDIR)/install_files_v_vars.sh $(HBASE)/variant.sh $(SRCDIR)/www $(TEMPINSTALL)/www
	touch $@

rootfs.done: $(HBASE)/variant.sh $(call DIRS_FILES_IN,$(SRCDIR)/rootfs)
	$(BUILDERDIR)/install_files_v_vars.sh $(HBASE)/variant.sh $(SRCDIR)/rootfs $(TEMPINSTALL)
	touch $@

languages.done: $(TEMPBUILD)/variant.json $(call DIRS_FILES_IN,$(SRCDIR)/languages)
	npx ts-node $(BUILDERDIR)/genLanguages.ts $(SRCDIR)/languages $(TEMPBUILD)/variant.json $(TEMPINSTALL)/www/js
	touch $@

server.done: $(HBASE)/variant.sh $(call DIRS_FILES_IN,$(SRCDIR)/server)
	cd $(SRCDIR)/server && make HBASE='$(HBASE)' TEMPINSTALL='$(TEMPINSTALL)'
	touch $@

install: prepare.done build.done
	mkdir -p "$(INSTALLDIR)"
	cd "$(TEMPINSTALL)" && tar -cf - * | tar -C "$(INSTALLDIR)" -xvf -

clean distclean:
	rm -f *.done
	rm -rf $(TEMPBUILD)
	rm -rf $(TEMPINSTALL)
	rm -rf $(INSTALLDIR)/www/
	cd $(SRCDIR)/server && make HBASE='$(HBASE)' TEMPINSTALL='$(TEMPINSTALL)' INSTALLDIR='$(INSTALLDIR)' distclean
