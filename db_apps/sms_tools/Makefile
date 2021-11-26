#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# INSTALLDIR     = Root of staging tree for install
# SBASE          = Source tree base directory

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
PREFIXDIR     ?= $(INSTALLDIR)
else
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= ../../staging
CDCS_INCLUDE  ?= ../../staging_l/include
CDCS_LIB      ?= ../../staging_l/lib
endif

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld

FILE_DBLIB := $(CDCS_LIB)/rdb.a

ifdef DEBUG
	CFLAGS := -O0 -g -c -Wall -I$(CDCS_INCLUDE)
	# This disables strip
	STRIP  := echo
else
	CFLAGS := -c -Wall -Os -I$(CDCS_INCLUDE)
endif

# Some parts are installed in prefix /, others in /usr. Set
# PREFIXDIR to move the /usr stuff somewhere else.
#PREFIXDIR ?= $(INSTALLDIR)/usr
TEMPINSTALL := $(shell /bin/pwd)/install

ifeq ($(PLATFORM),Platypus)
	# platypus
	LIBS := -lm $(FILE_DBLIB)
	TEST_LIBS := $(LIBS) $(CDCS_LIB)/unit_test.a
	RTSDK = $(shell CURDIR=`pwd`; echo $${CURDIR%%/user/cdcs/sms_tools})
	LIBNV_BIN = libnvram.a
	LIBNV_DIR = $(RTSDK)/lib/libnvram
	LIBNV_FNAME = $(LIBNV_DIR)/$(LIBNV_BIN)
	CFLAGS+=-I $(LIBNV_DIR) -DBOARD_$(V_BOARD)
	LIBS+=$(LIBNV_FNAME)
	SRC := batch_nv.c
	OBJ := $(patsubst %.c,%.o,$(SRC))
else ifeq ($(PLATFORM),Platypus2)
	# platypus
	LIBS := -lm $(FILE_DBLIB)
	#LDFLAGS :=
	CFLAGS += -I$(KERNELINC)
	# slic cal data save/restore feature
else
	# bovine / Avian
	LIBS := $(FILE_DBLIB)
	CFLAGS+= -DBOARD_$(V_BOARD)

ifeq ($(PLATFORM),$(filter $(PLATFORM), Bovine Serpent))
	SRC := sms_server.c
	OBJ := $(patsubst %.c,%.o,$(SRC))
endif

endif

SP_SRC := check_sp.c
SP_OBJ := $(patsubst %.c,%.o,$(SP_SRC))

IDX_SRC := get_file_index.c
IDX_OBJ := $(patsubst %.c,%.o,$(IDX_SRC))

ifeq ($(PLATFORM),Platypus)
	ETC_DIR = /etc_ro
	WWW_DIR = /etc_ro/www
	SRC_DIR = $(WWW_DIR)/smstools
	SRC_TGT = $(SRC_DIR)/sms.asp
	JS_DIR  = $(WWW_DIR)/js
else
	ETC_DIR = /etc
	WWW_DIR = /www
	SRC_DIR = $(WWW_DIR)
	SRC_TGT = $(SRC_DIR)/sms.html
	JS_DIR  = $(WWW_DIR)
endif
	JS_TGT  = $(JS_DIR)/sms.js

$(info ******************************************************************)
$(info platform=$(PLATFORM))
$(info board=$(V_BOARD))
$(info ioboard=$(V_IOBOARD))
$(info sms=$(V_SMS))
$(info voip=$(V_VOIP))
$(info autodial=$(V_AUTODIAL))
$(info V_SMS_UPDATE_UNREAD_CNT_INBOX_ONLY=$(V_SMS_UPDATE_UNREAD_CNT_INBOX_ONLY))
$(info V_CUSTOM_FEATURE_PACK=$(V_CUSTOM_FEATURE_PACK))
$(info ******************************************************************)

