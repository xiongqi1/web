var customLuaStaticRoutes = {
  lockRdb : false,
  get: function() {
    return ["=getRdbArray(authenticated,'service.router.static.route',0,99,true,{''})"]
  },
  set: function() {
    return [
      "local objs = o.objs",
      "for _, r in ipairs(objs) do",
      "  r.rawdata=r.name..','..r.destIp..','..r.destMask..','..r.gwyIp..','..r.metric..','..r.netIf",
      "end",
      "local termObj = {} termObj.id=#o.objs  termObj.rawdata = '' o.objs[#o.objs+1]=termObj",
      "local rc = setRdbArray(authenticated,'service.router.static.route',{''},o)",
      "luardb.set('service.router.static.route.trigger','1')",
      "return rc"
    ];
  },
  helpers: [
        "function isValidRouteName(val)",
        " if val and #val >= 1 and #val <= 64 "
          + "and val:match("+'"'+"[!()*/0-9;?A-Z_a-z-]+"+'"'+") then",
        "  return true",
        " end",
        " return false",
        "end"
    ]
};

class RouteName extends StaticTextVariable {
  setVal(obj: any, val: string) {
    var len = val.length;
    if (len >= 32)
        val = val.substring(0,10)+'...'+val.substring(len-10);
    $("#"+this.getHtmlId()).html(val);
  }
}

var StaticRoutes = PageTableObj("StaticRoutes", "staticRoutingList",
{
  customLua: customLuaStaticRoutes,
  editLabelText: "static route",
  arrayEditAllowed: true,
  colgroup: ["120px","160px","160px","160px","100px","100px","auto","auto"],
  tableAttribs: {class:"above-5-column"},
  initObj:  function() {
      var obj: any = {};
      obj.name = "";
      obj.destIp = "";
      obj.destMask = "";
      obj.gwyIp = "";
      obj.metric = "";
      obj.netIf = "";
      return obj;
  },

  getValidationError: function(obj: any) {

    if (obj.destIp === "") return "routing warningMsg08";
    if (obj.gwyIp === "" && obj.netIf === "auto" ) return "routing warningMsg10";
    switch(isValidSubnetMask(obj.destMask)) {
      case -1:
        return "invalidSubnetMask";
      case -2:
        return "wlan warningMsg16"; //The subnet mask has to be contiguous. Please enter a valid mask
    }
  },

  onEdit: function (obj) {
    setVisible("#objouterwrapperActiveRoutes", "0");
    setVisible("#buttonRow", "1");
  },

  offEdit: function () {
    setVisible("#buttonRow", "0");
    setVisible("#objouterwrapperActiveRoutes", "1");
  },

  saveObj:function (obj) {
      var txt = obj.netIf;
      if (txt == "wwan") txt = "wwan interface";
      obj.netIfTxt = _(txt);
  },

  decodeRdb: function(objs: any[]) {
    var newObjs = [];
    var updatePrivate = this.saveObj;
    objs.forEach(function(obj){
      var bits = obj.rawdata.split(",");
      var newObj: any = {};
      newObj.id = obj.id;
      newObj.name = bits[0];
      newObj.destIp = bits[1];
      newObj.destMask = bits[2];
      newObj.gwyIp = bits[3];
      newObj.metric = bits[4];
      newObj.netIf = bits[5];
      updatePrivate(newObj);
      newObjs[newObjs.length] = newObj;
    });
    return newObjs;
  },

  members: [
    new RouteName("name", "route name"),
    staticTextVariable("destIp", "destipaddr"),
    staticTextVariable("destMask", "subnet mask"),
    staticTextVariable("gwyIp", "gatewayip"),
    staticTextVariable("netIfTxt", "network interface"),
    staticTextVariable("metric", "routing del metric")
  ],
  editMembers: [
    editableTextVariable("name", "route name")
        .setMaxLength(64)
        .setValidate(
          function (val) { if (val.length == 0) return false; return true;}, "routeNameInvalid",
          function (field) {return nameFilter(field);},
          "isValidRouteName(v)"
        ),
#ifdef COMPILE_WEBUI
    editableIpAddressVariable("destIp", "destipaddr"),
#else
    editableIpAddressVariable("destIp", "destipaddr", genHtmlIpBlocks0),
#endif
    editableIpMaskVariable("destMask", "subnet mask"),
    editableIpAddressVariable("gwyIp", "gatewayip"),
    selectVariable("netIf", "network interface",
        function(obj){return ["auto", ["wwan","wwan interface"],
#ifdef V_BRIDGE_none
                              "eth0"
#else
                              "br0"
#endif
                             ];}),
    editableBoundedInteger("metric", "routing del metric", 0, 32766, "staticRouteMetricRangeGuide", {helperText: "0-32766"})
        .setSmall()
  ]
});

var ActiveRoutes = PageTableObj("ActiveRoutes", "activeRoutingList",
{
  customLua: true,
  readOnly: true,
  colgroup: ["110px","110px","110px","60px","70px","70px","70px","100px"],
  tableAttribs: {class:"above-5-column"},

  // We get the complete output of "route -n" in lines
  decodeRdb: function(objs) {
    var newObjs = [];
    var id = 1;
    objs.forEach(function(obj){
      var bits = obj.trim().split(/\s+/);
      if (bits.length !== 8) return;
      if (bits[0].indexOf(".") < 0) return;
      var newObj: any = {};
      newObj.id = id++;
      newObj.dest = bits[0];
      newObj.gwy = bits[1];
      newObj.mask = bits[2];
      newObj.flags = bits[3];
      newObj.metric = bits[4];
      newObj.ref = bits[5];
      newObj.use = bits[6];
      newObj.if = bits[7];
      newObjs[newObjs.length] = newObj;
    });
    return newObjs;
  },

  members: [
    staticTextVariable("dest", "destination"),
    staticTextVariable("gwy", "gateway"),
    staticTextVariable("mask", "netmask"),
    staticTextVariable("flags", "routing del flags"),
    staticTextVariable("metric", "routing del metric"),
    staticTextVariable("ref", "ref"),
    staticTextVariable("use", "routing del use"),
    staticTextVariable("if", "interface")
  ]
});

var pageData : PageDef = {
#if defined V_NETWORKING_UI_none
  onDevice : false,
#endif
  title: "NetComm Wireless Router",
  menuPos: ["Internet", "STATIC_ROUTING"],
  pageObjects: [StaticRoutes, ActiveRoutes],
  alertSuccessTxt: "routingSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, true));
    setVisible("#buttonRow", "0");
  }
}

disablePageIfPppoeEnabled();
