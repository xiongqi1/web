var MacFilterEnable = PageObj("MacFilterEnable", "MAC Filtering",
{
    members: [
        toggleVariable("macFltEn", "Enable").setRdb("service.firewall.mac_whitelist.enable"),
        buttonAction("macFltEnSaveBtn", "Save", "sendSingleObject(MacFilterEnable);", "", { buttonStyle: "submitButton" }),
    ],
});

#ifdef COMPILE_WEBUI
var customLuaMacWhitelist = {
    lockRdb: false,
    get: function () {
        return ["=getRdbArray(authenticated,'service.firewall.mac_whitelist',0,32,true,{'name','macaddr','enable'})"];
    },
    set: function () {
        return [
            "local termObj = {}",
            "termObj.__id=#o.objs",
            "termObj.name=''",
            "termObj.macaddr=''",
            "termObj.enable=''",
            "o.objs[#o.objs+1]=termObj",
            "local rc = setRdbArray(authenticated,'service.firewall.mac_whitelist',{'name','macaddr','enable'},o)",
            "luardb.set('service.firewall.mac_whitelist.num',termObj.__id)",
            "luardb.set('service.firewall.mac_whitelist.trigger','1')",
            "return rc"
        ];
    },
    helpers: [
        "function isValidName(val)",
        " if val and #val >= 1 and #val <= 32 "
        + "and val:match(" + '"' + "[!()*/0-9;?A-Z_a-z-]+" + '"' + ") then",
        "  return true",
        " end",
        " return false",
        "end"
    ]
};
#endif

var MacWhitelist = PageTableObj("MacWhitelist", "MAC Whitelist",
{
#ifdef COMPILE_WEBUI
    customLua: customLuaMacWhitelist,
#endif
    sendThisObjOnly: true,
    editLabelText: "MAC Whitelist Settings",
    arrayEditAllowed: true,

    initObj: function () {
        var obj: any = {};
        obj.name = "";
        obj.macaddr = "";
        obj.enable = 0;
        return obj;
    },

    onEdit: function (obj) {
        setEditButtonRowVisible(true);
        setPageObjVisible(false, "MacFilterEnable");
    },

    offEdit: function () {
        setEditButtonRowVisible(false);
        setPageObjVisible(true, "MacFilterEnable");
    },

    members: [
        staticTextVariable("name", "Name"),
        staticTextVariable("macaddr", "MAC address"),
        staticTextVariable("enable", "Enable"),
    ],

    editMembers: [
        editableTextVariable("name", "Name")
            .setMaxLength(32)
            .setValidate(
                function (val) { if (val.length == 0) return false; return true; }, "Name is invalid",
                function (field) { return nameFilter(field); },
                "isValidName(v)"
            ),
        editableTextVariable("macaddr", "MAC address")
            .setMaxLength(17)
            .setValidate(
                function (field) { return isValidMacAddress(field); },
                "Invalid MAC address. Please specify a HEX value."
            ),
        toggleVariable("enable", "Enable"),
    ]
});

var pageData: PageDef = {
    title: "MAC whitelist",
    menuPos: ["Networking", "Firewall", "MacWhitelist"],
    pageObjects: [MacFilterEnable, MacWhitelist],
    onReady: function () {
        appendButtons({ "save": "CSsave", "cancel": "cancel" });
        setButtonEvent('save', 'click', sendObjects);
        setEditButtonRowVisible(false);
    }
}