# merge those V variable directories into the top
V_VARIABLE_DIRS := V_CELL_NW V_SSH V_IPKG

.PHONY: all install romfs
ifeq ($(PLATFORM),Platypus)
all : batch_nv check_sp get_file_index
else ifeq ($(PLATFORM),$(filter $(PLATFORM), Bovine Serpent))
all : sms_server check_sp get_file_index
else
all : check_sp get_file_index
endif
	$(shell ./extract_smsjs_string.sh)
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/bin/
	install -m 0755 sms_handler.sh $(TEMPINSTALL)/usr/bin/
	install -m 0755 sms_admin.sh $(TEMPINSTALL)/usr/bin/
	install -m 0755 sendsms $(TEMPINSTALL)/usr/bin/
	install -m 0755 send_failed_msg $(TEMPINSTALL)/usr/bin/
	install -m 0755 -d $(TEMPINSTALL)/usr/lib
	install -m 0644 sms_utils $(TEMPINSTALL)/usr/lib
	install -m 0755 ssmtp $(TEMPINSTALL)/usr/bin/
	install -m 0755 sms_tx_counter.sh $(TEMPINSTALL)/usr/bin/
	install -m 0755 sms_rx_sub.sh $(TEMPINSTALL)/usr/bin/
	install -m 0755 sms_set_inbox_path.sh $(TEMPINSTALL)/usr/bin/
	install -m 0755 process_predefined_command.awk $(TEMPINSTALL)/usr/bin/
# in read-only platform, location for default config files.
# runtime config files should be stored in /var/sms folder
	install -m 0755 -d $(TEMPINSTALL)/usr/etc/sms
	install -m 0644 sms.cfg $(TEMPINSTALL)/usr/etc/sms/
	install -m 0644 sms_diag.cfg $(TEMPINSTALL)/usr/etc/sms/

	install -m 755 check_sp $(TEMPINSTALL)/usr/bin/
	install -m 755 get_file_index $(TEMPINSTALL)/usr/bin/

ifeq ($(PLATFORM),Platypus)
	install -m 0755 -d $(TEMPINSTALL)/usr/etc/sms
	install -m 0644 sms_common_platypus.cfg $(TEMPINSTALL)/usr/etc/sms/sms_common.cfg
	install -m 0755 -d $(TEMPINSTALL)/sbin/
	install -m 0755 sms_diagnostics.sh $(TEMPINSTALL)/sbin/
	install -m 0755 -d $(TEMPINSTALL)/etc_ro/cdcs/conf/mgr_templates/
	install -m 0755 sms.template $(TEMPINSTALL)/etc_ro/cdcs/conf/mgr_templates/
	install -m 0755 sms_onoff.template $(TEMPINSTALL)/etc_ro/cdcs/conf/mgr_templates/
	install -m 0755 sms_send_failed_msg.template $(TEMPINSTALL)/etc_ro/cdcs/conf/mgr_templates/
	install -m 0755 batch_nv $(TEMPINSTALL)/usr/bin/
else ifeq ($(PLATFORM),Platypus2)
	install -m 0644 sms_common_platypus.cfg $(TEMPINSTALL)/usr/etc/sms/sms_common.cfg
	install -m 0755 -d $(TEMPINSTALL)/etc/init.d/rc.d
	install -m 0755 -d $(TEMPINSTALL)/etc/init.d/rc2.d
	install -m 0755 -d $(TEMPINSTALL)/etc/init.d/rc3.d
	install -m 0755 sms_diagnostics.sh $(TEMPINSTALL)/etc/init.d/rc.d/
	ln -sf ../rc.d/sms_diagnostics.sh $(TEMPINSTALL)/etc/init.d/rc3.d/S898sms_diagnostics
	install -m 0755 sms_init_inbox_path $(TEMPINSTALL)/etc/init.d/rc.d/
	ln -sf ../rc.d/sms_init_inbox_path $(TEMPINSTALL)/etc/init.d/rc2.d/S127sms_init_inbox_path
	install -m 0755 -d $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates
	install -m 0755 sms.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 sms_onoff.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 sms_send_failed_msg.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	touch ../Internationalization/BuildFiles.c
