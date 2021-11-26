function onChangeOperMode() {
  if ($("#sel_operSel").val() == "Automatic") {
    setEnable("scanButton", false);
    OperatorList.setPageObjectVisible(false);
  } else {
    setEnable("scanButton", true);
    OperatorList.setPageObjectVisible(true);
    updateCurrentOperator();
  }
}

var selectedOperatorIndex = 0;
function onSelectButton(selBtName) {
  OperatorList.obj.forEach(function(obj,rowIndex) {
    var btnName = "OperatorList_" + rowIndex + "_operSel";
    var fullBtnName = btnName + "_btn";
    if (selBtName == fullBtnName) {
      setRadioButtonVal(btnName, true);
      selectedOperatorIndex = rowIndex;
    } else {
      setRadioButtonVal(btnName, false);
    }
  });
}

function updateCurrentOperator() {
  if (typeof OperatorList.obj != "undefined") {
    OperatorList.obj.forEach(function(obj,rowIndex) {
      var btnName = "OperatorList_" + rowIndex + "_operSel";
      if (obj.lstatus == "current") {
        setRadioButtonVal(btnName, true);
        selectedOperatorIndex = rowIndex;
      } else {
        setRadioButtonVal(btnName, false);
      }
    });
  }
}

function onClickScanBtnOperSet(){
  pageData.suppressAlertTxt = true;
  setEnable("saveButton", false);
  setEnable("scanButton", false);
  var modeParam = {};
  sendRdbCommand('wwan.0.plmn', 'get', modeParam, 600, csrfToken,
    function(param) {
      blockUI_alert("The scan operation was done successfully.", function () {
        setEnable("saveButton", true);
        requestSinglePageObject(OperatorList);
      });
    }
  );
}

function waitForNetworkAndScan() {
  var counter = 0;
  var numPolls = 60;     // 60 sec timeout with 5s polling interval
  var registered = "0";
  var rat, mnc, mcc;
  var paramRat;
  var obj = OperatorList.obj[selectedOperatorIndex];
  if (obj.ltype == "3") {
    paramRat = "gsm";
  }else if (obj.ltype == "7") {
    paramRat = "umts";
  }else if (obj.ltype == "9") {
    paramRat = "lte";
  }else if (obj.ltype == "12") {
    paramRat = "nr5g";
  }
  var intv = setInterval(function() {
    $.getJSON( "network_status", {},
      function(res){
        registered = res.data.registered;
        rat = res.data.rat;
        mcc = res.data.mcc;
        mnc = res.data.mnc;
      }
    ).fail(function() { registered = "0"; });
    numPolls--;
    // Wait until connect to new network or timeout
    if ((registered == "1" && rat == paramRat && mcc == obj.lmcc && mnc == obj.lmnc) ||
        numPolls <= 0) {
      clearInterval(intv);
      // Send scan command to update PLMN list
      sendRdbCommand('wwan.0.plmn', 'get', {}, 600, csrfToken,
        function(param) {
          blockUI_alert("The operation was done successfully.", function() {
            location.reload();});
        }
      );
    }
  }, 1000); // 1s
}

function onClickSaveBtnOperSet(){
  pageData.suppressAlertTxt = true;
  setEnable("saveButton", false);
  setEnable("scanButton", false);

  // For band selection command, get/set param is same but it could be
  // different for other command such as SIM command.
  // SIM command example
  //    set param {"pin":"1234","newpin":"0000","puk":"123456789"}
  //    get param {"unlock_lef":"10","verify_left":"3"}
  var mode = $("#sel_operSel").val();
  var modeParam = {"mode":"0"}; // Automatic
  if (mode == "Automatic") {
    sendRdbCommand('wwan.0.plmn', 'set', modeParam, 5, csrfToken,
      function(param) {
        blockUI_alert("The operation was done successfully.");
        requestSinglePageObject(OperatorList);
        onChangeOperMode();
        setEnable("saveButton", true);
      }
    );
  } else {
    // Manual network set needs network scan after setting.
    var obj = OperatorList.obj[selectedOperatorIndex];
    var modeVal = obj.ltype + "," + obj.lmcc + "," + obj.lmnc;
    modeParam = {"mode":modeVal};
    sendRdbCommand('wwan.0.plmn', 'set', modeParam, 5, csrfToken,
      function(param) {
        waitForNetworkAndScan();
      }
    );
  }
}

