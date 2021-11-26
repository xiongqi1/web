#
# When building an IPK, this project builds binaries for all
# bovine processors. The correct one is selected at installation
# time.
#
# Iwo

CROSS_COMPILE_x86:=
CROSS_COMPILE_arm9:=/opt/cross/gcc-4.9.3-glibc-2.23-arm9/bin/arm-ntc-linux-gnueabi-
CROSS_COMPILE_cortexA8:=/opt/cross/gcc-4.9.3-glibc-2.23-cortexA8/bin/arm-ntc-linux-gnueabi-

CFLAGS:=-Os -Wall -pedantic

BINS:=loggerdump.x86 loggerdump.arm9 loggerdump.cortexA8
IPK:=console-logger_1.0_arm.ipk

.PHONY : all clean distclean install

all: $(BINS) $(IPK)

loggerdump.x86: loggerdump.c
	$(CROSS_COMPILE_x86)gcc $(CFLAGS) -o $@ $^
	$(CROSS_COMPILE_x86)strip $@

loggerdump.arm9: loggerdump.c
	$(CROSS_COMPILE_arm9)gcc $(CFLAGS) -o $@ $^
	$(CROSS_COMPILE_arm9)strip $@

loggerdump.cortexA8: loggerdump.c
	$(CROSS_COMPILE_cortexA8)gcc $(CFLAGS) -o $@ $^
	$(CROSS_COMPILE_cortexA8)strip $@

loggerdump: loggerdump.c
	# Default target for direct install
	$(CROSS_COMPILE)gcc $(CFLAGS) -o $@ $^
	$(CROSS_COMPILE)strip $@

$(IPK): $(BINS) $(wildcard CONTROL/*) S105logger
	mkdir -p STAGING/CONTROL
	mkdir -p STAGING/bin
	mkdir -p STAGING/etc/init.d/rc2.d
	cp CONTROL/* STAGING/CONTROL/
	cp loggerdump.arm9 loggerdump.cortexA8 STAGING/bin/
	cp S105logger STAGING/etc/init.d/rc2.d/
	./ipkg-build.sh -o 0 -g 0 STAGING
	-cp $(IPK) $(IDIR)/

install: loggerdump
	mkdir -p "$(INSTALLDIR)/bin/"
	mkdir -p "$(INSTALLDIR)/etc/init.d/rc2.d/"
	cp loggerdump "$(INSTALLDIR)/bin/"
	cp S105logger "$(INSTALLDIR)/etc/init.d/rc2.d/"
	ln -sf /bin/loggerdump "$(INSTALLDIR)/etc/init.d/rc2.d/S102loggerdump"

clean distclean:
	rm -f $(BINS) $(IPK) loggerdump
	rm -fr STAGING
