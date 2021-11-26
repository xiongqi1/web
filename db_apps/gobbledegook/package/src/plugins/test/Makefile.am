
AM_CXXFLAGS = -fPIC -Wall -Wextra -std=c++17 -I../../ -I../../../include/ $(XML2_CFLAGS) $(DBUS_1_CFLAGS)
# There are a bunch of unused parameter warning from the lambda macros.
AM_CXXFLAGS += -Wno-unused-parameter
AM_LDFLAGS = -avoid-version $(XML2_CFLAGS) $(DBUS_1_CFLAGS)

pkglibdir = $(libdir)/ggk_plugins
LIBADD = ../../libggk.la

pkglib_LTLIBRARIES = libdevice.la \
		 libdeviceInformationService.la \
		 libcurrentConnectionService.la \
		 libvisibleCellService.la \
		 libledService.la \
		 librssiService.la \
		 libspeedTestService.la

libdevice_la_SOURCES = device.cpp

libdeviceInformationService_la_SOURCES = deviceInformationService.cpp

libcurrentConnectionService_la_SOURCES = currentConnectionService.cpp

libvisibleCellService_la_SOURCES = visibleCellService.cpp

libledService_la_SOURCES = ledService.cpp

librssiService_la_SOURCES = rssiService.cpp

libspeedTestService_la_SOURCES = speedTestService.cpp
