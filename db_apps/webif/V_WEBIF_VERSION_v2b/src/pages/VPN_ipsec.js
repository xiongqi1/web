var IPSEC_KEY_DIR = "/etc/ipsec.d/";
var IPSEC_CERTS_DIR = IPSEC_KEY_DIR + "certs/";
var IPSEC_CACERTS_DIR = IPSEC_KEY_DIR + "cacerts/";
var IPSEC_CRLCERTS_DIR = IPSEC_KEY_DIR + "crls/";
var IPSEC_PRIVATEKEY_DIR = IPSEC_KEY_DIR + "private/";
var IPSEC_RSAKEY_DIR = IPSEC_KEY_DIR + "rsakey/";

var customLuaGetVPNProfiles = {
  lockRdb : false,
  get: function(lua: string[]) {
    return lua.concat(["o.array=getProfiles(authenticated)"]);
  },
  validate: function(lua) { // Added a check for delfag at the front of array
    return [
      "if o.delflag ~= '0' then o.name='' o.enable='0' return true end"
    ].concat(lua);
  },
  set: function() {
    return [
      "local objs=o.objs",
      "local first=true",
      "for _, r in ipairs(objs) do", // set the profile id to first enabled
      "  if first and r.enable=='1' then",
      "    luardb.set('ipsec.0.profile',r.id)",
      "    first=false",
      "  end",
      "  if r.generate==true then",
      "    os.execute('/usr/netipsec/sbin/ipsec newhostkey --output " + IPSEC_RSAKEY_DIR + "leftrsa'..r.id..'.key --random /dev/urandom > /dev/null')",
      "  end",
      "end",
      "local rc = setRdbArray(authenticated,rdbPrefix,rdbMembers,o)",
      "return rc"
    ];
  },
  helpers: [
        "local rdbPrefix = 'link.profile'",
        "local rdbMembers = {'key_password','rsa_remoteid','rsa_localid','psk_remoteid','psk_localid','psk_value','ipsec_method','ike_mode','pfs','ike_hash','ike_enc','ipsec_dhg','ipsec_dpd','dpd_time','dpd_timeout','ike_time','life_time','ipsec_enc','enccap_protocol','ipsec_hash','dev','delflag','name','enable','remote_lan','remote_mask','local_lan','local_mask','remote_gateway'" +
#ifndef V_SCEP_CLIENT_none
        ",'scep_remoteid'" +
#endif
                "}",
        "function getProfiles(auth) return getRdbArray(auth,rdbPrefix,7,32,false,rdbMembers) end"
    ]
};

var allProfiles; // All profiles received from server including non IPSec profiles
var editedObj;   // The object being edited

// Get parameter for the file upload post
function getParam(fname: string) {
  return [editedObj.id];
}

// Get parameter for the file upload post
function getParamExt(fname: string) {
  var idx = fname.lastIndexOf('.');
  if (idx >= 0) {
    return [editedObj.id, fname.substring(idx)];
  }
  return [editedObj.id];
}

var localRsaFile = new FileUploader("localRsaFile","local rsa key upload", IPSEC_RSAKEY_DIR + "leftrsa*.key", [".key"], getParam, "Msg76", "Msg77");

