-- Copyright (C) 2021 Casa Systems.
-- request handler: change password

local m_turbo = require("turbo")
local m_auth = require("odu_dynamic_installer.authorization")

local ChangePasswordHandler = class("ChangePasswordHandler", m_turbo.web.RequestHandler)

function ChangePasswordHandler:post()
    local password = self:get_argument("password")

    if m_auth.set_password(password) then
        self:write({status = "success", errorMsg = ""})
    else
        self:write({status = "error", errorMsg = "Invalid password"})
    end
end

return ChangePasswordHandler
