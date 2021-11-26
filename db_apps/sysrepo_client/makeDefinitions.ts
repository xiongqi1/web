/*
 * In an effort to reduce duplication, this script generates both Yang models and C++ files
 * from a single data definition
 */
import * as  fs from "fs";

var yang_types: any = {
    string: "string",
    ipv4_address: "inet:ipv4-address"
}

class Leaf {
        name: string;
        type: string;
        desc: string;
        rdbPrefix: string;
        constructor(name: string, type: string, desc: string, rdbPrefix: string) {
            this.name = name;
            this.type = type;
            this.desc = desc;
            this.rdbPrefix = rdbPrefix;
        }
        getRdbName() {
            if (this.rdbPrefix.endsWith(".")) {
                return this.rdbPrefix + this.name;
            }
            return this.rdbPrefix;
        }
}

function defLeaf(name: string, type: string, desc: string, rdbPrefix: string): Leaf {
    return new Leaf(name, type, desc, rdbPrefix );
}

interface container {
    name: string, config: boolean, leaves: Leaf[]
};

function defContainer(name: string, config: boolean, leaves: Leaf[]): container {
    return {
        name: name, config: config, leaves: leaves
    };
}

var containers: container[] = [
    defContainer("LAN-profile-config", true,
       [
            defLeaf("address", "ipv4_address", "" , "link.profile.0."),
            defLeaf("defaultgw", "string", "Default Gateway" , "link.profile.0."),
            defLeaf("defaultroutemetric", "string", "Default route metric" , "link.profile.0."),
            defLeaf("hostname", "string", "Router host  name" , "link.profile.0."),
            defLeaf("netmask", "string", "Default Gateway" , "link.profile.0.")
        ]
    ),
    defContainer("LAN-profile-data", false,
        [
            defLeaf("dev", "string", "Device" , "link.profile.0.")
        ]
    ),
    defContainer("WAN-profile-config", true,
        [
            defLeaf("apn", "string", "APN" , "link.profile.1."),
            defLeaf("auth_type", "string", "Authentication type" , "link.profile.1."),
            defLeaf("autoapn", "string", "Auto APN" , "link.profile.1."),
            defLeaf("defaultroute", "string", "is default route" , "link.profile.1."),
            defLeaf("defaultroutemetric", "string", "Default route metric" , "link.profile.1."),
            defLeaf("dns1", "string", "DNS 1" , "link.profile.1."),
            defLeaf("dns2", "string", "DNS 2" , "link.profile.1."),
            defLeaf("ipv6_dns1", "string", "IPv6 DNS 1" , "link.profile.1."),
            defLeaf("ipv6_dns2", "string", "IPv6 DNS 2" , "link.profile.1."),
            defLeaf("enable", "string", "Profile enable", "link.profile.1."),
            defLeaf("gw", "string", "Gateway" , "link.profile.1."),
            defLeaf("ipv6_gw", "string", "IPv6 Gateway" , "link.profile.1."),
            defLeaf("iplocal", "string", "Local IP" , "link.profile.1."),
            defLeaf("ipv6_ipaddr", "string", "IPv6 Local IP" , "link.profile.1."),
            defLeaf("mask", "string", "Network mask" , "link.profile.1."),
            defLeaf("module_profile_idx", "string", "Module profile idx" , "link.profile.1."),
            defLeaf("mtu", "string", "Max transfer unit" , "link.profile.1."),
            defLeaf("name", "string", "Profile name" , "link.profile.1."),
            defLeaf("pdp_type", "string", "PDP type" , "link.profile.1."),
            defLeaf("readonly", "string", "Read only" , "link.profile.1."),
            defLeaf("reconnect_delay", "string", "Reconnect delay" , "link.profile.1."),
            defLeaf("reconnect_retries", "string", "Reconnect retries" , "link.profile.1."),
            defLeaf("routes", "string", "routes" , "link.profile.1."),
            defLeaf("snat", "string", "NAT" , "link.profile.1.")
        ]
    ),
    defContainer("WAN-profile-data", false,
        [
            defLeaf("connect_progress", "string", "Connect progress" , "link.profile.1."),
            defLeaf("pass", "string", "password" , "link.profile.1."),
            defLeaf("pdp_result", "string", "PDP result" , "link.profile.1."),
            defLeaf("status_ipv4", "string", "IPv4 status" , "link.profile.1."),
            defLeaf("status_ipv6", "string", "IPv6 status" , "link.profile.1."),
            defLeaf("status", "string", "status" , "link.profile.1."),
            defLeaf("dev", "string", "Device" , "link.profile.1."),
            defLeaf("interface", "string", "Interface" , "link.profile.1."),
            defLeaf("usage_current", "string", "Current usage" , "link.profile.1."),
            defLeaf("usage_current_startTimeInsysUpTime", "string", "usage_current_startTimeInsysUpTime" , "link.profile.1."),
            defLeaf("usage_history", "string", "Usage history" , "link.profile.1."),
            defLeaf("usage_total", "string", "Usage total" , "link.profile.1.")
        ]
    ),
    defContainer("ip-handover-config", true,
        [
            defLeaf("enable", "string", "enable" , "link.profile.1."),
            defLeaf("fake_wwan_address", "string", "fake_wwan_address" , "service.ip_handover."),
            defLeaf("fake_wwan_mask", "string", "fake_wwan_mask" , "service.ip_handover."),
            defLeaf("profile_index", "string", "Profile index" , "service.ip_handover."),
            defLeaf("vlan_index", "string", "VLAN index" , "service.ip_handover.")
        ]
    ),
    defContainer("ip-handover-data", false,
        [
            defLeaf("last_profile_index", "string", "Last profile index" , "link.profile.1."),
            defLeaf("last_wwan_ip", "string", "Last wwan ip" , "service.ip_handover.")
        ]
    ),
    defContainer("router-policy-config", true,
        [
            defLeaf("enable", "string", "enable" , "service.router.policy."),
            defLeaf("mark", "string", "mark" , "service.router.policy."),
            defLeaf("method", "string", "method" , "service.router.policy.")
        ]
    ),
    defContainer("router-policy-rule-config", true,
        [
            defLeaf("desc", "string", "desc" , "service.router.policy.rule.0."),
            defLeaf("enable", "string", "enable" , "service.router.policy.rule.0."),
            defLeaf("interface-in-name", "string", "interface.in.name" , "service.router.policy.rule.0."),
            defLeaf("priority", "string", "priority" , "service.router.policy.rule.0."),
            defLeaf("table", "string", "table" , "service.router.policy.rule.0.")
        ]
    ),
    defContainer("router-policy-table-config", true,
        [
            defLeaf("desc", "string", "desc" , "service.router.policy.table.0."),
            defLeaf("enable", "string", "enable" , "service.router.policy.table.0."),
            defLeaf("out_interface", "string", "out_interface" , "service.router.policy.table.0.")
        ]
    ),
/*    defContainer("vlan-data", false,
        [
            defLeaf("ip_handover_mode", "string", "ip_handover_mode" , "vlan.data."),
            defLeaf("num", "string", "num" , "vlan."),
            defLeaf("enable", "string", "enable" , "vlan.")
        ]
    ),*/
    defContainer("vlan-0-config", true,
        [
            defLeaf("address", "string", "address" , "vlan.data."),
            defLeaf("admin_access_allowed", "string", "admin_access_allowed" , "vlan.data."),
            defLeaf("enable", "string", "enable" , "vlan.data."),
            defLeaf("id", "string", "id" , "vlan.data."),
            defLeaf("interface", "string", "interface" , "vlan.data."),
            defLeaf("mtu", "string", "mtu" , "vlan.data."),
            defLeaf("name", "string", "name" , "vlan.data."),
            defLeaf("netmask", "string", "netmask" , "vlan.data."),
        ]
    ),
    defContainer("vlan-0-dhcp-data", false,
        [
            defLeaf("lease", "string", "lease" , "vlan.data.dhcp."),
            defLeaf("range", "string", "range" , "vlan.data.dhcp."),
            defLeaf("enable", "string", "enable" , "vlan.data.dhcp.")
        ]
    ),
    defContainer("wwan-0-data", false,
        [
            defLeaf("firmware_version", "string", "firmware_version" , "wwan.0."),
            defLeaf("hardware_version", "string", "hardware_version" , "wwan.0."),
            defLeaf("imei", "string", "imei" , "wwan.0."),
            defLeaf("msin", "string", "imsi.msin" , "wwan.0.imsi."),
            defLeaf("rssi", "string", "RSSI" , "wwan.0.signal."),
            defLeaf("BytesReceived", "string", "BytesReceived" , "wwan.0.rrc_session.0.pdcp_bytes_received"),
            defLeaf("BytesSent", "string", "BytesSent" , "wwan.0.rrc_session.0.pdcp_bytes_sent"),
            defLeaf("ICCID", "string", "ICCID" , "wwan.0.system_network_status.simICCID"),
            defLeaf("IMSI", "string", "IMSI" , "wwan.0.imsi.msin"),
            defLeaf("CQI", "string", "CQI" , "wwan.0.rrc_session.0.avg_cqi"),
            defLeaf("CellID", "string", "CellID" , "wwan.0.rrc_session.0.cell_id"),
            defLeaf("EARFCN", "string", "EARFCN" , "wwan.0.system_network_status.eci_pci_earfcn"),
            defLeaf("ECGI", "string", "ECGI" , "wwan.0.system_network_status.ECGI"),
            defLeaf("PCID", "string", "PCI" , "wwan.0.system_network_status.PCID"),
            defLeaf("rsrp", "string", "RSRP" , "wwan.0.signal.0."),
            defLeaf("rsrq", "string", "RSRQ" , "wwan.0.signal."),
            defLeaf("SINR", "string", "SINR" , "wwan.0.signal.rssinr")
        ]
    ),
    defContainer("system-product-data", false,
        [
            defLeaf("product", "string", "product" , "system."),
            defLeaf("class", "string", "class" , "system.product."),
            defLeaf("hwver", "string", "hwver" , "system.product."),
            defLeaf("mac", "string", "mac" , "system.product."),
            defLeaf("model", "string", "model" , "system.product."),
            defLeaf("skin", "string", "skin" , "system.product."),
            defLeaf("sn", "string", "sn" , "system.product."),
            defLeaf("title", "string", "title" , "system.product.")
        ]
    ),
    defContainer("Device-DeviceInfo", false,
        [
            defLeaf("HardwareVersion", "string", "HardwareVersion" , "system.product.hwver"),
            defLeaf("ModelName", "string", "ModelName" , "system.product.model"),
            defLeaf("SerialNumber", "string", "SerialNumber" , "system.product.sn"),
            defLeaf("SoftwareVersion", "string", "SoftwareVersion" , "sw.version"),
            // defLeaf("UpTime", "string", "UpTime" , "Device.DeviceInfo.")
        ]
    )
];

