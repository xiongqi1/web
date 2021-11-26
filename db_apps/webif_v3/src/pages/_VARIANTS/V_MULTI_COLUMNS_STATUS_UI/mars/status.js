// The status page uses a different paradigm t all the other pages.
// For the moment I can't see this being used for any other pages so the framework extension is kept in this page
// Status page objects are based around columns

// This helper function wraps an status object's html with the appropriate html
// heading is the name of the status object
// html is the objects statis html
function _genColumnsWrapper(heading, html) {
    return htmlDiv({class: "box"},
                htmlDiv({class: "box-header close h2-status"}, htmlTag("h2-status", {},_(heading))) + htmlDiv({class: "row hide"}, html ));
}

// This helper function  generates the static html for the columns of info
// objName is used to generate the html ids
// columns is an array of columns that describes the data to be displayed
function _genCols(objName, columns, extra) {
    var html = "";
    columns.forEach(function(col, i) {
        var colhtml = "";
        col.members.forEach(function(mem, j){
        if (!isDefined(mem.name)) { // If an name is not given generate one from its position
            mem.name = objName + "_" + i + "_" + j;
        }
        colhtml += htmlTag("dl", {id: mem.name + "_div"},
            htmlTag("dt", {}, _(mem.hdg)) + htmlTag("dd", {id: mem.name}, ""));
        });
        // First column gets alpha attribute, last gets omega
        var attr: any = {class: "each-box" + (i === 0? " alpha": "") + (i === columns.length-1? " omega": "")};
        if (i === columns.length-1) attr.style = "border:none";
        // Add a heading if one given
        html += htmlDiv(attr, (isDefined(col.heading)? htmlTag("h3", {}, _(col.heading)): "") + colhtml);
    });
    return htmlDiv({id: objName, class:"box-content"}, html + extra);
}

// This helper gives default action for simple pageObjects
function genCols() {
    return _genColumnsWrapper(this.labelText, _genCols(this.objName, this.columns, ""));
}

// When data is received this function is called to write values to the page
// this points to a page objects
// Each generates the dynamic html for the boxes
function _populateCols(columns, obj) {
    columns.forEach(function(col) {
        col.members.forEach(function(mem){
        if (isFunction(mem.genHtml)) {
            $("#" + mem.name).html(mem.genHtml(obj));
        }
        if (isFunction(mem.isVisible)) {
            var vis = mem.isVisible(obj);
            if (vis === true)
                $("#" + mem.name + "_div").show();
            if (vis === false)
                $("#" + mem.name + "_div").hide();
        }
        });
    });
}

function add_band_list(bandName, target, source) {
    if (source.length > 0) {
        if (target.length > 0) {
            target += "<br>";
        }
        target += bandName + " Band ";
        source = source.sort(function(a, b){return a-b});
        for (var i=0;i<source.length;i++) {
            if (i > 0) {
                target += ",";
            }
            target += source[i];
        }
        target += "<br>";
    }
    return target;
}


// This helper gives default action for simple pageObjects
function populateCols() {
    _populateCols(this.columns, this.obj);
}

function isLetter(c) {
    return c.toLowerCase() != c.toUpperCase();
}

function pad(str, max) {
    return str.length < max ? pad("0" + str, max) : str;
}
function isValidValue(val) {
    return isDefined(val) && val != "" && val != "N/A";
}
/* Decodes a Unicode string into ASCII ready for printing on status page */
function UrlDecode(str) {
    var output = "";
    for (var i = 0; i < str.length; i+=3) {
        var val = parseInt("0x" + str.substring(i + 1, i + 3));
        output += String.fromCharCode(val);
    }
    return output;
}

var stsPageObjects = [];