else ifeq ($(PLATFORM),$(filter $(PLATFORM), Bovine Serpent))
	install -m 0644 sms_common.cfg $(TEMPINSTALL)/usr/etc/sms/
	install -m 0755 -d $(TEMPINSTALL)/etc/init.d/rc.d
	install -m 0755 -d $(TEMPINSTALL)/etc/init.d/rc2.d
	install -m 0755 -d $(TEMPINSTALL)/etc/init.d/rc3.d
	install -m 0755 sms_diagnostics.sh $(TEMPINSTALL)/etc/init.d/rc.d/
	ln -sf ../rc.d/sms_diagnostics.sh $(TEMPINSTALL)/etc/init.d/rc3.d/S898sms_diagnostics
	install -m 0755 sms_server.sh $(TEMPINSTALL)/etc/init.d/rc.d/
	ln -sf ../rc.d/sms_server.sh $(TEMPINSTALL)/etc/init.d/rc3.d/S899sms_server
	install -m 0755 sms_init_inbox_path $(TEMPINSTALL)/etc/init.d/rc.d/
	ln -sf ../rc.d/sms_init_inbox_path $(TEMPINSTALL)/etc/init.d/rc2.d/S113sms_init_inbox_path
	install -m 0755 -d $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates
	install -m 0755 sms.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 sms_onoff.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 sms_send_failed_msg.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 sms_server $(TEMPINSTALL)/usr/bin/

