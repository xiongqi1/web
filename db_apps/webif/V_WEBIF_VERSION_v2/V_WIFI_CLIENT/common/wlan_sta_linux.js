// WiFi STA/Client Linux driver functions

// @todo If we add support for multiple client radios then set this.
var clientRadioInstance = 0;

#ifdef V_WIFI_CLIENT_backports
var extraStatusStr=[
	"&nbsp;",
	_("extraStatusAssocReject"),
	_("extraStatusEAPSuccess"),
	_("extraStatusEAPFailure"),
	_("extraStatusMaybeIncorrectPassword")];
#endif

function showWpaEncryptionOptions() {
  var authMode   = $("#id_network_auth").val();
  var encryption = $("#id_wpa_encryption").val();
  if ((authMode == "WPAPSK") ||
      (authMode == "WPAEAPTLS")) {
    $("#id_wpa_tkip").attr("disabled", false);
    $("#id_wpa_aes" ).attr("disabled", true);
    if (encryption == "AES") {
      $("#id_wpa_encryption").val("TKIP");
    }
  } else if ((authMode == "WPA2PSK") ||
             (authMode == "WPA2EAPTLS")) {
    $("#id_wpa_tkip").attr("disabled", true);
    $("#id_wpa_aes" ).attr("disabled", false);
    if (encryption == "TKIP") {
      $("#id_wpa_encryption").val("AES");
    }
  }
}

