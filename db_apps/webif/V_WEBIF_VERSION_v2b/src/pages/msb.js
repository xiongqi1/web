var MSBCfg = PageObj("MSBCfg", "gps msb",
{
  members: [
    objVisibilityVariable("MSBCfgEnable", "gps msb enable").setRdb("sensors.gps.0.gpsone.enable"),
    selectVariable("max_retry", "gps msb max retry",
        (o) => [["3", "3"], ["5" , "5"], ["10" ,"10"]])
      .setRdb("sensors.gps.0.gpsone.auto_update.max_retry_count"),
    selectVariable("retry_delay", "gps msb retry delay",
        (o) => [[3*60, "3"], [5*60, "5"], [10*60, "10"], [15*60, "15"], [30*60, "30"]])
      .setRdb("sensors.gps.0.gpsone.auto_update.retry_delay"),
    selectVariable("update_period", "gps msb update period",
        (o) => [[0, "manual"], [1*24*60, "1"], [2*24*60, "2"], [3*24*60, "3"], [4*24*60, "4"], [5*24*60, "5"], [6*24*60, "6"], [7*24*60, "7"]])
      .setRdb("sensors.gps.0.gpsone.auto_update.update_period"),
#ifdef V_MODULE_EC21
    hiddenVariable("cmd","sensors.gps.0.cmd.command"),
#endif
    hiddenVariable("gpsEnable", 'sensors.gps.0.enable')
  ],

  // Called before the data is sent to modem.
  encodeRdb: function(obj) {
    // Enable GPS functionality if required
    if (toBool(obj.MSBCfgEnable) && !toBool(obj.gpsEnable)) {
      obj.gpsEnable = "1";
    }
#ifdef V_MODULE_EC21
    /* In order to trigger simple_at_manager to response, set the command appropriately */
    if (!isDefined(obj.cmd)) { // Already defined if doing GNSS update
      obj.cmd = toBool(obj.MSBCfgEnable) ? "gpsone_enable": "gpsone_disable";
    }
#endif
    return obj;
  }
});

var epoch1970=new Date(Date.UTC(1970,0,1,0,0,0,0)).getTime();

function onUpdateMSB() {
  MSBStatus.obj.update_now = "1";
  MSBStatus.obj.updated = "";
#ifdef V_MODULE_EC21
  MSBCfg.obj.cmd = "gpsone_update";
#endif
  sendObjects();
  $("button").prop("disabled", true);
  var numPolls = 15; // 30 secs
  var intv = setInterval(function() {

    function stopWithError(err? : string) {
      clearInterval(intv);
      $("button").prop("disabled", false);
      if (isDefined(err))
        validate_alert( "", _(err));
    }

    if (isDefined(MSBStatus.obj.updated) && MSBStatus.obj.updated != "" ) {
      if (toBool(MSBStatus.obj.updated)) {
        return stopWithError();
      }
      return stopWithError("msg update unknown failure");
    }
    numPolls--;
    if (numPolls <= 0) {
      return stopWithError("msb update timeout");
    }
  }, 2000); // 2 secs
}

function myStaticTextVariable(name, label) {
  var pe = staticTextVariable(name, label);
  pe.class = "location-settings";
  pe.readOnly = true;
  return pe;
}

var MSBStatus = PageObj("MSBStatus","",
{
  pollPeriod: 2000,
  members: [
    myStaticTextVariable("gnss_time","gps msb gnss time").setRdb("sensors.gps.0.gpsone.xtra.info.gnss_time"),
    myStaticTextVariable("valid_time","gps msb valid time").setRdb("sensors.gps.0.gpsone.xtra.info.valid_time"),
    myStaticTextVariable("last_updated_time", "gps msb last updated time").setRdb("sensors.gps.0.gpsone.xtra.updated_time"),
    buttonAction("upDate", "update xtra", "onUpdateMSB"),
    hiddenVariable("update_now", 'sensors.gps.0.gpsone.update_now'),
    hiddenVariable("updated", 'sensors.gps.0.gpsone.updated')
  ],

  decodeRdb: function (obj) {

    function fmtDate(time) {
      var date = new Date(epoch1970 + time*1000);
      return date.toDateString() + " " + date.toLocaleTimeString();
    }

    /* set updated time */
    obj.last_updated_time = fmtDate(obj.last_updated_time);
    obj.valid_time = fmtDate(obj.valid_time);
    obj.gnss_time = fmtDate(obj.gnss_time);
    return obj;
  }
});

function mySendObjects() {
#ifdef V_MODULE_EC21
  delete MSBCfg.obj.cmd;  // Clear it so that it gets set in encodeRdb()
#endif
  sendObjects();
}

var pageData : PageDef = {
#if defined V_SERVICES_UI_none || defined V_GPS_none
  onDevice : false,
#endif
  title: "GPS",
  menuPos: ["Services", "MSB"],
  pageObjects: [MSBCfg, MSBStatus],
  onReady: function () {
    $("#objouterwrapperMSBCfg").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', mySendObjects);
  }
}
