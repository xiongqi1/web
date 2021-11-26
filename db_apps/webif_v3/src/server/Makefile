# Build facilities for web servers
#
# Expected following external variables to be set
#
# HBASE           = Platform build history
# INSTALLDIR      = Staging directory
# TEMPINSTALL     = Temporary installation directory

.PHONY: all clean distclean

include $(HBASE)/variant.mk

ifneq ($(V_WEBIF_SERVER),)
ifneq ($(wildcard $(V_WEBIF_SERVER)/.),)
all: $(V_WEBIF_SERVER) $(shell find $(V_WEBIF_SERVER) -type f)
	cd $(V_WEBIF_SERVER) && make HBASE='$(HBASE)' TEMPINSTALL='$(TEMPINSTALL)'

distclean:
	cd $(V_WEBIF_SERVER) && make HBASE='$(HBASE)' TEMPINSTALL='$(TEMPINSTALL)' INSTALLDIR='$(INSTALLDIR)' distclean

else
$(info no server directory for V_WEBIF_SERVER=$(V_WEBIF_SERVER)
all:

distclean:

endif

else
all:

distclean:

endif

