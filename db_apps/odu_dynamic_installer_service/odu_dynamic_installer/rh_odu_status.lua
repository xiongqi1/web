-- Copyright (C) 2021 Casa Systems.
-- request handler: ODU status

local m_turbo = require("turbo")
local m_odu_entry = require("odu_dynamic_installer.odu_entry")

local OduStatusHandler = class("OduStatusHandler", m_turbo.web.RequestHandler)

function OduStatusHandler:get()
    local odu_info = m_odu_entry.get_odu_info()

    local out = {
        physicalConnected = odu_info.phy_connect,
        interfaceConnected = odu_info.interface_connect
    }
    out["oduModel"] = odu_info["model"]
    out["firmwareVersion"] = odu_info["firmware_version"]
    if odu_info["config_update_status"] and type(odu_info["config_update_status"]) == "table" then
        for config_type, ids in pairs(odu_info["config_update_status"]) do
            out[config_type.."ConfigIdList"] = table.concat(ids, ",")
        end
    end
    self:write(out)
end

return OduStatusHandler