var operSel = selectVariable("operSel", "Select operator mode", ()=>[],
  "onChangeOperMode", false);

var OperatorSetting = PageObj("OperatorSetting", "Operator selection mode",
{
  members: [
    operSel,
    staticTextVariable("currOper", "Current operator registration"),
    hiddenVariable("selMode", '').setRdb("wwan.0.PLMN_selectionMode"),
    hiddenVariable("networkName", '').setRdb("wwan.0.system_network_status.network"),
    hiddenVariable("networkType", '').setRdb("wwan.0.system_network_status.service_type")
  ],
  decodeRdb: function(obj) {
    if (obj.networkName == "") {
      obj.currOper = "N/A";
    } else {
      obj.currOper = obj.networkName + " / " + obj.networkType.toUpperCase();
    }
    return obj;
  },
  populate: function () {
    $("#inp_currOper").html(this.obj.currOper.replace("WCDMA","UMTS"));
    $("#sel_operSel").empty();
    $("#sel_operSel").append(new Option ("automatic", "Automatic"));
    $("#sel_operSel").append(new Option ("manual", "Manual"));
    $("#sel_operSel").val(this.obj.selMode);
    onChangeOperMode();
  }
});

#ifdef COMPILE_WEBUI
var customLuaGetOperatorList = {
  lockRdb: false,
  get: (arr) => [...arr,`
      local plmnList = luardb.get('wwan.0.PLMN_list') or ''
      local plmnTbl = plmnList:explode('&')
      for _, plmn in ipairs(plmnTbl) do
        local netInfo = plmn:explode(',')
        local oper = {}
        oper.lname = netInfo[1]
        oper.lmcc = netInfo[2]
        oper.lmnc = netInfo[3]
        oper.lstatus = netInfo[4]
        oper.ltype = netInfo[5]
        if oper.lname ~= '' then
          table.insert(o, oper)
        end
      end
  `]
};
#endif

var OperatorList = PageTableObj("OperatorList", "Operator list",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaGetOperatorList,
#endif
  colgroup: ["20px", "200px","100px","100px","200px","200px"],
  members: [
    radioButtonVariable("operSel", "", "onSelectButton"),
    staticTextVariable("lname", "Operator name"),
    staticTextVariable("lmcc", "MCC"),
    staticTextVariable("lmnc", "MNC"),
    staticTextVariable("lstatus", "Operator status"),
    staticTextVariable("lntype", "Network type"),
    hiddenVariable("ltype", ""),
  ],
  decodeRdb: function(objs: any[]) {
    objs.forEach(function(obj){
      obj.lname.replace("Telstra Mobile", "Telstra");
      if (obj.lstatus == "1") {
        obj.lstatus = "available";
      } else if (obj.lstatus == "2") {
        obj.lstatus = "forbidden";
      } else if (obj.lstatus == "4") {
        obj.lstatus = "current";
      } else {
        obj.lstatus = "unknown";
      }
      if (obj.ltype == "3") {
        obj.lntype = "GSM (2G)";
      } else if (obj.ltype == "7") {
        obj.lntype = "UMTS (3G)";
      } else if (obj.ltype == "9") {
        obj.lntype = "LTE (4G)";
      } else if (obj.ltype == "12") {
        obj.lntype = "NR5G (5G)";
      } else {
        obj.lntype = "unknown";
      }
    });
    return objs;
  },
  postPopulate: function() {
    onChangeOperMode();
  },
  invisible: true
});

var pageData : PageDef = {
  title: "Operator setting",
  menuPos: ["Networking", "WirelessWAN", "OperatorSetting"],
  pageObjects: [OperatorSetting, OperatorList],
  onReady: function (){
    appendButtons({"scan":"scan", "save":"CSsave"});
    setButtonEvent('scan', 'click', onClickScanBtnOperSet);
    setButtonEvent('save', 'click', onClickSaveBtnOperSet);
    if (OperatorSetting.obj.selMode == "Automatic") {
        setEnable("scanButton", false);
        OperatorList.setPageObjectVisible(false);
    }
    updateCurrentOperator();
  }
}
