# Building facilities for Lua turbo web server
#
# Expects the following external variables to be set
#
# HBASE           = Platform build history
# INSTALLDIR      = Staging directory
# TEMPINSTALL     = Temporary installation directory


TEMPINSTALL_SERVER:=$(TEMPINSTALL)/usr/share/lua/5.1/webif

.PHONY: all clean distclean

all:
	mkdir -p $(TEMPINSTALL_SERVER)
	./install_extension.sh $(HBASE)/variant.sh ./_VARIANTS $(TEMPINSTALL_SERVER)
	cp support.lua $(TEMPINSTALL_SERVER)
	cp turbontc.lua $(TEMPINSTALL_SERVER)
	cp csrf_session.lua $(TEMPINSTALL_SERVER)
	cp shell_handler.lua $(TEMPINSTALL_SERVER)

clean distclean:
	rm -rf $(TEMPINSTALL_SERVER)
	rm -rf $(INSTALLDIR)/usr/share/lua/5.1/webif
