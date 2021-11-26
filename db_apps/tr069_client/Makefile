#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# MODE           = Build mode (for bovine: recovery, main)
# SBASE          = Source tree base directory

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Bovine)
	CROSS_COMPILE ?= arm-linux-
else ifeq ($(PLATFORM),Avian)
	CROSS_COMPILE ?= arm-linux-
else ifeq ($(PLATFORM),Platypus)
	CROSS_COMPILE ?= mipsel-linux-
else ifeq ($(PLATFORM),882)
	CROSS_COMPILE ?= arm-linux-
else ifeq ($(PLATFORM),ubicom32)
	CROSS_COMPILE ?= ubicom32-linux-uclibc-
else
	CROSS_COMPILE ?= mipsel-linux-
endif	

ifeq ($(PLATFORM),Platypus)
INSTALLDIR    ?= ../../../romfs
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib
KERNELINC     ?= $(shell readlink -f ../../../linux-2.6.21.x/include)
VARIANT 	  ?= $(shell cat "../../../variant.last_build" 2> /dev/null )
else
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
endif

TMP_IDIR      := $(shell readlink -f ./)/gsoap/TMPIDIR

TARGET_DIRS := gsoap dimclient4 host_bridge

export CROSS_COMPILE INSTALLDIR KERNELINC CDCS_INCLUDE CDCS_LIB PLATFORM VARIANT

.PHONY: all install clean distclean
all: $(patsubst %,%_build,$(TARGET_DIRS))

install: $(patsubst %,%_build,$(TARGET_DIRS))

clean: $(patsubst %,%_clean,$(TARGET_DIRS))
	# For build system, force rebuild
	touch Makefile

distclean: $(patsubst %,%_distclean,$(TARGET_DIRS))
	# For build system, force rebuild
	touch Makefile

%_build:
#	(make.sh $(patsubst %_build,%,$@))
	$(info ##### build $@)
#	(cd $(patsubst %_build,%,$@) && $(MAKE) all )
#	if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
#	@if [ "$@" == "gsoap" \; then echo "xxx"; fi;
	@if [ "$@" = "dimclient4_build" ]; then \
		echo "##### $@ : Makefile exists, dont need to configure"; \
		(cd $(patsubst %_build,%,$@) && $(MAKE) install;) \
	elif [ -f "./gsoap/Makefile" ]; then \
		echo "##### $@ : Makefile exists, dont need to configure"; \
		(cd $(patsubst %_build,%,$@) && $(MAKE) && $(MAKE) install  exec_prefix=$(TMP_IDIR) prefix=$(TMP_IDIR);) \
	else \
		echo "##### $@ : No Makefile, need to configure"; \
		(cd $(patsubst %_build,%,$@) && ./makemake && $(MAKE) && $(MAKE) install  exec_prefix=$(TMP_IDIR) prefix=$(TMP_IDIR);) \
	fi;

%_install:
#	(make.sh $(patsubst %_install,%,$@))
	(cd $(patsubst %_install,%,$@) && $(MAKE) install exec_prefix=$(TMP_IDIR))

gsoap_clean:
	(cd $(patsubst %_clean,%,$@) && test -f Makefile && $(MAKE) clean || true)
	test -z dimclient4/gsoap/soapcpp2 || rm -f dimclient4/gsoap/soapcpp2
	test -z gsoap/soapcpp2/wsdl2h || rm -f gsoap/soapcpp2/wsdl2h

gsoap_distclean:
	(cd $(patsubst %_distclean,%,$@) && $(MAKE) distclean)
	test -z $(TMP_IDIR) || rm -fr $(TMP_IDIR)
	test -z gsoap/autom4te.cache || rm -fr gsoap/autom4te.cache
	test -z dimclient4/gsoap/soapcpp2 || rm -f dimclient4/gsoap/soapcpp2
	test -z gsoap/soapcpp2/wsdl2h || rm -f gsoap/soapcpp2/wsdl2h
	test -z gsoap/Makefile.in || rm -f gsoap/Makefile.in
	test -z gsoap/soapcpp2/Makefile.in || rm -f gsoap/soapcpp2/Makefile.in

dimclient4_clean:
	(cd $(patsubst %_clean,%,$@) && $(MAKE) clean)

dimclient4_distclean:
	(cd $(patsubst %_distclean,%,$@) && $(MAKE) distclean)

host_bridge_clean:
	(cd $(patsubst %_clean,%,$@) && $(MAKE) clean)

host_bridge_distclean:
	(cd $(patsubst %_clean,%,$@) && $(MAKE) distclean)