//*****************************************************************************
// System Information
//*****************************************************************************
var SystemInfo = PageObj("StsSystemInfo", "System information",
{
#ifdef COMPILE_WEBUI
    customLua: {
        lockRdb: false,
        get: function(arr) {
            arr.push("for line in io.lines('/proc/uptime') do o.uptime = line break end");
            return arr;
        },
    },
#endif

    readOnly: true,
    column: 1,
    genObjHtml: genCols,
    pollPeriod: 1000,

    columns : [{
        heading:"System up time",
        members:[
            {hdg: "",
             genHtml: (obj) => "<span class='time' id='uptime'>"+(isDefined(obj.uptime) ? toUpTime(parseInt(obj.uptime)) : "")+"</span>"},
            // dummy for guide line
            {hdg: "", genHtml: (obj) => "<br><br><br>"},
        ]
    },
    {
        heading:"Device version",
        members:[
            {hdg: "Hardware version", genHtml: (obj) => obj.hw_ver === "" ? "1.0" : obj.hw_ver},
            {hdg: "Serial number", genHtml: (obj) => obj.serialNumber},
            {hdg: "Firmware version", genHtml: (obj) => obj.version},
        ]
    },
    {
        heading:"Cellular module",
        members:[
            {hdg: "Model", genHtml: (obj) => obj.moduleModel === "" ? "" : obj.moduleModel},
            {hdg: "Module firmware", genHtml: (obj) => {
              var verArr = obj.moduleFirmwareVersion.split(" ");
              return "<span style='display:inline-block;word-wrap:break-word;width:180px'>" + verArr[0] + "</span>"}},
            {hdg: "IMEI", genHtml: (obj) => obj.imei},
        ]
    }],
    populate: populateCols,

    members: [
        hiddenVariable("version", "sw.version"),
        hiddenVariable("hw_ver", "system.product.hwver"),
        hiddenVariable("moduleFirmwareVersion", "wwan.0.firmware_version"),
        hiddenVariable("moduleModel", "wwan.0.module_model_name"),
        hiddenVariable("serialNumber", "system.product.sn"),
        hiddenVariable("moduleHardwareVersion", "wwan.0.hardware_version"),
        hiddenVariable("imei", "wwan.0.imei")
    ]
});

stsPageObjects.push(SystemInfo);

//*****************************************************************************
// Lan Information
//*****************************************************************************
function lan_str(pos) {
    if (pos == "down") {
            return "<i class='warning-sml side'></i><span class='span side'>Down</span>";
    }
    return "<i class='success-sml side'></i><span class='span side'>Up&nbsp/&nbsp" + pos + "</span>";
}

var LanInfo = PageObj("StsLanInfo", "LAN",
{
#ifdef COMPILE_WEBUI
    customLua: {
        lockRdb: false,
        get: function(arr) {
          arr.push("for line in io.lines('/sys/class/net/eth0/operstate') do");
          arr.push("  o.port0sts = line");
          arr.push("end");
          arr.push("if (o.port0sts == 'up') then");
          arr.push("  for line in io.lines('/sys/class/net/eth0/speed') do");
          arr.push("    o.port0sts = line .. ' Mbps   /   '");
          arr.push("  end");
          arr.push("  for line in io.lines('/sys/class/net/eth0/duplex') do");
          arr.push("    if (line == 'full') then");
          arr.push("      o.port0sts = o.port0sts .. 'FDX'");
          arr.push("    else");
          arr.push("      o.port0sts = o.port0sts .. 'HDX'");
          arr.push("    end");
          arr.push("  end");
          arr.push("end");
          return arr;
        }
    },
#endif
    readOnly: true,
    column: 2,
    genObjHtml: genCols,
    pollPeriod: 1000,
    columns : [{
      members:[
          {hdg: "IP", genHtml: (obj) => obj.lan_ip + " / " + obj.lan_mask},
          {hdg: "MAC address", genHtml: (obj) => obj.eth0mac},
          {hdg: "Ethernet port status", genHtml: (obj) => lan_str(obj.port0sts)},
          {hdg: "IP passthrough host MAC", genHtml: (obj) => obj.passthrough_mac}
      ]
    }],

    populate: populateCols,

    members: [
      hiddenVariable("lan_ip","link.profile.0.address"),
      hiddenVariable("lan_mask","link.profile.0.netmask"),
      hiddenVariable("eth0mac", "system.product.mac"),
      hiddenVariable("handover_mac","service.ip_handover.mac_address"),
      hiddenVariable("handover_en","service.ip_handover.enable")
    ],

    decodeRdb: obj => {
      obj.eth0mac = obj.eth0mac.toUpperCase();
      if (obj.handover_en == "1") {
        obj.passthrough_mac = obj.handover_mac.toUpperCase();
      }
      if (!obj.passthrough_mac) {
        obj.passthrough_mac = "N/A";
      }
      return obj;
    }
});

stsPageObjects.push(LanInfo);

