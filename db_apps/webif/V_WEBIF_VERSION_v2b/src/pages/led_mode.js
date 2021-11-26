function showTimer(show) {
  if (show) {
    $("#div_ledPowerOffTimer").show();
  }
  else {
    $("#div_ledPowerOffTimer").hide();
  }
}

function onChangeMode(_this) {
  showTimer(toBool(_this.value)); // This->value is "0" or "1"
}

var LedModeCfg = PageObj("LedModeCfg", "led operation mode",
{
  members: [
    selectVariable("mode", "mode",
      function(obj){return [["0","alwayson"],["1","turnOffAfterTimeout"]];}, "onChangeMode"),
    editableBoundedInteger("ledPowerOffTimer", "led power off timer",
      1, 65535, "led warningMsg1", {helperText: "led power off range"}),
    hiddenVariable("timer", "system.led_off_timer")
  ],

  // Called after the data is fetched from modem.
  // Only the hidden timer is retrieved so we have to generate the two
  // displayed derivatives
  decodeRdb: function (obj) {
    var timer = Number(obj.timer);
    if(isNaN(timer) || timer === 0) {
        obj.mode = "0";
        obj.ledPowerOffTimer = 10;
        showTimer(false);
    }
    else {
      obj.mode = "1";
      obj.ledPowerOffTimer = timer;
    }
    return obj;
  },

  // Called before the data is sent to modem.
  // Only the hidden timer is required so set it from the displayed elements
  encodeRdb: function(obj) {
    obj.timer = toBool(obj.mode) ? obj.ledPowerOffTimer : 0;
    return obj;
  }
});

var pageData  : PageDef = {
#if defined V_ADMINISTRATION_UI_none
  onDevice: false,
#endif
  title: "LED Mode",
  menuPos: ["System", "LED"],
  pageObjects: [LedModeCfg],
  alertSuccessTxt: "ledModeSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}
