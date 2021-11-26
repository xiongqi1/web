# Build a shared library (libggk.la)
lib_LTLIBRARIES = libggk.la
libggk_la_LDFLAGS = -avoid-version $(XML2_LIBS) $(DBUS_1_LIBS)
libggk_la_CXXFLAGS = -fPIC -Wall -Wextra -std=c++17 -I../include/ $(GLIB_CFLAGS) $(GIO_CFLAGS) $(GOBJECT_CFLAGS) $(XML2_CFLAGS) $(DBUS_1_CFLAGS)
libggk_la_SOURCES = CasaData.cpp \
                   CasaData.h \
                   CasaDbus.cpp \
                   CasaDbus.h \
                   DBusInterface.cpp \
                   DBusInterface.h \
                   DBusMethod.cpp \
                   DBusMethod.h \
                   DBusObject.cpp \
                   DBusObject.h \
                   DBusObjectPath.h \
                   GattCharacteristic.cpp \
                   GattCharacteristic.h \
                   GattDescriptor.cpp \
                   GattDescriptor.h \
                   GattInterface.cpp \
                   GattInterface.h \
                   GattProperty.cpp \
                   GattProperty.h \
                   GattService.cpp \
                   GattService.h \
                   GattUuid.h \
                   Globals.h \
                   Gobbledegook.cpp \
                   ../include/Gobbledegook.h \
                   HciAdapter.cpp \
                   HciAdapter.h \
                   HciSocket.cpp \
                   HciSocket.h \
                   Housekeeping.cpp \
                   Housekeeping.h \
                   Init.cpp \
                   Init.h \
                   Logger.cpp \
                   Logger.h \
                   Mgmt.cpp \
                   Mgmt.h \
                   Server.cpp \
                   Server.h \
                   ServerUtils.cpp \
                   ServerUtils.h \
                   standalone.cpp \
                   TickEvent.h \
                   Utils.cpp \
                   Utils.h \
                   Base64.cpp Base64.h \
                   HexMask.cpp HexMask.h

# Build our standalone server (linking statically with libggk.a, linking dynamically with GLib)
standalone_CXXFLAGS = -fPIC -Wall -Wextra -std=c++17 -I../include/ $(XML2_CFLAGS) $(DBUS_1_CFLAGS)
bin_PROGRAMS = standalone
standalone_SOURCES = standalone.cpp
standalone_LDADD = libggk.la
standalone_LDFLAGS = $(XML2_LIBS) $(DBUS_1_LIBS)
standalone_LDLIBS = $(GLIB_LIBS) $(GIO_LIBS) $(GOBJECT_LIBS)

# Build plugins
SUBDIRS = . plugins
