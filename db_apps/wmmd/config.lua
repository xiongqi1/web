-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

local string = require("string")

-- global configuration
local _m = {

    instance = 0,
    rdb_g_prefix = "wwan.0.",

    ipup_script = "/etc/ppp/ip-up",
    ipdn_script = "/etc/ppp/ip-down",

    modem_poll_interval = 10000, -- interval to poll modem
    modem_poll_quick_interval = 1000, -- interval to poll modem

    -- this timeout is originally from "apps_proc/mbim/qbi/svc/src/qbi_svc_bc_nas.c""
    -- #define QBI_SVC_BC_NAS_VISIBLE_PROVIDERS_TIMEOUT_MS (600 * 1000)
    modem_bplmn_scan_timeout =  600 * 1000,

    -- pci scan timeout
    -- default as per Qualcomm specification
    -- #define QBI_SVC_BC_NAS_VISIBLE_PROVIDERS_TIMEOUT_MS (600 * 1000)
    modem_pci_scan_timeout_msec =  600 * 1000,

    -- QMI async timeout for poll activities
    modem_generic_poll_timeout = 10 * 1000,
    -- QMI async timeout for quick poll activities
    modem_generic_poll_quick_timeout = 3 * 1000,

    ril_rdb_command = "ril.command",
    ril_rdb_result = "ril.result",

    -- POE Ethernet port
    poe_ether_port = 0,

    -- SMS incoming message folder
    incoming_sms_dir = "/usr/local/cdcs/conf/sms/incoming",
}



--------------------------------------------------------------------------------
-- Extends the standard string library with a split method.
--
-- @param sep separator. If no separator is specified, colon(":") is used.
-- @return separated strings.
function string:split(sep)
        local sep2, fields = sep or ":", {}
        local pattern = string.format("([^%s]+)", sep2)
        self:gsub(pattern, function(c) fields[#fields+1] = c end)
        return fields
end

return _m


