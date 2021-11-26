#
# Expects the following external variables to be set
#
# INSTALLDIR     = Root of staging tree for install
# V_*            = Variant specifiers

TEMPINSTALL := $(shell /bin/pwd)/install
TEMPBUILD := $(shell /bin/pwd)/build

.PHONY : all dirs json
dirs:
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin/
	install -m 0755 -d $(TEMPINSTALL)/usr/share/lua/5.1/
	install -m 0755 -d $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
ifneq ($(V_RGIF),none)
ifneq ($(V_RGIF),)
	install -m 0755 -d $(TEMPINSTALL)/usr/share/turbontc/
	mkdir -p $(TEMPBUILD)
endif
endif
ifneq ($(V_DA_AUTHENTICATE),none)
ifneq ($(V_DA_AUTHENTICATE),)
	install -m 0755 -d $(TEMPINSTALL)/usr/share/authenticate/
	install -m 0755 -d $(TEMPINSTALL)/usr/share/authenticate/service
endif
endif

json: dirs
ifneq ($(V_RGIF),none)
ifneq ($(V_RGIF),)
    # Merge common and variant specific JSON files
	./merge_json variants/V_RGIF/add/rgif_common.json variants/V_RGIF/add_$(V_RGIF)/rgif_variant.json > $(TEMPBUILD)/rgif_swagger.json
	install -m 755 $(TEMPBUILD)/rgif_swagger.json $(TEMPINSTALL)/usr/share/turbontc/
endif
endif
ifneq ($(V_DA_AUTHENTICATE),none)
ifneq ($(V_DA_AUTHENTICATE),)
	install -m 644 variants/V_DA_AUTHENTICATE/add/common.json $(TEMPINSTALL)/usr/share/authenticate/
ifneq ($(V_DA_NIT_IF),none)
ifneq ($(V_DA_NIT_IF),)
	install -m 644 variants/V_DA_NIT_IF/add/nit.json $(TEMPINSTALL)/usr/share/authenticate/
endif
endif
endif
endif

all: dirs json
	install -m 755 turbontc.lua $(TEMPINSTALL)/usr/bin/
ifneq ($(V_RESTFULWEBIF),none)
ifneq ($(V_RESTFULWEBIF),)
	install -m 755 historyDB.lua $(TEMPINSTALL)/usr/share/lua/5.1/
	install -m 755 turbontc.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
endif
endif
ifneq ($(V_RGIF),none)
ifneq ($(V_RGIF),)
	install -m 755 variants/V_RGIF/add/*.lua $(TEMPINSTALL)/usr/share/turbontc/
	install -m 755 variants/V_RGIF/add_$(V_RGIF)/*.lua $(TEMPINSTALL)/usr/share/turbontc/
endif
endif
ifneq ($(V_DA_AUTHENTICATE),none)
ifneq ($(V_DA_AUTHENTICATE),)
	install -m 644 variants/V_DA_AUTHENTICATE/add/*.lua $(TEMPINSTALL)/usr/share/authenticate/
	install -m 755 variants/V_DA_AUTHENTICATE/add/authenticate.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
endif
endif
ifneq ($(V_DA_AUTHENTICATE_SERVICE),none)
ifneq ($(V_DA_AUTHENTICATE_SERVICE),)
	install -m 644 variants/V_DA_AUTHENTICATE_SERVICE/$(V_DA_AUTHENTICATE_SERVICE)/*.lua $(TEMPINSTALL)/usr/share/authenticate/service
endif
endif
ifneq ($(V_DA_AUTHENTICATE),none)
ifneq ($(V_DA_AUTHENTICATE),)
ifeq ($(V_RUNTIME_CONFIG),y)
ifeq ($(PLATFORM),Cassini)
	install -m 644 variants/V_RUNTIME_CONFIG/add_Cassini/*.lua $(TEMPINSTALL)/usr/share/authenticate/
	install -m 644 variants/V_RUNTIME_CONFIG/add_Cassini/update.json $(TEMPINSTALL)/usr/share/authenticate/
else
	install -m 644 variants/V_RUNTIME_CONFIG/add/*.lua $(TEMPINSTALL)/usr/share/authenticate/
endif
endif
ifneq ($(V_DA_NIT_IF),none)
ifneq ($(V_DA_NIT_IF),)
	install -m 644 variants/V_DA_NIT_IF/add/*.lua $(TEMPINSTALL)/usr/share/authenticate/
	install -m 644 variants/V_DA_NIT_IF/add/service/*.lua $(TEMPINSTALL)/usr/share/authenticate/service
endif
endif
endif
endif

.PHONY : install
install: all
	scatter_install.sh -s true "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr -D /etc/
