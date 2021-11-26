#
# Alarm Daemon
#
# Alan Yates <alan.yates@netcomm.com.au>


.PHONY: all clean distclean install


all:


clean distclean:


install: all
	cp src/alarmclient.lua $(INSTALLDIR)/usr/share/lua/5.1
	cp src/alarmd.lua $(INSTALLDIR)/usr/bin
	cp src/alarm $(INSTALLDIR)/usr/bin
	cp src/alarms_mgr_$(PLATFORM).lua $(INSTALLDIR)/usr/bin/alarms_mgr.lua
