#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR        = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path

# Set a few defaults (build for Platypus)
CROSS_COMPILE ?= /opt/cross/gcc-4.3.4-glibc-2.9/bin/arm-linux-
CDCS_INCLUDE  ?= /storage/src/products/build-Bovine-ntc_140attdev-trunk/Bovine_src/staging_l/include/
CDCS_LIB      ?= /storage/src/products/build-Bovine-ntc_140attdev-trunk/Bovine_src/staging_l/lib/

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := $(CROSS_COMPILE)strip

FILE_DBLIB := $(CDCS_LIB)/rdb.a

CFLAGS:=-Wall -g -Os -fPIC

LUA_DIR ?= /usr/share/lua/5.1
LUA_CDIR ?= /usr/lib/lua/5.1
TMP_INST_DIR := $(shell /bin/pwd)/tmp_install

# compile options
###################################################################################################
ifdef CDCS_INCLUDE_OPTS
# directly use cdcs include options but expand CDCS_INCLUDE_OPTS - replace -I<dir> with <dir> of each qmi include directories.
QMI_INCLUDE_DIR:=\
	. \
	dsutils \
	qmi \
	qmi-framework \
	qmi-idl \
	loc-api-v02

CFLAGS+=\
	$(foreach opt, \
		$(CDCS_INCLUDE_OPTS), \
		$(if \
			$(filter -I%,$(strip $(opt))), \
			$(foreach idir,$(QMI_INCLUDE_DIR),$(strip $(opt))/$(idir)), \
			$(opt) \
		) \
	)
endif # CDCS_INCLUDE_OPTS

# support NTC 3pp apps directory locations
CFLAGS+= \
	-I$(PROPRIETARY_INCLUDE)/qc \
	-I$(PROPRIETARY_INCLUDE)/qc/qmi-idl \
	-I$(PROPRIETARY_INCLUDE)/qc/qmi-framework \
	-I$(PROPRIETARY_INCLUDE)/qc/qmi \
	-I$(PROPRIETARY_INCLUDE)/qc/dsutils

# add V variables as C defines
CFLAGS+= \
	-DV_EXTRA_NAND_FLASH_SCRUB_$(V_EXTRA_NAND_FLASH_SCRUB) \
	-DV_PROCESSOR_$(V_PROCESSOR)

# preprocess V variables
PREPROC_CFLAGS=\
	-DV_PROCESSOR_$(V_PROCESSOR) \
	-DV_CUSTOM_FEATURE_PACK_$(V_CUSTOM_FEATURE_PACK) \
	-DV_RAT_SEL_$(V_RAT_SEL) \
	-DV_QMI_VOICE_$(V_QMI_VOICE) \
	-DV_QMI_IMS_$(V_QMI_IMS) \
	-DV_QMI_PBM_$(V_QMI_PBM) \
	-DV_QMI_CSD_$(V_QMI_CSD) \
	-DV_QMI_WMS_$(V_QMI_WMS) \
	-DV_NR5G_CELL_LOCK_$(V_NR5G_CELL_LOCK) \
	-DV_POLL_WDS_CHAN_RATE_$(V_POLL_WDS_CHAN_RATE) \
	-DV_MODULE_MODEL_NAME_$(V_MODULE_MODEL_NAME)

# log extra information in luaqmi.c
#CFLAGS+= -DDEBUG

# link options
###################################################################################################
ifdef CDCS_LIB_OPTS
# directly use cdcs library options
LDFLAGS+=$(CDCS_LIB_OPTS) -L$(INSTALLDIR)/usr/lib -Wl,-rpath-link,$(INSTALLDIR)/usr/lib
else
LDFLAGS+= -L$(PROPRIETARY_LIB) -Wl,-rpath-link,$(PROPRIETARY_LIB)
endif # CDCS_LIB_OPTS

LDFLAGS+=-lqmiservices -lqmi_cci

# build lua qmi header
###################################################################################################
ifdef CDCS_INCLUDE_OPTS
QMI_IDL_INCLUDE_DIR:=\
	qmi \
	qmi-idl \
	qmi-framework\
	loc-api-v02

