#
# build Quectel QMI proxy server
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
#
# Copyright (C) 2018 NetComm Wireless Limited.
#
#
# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

.PHONY: all install clean

all:
	@echo "Don't invoke this directly!"

quectel-qmi-proxy.o: quectel-qmi-proxy.c

quectel-qmi-proxy: quectel-qmi-proxy.o
	$(CC) -Wall -s $^ -lpthread -ldl -o $@

install: quectel-qmi-proxy
	install -m 0755 quectel-qmi-proxy $(INSTALLDIR)/usr/bin/quectel-qmi-proxy
	$(STRIP) $(INSTALLDIR)/usr/bin/quectel-qmi-proxy

distclean clean:
	# remove targets
	rm -f quectel-qmi-proxy *.o *~
