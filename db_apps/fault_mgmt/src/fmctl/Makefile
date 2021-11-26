# ---------------------------------------------------------
# Copyright (C) 2018 NetComm Wireless Limited.
# ---------------------------------------------------------
#
# Expects the following external variables to be set
#
# PATH		   = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR	 = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB	   = CDCS library path

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -Werror -DDEBUG=1
else
	CFLAGS := -c -Wall -Werror -Os
endif
CFLAGS+= -I$(CDCS_INCLUDE) -I../../inc/

#FAULT_MGMT_LIB is passed from calling script
#FAULT_MGMT_CTL_EXE is passed from calling script

MGMT_CTL_SRC := main.c
MGMT_CTL_OBJ := $(MGMT_CTL_SRC:.c=.o)


.PHONY: all install clean distclean

all: $(FAULT_MGMT_CTL_EXE)


$(FAULT_MGMT_CTL_EXE): $(MGMT_CTL_OBJ)
	$(CC) $(LDFLAGS) $(MGMT_CTL_OBJ) -L ../../ -lfm -o $@


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean distclean:
	rm -f $(FAULT_MGMT_CTL_EXE) $(MGMT_CTL_OBJ)