qmi_idl_headers+=\
	$(foreach opt, \
		$(CDCS_INCLUDE_OPTS), \
		$(if \
			$(filter -I%,$(strip $(opt))), \
			$(foreach idir, \
				$(QMI_IDL_INCLUDE_DIR),$(wildcard $(patsubst -I%,%,$(strip $(opt)))/$(idir)/*.h)), \
		) \
	)
endif # CDCS_INCLUDE_OPTS
# support NTC 3pp apps directory locations
qmi_idl_headers+=$(wildcard $(PROPRIETARY_INCLUDE)/qc/qmi/*.h $(PROPRIETARY_INCLUDE)/qc/qmi-idl/*.h ./qmi_extra.h)

# exclude IDL headers based on V variable
ifneq ($(V_EXTRA_NAND_FLASH_SCRUB),qualcomm)
qmi_idl_headers_to_exclude=$(filter %/flash_driver_service_v01.h %/flash_driver_service_v02.h, $(qmi_idl_headers))
endif

# if below V variables are defined and with "none" value, remove corresponding IDL header files for saving wmmd memory
ifeq ($(V_QMI_PBM),none)
qmi_idl_headers_to_exclude+=$(filter %/phonebook_manager_service_v01.h %/phonebook_manager_service_v02.h, $(qmi_idl_headers))
endif

ifeq ($(V_QMI_IMS),none)
qmi_idl_headers_to_exclude+=$(filter %/ip_multimedia_subsystem_application_v01.h %/ip_multimedia_subsystem_application_v02.h, $(qmi_idl_headers))
qmi_idl_headers_to_exclude+=$(filter %/ip_multimedia_subsystem_dcm_v01.h %/ip_multimedia_subsystem_dcm_v02.h, $(qmi_idl_headers))
qmi_idl_headers_to_exclude+=$(filter %/ip_multimedia_subsystem_ims_common_v01.h %/ip_multimedia_subsystem_ims_common_v02.h, $(qmi_idl_headers))
qmi_idl_headers_to_exclude+=$(filter %/ip_multimedia_subsystem_presence_v01.h %/ip_multimedia_subsystem_presence_v02.h, $(qmi_idl_headers))
qmi_idl_headers_to_exclude+=$(filter %/ip_multimedia_subsystem_rtp_v01.h %/ip_multimedia_subsystem_rtp_v02.h, $(qmi_idl_headers))
qmi_idl_headers_to_exclude+=$(filter %/ip_multimedia_subsystem_settings_v01.h %/ip_multimedia_subsystem_settings_v02.h, $(qmi_idl_headers))
endif

ifeq ($(V_QMI_VOICE),none)
qmi_idl_headers_to_exclude+=$(filter %/voice_nas_services_common_v01.h %/voice_nas_services_common_v02.h, $(qmi_idl_headers))
qmi_idl_headers_to_exclude+=$(filter %/voice_service_common_v01.h %/voice_service_common_v02.h, $(qmi_idl_headers))
qmi_idl_headers_to_exclude+=$(filter %/voice_service_v01.h %/voice_service_v02.h, $(qmi_idl_headers))
endif

ifeq ($(V_QMI_CSD),none)
qmi_idl_headers_to_exclude+=$(filter %/core_sound_driver_v01.h %/core_sound_driver_v02.h, $(qmi_idl_headers))
endif

ifeq ($(V_QMI_WMS),none)
qmi_idl_headers_to_exclude+=$(filter %/wireless_messaging_service_v01.h %/wireless_messaging_service_v02.h, $(qmi_idl_headers))
endif

qmi_idl_headers := $(filter-out $(qmi_idl_headers_to_exclude), $(qmi_idl_headers))

OBJS:=luaqmi.o

.PHONY: all install install_adb clean distclean

all: libluaqmi.so qmi_call.pch qmi_msg.pch qmi_err.pch

install: all
	# install luaqmi
	install -m 0644 -D libluaqmi.so $(INSTALLDIR)/$(LUA_CDIR)/libluaqmi.so
	$(STRIP) $(INSTALLDIR)/$(LUA_CDIR)/libluaqmi.so
	ln -sf /$(LUA_CDIR)/libluaqmi.so $(INSTALLDIR)/usr/lib/libluaqmi.so
ifeq ($(PLATFORM),Cassini)
	#
	# Cassini platform is systemd-based and init.d scripts are not required.
	# On Cassini platofrm, fsextra includes systemd service files.
	#
	install -m 0755 -D wmmd_start.lua $(INSTALLDIR)/usr/bin/wmmd_start.lua
else
ifneq ($(V_PRODUCT),$(filter $(V_PRODUCT), ntc_pentecostfactory ntc_napafactory))
	install -m 0755 -D wmmd_start.lua $(INSTALLDIR)/usr/bin/wmmd_start.lua
	install -m 0755 -D "wmmd_launcher.default" $(INSTALLDIR)/usr/bin/wmmd_launcher
	-install -m 0755 -D "wmmd_launcher.$(V_CUSTOM_FEATURE_PACK)" $(INSTALLDIR)/usr/bin/wmmd_launcher
	install -m 0755 -D wmmd "$(INSTALLDIR)/etc/init.d/rc.d/wmmd"
	install -m 0755 -D set_tty_mode.sh "$(INSTALLDIR)/usr/bin/set_tty_mode.sh"
	install -m 0755 -D enable_dtmf_mute.sh "$(INSTALLDIR)/usr/bin/enable_dtmf_mute.sh"
endif
endif
	install -m 0755 -D qmisys.lua $(INSTALLDIR)/usr/bin/qmisys
	# wmmd is no longer started by init scripts, but rather by a template.
	# The longer term solution for these race conditions may be different, so keep
	# the existing structure intact.
	#mkdir -p "$(INSTALLDIR)/etc/init.d/rc3.d"
	#ln -sf ../rc.d/wmmd "$(INSTALLDIR)/etc/init.d/rc3.d/S400wmmd"
	# lua
	for i in iconv.lua; do \
		install -m 0644 -D "$$i" "$(INSTALLDIR)/$(LUA_DIR)/$$i"; \
	done
	# pch
	for i in *.pch; do \
		install -m 0644 -D "$$i" "$(INSTALLDIR)/etc/luaqmi/$$i"; \
	done
	# preprocess V variable
	mkdir -p $(TMP_INST_DIR)
	for i in *.lua; do \
		if [ "$$i" != "wmmd_start.lua" -a "$$i" != "iconv.lua" ]; then \
			($(CC) -E -x c -P -nostdinc $(PREPROC_CFLAGS) -C $$i >$(TMP_INST_DIR)/$$i); \
		fi; \
	done
	mv $(TMP_INST_DIR)/luaqmi.lua $(INSTALLDIR)/$(LUA_DIR)/
	mkdir -p $(INSTALLDIR)/$(LUA_DIR)/wmmd
	cp $(TMP_INST_DIR)/* $(INSTALLDIR)/$(LUA_DIR)/wmmd
	rm -fr $(TMP_INST_DIR)


ifneq ($(V_PRODUCT),$(filter $(V_PRODUCT), ntc_pentecostfactory ntc_napafactory))
	./install_v_variables.sh $(HBASE)/variant.sh . "$(INSTALLDIR)/$(LUA_DIR)/wmmd"
endif

install_adb: all
	# install luaqmi
	adb push libluaqmi.so /$(LUA_CDIR)/libluaqmi.so
	adb shell 'ln -sf /$(LUA_CDIR)/libluaqmi.so /usr/lib/libluaqmi.so'
	adb push wmmd "/etc/init.d/rc.d/wmmd"
	adb push wmmd_start.lua "/usr/bin/wmmd_start.lua"
	adb push qmisys.lua "/usr/bin/qmisys"
	# lua
	for i in iconv.lua; do \
		adb push "$$i" "/$(LUA_DIR)/$$i"; \
	done
	# pch
	for i in *.pch; do \
		adb push "$$i" "/etc/luaqmi/$$i"; \
	done
	# preprocess V variable and push
	mkdir -p $(TMP_INST_DIR)
	for i in *.lua; do \
		if [ "$$i" != "luaqmi.lua" -a "$$i" != "wmmd_start.lua" -a "$$i" != "iconv.lua" ]; then \
			($(CC) -E -x c -P -nostdinc $(PREPROC_CFLAGS) -C $$i >$(TMP_INST_DIR)/$$i); \
			adb push  $(TMP_INST_DIR)/$$i /$(LUA_DIR)/wmmd/$$i; \
		elif [ "$$i" == "luaqmi.lua" ]; then \
			($(CC) -E -x c -P -nostdinc $(PREPROC_CFLAGS) -C $$i >$(TMP_INST_DIR)/$$i); \
			adb push  $(TMP_INST_DIR)/$$i /$(LUA_DIR)/$$i; \
		fi; \
	done
	rm -fr $(TMP_INST_DIR)

libluaqmi.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared $< -o $@

# these targets actually have depdency on Qualcomm header
.PHONY: qmi_call.pch qmi_msg.pch

ifeq ($(filter $(V_PROCESSOR),sdx20 sdx55),)
CUSTOMISED_IDL_HEADERS+=$(PROPRIETARY_INCLUDE)/qc/qmi-idl/netcomm_v01.h
endif

qmi_call.pch: luaqmi.h $(CUSTOMISED_IDL_HEADERS)
	$(CC) $(CFLAGS) -I$(PROPRIETARY_INCLUDE) -E $< | sed "s/#.*//g;/^ *\$$/d" \
			| ./undef.sh '^struct timespec\b'  '};' \
			| ./undef.sh '^struct tm\b' '};' \
			| ./undef.sh '^struct timeval\b' '};' \
			| ./undef.sh '^struct timezone\b' '};' \
			| ./undef.sh '^struct itimerspec\b' '};' \
			| ./undef.sh '^struct itimerval\b' '};' \
			| ./undef.sh '^struct sockaddr\b' '};' \
			| ./undef.sh '^struct sockaddr_storage\b' '};' \
			| ./undef.sh '^extern int utimes\b' ';' \
			| ./undef.sh '^extern int lutimes\b' ';' \
			| ./undef.sh '^extern int futimes\b' ';' \
			| ./undef.sh '^struct sockaddr_un\b' '};' \
			| ./undef.sh '^typedef unsigned short int sa_family_t' ';' \
		> $@

qmi_err.pch: qmi_err.h
	sed -rn 's/([A-Z_0-9]+) *= *([0-9]+).*/e[\2]="\1"/p' $^ > $@

qmi_msg.pch: *.lua
	cat $(qmi_idl_headers) | ./conv_cdef_to_lua.sh > $@
	# only reserve QMI_ messages that we have used in all .lua files
	./cdef_select_used_QMI_messages.sh

%.o: %.c
	$(CC) $(CFLAGS) -I$(CDCS_INCLUDE) -I$(PROPRIETARY_INCLUDE) -c $< -o $@

# dependency
$(OBJS) qmi_call.pch: luaqmi.h Makefile

clean distclean:
	rm -f *.o *.so core *.pch *~
