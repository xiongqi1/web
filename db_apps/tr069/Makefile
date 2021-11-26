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

TARGET_DIRS := cwmpd cwmp-crd #cwmp-xfrd

export CROSS_COMPILE INSTALLDIR KERNELINC CDCS_INCLUDE CDCS_LIB PLATFORM VARIANT

.PHONY: all install clean distclean
all: $(patsubst %,%_build,$(TARGET_DIRS))

install: $(patsubst %,%_install,$(TARGET_DIRS))

build: $(patsubst %,%_build,$(TARGET_DIRS))

clean: $(patsubst %,%_clean,$(TARGET_DIRS))
	# For build system, force rebuild
	touch Makefile

distclean: $(patsubst %,%_distclean,$(TARGET_DIRS))
	# For build system, force rebuild
	touch Makefile

%_build:
	$(info ##### build $@)
	(cd $(patsubst %_build,%,$@) && $(MAKE) build)

%_install:
	$(info ##### install $@)
	(cd $(patsubst %_install,%,$@) && $(MAKE) install)

%_clean:
	$(info ##### clean $@)
	(cd $(patsubst %_clean,%,$@) && $(MAKE) clean)

%_distclean:
	$(info ##### distclean $@)
	(cd $(patsubst %_clean,%,$@) && $(MAKE) distclean)
