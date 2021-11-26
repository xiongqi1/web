--[[
    Manufacturing status/version information WEBUI module

    Copyright (C) 2020 Casa Systems Inc.
--]]

local luardb = require("luardb")
local turbo = require("turbo")

local function get_manufacture_version(handler)
    local response = {}
    local v_board = luardb.get("system.product.board") or ""
    local v_ioboard = luardb.get("system.product.ioboard") or ""
    response["board_hw_info"] = v_board..","..v_ioboard
    response["class"] = luardb.get("system.product.class") or ""
    response["fw_ver"] = luardb.get("wwan.0.firmware_version") or ""
    response["hw_ver"] = luardb.get("system.product.hwver") or ""
    response["imei"] = luardb.get("wwan.0.imei") or ""
    response["imsi"] = luardb.get("wwan.0.imsi.msin") or ""
    response["mac"] = luardb.get("system.product.mac") or ""
    response["model"] = luardb.get("system.product") or ""
    response["serial_number"] = luardb.get("system.product.sn") or ""
    response["skin"] = luardb.get("system.product.skin") or ""
    response["sw_ver"] = luardb.get("sw.version") or ""
    response["title"] = luardb.get("system.product.title") or ""
    handler:write(response)
end

local function get_manufacture_status(handler)
    local response = {}
    -- Report 1st APN only for now
    response["apn"] = luardb.get("link.profile.1.apn") or ""
    response["cell_id"] = luardb.get("wwan.0.system_network_status.CellID") or ""
    response["earfcn"] = luardb.get("wwan.0.system_network_status.channel") or ""
    response["band"] = luardb.get("wwan.0.system_network_status.current_band") or ""
    response["network_registration_status"] = luardb.get("wwan.0.system_network_status.reg_stat") or ""
    response["rsrp"] = luardb.get("wwan.0.signal.0.rsrp") or ""
    response["rsrq"] = luardb.get("wwan.0.signal.rsrq") or ""
    response["sim_iccid"] = luardb.get("wwan.0.system_network_status.simICCID") or ""

    -- [btmgmt info] command returns null if stdin is not given
    local stdout = io.popen("btmgmt info < /dev/random", "r")
    local result = stdout:read("*a")
    local btMac = result:match("addr%s([A-Fa-f0-9:]+).*")
    local btSname = result:match("short%sname%s+(%S+)")
    stdout:close()
    response["bt_mac"] = btMac or ""
    response["bt_sname"] = btSname or ""
    nr5g_up = luardb.get("wwan.0.radio_stack.nr5g.up")
    if nr5g_up then
        response["nr5g_up"] = nr5g_up
        response["nr5g_arfcn"] = luardb.get("wwan.0.radio_stack.nr5g.arfcn") or ""
        response["nr5g_rsrp"] = luardb.get("wwan.0.radio_stack.nr5g.rsrp") or ""
        response["nr5g_rsrq"] = luardb.get("wwan.0.radio_stack.nr5g.rsrq") or ""
    end

    -- Mr Swivel Home Position
    local hindex = luardb.get("swivelling_scan.conf.home_position_index")
    if hindex and tonumber(hindex) then
        response["home_position"] = luardb.get("swivelling_scan.conf.position."..hindex..".coordinate") or ""
        local status = turbo.escape.json_decode(luardb.get("swivelling_scan.status"))
        response["swivelling_status"] = status.status
        response["current_position_index"] = status.currentPositionIndex
    end

    handler:write(response)
end

-- Create manufacture handler to get the version or status information
local ManufacturerHandler = class("ManufacturerHandler", turbo.web.RequestHandler)

function ManufacturerHandler:get(url)
    turbo.log.debug("Entering ManufacturerHandler handler : URL -- "..url)
    if url == "version" then
        return get_manufacture_version(self)
    elseif url == "status" then
        return get_manufacture_status(self)
    else
        error(turbo.web.HTTPError(404, "Not found"))
    end
end

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(handlers)
    table.insert(handlers, {"^/manufacture/(.*)$", ManufacturerHandler})
end

return module
