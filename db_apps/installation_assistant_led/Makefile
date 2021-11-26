#
# Make file for titan installation assistant led controller
# Expects the following external variables to be set
#
# INSTALLDIR     = Root of staging tree for install
# V_*            = Variant specifiers

TEMPINSTALL := $(shell /bin/pwd)/install

common_files = ia_led_controller.lua ia_led_common.lua

mode_platform_file = ia_led_mode_$(V_INSTALLATION_ASSISTANT_LED).lua
ifeq ($(wildcard $(mode_platform_file)),)
    $(error "Mode file $(mode_platform_file) does not exist")
endif

# mode_platform_file will be installed using this platform independent name
mode_install_file = ia_led_mode.lua

.PHONY : all
all:
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/share/
	install -m 0755 -d $(TEMPINSTALL)/usr/share/installation_assistant/
	install -m 755 $(common_files) $(TEMPINSTALL)/usr/share/installation_assistant/
	install -m 755 $(mode_platform_file) $(TEMPINSTALL)/usr/share/installation_assistant/$(mode_install_file)

.PHONY : install
install: all
	scatter_install.sh -s true "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr
