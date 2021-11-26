
AM_CXXFLAGS = -fPIC -Wall -Wextra -std=c++17 -I../../ -I../../../include/ $(XML2_CFLAGS) $(DBUS_1_CFLAGS)
# There are a bunch of unused parameter warning from the lambda macros.
AM_CXXFLAGS += -Wno-unused-parameter
AM_LDFLAGS = -avoid-version $(XML2_LIBS) $(DBUS_1_LIBS)

AUTOMAKE_OPTIONS = subdir-objects

pkglibdir = $(libdir)/ggk_plugins
LIBADD = ../../libggk.la

pkglib_LTLIBRARIES = libdevice.la \
		 libdeviceInformationService.la \
		 libcurrentConnectionService.la \
		 libvisibleCellService.la \
		 libledService.la \
		 librssiService.la \
		 libspeedTestService.la \
		 libcustomDesignedService.la \
		 libswivellingScanService.la

libdevice_la_SOURCES = ../saturn/device.cpp

libdeviceInformationService_la_SOURCES = ../saturn/deviceInformationService.cpp

libcurrentConnectionService_la_SOURCES = ../saturn/currentConnectionService.cpp

libvisibleCellService_la_SOURCES = ../saturn/visibleCellService.cpp

libledService_la_SOURCES = ../saturn/ledService.cpp

librssiService_la_SOURCES = ../saturn/rssiService.cpp

libspeedTestService_la_SOURCES = ../saturn/speedTestService.cpp

libcustomDesignedService_la_SOURCES = ../saturn/customDesignedService.cpp ../saturn/Ping.cpp

libswivellingScanService_la_SOURCES = swivellingScan.cpp