function writeYangFile(name: string) {
    function genLeaves(leaves: Leaf[])
    {
        var res = "";
        var spc = "    "
        for (let leaf of leaves) {
            res += spc + "leaf " + leaf.name + " {\n" +
                    spc + spc + "type " + yang_types[leaf.type] + ";\n" +
                    spc + spc + 'description "' + leaf.desc + '";\n' +
                    spc + "}\n\n"
        }
        return res;
    }

    function genContainers(containers:container[])
    {
        var res = "";
        var spc = "  "
        for (var container of containers) {
            res += spc + "container " + container.name + " {\n" +
                    spc + spc + "config " + container.config.toString() + ";\n" +
                    genLeaves(container.leaves) +
                    spc + "}\n\n"
        }
        return res;
    }

    let yangHdr: string = 'module ' + name + ' {\n\n' +
    '  namespace "http://www.netcommwireless.com/ns/yang/ifwa";\n' +
    '  prefix ifwa;\n\n' +
    '  import ietf-yang-types {\n' +
    '    prefix yang;\n' +
    '  }\n\n' +
    '  import ietf-inet-types {\n' +
    '    prefix inet;\n' +
    '  }\n\n' +
    '  organization\n' +
    '    "NetComm Wireless Limited";\n\n' +
    '  contact\n' +
    '    "NetComm Wireless Limited\n' +
    '    18-20 Orion Rd,\n' +
    '    Lane Cove NSW 2066,\n' +
    '    Australia\n' +
    '      Phone: +61 2 9424 2070";\n\n' +
    '  description\n' +
    '    "This autogenerated module contains a collection of YANG definitions for\n' +
    '    managing NetComm Wireless IFWA.\n' +
    '    Copyright (c) 2018 NetComm Wireless Limited.";\n\n' +
    '  revision 2018-01-01 {\n' +
    '    description\n' +
    '      "Initial revision.";\n' +
    '  }\n\n';

    fs.writeFileSync(name + ".yang", yangHdr + genContainers(containers) + "}\n");
}

