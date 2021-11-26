/*
 * DHCP profile settings
 *
 * Copyright (C) 2020 Casa Systems
 */

var lanIp;
var lanMask;

// Perform DHCP address range and LAN IP address cross check
// Ported from legacy v2 WEBUI and this logic was verified already
// Sanity check DHCP start and end address
// @params obj A target page object
// @return Error message if the validity check fails
function lanAddrCrossCheck(obj: any) {
  // if the dhcp is enabled then validate all the DHCP values...
  var startAddr = obj.startRange.split(",")[0];
  var endAddr = obj.endRange;
  if (obj.enable == "1") {
    if (!checkIPrange(startAddr, endAddr, endAddr)) {
      return "invalidDHCPange";
    }
    if (!isWithinHostIpRange(lanIp, lanMask, startAddr)) {
      return "dhcp warningMsg14";
    }
    if (!isWithinHostIpRange(lanIp, lanMask, endAddr)) {
      return "dhcp warningMsg15";
    }
  }
}

// Custom submit function for the save button in this page
// Do DHCP address range and LAN IP address cross check
function onClickSaveBtnDhcpCfg(){
  // Get the object to validate
  var obj = DhcpCfg.packageObj();
  var errMsg = lanAddrCrossCheck(obj);
  if (isDefined(errMsg)) {
    validate_alert("", _(errMsg));
    return;
  }
  sendSingleObject(DhcpCfg);
}

var dhcpEnableToggle = objVisibilityVariable("enable", "DHCP").setRdb("service.dhcp.enable");
var ipPassthroughActivated = true;
var dhcpMustEnableNotice = noticeText("dhcpMustEnableNotice", _("As IP passthrough is enabled, DHCP must be enabled. When WWAN is in connected state, WWAN IP address is assigned in DHCP responses to the LAN device."));
dhcpMustEnableNotice.setIsVisible(() => ipPassthroughActivated);

var DhcpCfg = PageObj("DhcpCfg", "DHCP configuration",
{
#ifdef COMPILE_WEBUI
  customLua: {
    get: (arr) => [...arr, `o.ipPassthroughActivated = isIpPassThroughActive()`]
  },
#endif
  members: [
    hiddenVariable("ipPassthroughActivated", "").setReadOnly(true),
    dhcpMustEnableNotice,
    dhcpEnableToggle,
    editableIpAddressVariable("startRange", "DHCP start range").setRdb("service.dhcp.range.0"),
    editableIpAddressVariable("endRange", "DHCP end range"),
    editableBoundedInteger("leaseTime", "DHCP lease time (seconds)", 120, 99999, "Minimum lease time is 120 seconds", {helperText: "(>=120)"})
      .setRdb("service.dhcp.lease.0")
      .setMaxLength(5),
#ifdef NO_USE_UNTIL_BACKEND_READY
    editableTextVariable("domainSuffix", "Default domain name suffix")
      .setRdb("service.dhcp.suffix.0")
      .setRequired(false),
    editableIpAddressVariable("dns1Addr", "DNS server 1 IP address").setRdb("service.dhcp.dns1.0"),
    editableIpAddressVariable("dns2Addr", "DNS server 2 IP address").setRdb("service.dhcp.dns2.0"),
    editableIpAddressVariable("wins1", "WINS server 1 IP address")
      .setRdb("service.dhcp.win1.0")
      .setRequired(false),
    editableIpAddressVariable("wins2", "WINS server 2 IP address")
      .setRdb("service.dhcp.win2.0")
      .setRequired(false),
    editableIpAddressVariable("ntpServer", "NTP server (option 42)")
      .setRdb("service.dhcp.ntp_server.0")
      .setRequired(false),
    editableTextVariable("tftpServer", "TFTP server (option 66)")
      .setRdb("service.dhcp.tftp_server.0")
      .setRequired(false),
    editableTextVariable("option150", "DHCP option 150")
      .setRdb("service.dhcp.option150.0")
      .setRequired(false),
    editableTextVariable("option160", "DHCP option 160")
      .setRdb("service.dhcp.option160.0")
      .setRequired(false),
#endif
    buttonAction("dhcpCfgSaveButton", "Save", "onClickSaveBtnDhcpCfg();", "", {buttonStyle: "submitButton"}),
    hiddenVariable("lanIp", "link.profile.0.address").setReadOnly(true),
    hiddenVariable("lanMask", "link.profile.0.netmask").setReadOnly(true),
    hiddenVariable("dhcpCfgTrig", "service.dhcp.trigger"),
  ],
  // Called before the data is sent to device.
  encodeRdb: function(obj) {
    obj.startRange = obj.startRange + "," + obj.endRange;
    obj.dhcpCfgTrig = "1";
    return obj;
  },
  decodeRdb: function(obj) {
    var range = obj.startRange.split(",");
    obj.startRange = range[0];
    obj.endRange = range[1];
    lanIp = obj.lanIp;
    lanMask = obj.lanMask;

    // if IP passthrough is enabled, DHCP must be enabled
    ipPassthroughActivated = obj.ipPassthroughActivated == "1";
    if (ipPassthroughActivated) {
      dhcpEnableToggle.setEnable(false);
      obj.enable = "1";
      StaticDhcp.setPageObjectVisible(false);
      ClientList.setPageObjectVisible(false);
    } else {
      StaticDhcp.setPageObjectVisible(true);
      ClientList.setPageObjectVisible(true);
    }

    return obj;
  },
});


