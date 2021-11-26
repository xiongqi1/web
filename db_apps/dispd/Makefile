# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../../romfs
CDCS_INCLUDE  ?= ../../../staging_l/include
CDCS_LIB      ?= ../../../staging_l/lib
else ifeq ($(PLATFORM),Platypus2)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../staging
KERNELINC     ?= $(shell readlink -f ../../RT288x_SDK/source/linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
else ifeq ($(PLATFORM),Avian)
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging/cdcs
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
PREFIXDIR 	  ?= $(INSTALLDIR)
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
endif

# compile tools
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

#DEBUG:=1

# compile and link flags
ifdef DEBUG
CFLAGS := -c -Wall -O0 -g -I$(CDCS_INCLUDE)
LFLAGS := -g
STRIP := echo
else
CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
LFLAGS :=
endif

CFLAGS += -DV_BOARD_$(V_BOARD) -DV_LEDFUN_$(V_LEDFUN) -DV_GPS_$(V_GPS) -DV_MULTIPLE_WWAN_PROFILES_$(V_MULTIPLE_WWAN_PROFILES) -DV_WIFI_$(V_WIFI) -DV_BATTERY_$(V_BATTERY) -DV_SKIN_$(V_SKIN) -DV_IOBOARD_$(V_IOBOARD) -DV_REQUIRE_FACTORY_PASSWORD_CHANGE_$(V_REQUIRE_FACTORY_PASSWORD_CHANGE)

# set project
PRJ = dispd

# specify src, obj and lib
MODULE_SRC := stridx.c rdb.c
MODULE_BIN := $(patsubst %.c,modules/%,$(MODULE_SRC))
MODULE_OBJ := $(patsubst %.c,modules/%.o,$(MODULE_SRC))

ifeq ($(V_LEDFUN),wntd_idu)
	SRC := dispd.c dispd_def.c $(MODULE_SRC)
else
	SRC := dispd.c dispd_def.c led.c $(MODULE_SRC)
endif 

OBJ := $(patsubst %.c,%.o,$(SRC))
LIB := $(CDCS_LIB)/rdb.a

.PHONY:	all install clean distclean modules

all: $(PRJ) modules
	# done
	
install: all
	install -D -m 0755 $(PRJ) "$(INSTALLDIR)/usr/bin/$(PRJ)"
	install -D -m 0755 ctrl_dispd.sh "$(INSTALLDIR)/usr/bin/ctrl_dispd.sh"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(PRJ)"

clean distclean:
	rm -f $(OBJ) $(PRJ) $(MODULE_BUILT) $(MODULE_OBJ) $(MODULE_BIN) dispd_def.h dispd_def.c

$(PRJ): $(OBJ)
	$(CC) $(LFLAGS) $(OBJ) $(LIB) -o $@

modules/%.o: %.c
	$(CC) $(CFLAGS) -O0 -g -DMODULE_TEST $< -o $@
	
dispd_def.c dispd_def.h: rdbnoti.conf rdbnoti_gen.sh
	./rdbnoti_gen.sh
	
dispd.o: dispd.c dispd_def.h
	$(CC) $(CFLAGS) dispd.c -o $@
	
$(MODULE_BIN): $(MODULE_OBJ)
	@for bin in $(patsubst %.o,%,$^); do \
		$(CC) $(LFLAGS) $${bin}.o $(LIB) -o $$bin; \
	done
	
modules: $(MODULE_BIN)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
