#
# This Makefile builds port_activity_monitor.
#
# Copyright Notice:
# Copyright (C) 2016 NetComm Wireless limited.
#
# This file or portions thereof may not be copied or distributed in any form
# (including but not limited to printed or electronic forms and binary or object forms)
# without the expressed written consent of NetComm Wireless Ltd.
# Copyright laws and International Treaties protect the contents of this file.
# Unauthorized use is prohibited.
#
#
# THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
# WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# Expects the following variables to be available:
#
# PATH          = Must include a path to the correct toolchain
# CDCS_INCLUDE  = Where to install include files
# CDCS_LIB      = Where to install libraries
# CROSS_COMPILE = toolchain prefix
# PLATFORM      = Sim, Platypus, Bovine, Elephant
# VARIANT       = Platform variant

CROSS_COMPILE ?=

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar
STRIP := $(CROSS_COMPILE)strip

LIBS := -lgcc -lpthread $(CDCS_LIB)/rdb.a $(CDCS_LIB)/cdcs_syslog_lib.a $(CDCS_LIB)/daemon_lib.a

CFLAGS += -g -Os -Wall -Wsequence-point -W -DPLATFORM_$(PLATFORM)=y -DVARIANT_$(VARIANT)=y
CFLAGS += -I$(CDCS_INCLUDE)
CFLAGS += -DDEBUG

LDFLAGS += -g -Os -pedantic -Wall -W
LDFLAGS += -L$(CDCS_LIB)

SRCS := port_activity_monitor.c
DEPS := port_activity_monitor.h
OBJS := $(patsubst %.c,%.o,$(SRCS))

PROJECT = port_activity_monitor
.PHONY: all
all: $(PROJECT)

$(OBJS): $(DEPS)

$(PROJECT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean, distclean
clean distclean:
	rm -f $(PROJECT) *.o

.PHONY: install
install: $(PROJECT)
	cp $(PROJECT) $(INSTALLDIR)/usr/bin/$(PROJECT)

uninstall:
	rm -f $(INSTALLDIR)/usr/bin/$(PROJECT)
