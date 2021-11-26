function showTzInfo_(theZone){
    var dst = theZone.DST.split(","); //EDT,M10.1.0,M4.1.0/3
	if(dst.length < 3){
        blockUI_alert(_("Msg105")); //Daylight Saving Time details are not available. ( Error "+dst.length+" )
        return;
    }

    var months = [_("January"), _("February"), _("March"), _("April"), _("May"), _("June"), _("July"), _("August"), _("September"), _("October"), _("November"), _("December")];
    var weekdays = [_("Sunday"), _("Monday"), _("Tuesday"), _("Wednesday"), _("Thursday"), _("Friday"), _("Saturday")];
    var weeknumbers = [_("first"), _("second"), _("third"), _("fourth"), _("last")];

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
        blockUI_alert_l(lang_sentence(_("ntpInfo"), params));
        return;
    }
    blockUI_alert(errMsg);
}

var zoneinfo;
// time zone data format in /usr/zoneinfo.ref file
// Pacific/Midway;SST11;;(GMT-11:00) Pacific/Midway;;;;;
var customLuaGetTzInfo = {
  lockRdb : false,
  get: function() {
    var luaScript = `
      require("stringutil")
      zoneinfo = {}
      o = {}
      local index_file = "/usr/zoneinfo.ref"
      for line in io.lines(index_file) do
        local zone = line:explode(';')
        local info = {}
        info.FL = zone[1]
        info.TZ = zone[2]
        info.DST = zone[3]
        info.NAME = zone[4]
        table.insert(zoneinfo, info)
      end
      o.zoneInfo = zoneinfo
      o.tzSel = luardb.get("system.config.tz")
`;
    return luaScript.split("\n");
  }
};


var tzSel = selectVariable("tzSel", "GUI timeZone",
function(obj){
  var options = []
  return options;
}, "onChangeTz").setRdb("system.config.tz");

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
    blockUI_alert(_("Msg105"));
}

var tzChanged = false;
function onChangeTz(){
    var tz = findTz();
    if (isDefined(tz)){
            document.getElementById("div_tzInfo").style["display"]= tz.DST == ""? "none": "";
    }
    tzChanged = true;
}

var TimeDate = PageObj("TimeDate", "timezoneSettings",
{
  pollPeriod: 1000,
  customLua: {
    lockRdb : false,
    get: function(getArray){
      return getArray.concat([
                  "local h = io.popen('date')",
                  "local res = h:read('*a')",
                  "h:close()",
                  "o.now=res"
                ]);
    }
  },
  members: [
    staticTextVariable( "now", "man ntp current time"),
  ]
});

var intv;
var TzInfo = PageObj("TzInfo", "",
{
  customLua: customLuaGetTzInfo,
  members: [
    tzSel,
    buttonAction("tzInfo", "daylightSavingTimeSchedule", "showTzInfo"),
    hiddenVariable("zoneInfo", '')
  ],
  populate: function () {
    zoneinfo = this.obj.zoneInfo;
    if (zoneinfo.length > 0){
      for (var i = 0; i < zoneinfo.length; i++){
        $("#sel_tzSel").append(new Option (zoneinfo[i].NAME, zoneinfo[i].FL));
      }
      $("#sel_tzSel").val(this.obj.tzSel);
    }
  }
});

var NtpCfg = PageObj("NtpCfg", "ntpSettings",
{
#ifdef V_NON_SECURE_WARNING_y
  setEnabled: function (en){ if (en) blockUI_alert(_("noEncryptionWarning")); },
#endif
  members: [
    objVisibilityVariable("ntpEnable", "NTP").setRdb("service.ntp.enable"),
    editableHostname("ntpSrvr", "NTP Service").setRdb("service.ntp.server_address").setEncode(true),
    toggleVariable("onconnect", "ntp on wwan connection").setRdb("service.ntp.onconnect"),
    toggleVariable("dailysync", "ntp daily sync").setRdb("service.ntp.dailysync"),
  ]
});

var pageData : PageDef = {
#if defined V_SERVICES_UI_none
  onDevice : false,
#endif
  title: "NTP",
  menuPos: ["Services", "NTP"],
  pageObjects: [TimeDate, TzInfo, NtpCfg],
  alertSuccessTxt: "ntpSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', function (){
        sendObjects();
        if (tzChanged){
            tzChanged = false;
            blockUI_alert_l(_("Msg130"),function(){});
        }
    });
    onChangeTz();
  }
}

disablePageIfPppoeEnabled();