function checkHex(str) {
  var len = str.length;
  var i;
  for (i = 0; i < str.length; i++) {
    if ((str.charAt(i) >= '0' && str.charAt(i) <= '9') ||
        (str.charAt(i) >= 'a' && str.charAt(i) <= 'f') ||
        (str.charAt(i) >= 'A' && str.charAt(i) <= 'F')) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

function displayKeyDetails(keyNum) {
  var keyValue = $("#id_network_key" + keyNum).val();
  var keylength = keyValue.length;

  if (keylength != 0) {
    if (keylength != 10 && keylength != 26) {
      $("#id_network_key" + keyNum).css("color", "BLACK");
      $("#TEXT" + keyNum).html("");
    } else {
      $("#id_network_key" + keyNum).css("color", "GREEN");
      if (keylength == 10) {
        $("#TEXT" + keyNum).html("&nbsp;&nbsp;64 bit&nbsp;&nbsp;HEX");
      } else {
        $("#TEXT" + keyNum).html("&nbsp;&nbsp;128 bit&nbsp;&nbsp;HEX");
      }
      if ((isValidKey(keyValue, 5) == false) && (isValidKey(keyValue, 13) == false)) {
        $("#id_network_key" + keyNum).css("color", "RED");
      }
    }
    if (checkHex(keyValue) == false) {
      $("#id_network_key" + keyNum).css("color", "RED");
    }
  }
}

function isValidWpaPreSharedKey(val) {
  var ret = false;
  var len = val.length;
  var maxSize = 64;
  var minSize = 8;

  if (len >= minSize && len < maxSize) {
    ret = true;
  } else if (len == maxSize) {
    for (i = 0; i < maxSize; i++) {
      if (isHexaDigit(val.charAt(i)) == false) {
        break;
      }
    }
    if (i == maxSize) {
      ret = true;
    }
  } else {
    ret = false;
  }
  return ret;
}

function hideAllAuthorisationElements() {
  $("#pre_shared_key_ip" ).hide();
  $("#encryption_type_ip").hide();
  $("#wep_encryption_ip" ).hide();
  $("#network_key_ip"    ).hide();
  $("#wpa_eap_tls_ip"    ).hide();
  $("#eap_tls_configs_ip").hide();
  $("#eap_tls_uploads_ip").hide();
}

// There are a number of settings that depend on the network authentication setting.
// Show only the settings for the current authorisation mode.
function authModeChange(showAlertMsg) {
  var showNetworkKeys = false;

  hideAllAuthorisationElements();

  var authMode = $("#id_network_auth").val();
  switch (authMode) {

  case 'OPEN':
    $("#wep_encryption_ip").show();
    if ($("#id_wep_encryption").val() == "enabled") {
      $("#network_key_ip").show();
      if (showAlertMsg) {
        alert(_("wepExplanation"));
      }
    } else {
      if(showAlertMsg)
        alert(_("weakWepSecurityWarning"));
    }
    break;

  case 'SHARED':
    $("#network_key_ip").show();
    if (showAlertMsg) {
      alert(_("wepExplanation"));
    }
    break;

  case 'WPAPSK':
  case 'WPA2PSK':
    $("#pre_shared_key_ip"    ).show();
    $("#encryption_type_ip"   ).show();
    $("#id_wpa_pre_shared_key").focus();
    break;

  case 'WPAEAPTLS':
    if (showAlertMsg) {
      alert(_("restrictAccess"));
    }
    // Note: intentional fall-through to next case
  case 'WPA2EAPTLS':
    $("#encryption_type_ip").show();
    $("#eap_tls_configs_ip").show();
    $("#eap_tls_uploads_ip").show();
    getAllUploadFileMetadata();
    break;
  }

  // Update these in case the authorisation mode has changed.
  showWpaEncryptionOptions();
}

function displayMsgWindow(msg, id) {
  var val = $("#" + id).val();
  var msgWindow = window.open("", "", "toolbar=no,width=500,height=100");
  if (Butterlate.getLang() == "ar") {
    msgWindow.document.write("<style>body { direction:rtl; }</style>");
  }
  msgWindow.document.write(_(msg) + "<b>&nbsp;" + htmlNumberEncode(val) + "</b>");
  msgWindow.document.close();
}

function wpaPreSharedKeyWindow() {
  displayMsgWindow(_("WEPtext4"), "id_wpa_pre_shared_key");
}

function passwordWindow() {
  displayMsgWindow(_("thePassphraseIs"), "id_wpa_pki_client_key_password");
}

// Load the form input element values from the variables derived from the RDB values
function formLoad() {

  setFormElementsFromRdb();

  // Update derived radio button elements
  if ($("#id_radio").val() == '1') {
    $("#radioButton_0").attr("checked", "checked");
  } else {
    $("#radioButton_1").attr("checked", "checked");
  }

  if ($("#id_wds_sta_enable").val() == "1") {
    $("#wdsSta-0").attr("checked", "checked");
  } else {
    $("#wdsSta-1").attr("checked", "checked");
  }

  if ($("#id_auto_roam_enable").val() == "1") {
    $("#autoRoaming-0").attr("checked", "checked");
    $("#autoRoamParams").css("display", '');
  } else {
    $("#autoRoaming-1").attr("checked", "checked");
    $("#autoRoamParams").css("display", 'none');
  }

#if defined(V_WIFI_qca_soc_lsdk)
  $("#autoRoam_radio_on_off").hide();
#endif

  showWpaEncryptionOptions();

  // Show the inputs needed for the current authorisation mode.
  authModeChange(0);

  // Display the network key details
  var i;
  for (i = 1; i <= 4; i++) {
    displayKeyDetails(i);
  }

  $("#aplist_block").hide();

  //
  // Set handlers for inputs below:
  //

  // handler - short scan interval
  $("#id_short_scan_interval").keyup(function(e) {
          $(this).val($(this).val().replace(/[^0-9]/g, '')); //allow only numeric values
  });

  // handler - long scan interval
  $("#id_long_scan_interval").keyup(function(e) {
          $(this).val($(this).val().replace(/[^0-9]/g, '')); //allow only numeric values
  });

  // handler - metric
  $("#id_default_route_metric").keyup(function(e) {
          $(this).val($(this).val().replace(/[^0-9]/g, '')); //allow only numeric values
  });

  // handler - Network Authentication button
  $("#id_network_auth").change(function() {
    authModeChange(1);
  });

  // handler - WEP Encryption Type
  $("#id_wep_encryption").change(function() {
    authModeChange(1);
  });

  // handler - Network Key
  $("#id_network_key1").keyup(function() {displayKeyDetails(1);});
  $("#id_network_key2").keyup(function() {displayKeyDetails(2);});
  $("#id_network_key3").keyup(function() {displayKeyDetails(3);});
  $("#id_network_key4").keyup(function() {displayKeyDetails(4);});

  // Show the current status periodically, if the radio is on.
  if ($("#id_radio").val() == '1') {
#ifdef V_WIFI_CLIENT_qca_soc_lsdk
    var pollTime = 3000;  // 3s
#else
    var pollTime = 5000;  // 5s
#endif
    var statusPollTimer;
    var statusPoll = function() {
      $.ajax({
        url:"./cgi-bin/wlan_sta_linux_status.cgi",
        dataType: 'json',
        data: {radio:clientRadioInstance},
        success:
          function(rspData, status) {
            if (status == "success") {
              $("#statusText").html("Channel:&nbsp;" + rspData.chan +
                                    "&nbsp;&nbsp;State:&nbsp;" + rspData.state);
#ifdef V_WIFI_CLIENT_backports
              extraStatus=parseInt(rspData.extra_status);
              if (extraStatus > 0 && extraStatus < extraStatusStr.length) {
                $("#extraStatusText").html(extraStatusStr[extraStatus]);
              }
              else {
                $("#extraStatusText").html(extraStatusStr[0]);
              }
#elif V_WIFI_CLIENT_qca_soc_lsdk
              if (rspData.state.length > 0) { // WIFI Module is ready.
                  $("#scan_button").attr("disabled", false);
                  $("#scan_refresh").attr("disabled", false);
              }
              else {
                  $("#scan_button").attr("disabled", true);
                  $("#scan_refresh").attr("disabled", true);
              }
#endif
            }
            else {
              $("#statusText").html("Server Error");
#ifdef V_WIFI_CLIENT_backports
              $("#extraStatusText").html(extraStatusStr[0]);
#endif
            }
          },
        timeout: pollTime * 2,
        error:
          function(jqXHR, textStatus, errorThrown) {
#ifdef V_WIFI_CLIENT_qca_soc_lsdk
            $("#statusText").html("Channel:&nbsp;&nbsp;&nbsp;State:&nbsp;");
            if (jqXHR.status == 440)
                clearInterval(statusPollTimer);
#else
            $("#statusText").html("Comms error: " + textStatus);
#endif
          }
      });
    }
#ifdef V_WIFI_CLIENT_qca_soc_lsdk
<%if(request['QUERY_STRING'] != "processing") {%>
    statusPoll();
<%}%>
#endif
    statusPollTimer=setInterval(statusPoll, pollTime);
#ifdef V_WIFI_CLIENT_backports
    // extra status only
    var extraStatusPollTime = 1000;  // 1s
    var extraStatusPoll = function() {
      $.ajax({
        url:"./cgi-bin/wlan_sta_linux_status.cgi",
        dataType: 'json',
        data: {radio:clientRadioInstance,onlyextra:1},
        success:
          function(rspData, status) {
            if (status == "success") {
              extraStatus=parseInt(rspData.extra_status);
              if (extraStatus > 0 && extraStatus < extraStatusStr.length) {
                $("#extraStatusText").html(extraStatusStr[extraStatus]);
              }
              else {
                $("#extraStatusText").html(extraStatusStr[0]);
              }
            }
            else {
              $("#extraStatusText").html(extraStatusStr[0]);
            }
          },
        timeout: extraStatusPollTime * 2,
        error:
          function(jqXHR, textStatus, errorThrown) {
            $("#extraStatusText").html(extraStatusStr[0]);
          }
      });
    }
    setInterval(extraStatusPoll, extraStatusPollTime);
#endif
  }
}

// Called on save
function checkSettings() {

  // If both radios will be on:
  if ((apRadio == 1) && ($("#id_radio").val() == 1)) {

    // If the AP channel is auto (0) then don't allow the client radio to be turned on,
    // otherwise the client channel would change every time the AP channel automatically changed.
    if (apChannel == 0) {
      // Reset underlying element state and buttons
      $("#id_radio").val(0);
      $("#radioButton_0")[0].checked = false;
      $("#radioButton_1")[0].checked = true;
      alert(_("wlanStaAutoChanClash"));
      return 0;
    }
  }

  // If ap-follow is enabled, wds cannot be enabled
  if ((apFollow == 1) && ($("#id_wds_sta_enable").val() == 1)) {
    // Reset underlying element state and buttons
    $("#id_wds_sta_enable").val(0);
    $("#wdsSta-0")[0].checked = false;
    $("#wdsSta-1")[0].checked = true;
    alert(_("wlanStaWdsCannotBeEnabled"));
    return 0;
  }

  // Check WPA PSK length
  var authMode = $("#id_network_auth").val();
  if (authMode.indexOf("PSK") != -1) {
    if (isValidWpaPreSharedKey($("#id_wpa_pre_shared_key").val()) == false) {
      // WPA Pre-Shared Key should be between 8 and 63 ASCII characters or 64 hexadecimal digits.
      alert(_("wlan warningMsg1"));
      return 0;
    }
  }

  // Check the WEP key index and length
  if ($("#id_wep_encryption").val() == "enabled") {
    var val;
    var num = parseInt($("#id_network_key_id").val()) -1;
    if ((num >= 0) && (num < 4)) {
      val = $("#id_network_key" + (num +1)).val();
    }
    if ((val == '') && ($("#id_wep_encryption").val() != 'enabled')) {
      alert(_("wlan warningMsg12"));  // Cannot choose key that has empty value.
      return 0;
    }
    if ((val.length != 10) && (val.length != 26)) {
      alert(_("secure warningMsg22"));  // Please input 10 or 26 characters of WEP key
      return 0;
    }
  }

  // Check SSID, reset if bad
  if ($("#id_ssid").val().length > 32) {
    alert(_("wlan warningMsg5")); // SSID should not be longer than 32 characters.
    $("#id_ssid").val("");
    return 0;
  }

  return 1;
}

function submitSettings() {
  clear_alert();
  if (checkSettings()) {
#ifdef V_WEBIF_SPEC_vdf
    if(!$("#form").valid()) {
      return;
    }
#else
    if (!$("#form").validationEngine("validate")) {
      validate_alert("","");
      return;
    }
#endif
    $("#submit_form").attr("disabled", true);
#ifndef V_WIFI_CLIENT_qca_soc_lsdk
    blockUI_wait(_("GUI pleaseWait"));
#endif
    $("#form").submit();
  }
}

// Adviser users to check WiFi credentials
function adviseCheckCredentials(apConfig) {
  var authMode   = apConfig.authentication;
  var encryption = apConfig.encryption;
  var warnings;
  var extra = "";
  // identify warnings and extra message
  switch (authMode) {
  case "OPEN/SHARED":
    // encryption must be WEP in this case
    extra = _("openSharedScanningWarning");
  case "OPEN":
  case "SHARED":
    if (encryption == "WEP") {
      warnings = {
        id_network_key_id : _("pleaseCheckCurrentNetworkKey"),
        id_network_key1 : _("pleaseCheckNetworkKey1"),
        id_network_key2 : _("pleaseCheckNetworkKey2"),
        id_network_key3 : _("pleaseCheckNetworkKey3"),
        id_network_key4 : _("pleaseCheckNetworkKey4")
      };
    }
    break;
  case "WPA-PSK":
  case "WPA2-PSK":
    warnings = {
      id_wpa_pre_shared_key : _("pleaseCheckWpaPreSharedKey")
    };
    break;
  case "WPA":
  case "WPA2":
    warnings = {
      id_wpa_pki_client_identity : _("pleaseCheckIdentity"),
      id_wpa_pki_client_key_password : _("pleaseCheckPrivateKeyPassphrase")
    };
    extra = _("eaptlsRequiresCorrectKeys");
    break;
  default:
    break;
  }

  // clear previous warning or alert
  $(".note").remove();

  // show warnings and alert
  if (warnings) {
    validate_alert(_("WiFiCredentialsRequired"),_("pleaseVerifyWiFiCredentialsBeforeProceeding") + (extra? (" " + extra) : ""));

#ifdef V_WEBIF_SPEC_vdf
    $("#form").validate().showErrors(warnings);
#else
    for (var id in warnings) {
      var el = $('#'+id);
      el.validationEngine('showPrompt', warnings[id], 'error', 'topRight', true);
    }
#endif
  }
}

function fillInApInfo(apConfig) {
  // Disallow if the remote AP channel clashes with the local AP channel.
  // We are always locked to the local AP channel if the AP radio is on.
  if ((apRadio == "1") && (apFollow != "1") &&
      (apConfig.channel != apChannel)) {
    alert(_("wlanStaChanClash"));
    return;
  }

  $("#id_ssid"   ).val(Base64.decode(apConfig.ssid));
  $("#id_bssid"  ).val(apConfig.bssid);

  // Translate the authentication value and set the encryption.
  //  Note: For WPA the encryption translation is direct.
  var wep = false;
  var authMode   = apConfig.authentication;
  var encryption = apConfig.encryption;
  switch (authMode) {
  case "OPEN":
    $("#id_network_auth").val("OPEN");
    encryption = (encryption == "WEP") ? "enabled": "disabled";
    wep = true;
    break;
  case "OPEN/SHARED":
  case "SHARED":
    $("#id_network_auth").val("SHARED");
    encryption = (encryption == "WEP") ? "enabled": "disabled";
    wep = true;
    break;
  case "WPA-PSK":
    $("#id_network_auth").val("WPAPSK");
    break;
  case "WPA2-PSK":
    $("#id_network_auth").val("WPA2PSK");
    break;
  case "WPA":
    $("#id_network_auth").val("WPAEAPTLS");
    break;
  case "WPA2":
    $("#id_network_auth").val("WPA2EAPTLS");
    break;
  default:
    $("#id_network_auth").val("OPEN");
    break;
  }
  if (wep) {
    $("#id_wep_encryption").val(encryption);
  } else {
    $("#id_wpa_encryption").val(encryption);
  }
  authModeChange(0);
  adviseCheckCredentials(apConfig);
}

// List the scanned AP settings.
// Display a clickable "connect" action to copy the settings to the form.
// Note the element names used hear should match those in the cgi script.
// The expected 'apScanRsp' object structure is:
// {
//   apConfigs:[ssid:, bssid:, authentication:, encryption:, channel:, signalLevel:],
//   connectedBssid:,
//   result:
// };
function updateScanInfo(apScanRsp) {

  // Remove the security options we don't support
  for (i = 0; i < apScanRsp.apConfigs.length; i++) {
    var auth = apScanRsp.apConfigs[i].authentication;
    var enc = apScanRsp.apConfigs[i].encryption;
    if ((((auth == "WPA2"  ) || (auth == "WPA2-PSK")) && (enc == "TKIP")) ||
        (((auth == "WPA"   ) || (auth == "WPA-PSK" )) && (enc == "AES" )) ||
        (( auth == "SHARED"                         ) && (enc == "NONE"))) {
      apScanRsp.apConfigs.splice(i, 1);
    }
  }

  for (i = 0; i < apScanRsp.apConfigs.length; i++) {
    apScanRsp.apConfigs[i].real_ssid = Base64.decode(apScanRsp.apConfigs[i].ssid);
  }
  // Sort in order of SSID.
  // This makes it easy for the user to see the difference between refreshes.
  apScanRsp.apConfigs.sort(function(a, b) {
    if      (a.real_ssid == b.real_ssid) return  0;
    else if (a.real_ssid >  b.real_ssid) return  1;
    else                       return -1;
  });

  // Create a new list
  $("#tbody_aplist").empty();
  var action;
  var connected = false;
  var i;
  var network_auth = <%val=get_single('wlan_sta.0.ap.0.network_auth');%>"@@val";
  if (network_auth == "WPAPSK") {
    network_auth = "WPA-PSK";
  }
  else if (network_auth == "WPA2PSK") {
    network_auth = "WPA2-PSK";
  }
  else if (network_auth == "WPAEAPTLS") {
    network_auth = "WPA";
  }
  else if (network_auth == "WPA2EAPTLS") {
    network_auth = "WPA2";
  }
  for (i = 0; i < apScanRsp.apConfigs.length; i++) {
    // Indicate which, if any, AP is currently connected
    if (!connected && (apScanRsp.apConfigs[i].bssid == apScanRsp.connectedBssid
                    && apScanRsp.apConfigs[i].authentication == network_auth)) {
      connected = true;
      action = "<button type='button' class='secondary short noticeListButton ' id='ap_cfg_" + i + "'>" + _("Connected") + "</button>";
      $("#id_bssid").val(apScanRsp.connectedBssid);
    } else {
      action = "<button type='button' class='secondary short'                   id='ap_cfg_" + i + "'>" + _("Connect")   + "</button>";
    }
    var ap_encrypt = apScanRsp.apConfigs[i].encryption;
    if (ap_encrypt == "TKIPAES") {
      ap_encrypt = _("tkipAes");
    }
    $("#tbody_aplist").append(
      "<tr>" +
        "<td>" + (i+1)                                 + "</td>" +
        "<td>" + htmlNumberEncode(apScanRsp.apConfigs[i].real_ssid)           + "</td>" +
        "<td>" + apScanRsp.apConfigs[i].bssid          + "</td>" +
        "<td>" + apScanRsp.apConfigs[i].authentication + "</td>" +
        "<td>" + ap_encrypt + "</td>" +
        "<td>" + apScanRsp.apConfigs[i].channel        + "</td>" +
        "<td>" + apScanRsp.apConfigs[i].signalLevel    + "</td>" +
        "<td>" + action                                + "</td>" +
      "</tr>");

    setClickFuncToFillInApInfo("#ap_cfg_" + i, apScanRsp.apConfigs[i]);
  }
}

function setClickFuncToFillInApInfo(id, apConfig) {
  $(id).click(function() { fillInApInfo(apConfig); });
}

function doApScan() {
  // The scan won't work if the radio has not been on at least once.
  if ($("#id_radio").val() != 1) {
    alert(_("turnRadioOn"));
    return;
  }

  // Show scanning in progress indication
  $("#aplist_block").show();
  $("#scan_status").html(_("scanningWait"));
  $("#scan_button").attr("disabled", true);
  $("#scan_refresh").attr("disabled", true);

  // Send a get request to the server and interpret the JSON response when it arrives.
  // Indicate if it fails or times out to the user.
  // Note: The client radio instance is hardwired to 0 for now.
  $.ajax({
    url:"./cgi-bin/wlan_sta_linux_ap_scan.cgi",
    dataType: 'json',
    data: {cmd:"getApScanList", radio:clientRadioInstance},
    success:
    function(rspData, status) {
      if (status == "success") {
        if (rspData.result != 0) {
          alert(_("scanFailed"));
          $("#aplist_block").hide();
        } else {
          updateScanInfo(rspData);
        }
      } else {
        alert(_("scanServerError"));
        $("#aplist_block").hide();
      }
      // Clear progress indication
      $("#scan_status").html("");
      $("#scan_button").attr("disabled", false);
      $("#scan_refresh").attr("disabled", false);
    },
#ifdef V_WIFI_CLIENT_qca_soc_lsdk
    // Due to poor performance of WIFI system and interfaceing method,
    // it takes much more time to get information.
    timeout: 60000,
#else
    // The scan time depends on how many APs there are.
    // Times around 10 seconds have been measured.
    timeout: 15000,
#endif
    error:
      function(jqXHR, textStatus, errorThrown) {
        $("#scan_status").html("");
        //$("#scan_status").html("Comms error: " + textStatus);
        $("#scan_button").attr("disabled", false);
        $("#scan_refresh").attr("disabled", false);
      }
  });
}

function getAllUploadFileMetadata() {
  // Send a get request to the server and interpret the JSON response when it arrives.
  $.ajax({
    url:"./cgi-bin/wlan_sta_linux_uploads.cgi",
    dataType: 'json',
    data: {cmd:"GetAllMetadata"},
    success:
      function(rspData, status) {
        if (status == "success") {
          keyFileMetadata           = rspData.wpa_pki_client_private_key;
          caCertificateMetadata     = rspData.wpa_pki_ca_certificate;
          clientCertificateMetadata = rspData.wpa_pki_client_certificate;
        } else {
          var failed = "Failed to upload";
          keyFileMetadata.result           = failed;
          caCertificateMetadata.result     = failed;
          clientCertificateMetadata.result = failed;
        }
      },
    timeout: 5000,              // 5s
    error:
      function(jqXHR, textStatus, errorThrown) {
        var msg = "Comms error: " + textStatus;
        keyFileMetadata.result           = msg;
        caCertificateMetadata.result     = msg;
        clientCertificateMetadata.result = msg;
      }
  });
}

function showUploadedFileMetadata(metadata, name) {
  // Open a new window
  // - set height according to message type
  var height = "800";
  if (metadata.result === "empty") {
    height = "100";
  } else if (metadata.details.length == 0) {
    height = "200";
  }
  var win = window.open("", "", "toolbar=no,width=900,height=" + height + ",scrollbars=yes");
  win.document.write("<title>" + name + "</title>");

  if (metadata.result === "ok" ) {
    win.document.write("<b><u>" + name + " metadata</b></u></br>" +
                       "<b>Filename</b>:&nbsp"  + metadata.filename                 + "</br>" +
                       "<b>Size    </b>:&nbsp"  + metadata.size     + "&nbsp;bytes" + "</br>" +
                       "<b>Uploaded</b>:&nbsp"  + metadata.uploaded                 + "</br>");
    if (metadata.details.length != 0) {
      win.document.write("<br><b><u>" + name + " details</b></u></br>" + metadata.details);
    }
  } else if (metadata.result === "invalid" ) {
    win.document.write("<b><u>" + name + " metadata</b></u></br>" +
                       "<b>Filename</b>:&nbsp"  + metadata.filename                 + "</br>" +
                       "<b>Size    </b>:&nbsp"  + metadata.size     + "&nbsp;bytes" + "</br>" +
                       "<b>Uploaded</b>:&nbsp"  + metadata.uploaded                 + "</br>");
    win.document.write("</br><b>Uploaded file was not valid<b>");
  } else if (metadata.result === "empty" ) {
    win.document.write("No file uploaded");
  } else {
    win.document.write(metadata.result);
  }

  win.document.close();
}

var empty = {result:"empty", filename:"", size:"", uploaded:"", details:""};
var keyFileMetadata           = empty;
var caCertificateMetadata     = empty;
var clientCertificateMetadata = empty;

// To upload key and certificates via cgi
var fileUploader;

/* jQuery main function */
$(function() {
  var fileUploader = new cgi("./cgi-bin/wlan_sta_linux_uploads.cgi");

  //
  // Add click handlers
  //
  $("#radioButton_0").click(function() { $("#id_radio").val(1); });
  $("#radioButton_1").click(function() { $("#id_radio").val(0); });

  $("#wdsSta-0").click(function() { $("#id_wds_sta_enable").val(1); });
  $("#wdsSta-1").click(function() { $("#id_wds_sta_enable").val(0); });

  $("#autoRoaming-0").click(function() { $("#id_auto_roam_enable").val(1); $("#autoRoamParams").css('display', ''); });
  $("#autoRoaming-1").click(function() { $("#id_auto_roam_enable").val(0); $("#autoRoamParams").css('display', 'none'); });

  $("#scan_button").click(function() { doApScan();$(this).blur(); });

  $("#submit_form").click(function() { submitSettings(); });

  $("#scan_refresh").click(function() { doApScan();$(this).blur(); });
  $("#scan_close"  ).click(function() { $("#aplist_block").hide(); });

  // Add dynamic anchors
  $("#display_psk").attr("href", "javascript:wpaPreSharedKeyWindow()");
  $("#display_pki_key").attr("href", "javascript:passwordWindow()");

  // Send a post cmd to delete file
  function doDelete(theCmd, msg, metadata, showButton, delButton) {
    if (!confirm("Really delete the " + msg)) {
      return;
    }

    // Send a get request to the server and interpret the JSON response when it arrives.
    // Update the status text after the buttons.
    $.ajax({
      url:"./cgi-bin/wlan_sta_linux_uploads.cgi",
      dataType: 'json',
      data: {cmd:theCmd},
      success:
        function(rspData, status) {
          if (status == "success") {
            if (rspData.result === "ok" ) {
              // Clear the metadata and disable the show/delete buttons
              metadata.result = "empty";
              $("#" + showButton).attr("disabled", true);
              $("#" + delButton).attr("disabled", true);
            } else {
              metadata.result = rspData.result;
            }
          } else {
            metadata.result = "Failed to delete file";
          }
        },
      timeout: 5000,              // 5s
      error:
        function(jqXHR, textStatus, errorThrown) {
          metadata.result = "Comms error: " + textStatus;
      }
    });
  }

  // Bind to the file delete buttons to delete the files
  $("#delKeyFile").click(function() {
    doDelete("DeleteKeyFile", "key file", keyFileMetadata,
             "showKeyFile", "delKeyFile");
  });
  $("#delCaCertificate").click(function() {
    doDelete("DeleteCaCertificate", "CA Certificate", caCertificateMetadata,
             "showCaCertificate", "delCaCertificate");
  });
  $("#delClientCertificate").click(function() {
    doDelete("DeleteClientCertificate", "Client Certificate", clientCertificateMetadata,
             "showClientCertificate", "delClientCertificate");
  });

  // Send a post cmd to upload a file
  function doUpload(cmd, inputId, showButton, delButton, func) {
    $("#" + showButton).attr("disabled", true);
    $("#" + delButton).attr("disabled", true);
    fileUploader.reset();
    fileUploader.setcmd(cmd);
    fileUploader.up("#" + inputId, func, true);
  }

  function uploadDoneHook(rsp, metadata, showButton, delButton) {
    metadata.result   = rsp.result;
    metadata.filename = rsp.filename;
    metadata.size     = rsp.size;
    metadata.uploaded = rsp.uploaded;
    metadata.details  = rsp.details;

    // Enable the show/delete buttons
    $("#" + showButton).attr("disabled", false);
    $("#" + delButton).attr("disabled", false);
  };

  // Bind to the file upload buttons to send the files
  $("#uploadKeyFile").change(function() {
    doUpload("KeyFile", "uploadKeyFile", "showKeyFile", "delKeyFile",
             function(rsp) {
               uploadDoneHook(rsp, keyFileMetadata,
                              "showKeyFile", "delKeyFile");
             });
  });
  $("#uploadCaCertificate").change(function() {
    doUpload("CaCertificate", "uploadCaCertificate", "showCaCertificate", "delCaCertificate",
             function(rsp) {
               uploadDoneHook(rsp, caCertificateMetadata,
                              "showCaCertificate", "delCaCertificate");
             });
  });
  $("#uploadClientCertificate").change(function() {
    doUpload("ClientCertificate", "uploadClientCertificate", "showClientCertificate", "delClientCertificate",
             function(rsp) {
               uploadDoneHook(rsp, clientCertificateMetadata,
                              "showClientCertificate", "delClientCertificate");
             });
  });

  // Bind to the file show buttons to show the file metadata
  $("#showKeyFile").click(function() {
    showUploadedFileMetadata(keyFileMetadata, _("local private key"));
  });
  $("#showCaCertificate").click(function() {
    showUploadedFileMetadata(caCertificateMetadata, _("addprof ca cert"));
  });
  $("#showClientCertificate").click(function() {
    showUploadedFileMetadata(clientCertificateMetadata, _("addprof client cert"));
  });

#ifdef V_WIFI_CLIENT_qca_soc_lsdk
  // Wait until WIFI module is ready.
  $("#scan_button").attr("disabled", true);
  $("#scan_refresh").attr("disabled", true);
#endif

});

$(document).ready(function() {
  formLoad();
});

function validateSsid(field) {
  if (field.val() == "") {
    // SSID should not be empty.
    return _("wlan warningMsg4");
  }
}

function validateBssid(field) {
  if (field.val() != "" && !isValidMacAddress(field.val())) {
    // You've entered the MAC address incorrectly. Please try again in the format (XX:XX:XX:XX:XX:XX).
    return _("warningMsg11");
  }
}

function validateMetric(field) {
  if (field.val() < 0 || field.val() > 65535 || !isAllNum(field.val())) {
    // Metric values must be between 0 and 65535. Please try again.
    return _("Msg48");
  }
}

function validatePkiIdentity(field) {
  if (field.val() == "") {
    return _("theIdentityShouldNotBeEmpty");
  }
}
