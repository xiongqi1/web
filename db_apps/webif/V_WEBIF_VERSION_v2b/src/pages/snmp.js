var SNMPCfg = PageObj("SNMPCfg", "snmp configuration",
  {
#ifdef COMPILE_WEBUI // This block only required for build and not on client
    customLua: {
        get: function(arr) { // Add a file check to the object (for download MIB)
            arr.push("o.isMibFile=file_exists('/www/snmp.mib')");
            return  arr;
        },
        helpers: [
            "function isValidSnmpUserName(val)",
            " if val and #val >= 1 and #val <= 31 and val:match('^[a-zA-Z0-9]+$') then",
            "  return true",
            " end",
            " return false",
            "end"
        ]
      },
    rdbPrefix: "service.snmp.",
#endif
    members: [
      objVisibilityVariable("snmpenable", "SNMP").setRdb("enable"),
      editableBoundedInteger("snmpPort", "SNMP Port", 0, 65535, "Msg126")
        .setRdb("port.new").setSmall().setMaxLength(64),
      selectVariable("secVers", "snmpVersion",
        function(obj){return [["v1v2c","snmpVersionV1V2c"],["v3","snmpVersionV3"]];}, "onChangeVer")
        .setRdb("version"),

      editableHostname("snmpROCommunityName", "readonly community name").setRdb("name.readonly")
        .setIsVisible( function () {return SNMPCfg.obj.secVers !=="v3";}),
      editableHostname("snmpRWCommunityName", "rw community name").setRdb("name.readwrite")
        .setIsVisible( function () {return SNMPCfg.obj.secVers !=="v3";}),

      editableTextVariable("snmpEngineIdSuffix", "snmpEngineIdSuffix", {required:false})
        .setRdb("engine_id_suffix")
        .setIsVisible( function () {return SNMPCfg.obj.secVers ==="v3";})
        .setConditionsForCheck(["secVers=='v3'"])
        .setMinLength(1).setMaxLength(27).setEncode(true),

      editableTextVariable("snmpUserName", "snmpUserName").setRdb("username")
        .setIsVisible( function () {return SNMPCfg.obj.secVers ==="v3";})
        .setConditionsForCheck(["secVers=='v3'"])
        .setMaxLength(31)
        .setValidate(
            function (val) {return /^[a-zA-Z0-9]{1,31}$/.test(val);},
            "snmpUserNameValidation",
            undefined,
            "isValidSnmpUserName(v)"
          ),

      selectVariable("snmpSecurityLevel", "snmpSecurityLevel",
        function(obj){return [
                ["noAuthNoPriv", "snmpSecurityLevelNoAuthNoPriv"],
                ["authNoPriv", "snmpSecurityLevelAuthNoPriv"],
                ["authPriv", "snmpSecurityLevelAuthPriv"]
            ];}, "onChangeSec")
        .setRdb("security_level")
        .setIsVisible( function () {return SNMPCfg.obj.secVers ==="v3";})
        .setConditionsForCheck(["secVers=='v3'"]),

      selectVariable("snmpAuthProt", "snmpAuthProt",
        function(obj){return [
                ["md5", "snmpAuthProtMd5"],
                ["sha", "snmpAuthProtSha"]
            ];})
        .setRdb("auth_prot")
        .setIsVisible(show_authorisation_callback)
        .setConditionsForCheck(["secVers=='v3'", "snmpSecurityLevel ~= 'noAuthNoPriv'"]),
      editableTextVariable("snmpAuthPassphrase", "snmpAuthPassphrase").setRdb("auth_passphrase")
        .setIsVisible(show_authorisation_callback)
        .setConditionsForCheck(["secVers=='v3'", "snmpSecurityLevel ~= 'noAuthNoPriv'"])
        .setMinLength(8).setMaxLength(128).setEncode(true)
        .setValidate(function (val) { return val.length >= 8;}, "snmpAuthPassphraseValidation"),

      selectVariable("snmpPrivProt", "snmpPrivProt",
        function(obj){return [["des", "snmpPrivProtDes"], ["aes", "snmpPrivProtAes"]];})
        .setRdb("priv_prot")
        .setIsVisible(show_privacy_callback)
        .setConditionsForCheck(["secVers=='v3'", "snmpSecurityLevel == 'authPriv'"]),
      editableTextVariable("snmpPrivPassphrase", "snmpPrivPassphrase").setRdb("priv_passphrase")
        .setIsVisible(show_privacy_callback)
        .setConditionsForCheck(["secVers=='v3'", "snmpSecurityLevel == 'authPriv'"])
        .setMinLength(8).setMaxLength(128).setEncode(true)
        .setValidate(function (val) { return val.length >= 8;}, "snmpPrivPassphraseValidation"),

      buttonAction("download_mib", "download", "onClickDownloadMib", "download mib", {helperText: "snmp mib info"})
    ]
  });

function onClickDownloadMib() {
    if (SNMPCfg.obj.isMibFile) {
        location.href = "/cgi-bin/logfile.cgi?action=downloadMib";
    }
    else {
        blockUI_confirm("noMibFile");
    }
}