#ifdef COMPILE_WEBUI
var customLuaGetClientList = {
  lockRdb: false,
  get: function(arr) {
      arr.push("o = {}");
      arr.push("f = io.open('/var/run/data/dnsmasq.leases')")
      arr.push("if (f) then");
      arr.push("  for line in f:lines() do");
      arr.push("    local leaseArr = line:explode(' ')")
      arr.push("    local lease = {}")
      arr.push("    local expiry = tonumber(leaseArr[1])");
      arr.push("    if not expiry then break end");
                    // checking the change of the system time
                    // 1585699200 : 2020/04/01 12:00:00 AM
      arr.push("    if expiry < 1585699200 then");
                    // the endtime was setup before the system time updated,
                    // adjust the leasetime by time offset.
      arr.push("      local uptime");
      arr.push("      for line in io.lines('/proc/uptime') do");
      arr.push("        local timeArr = line:explode(' ')");
      arr.push("        uptime = math.floor(tonumber(timeArr[1]))");
      arr.push("        break");
      arr.push("      end");
      arr.push("      local timeTbl = executeCommand('date +%s')");
      arr.push("      timeOffset = tonumber(timeTbl[1]) - uptime");
      arr.push("      expiry = expiry + timeOffset");
      arr.push("    end");
      arr.push("    local cmd = 'date -d @'..expiry");
      arr.push("    local timeTbl = executeCommand(cmd)");
      arr.push("    lease.lexpiry = timeTbl[1]")
      arr.push("    lease.lmac = leaseArr[2]")
      arr.push("    lease.lip = leaseArr[3]")
      arr.push("    lease.lname = leaseArr[4]")
      arr.push("    table.insert(o, lease)")
      arr.push("  end");
      arr.push("  f:close()");
      arr.push("end");
      return arr;
  },
};
#endif

var ClientList = PageTableObj("ClientList", "Dynamic DHCP client list",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaGetClientList,
#endif
  readOnly: true,
  colgroup: ["200px","150px","150px","300px"],
  members: [
    staticTextVariable("lname", "Computer name"),
    staticTextVariable("lmac", "MAC address"),
    staticTextVariable("lip", "IP address"),
    staticTextVariable("lexpiry", "Expiry time"),
  ],
  invisible: true
});

#ifdef COMPILE_WEBUI
var customLuaStaticDhcp = {
  lockRdb : false,
  get: function() {
    return ["=getRdbArray(authenticated,'service.dhcp.static',0,99,true,{''})"]
  },
  set: function() {
    return [
      "local objs = o.objs",
      "for _, r in ipairs(objs) do",
      "  r.rawdata=r.sname..','..r.smac..','..r.sip..','..r.senable",
      "end",
      "local termObj = {} termObj.__id=#o.objs  termObj.rawdata = '' o.objs[#o.objs+1]=termObj",
      "local rc = setRdbArray(authenticated,'service.dhcp.static',{''},o)",
      "luardb.set('service.dhcp.static.trigger','1')",
      "return rc"
    ];
  },
  helpers: [
        "function isValidName(val)",
        " if val and #val >= 1 and #val <= 64 "
          + "and val:match("+'"'+"[!()*/0-9;?A-Z_a-z-]+"+'"'+") then",
        "  return true",
        " end",
        " return false",
        "end"
    ]
};
#endif

