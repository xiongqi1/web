-- Copyright (C) 2021 Casa Systems.
-- configuration

local m_luardb = require("luardb")
local m_prefix = "odu_dynamic_installer."

local config = {
    upload_dir = m_luardb.get(m_prefix.."upload_dir"),
    port = m_luardb.get(m_prefix.."port")
}

return config
