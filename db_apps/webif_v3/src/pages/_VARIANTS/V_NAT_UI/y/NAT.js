#ifdef COMPILE_WEBUI
var customLuaDNAT = {
  lockRdb : false,
  get: function() {
    return ["=getRdbArray(authenticated,'service.firewall.dnat',0,49,true,{''})"]
  },
  set: function() {
    return [
      // Combine DNAT rules, save to RDB variables and delete leftover rules
      // then set trigger variable
      // rawdata example
      // name, enable (0|1), pdn index number, public port, local IP address, local port,
      // protocol (UDP,TCP,TCP/UDP)
      "local objs = o.objs",
      "for _, r in ipairs(objs) do",
      "  r.rawdata = r.ruleName..','..r.enable..','..r.indexNo..','",
      "  r.rawdata = r.rawdata..r.oport..','..r.dip..','..r.dport..','..r.prot",
      "end",
      "local termObj = {} termObj.__id=#o.objs  termObj.rawdata = '' o.objs[#o.objs+1]=termObj",
      "local rc = setRdbArray(authenticated,'service.firewall.dnat',{''},o)",
      "luardb.set('service.firewall.dnat.trigger','1')",
      "return rc"
    ];
  },
  helpers: [
        "function isValidName(val)",
        " if val and #val >= 1 and #val <= 32 "
          + "and val:match("+'"'+"[!()*/0-9;?A-Z_a-z-]+"+'"'+") then",
        "  return true",
        " end",
        " return false",
        "end"
    ]
};
#endif

var DynamicNAT = PageTableObj("DynamicNAT", "Port Forwarding List",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaDNAT,
#endif
  sendThisObjOnly: true,
  editLabelText: "Port Forwarding Settings",
  arrayEditAllowed: true,
  extraAttr: {
    tableAttr: {class:"above-5-column"},
    thAttr: [
      {class:"customTh field1"},
      {class:"customTh field1"},
      {class:"customTh field2"},
      {class:"customTh field1"},
      {class:"customTh field1"},
      {class:"customTh field1"},
      {class:"customTh field4"},
    ],
  },

  initObj:  function() {
      var obj: any = {};
      obj.ruleName = "";
      obj.indexNo = 1;
      obj.prot = "";
      obj.oport = "";
      obj.dip =  "";
      obj.dport =  "";
      obj.enable =  0;
      return obj;
  },

  onEdit: function (obj) {
    setEditButtonRowVisible(true);
    setPageObjVisible(false, "DynamicNATV6");
  },

  offEdit: function () {
    setEditButtonRowVisible(false);
    setPageObjVisible(true, "DynamicNATV6");
  },

  decodeRdb: function(objs: any[]) {
    var newObjs = [];
    objs.forEach(function(obj){
      // rawdata example
      // name, enable (0|1), pdn index number, public port, local IP address, local port, protocol (UDP,TCP,TCP/UDP)
      var ar = obj.rawdata.split(",");
      var newObj: any = {};
      newObj.__id = obj.__id;
      newObj.ruleName = ar[0];
      newObj.enable = ar[1];
      newObj.indexNo = ar[2];
      newObj.oport = ar[3];
      newObj.dip = ar[4];
      newObj.dport = ar[5];
      newObj.prot = ar[6];
      newObjs[newObjs.length] = newObj;
    });
    return newObjs;
  },

  members: [
    staticTextVariable("ruleName", "Name"),
    staticTextVariable("indexNo", "Profile no."),
    staticTextVariable("prot", "Protocol"),
    staticTextVariable("oport", "Public port"),
    staticTextVariable("dip", "Local IP address"),
    staticTextVariable("dport", "Local port"),
    staticTextVariable("enable", "Enable"),
  ],

  editMembers: [
    editableTextVariable("ruleName", "Rule name")
      .setMaxLength(32)
      .setValidate(
        function (val) { if (val.length == 0) return false; return true;}, "Rule name is invalid",
        function (field) {return nameFilter(field);},
        "isValidName(v)"
      ),
    editableBoundedInteger("indexNo", "Profile no.", 1, 6, "Please specify a value between 1 and 6", {helperText: _("1-6")}),
    selectVariable("prot", "Protocol", function(o){
      return [
        ["TCP","TCP"],
        ["UDP","UDP"],
        ["TCP/UDP","TCP/UDP"],
      ]}),
    editablePortNumber("oport", "Public port"),
#ifdef COMPILE_WEBUI
    editableIpAddressVariable("dip", "Local IP address"),
#else
    editableIpAddressVariable("dip", "Local IP address", genHtmlIpBlocks0),
#endif
    editablePortNumber("dport", "Local port"),
    toggleVariable("enable", "Enable"),
  ]
});

var pageData : PageDef = {
  title: "NAT",
  menuPos: ["Networking", "Firewall", "NAT"],
  pageObjects: [DynamicNAT],
  onReady: function (){
    appendButtons({"save":"CSsave", "cancel":"cancel"});
    setButtonEvent('save', 'click', sendObjects);
    setEditButtonRowVisible(false);
  }
}