//*****************************************************************************
// Bluetooth information
//*****************************************************************************
var BtInfo = PageObj("StsBtInfo", "Bluetooth",
{
#ifdef COMPILE_WEBUI
    customLua: {
        lockRdb: false,
        get: function(arr) {
          arr.push("o = {}");
          arr.push("local addr = nil");
          arr.push("local retTbl = executeCommand('hciconfig hci0')");
          arr.push("for _, v in ipairs(retTbl) do");
          arr.push("  addr = v:match('^%s+BD Address: ([A-Fa-f0-9:]+).*')");
          arr.push("  if addr then break end");
          arr.push("end");
          arr.push("o.bt0mac = addr");
          return arr;
        }
    },
#endif
    readOnly: true,
    column: 2,
    genObjHtml: genCols,
    pollPeriod: 1000,
    columns : [{
      members:[
          {hdg: "MAC address", genHtml: (obj) => obj.bt0mac},
      ]
    }],

    populate: populateCols,

    members: [
      // nothing needed, but still need an empty array placeholder
    ],

    decodeRdb: obj => {
      if (typeof(obj.bt0mac)=="undefined") {
        obj.bt0mac = "";
      } else {
        obj.bt0mac = obj.bt0mac.toUpperCase();
      }
      return obj;
    }
});

stsPageObjects.push(BtInfo);

#ifdef V_WMMD_GPS_y
//*****************************************************************************
// GPS information
//*****************************************************************************
var gpsRdbPrefix = "sensors.gps.0.common.";
var GpsInfo = PageObj("StsGpsInfo", "GPS",
{
    rdbPrefix: gpsRdbPrefix,
    readOnly: true,
    column: 2,
    genObjHtml: genCols,
    pollPeriod: 1000,
    columns : [{
      members:[
          {hdg: "Latitude", genHtml: (obj) => obj.status === "success" ? obj.latitude:""},
          {hdg: "Longitude", genHtml: (obj) => obj.status === "success" ? obj.longitude:""},
          {hdg: "Altitude", genHtml: (obj) => obj.status === "success" ? obj.altitude:""},
          {hdg: "Height of geoid", genHtml: (obj) => obj.status === "success" ? obj.height_of_geoid:""},
          {hdg: "PDOP", genHtml: (obj) => obj.status === "success" ? obj.pdop:""},
          {hdg: "Horizontal uncertainty", genHtml: (obj) => obj.status === "success" ? obj.horizontal_uncertainty+" meters":""},
          {hdg: "Vertical uncertainty", genHtml: (obj) => obj.status === "success" ? obj.vertical_uncertainty+" meters":""},
      ]
    }],
    populate: populateCols,

    members: [
      hiddenVariable("status", "status"),
      hiddenVariable("latitude", "latitude_degrees"),
      hiddenVariable("longitude", "longitude_degrees"),
      hiddenVariable("altitude", "altitude"),
      hiddenVariable("height_of_geoid", "height_of_geoid"),
      hiddenVariable("pdop", "pdop"),
      hiddenVariable("horizontal_uncertainty", "horizontal_uncertainty"),
      hiddenVariable("vertical_uncertainty", "vertical_uncertainty"),
    ],

});

stsPageObjects.push(GpsInfo);
#endif

#ifdef V_DA_NIT_IF_y
//*****************************************************************************
// NIT information
//*****************************************************************************
var NitInfo = PageObj("StsNitInfo", "NIT",
{
    readOnly: true,
    column: 2,
    genObjHtml: genCols,
    pollPeriod: 1000,
    columns : [{
      members:[
          {hdg: "Azimuth", genHtml: (obj) => obj.azimuth},
          {hdg: "Downtilt", genHtml: (obj) => obj.downtilt},
      ]
    }],
    populate: populateCols,

    members: [
      hiddenVariable("azimuth", "nit.azimuth_p"),
      hiddenVariable("downtilt", "nit.downtilt_p"),
    ],

});

stsPageObjects.push(NitInfo);
#endif

//*****************************************************************************
// Cellular Information
//*****************************************************************************
var registrationStr=[
    "Not registered, searching stopped",
    "Registered, home network",
    "Not registered, searching...",
    "Registration denied",
    "Unknown",
    "Registered, roaming",
    "Registered for SMS<br/>(home network)",
    "Registered for SMS<br/>(roaming)",
    "Emergency",
    "N/A"
];

