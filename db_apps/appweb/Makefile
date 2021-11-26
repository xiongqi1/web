.EXPORT_ALL_VARIABLES:

CDIR=$(shell pwd)

# Set this to install somewhere else
INSTALLDIR ?= $(CDIR)/target

# Some parts are installed in prefix /, others in /usr. Set
# PREFIXDIR to move the /usr stuff somewhere else.
PREFIXDIR ?= $(INSTALLDIR)/usr

SRC := $(CDIR)/appweb-src

ifneq ($(CDCS_INCLUDE),)
	CFLAGS := -I$(CDCS_INCLUDE)
	LDFLAGS := -L$(CDCS_LIB)
endif



ifeq ($(PLATFORM),Arachnid)

	TARGET := ubicom32-linux-uclibc
	HOST:= $(TARGET)

else ifeq ($(PLATFORM),Platypus2)

	TARGET := mipsel-linux
	HOST:= $(TARGET)

ifeq ($(V_HTTPS),y)
	EXTRA_CONFIG := --disable-access-log --disable-config-save --disable-ipv6 --without-aspnet --disable-assert \
	--disable-decimal --disable-doc --disable-legacy-api --disable-samples --disable-test --disable-shared \
	--with-openssl=builtin --with-openssl-dir=$(CDCS_LIB) --with-dir=builtin \
	--with-openssl-libpath=$(CDCS_LIB) --with-openssl-libs="ssl crypto"
else
	EXTRA_CONFIG := --without-ssl --disable-access-log --disable-config-save --disable-ipv6 --without-aspnet \
	--disable-assert --disable-decimal --disable-doc --disable-legacy-api --disable-samples --enable-shared-libc \
	--enable-squeeze --disable-test --disable-shared
endif

	CFLAGS+=-DMPR_USE_PIPES=1 -DWORKAROUND_FROZEN_BREAKOUT

else ifeq ($(PLATFORM),Bovine)

	TARGET := $(BUILD_TARGET)
	HOST:= arm-softfloat-linux-gnueabi

ifeq ($(V_HTTPS),y)
	ifneq ($(MODE),recovery)
		EXTRA_CONFIG := --with-openssl=loadable \
		--with-openssl-dir=$(CDCS_LIB) \
		--with-openssl-libpath=$(CDCS_LIB) \
		--with-openssl-libs="ssl crypto"
	else
		EXTRA_CONFIG := --without-ssl
	endif
else
	EXTRA_CONFIG := --without-ssl
endif

else ifeq ($(PLATFORM),Antelope)

	TARGET := $(BUILD_TARGET)
	HOST:= $(TARGET)
	EXTRA_CONFIG := --with-ssl=builtin

else ifeq ($(PLATFORM),Serpent)

	TARGET := $(BUILD_TARGET)
	HOST:= $(TARGET)

ifeq ($(V_HTTPS),y)
	ifneq ($(MODE),recovery)
		EXTRA_CONFIG := --with-openssl=loadable \
		--with-openssl-dir=$(CDCS_LIB) \
		--with-openssl-libpath=$(CDCS_LIB) \
		--with-openssl-libs="ssl crypto"
	else
		EXTRA_CONFIG := --without-ssl
	endif
else
	EXTRA_CONFIG := --without-ssl
endif

ifeq ($(V_IPV6),none)
	EXTRA_CONFIG += --disable-ipv6
else
	EXTRA_CONFIG += --enable-ipv6
endif

else
# We allow clean and distclean targets through, even without a valid PLATFORM
ifeq (,$(filter clean distclean,$(MAKECMDGOALS)))
$(error PLATFORM '$(PLATFORM)' is unsupported)
endif
endif

# disable all HTTP methods except for GET/POST for all platforms/variants
# use FEATURE_SECURE_APPWEB to keep original source codes for future upgrading
CFLAGS+=-DFEATURE_SECURE_APPWEB
EXTRA_CONFIG += --without-put

VARS := CC=$(TARGET)-gcc AR=$(TARGET)-ar LD=$(TARGET)-ld LDXX=$(TARGET)-g++ STRIP=$(TARGET)-strip CFLAGS="$(CFLAGS) -Wno-write-strings" LDFLAGS="$(LDFLAGS)"

CONFIG := --host=$(HOST) --build=i386-pc-linux \
	--defaults=cdcs --without-admin --without-egi --without-dir --webDir=/www --port=80 --sslPort=443 \
	$(EXTRA_CONFIG)

.PHONY:all
all: hist.installed
install: hist.installed

PATCHLIST := appweb.priv.patch

