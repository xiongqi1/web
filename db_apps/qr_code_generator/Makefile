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

CFLAGS := -Os -Wall -pedantic -std=gnu99 -Werror
LDFLAGS:= -lrdb -lpng -lz

ifneq ($(CDCS_INCLUDE),)
CFLAGS += -I$(CDCS_INCLUDE)
endif

#CFLAGS += -DDEBUG
CFLAGS += -DV_TITAN_INSTALLATION_ASSISTANT_CONN_$(V_TITAN_INSTALLATION_ASSISTANT_CONN)

ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

PROJECT:=qr_code_generator
OBJS:=main.o rdb_helper.o data_build_helper.o json_data.o print_json.o qr_code_image.o qrcodegen.o data_model_$(V_QR_CODE_GENERATOR).o

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

TEMPINSTALL := $(shell /bin/pwd)/install

.PHONY: install
install: $(PROJECT)
	mkdir -p $(TEMPINSTALL)/bin
	install -m 0755 $(PROJECT) $(TEMPINSTALL)/bin
	scatter_install.sh -l -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -b

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o
	rm -rf $(TEMPINSTALL)