var prvSimStatus = "0";
var pincounter;
var pukRetries;
var cellularConnectionStatus = PageObj("StsCellularConnectionStatus", "Cellular connection status",
{
    readOnly: true,
    column: 1,
    genObjHtml: genCols,
    pollPeriod: 1000,

#ifdef COMPILE_WEBUI
    customLua: {
        lockRdb: false,
        get: function(arr) {
	    arr.push("local rdbBands = luardb.get('wwan.0.currentband.current_selband') or ''");
            arr.push("local rawBandList = rdbBands:explode(';')");
            arr.push("local lte_BandList = {}");
	    arr.push("for _, v in ipairs(rawBandList) do");
            arr.push("  if v:find('all') == nil then");
            arr.push("    if v:find('LTE Band') then");
	    arr.push("      table.insert(lte_BandList, v:match('LTE Band (%d+).*'))");
            arr.push("    end");
            arr.push("  end");
            arr.push("end");
	    arr.push("o.lte_BandList = lte_BandList");
        return arr;
        },
    },
#endif

    columns : [{
      members:[
        {hdg: "SIM status", name: "simID"},
        {hdg: "Signal strength (dBm)",
            genHtml: (obj) => {
            var csq = obj.csq;
            var connType = obj.connType
            csq = csq.substring(0, csq.indexOf("dBm") );
            var csqstr = "(not available)";
            var imageidx;
            var signalBar = [-70, -80, -90, -100, -120];
            if (obj.signalBar != "") {
                signalBar = obj.signalBar.split(',');
            }
            if(parseInt(csq) == 0)
                imageidx = 6;
            else if(parseInt(csq) > signalBar[0]) {
                csqstr = "(High)";
                imageidx = 1;
            } else if(parseInt(csq) > signalBar[2]) {
                csqstr = "(Medium)";
                if(parseInt(csq) > signalBar[1])
                    imageidx = 2;
                else
                    imageidx = 3;
            } else if(parseInt(csq) > signalBar[4]) {
                csqstr = "(Low)";
                if(parseInt(csq) > signalBar[3])
                    imageidx = 4;
                else
                    imageidx = 5;
            } else {
                imageidx = 6;
            }

            var umtsss = "&nbsp;";
            var csqImg = "&nbsp;";
            if(csq=="") {
                imageidx = 6;
            }
            else {
                umtsss = csq + " dBm     " + csqstr;
            }

            if(imageidx < 6) {
            csqImg = "<i class='connection-sml connection-sml-level" + imageidx + "'></i>";
            }
            else {
            umtsss = "";
            csqImg = "";
            }
            return '<span>' + umtsss + '</span><span>' + csqImg + '</span>';
        }},
        {hdg: "Network registration status", name: "networkRegistration"}
      ]},
    {
      members:[
        {hdg: "Operator selection", genHtml: (obj) => obj.operationMode},
        {hdg: "Provider",
            genHtml: (obj) => {
            var provider = obj.provider;
            if (provider.charAt(0) == "%") {
                provider = UrlDecode(provider).replace("&","&#38");
            }

            if(provider == "Limited Service") {
                provider=_("limited service");
            }
            else if (provider == "N/A") {
                provider=_("na");
            }
            return provider;
            }
        },
        {hdg: "Roaming status", genHtml: (obj) => obj.roaming}
      ]},
    {
      members:[
        {hdg: "Allowed bands", genHtml: (obj) => {
	        var bandList = "";
		bandList = add_band_list("LTE", bandList, obj.lte_BandList);
		return bandList;
            }},
        {hdg: "Current band", genHtml: (obj) => obj.currBand.replace("IMT2000", "WCDMA 2100")},
        {hdg: "Connection (RAT)", genHtml: (obj) => {
                return obj.coverage.toUpperCase();
            }},
        {hdg: "Coverage", genHtml: (obj) => {
                return obj.coverage.toUpperCase();
            }},
      ]}
    ],

    populate: function () {
        var obj = this.obj;
        var simStatus = obj.simStatus;
        var networkRegistration = obj.networkRegistration;
        if( simStatus!="SIM OK" ) {
            networkRegistration=9; //N/A
        }

        $("#networkRegistration").html(registrationStr[networkRegistration]);

        switch(parseInt(networkRegistration)) {
            case 0:
            case 2:
            case 9:
            obj.freq = "N/A";
            break;
        }
        // roaming
        var roaming;
        var hint = obj.hint;
        if( isValidValue(hint)) {
            var patt=/%[0-1][0-9a-f]%/gi
            hint = hint.replace(patt, "%20%") // Some SIM cards have character less than %20. This ruins decoding rules.
            roaming = UrlDecode(hint);
        } else {
            roaming = "";
        }
        if(typeof(networkRegistration)!="undefined" && (networkRegistration==5)) {
            if(roaming!="")
                roaming = roaming+"&nbsp;&nbsp;-&nbsp;&nbsp;";
            roaming = roaming+"<font style='color:red'>Roaming</font>";
        } else {
            roaming = "Not roaming";
        }
        obj.roaming = roaming;

        //---- SIM Status
        var simStatusID: any =document.getElementById('simID');
        simStatusID.style.color = "var(--CasaOrange)";  // Casa orange color
        if( (simStatus.indexOf("SIM locked")!= -1)||(simStatus.indexOf("incorrect SIM")!= -1)||		(simStatus.indexOf("SIM PIN")!= -1) ) {
            simStatus = "SIM locked";
            $("#simID").html("Detecting SIM...<i class='progress-sml'></i>");
        } else if( (simStatus.indexOf("CHV1")!= -1) || (simStatus.indexOf("PUK")!= -1) ) {
            pincounter=3;
            if (pukRetries != "0") {
                simStatus = "SIM PUK locked";
            } else {
                simStatus = "SIM invalidated";
            }
        } else if( (simStatus.indexOf("MEP")!= -1) ) {
            simStatus = "MEP locked";
            pincounter=3;
        }

        if(simStatus == "SIM OK") {
            simStatusID.style.color = 'GREEN';
            $("#simID").html("SIM OK<i class='success-sml'></i>");
        } else if(simStatus == "N/A" || simStatus == "Negotiating") {
            obj.MCC=obj.MNC=obj.IMSI=obj.simICCID=_("na");
            if (pukRetries != "0") {
                if(obj.manualroamResetting=="1") {
                    $("#simID").html("Resetting modem<i class='progress-sml'></i>");
                } else {
                    $("#simID").html("Detecting SIM...<i class='progress-sml'></i>");
                }
            } else {
                $("#simID").html("SIM hardware error<i class='warning-sml'></i>");
            }
        }
        else if(simStatus == "SIM not inserted" || simStatus == "SIM removed") {
            obj.MCC=obj.MNC=obj.IMSI=obj.simICCID=_("na");
            if (pukRetries != "0") {
            $("#simID").html("SIM is not inserted<i class='warning-sml'></i>");
            }
            else {
            $("#simID").html(_("puk warningMsg1")+"<i class='warning-sml'></i>");
            }
        } else {
            if( prvSimStatus != simStatus ) {
                if( ++ pincounter > 1) {
                    prvSimStatus = simStatus;
                    simStatusID.style.color = "var(--CasaOrange)";  // Casa orange color
                    switch(simStatus) {
                        case "SIM removed":
                            blockUI_alert_l("Please insert an active SIM card in order to connect. It will take around a minute for the router to restart after you insert the SIM.", function(){return;});
                            break;
                        case "SIM general failure":
                            $("#simID").html(simStatus);
                            blockUI_alert_l("SIM general failure", function(){return;});
                        break;
                        case "SIM locked":
                            /* only when autopin is disabled, we redirect the page */
                            if(obj.autopin=="0") {
                                $("#simID").html("SIM is PIN locked<i class='warning-sml'></i>");
                                blockUI_alert_l("You must enter your PIN code to unlock and use the SIM.", function(){window.location.href="/sim_security.html";});
                            }
                            break;
                        case "SIM PUK locked":
                            $("#simID").html("SIM is PUK locked<i class='warning-sml'></i>");
                            blockUI_alert_l("You must enter your PUK code to unlock and use the SIM.", function(){window.location.href="/sim_security.html";});
                            break;
                        case "MEP locked":
                            $("#simID").html(simStatus);
                            blockUI_confirm("You must enter the network unlock code on the network unlock page to continue", function(){window.location.href="/sim_security.html?%4d%45%50%4c%6f%63%6b";});
                            break;
                        case "PH-NET PIN":
                        case "SIM PH-NET":
                            $("#simID").html(simStatus); // TODO - There was a window.location="/pinsettings.html" with session check;
                            break;
                        case "Network reject - Account":
                            $("#simID").html(simStatus);
                            blockUI_alert_l("Network reject - account", function(){return;});
                            break;
                        case "Network reject":
                            $("#simID").html(simStatus);
                            blockUI_alert_l("Network reject", function(){return;});
                            break;
                        case "SIM invalidated":
                            $("#simID").html("SIM hardware error<i class='warning-sml'></i>");
                            blockUI_alert_l("SIM hardware error. Please contact your customer care center.", function(){return;});
                            break;
                        default:
                            $("#simID").html(simStatus);
                            pincounter = 0;
                            break;
                    }
                }
            }
        }
        //---- End of SIM Status

        _populateCols(this.columns, this.obj);
    },

    members: [
        hiddenVariable("networkRegistration", "wwan.0.system_network_status.reg_stat"),
        hiddenVariable("simStatus", "wwan.0.sim.status.status"),
        hiddenVariable("provider", "wwan.0.system_network_status.network"),
        hiddenVariable("connType", "wwan.0.system_network_status.service_type"),
        hiddenVariable("csq", "wwan.0.radio.information.signal_strength"),
        hiddenVariable("coverage", "wwan.0.system_network_status.system_mode"),
        hiddenVariable("hint", "wwan.0.system_network_status.hint.encoded"),
        hiddenVariable("simICCID", "wwan.0.system_network_status.simICCID"),
        hiddenVariable("manualroamResetting", "manualroam.resetting"),
        hiddenVariable("IMSI", "wwan.0.imsi.msin"),
        hiddenVariable("autopin", "wwan.0.sim.autopin"),
        hiddenVariable("operationMode", "wwan.0.PLMN_selectionMode"),
        hiddenVariable("selBand", "wwan.0.currentband.current_selband"),
        hiddenVariable("currBand", "wwan.0.system_network_status.current_band"),
        hiddenVariable("signalBar", "system.signal_bar_threshold"),
    ]
}
);

