#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# set compiler environment variables
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

CXXSTD = c++03

WARNFLAGS := -Wall -Wextra -pedantic -Wno-switch-default -Wuninitialized -Wfloat-equal -Wno-shadow \
	-Wcast-qual -Wno-cast-align -Wwrite-strings -Wno-conversion -Wno-packed -Wno-padded \
	-Wredundant-decls -Wno-inline -Wctor-dtor-privacy -Wdisabled-optimization \
	-Wmissing-format-attribute -Wno-aggregate-return -Wundef -Woverloaded-virtual -Wpointer-arith \
	-Wno-unused-parameter -Wno-comment -Wno-switch-enum -Wno-long-long -Wno-variadic-macros \
	-Wno-deprecated -Wdouble-promotion -Wno-unused-result -Wno-format-zero-length \
	-Wno-suggest-attribute=format

CXXFLAGS :=
ifneq ($(CDCS_INCLUDE),)
CXXFLAGS += -I$(CDCS_INCLUDE)
endif
CXXFLAGS += -std=$(CXXSTD) -O3 $(WARNFLAGS)
CXXFLAGS += -DV_IOBOARD_$(V_IOBOARD)
# comment the next line to enable printing debug messages in the program
CXXFLAGS += -DNDEBUG

LDFLAGS:= -lrdb
ifneq ($(CDCS_LIB),)
LDFLAGS += -L$(CDCS_LIB)
endif

PROJECT:=leds_monitor
OBJS:=main.o logger.o leds_config.o leds_handler.o leds_sysfs.o leds_rdb.o

.PHONY: all
all: $(PROJECT)

$(PROJECT): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c Makefile
	$(CXX) $(CXXFLAGS) -c $< -o $@

TEMPINSTALL := $(shell /bin/pwd)/install

.PHONY: install
install: $(PROJECT)
	mkdir -p $(TEMPINSTALL)/sbin
	install -m 0755 $(PROJECT) $(TEMPINSTALL)/sbin
	scatter_install.sh -l -s "$(CROSS_COMPILE)strip" "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -b

.PHONY: clean distclean
clean distclean:
	rm -f $(PROJECT) *.o
	rm -rf $(TEMPINSTALL)
