# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip
CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE) -fPIE -fstack-protector-strong
LDFLAGS := -pie

#DEBUG=1

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -I$(CDCS_INCLUDE)
	STRIP  := echo
endif

# platform defines
ifeq ($(PLATFORM),Avian)
	CFLAGS += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
	CFLAGS += -D PLATFORM_BOVINE
else ifeq ($(PLATFORM),Arachnid)
	CFLAGS += -D PLATFORM_ARACHNID $(PLATFORM_CFLAGS)
else ifeq ($(PLATFORM),Platypus)
	CFLAGS += -D PLATFORM_PLATYPUS
else ifeq ($(PLATFORM),Platypus2)
	CFLAGS += -D PLATFORM_PLATYPUS
endif

# libraries
PROJECT=cgipipe

#SRCS := featurehash.c binqueue.c dbhash.c generictree.c linkedlist.c minilib.c port.c qmiservtran.c resourcetree.c dbenum.c funcschedule.c growingmem.c main.c msgqueue.c qmimsg.c qmiuniclient.c strqueue.c
SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))
#LIBS := -lgcc -lm $(CDCS_LIB)/rdb.a
LIBS := -lgcc -lm

.PHONY: all

all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
	$(STRIP) $@

install: all
	install -m 755 $(PROJECT) $(INSTALLDIR)/usr/bin/
	$(STRIP) $(INSTALLDIR)/usr/bin/$(PROJECT)
	ln -sf cgipipe $(INSTALLDIR)/usr/bin/cgipipe_server
	ln -sf cgipipe $(INSTALLDIR)/usr/bin/cgipipe_read
	ln -sf cgipipe $(INSTALLDIR)/usr/bin/cgipipe_write
	ln -sf cgipipe $(INSTALLDIR)/usr/bin/cgipipe_check
	ln -sf cgipipe $(INSTALLDIR)/usr/bin/cgipipe_ctrl

distclean clean:
	rm -f $(PROJECT) *.o *~
