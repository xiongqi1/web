#
# Copyright (C) 2016 NetComm Wireless limited.
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

CFLAGS := -g -c -Wall -I$(CDCS_INCLUDE) -I$(CDCS_INCLUDE)/glib-2.0 -I$(CDCS_LIB)/glib-2.0/include -I$(CDCS_INCLUDE)/gio-unix-2.0 -I.. -Werror

# Uncomment the next line to enable debugs and asserts
DEBUG=1
#
ifdef DEBUG
CFLAGS += -DDEBUG
else
# disable asserts
CFLAGS += -DNDEBUG
endif

LDFLAGS := -L$(CDCS_LIB) -Wl,-rpath-link,$(CDCS_LIB) -lrdb -lglib-2.0 -lgio-2.0

GTEST_PROJECT = dsm_gatt_test
GTEST_SOURCES = dsm_gatt_test.c ../dsm_bt_utils.c
GTEST_OBJECTS = $(patsubst %.c,%.o,$(GTEST_SOURCES))

TAIDOC_PROJECT = taidoc_app
TAIDOC_SOURCES = taidoc_app.c taidoc_base.c ../dsm_bt_utils.c
TAIDOC_OBJECTS = $(patsubst %.c,%.o,$(TAIDOC_SOURCES))

CRAZY_TAIDOC_PROJECT = crazy_taidoc
CRAZY_TAIDOC_SOURCES = crazy_taidoc.c taidoc_base.c ../dsm_bt_utils.c
CRAZY_TAIDOC_OBJECTS = $(patsubst %.c,%.o,$(CRAZY_TAIDOC_SOURCES))

NONIN_PROJECT = nonin_app
NONIN_SOURCES = nonin_app.c ../dsm_bt_utils.c
NONIN_OBJECTS = $(patsubst %.c,%.o,$(NONIN_SOURCES))

DUMMY_SENSOR_PROJECT = bt_dummy_sensor
DUMMY_SENSOR_SOURCES = bt_dummy_sensor.c
DUMMY_SENSOR_OBJECTS = $(patsubst %.c,%.o,$(DUMMY_SENSOR_SOURCES))

DUMMY_SENSOR_READER_PROJECT = bt_dummy_sensor_reader
DUMMY_SENSOR_READER_SOURCES = bt_dummy_sensor_reader.c
DUMMY_SENSOR_READER_OBJECTS = $(patsubst %.c,%.o,$(DUMMY_SENSOR_READER_SOURCES))

DEPS = taidoc.h nonin.h bt_dummy_sensor.h ../dsm_bluetooth_ep.h ../dsm_btep_rpc.h ../dsm_gatt_rpc.h ../dsm_bt_utils.h

.PHONY : all
all: $(GTEST_PROJECT) $(TAIDOC_PROJECT) $(CRAZY_TAIDOC_PROJECT) $(NONIN_PROJECT) $(DUMMY_SENSOR_PROJECT) $(DUMMY_SENSOR_READER_PROJECT)

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(GTEST_PROJECT).* $(TAIDOC_PROJECT).* $(CRAZY_TAIDOC_PROJECT).* $(NONIN_PROJECT).* $(DUMMY_SENSOR_PROJECT).* $(DUMMY_SENSOR_READER_PROJECT).*; \
	rm -f $(GTEST_PROJECT) $(GTEST_PROJECT).a $(GTEST_OBJECTS); \
	rm -f $(TAIDOC_PROJECT) $(TAIDOC_PROJECT).a $(TAIDOC_OBJECTS); \
	rm -f $(CRAZY_TAIDOC_PROJECT) $(CRAZY_TAIDOC_PROJECT).a $(CRAZY_TAIDOC_OBJECTS); \
	rm -f $(NONIN_PROJECT) $(NONIN_PROJECT).a $(NONIN_OBJECTS); \
	rm -f $(DUMMY_SENSOR_PROJECT) $(DUMMY_SENSOR_PROJECT).a $(DUMMY_SENSOR_OBJECTS); \
	rm -f $(DUMMY_SENSOR_READER_PROJECT) $(DUMMY_SENSOR_READER_PROJECT).a $(DUMMY_SENSOR_READER_OBJECTS); \
	rm -f *~ ~*;

$(GTEST_OBJECTS) : $(DEPS)

$(GTEST_PROJECT): $(GTEST_OBJECTS)
	$(CC) $(GTEST_OBJECTS) $(LDFLAGS) -o $@

$(TAIDOC_OBJECTS) : $(DEPS)

$(TAIDOC_PROJECT): $(TAIDOC_OBJECTS)
	$(CC) $(TAIDOC_OBJECTS) $(LDFLAGS) -o $@

$(CRAZY_TAIDOC_OBJECTS) : $(DEPS)

$(CRAZY_TAIDOC_PROJECT): $(CRAZY_TAIDOC_OBJECTS)
	$(CC) $(CRAZY_TAIDOC_OBJECTS) $(LDFLAGS) -o $@

$(NONIN_OBJECTS) : $(DEPS)

$(NONIN_PROJECT): $(NONIN_OBJECTS)
	$(CC) $(NONIN_OBJECTS) $(LDFLAGS) -o $@

$(DUMMY_SENSOR_OBJECTS) : $(DEPS)

$(DUMMY_SENSOR_PROJECT): $(DUMMY_SENSOR_OBJECTS)
	$(CC) $(DUMMY_SENSOR_OBJECTS) $(LDFLAGS) -o $@

$(DUMMY_SENSOR_READER_OBJECTS) : $(DEPS)

$(DUMMY_SENSOR_READER_PROJECT): $(DUMMY_SENSOR_READER_OBJECTS)
	$(CC) $(DUMMY_SENSOR_READER_OBJECTS) $(LDFLAGS) -o $@
