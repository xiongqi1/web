#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

RDBTYPE := normal
#RDBTYPE := fake_stdout

ifeq ($(RDBTYPE),normal)
  RDBOBJ:=rdb.o
  RDBLIB:=-lrdb
  ECFLAGS:=
else
  RDBOBJ:=rdb_$(RDBTYPE).o
  RDBLIB:=
  ECFLAGS:=-DNR5G
endif

CFLAGS:=-c -Wall -Werror -Wfatal-errors
ifdef CDCS_INCLUDE_OPTS
CFLAGS+=$(CDCS_INCLUDE_OPTS) -I$(CDCS_INCLUDE)/asn1c
else
CFLAGS =-I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/asn1c/ -I$(PROPRIETARY_INCLUDE) -I$(PROPRIETARY_INCLUDE)/qc
endif

LDFLAGS:=
ifdef CDCS_LIB_OPTS
# directly use cdcs library options
LDFLAGS+=$(CDCS_LIB_OPTS) -Wl,-rpath-link,$(INSTALLDIR)/usr/lib
else
LDFLAGS=-L$(CDCS_LIB) -L$(PROPRIETARY_LIB)
endif

ifdef DEBUG
	CFLAGS += -g -O0
	STRIP  := true
else
	CFLAGS += -Os
endif

#############################################################################
# convert V variables to compile flags
#############################################################################
DEFINES_CFLAGS:=
# measurement report variant
DEFINES_CFLAGS+=-DV_RF_MEASURE_REPORT_$(V_RF_MEASURE_REPORT) $(ECFLAGS)
# NR5G support
DEFINES_CFLAGS+=$(if $(filter $(V_MODULE),rg500q-ea rm500q-gl rm502q-gl),-DNR5G)
$(info DEFINES_CFLAGS=$(DEFINES_CFLAGS))
CFLAGS+=$(DEFINES_CFLAGS)

.PHONY: all install clean

all:
	echo "do not call directly, use Netcomm build tool"

qualcommNg.done:
	pushd ./RouterNg/QualcommNg && make install && popd
	touch qualcommNg.done

webclient.done:
	# next line for local build, not required if pulling output from concourse
	#pushd ./RouterNg/webclient && npm install --loglevel silent && ./install.sh && popd
	touch webclient.done

routerng.done:
	mkdir -p $(INSTALLDIR)/usr/bin/routerNg
	mkdir -p $(INSTALLDIR)/etc/routerNg
	# next 2 lines for local build, not required if pulling output from concourse
	#pushd ./RouterNg/node && npm install --loglevel silent && ./install.sh && popd
	#tar -xf ./RouterNg/node/ng.tar.gz -C $(INSTALLDIR)/usr/bin/routerNg
	tar -xf ./output-artifacts/ng.tar.gz -C $(INSTALLDIR)/usr/bin/routerNg
	mv $(INSTALLDIR)/usr/bin/routerNg/config/* $(INSTALLDIR)/etc/routerNg
	touch routerng.done

RouterNg:
	git clone git@gitlab.corp.netcomm.com.au:CliffordJ/RouterNg
output-artifacts:
	git clone git@gitlab.corp.netcomm.com.au:nodejs/output-artifacts
nodejs-artifacts:
	git clone git@gitlab.corp.netcomm.com.au:nodejs/nodejs-artifacts

install: RouterNg output-artifacts nodejs-artifacts webclient.done routerng.done qualcommNg.done
	install -v -m 0755 nodejs-artifacts/install/bin/node $(INSTALLDIR)/bin/node
	mkdir -p $(INSTALLDIR)/etc/initscripts
	install -v -m 0755 syslog $(INSTALLDIR)/etc/initscripts
	install -v -m 0644 syslog-startup.conf $(INSTALLDIR)/etc
	mkdir -p $(INSTALLDIR)/lib/systemd/system
	install -v -m 0644 ./RouterNg/node/routerNg.service $(INSTALLDIR)/lib/systemd/system/
	mkdir -p $(INSTALLDIR)/etc/systemd/system/multi-user.target.wants
	ln -sf  /lib/systemd/system/routerNg.service $(INSTALLDIR)/etc/systemd/system/multi-user.target.wants/routerNg.service

distclean clean:
	# remove targets
	pushd ./RouterNg/QualcommNg && make clean && popd