stsPageObjects.push(cellularConnectionStatus);

//*****************************************************************************
// Wwan Information
//*****************************************************************************
function getWWanColumns(i) {
    return [
        {
        members:[
            {hdg: "Profile name", genHtml: (obj) => obj.name},
            {hdg: "Status", genHtml: (obj) => obj.status_ipv4},
            {hdg: "IPv6 status", genHtml: (obj) => obj.status_ipv6},
            {hdg: "", genHtml: (obj) => "<br><br><br><br><br>"},
        ]
        },
        {
        members:[
            {hdg: "WWAN IP", genHtml: (obj) => obj.iplocal},
            {hdg: "DNS server", genHtml: (obj) => {return obj.dns1+'<br>'+obj.dns2;}},
            {hdg: "WWAN IPv6", genHtml: (obj) => obj.ipv6_ipaddr},
            {hdg: "IPv6 DNS server", genHtml: (obj) => {
              var dnsStr = "";
              if (obj.ipv6_dns1 != "::") {
                dnsStr = obj.ipv6_dns1;
              }
              if (obj.ipv6_dns2 != "::") {
                dnsStr += '<br>'+obj.ipv6_dns2;
              }
              return dnsStr;}},
        ]
        },
        {
        members:[
            {hdg: "APN", genHtml: (obj) => obj.apn},
            {hdg: "Connection uptime", genHtml: (obj) => obj.conn_uptime}
        ]
        }
    ]
}