var IpsecVpnLinkProfile = PageTableObj("IpsecVpnLinkProfile", "IPsecList",
{
  customLua: customLuaGetVPNProfiles,
  editLabelText: "ipSecProfileEdit",
  arrayEditAllowed: true,
  tableAttribs: {class:"above-5-column"},

  initObj:  function() { // This function is called when the add button is pressed
      var id;

      // Find a free slot in RDB profile array
      if (!allProfiles.some(function (prof){
          id = prof.id;
          if (!isDefined(prof.delflag)) return true;
          if (toBool(prof.delflag)) return true;
          if (prof.name == "") return true;
          return false;
        })){
          return;
      }
      // Initialise all the members
      var obj = {
        id:  id,
        name: "IpSec" + String(id),
        enable: "1",
        dev:  "ipsec.0",
        delflag: "0",
        ipsec_method: "psk",
        ike_mode: "main",
        ike_time: "3600",
        ipsec_dpd: "hold",
        dpd_time : "10",
        dpd_timeout: "60",
        life_time: "28800",
        enccap_protocol: "esp",
        remote_lan: "0.0.0.0",
        remote_mask: "255.255.255.0",
        local_lan: "0.0.0.0",
        local_mask: "255.255.255.0"
      };
      return obj;
  },

  // This is called when user deletes the profile
  // We'll override the default by not deleting this profile from the array
  delObj:  function(id) {
    this.obj.some(function(obj: any){
      if (obj.id === id) {
        obj.delflag = "1"; // The server side will also clear name and set enable to false
      }
    });
    return false; // false says don't delete this from array
  },

  onEdit: function (obj) {
    editedObj = obj;
    onChangeIpsecMode({value: obj.ipsec_method});
    setVisible("#buttonRow", "1");
  },

  offEdit: function () {
    setVisible("#buttonRow", "0");
  },

  // This is called when the edited object is saved
  saveObj:function (obj) {
  },

  // This is called when the obects are recevied from the server, do some massaging
  decodeRdb: function(objs) {
    allProfiles = objs.array;

    // Only interested in PPTP profiles
    var newObjs = allProfiles.filter((o) => o.dev === "ipsec.0" && toBool(o.delflag) === false);

    // The html side doesn't like the dots in the name so
    // create a few shadow members
    for (let obj of newObjs) {
      obj.remoteLanAddressMask = obj.remote_lan + "/" + maskBits(obj.remote_mask);
      obj.localLanAddressMask = obj.local_lan + "/" + maskBits(obj.local_mask);

      // The file listings are not part of the array so copy them in.
      obj.localRsaFile = objs.localRsaFile;
      obj.remoteRsaFile = objs.remoteRsaFile;
      obj.localPrivateKeyFile = objs.localPrivateKeyFile;
      obj.localPublicCertFile = objs.localPublicCertFile;
      obj.remotePublicCertFile = objs.remotePublicCertFile;
      obj.caCertFile = objs.caCertFile;
      obj.crlCertFile = objs.crlCertFile;
    };
    return newObjs;
  },

  members: [
    staticTextVariable("name", "name"),
    staticTextVariable("remoteLanAddressMask", "remoteLanAddressMask"),
    staticTextVariable("remote_gateway", "remoteGateway"),
    staticTextVariable("localLanAddressMask", "localLanAddressMask"),
    new EnabledTextVariable("enable", "enable")
  ],
  editMembers: [
    toggleVariable("enable", "ipSecProfile"),
    editableUsername("name", "profile name").setMaxLength(64),
    sectionHeader("ipSecPhase1"),
    editableURL("remote_gateway", "remoteIPsecServerAddress").setMaxLength(128),
    selectVariable("ipsec_method", "key mode", (obj) => [
        ["psk","preSharedKeys"],
        ["rsa","rsa keys"],
        ["cert","certificates"]
#ifndef V_SCEP_CLIENT_none
        ,["scep","scepClient"]
#endif
      ], "onChangeIpsecMode"),
    //---- This section varies depend on mode
    editableStrongPasswordVariable("psk_value","preSharedKey")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "psk")
      .setClearText(true),
    editableTextVariable('psk_remoteid',"remote id", {helperText: "xySampleOrBlank", required: false}).setMaxLength(64)
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "psk"),
    editableTextVariable('psk_localid',"local id", {helperText: "xySampleOrBlank", required: false}).setMaxLength(64)
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "psk"),
    //---- This section varies depend on mode
    editableTextVariable('rsa_remoteid',"remote id", {helperText: "xySampleOrBlank", required: false}).setMaxLength(64)
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "rsa"),
    editableTextVariable('rsa_localid',"local id", {helperText: "xySampleOrBlank", required: false}).setMaxLength(64)
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "rsa"),
    staticText("update_time", "update time2","Sep 19 2011 14:18:00")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "rsa"),
    buttonAction("generate", "generate", "onGenerate")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "rsa"),
    buttonAction("download", "download", "onDownload")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "rsa"),
    localRsaFile.setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "rsa"),
    new FileUploader("remoteRsaFile","remote rsa key upload", IPSEC_RSAKEY_DIR + "rightrsa*.key", [".key"], getParam, "Msg76", "Msg77")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "rsa"),
    //---- This section varies depend on mode
    editablePasswordVariable("key_password","privateKeyPassphrase")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "cert"),
    new FileUploader("localPrivateKeyFile","local private key", IPSEC_PRIVATEKEY_DIR + "local**", [".crt",".key",".pem"], getParamExt, "Msg74", "Msg77")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "cert"),
    new FileUploader("localPublicCertFile","local public certificate", IPSEC_CERTS_DIR + "local**", [".crt",".key",".pem"], getParamExt, "Msg74", "Msg77")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "cert"),
    new FileUploader("remotePublicCertFile","remote public certificate", IPSEC_CERTS_DIR + "remote**", [".crt",".key",".pem"], getParamExt, "Msg74", "Msg77")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "cert"),
    new FileUploader("caCertFile","ca certificate", IPSEC_CACERTS_DIR + "ca**", [".crt",".key",".pem"], getParamExt, "Msg74", "Msg77")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "cert"),
    new FileUploader("crlCertFile","crl certificate", IPSEC_CRLCERTS_DIR + "crl**", [".crt",".key",".pem"], getParamExt, "Msg74", "Msg77")
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "cert"),
    //---- This section varies depend on mode