function onChangeVer(_this) {
    var secVers = _this.value;
#ifdef V_NON_SECURE_WARNING_y
    if (secVers !== "v3") {
        blockUI_alert(_("snmpWarningOnV1v2"));
    }
#endif
    setVisible("#div_snmpROCommunityName", secVers !== "v3");
    setVisible("#div_snmpRWCommunityName", secVers !== "v3");

    setVisible("#div_snmpEngineIdSuffix", secVers === "v3");
    setVisible("#div_snmpUserName", secVers === "v3");
    setVisible("#div_snmpSecurityLevel", secVers === "v3");
    onChangeSec(null, secVers);
}


function show_privacy_callback(_this) {
    return show_privacy_options(SNMPCfg.obj.snmpSecurityLevel, SNMPCfg.obj.secVers);
}


function show_authorisation_callback(_this) {
    return show_authorisation_options(SNMPCfg.obj.snmpSecurityLevel, SNMPCfg.obj.secVers);
}

// Return true if we need to show the SNMP privacy fields.
// If _this is not set then access the SNMPCfg.obj settings.
function show_privacy_options(sec_level, sec_version) {
    if (sec_version !== "v3") {
        return false;
    }
    return sec_level === "authPriv";
}


// Return true if we need to show the SNMP authentication fields.
// If _this is not set then access the SNMPCfg.obj settings.
function show_authorisation_options(sec_level, sec_version) {
    if (sec_version !== "v3") {
        return false;
    }
    return (sec_level === "authPriv") || (sec_level === "authNoPriv");
}

// Hide the SNMP authentication/privacy protocol and passphrase fields unless they are needed.
function onChangeSec(_this, sec_version) {
    var sec_level = _this ? _this.value : $("#sel_snmpSecurityLevel").val();
    if (!sec_version) {
        sec_version = $("#sel_secVers").val(); // get current value on the ui element
}
    setVisible("#div_snmpAuthProt", show_authorisation_options(sec_level, sec_version));
    setVisible("#div_snmpAuthPassphrase", show_authorisation_options(sec_level, sec_version));
    setVisible("#div_snmpPrivProt", show_privacy_options(sec_level, sec_version));
    setVisible("#div_snmpPrivPassphrase", show_privacy_options(sec_level, sec_version));
}

var SNMPTraps = PageObj("SNMPTraps", "snmp traps",
  {
#ifdef COMPILE_WEBUI // This block only required for build and not on client
    rdbPrefix: "service.snmp.",
#endif
    members: [
      objVisibilityVariable("snmpTrapEnable", "snmp traps").setRdb("enable_trap"),
      editableHostname("snmpTrapDestination","trap destination").setRdb("snmp_trap_dest").setMaxLength(64),
#if defined V_CUSTOM_FEATURE_PACK_Santos
      editableHostname("snmpTrapDestination0","").setRdb("snmp_trap_dest.0").setMaxLength(64),
      editableHostname("snmpTrapDestination1","").setRdb("snmp_trap_dest.1").setMaxLength(64),
      editableHostname("snmpTrapDestination2","").setRdb("snmp_trap_dest.2").setMaxLength(64),
#endif
      editableInteger("snmpHeartbeatInterval","heartbeat interval",{helperText: "seconds"}).setRdb("heartbeat_interval").setMaxLength(9),
      editableInteger("snmpTrapPersistence","trap persistence time",{helperText: "seconds"}).setRdb("trap_persist").setMaxLength(9),
      editableInteger("snmpTrapRetransmission","trap retransmission time",{helperText: "seconds"}).setRdb("trap_resend").setMaxLength(9),
      buttonAction("send_heart_beat", "send heartbeat now", "sendSNMPHeartbeat")
    ],
    decodeRdb: function (obj) {
        // This is because the snmptrapenable variable is new and might not exist.
        if(!isDefined(obj.snmpTrapEnable) || obj.snmpTrapEnable === "") {
            obj.snmpTrapEnable = "0";
        }
        return obj;
    }
});

function sendSNMPHeartbeat() {
    if (!isFormValid()) {
        return;
    }
    $.getJSON(
        "/cgi-bin/heartbeat.cgi",
        {dest:$("#inp_snmpTrapDestination").val()},
        function(res) {
            if(res.cgiresult==0) {
                $.blockUI( {
                message: "<div>"+_("heartbeat sent ok")+"\
                    <div class='button-raw med'>\
                    <button class='secondary med' onClick='$.unblockUI();'>"+_("CSok")+"</button>\
                    </div></div>", css: { width: '300px' } });
            }
            else {
                $.blockUI( {
                message: "<div>"+_("heartbeat sending failure")+"\
                    <div class='button-raw med'>\
                    <button class='secondary med' onClick='$.unblockUI();'>"+_("CSok")+"</button>\
                    </div></div>", css: { width: '300px' } });
            }
        }
    ).fail(function(jqXHR, textStatus, errorThrown) {
                $.blockUI( {
                message: "<div>"+_("heartbeat sending failure")+"<br/>"+errorThrown+"\
                    <div class='button-raw med'>\
                    <button class='secondary med' onClick='$.unblockUI();'>"+_("CSok")+"</button>\
                    </div></div>", css: { width: '300px' } });
        }
    );
}

var pageData : PageDef = {
#if defined V_SERVICES_UI_none || defined V_NETSNMP_none
  onDevice : false,
#endif
  title: "SNMP",
  menuPos: ["Services", "SNMP"],
  pageObjects: [SNMPCfg, SNMPTraps],
  alertSuccessTxt: "snmpSubmitSuccess",
  onReady: function (){
    $("#objouterwrapperSNMPTraps").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

