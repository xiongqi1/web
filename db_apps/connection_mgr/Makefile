
#
#  This is the first step for a migrate procedure from wwan_daemon to
# connection_mgr. wwan_daemon is obsolete.
#

# FIXME: xmlgen uses these variables. There should be a specific
# xmlgen related V_* variable for this.
export PLATFORM V_BOARD V_SKIN V_CELL_NW

ifeq ($(PLATFORM),Platypus)
	CONNECTION_MGR_PROJECT := wwan_daemon
else
	CONNECTION_MGR_PROJECT := connection_mgr
endif

.PHONY: all clean distclean install

all:
	make all -C $(CONNECTION_MGR_PROJECT)
	make all -C xmls
	
clean:
	make clean -C wwan_daemon
	make clean -C connection_mgr
	make clean -C xmls
	rm -f *.o
	
distclean:
	make distclean -C wwan_daemon
	make distclean -C connection_mgr
	make distclean -C xmls
	rm -f *.o

install:
	make install -C $(CONNECTION_MGR_PROJECT)
	make install -C xmls
	# copy scripts
	cp ./scripts/module_powerup $(INSTALLDIR)/usr/bin/module_powerup
	cp ./scripts/cns_profile $(INSTALLDIR)/usr/bin/cns_profile
	cp ./scripts/cns_ifup $(INSTALLDIR)/usr/bin/cns_ifup
	ln -sf /usr/bin/cns_ifup $(INSTALLDIR)/usr/bin/cns_ifdn
