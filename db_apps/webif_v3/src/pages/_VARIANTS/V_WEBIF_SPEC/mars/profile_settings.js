/*
 * Wireless WAN link profile settings for Saturn/Neptune
 *
 * Copyright (C) 2020 Casa Systems
 */

var ProfileCfg = PageObj("ProfileCfg", "Profile settings",
{
#ifdef COMPILE_WEBUI
    rdbIndexQueryParam: "id", // the query string param to be used as RDB index
    defaultId: 1, // if page is visited without param id, use profile 1
    customLua: {
        get: (arr) => {
#ifdef V_LOCK_IMS_SOS_PROFILE_y
            arr.push("o.profID = tonumber(objHandler.queryParams." +
                     ProfileCfg.rdbIndexQueryParam + ") or " +
                     ProfileCfg.defaultId );
            arr.push("o.writable = o.profID ~= 2 and o.profID ~= 3 and true");
#else
            arr.push("o.writable = true");
#endif
            arr.push("o.vlans = getVlanList(true)");
            return arr;
        },
        validate: (arr) => {
            arr.push("v=o.profVlanIdx if not isValid.Key(v, getVlanList(true)) and v ~= '' then return false, 'oops! ' .. 'profVlanIdx' end");
            return arr;
        },
        set: (arr) => {
            arr.push("if (toBool(o.profEnable) and o.writable) then luardb.set(string.format('link.profile.%d.writeflag', tonumber(objHandler.queryParams." +
                     ProfileCfg.rdbIndexQueryParam + ") or " +
                     ProfileCfg.defaultId + "), '1') end");
            return arr;
        }
    },
#endif
#ifdef V_READONLY_WWAN_PROFILE_y
    readOnly: true,
#endif
    members: [
        objVisibilityVariable("profEnable", "Enable")
        .setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.enable"),
        editableTextVariable("profName", "Name", {required: false}).setRdb("link.profile.%d.name"),
        editableTextVariable("profApn", "APN", {required: false})
        .setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.apn"),
        editableUsername("profUser", "Username", {required: false})
        .setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.user"),
        editablePasswordVariable("profPasswd", "Password", {required: false})
        .setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.pass"),
        selectVariable("profAuthType", "Authentication type",
                    (o) => { return [["chap", "CHAP"], ["pap","PAP"], ["none","None"]]; }
        ).setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.auth_type"),
        selectVariable("profPdpType", "PDP type",
                    (o) => { return [["ipv4", "IPv4"], ["ipv6","IPv6"], ["ipv4v6", "IPv4v6"]]; }
        ).setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.pdp_type"),
        selectVariable("profVlanIdx", "Map to LAN/VLAN",
                    (o) => {
                        let vlans: OptionsType[] = [["", "None"]];
                        if (isDefined(o) && o.vlans.constructor == Object) {
                            for (const ix in o.vlans) {
                                vlans.push([ix, o.vlans[ix]]);
                            }
                        }
                        return vlans;
                    },
                    undefined,
                    true
        ).setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.vlan_index"),
        toggleVariable("profIpHandoverEnable", "IP passthrough")
        .setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.ip_handover.enable"),
        toggleVariable("profAdminAccess", "Remote admin access")
        .setConditionsForCheck(["writable"])
        .setRdb("link.profile.%d.admin_access_allowed"),
        hiddenVariable("writable", ""),
    ],
    decodeRdb: (obj) => {
        if (obj.profAuthType != "chap" && obj.profAuthType != "pap") {
            obj.profAuthType = "none";
        }
        return obj;
    },
});

function onClickSaveBtnProfileSetting() {
    sendObjects(function() {
        removeValidClass("inp_profUser");
    });
}

var pageData : PageDef = {
    title: "Wireless WAN profile settings",
    menuPos: ["Networking", "WirelessWAN", "WWAN"],
    pageObjects: [ProfileCfg],
    alertSuccessTxt : "You wireless WAN configuration changes have been successfully saved.",
    onReady: () => {
#ifdef V_READONLY_WWAN_PROFILE_y
		appendButtons({"cancel":"cancel"});
#else
#ifdef V_LOCK_IMS_SOS_PROFILE_y
        if (ProfileCfg.obj['writable'] == false) {
            setEnable("inp_profApn", false)
            setEnable("inp_profUser", false)
            setEnable("inp_profPasswd", false)
            setEnable("sel_profAuthType", false)
            setEnable("sel_profPdpType", false)
            setEnable("sel_profVlanIdx", false)
            setToggleEnable("inp_profIpHandoverEnable", false)
            setToggleEnable("inp_profAdminAccess", false)
        }
#endif
        appendButtons({"save":"CSsave", "cancel":"cancel"});
        setButtonEvent("save", "click", onClickSaveBtnProfileSetting);
#endif
        setButtonEvent("cancel", "click", () => {window.location.href = "/profile_list.html";});
    }
};