var WWanStatus = PageObj("StsWWanStatus", "WWAN connection status",
{
#ifdef COMPILE_WEBUI
    customLua: {
        lockRdb: false,
        get: function() {
        var luaScript = `
            o = {}
            local NumProfiles = luardb.get("wwan.0.max_sub_if") or 6
            o.NumProfiles = NumProfiles
            for pf=1,NumProfiles do
                local info = {}
                info.__id = pf
                info.name = luardb.get("link.profile."..pf..".name") or ""
                info.status_ipv4 = luardb.get("link.profile."..pf..".status_ipv4") or ""
                info.status_ipv6 = luardb.get("link.profile."..pf..".status_ipv6") or ""
                info.iplocal = luardb.get("link.profile."..pf..".iplocal") or ""
                info.dns1 = luardb.get("link.profile."..pf..".dns1") or ""
                info.dns2 = luardb.get("link.profile."..pf..".dns2") or ""
                info.ipv6_ipaddr = luardb.get("link.profile."..pf..".ipv6_ipaddr") or ""
                info.ipv6_dns1 = luardb.get("link.profile."..pf..".ipv6_dns1") or ""
                info.ipv6_dns2 = luardb.get("link.profile."..pf..".ipv6_dns2") or ""
                info.apn = luardb.get("link.profile."..pf..".apn") or ""
                info.enable = luardb.get("link.profile."..pf..".enable") or "0"
                info.uptime = luardb.get("link.profile."..pf..".sysuptime_at_ifdev_up") or ""
                table.insert(o, info)
            end
            for line in io.lines('/proc/uptime') do o.uptime = line break end
`;
        return luaScript.split("\n");
        }
    },
#endif

    readOnly: true,
    column: 1,
    columns: [],

    genObjHtml: function () {
        var html = [];
        for (var i = 0; i < 6; i++) {
            var columns = getWWanColumns(i);
            html.push(_genCols(this.objName + "_" + i, columns, ""));
            this.columns.push(columns);
        }
        return _genColumnsWrapper(this.labelText, html.join('<div class="hr" style="top:0; margin:0;"></div>'));
    },

    pollPeriod: 1000,

    populate: function () { //objs is an array of objects
        var _this = this;
        var obj = this.obj;
        for (var i = 1; i <= obj.NumProfiles; i++) {
            var columns = _this.columns[obj[i].__id-1];
            if (columns) {
                _populateCols(columns, obj[i]);
            }
            if (obj[i].enable == '1') {
                $("#StsWWanStatus_"+(i-1)).show();
            } else {
                $("#StsWWanStatus_"+(i-1)).hide();
            }
        };
    },

    decodeRdb: function (obj) {
        for (var i = 1; i <= obj.NumProfiles; i++) {
            var po_uptime = parseInt(obj[i].uptime);
            if (isNaN(po_uptime)) {
                obj[i].conn_uptime = "";
            } else {
                obj[i].conn_uptime = toUpTime(parseInt(obj.uptime) - po_uptime);
            }
        }
        return obj;
    },

    members: [
        hiddenVariable("NumProfiles", "")
    ]
});

