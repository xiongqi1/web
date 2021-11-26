# compile tools
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

# compile and link flags
ifdef DEBUG
CFLAGS := -c -Wall -O0 -g -I$(CDCS_INCLUDE)
LFLAGS := -g
STRIP := echo
else
CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
LFLAGS :=
endif

# set project
PRJ = timedaemon

# specify src, obj and lib
MODULE_SRC := rbtreehash-dlist.c
MODULE_BIN := $(patsubst %.c,modules/%,$(MODULE_SRC))
MODULE_OBJ := $(patsubst %.c,modules/%.o,$(MODULE_SRC))

SRC := timedaemon.c $(MODULE_SRC)
OBJ := $(patsubst %.c,%.o,$(SRC))
LIB := $(CDCS_LIB)/rdb.a $(CDCS_LIB)/cdcs_syslog_lib.a

ifeq ($(V_GPS),y)
V_VARS = V_GPS
endif

# V_GPS installs GPS daemons and these daemons provide GPS time.
# Aside from these GPS daemons, WMMD also provides GPS time. V_WMMD is used to select GPS time. This association avoids missing GPS time.
ifeq ($(V_WMMD),y)
V_VARS = V_GPS
endif

# overide with V_NTP_SERVER
ifeq ($(V_NTP_SERVER),y)
V_VARS = V_NTP_SERVER
endif

.PHONY:	all install clean distclean modules config

all: $(PRJ) modules config
	# done

install: all
	install -D -m 0755 $(PRJ) "$(INSTALLDIR)/usr/bin/$(PRJ)"
	$(STRIP) "$(INSTALLDIR)/usr/bin/$(PRJ)"
	cp timedaemon.conf $(INSTALLDIR)/etc

clean distclean:
	rm -f $(OBJ) $(PRJ) $(MODULE_BUILT) $(MODULE_OBJ) $(MODULE_BIN)  timedaemon.conf

$(PRJ): $(OBJ)
	$(CC) $(LFLAGS) $(OBJ) $(LIB) -o $@

modules/%.o: %.c
	$(CC) $(CFLAGS) -O0 -g -DMODULE_TEST $< -o $@
	
$(MODULE_BIN): $(MODULE_OBJ)
	@for bin in $(patsubst %.o,%,$^); do \
		$(CC) $(LFLAGS) $${bin}.o $(LIB) -o $$bin; \
	done
	
modules: $(MODULE_BIN)

config:
	./gen_conf.sh $(V_VARS)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
