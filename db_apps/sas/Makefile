#
# Expects the following external variables to be set
#
# INSTALLDIR     = Root of staging tree for install

SAS_DIR ?= usr/share/lua/5.1/sas
LUA_DIR ?= usr/share/lua/5.1
KEY_DIR ?= etc/ssl

.PHONY: all install clean distclean

all install:
ifeq ($(PLATFORM),Serpent)
	for i in *.lua; do \
		if [ "$$i" != "sas_client_start.lua" ]; then \
			install -m 0644 -D "$$i" "$(INSTALLDIR)/$(SAS_DIR)/$$i"; \
		fi; \
	done
ifeq ($(V_SKIN),$(filter $(V_SKIN),ATTeng NTC CASAeng CCIeng))
	# key & certs are only in ATTeng, NTC, CASAeng and CCIeng skin build
	for i in *.pem *.cert; do \
		install -m 0644 -D "$$i" "$(INSTALLDIR)/$(KEY_DIR)/$$i"; \
	done
	# install -m 0644 -D cpicertprivkey.key "$(INSTALLDIR)/$(KEY_DIR)/cpicertprivkey.key"
	install -m 0755 -D cpiSignature.template "$(INSTALLDIR)/etc/cdcs/conf/mgr_templates/cpiSignature.template"
	install -m 0755 -D sasCpiSignature.sh "$(INSTALLDIR)/bin/sasCpiSignature.sh"
endif
ifeq ($(V_SKIN),$(filter $(V_SKIN),NTC CASA CASAeng CCIeng CCI))
	# modem configuration for eud is currently in Myna (Casa skin build)
	install -m 0755 -D modem_conf_change.template "$(INSTALLDIR)/etc/cdcs/conf/mgr_templates/modem_conf_change.template"
endif
	install -m 0755 -D sas_client_start.lua "$(INSTALLDIR)/usr/bin/sas_client_start.lua"
	install -m 0755 -D sas_client_launcher.default "$(INSTALLDIR)/usr/bin/sas_client_launcher"
	install -m 0755 -D sas_client "$(INSTALLDIR)/etc/init.d/rc.d/sas_client"
	install -m 0755 -D sas_client.template "$(INSTALLDIR)/etc/cdcs/conf/mgr_templates/sas_client.template"
	install -m 0755 -D sas_route.sh "$(INSTALLDIR)/usr/bin/sas_route.sh"
	install -m 0755 -D sas_non_cbrs_traffic_control.sh "$(INSTALLDIR)/usr/bin/sas_non_cbrs_traffic_control.sh"
	# V variables code
	./install_v_variables.sh $(HBASE)/variant.sh . "$(INSTALLDIR)/$(SAS_DIR)/"
else ifeq ($(PLATFORM), Minions)
	# install -m 0644 -D cpicertprivkey.key "$(INSTALLDIR)/$(KEY_DIR)/cpicertprivkey.key"
	# do not ship private key (even test key), copy key manually to test if needed.
	install -m 0755 -D cpiSignature.template "$(INSTALLDIR)/etc/cdcs/conf/mgr_templates/cpiSignature.template"
	install -m 0755 -D sasCpiSignature.sh "$(INSTALLDIR)/bin/sasCpiSignature.sh"
else
	echo "sas client, not support platform $(PLATFORM)"
endif

clean distclean:
	echo "Nothing to clean for SAS"
