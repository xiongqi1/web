function showTzInfo_(theZone){
    var dst = theZone.DST.split(","); //EDT,M10.1.0,M4.1.0/3
	if(dst.length < 3){
        blockUI_alert("Sorry, daylight saving time details aren't available at the moment. Please try again later.");
        return;
    }

    var months = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
    var weekdays = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
    var weeknumbers = ["first", "second", "third", "fourth", "last"];

    function isWithin(val,lower,upper){return val >= lower && val <= upper;}

    var params = [];

    // This gets a time specifier in the form M10.1.0
    function getParmArrayOrError(time)
    {
        var el = time.split(".");
        if(el.length != 3){
            return "DST format error " + el.length;
        }
        var month = parseInt(el[0].substring(1));
        var week = parseInt(el[1]);
        var day = parseInt(el[2]);
        var idx = el[2].indexOf("/");
        var hour = (idx == -1 ? "2": parseInt(el[2].substring(idx+1)).toString()) + ":00 AM";
        if(isWithin(month, 1, 12) && isWithin(week, 1, 5) && isWithin(day, 0, 6)){
            params = params.concat([weeknumbers[week-1], weekdays[day], months[month-1], hour]);
            return;
        }
        return "DST format error-2";
    }

    var errMsg = getParmArrayOrError(dst[1]);
    if (!isDefined(errMsg)){
        errMsg = getParmArrayOrError(dst[2]);
    }
    if (!isDefined(errMsg)){
        blockUI_alert_l(lang_sentence("Daylight Saving Time begins on the %%0 %%1 in %%2 at %%3, and ends on the %%4 %%5 in %%6 at %%7", params));
        return;
    }
    blockUI_alert(errMsg);
}

var zoneinfo;
#ifdef COMPILE_WEBUI
// time zone data format in /usr/zoneinfo.ref file
// Pacific/Midway;SST11;;(GMT-11:00) Pacific/Midway;;;;;
var customLuaGetTzInfo = {
  lockRdb : false,
  get: function(arr) {
    arr.push("zoneinfo = {}")
    arr.push("o = {}")
    arr.push("local index_file = '/usr/zoneinfo.ref'")
    arr.push("for line in io.lines(index_file) do")
    arr.push("  local zone = line:explode(';')")
    arr.push("  local info = {}")
    arr.push("  info.FL = zone[1]")
    arr.push("  info.TZ = zone[2]")
    arr.push("  info.DST = zone[3]")
    arr.push("  info.NAME = zone[4]")
    arr.push("  table.insert(zoneinfo, info)")
    arr.push("end")
    arr.push("o.zoneInfo = zoneinfo")
    arr.push("o.tzSel = luardb.get('system.config.tz')")
    return arr
  }
};
#endif

var tzSel = selectVariable("tzSel", "Timezone", function(obj){
  var options = []; return options;},
  "onChangeTz", false, {overwriteStyle: "xWideWidth"})
  .setRdb("system.config.tz");

function findTz(){
    var sel = tzSel.getVal();
    if (isDefined(zoneinfo)){
        for ( var i = 0; i < zoneinfo.length; i++){
            if (zoneinfo[i].FL === sel){
                return zoneinfo[i];
            }
        }
    }
}

function showTzInfo(){
    var tz = findTz();
    if (isDefined(tz)){
        return showTzInfo_(tz);
    }
    blockUI_alert("Sorry, daylight saving time details aren't available at the moment. Please try again later.");
}

var tzChanged = false;
function onChangeTz(){
    var tz = findTz();
    if (isDefined(tz)){
            document.getElementById("div_tzInfo").style["display"]= tz.DST == ""? "none": "";
    }
    tzChanged = true;
}

var TimeDate = PageObj("TimeDate", "Timezone settings",
{
  pollPeriod: 1000,
#ifdef COMPILE_WEBUI
  customLua: {
    lockRdb : false,
    get: function(arr) {
      arr.push("o.now = executeCommand('date')");
      return arr;
    },
  },
#endif
  members: [
    staticTextVariable( "now", "Current time"),
  ]
});

var intv;
var TzInfo = PageObj("TzInfo", "",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaGetTzInfo,
#endif
  members: [
    tzSel,
    buttonAction("tzInfo", "Daylight savings time schedule", "showTzInfo"),
    hiddenVariable("zoneInfo", '')
  ],
  populate: function () {
    zoneinfo = this.obj.zoneInfo;
    if (zoneinfo.length > 0){
      $("#sel_tzSel").empty();
      for (var i = 0; i < zoneinfo.length; i++){
        $("#sel_tzSel").append(new Option (zoneinfo[i].NAME, zoneinfo[i].FL));
      }
      $("#sel_tzSel").val(this.obj.tzSel);
    }
  }
});

var NtpCfg = PageObj("NtpCfg", "NTP settings",
{
#ifdef V_NON_SECURE_WARNING_y
  setEnabled: function (en){ if (en) blockUI_alert("WARNING: This service does not provide encryption."); },
#endif
  members: [
    objVisibilityVariable("ntpEnable", "NTP").setRdb("service.ntp.enable"),
#if V_NTP_SERVER_ADDR_NUMBER > 0
    editableHostname("ntpSrvr", "NTP service").setRdb("service.ntp.server_address").setEncode(true),
#endif
#if V_NTP_SERVER_ADDR_NUMBER > 1
    editableHostname("ntpSrvr2", "NTP service 2").setRdb("service.ntp.server_address1").setEncode(true),
#endif
#if V_NTP_SERVER_ADDR_NUMBER > 2
    editableHostname("ntpSrvr3", "NTP service 3").setRdb("service.ntp.server_address2").setEncode(true),
#endif
#if V_NTP_SERVER_ADDR_NUMBER > 3
    editableHostname("ntpSrvr4", "NTP service 4").setRdb("service.ntp.server_address3").setEncode(true),
#endif
#if V_NTP_SERVER_ADDR_NUMBER > 4
    editableHostname("ntpSrvr5", "NTP service 5").setRdb("service.ntp.server_address4").setEncode(true),
#endif
    toggleVariable("onconnect", "Synchronisation on WWAN connection").setRdb("service.ntp.onconnect"),
  ],
  // Called before the data is sent to modem.
  encodeRdb: function(obj) {
    if (!toBool(obj.ntpEnable)) {
      obj.ntpEnable = "0";
    }
    if (!toBool(obj.onconnect)) {
      obj.onconnect = "0";
    }
    return obj;
  }
});

function onClickSaveBtn(){
  // Get the object to validate
  var obj = NtpCfg.packageObj();
  if (obj.ntpEnable && obj.ntpSrvr == "") {
    blockUI_alert_l("NTP server address should be set",function(){});
    return;
  }
  delete TzInfo.obj.zoneInfo;
  sendSingleObject(TzInfo);
  sendSingleObject(NtpCfg);
  if (tzChanged){
      tzChanged = false;
      blockUI_alert_l("The new timezone settings have been saved.",function(){});
  }
}

var pageData : PageDef = {
  title: "NTP",
  menuPos: ["Services", "NTP"],
  pageObjects: [TimeDate, TzInfo, NtpCfg],
  alertSuccessTxt: "ntpSubmitSuccess",
  onReady: function (){
    appendButtons({"save":"CSsave"});
    setButtonEvent('save', 'click', onClickSaveBtn);
    onChangeTz();
  }
}

disablePageIfPppoeEnabled();
