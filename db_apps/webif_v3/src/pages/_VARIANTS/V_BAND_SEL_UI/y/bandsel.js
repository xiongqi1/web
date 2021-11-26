const MAX_COL_NO = 5;
#ifdef COMPILE_WEBUI
var customLuaGetBandList = {
  lockRdb : true,
  get: (arr) => [...arr, `
    require('stringutil')
    o = {}
    -- Extract each band list from 'wwan.0.fixed_module_band_list or wwan.0.module_band_list
    -- ex) wwan.0.module_band_list is;
    --          0050,WCDMA 2100&0051,WCDMA PCS 1900&0052,WCDMA DCS 1800&00...
    --          ... 2800,WCDMA/NR5G all&3000,LTE/NR5G all&3800,WCDMA/LTE/NR5G all
    -- Fixed band list has higher priority than modem band list.
    local rdbBands = luardb.get('wwan.0.fixed_module_band_list') or luardb.get('wwan.0.module_band_list') or ''
    local rawBandList = rdbBands:explode('&')
    local cdmaBandList, gsmBandList, wcdmaBandList, lteBandList = {}, {}, {}, {}
    local nr5gBandList, nr5gNsaBandList, nr5gSaBandList = {}, {}, {}
    for _, v in ipairs(rawBandList) do
      if v:find('all') == nil then
        if v:find('WCDMA') then
          table.insert(wcdmaBandList, v:split(',')[2])
        elseif v:find('CDMA') then
          table.insert(cdmaBandList, v:split(',')[2])
        elseif v:find('GSM') then
          table.insert(gsmBandList, v:split(',')[2])
        elseif v:find('LTE Band') then
          table.insert(lteBandList, v:split(',')[2])
        elseif v:find('NR5G BAND') then
          table.insert(nr5gBandList, v:split(',')[2])
        elseif v:find('NR5G NSA BAND') then
          table.insert(nr5gNsaBandList, v:split(',')[2])
        elseif v:find('NR5G SA BAND') then
          table.insert(nr5gSaBandList, v:split(',')[2])
        end
      end
    end
    o.cdmaBandList = cdmaBandList
    o.gsmBandList = gsmBandList
    o.wcdmaBandList = wcdmaBandList
    o.lteBandList = lteBandList
    o.nr5gBandList = nr5gBandList
    o.nr5gNsaBandList = nr5gNsaBandList
    o.nr5gSaBandList = nr5gSaBandList
    o.currSelBand = luardb.get('wwan.0.currentband.current_selband')
  `]
};
#endif

var BandSel = PageObj("BandSel", "",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaGetBandList,
#endif
  members: [
    hiddenVariable("currSelBand", ''),
    hiddenVariable("cdmaBandList", ''),
    hiddenVariable("gsmBandList", ''),
    hiddenVariable("wcdmaBandList", ''),
    hiddenVariable("lteBandList", ''),
    hiddenVariable("nr5gBandList", ''),
    hiddenVariable("nr5gNsaBandList", ''),
    hiddenVariable("nr5gSaBandList", '')
  ],
  populate: function () {
    var bandList = this.obj.bandList;
  }
});

var bandSelCommonObjs = {
  colgroup: ["200px","200px","200px","200px","200px"],
  extraAttr: {
    tableAttr: {
      class:"above-5-column no-table-padding hidden-border"
    },
    boxAttr:"body-box reboot",
  },
  members: [
    checkboxVariable("band1", "", ""),
    checkboxVariable("band2", "", ""),
    checkboxVariable("band3", "", ""),
    checkboxVariable("band4", "", ""),
    checkboxVariable("band5", "", ""),
  ],
};

function parseBandList(list: string[]) {
  var newObjs = [];
  var i, j;
  for (i=0;;) {
    var newObj: any = {};
    for (j=0;j<MAX_COL_NO;j++) {
      newObj["band"+j] = (typeof list[i] !== "undefined")? list[i++]:"";
    }
    if (newObj.band0 == "") {
      break;
    }
    newObjs.push(newObj);
  };
  return newObjs;
}

var CdmaBandSel = PageTableObj("CdmaBandSel", "CDMA band",
{
  colgroup: bandSelCommonObjs.colgroup,
  extraAttr: bandSelCommonObjs.extraAttr,
  members: bandSelCommonObjs.members,
  decodeRdb: function(objs: any[]) {
    return parseBandList(BandSel.obj.cdmaBandList);
  },
});

var GsmBandSel = PageTableObj("GsmBandSel", "GSM band",
{
  colgroup: bandSelCommonObjs.colgroup,
  extraAttr: bandSelCommonObjs.extraAttr,
  members: bandSelCommonObjs.members,
  decodeRdb: function(objs: any[]) {
    return parseBandList(BandSel.obj.gsmBandList);
  },
});

var WcdmaBandSel = PageTableObj("WcdmaBandSel", "WCDMA band",
{
  colgroup: bandSelCommonObjs.colgroup,
  extraAttr: bandSelCommonObjs.extraAttr,
  members: bandSelCommonObjs.members,
  decodeRdb: function(objs: any[]) {
    return parseBandList(BandSel.obj.wcdmaBandList);
  },
});

var LteBandSel = PageTableObj("LteBandSel", "LTE band",
{
  colgroup: bandSelCommonObjs.colgroup,
  extraAttr: bandSelCommonObjs.extraAttr,
  members: bandSelCommonObjs.members,
  decodeRdb: function(objs: any[]) {
    return parseBandList(BandSel.obj.lteBandList);
  },
});