hist.configured: $(shell find -newer hist.installed 2>/dev/null)
	(cd $(SRC) && rm -f buildConfig.* )
	(cd $(SRC) && $(VARS) ./configure $(CONFIG) )
	touch $(@)

hist.built: hist.configured
	(cd $(SRC) && $(VARS) make )
	touch $(@)

hist.installed: hist.built
	mkdir -p install
	$(MAKE) -C $(SRC) ROOT_DIR=$(CDIR)/install DISABLE_PRIV=YES install
	# Remove manual pages
	rm -rf $(CDIR)/install/usr/share/man
	# Create directories
	mkdir -p $(INSTALLDIR)/etc/appweb
	mkdir -p $(PREFIXDIR)/lib/appweb/modules
	mkdir -p $(PREFIXDIR)/sbin
	mkdir -p $(INSTALLDIR)/var/log/appweb
	mkdir -p $(INSTALLDIR)/var/cache/ldconfig
	mkdir -p $(INSTALLDIR)/www

	# Copy
	rm -f $(INSTALLDIR)/etc/appweb/appweb.conf
	cp -r    $(CDIR)/install/etc/appweb/* $(INSTALLDIR)/etc/appweb/
ifeq ($(PLATFORM),Platypus2)
	cp -dpf  $(SRC)/appweb/conf/platypus2.conf $(INSTALLDIR)/etc/appweb/appweb.conf
else
ifeq ($(V_PRODUCT),ntc_140attdev)
	cp -dpf  $(SRC)/appweb/conf/bridgeonly.conf $(INSTALLDIR)/etc/appweb/appweb.conf
else
	cp -dpf  $(SRC)/appweb/conf/cdcs.conf $(INSTALLDIR)/etc/appweb/appweb.conf
endif
	mkdir -p $(INSTALLDIR)/etc/appweb/applications
endif

ifeq ($(V_HTTPS_STRONG_CIPHERS),y)
	# apply strong ciphers
	cp -dpf $(SRC)/appweb/conf/strong.conf $(INSTALLDIR)/etc/appweb/appweb.conf
endif

ifneq ($(filter $(V_CUSTOM_FEATURE_PACK),myna cci myna_lite),)
	# Myna is RO rootfs so create a symlink to modify the config file
	mv $(INSTALLDIR)/etc/appweb/appweb.conf $(INSTALLDIR)/etc/appweb/appweb.conf.org
	ln -s /usr/local/cdcs/conf/appweb.conf $(INSTALLDIR)/etc/appweb/appweb.conf
endif

	-cp -r    $(CDIR)/install/usr/lib/appweb/modules/*.so $(PREFIXDIR)/lib/appweb/modules/
	-cp -dpf  $(CDIR)/install/usr/lib/libappweb.so* $(PREFIXDIR)/lib

	cp -r    $(CDIR)/install/usr/sbin/* $(PREFIXDIR)/sbin

	touch $(INSTALLDIR)/etc/ld.so.conf && ldconfig -r $(INSTALLDIR)

	touch $(@)

# These files are leftovers after make clean in appweb-src
CLEANLIST := buildConfig.sh buildConfig.make buildConfig.h build/.buildConfig.settings build/.buildConfig.cache \
	build/.buildConfig.args .buildStarted appweb/appweb samples/C/simpleServer/simpleServer \
	samples/C/simpleEsp/simpleEsp samples/C/pollEventLoop/pollEventLoop \
	samples/C/selectEventLoop/selectEventLoop samples/C++/simpleServer/simpleServer \
	samples/C++/simpleEsp/simpleEsp samples/C++/pollEventLoop/pollEventLoop \
	samples/C++/simpleClient/simpleClient samples/C++/configByApi/configByApi \
	samples/C++/selectEventLoop/selectEventLoop appweb/install.conf appweb/appweb.conf \
	samples/C/simpleEgi/simpleEgi samples/C++/simpleEgi/simpleEgi http/modules/netcomm/drvinfo.h \
	tools/dsi tools/pp tools/genDepend tools/incPatch

clean distclean:
	-(cd $(SRC) && make clean )
	(cd $(SRC) && find -name "*.dep" -delete )
	rm -fr $(SRC)/lib/*-linux
	rm -fr $(SRC)/obj/*-linux $(SRC)/obj/classes $(SRC)/obj/crushed $(SRC)/obj/verified
	rm -fr $(SRC)/bin/*-linux
	(cd $(SRC) && rm -f $(CLEANLIST))

	rm -fr install target
	rm -f hist.installed hist.built hist.configured
