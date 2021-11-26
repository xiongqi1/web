#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# INSTALLDIR     = Root of staging tree for install
# SBASE          = Source tree base directory

# Set a few defaults (build for Platypus)
PLATFORM 	  ?= Platypus
VARIANT ?= $(shell cat "../../../variant.last_build" 2> /dev/null )

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= $(shell readlink -f ../../../romfs)
CDCS_INCLUDE  ?= $(shell readlink -f ../../../staging_l/include)
CDCS_LIB      ?= $(shell readlink -f ../../../staging_l/lib)
else ifeq ($(PLATFORM),Platypus2)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= ../../staging
KERNELINC     ?= $(shell readlink -f ../../RT288x_SDK/source/linux-2.6.21.x/include)
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
else ifeq ($(PLATFORM),Avian)
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= $(shell readlink -f ../../staging/cdcs)
CDCS_INCLUDE  ?= $(shell readlink -f ../../staging_l/include)
CDCS_LIB      ?= $(shell readlink -f ../../staging_l/lib)
PREFIXDIR     ?= $(INSTALLDIR)
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= $(shell readlink -f ../../staging)
CDCS_INCLUDE  ?= $(shell readlink -f ../../staging_l/include)
CDCS_LIB      ?= $(shell readlink -f ../../staging_l/lib)
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc

# Some parts are installed in prefix /, others in /usr. Set
# PREFIXDIR to move the /usr stuff somewhere else.
PREFIXDIR ?= $(INSTALLDIR)/usr

ifeq ($(PLATFORM),Platypus)
	WWW_DIR = $(INSTALLDIR)/etc_ro/www
	SRC_TGT = $(WWW_DIR)/internet/ussd.asp
	JS_TGT  = $(WWW_DIR)/js/ussd.js
else
	WWW_DIR = $(INSTALLDIR)/www
	SRC_TGT = $(WWW_DIR)/ussd.html
	JS_TGT  = $(WWW_DIR)/ussd.js
endif

ifeq ($(V_WEBIF_VERSION),v2)
	USSD_SRC = ussd_v2.html
else
	USSD_SRC = ussd.html
endif

$(info ******************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info slic=$(V_SLIC))
$(info voip=$(V_VOIP))
$(info autodial=$(V_AUTODIAL))
$(info ******************************************************************)

.PHONY: all install romfs
all install romfs:
	mkdir -p $(WWW_DIR)
ifeq ($(PLATFORM),Platypus)
	mkdir -p $(WWW_DIR)/internet
	mkdir -p $(WWW_DIR)/js
endif
	mkdir -p $(WWW_DIR)/cgi-bin
	cp ussd.cgi $(WWW_DIR)/cgi-bin/
	$(CC) -E -x c -P -nostdinc -D PLATFORM_$(PLATFORM) -C $(USSD_SRC) > $(SRC_TGT)
	$(CC) -E -x c -P -nostdinc -D PLATFORM_$(PLATFORM) -C ussd.js > $(JS_TGT)

ifeq ($(PLATFORM),Platypus2)
	touch ../Internationalization/BuildFiles.c
endif

.PHONY: clean distclean
clean distclean:
	rm -f *~