class ComputerName extends StaticTextVariable {
  setVal(obj: any, val: string) {
    var len = val.length;
    if (len >= 32)
        val = val.substring(0,10)+'...'+val.substring(len-10);
    $("#"+this.getHtmlId()).html(val);
  }
}

var StaticDhcp = PageTableObj("StaticDhcp", "Address reservation list",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaStaticDhcp,
#endif
  editLabelText: "Static DHCP",
  arrayEditAllowed: true,
  colgroup: ["200px","200px","200px","200px", "50px"],
  sendThisObjOnly: true,

  initObj:  function() {
      var obj: any = {};
      obj.sname = "";
      obj.smac = "";
      obj.sip = "";
      obj.senable = 0;
      return obj;
  },

  onEdit: function (obj) {
    setEditButtonRowVisible(true);
    setPageObjVisible(false, "DhcpCfg");
    setPageObjVisible(false, "ClientList");
  },

  offEdit: function () {
    setEditButtonRowVisible(false);
    setPageObjVisible(true, "DhcpCfg");
    setPageObjVisible(true, "ClientList");
  },

  decodeRdb: function(objs: any[]) {
    var newObjs = [];
    objs.forEach(function(obj){
      var ar = obj.rawdata.split(",");
      var newObj: any = {};
      newObj.__id = obj.__id;
      newObj.sname = ar[0];
      newObj.smac = ar[1];
      newObj.sip = ar[2];
      newObj.senable = ar[3];
      newObjs[newObjs.length] = newObj;
    });
    return newObjs;
  },

  getValidationError: function(obj: any) {
    var dhcpStartAddr = DhcpCfg.obj.startRange.split(",")[0];
    var dhcpEndAddr = DhcpCfg.obj.endRange;
    var dhcpReservAddr = obj.sip;
    /* Check if the reserved IP address is within the DHCP address range */
    if (!checkIPrange(dhcpStartAddr, dhcpEndAddr, dhcpReservAddr)) {
      return "invalidResvdIPAddr";
    }
    if (obj.smac == "") {
      return "MAC address is empty. Please enter MAC address.";
    }
    if (!isValidMacAddress(obj.smac)) {
      return "Invalid MAC address. Please enter valid MAC address.";
    }
  },

  members: [
    new ComputerName("sname", "Computer name"),
    staticTextVariable("smac", "MAC address"),
    staticTextVariable("sip", "IP address"),
    staticTextVariable("senable", "Enable"),
  ],

  editMembers: [
    editableTextVariable("sname", "Computer name")
        .setMaxLength(64)
        .setValidate(
          function (val) { if (val.length == 0) return false; return true;}, "Computer name is invalid",
          function (field) {return nameFilter(field);},
          "isValidName(v)"
        ),
    editableTextVariable("smac", "MAC address")
        .setMaxLength(17)
        .setValidate(
          function (field) {return isValidMacAddress(field);},
          "Invalid MAC address. Please specify a HEX value."
        ),
#ifdef COMPILE_WEBUI
    editableIpAddressVariable("sip", "IP address"),
#else
    editableIpAddressVariable("sip", "IP address", genHtmlIpBlocks0),
#endif
    toggleVariable("senable", "Enable"),
  ],
  invisible: true
});

function onClickSaveBtnDhcp(){
  sendSingleObject(StaticDhcp);
}

var pageData : PageDef = {
  title: "DHCP",
  menuPos: ["Networking", "LAN", "DhcpSettings"],
  pageObjects: [DhcpCfg, ClientList, StaticDhcp],
  onReady: function (){
    appendButtons({"save":"CSsave", "cancel":"cancel"});
    setButtonEvent('save', 'click', onClickSaveBtnDhcp);
    setEditButtonRowVisible(false);
  }
}