ifneq ($(V_SMS_EXTRA_COMMANDS),none)
	-cp -f extra_commands_$(V_SMS_EXTRA_COMMANDS)/* $(TEMPINSTALL)/usr/bin/
	#TODO: the following lines have to be moved to fsextra
	cp -af "extra_commands_$(V_SMS_EXTRA_COMMANDS)/sms_commands.conf" sms_commands.conf; \
	for vn in $(V_VARIABLE_DIRS); do \
		vv=$$(eval echo \$$$${vn}); \
		if [ ! -d "$${vn}_$${vv}" ]; then \
			continue; \
		fi; \
		echo "copying... $${vn}_$${vv}"; \
		cp -af "$${vn}_$${vv}/"sms_* $(TEMPINSTALL)/usr/bin/; \
		cat "$${vn}_$${vv}/add_sms_commands.conf" >> sms_commands.conf; \
	done; \
	./sms_preprocess.sh "sms_commands.conf" > "$(TEMPINSTALL)/usr/bin/sms_commands.conf"
ifeq ($(V_CUSTOM_FEATURE_PACK),Verizon_USA)
	./sms_remove_plmn_command.sh "$(TEMPINSTALL)/usr/bin/sms_commands.conf" > "$(TEMPINSTALL)/usr/bin/sms_commands.conf.new"
	mv "$(TEMPINSTALL)/usr/bin/sms_commands.conf.new" "$(TEMPINSTALL)/usr/bin/sms_commands.conf"
endif
	chmod --reference="extra_commands_$(V_SMS_EXTRA_COMMANDS)/sms_commands.conf" "$(TEMPINSTALL)/usr/bin/sms_commands.conf"
endif

else ifeq ($(PLATFORM),Avian)
	install -m 0644 sms_common_avian.cfg $(TEMPINSTALL)/usr/etc/sms/sms_common.cfg
	#mkdir -p $(TEMPINSTALL)/etc/init.d/rc.d
	#mkdir -p $(TEMPINSTALL)/etc/init.d/rc3.d
	#cp -f sms_diagnostics.sh $(TEMPINSTALL)/etc/init.d/rc.d/
	#ln -sf ../rc.d/sms_diagnostics.sh $(TEMPINSTALL)/etc/init.d/rc3.d/S899sms_diagnostics
	install -m 0755 -d $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates
	install -m 0755 sms.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 sms_onoff.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 sms_send_failed_msg.template $(TEMPINSTALL)/etc/cdcs/conf/mgr_templates/
	install -m 0755 atmgr_launch.sh $(TEMPINSTALL)/usr/bin/
endif

	#$(shell echo "platform=$(PLATFORM)" > $(TEMPINSTALL)$(ETC_DIR)/platform.txt)
	echo "platform=$(PLATFORM)" > $(TEMPINSTALL)$(ETC_DIR)/platform.txt

	install -m 0755 -d $(TEMPINSTALL)$(WWW_DIR)
	install -m 0755 -d $(TEMPINSTALL)$(SRC_DIR)
ifneq ($(V_WEBIF_VERSION),v2)
	gcc -E -x c -P -nostdinc -D PLATFORM_$(PLATFORM) -D V_SMS_UPDATE_UNREAD_CNT_INBOX_ONLY_$(V_SMS_UPDATE_UNREAD_CNT_INBOX_ONLY) -C sms.html > $(TEMPINSTALL)$(SRC_TGT)
endif
	install -m 0755 -d $(TEMPINSTALL)$(JS_DIR)
	gcc -E -x c -P -D PLATFORM_$(PLATFORM) -D V_SMS_UPDATE_UNREAD_CNT_INBOX_ONLY_$(V_SMS_UPDATE_UNREAD_CNT_INBOX_ONLY) -C sms.js > $(TEMPINSTALL)$(JS_TGT)
	install -m 0755 -d $(TEMPINSTALL)$(WWW_DIR)/cgi-bin
	install -m 0755 sms.cgi $(TEMPINSTALL)$(WWW_DIR)/cgi-bin/
	install -m 0755 -d $(TEMPINSTALL)/usr/etc/ssmtp
	install -m 0644 ssmtp.conf $(TEMPINSTALL)/usr/etc/ssmtp/

	# tx/rx sms create script for test
	#mkdir -p $(TEMPINSTALL)/usr/sms/
	#cp -f makesmsfile.sh $(TEMPINSTALL)/usr/sms/

ifeq ($(PLATFORM),Platypus)
batch_nv: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
endif

ifeq ($(PLATFORM),$(filter $(PLATFORM), Bovine Serpent))
sms_server: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
endif

check_sp: $(SP_OBJ)
	$(CC) $(LDFLAGS) $(SP_OBJ) $(LIBS) -o $@

get_file_index: $(IDX_OBJ)
	$(CC) $(LDFLAGS) $(IDX_OBJ) $(LIBS) -o $@

install: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" \
	-D /usr -D /etc -D $(WWW_DIR)

package: all
	scatter_install.sh -s $(CROSS_COMPILE)strip "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" \
	-D /usr -D /etc -D $(WWW_DIR) --ipk CONTROL

romfs: all
ifeq ($(PLATFORM),Platypus)
	$(ROMFSINST) /usr/bin/batch_nv
endif
ifeq ($(PLATFORM),$(filter $(PLATFORM), Bovine Serpent))
	$(ROMFSINST) /usr/bin/sms_server
endif
	$(ROMFSINST) /usr/bin/check_sp

	$(ROMFSINST) /usr/bin/get_file_index

ifeq ($(PLATFORM),Platypus2)
	touch ../Internationalization/BuildFiles.c
endif

.PHONY: clean distclean
clean distclean:
	$(shell rm *.o)
	$(shell test -e batch_nv && rm batch_nv)
	$(shell test -e sms_server && rm sms_server)
	$(shell test -e check_sp && rm check_sp)
	$(shell test -e get_file_index && rm get_file_index)
	$(shell test -e sms_js_strings.inc && rm sms_js_strings.inc)
	$(shell test -e sms_commands.conf && rm sms_commands.conf)
	rm -f ../sms_tools.done
