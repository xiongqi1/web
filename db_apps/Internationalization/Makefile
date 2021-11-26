# Use make to generate all the xml language files.
##################################################
# Use make install (for Platypus2 and Bovine) to install the xml language files to the staging directory which depends on V_LANGUAGE_??
# (?? are ISO Language Code)
#
# Expects the following external variables to be set
# PLATFORM       = Platform name
# INSTALLDIR     = Root of staging tree for install
# V_LANGUAGE_??  = Language files to be installed to the $INSTALLDIR
#
##################################################

PLATFORM ?= Platypus

ifeq ($(PLATFORM),Platypus)
# the build install fuction for the Platypus1 is not verified yet. Please use manual make to update the xml files for Platypus1.
INSTALLDIR = ../../../romfs/etc_ro
endif

STAGING_P ?= ../../staging_p

CFLAGS += -DV_LANGUAGE_FR_$(V_LANGUAGE_FR) -DV_LANGUAGE_AR_$(V_LANGUAGE_AR) -DV_LANGUAGE_DE_$(V_LANGUAGE_DE) -DV_LANGUAGE_JP_$(V_LANGUAGE_JP)

.PHONY: all

all: .lang

.lang: strings.csv BuildFiles
ifeq ($(PLATFORM),Platypus)
	./gen-all-langs.sh
else
	./gen-all-langs.sh "OPTIMISE" $(INSTALLDIR)/www
endif

$(info webif_spec = $(V_WEBIF_SPEC))

BuildFiles: BuildFiles.c
ifeq ($(V_WEBIF_SPEC),)
	echo -e "#ifndef V_WEBIF_SPEC\n#define V_WEBIF_SPEC \"\"\n#endif\n" >webif_spec.h
else
	echo -e "#ifndef V_WEBIF_SPEC\n#define V_WEBIF_SPEC \"spec_"$(V_WEBIF_SPEC)"\"\n#endif\n" >webif_spec.h
endif
	gcc $(CFLAGS) -g -Wall BuildFiles.c -o BuildFiles

clean:
	rm -rf BuildFiles .lang-merged lang-merged lang

install: all
ifneq ($(MODE),recovery)
	# Always install English language reference files for web interface.
	-rm -fr $(INSTALLDIR)/www/lang
	mkdir $(INSTALLDIR)/www/lang
	-cp -r lang/en $(INSTALLDIR)/www/lang/

	# this b28n.js is for Bovine / Platypus2. Please check the with the diff if use it for Platypus1
ifeq ($(V_WEBIF_VERSION),v2)
	-cp b28n.min.js $(INSTALLDIR)/www/lang/
else
	-cp b28n.js $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_FR),y)
	# Install French language reference files.
	-cp -r lang/fr $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_AR),y)
	# Install Arabic language reference files
	cp -r lang/ar $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_DE),y)
	# Install German language reference files
	cp -r lang/de $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_IT),y)
	# Install Italian language reference files
	cp -r lang/it $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_ES),y)
	# Install Spanish language reference files
	cp -r lang/es $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_PT),y)
	# Install Portugese language reference files
	cp -r lang/pt $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_CZ),y)
	# Install Czech language reference files
	cp -r lang/cz $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_NL),y)
	# Install Dutch language reference files
	cp -r lang/nl $(INSTALLDIR)/www/lang/
endif

ifeq ($(V_LANGUAGE_JP),y)
	# Install Japanese language reference files
	cp -r lang/jp $(INSTALLDIR)/www/lang/
endif

#-------------------------------------------------------------------------------------
# Copy NAS language file to staging_p folder for NAS package
#-------------------------------------------------------------------------------------
ifeq ($(V_NAS),y)

	# need to copy necessary files to create language files befire run gen-all-langs.sh
	mkdir -p $(STAGING_P)/nas/usr/local/www/
	-cp $(INSTALLDIR)/www/*.inc $(STAGING_P)/nas/usr/local/www/
	-cp $(INSTALLDIR)/www/*.js $(STAGING_P)/nas/usr/local/www/

	# need to generate specific language files for V_ variable
ifeq ($(PLATFORM),Platypus)
	./gen-all-langs.sh
else
	./gen-all-langs.sh "OPTIMISE" $(STAGING_P)/nas/usr/local/www/
endif
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/en
	-cp -r lang/en/nas.xml $(STAGING_P)/nas/usr/local/www/lang/en

ifeq ($(V_LANGUAGE_FR),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/fr
	-cp -r lang/fr/nas.xml $(STAGING_P)/nas/usr/local/www/lang/fr
endif

ifeq ($(V_LANGUAGE_AR),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/ar
	-cp -r lang/ar/nas.xml $(STAGING_P)/nas/usr/local/www/lang/ar
endif

ifeq ($(V_LANGUAGE_DE),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/de
	-cp -r lang/de/nas.xml $(STAGING_P)/nas/usr/local/www/lang/de
endif

ifeq ($(V_LANGUAGE_IT),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/it
	-cp -r lang/it/nas.xml $(STAGING_P)/nas/usr/local/www/lang/it
endif

ifeq ($(V_LANGUAGE_ES),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/es
	-cp -r lang/es/nas.xml $(STAGING_P)/nas/usr/local/www/lang/es
endif

ifeq ($(V_LANGUAGE_PT),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/pt
	-cp -r lang/pt/nas.xml $(STAGING_P)/nas/usr/local/www/lang/pt
endif

ifeq ($(V_LANGUAGE_CZ),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/cz
	-cp -r lang/cz/nas.xml $(STAGING_P)/nas/usr/local/www/lang/cz
endif

ifeq ($(V_LANGUAGE_NL),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/nl
	-cp -r lang/nl/nas.xml $(STAGING_P)/nas/usr/local/www/lang/nl
endif

ifeq ($(V_LANGUAGE_JP),y)
	mkdir -p $(STAGING_P)/nas/usr/local/www/lang/jp
	-cp -r lang/jp/nas.xml $(STAGING_P)/nas/usr/local/www/lang/jp
endif

	# clean up unnecessary files
	rm $(STAGING_P)/nas/usr/local/www/*.inc
	rm $(STAGING_P)/nas/usr/local/www/*.js

endif	# ifeq ($(V_NAS),y)

endif	# ifneq ($(MODE),recovery)
