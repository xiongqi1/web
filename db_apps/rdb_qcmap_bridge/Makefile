PACKAGE_TOP_DIR:=$(shell /bin/pwd)
PACKAGE_DIR:=package
PACKAGE_STAGING_DIR:=staging
PACKAGE_MAKE=$(MAKE_ENV) $(MAKE) -C $(PACKAGE_DIR) $(MAKE_OPT)
PACKAGE_CONFIGURATE=cd $(PACKAGE_DIR) && $(CONFIGURATE_ENV) ./configure $(CONFIGURATE_OPT)

# enable debuug for QMI packet
#CDCS_INCLUDE_OPTS+=DDEBUG_DUMP_QMI

# enable debug
CONFIGURATE_OPT+=--enable-debug

# configure
CONFIGURATE_ENV:=PKG_CONFIG_SYSROOT_DIR=$(SBASE)/staging_l_qcle \
	PKG_CONFIG_PATH= \
	PKG_CONFIG_LIBDIR=$(CDCS_PKG_CONFIG_LIBDIR) \
	LDFLAGS="$(CDCS_LIB_OPTS) -Wl,-rpath-link=$(SBASE)/staging_basefs/rootfs/usr/lib" \
	CXXFLAGS="$(CDCS_INCLUDE_OPTS)"

CONFIGURATE_OPT+= \
	--prefix=/usr \
	--host=$(BUILD_TARGET) \
	--target=$(BUILD_TARGET) \
	--with-sysroot=$(SBASE)/staging_l_qcle


# make
NPROCS:=$(shell grep -c ^processor /proc/cpuinfo)
MAKE_ENV:=
MAKE_OPT:=$(if $(NPROCS),-j$(NPROCS))

.PHONY: all build install

all: install

$(PACKAGE_DIR)/Makefile:
	@echo "CONFIGURATE_ENV=$(CONFIGURATE_ENV)"
	$(PACKAGE_CONFIGURATE)
	touch $@

build: $(PACKAGE_DIR)/Makefile
	$(PACKAGE_MAKE)

install: build
	$(PACKAGE_MAKE) SHELL= DESTDIR=$(PACKAGE_TOP_DIR)/$(PACKAGE_STAGING_DIR) install-strip
	# cherry-pick files to install
	cp -ar $(PACKAGE_STAGING_DIR)/. $(STAGING)

clean:
	$(PACKAGE_MAKE) $@

distclean:
	$(PACKAGE_MAKE) maintainer-clean
	rm -fr $(PACKAGE_STAGING_DIR)