#ifndef V_SCEP_CLIENT_none
    editableTextVariable('scep_remoteid',"scep remote id",
      {helperText: "distinguishedName", helperAttr: {class: "field-des"}})
      .setMaxLength(64)
      .setIsVisible( () => IpsecVpnLinkProfile.obj.ipsec_method == "scep"),
#endif
    //----
    selectVariable("ike_mode", "ike mode", (obj) => ["any","main","aggressive"]),
    selectVariable("pfs", "Pfs", (obj) => [["on","On"],["off","Off"]]),
    selectVariable("ike_enc", "ike encryption", (obj) => ["any","aes","aes128","aes192","aes256","3des","des"]),
    selectVariable("ike_hash", "ike hash", (obj) => ["any","md5","sha1"]),
    selectVariable("ipsec_dhg", "dh group", (obj) => [
          "any", ["modp768","group1"], ["modp1024","group2"], ["modp1536","group5"],
          ["modp2048","group14"], ["modp3072","group15"], ["modp4096","group16"], ["modp6144","group17"],
          ["modp8192","group18"]]),
    editableBoundedInteger("ike_time", "ike rekey time", 0, 78400, "ike warningMsg", {helperText: "Msg79"}).setSmall(),
    selectVariable("ipsec_dpd", "dpd action", (obj) => ["none","clear","hold","restart"]),
    editableInteger("dpd_time", "dpd keep alive time", {helperText: "secs"}).setSmall(),
    editableInteger("dpd_timeout", "dpd timeout", {helperText: "secs"}).setSmall(),
    editableBoundedInteger("life_time", "sa life time", 0, 78400, "sa warningMsg", {helperText: "Msg79"}).setSmall(),

    sectionHeader("ipSecPhase2"),
    editableIpAddressVariable("remote_lan","remoteLanAddress"),
    editableIpMaskVariable("remote_mask","remoteLanSubnetMask"),
    editableIpAddressVariable("local_lan","localLanAddress"),
    editableIpMaskVariable("local_mask","localLanSubnetMask"),
    selectVariable("enccap_protocol", "encapsulationType", (obj) => ["any",["esp","ESP"],["ah","AH"]]),
    selectVariable("ipsec_enc", "IPsec encryption", (obj) => ["any","aes","aes128","aes192","aes256","3des","des"]),
    selectVariable("ipsec_hash", "IPsec hash", (obj) => ["any","md5","sha1"])
  ]
});

function onGenerate() {
  editedObj.generate = true;
  $("button").prop("disabled", true);
  let orgCursor = document.body.style.cursor;
  document.body.style.cursor = 'wait';
  sendObjects(
    () => {
      // Success, update edittedObj with that received and update status of file to loaded
      for (var obj of IpsecVpnLinkProfile.obj) {
        if (obj.id === editedObj.id) {
          editedObj = obj;
          localRsaFile.setVal(obj, obj.localRsaFile);
        }
      }
      document.body.style.cursor = orgCursor;
    },
    (err?: any) => {
      document.body.style.cursor = orgCursor;
    }
  );
}

function onDownload() {
  let url: string = "/cgi-bin/ipsec_action.cgi?"
                    + "action=dnrsa&"
                    + "param=" + String(editedObj.id)
                    + "&csrfTokenGet=" + csrfToken;
  window.location.assign(url);
}

function onChangeIpsecMode(_this) {
  var method =  _this.value;
  IpsecVpnLinkProfile.obj.ipsec_method = method;
  setMemberVisibility(IpsecVpnLinkProfile, true, IpsecVpnLinkProfile.editMembers);
}

var pageData: PageDef = {
#if defined V_NETWORKING_UI_none
  onDevice: false,
#endif
  title: "NetComm Wireless Router",
  menuPos: ["Internet", "IP_Sec"],
  pageObjects: [IpsecVpnLinkProfile],
  alertSuccessTxt: "ipsecSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, true));
    setVisible("#buttonRow", "0");
  }
}

disablePageIfPppoeEnabled();