stsPageObjects.push(WWanStatus);

//*****************************************************************************
// Advanced Information
//*****************************************************************************
function dBmFmt(v, units, thres) {
    if (!isDefined(thres))
        thres = -120;
    var val=parseInt(v);
    if (val != NaN && val <= thres)
        return _("na");
    return v + units;
}

var advStatus = PageObj("StsAdvStatus", "Advanced status",
{
    readOnly: true,
    column: 1,
    genObjHtml: genCols,
    pollPeriod: 1000,

    columns : [
    // Common column
    {
        members:[
          {hdg: "Mobile country code", genHtml: (obj) => obj.MCC},
          {hdg: "Mobile network code", genHtml: (obj) => pad(obj.MNC, 2)},
          {hdg: "SIM ICCID", genHtml: (obj) => obj.simICCID},
          {hdg: "IMSI", genHtml: (obj) => obj.IMSI},
          {hdg: "Packet service status", genHtml: (obj) => obj.psAttached},
          // dummy for guide line
   //       {hdg: "", genHtml: (obj) => "<br><br><br><br><br><br><br><br><br><br><br>"},
        ]
    },

    // LTE column 1
    {
        members:[
          {hdg: "eNodeB", genHtml: (obj) => obj.enodeB},
          {hdg: "Cell ID", genHtml: (obj) => obj.cellId},
          {hdg: "PCI", genHtml: (obj) => obj.pci},
          {hdg: "Channel number (EARFCN )", genHtml: (obj) => obj.earfcn},
          {hdg: "Scell band", genHtml: (obj) => obj.scellBand, isVisible: (obj) => obj.scellBand != ""},
          {hdg: "Scell PCI", genHtml: (obj) => obj.scellPci, isVisible: (obj) => obj.scellPci != ""},
          {hdg: "Scell channel number (EARFCN)", genHtml: (obj) => obj.scellEarfcn, isVisible: (obj) => obj.scellEarfcn != ""},
        ]
    },

    // LTE column 2
    {
        members:[
          {hdg: "Reference Signal Received Power (RSRP)",
           genHtml: (obj) => dBmFmt(obj.rsrp, " dBm", -140)},
          {hdg: "Reference Signal Received Quality (RSRQ)",
           genHtml: (obj) => dBmFmt(obj.rsrq, " dB", -140)},
          {hdg: "Signal to Interference plus Noise Ratio (SINR)",
           genHtml: (obj) => dBmFmt(obj.snr, " dB", 0)},
        ]
    }
    ],

    populate: function () {
        _populateCols(this.columns, this.obj);
    },

    decodeRdb: function (obj) {
    if (obj.psAttached == "1") {
        obj.psAttached = "Attached";
    } else {
        obj.psAttached = "Detached";
    }
    return obj;
    },

    members: [
        hiddenVariable("MCC", "wwan.0.system_network_status.MCC"),
        hiddenVariable("MNC", "wwan.0.system_network_status.MNC"),
        hiddenVariable("simICCID", "wwan.0.system_network_status.simICCID"),
        hiddenVariable("IMSI", "wwan.0.imsi.msin"),
        hiddenVariable("psAttached", "wwan.0.system_network_status.attached"),
        hiddenVariable("enodeB", "wwan.0.system_network_status.eNB_ID"),
        hiddenVariable("cellId", "wwan.0.system_network_status.CellID"),
        hiddenVariable("pci", "wwan.0.system_network_status.PCID"),
        hiddenVariable("earfcn", "wwan.0.system_network_status.channel"),
        hiddenVariable("rsrp", "wwan.0.signal.0.rsrp"),
        hiddenVariable("rsrq", "wwan.0.signal.rsrq"),
        hiddenVariable("snr", "wwan.0.signal.snr"),
        hiddenVariable("scellBand", "wwan.0.system_network_status.lte_ca_scell.list.1.band"),
        hiddenVariable("scellPci", "wwan.0.system_network_status.lte_ca_scell.list.1.pci"),
        hiddenVariable("scellEarfcn", "wwan.0.system_network_status.lte_ca_scell.list.1.freq"),
    ]
});

