// WiFi STA/Client Linux driver server side script (AppWeb ESP)
// Server side script (AppWeb ESP)

function validate_input() {
#if 0
	//  TODO: validate "required" and range
#endif
	tryValidateOnOffRadio(form["id_wds_sta_enable"]);
	tryValidateOnOffRadio(form["id_auto_roam_enable"]);
	tryValidateDecimalNumber(form["id_short_scan_interval"]);
	tryValidateDecimalNumber(form["id_long_scan_interval"]);
	var idNetworkAuthOpts = new Array("OPEN", "SHARED", "WPAPSK", "WPAEAPTLS", "WPA2PSK", "WPA2EAPTLS");
	if (isElementOfArray(form['id_network_auth'], idNetworkAuthOpts) == false) {
		onBadRequest();
	}
	var idWpaEncryptionOpts = new Array("NONE", "TKIP", "AES");
	if (isElementOfArray(form["id_wpa_encryption"], idWpaEncryptionOpts) == false) {
		onBadRequest();
	}
	if (form["id_bssid"] != "") {
		validateMacAddress(form["id_bssid"]);
	}
	tryValidateDecimalNumber(form["id_default_route_metric"]);
	var networkKeyIds = new Array("1", "2", "3", "4");
	if (isElementOfArray(form["id_network_key_id"], networkKeyIds) == false) {
		onBadRequest();
	}
	validateOnOffRadio(form['id_radio']);
}

// Process the form
if (request['REQUEST_METHOD'] == "POST") {

  validate_input();

  // Set the encryption type based on the authorisation and the relevant encryption value.
  var encryptionType = "NONE";
  var authMode = form["id_network_auth"];
  if (authMode == "OPEN") {
    // Map the wep encryption selection to encryption type
    var wepEncryption = form["id_wep_encryption"];
    if (wepEncryption == "disabled") {
      encryptionType = "NONE";
    }
    else {
      encryptionType = "WEP";
    }
  }
  else if (authMode == "SHARED") {
    // Always use encryption since the keys are needed for authorisation anyway.
    encryptionType = "WEP";
  }
  else {
    encryptionType = form["id_wpa_encryption"];
  }

  var apProfile = "wlan_sta.0.ap.0.";
  var staCommon = "wlan_sta.0.";
  // Set the RDBs.
  set_single_direct("-p",staCommon + "wds_sta_enable"                      , form["id_wds_sta_enable"]);
  set_single_direct("-p",staCommon + "auto_roam_enable"                    , form["id_auto_roam_enable"]);
  set_single_direct("-p",staCommon + "short_scan_interval"                 , form["id_short_scan_interval"]);
  set_single_direct("-p",staCommon + "signal_strength_threshold"           , form["id_signal_strength_threshold"]);
  set_single_direct("-p",staCommon + "long_scan_interval"                  , form["id_long_scan_interval"]);
  set_single_direct("-p",apProfile + "network_auth"                        , form["id_network_auth"]);
  set_single_direct("-p",apProfile + "wpa_pre_shared_key"                  , form["id_wpa_pre_shared_key"]);
  set_single_direct("-p",apProfile + "encryption_type"                     , encryptionType);
  set_single_direct("-p",apProfile + "ssid"                                , form["id_ssid"]);
  set_single_direct("-p",apProfile + "bssid"                               , form["id_bssid"]);
  set_single_direct("-p",apProfile + "default_route_metric"                , form["id_default_route_metric"]);
  set_single_direct("-p",apProfile + "network_key_id"                      , form["id_network_key_id"]);
  set_single_direct("-p",apProfile + "network_key1"                        , form["id_network_key1"]);
  set_single_direct("-p",apProfile + "network_key2"                        , form["id_network_key2"]);
  set_single_direct("-p",apProfile + "network_key3"                        , form["id_network_key3"]);
  set_single_direct("-p",apProfile + "network_key4"                        , form["id_network_key4"]);
  set_single_direct("-p",apProfile + "wpa_pki_client_identity"             , form["id_wpa_pki_client_identity"]);
  set_single_direct("-p",apProfile + "wpa_pki_client_private_key_password" , form["id_wpa_pki_client_key_password"]);

  // The radio RDB is the trigger and must be written last to ensure all the configs are
  // processed for this "post" method.
  set_single('wlan_sta.0.radio=' + form['id_radio']);
#ifdef V_WIFI_CLIENT_qca_soc_lsdk
  set_single('wlan_sta.0.trigger=1');
  redirect('/wlan_sta_linux.html?processing');
#else
  redirect('/wlan_sta_linux.html?success');
#endif
}
