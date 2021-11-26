#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

ifeq ($(PC_SIMULATOR),y)
CROSS_COMPILE :=
INSTALLDIR    :=
CDCS_INCLUDE  :=
CDCS_LIB      :=
else
CROSS_COMPILE ?= arm-ntc-linux-gnueabi-
INSTALLDIR    ?= ../../stagging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

#DEBUG := true

ifeq ($(PC_SIMULATOR),y)
CFLAGS := -c -Wall -Werror -Os -DPC_SIMULATOR
LDFLAGS := -L$(CDCS_LIB) -lrdb -lpthread
else
ifdef DEBUG
	CFLAGS := -g -c -O0 -Wall -Werror -I$(CDCS_INCLUDE)
	STRIP  := true
else
	CFLAGS := -c -Wall -Werror -Os -I$(CDCS_INCLUDE)
endif

LDFLAGS := -L$(CDCS_LIB) -lrdb -lpthread

ifeq ($(V_SECURED_REPAIR),bellca)
CFLAGS += -DHAVE_AUTHENTICATION
LDFLAGS += -lmbedcrypto
endif

endif

TEMPINSTALL := $(shell /bin/pwd)/install

PROJECT = ia_relay
SOURCES = main.c \
          scomm_datalink.c \
          scomm_frame.c \
          scomm_packet.c \
          scomm_service.c \
          crc.c \
          oss.c \
          logger.c \
          event_loop.c \
          serial_channel.c \
          socket_channel.c \
          pipe_channel.c \
          scomm_manager.c \
          mgmt_service_app.c \
          mgmt_service_proto.c \
          sock_service_app.c \
          sock_service_proto.c \
          test_service_app.c

ifeq ($(V_SECURED_REPAIR),bellca)
SOURCES += authentication.c
endif

ifneq ($(PC_SIMULATOR),y)
SOURCES += rdb_app.c
endif

DEPS = $(patsubst %.c,%.d,$(SOURCES))
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

-include $(DEPS)

.PHONY : all
all: $(PROJECT)
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin/
	install -m 755 $(PROJECT) $(TEMPINSTALL)/usr/bin/

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(PROJECT).*; \
	rm -f $(PROJECT) $(PROJECT).a $(OBJECTS) $(DEPS);
	rm -f *~ ~*;

%.o: %.c
	$(CC) $< $(CFLAGS) -o $@
	$(CC) -MM $*.c > $*.d

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.PHONY: install
install: all
	install -D -m 0755 $(PROJECT) "$(INSTALLDIR)/usr/bin/$(PROJECT)"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(PROJECT)"

