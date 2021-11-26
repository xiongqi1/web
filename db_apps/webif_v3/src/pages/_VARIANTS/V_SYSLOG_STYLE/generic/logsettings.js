// ------------------------------------------------------------------------------------------------
// System log settings user interface
// Copyright (C) 2020 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

var syslogRdbPrefix = "service.syslog.option.";

// log capture level setting
var LogCaptureLevel = PageObj("LogCaptureLevel", "Log capture level",
{
  rdbPrefix: syslogRdbPrefix,
  members:[
    selectVariable("capturelevel", "LogCaptureLevel", function(o){
      return [
        ["8",_("log debug")],
        ["7",_("log info")],
        ["6",_("log notice")],
        ["5",_("log warning")],
        ["4",_("log Error")]];},
        "", false)
    .setRdb("capturelevel")
  ]
});

// volatile log setting
var VolatileLog = PageObj("VolatileLog", "Volatile log",
{
  rdbPrefix: syslogRdbPrefix,
  members:[
    editableBoundedInteger("buffersizekb", "Log buffer size (KB)", 100, 512, "Please specify a value between 100 and 512", {helperText: _("100-512")})
      .setRdb("buffersizekb")
      .setInputSizeCssName("med"),
  ]
});

// non-volatile log setting
var NonVolatileLog = PageObj("NonVolatileLog", "Non-volatile log",
{
#ifdef COMPILE_WEBUI
  customLua: {
    get: function(getArray) {
      return getArray.concat([
        "if o.rotatesize == '' then o.rotatesize=5000 end\n",
        "if o.rotategens == '' then o.rotategens=1 end\n",
        "if o.logtofile == '' then o.logtofile = 0 end\n"
        ]);
    }
  },
#endif
  rdbPrefix: syslogRdbPrefix,
  members:[
    toggleVariable("logtofile", "Log to non-volatile memory").setRdb("logtofile"),
#ifdef V_LARGE_LOGFILE_y
    editableBoundedInteger("filesizekb", "Log file size (KB)", 10000, 200000, "Please specify a value between 10000 and 200000", {helperText: _("10000-200000")})
#else
    editableBoundedInteger("filesizekb", "Log file size (KB)", 500, 5000, "Please specify a value between 500 and 5000", {helperText: _("500-5000")})
#endif
      .setInputSizeCssName("med"),
    hiddenVariable("rotateSize", "rotatesize"),
    hiddenVariable("rotateGens", "rotategens")
  ],
  // syslogd creates (obj.rotateGens + 1) number of message files
  // so total file size is rotatesize * (rotategens + 1)
  decodeRdb: function(obj) {
    obj.filesizekb = parseInt(obj.rotateSize) * (parseInt(obj.rotateGens) + 1);
    return obj;
  },
  // Called before the data is sent to device.
  // syslogd creates (obj.rotateGens + 1) number of message files
  // so save rotategens as filesizekb / (rotategens + 1)
  encodeRdb: function(obj) {
    obj.rotateSize = obj.filesizekb / (parseInt(obj.rotateGens) + 1);
    return obj;
  }
});

// remote syslog server setting
var RemoteSyslogServer = PageObj("RemoteSyslogServer", "Remote syslog server",
{
#ifdef COMPILE_WEBUI
  customLua: {
    set: function(setArray) {
      return setArray.concat(["setRdb('"+syslogRdbPrefix+"trigger', '1')"]);
    }
  },
#endif
  rdbPrefix: syslogRdbPrefix,
  members:[
    hiddenVariable("remoteEn", "logtoremote"),
    editableHostnameWPort("remote", "IP / Hostname [:PORT]", {required: false})
      .setRdb("remote")
      .setInputSizeCssName("med")
  ],
  // Called before the data is sent to device.
  encodeRdb: function(obj) {
    if (obj.remote == "") {
      obj.remoteEn = "0";
    } else {
      obj.remoteEn = "1";
    }
    return obj;
  }
});

var pageData : PageDef = {
  title: "System Log Settings",
  menuPos: ["System", "Log", "SystemLogSettings"],
  pageObjects: [LogCaptureLevel, VolatileLog, NonVolatileLog, RemoteSyslogServer],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    $("#objouterwrapperRemoteSyslogServer").after(genButtons({"save":"CSsave"}));
    setButtonEvent('save', 'click', sendObjects);
  }
}
