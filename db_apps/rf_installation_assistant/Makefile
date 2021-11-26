#
# Make file for titan installation assistant
# Expects the following external variables to be set
#
# INSTALLDIR     = Root of staging tree for install
# V_*            = Variant specifiers

TEMPINSTALL := $(shell /bin/pwd)/install
# shorthand
TEMPINSTALL_CLI := $(TEMPINSTALL)/usr/share/installation_assistant/client
CC := $(CROSS_COMPILE)gcc
JSOPTIMIZER := UglifyJS/bin/uglifyjs

# The V variable must be defined to something other to None for this project to be included in the build at all.
SPECIFIC_DIR = "variants/V_TITAN_INSTALLATION_ASSISTANT/$(V_TITAN_INSTALLATION_ASSISTANT)/"

# The list of common resource files
LUA_COMMON_FILES = simulator.lua support.lua config.lua
JS_COMMON_FILES = all.js common.js
HTML_COMMON_FILES =
CSS_COMMON_FILES = style.css
PNG_COMMON_FILES = netcomm-wireless-logo.png

# The list of specific resource files. Defined based on V_TITAN_INSTALLATION_ASSISTANT variable
LUA_SPECIFIC_FILES =
JS_SPECIFIC_FILES =
JS_SPECIFIC_FILES_NO_REMOVE =
HTML_SPECIFIC_FILES =
CSS_SPECIFIC_FILES =
PNG_SPECIFIC_FILES =
RESOURCE_SPECIFIC_FILES =

ifeq ($(V_TITAN_INSTALLATION_ASSISTANT), titan)
	LUA_SPECIFIC_FILES = ia_web_server.lua data_collector.lua simulator_titan_dual_plmn.lua
	JS_SPECIFIC_FILES = create_html.js ws_client.js ajax_get.js
	HTML_SPECIFIC_FILES = scan.html adv_scan.html data_entry.html ttest.html report.html server_detail.html
else ifeq ($(V_TITAN_INSTALLATION_ASSISTANT), myna)
	LUA_SPECIFIC_FILES = ia_web_server.lua data_collector.lua cbrs.lua simulator_myna.lua
	JS_SPECIFIC_FILES = create_html.js ws_client.js ajax_get.js
	JS_SPECIFIC_FILES_NO_REMOVE = handle_qr_code_view.js rf_scan_result_processor.js orientation_data_handler.js registration_status_handler.js
	JS_SPECIFIC_FILES_NO_REMOVE += battery_handler.js keep_alive.js
	HTML_SPECIFIC_FILES = index.html scan.html data_entry.html ttest.html server_detail.html
	HTML_SPECIFIC_FILES += advanced_operations.html firmware_update.html move_to_new_location.html cbrs_install_parameters.html
	HTML_SPECIFIC_FILES += finish_installation.html mount_installation.html factory_reset.html
	PNG_SPECIFIC_FILES = loading.gif
	RESOURCE_SPECIFIC_FILES = signal_ready.mp3 favicon.ico
	CSS_SPECIFIC_FILES = custom_style.css
else ifeq ($(V_TITAN_INSTALLATION_ASSISTANT), magpie)
	LUA_SPECIFIC_FILES = ia_web_server.lua data_collector.lua cbrs.lua simulator_magpie.lua
	JS_SPECIFIC_FILES = create_html.js ws_client.js ajax_get.js
	JS_SPECIFIC_FILES_NO_REMOVE = handle_qr_code_view.js rf_scan_result_processor.js orientation_data_handler.js registration_status_handler.js
	JS_SPECIFIC_FILES_NO_REMOVE += battery_handler.js keep_alive.js
	HTML_SPECIFIC_FILES = index.html scan.html data_entry.html ttest.html server_detail.html
	HTML_SPECIFIC_FILES += advanced_operations.html firmware_update.html move_to_new_location.html cbrs_install_parameters.html
	HTML_SPECIFIC_FILES += finish_installation.html mount_installation.html factory_reset.html
	PNG_SPECIFIC_FILES = loading.gif
	RESOURCE_SPECIFIC_FILES = signal_ready.mp3 favicon.ico
	CSS_SPECIFIC_FILES = custom_style.css
else ifeq ($(V_TITAN_INSTALLATION_ASSISTANT), fisher)
	LUA_SPECIFIC_FILES = ia_web_server.lua data_collector.lua
	JS_SPECIFIC_FILES = create_html.js ws_client.js ajax_get.js
	HTML_SPECIFIC_FILES = scan.html status.html
	CSS_SPECIFIC_FILES = style.css
else ifeq ($(V_TITAN_INSTALLATION_ASSISTANT), cracknell)
	LUA_SPECIFIC_FILES = ia_web_server.lua data_collector.lua
	JS_SPECIFIC_FILES = create_html.js ws_client.js ajax_get.js
	HTML_SPECIFIC_FILES = scan.html mode2.html
