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
      "  if r.enable == '1' then luardb.set('service.vpn.gre.profile',r.id) break end",
      "end",
      "local rc = setRdbArray(authenticated,rdbPrefix,rdbMembers,o)",
//    "luardb.set('openvpn.0.restart','1')",
      "return rc"
    ];
  },
  helpers: [
        "local rdbPrefix = 'link.profile'",
        "local rdbMembers = {'dev','delflag','name','serveraddress','ttl','local_ipaddr','enable','remote_ipaddr','remote_nwaddr','remote_nwmask','verbose_logging','reconnect_delay','reconnect_retries'}",
        "function getProfiles(auth) return getRdbArray(auth,rdbPrefix,7,32,false,rdbMembers) end"
    ]
};

var allProfiles; // All profiles received from server including non GRE

var GreVpnLinkProfile = PageTableObj("GreVpnLinkProfile", "gre client list",
{
  customLua: customLuaGetLinkProfile,
  editLabelText: "gre client edit",
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
      var obj: any = {};
      obj.id = id;
      obj.enable = "1";
      obj.dev =  "gre.0";
      obj.delflag = "0";
      obj.local_ipaddr = "";
      obj.remote_ipaddr = "";
      obj.remote_nwaddr = "";
      obj.remote_nwmask = "";
      obj.ttl = 255;
      obj.reconnect_delay = 30;
      obj.reconnect_retries = 0;
      obj.verbose_logging = "1";
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

  // This is called when the obects are recevied from the server, do some massaging
  decodeRdb: function(objs) {
    allProfiles = objs;

    // Only interested in GRE profiles
    var newObjs = objs.filter(function(obj){
      return obj.dev === "gre.0" && toBool(obj.delflag) === false;
    });

    return newObjs;
  },

  members: [
    staticTextVariable("name", "name"),
    staticTextVariable("serveraddress", "gre server address"),
    staticTextVariable("local_ipaddr","local tunnel address"),
    staticTextVariable("remote_ipaddr","remote tunnel address"),
    new EnabledTextVariable("enable", "enable")
  ],
  editMembers: [
    toggleVariable("enable", "enable vpn"),
    editableUsername("name", "profile name").setMaxLength(128),
    editableHostname("serveraddress", "gre server address").setMaxLength(128),
    editableIpAddressVariable("local_ipaddr","local tunnel address"),
    editableIpAddressVariable("remote_ipaddr","remote tunnel address"),
    editableIpAddressVariable("remote_nwaddr","remoteNetworkAddress"),
    editableIpMaskVariable("remote_nwmask","remoteNetworkMask"),
    editableBoundedInteger("ttl", "TTL", 0, 255, "Msg93", {helperText: "(0-255)"}).setSmall()
      .setMaxLength(5),
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
  menuPos: ["Internet", "GRE"],
  pageObjects: [GreVpnLinkProfile],
  alertSuccessTxt: "vpngreSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, true));
    setVisible("#buttonRow", "0");
  }
}

disablePageIfPppoeEnabled();
