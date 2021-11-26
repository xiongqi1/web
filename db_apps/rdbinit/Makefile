CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
STRIP=$(CROSS_COMPILE)strip

CFLAGS=-Wall -O2 -fPIC -L$(CDCS_LIB) -I$(CDCS_INCLUDE) -g #-D_DEBUG

CDCS_INCLUDE?=../../staging_l/include
CDCS_LIB?=../../staging_l/lib

ifeq ($(PLATFORM),Arachnid)
CFLAGS := $(CFLAGS) -DUSE_VFORK
endif

TARGET=rdbinit
TESTS=

all: target tests

target: $(TARGET)
tests: $(TESTS)

# basic object dependencies
lrt.o: lrt.c lrt.h Makefile
luaprocman.o: luaprocman.c Makefile
rdbinit.o: rdbinit.c global.h Makefile

# test dependencies
test_%.o: test_%.c Makefile

# build tests
test_%: test_%.o %.o
	$(CC) $(CFLAGS) -lnc -o $@ $^

# build rdbinit binary
rdbinit: rdbinit.o lrt.o luaprocman.o
	$(CC) $(CFLAGS) -llua -lm -ldl -lnc -o $@ $^

# install
install: target tests
	mkdir -p $(INSTALLDIR)/usr/bin
	cp $(TARGET) $(INSTALLDIR)/usr/bin
	$(STRIP) $(INSTALLDIR)/usr/bin/$(TARGET)
	mkdir -p $(INSTALLDIR)/usr/lib/rdbinit
	cp lua/* $(INSTALLDIR)/usr/lib/rdbinit
	mkdir -p $(INSTALLDIR)/etc/rdbinit.d

# clean up
clean:
	rm -f *.o core $(TESTS) $(TARGET)

.PHONY: all target tests clean install
