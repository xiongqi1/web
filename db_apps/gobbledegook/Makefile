PACKAGE_TOP_DIR:=$(shell /bin/pwd)
PACKAGE_DIR:=package
PACKAGE_STAGING_DIR:=staging
PACKAGE_MAKE=$(MAKE_ENV) $(MAKE) -C $(PACKAGE_DIR) $(MAKE_OPT)
PACKAGE_CONFIGURATE=cd $(PACKAGE_DIR) && $(CONFIGURATE_ENV) ./configure $(CONFIGURATE_OPT)

TARGET:=$(BUILD_TARGET)
TEMPINSTALL:=$(PACKAGE_TOP_DIR)/$(PACKAGE_STAGING_DIR)


ifdef CDCS_INCLUDE_OPTS
	CDCS_GLIB_INC_OPTS:=$(CDCS_INCLUDE_OPTS:%=%/glib-2.0)
	CDCS_GLIB_INC_OPTS+=$(CDCS_INCLUDE_OPTS:%=%/glib-2.0/include)

	CDCS_DBUS_INC_OPTS:=$(CDCS_INCLUDE_OPTS:%=%/dbus-1.0)
	CDCS_DBUS_INC_OPTS+=$(CDCS_INCLUDE_OPTS:%=%/dbus-1.0/include)
	CDCS_DBUS_INC_OPTS+=$(CDCS_INCLUDE_OPTS:%/usr/include=%/usr/lib/dbus-1.0/include)
	CDCS_DBUS_INC_OPTS+=$(CDCS_INCLUDE_OPTS:%_openwrt/usr/include=%_openwrt/usr/lib/dbus-1.0/include)
	CDCS_DBUS_INC_OPTS+=$(CDCS_INCLUDE_OPTS:%_openwrt/include=%_openwrt/include/libxml2)
endif


# configure
CONFIGURATE_ENV:= \
	PKG_CONFIG_LIBDIR=$(CDCS_PKG_CONFIG_LIBDIR) \
	PKG_CONFIG_PATH="$(CDCS_LIBSTAGE)/lib/pkgconfig" \
	GIO_CFLAGS="-pthread -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include" \
	GLIB_CFLAGS="-I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include" \
	GOBJECT_CFLAGS="-I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include" \
	XML2_CFLAGS="-I$(CDCS_INCLUDE)/libxml -I$(CDCS_LIB)/libxml/include" XML2_LIBS="-lxml2" \
	DBUS_1_CFLAGS="-I$(CDCS_INCLUDE)/dbus-1.0/ -I$(CDCS_LIB)/dbus-1.0/include" \
	LDFLAGS="$(CDCS_LIB_OPTS) -ldl -ldbus-1 -lxml2 -lestd" \
	CXXFLAGS="$(CDCS_INCLUDE_OPTS) \
				-I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include \
				-I$(CDCS_INCLUDE)/dbus-1.0 -I$(CDCS_LIB)/dbus-1.0/include \
				$(CDCS_GLIB_INC_OPTS) $(CDCS_DBUS_INC_OPTS)"

CONFIGURATE_OPT+= \
	--prefix=/usr \
	--host=$(TARGET) \
	--target=$(TARGET) \
	--enable-casa_build

ifeq ($(V_GATT_SERVER_PLUGIN),mars)
CONFIGURATE_OPT+=--with-service_plugin="mars"
else ifeq ($(V_GATT_SERVER_PLUGIN),saturn)
CONFIGURATE_OPT+=--with-service_plugin="saturn"
else ifeq ($(V_GATT_SERVER_PLUGIN),mrswivel)
CONFIGURATE_OPT+=--with-service_plugin="mrswivel"
else
CONFIGURATE_OPT+=--with-service_plugin="test"
endif

# make
NPROCS:=$(shell grep -c ^processor /proc/cpuinfo)
MAKE_ENV:=
MAKE_OPT:=$(if $(NPROCS),-j$(NPROCS))

.PHONY: all build install

all: install

$(PACKAGE_DIR)/aclocal.m4:
	cd $(PACKAGE_DIR) && autoreconf -v -f -i -s
	touch $@

$(PACKAGE_DIR)/Makefile: $(PACKAGE_DIR)/aclocal.m4
	@echo "CONFIGURATE_ENV=$(CONFIGURATE_ENV)"
	$(PACKAGE_CONFIGURATE)
	touch $@

build: $(PACKAGE_DIR)/Makefile
	$(PACKAGE_MAKE)

install: build
	$(PACKAGE_MAKE) SHELL= DESTDIR=$(TEMPINSTALL) install-strip
	mv $(TEMPINSTALL)/usr/bin/standalone $(TEMPINSTALL)/usr/bin/ggk-standalone
	# cherry-pick files to install
	scatter_install.sh -s $(TARGET)-strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" \
		-b -l -x "*.la" -L /usr/lib/ggk_plugins

clean:
	$(PACKAGE_MAKE) $@

distclean:
	$(PACKAGE_MAKE) maintainer-clean
	rm -fr $(TEMPINSTALL)
