var saveButton = buttonAction("save", "Save", "onClickSaveBtnRatSel", "", {buttonStyle: "submitButton"});

#ifdef COMPILE_WEBUI
var customLuaGetRatList = {
  get: (arr) => [...arr, `
    require('stringutil')
    o = {}
    -- Get available RAT list
    -- ex) wwan.0.module_rat_list
    --          GSM;WCDMA;LTE;NR5G;
    -- Fixed rat list has higher priority than modem rat list.
    local rdbRats = luardb.get('wwan.0.fixed_module_rat_list') or luardb.get('wwan.0.module_rat_list') or ''
    local rawRatsList = rdbRats:gsub(',',';'):explode(';')
    local ratList = {}
    for _, v in ipairs(rawRatsList) do
      if v ~= '' then
        table.insert(ratList, v)
      end
    end
    o.ratList = ratList
    -- Get current RAT list
    -- ex) wwan.0.currentband.current_selrat
    --          WCDMA;LTE;NR5G;
    rdbRats = luardb.get('wwan.0.currentband.current_selrat') or ''
    rawRatsList = rdbRats:explode(';')
    local currRatList = {}
    for _, v in ipairs(rawRatsList) do
      if v ~= '' then
        table.insert(currRatList, v)
      end
    end
    o.currRatList = currRatList
  `]
};
#endif

var ratSel = selectVariable("ratSel", "RAT list", ()=>[],
  "", false,
  { multiple: true,
    overwriteStyle: "autoHeight wideWidth multiple",
  });

var RatSel = PageObj("RatSel", "Radio technology selection",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaGetRatList,
#endif
  members: [
    ratSel,
    saveButton,
    hiddenVariable("ratList", ''),
    hiddenVariable("currRatList", '')
  ],
  populate: function () {
    var ratList = this.obj.ratList;
    if (ratList.length > 0){
      $("#sel_ratSel").empty();
      for (var i = 0; i < ratList.length; i++){
        $("#sel_ratSel").append(new Option (ratList[i], ratList[i]));
      }
      $("#sel_ratSel").val(this.obj.currRatList);
    }
  }
});

function onClickSaveBtnRatSel(){
  pageData.suppressAlertTxt = true;
  saveButton.setEnable(false);
  // Using band selection command format but different set command, 'set_rat'
  // for RAT selection.
  var selRat = $("#sel_ratSel").val().toString().replace(/,/g, ";") + ";";
  var ratParam = {"rat":selRat};
  var bandParam = {"band":""};
  sendRdbCommand('wwan.0.currentband', 'set_rat', ratParam, 5, csrfToken,
    function(param) {
      sendRdbCommand('wwan.0.currentband', 'get', bandParam, 5, csrfToken,
        function(param) {
          blockUI_alert("The operation was done successfully.");
          requestSinglePageObject(RatSel);
          saveButton.setEnable(true);
        }
      );
    }
  );
}

var pageData : PageDef = {
  title: "Radio Technology Selection",
  menuPos: ["Networking", "WirelessWAN", "RATSEL"],
  pageObjects: [RatSel],
}
