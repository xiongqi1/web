
#CPU := atmega328p
CPU := atmega168p
CLOCK := 16000000

# Programing
CPUS := m328p
#CPUS := m168p

# For use by serdeb
DBGBAUD := 921600
DBGPIN := -DDADDR=0x05 -DDPIN=PB3

# Libraries may live in avr_lib(AVR) or libs(generic)
ALIBS := /home/m/progs/avr_lib
GLIBS := /home/m/progs/libs

TARGETS := framlog.hex

OBJS_framlog := framlog.o
ALIBS_framlog := serdeb.o i2c.o
GLIBS_framlog :=

TOOLPATH := /opt/cross/avrc/bin
CROSS_COMPILE := avr-

export PATH:=$(PATH):$(TOOLPATH)

OPT := -ffunction-sections -fdata-sections -ffast-math -mcall-prologues \
-ffreestanding -Wl,--relax -fno-tree-scev-cprop -Os -g -gdwarf-2

WARNS := -Wall -Wsequence-point
OPTS := -mmcu=$(CPU) $(OPT)

CFLAGS := $(WARNS) $(OPTS) -I$(ALIBS) -I$(GLIBS) -I$(PWD) -DF_CPU=$(CLOCK)
CFLAGS += $(DBGPIN)

LDFLAGS := -Wl,-gc-sections -lm

LD := $(CROSS_COMPILE)ld
CC := $(CROSS_COMPILE)gcc
OBJCP := $(CROSS_COMPILE)objcopy
OBJDP := $(CROSS_COMPILE)objdump

LSTFILES := $(patsubst %.hex,%.lst,$(TARGETS))

all: $(TARGETS) $(LSTFILES)

%.hex: %.elf
	$(OBJCP) -j .text -j .data -O ihex $< $@

%.lst: %.elf
	$(OBJDP) -h -dS $< >$@

# For serdebug
$(ALIBS)/serdeb.c: sout.h

sout.h:
	$(ALIBS)/debugout.tcl $(CLOCK) $(DBGBAUD) >$@

framlog.elf: $(OBJS_framlog) $(patsubst %,$(ALIBS)/%,$(ALIBS_framlog)) $(patsubst %,$(GLIBS)/%,$(GLIBS_framlog))
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	avr-size -A $@

%.prog: %.hex
	# Use jtagusbmk2, bootloader seems broken.
	sudo avrdude -p $(CPUS) -c jtag2isp -P usb -b 19200 -F -U flash:w:$<

erase:
	sudo avrdude -p $(CPUS) -c jtag2isp -P usb -b 19200 -F -e

fuses: sens.o
	F=`avr-objdump -s -j .fuse  sens.o | sed -n 's/.*0000 \([0-9a-f]\+\)/\1/p'`; \
	LF=$${F:0:2}; HF=$${F:2:2}; EF=$${F:4:2}; \
	echo "$$LF - $$HF - $$EF"; \
	avrdude -p $(CPUS) -c jtag2isp -P usb -B 6 -U lfuse:w:0x$$LF:m -U hfuse:w:0x$$HF:m -U efuse:w:0x$$EF:m

clean distclean:
	(cd $(ALIBS) && make distclean)
	(cd $(GLIBS) && make distclean)
	rm -f *.o *.elf *.hex *.lst sout.h

.PHONE: all clean distclean
