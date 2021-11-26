var customLuaGetLinkProfile = {
  lockRdb : false,
  get: function() {
    return  ["=getProfiles(authenticated)"]
  },
  validate: function(lua) { // Added a check for delfag at the front of array
    return [
      "if o.delflag ~= '0' then o.name='' o.enable='0' return true end"
    ].concat(lua);
  },
  set: function() {
    return [
      "local objs = o.objs",
      "for _, r in ipairs(objs) do", // set the profile id to first enabled
      "  if r.enable == '1' then luardb.set('service.vpn.pptp.profile',r.id) break end",
      "end",
      "local rc = setRdbArray(authenticated,rdbPrefix,rdbMembers,o)",
      "luardb.set('openvpn.0.restart','1')",
      "return rc"
    ];
  },
  helpers: [
        "local rdbPrefix = 'link.profile'",
        "local rdbMembers = {'dev','delflag','name','serveraddress','user','pass','enable','authtype','defaultroutemetric','mtu','default.dnstopptp','snat','default.defaultroutemetric','mppe_en','opt','verbose_logging','reconnect_delay','reconnect_retries'}",
        "function getProfiles(auth) return getRdbArray(auth,rdbPrefix,7,32,false,rdbMembers) end"
    ]
};

var allProfiles; // All profiles received from server including non PPTP
var def_mtu = "<%get_single_direct('system.config.mtu');%>";

var PptpVpnLinkProfile = PageTableObj("PptpVpnLinkProfile", "pptp client list",
{
  customLua: customLuaGetLinkProfile,
  editLabelText: "pptp client edit",
  arrayEditAllowed: true,
  tableAttribs: {class:"above-5-column"},

  initObj:  function() { // This function is called when the add button is pressed
      var id;

      // Find a free slot in RDB profile array
      if (!allProfiles.some(function (prof){
          id = prof.id;
          if (!isDefined(prof.delflag)) return true;
          if (toBool(prof.delflag)) return true;
          if ( prof.name == "") return true;
          return false;
        })){
          return;
      }
      // Initialise all the members
      var obj : any = {};
      obj.id = id;
      obj.enable = "1";
      obj.dev =  "pptp.0";
      obj.delflag = "0";
      obj.authtype = "any";
      obj.defaultroutemetric = 10;
      obj.mtu = Number(def_mtu) - 40;
      obj.reconnect_delay = 30;
      obj.reconnect_retries = 0;
      obj.mppe_en = "1";
      obj.default_dnstopptp = "0";
      obj.snat = "0";
      obj.default_defaultroutemetric = "0";
      obj.opt = "";
      obj.verbose_logging = "0";
      return obj;
  },

  // This is called when user deletes the profile
  // We'll override the default by not deleting this profile from the array
  delObj:  function(id) {
    this.obj.some(function(obj){
      if (obj.id === id) {
        obj.delflag = "1"; // The server side will also clear name and set enable to false
      }
    });
    return false; // false says don't delete this from array
  },

  onEdit: function (obj) {
    setVisible("#buttonRow", "1");
  },

  offEdit: function () {
    setVisible("#buttonRow", "0");
  },

  // This is called when the edited object is saved
  saveObj:function (obj) {
    // restore from the shadow variables that were created in the next function decodeRdb()
    obj["default.defaultroutemetric"] = obj.default_defaultroutemetric;
    obj["default.dnstopptp"] = obj.default_dnstopptp;
  },

  // This is called when the obects are recevied from the server, do some massaging
  decodeRdb: function(objs) {
    allProfiles = objs;

    // Only interested in PPTP profiles
    var newObjs = objs.filter(function(obj){
      return obj.dev === "pptp.0" && toBool(obj.delflag) === false;
    });

    // The html side doesn't like the dots in the name so
    // create a few shadow members
    newObjs.forEach(function(obj){
      obj.default_defaultroutemetric = obj["default.defaultroutemetric"];
      obj.default_dnstopptp = obj["default.dnstopptp"];
    });
    return newObjs;
  },

  members: [
    staticTextVariable("name", "name"),
    staticTextVariable("serveraddress", "remotePPTPaddress"),
    staticTextVariable("user", "user name"),
    new EnabledTextVariable("enable", "enable")
  ],
  editMembers: [
    toggleVariable("enable", "enable pptp client"),
    editableUsername("name", "name").setMaxLength(64),
    editableUsername("user", "user name").setMaxLength(64),
    editableTextVariable("pass", "password").setMaxLength(64),
    editableHostname("serveraddress", "pptp server address").setMaxLength(64),
    selectVariable("authtype", "authentication type",
        function(obj){return ["any","ms-chap-v2","ms-chap","chap","eap","pap"];
    }),
    editableBoundedInteger("defaultroutemetric", "metric", 0, 65535, "Msg48", {helperText: "(0-65535)"}).setSmall(),
    editableBoundedInteger("mtu", "mtu", 68, 65535, "field68and65535", {helperText: "(68-65535)"}).setSmall(),

    toggleVariable("default_dnstopptp", "use peer dns"),
    toggleVariable("snat", "NatMasq"),
    toggleVariable("default_defaultroutemetric", "default route to pptp"),
    toggleVariable("mppe_en", "MPPE"),
    editableTextVariable("opt", "extraPPPoption", {required: false}).setMaxLength(64),
    toggleVariable("verbose_logging", "verbose logging"),
    editableBoundedInteger("reconnect_delay", "reconnectDelay", 30, 65535, "Msg91", {helperText: "thirtyTo65535secs"}).setSmall()
      .setMaxLength(5),
    editableBoundedInteger("reconnect_retries", "reconnectRetries", 0, 65535, "Msg92", {helperText: "zTo65535Unlimited"}).setSmall()
      .setMaxLength(5),
]
});

var pageData : PageDef = {
#if defined V_NETWORKING_UI_none || defined V_VPN_none
  onDevice : false,
#endif
  title: "NetComm Wireless Router",
  menuPos: ["Internet", "pptpClient"],
  pageObjects: [PptpVpnLinkProfile],
  alertSuccessTxt: "pptpcSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, true));
    setVisible("#buttonRow", "0");
  }
}

disablePageIfPppoeEnabled();
