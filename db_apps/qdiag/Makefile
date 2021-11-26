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
DEFINES_CFLAGS+=$(if $(filter $(V_QDIAG_NR5G),y),-DNR5G)
$(info DEFINES_CFLAGS=$(DEFINES_CFLAGS))
CFLAGS+=$(DEFINES_CFLAGS)

.PHONY: all install clean

all:
	echo "do not call directly, use Netcomm build tool"

unit_test_pdn_mgr: pdn_mgr.c
	#$(CC) $^ $(LDFLAGS) -g -DCONFIG_UNIT_TEST -o $@
	gcc $^ $(LDFLAGS) -g -DCONFIG_UNIT_TEST -o $@

unit_test_lte_emm_esm: lte_nas_parser.c
	gcc $^ $(LDFLAGS) -g -DCONFIG_UNIT_TEST -o $@

pdn_mgr.o: pdn_mgr.h

gcell.o: gcell.h

def.o: def.h

hash.o: hash.h

$(RDBOBJ): rdb.h

asn1c_helper.o: asn1c_helper.h

qdiagd.o: logcodes.h avg.h def.h

lte_nas_parser.o: lte_nas_parser.h

eps_bearer_rdb.o: eps_bearer_rdb.h

qdiagd: avg.o qdiagd.o gcell.o hash.o asn1c_helper.o $(RDBOBJ) lte_nas_parser.o pdn_mgr.o eps_bearer_rdb.o def.o
	$(CC) $^ $(LDFLAGS) $(RDBLIB) -lgthread-2.0 -ldiag -lglib-2.0 -lasn1parser -lpthread -ltime_genoff -o $@
	$(STRIP) $@

ifeq ($(V_CBRS_SAS), y)
qdiagtxctrl: qdiagtxctrl.o
	$(CC) $^ $(LDFLAGS) -lrdb -lgthread-2.0 -ldiag -lglib-2.0 -lpthread -ltime_genoff -o $@
	$(STRIP) $@

install: qdiagd qdiagtxctrl
	install -m 0755 qdiagtxctrl $(INSTALLDIR)/bin/qdiagtxctrl
else
install: qdiagd
endif
	install -m 0755 qdiagd $(INSTALLDIR)/bin/qdiagd
	$(STRIP) $(INSTALLDIR)/bin/qdiagd
	# install scripts
ifeq ($(V_STARTUP_TYPE),systemd)
	# fsextra requires all of systemd startup services and scripts
else
	install -m 0755 "scripts/qdiagd_launcher.default" $(INSTALLDIR)/bin/qdiagd_launcher
	-install -m 0755 "scripts/qdiagd_launcher.$(V_SKIN)" $(INSTALLDIR)/bin/qdiagd_launcher
	install -d $(INSTALLDIR)/etc/init.d/rc.d
	install -m 0755 scripts/qdiag $(INSTALLDIR)/etc/init.d/rc.d/qdiag
	ln -sf  ../rc.d/qdiag $(INSTALLDIR)/etc/init.d/rc3.d/S305qdiag
endif

distclean clean:
	# remove targets
	rm -f qdiagd qdiagtxctrl *.o *~ scripts/*~ unit_test_pdn_mgr
