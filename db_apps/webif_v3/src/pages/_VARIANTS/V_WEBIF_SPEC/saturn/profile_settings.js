/*
 * Wireless WAN link profile settings for Saturn/Neptune
 *
 * Copyright (C) 2020 Casa Systems
 */

var NumProfiles = 6;

var ProfileCfg = PageObj("ProfileCfg", "Profile settings",
{
#ifdef COMPILE_WEBUI
    rdbIndexQueryParam: "id", // the query string param to be used as RDB index
    defaultId: 1, // if page is visited without param id, use profile 1
    customLua: {
        get: (arr) => {
            arr.push("o.vlans = getVlanList(true)");
            return arr;
        },
        validate: (arr) => {
            arr.push("v=o.profVlanIdx if not isValid.Key(v, getVlanList(true)) and v ~= '' then return false, 'oops! ' .. 'profVlanIdx' end");
            return arr;
        },
        set: (arr) => {
            arr.push("local index = tonumber(objHandler.queryParams." + ProfileCfg.rdbIndexQueryParam + ") or " + ProfileCfg.defaultId);
            arr.push("luardb.set(string.format('link.profile.%d.writeflag', index), '1')");
            arr.push("luardb.set(string.format('link.policy.%d.trigger_connect', index), o.profEnable)");
            return arr;
        }
    },
#endif
#ifdef V_READONLY_WWAN_PROFILE_y
    readOnly: true,
#endif
    members: [
        objVisibilityVariable("profEnable", "Enable").setRdb("link.policy.%d.enable"),
        editableTextVariable("profName", "Name", {required: false}).setRdb("link.profile.%d.name"),
        editableTextVariable("profApn", "APN", {required: false}).setRdb("link.profile.%d.apn"),
        editableUsername("profUser", "Username", {required: false}).setRdb("link.profile.%d.user"),
        editablePasswordVariable("profPasswd", "Password", {required: false}).setRdb("link.profile.%d.pass"),
        selectVariable("profAuthType", "Authentication type",
                       (o) => { return [["chap", "CHAP"], ["pap","PAP"], ["none","None"]]; }
        ).setRdb("link.profile.%d.auth_type"),
        selectVariable("profPdpType", "PDP type",
                       (o) => { return [["ipv4", "IPv4"], ["ipv6","IPv6"], ["ipv4v6", "IPv4v6"]]; }
        ).setRdb("link.profile.%d.pdp_type"),
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
        ).setRdb("link.profile.%d.vlan_index"),
#ifdef V_PER_PROFILE_MTU_y
        // Upper bound is set to 1500 because ipa does not allow for mtu larger than DEFAULT_MTU_SIZE (1500)
        editableBoundedInteger("mtuSize", "MTU size", 1, 1500,
        "Please specify a value between 1 and 1500",
        {required: false, helperText: "1-1500"}).setRdb("link.profile.%d.mtu"),
#endif
        toggleVariable("profIpHandoverEnable", "IP passthrough")
        .setRdb("link.profile.%d.ip_handover.enable"),
        selectVariable("profModemIdx", "Modem profile",
                       (o) => { let m_idx: OptionsType[] = [];
                                for (let i = 0; i < NumProfiles; ++i) {
				    let idx_str = (i + 1).toString();
                                    m_idx.push([idx_str, idx_str]);
                                }
                                return m_idx;
                              },
		       undefined, undefined, {helperText: "modem profile changes require a reboot to take effect"}
        ).setRdb("link.profile.%d.module_profile_idx"),
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
        appendButtons({"save":"CSsave", "cancel":"cancel"});
        setButtonEvent("save", "click", onClickSaveBtnProfileSetting);
#endif
        setButtonEvent("cancel", "click", () => {window.location.href = "/profile_list.html";});
    }
};