var Nr5gBandSel = PageTableObj("Nr5gBandSel", "NR5G band",
{
  colgroup: bandSelCommonObjs.colgroup,
  extraAttr: bandSelCommonObjs.extraAttr,
  members: bandSelCommonObjs.members,
  decodeRdb: function(objs: any[]) {
    return parseBandList(BandSel.obj.nr5gBandList);
  },
});

var Nr5gNsaBandSel = PageTableObj("Nr5gNsaBandSel", "NR5G NSA band",
{
  colgroup: bandSelCommonObjs.colgroup,
  extraAttr: bandSelCommonObjs.extraAttr,
  members: bandSelCommonObjs.members,
  decodeRdb: function(objs: any[]) {
    return parseBandList(BandSel.obj.nr5gNsaBandList);
  },
});

var Nr5gSaBandSel = PageTableObj("Nr5gSaBandSel", "NR5G SA band",
{
  colgroup: bandSelCommonObjs.colgroup,
  extraAttr: bandSelCommonObjs.extraAttr,
  members: bandSelCommonObjs.members,
  decodeRdb: function(objs: any[]) {
    return parseBandList(BandSel.obj.nr5gSaBandList);
  },
});

function onClickBandSelButtons(bandObj, sel) {
  console.log("bandObj = " + bandObj.objName);
  console.log("select = " + sel);
  bandObj.obj.forEach(function(ob,rowIndex) {
    var rowId = rowIndex;
    bandObj.members.forEach(function(mem, idx){
      var hid = bandObj.objName + "_" + rowIndex + "_" + mem.memberName + '_btn';
      var band = ob["band"+idx];
      if (band != "") {
        if (sel == "selAll") {
          setCheckboxVal(hid, true);
        } else if (sel == "unselAll") {
          setCheckboxVal(hid, false);
        } else if (sel == "resetAll") {
          setCheckboxVal(hid, ((BandSel.obj.currSelBand+";").search(band+";")>=0));
        }
      }
    });
  });
}

function loadBandList() {
  [CdmaBandSel,GsmBandSel,WcdmaBandSel,LteBandSel,Nr5gBandSel,Nr5gNsaBandSel,Nr5gSaBandSel].forEach(function(bandObj){
    if (bandObj.obj.length == 0) {
      setPageObjVisible(false, bandObj.objName);
    }
    bandObj.obj.forEach(function(ob,rowIndex) {
      var rowId = rowIndex;
      bandObj.members.forEach(function(mem, idx){
        var hid = bandObj.objName + "_" + rowIndex + "_" + mem.memberName + '_btn';
        var band = ob["band"+idx];
        if (band != "") {
          setLabelText(hid, band);
          setCheckboxVal(hid, ((BandSel.obj.currSelBand+";").search(band+";")>=0));
        } else {
          setDivVisible(false, hid);
        }
      });
    });
    if (bandObj.obj.length > 0) {
      var selBtn = "selAll"+bandObj.objName;
      var unselBtn = "unselAll"+bandObj.objName;
      var resetBtn = "resetAll"+bandObj.objName;
      $("#objouterwrapper"+bandObj.objName).after(
        genSelUnselAllReset(selBtn, unselBtn, resetBtn));
      setButtonEvent(selBtn, 'click', function() {
        onClickBandSelButtons(bandObj, "selAll");});
      setButtonEvent(unselBtn, 'click', function() {
        onClickBandSelButtons(bandObj, "unselAll");});
      setButtonEvent(resetBtn, 'click', function() {
        onClickBandSelButtons(bandObj, "resetAll");});
    }
  });
}

function getSelectedBand() {
  var bandLists = "";
  [CdmaBandSel,GsmBandSel,WcdmaBandSel,LteBandSel,Nr5gBandSel,Nr5gNsaBandSel,Nr5gSaBandSel].forEach(function(bandObj){
    if (bandObj.obj.length == 0) {
      return;
    }
    bandObj.obj.forEach(function(ob,rowIndex) {
      var id = ob._id;
      var rowId = rowIndex;
      bandObj.members.forEach(function(mem, idx){
        var hid = bandObj.objName + "_" + rowIndex + "_" + mem.memberName + '_btn';
        var band = ob["band"+idx];
        if (band != "" && getCheckboxVal(hid)) {
          bandLists += band + ";";
        }
      });
    });
  });
  return bandLists;
}

function onClickSaveBtnBandSel(){
  pageData.suppressAlertTxt = true;
  // For band selection command, get/set param is same but it could be
  // different for other command such as SIM command.
  // SIM command example
  //    set param {"pin":"1234","newpin":"0000","puk":"123456789"}
  //    get param {"unlock_lef":"10","verify_left":"3"}
  var bandParam = {"band":getSelectedBand()};
  sendRdbCommand('wwan.0.currentband', 'set', bandParam, 5, csrfToken,
    function(param) {
      sendRdbCommand('wwan.0.currentband', 'get', {}, 5, csrfToken,
        function(param) {
          blockUI_alert("The operation was done successfully.");
          requestSinglePageObject(BandSel);
        }
      );
    }
  );
}

var pageData : PageDef = {
  title: "Band Selection",
  menuPos: ["Networking", "WirelessWAN", "BANDSEL"],
  pageObjects: [BandSel, CdmaBandSel, GsmBandSel, WcdmaBandSel, LteBandSel, Nr5gBandSel, Nr5gNsaBandSel, Nr5gSaBandSel],
  onReady: () => {
    appendButtons({"save":"CSsave"});
    setButtonEvent("save", "click", onClickSaveBtnBandSel);
    loadBandList();
  }
}
