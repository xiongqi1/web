// ------------------------------------------------------------------------------------------------
// System log settings user interface
// Copyright (C) 2018 NetComm Wireless Limited.
// ------------------------------------------------------------------------------------------------

// heading with info string
class InfoVariable extends ShownVariable {
  genHtml() {
    return htmlDiv({class:"p-des-full-width"}, htmlTag("p", {}, _("systemLogMsg")));
  }
}

var SystemLogSetting = PageObj("SystemLogSetting", "systemLogSettings",
{
  members:[
    new InfoVariable("systemLogMsg", "")
  ]
});

var syslogRdbPrefix = "service.syslog.option.";

// log capture level setting
var LogCaptureLevel = PageObj("LogCaptureLevel", "log capture level",
{
  rdbPrefix: syslogRdbPrefix,
  members:[
    selectVariable("capturelevel", "log capture level", function(o){
      return [
        ["8",_("log debug")],
        ["7",_("log info")],
        ["6",_("log notice")],
        ["5",_("log warning")],
        ["4",_("log Error")]];})
    .setRdb("capturelevel")
  ]
});

// non-volatile log setting
var NonVolatileLog = PageObj("NonVolatileLog", "nonVolatilelogSettings",
{
  customLua: {
    // assign default value to sizekb if it has no value
    get: function(getArray) {
      return getArray.concat([
        "if o.sizekb == '' then o.sizekb=256 end\n",
        "if o.logtofile == '' then o.logtofile = 0 end\n"
      ]);
    },
    validate: function(validateArray) {
      return validateArray.concat(`v=o.sizekb
        min=tonumber(luardb.get('service.syslog.option.sizekb_min'))
        max=tonumber(luardb.get('service.syslog.option.sizekb_max'))
        if isValid.BoundedInteger(v,min,max)==false then
          return false,'oops! '..'sizekb'
        end`.split("\n")
      );
    }
  },
  rdbPrefix: syslogRdbPrefix,
  decodeRdb: function(o) {
    (this.members[1] as EditableBoundedInteger).setBounds(o.sizemin, o.sizemax);
    $("#sizekb_helperText").text(o.sizemin + "-" + o.sizemax);
    return o;
  },
  members:[
    toggleVariable("logtofile", "syslog to file").setRdb("logtofile"),
    editableBoundedInteger("sizekb", "loggingSize", 1, 1000000, "givenRange",  {helperText: "&nbsp"}).setRdb("sizekb"),
    hiddenVariable("sizemin", "sizekb_min"),
    hiddenVariable("sizemax", "sizekb_max")
  ]
});

// remote syslog server setting
var RemoteSyslogServer = PageObj("RemoteSyslogServer", "remote syslog server",
{
  customLua: {
    set: function(setArray) {
      return setArray.concat(["setRdb('"+syslogRdbPrefix+"trigger', '1')"]);
    }
  },
  rdbPrefix: syslogRdbPrefix,
  members:[ editableHostnameWPort("remote", "ip hostname", {required: false}).setRdb("remote") ]
});

var pageData : PageDef = {
#if defined V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Log Settings",
  menuPos: ["System", "LOGSETTINGS"],
  pageObjects: [SystemLogSetting, LogCaptureLevel, NonVolatileLog, RemoteSyslogServer],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    $("#objouterwrapperRemoteSyslogServer").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}
