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
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CFLAGS := -I. -I./wmm -Wall -pedantic -std=c11 -Wextra -Os
CXXFLAGS := -I. -I./wmm -Wall -pedantic -std=c++11 -Wextra -Os
LDFLAGS := -lm -lrdb

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
CXXFLAGS += -I$(CDCS_INCLUDE)
endif

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

############## TARGETS ##############
##
## lark_sensors      : the service daemon that accesses the sensor and writes rdb
## lark_sensors_test : a binary that runs without rdb support for test/hardware team
## bhi_160           : an experimental binary with verbose output
##
############## TARGETS ##############

TARGETS := bhi_160 lark_sensors lark_sensors_test
INSTALL := lark_sensors lark_sensors_test ./wmm/WMM.COF

# WARNING: Incorrect chip revision firmware simply resets the chip again,
# about 100ms after firmware upload. The Bosch dev board appears to have
# an 'A' revision chip.
#
# NOTE: There is an SDK available that allows the creation of
# custom firmware. Using that, we could make firmware that
# has the correct sensors enabled by default, using the correct
# rotation of the chips and possibly using larger FIFOs and/or
# more compact communication protocol. It would also allow
# the sensor to control the power to the Onion module and
# use a ring buffer to provide old sensor data after boot.

# BHI160B stand-alone
#FIRMWARE:=Bosch_PCB_7183_di03_BMI160-7183_di03.2.1.11696_170103.fw
# BHI160B with BMM150
#FIRMWARE:=Bosch_PCB_7183_di03_BMI160_BMM150-7183_di03.2.1.11696_170103.fw

# BHI160A stand-alone
#FIRMWARE:=Bosch_PCB_7183_di01_BMI160-7183_di01.2.1.10836_170103.fw
# BHI160A with BMM150
FIRMWARE := ./imu/Bosch_PCB_7183_di01_BMI160_BMM150-7183_di01.2.1.10836_170103.fw

all: $(TARGETS) Makefile

bhi-160_fw.h: $(FIRMWARE)
	# Creating $@ from $^
	@echo "#ifndef _BHI160_FW_H" >$@
	@echo "#define _BHI160_FW_H" >>$@
	@echo "// Generated from $^" >>$@
	@echo "#include <stdint.h>" >>$@
	@echo "const uint8_t BHI160_FW_DATA[] = {" >>$@
	@# Hexdump adds unaligned hex numbers as spaces - sed deletes them again
	@hexdump -v -e '"	/* %04.4_ax */ " 16/1 "0x%02x," "\n"' "$^" \
		| sed 's/0x  ,//g' >>$@
	@echo "};" >>$@
	@#echo "const int BHI160_FW_LEN = $$(stat --printf='%s' '$^')" >>$@
	@echo "const int BHI160_FW_LEN = sizeof(BHI160_FW_DATA);" >>$@
	@echo "#endif // !_BHI160_FW_H" >>$@

i2c_linux.o: i2c_linux.h

#build for lark_sensors
bhi-160.o: bhi-160.h

#build for lark_sensors_test
bhi-160_test.o: bhi-160.c bhi-160_fw.h bhi-160.h
	$(CC) $(CFLAGS) -DCLI_TEST -c -o $@ $<

#build for bhi_160(experiement tool)
bhi-160_exp_test.o: bhi-160.c bhi-160_fw.h bhi-160.h
	$(CC) $(CFLAGS) -DBHI_TEST -c -o $@ $<

bhi_160: bhi-160_exp_test.o i2c_linux.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

#build for lark_sensors_test
lark_sensors_test.o: lark_sensors.cpp bhi-160_fw.h bhi-160.h
	$(CXX) $(CXXFLAGS) -DCLI_TEST -c -o $@ $<

#build for lark_sensor
lark_sensors_rdb.o: lark_sensors.cpp bhi-160_fw.h bhi-160.h
	$(CXX) $(CXXFLAGS) -DHAS_RDB_SUPPORT -c -o $@ $<

lark_sensors_test: lark_sensors_test.o bhi-160_test.o i2c_linux.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

process.o: process.cpp process.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

GeomagnetismLibrary.o: ./wmm/GeomagnetismLibrary.c ./wmm/GeomagnetismHeader.h
	$(CC) $(CFLAGS) -c -o $@ $<

gridvar.o: gridvar.cc gridvar.h  ./wmm/GeomagnetismHeader.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

lark_sensors: lark_sensors_rdb.o bhi-160.o i2c_linux.o process.o GeomagnetismLibrary.o gridvar.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

TEMPINSTALL := $(shell /bin/pwd)/install

.PHONY: install
install: $(INSTALL)
	mkdir -p $(TEMPINSTALL)/sbin
	install -m 0755 lark_sensors $(TEMPINSTALL)/sbin
	install -m 0644 ./wmm/WMM.COF $(TEMPINSTALL)/sbin
	if [ -n "${DEV_BUILD}" ]; then install -m 0755 lark_sensors_test $(TEMPINSTALL)/sbin; fi
	if [ -n "${DEV_BUILD}" ]; then install -m 0755 bhi_160 $(TEMPINSTALL)/sbin; fi
	scatter_install.sh -l -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -b

clean distclean:
	rm -f $(TARGETS)
	rm -f *.o
	rm -f bhi-160_fw.h