endif

# The list of UI options
UI_FLAGS :=
UI_FLAGS += -DV_TITAN_INSTALLATION_ASSISTANT_CONN_$(V_TITAN_INSTALLATION_ASSISTANT_CONN)
UI_FLAGS += -DV_ORIENTATION_$(V_ORIENTATION)

# Choose customising configuration
CONF_FLAGS :=

.PHONY : all
all:
	mkdir -p $(TEMPINSTALL)
	install -m 0755 -d $(TEMPINSTALL)/usr/share/
	install -m 0755 -d $(TEMPINSTALL)/usr/share/installation_assistant/
	install -m 0755 -d $(TEMPINSTALL)/usr/share/installation_assistant/client
	install -m 0755 -d $(TEMPINSTALL)/usr/share/installation_assistant/client/js
	install -m 0755 -d $(TEMPINSTALL)/usr/share/installation_assistant/client/css
	install -m 0755 -d $(TEMPINSTALL)/usr/share/installation_assistant/client/res
	for i in $(LUA_COMMON_FILES); do \
		(install -m 755 $$i $(TEMPINSTALL)/usr/share/installation_assistant/); \
	done
	for i in $(LUA_SPECIFIC_FILES); do \
		(install -m 755 $(SPECIFIC_DIR)$$i $(TEMPINSTALL)/usr/share/installation_assistant/); \
	done
	for i in $(HTML_COMMON_FILES); do \
		(install -m 755 client/$$i $(TEMPINSTALL_CLI)); \
	done
	for i in $(HTML_SPECIFIC_FILES); do \
		(install -m 755 $(SPECIFIC_DIR)client/$$i $(TEMPINSTALL_CLI)); \
	done
	for i in $(JS_COMMON_FILES); do \
		(install -m 755 client/js/$$i $(TEMPINSTALL_CLI)); \
	done
	for i in $(JS_SPECIFIC_FILES); do \
		(install -m 755 $(SPECIFIC_DIR)client/js/$$i $(TEMPINSTALL_CLI)); \
	done
	for i in $(JS_SPECIFIC_FILES_NO_REMOVE); do \
		(install -m 755 $(SPECIFIC_DIR)client/js/$$i $(TEMPINSTALL_CLI)/js); \
	done
	for i in $(CSS_COMMON_FILES); do \
		(install -m 755 client/css/$$i $(TEMPINSTALL_CLI)/css); \
	done
	for i in $(CSS_SPECIFIC_FILES); do \
		(install -m 755 $(SPECIFIC_DIR)client/css/$$i $(TEMPINSTALL_CLI)/css); \
	done
	for i in $(PNG_COMMON_FILES); do \
		(install -m 755 client/$$i $(TEMPINSTALL_CLI)); \
	done
	for i in $(PNG_SPECIFIC_FILES); do \
		(install -m 755 $(SPECIFIC_DIR)client/$$i $(TEMPINSTALL_CLI)); \
	done
	for i in $(RESOURCE_SPECIFIC_FILES); do \
		(install -m 644 $(SPECIFIC_DIR)client/res/$$i $(TEMPINSTALL_CLI)/res); \
	done
	./install_extension.sh $(HBASE)/variant.sh variants $(TEMPINSTALL)/usr/share/installation_assistant
	# Preprocess html files
	mkdir -p $(TEMPINSTALL_CLI)/preproc
	for i in $(HTML_COMMON_FILES) $(HTML_SPECIFIC_FILES); do \
		($(CC) -E -x c -P -nostdinc $(UI_FLAGS) -C $(TEMPINSTALL_CLI)/$$i > $(TEMPINSTALL_CLI)/preproc/$$i); \
	done
	# Move the preprocessed html files to the installing directory.
	cp $(TEMPINSTALL_CLI)/preproc/* $(TEMPINSTALL_CLI)
	# Remove unused html files
	rm -rf $(TEMPINSTALL_CLI)/preproc
	# Preprocess javascript files
	$(CC) -E -x c -P -nostdinc $(UI_FLAGS) -C $(TEMPINSTALL_CLI)/all.js > $(TEMPINSTALL_CLI)/js/all_non_minimized.js
	# Optimize javascript files
	$(JSOPTIMIZER) -o $(TEMPINSTALL_CLI)/js/all.js $(TEMPINSTALL_CLI)/js/all_non_minimized.js
	# Remove unused javascript files
	for i in $(JS_COMMON_FILES) $(JS_SPECIFIC_FILES); do \
		(rm -f $(TEMPINSTALL_CLI)/$$i); \
	done

.PHONY : clean
clean:
	rm -rf $(TEMPINSTALL)
.PHONY : install
install: all
	scatter_install.sh -s true "$(TEMPINSTALL)" "$(INSTALLDIR)" "$(CDCS_LIBSTAGE)" -D /usr