stsPageObjects.push(advStatus);

//*****************************************************************************
// Cell Information
//*****************************************************************************
function genCellInfoTable() {
    var table = '<table id="cellInfo">'+
    '<colgroup><col width="150px"><col width="150px"><col width="150px"><col width="150px"><col width="150px"></colgroup>'+
    '<thead><tr><th class="cell">PCI</th><th class="cell">EARFCN</th><th class="cell">RSRP</th><th class="cell">RSRQ</th><th class="cell">Serving</th></tr>'+
    '</thead><tbody></tbody></table>'
    return table;
}

#ifdef COMPILE_WEBUI
var customLuaCellInfo = {
  lockRdb : false,
  get: function() {
    return ["=getRdbArray(authenticated,'wwan.0.cell_measurement',0,49,true,{''})"]
  }
};
#endif

var cellInfo = PageObj("cellInfo", "Neighbouring cell information",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaCellInfo,
#endif
  readOnly: true,
  column: 1,
  columns: [],
  genObjHtml: function () {
    var html = [];
    html.push(genCellInfoTable());
    return _genColumnsWrapper(this.labelText, html.join('<div class="hr"></div>'));
  },
  pollPeriod: 1000,

  decodeRdb: function(objs: any[]) {
    var newObjs = [];
    objs.forEach(function(obj){
      // rawdata example
      // type, earfcn, pci, rsrp, rsrq
      // E,900,0,-113.0,-5.5
      var ar = obj.rawdata.split(",");
      var newObj: any = {};
      newObj.__id = obj.__id;
      newObj.cellPci = ar[2];
      newObj.cellEarfcn = ar[1];
      newObj.cellRsrp = ar[3];
      newObj.cellRsrq = ar[4];
      // TODO : 5G serving cell type should be defined in backend
      if (ar[0] == 'E') {
        newObj.cellServ = 'LTE';
      } else if (ar[0] == 'U') {
        newObj.cellServ = 'UMTS';
      } else if (ar[0] == 'G') {
        newObj.cellServ = 'GSM';
      } else if (ar[0] == 'N') {
        newObj.cellServ = 'NR5G';
      } else {
        newObj.cellServ = 'N/A';
      }
      newObjs[newObjs.length] = newObj;
    });
    return newObjs;
  },

  // Should use populateTable instead of pageObj populate function
  populate: populateTable,

  members: [
    staticTextVariable("cellPci", "PCI"),
    staticTextVariable("cellEarfcn", "EARFCN"),
    staticTextVariable("cellRsrp", "RSRP"),
    staticTextVariable("cellRsrq", "RSRQ"),
    staticTextVariable("cellServ", "Serving"),
  ]
});

stsPageObjects.push(cellInfo);

var pageData : PageDef = {
    title: "Status",
    menuPos: ["Status", "TR"],
    authenticatedOnly: false,
    multiColumn: true,
    pageObjects: stsPageObjects,
    onReady: function (){
      $.getScript( "web_server_status",
        function(){ setLogoffVisible(loginStatus=="login"); }
      );
    }
}
