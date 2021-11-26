#
# Top-level Makefile for all database applications
#
# Expects
#	INSTALLDIR      = Root of staging tree
#	CROSS_COMPILE   = Cross compiler prefix, e.g. (arm-linux-)
#	CDCS_INCLUDE    = CDCS include files (libraries, etc.)
#	CDCS_LIB        = CDCS libraries
#
# rdb_lib is the common dependency for all projects and get treated
# separately.
#
$(info --------------------------------- )
$(info PATH=$(PATH))
$(info CROSS_COMPILE=$(CROSS_COMPILE))
$(info INSTALLDIR=$(INSTALLDIR))
$(info CDCS_INCLUDE=$(CDCS_INCLUDE))
$(info CDCS_LIB=$(CDCS_LIB))
$(info --------------------------------- )

# Goes into each subdir in turn and runs make install.
# This should be set by the platform makefile, so won't have any effect
PROJLIST ?= appweb cnsmgr connection_mgr rdb_manager rdb_tool webif switchd \
	    rp-pppoe dbcfg_export pots_bridge simple_at_manager modem_emulator \
	    gps_port_manager supervisor mtd_statistics system_monitor sms_tools \
	    tr069 tr143_ping tr143_http alarmd ethoamd cgipipe timedaemon sysmonitor \
	    voltage_monitor mini_conntrack_tools qmimgr dispd module_fw_dloader turbontc \
	    port_activity_monitor rdb_watchdog

export CROSS_COMPILE
export BUILD_TARGET
export MODE

export CDCS_INCLUDE
export CDCS_LIB
SCAT_ARGS:=$(CDCS_LIBSTAGE) -s $(CROSS_COMPILE)strip $(SCAT_ARGS)
export SCAT_ARGS
export DEFPREFIX

.PHONY:all install packages clean distclean
# Build all projects - end result are the project.done files
all install: checkrev $(patsubst %,%.done,$(PROJLIST))
packages: checkrev $(patsubst %,%.package,$(PKGLIST))
# Create dependency lists (make is automatically recursing to include them)
# If the project doesn't supply a global dependency list, we create one
# with all files in the project. Only files under SVN are considered.
# NOTE: this will
%.dep: svnrevision
	if [ -e "$*/$*.dep" ]; then \
		cp "$*/$*.dep" $(@); \
	else \
		files_under_svn.sh "$*" | sed "s/^/$*\//g" | xargs echo "$*.done: " >$@ ; \
	fi
# Include the dependency files. The '-' avoids spurious warnings.
# Make will rescan the Makefile after all dependency files are created.
-include $(patsubst %,%.dep,$(PROJLIST))

# In case we are on an exported tree, we don't want to fail this - ignoring errors

.PHONY:checkrev
checkrev:
	-V=`svn info | sed -n 's/^Revision: //p'`; O=`cat svnrevision`; \
	echo "$$V || $$O"; \
	if [ "$$V" != "$$O" ]; then echo "$$V" >svnrevision; fi

svnrevision:
	-svn info | sed -n 's/^Revision: //p' >$@
	touch $@

# This one creates a rule for each project.
# The targets are the project.done files. Further dependencies come from
# the included PROJECT.dep files.
%.done: $(MFILE)
	-@echo "[make] $@: $^"
	( cd $(patsubst %.done,%,$(@)) && \
	INSTALLDIR=$(INSTALLDIR) MODE=$(MODE) gen_makefile_script.sh create . $(MAKE) install ) && \
	touch $(@);

%.package: $(MFILE)
	@echo "[make] $@: $^"
	( cd $(patsubst %.package,%,$(@)) && \
	INSTALLDIR=$(INSTALLDIR) MODE=$(MODE) $(MAKE) package ) && \
	touch $(@);

# Stuff that is not properly cleaned by project makefiles
EXTRACLEAN := Internationalization/lang Internationalization/BuildFiles Internationalization/webif_spec.h gpiomon/gpiomon

clean distclean:
	for i in $(PROJLIST); do \
	(cd $$i && INSTALLDIR=$(INSTALLDIR) MODE=$(MODE) gen_makefile_script.sh delete . $(MAKE) clean ); \
	done
	rm -f *.done *.dep *.package
	# Remove leftovers
	rm -r -f $(EXTRACLEAN)
	rm -f svnrevision