writeYangFile("netcommwireless-ifwa");


/*
Generate the auto generated code which is of the form
bool Sysrepo::provideData()
{
    SysrepoModule *pModule = new SysrepoModule();
    pModule->addConfigItem(new ConfigItem("a","b"));
    if (!subscribeModule("netcommwireless-ifwa", pModule)) {
        return false;
    }

    const char * xpath = "/netcommwireless-ifwa:WAN-profile-data";
    SysrepoDataContainer *pCntnr = new SysrepoDataContainer(xpath);
    pCntnr->addDataProvider(new DataProvider("connect_progress","link.profile.0.address"));
    if (subscribeData(xpath, pCntnr)) {
        return false;
    }
    return true;
}
*/

function writeCPPFile(name: string) {
    function genDataLeaves(leaves: Leaf[])
    {
        var res = "";
        var spc = "  ";
        for (var key in leaves) {
            var leaf = leaves[key];
            res += spc + 'pCntnr->addDataProvider(new DataProvider("' + leaf.name + '","' + leaf.getRdbName() + '"));\n'
        }
        return res;
    }

    function genCfgLeaves(xpath: string, leaves: Leaf[])
    {
        var res = "";
        var spc = "  ";
        for (var key in leaves) {
            var leaf = leaves[key];
            res += spc + 'pModule->addConfigItem(new ConfigItem("' + xpath + "/" + leaf.name + '","' + leaf.getRdbName() + '"));\n'
        }
        return res;
    }

    function genContainers(containers: container[])
    {
        var res = "";
        var spc = "  ";
        var isModuleSubscribed = false;
        for (var container of containers) {
            var xpath = "/" + name + ':' + container.name;
            if (container.config) {
                if (!isModuleSubscribed) {
                    res +=  '  SysrepoModule *pModule = new SysrepoModule("' + name + '");\n';
                    isModuleSubscribed = true;
                }
                res += genCfgLeaves(container.name, container.leaves);
            }
            else {
                res += spc + 'xpath = "' + xpath + '";\n' +
                spc + 'pCntnr = new SysrepoDataContainer(xpath);\n' +
                genDataLeaves(container.leaves) +
                spc + 'if (!subscribeData(xpath, pCntnr)) {\n' +
                spc + '    return false;\n' +
                spc + '}\n'
            }
        }
        if (isModuleSubscribed) {
            res +=  '  if (!subscribeModule(pModule)) {\n' +
                    '    return false;\n' +
                    '  }\n';
        }

        return res;
    }

    fs.writeFileSync("yangdefs.cpp",
        '#include "Sysrepo.h"\n' +
        "bool Sysrepo::provideData() {\n" +
        "  const char * xpath;\n" +
        "  SysrepoDataContainer *pCntnr;\n" +
        genContainers(containers) + "  return true;\n}\n");
}
writeCPPFile("netcommwireless-ifwa");
