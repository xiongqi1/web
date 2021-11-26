//--------------------------------------------------------------------------------------
// LTE cell lock
//--------------------------------------------------------------------------------------

// Send Cell Lock comand when clicking save button on edit page or
// delete button on table list.
// @params idx A object index number to delete. It is -1 for save button
//             to save all the entries.
function sendCellLockCommand(idx){
  pageData.suppressAlertTxt = true;
  var lockList = "";
  CellLockList.obj.forEach(function(ob,rowIndex) {
    if (rowIndex != idx) {
      if (rowIndex != 0) {
        lockList += ";"
      }
      lockList += "pci:" + ob.pci + ",earfcn:" + ob.earfcn;
    }
  });

  var lockParam = {"lock_list":lockList, "rat":"lte"};
  sendRdbCommand('wwan.0.cell_lock', 'set', lockParam, 5, csrfToken,
    function(param) {
      sendRdbCommand('wwan.0.cell_lock', 'get', lockParam, 5, csrfToken,
        function(param) {
          blockUI_alert("The operation was done successfully.");
          requestSinglePageObject(CellLockList);
        }
      );
    }
  );
}

#ifdef COMPILE_WEBUI
var customLuaCellLock = {
  lockRdb : false,
  // example value:
  // pci:1,earfcn:900,eci:27447297;pci:2,earfcn:918;pci:3,earfcn:906
  get: (arr) => [...arr, `
    require('stringutil')
    local lockList = luardb.get("wwan.0.modem_pci_lock_list") or ""
    local lockTbl = lockList:explode(";")
    local id = 0
    for _, v in ipairs(lockTbl) do
      local obj = {}
      local p, f = v:match("pci:(%d+),earfcn:(%d+)")
      if p then
        obj.pci, obj.earfcn = p, f
        obj.__id = id
        table.insert(o, obj)
        id = id + 1
      end
    end
  `],
};
#endif

var CellLockList = PageTableObj("CellLockList", "Lte cell lock list",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaCellLock,
#endif
  sendThisObjOnly: true,
  editLabelText: "Lte cell lock setting",
  arrayEditAllowed: true,
  extraAttr: {
    thAttr: [
      {class:"customTh field5"},
      {class:"customTh field5"},
    ],
  },

  initObj:  function() {
      var obj: any = {};
      obj.pci = "";
      obj.earfcn = "";
      return obj;
  },

  onEdit: function (obj) {
    setEditButtonRowVisible(true);
    setPageObjVisible(false, "Nr5gCellLockList");
  },

  offEdit: function () {
    setEditButtonRowVisible(false);
    setPageObjVisible(true, "Nr5gCellLockList");
  },

  delObj: function (id) {
    sendCellLockCommand(id);
    return true;
  },

  saveObj: function (obj) {
    sendCellLockCommand(-1);
  },

  members: [
    staticTextVariable("pci", "PCI"),
    staticTextVariable("earfcn", "EARFCN"),
  ],

  editMembers: [
    editableInteger("pci", "PCI"),
    editableInteger("earfcn", "EARFCN"),
  ]
});

//--------------------------------------------------------------------------------------
// NR5G cell lock
//--------------------------------------------------------------------------------------

// Send Cell Lock comand when clicking save button on edit page or
// delete button on table list.
// @params idx A object index number to delete. It is -1 for save button
//             to save all the entries.
function sendNr5gCellLockCommand(idx){
  pageData.suppressAlertTxt = true;
  var lockList = "";
  Nr5gCellLockList.obj.forEach(function(ob,rowIndex) {
    if (rowIndex != idx) {
      if (rowIndex != 0) {
        lockList += ";"
      }
      lockList += "pci:" + ob.gnbpci + ",arfcn:" + ob.arfcn;
      lockList += ",scs:" + ob.scs + ",band:NR5G SA BAND " + ob.band;
    }
  });

  var lockParam = {"lock_list":lockList, "rat":"5g"};
  sendRdbCommand('wwan.0.cell_lock', 'set', lockParam, 5, csrfToken,
    function(param) {
      sendRdbCommand('wwan.0.cell_lock', 'get', lockParam, 5, csrfToken,
        function(param) {
          blockUI_alert("The operation was done successfully.");
          requestSinglePageObject(Nr5gCellLockList);
        }
      );
    }
  );
}

#ifdef COMPILE_WEBUI
var customLuaNr5gCellLock = {
  lockRdb : false,
  // example value:
  // pci:510,arfcn:630912,scs:30,band:NR5G SA BAND 77
  get: (arr) => [...arr, `
    require('stringutil')
    local lockList = luardb.get("wwan.0.modem_pci_lock_list_5g") or ""
    local lockTbl = lockList:explode(";")
    local id = 0
    for _, v in ipairs(lockTbl) do
      local obj = {}
      local p, f, s, b = v:match("pci:(%d+),arfcn:(%d+),scs:(%d+),band:NR5G SA BAND (%d+)")
      if p then
        obj.gnbpci, obj.arfcn, obj.scs, obj.band = p, f, s, b
        obj.__id = id
        table.insert(o, obj)
        id = id + 1
      end
    end
  `],
};
#endif

var Nr5gCellLockList = PageTableObj("Nr5gCellLockList", "Nr5g cell lock list",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaNr5gCellLock,
#endif
  sendThisObjOnly: true,
  editLabelText: "Nr5g cell lock setting",
  arrayEditAllowed: true,
  extraAttr: {
    thAttr: [
      {class:"customTh field6"},
      {class:"customTh field6"},
      {class:"customTh field6"},
      {class:"customTh field6"},
    ],
  },

  initObj:  function() {
      var obj: any = {};
      obj.gnbpci = "";
      obj.arfcn = "";
      obj.scs = "";
      obj.band = "";
      return obj;
  },

  onEdit: function (obj) {
    setEditButtonRowVisible(true);
    setPageObjVisible(false, "CellLockList");
  },

  offEdit: function () {
    setEditButtonRowVisible(false);
    setPageObjVisible(true, "CellLockList");
  },

  delObj: function (id) {
    sendNr5gCellLockCommand(id);
    return true;
  },

  saveObj: function (obj) {
    sendNr5gCellLockCommand(-1);
  },

  decodeRdb: function(obj) {
    // NR5G cell lock only support one cell lock at the moment.
    // If already locked then disable "add" button.
    setEnable(this.objName + "_addBtn", (obj.length == 0));
    return obj;
  },

  members: [
    staticTextVariable("gnbpci", "gNB PCI"),
    staticTextVariable("arfcn", "NR ARFCN"),
    staticTextVariable("scs", "SCS"),
    staticTextVariable("band", "NR SA band"),
  ],

  editMembers: [
    editableInteger("gnbpci", "gNB PCI"),
    editableInteger("arfcn", "NR ARFCN"),
    editableInteger("scs", "SCS"),
    editableInteger("band", "NR SA band"),
  ]
});

var pageData : PageDef = {
  title: "Cell lock",
  menuPos: ["Networking", "WirelessWAN", "CellLock"],
  pageObjects: [CellLockList, Nr5gCellLockList],
  onReady: function (){
    appendButtons({"save":"CSsave", "cancel":"cancel"});
    setEditButtonRowVisible(false);
  }
}
