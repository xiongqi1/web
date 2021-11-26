# Makefile for Installation Assistant UI (supporting multiple UIs)
#
# Copyright (C) 2020 Casa Systems.
#
# Expects the following external variables to be set
#
# INSTALLDIR     = Root of staging tree for install
# V_INSTALLATION_ASSISTANT_UI_MODEL_LIST  = list of supported UI models

TEMPINSTALL := $(shell /bin/pwd)/install
TEMPIA := $(TEMPINSTALL)/usr/share/installation_assistant
CC := $(CROSS_COMPILE)gcc

# source tree structure
#
# common/: common files for all models
# generic/: files for generic UI (shown before synchronisation is done)
# <model>/: model specific files
#
# The structure for each of the above directories is the same as it will appear in the install tree below.
#
#
# install tree structure
#
# generic/: all files for generic UI. i.e. common + generic overwrite
# <model>/: all files for <model>. i.e. common + <model> overwrite
#
# The subtree under common is first copied to $(TEMPIA)/generic|<model>.
# Then the corresponding subtree under generic|<model> is copied,
# adding to/overwriting the common stuff.
#
# During the above process, javascript files under js2merge will be merged into
# a single file, js/all.js, for historic (nrb0200 era) reason.
# uglifyjs is no longer necessary since we are not limited by serial port.

UI_FLAGS :=

ifeq ($(V_SKIN), ATT)
	UI_FLAGS += -DNO_HEIGHT_TYPE
	UI_FLAGS += -DNO_DETACH_NETWORK
endif

# file name regular expressions - files to be installed into staging
FILE_REGEX := ".*\.\(html\|js\|css\|lua\|gif\|png\|ico\|mp3\)"

.PHONY : all clean install
all:
	mkdir -p $(TEMPINSTALL)

	echo "install generic ui ..."
	install -m 0755 -d $(TEMPIA)/generic/client/js
	install -m 0755 -d $(TEMPIA)/generic/client/css
	install -m 0755 -d $(TEMPIA)/generic/client/res
	(cd common && find . -regex $(FILE_REGEX) -exec install -Dm 0755 "{}" "$(TEMPIA)/generic/{}" \;)
	(cd generic && find . -regex $(FILE_REGEX) -exec install -Dm 0755 "{}" "$(TEMPIA)/generic/{}" \;)
	mkdir -p $(TEMPIA)/generic/client/preproc
	(cd $(TEMPIA)/generic/client && find . -maxdepth 1 -name \*.html -exec $(CC) -E -x c -P -nostdinc $(UI_FLAGS) -C {} -o $(TEMPIA)/generic/client/preproc/{} \;)
	-cp $(TEMPIA)/generic/client/preproc/* $(TEMPIA)/generic/client
	rm -rf $(TEMPIA)/generic/client/preproc
	-$(CC) -E -x c -P -nostdinc $(UI_FLAGS) -C $(TEMPIA)/generic/client/js2merge/all.js -o $(TEMPIA)/generic/client/js/all.js
	rm -rf $(TEMPIA)/generic/client/js2merge

	echo "install model specific ui ..."
	for i in $(V_INSTALLATION_ASSISTANT_UI_MODEL_LIST); do \
		install -m 0755 -d $(TEMPIA)/$$i/client/js ; \
		install -m 0755 -d $(TEMPIA)/$$i/client/css ; \
		install -m 0755 -d $(TEMPIA)/$$i/client/res ; \
		(cd common && find . -regex $(FILE_REGEX) -exec install -Dm 0755 "{}" "$(TEMPIA)/$$i/{}" \;) ; \
		(cd $$i && find . -regex $(FILE_REGEX) -exec install -Dm 0755 "{}" "$(TEMPIA)/$$i/{}" \;) ; \
		mkdir -p $(TEMPIA)/$$i/client/preproc ; \
		(cd $(TEMPIA)/$$i/client && find . -maxdepth 1 -name \*.html -exec $(CC) -E -x c -P -nostdinc $(UI_FLAGS) -C {} -o $(TEMPIA)/$$i/client/preproc/{} \;) ; \
		cp $(TEMPIA)/$$i/client/preproc/* $(TEMPIA)/$$i/client ; \
		rm -rf $(TEMPIA)/$$i/client/preproc ; \
		$(CC) -E -x c -P -nostdinc $(UI_FLAGS) -C $(TEMPIA)/$$i/client/js2merge/all.js -o $(TEMPIA)/$$i/client/js/all.js ; \
		rm -rf $(TEMPIA)/$$i/client/js2merge ; \
	done

clean:
	rm -rf $(TEMPINSTALL)

install: all
	scatter_install.sh -s true "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr
