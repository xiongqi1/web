-- Copyright (C) 2021 Casa Systems.
-- authorisation module

local m_luardb = require("luardb")
local m_prefix = "odu_dynamic_installer."
local m_password_hash_rdb = m_prefix.."password_hash"

-- hash: sha512
-- @return hexadecimal string
local function get_password_hash(b64_password)
    if not b64_password or type(b64_password) ~= "string"
            or not b64_password:match("^[%w+/=]+$") then
        return false
    end

    local cmd = "echo -n \""..b64_password.."\" | openssl sha512 | sed -nE 's/^\\(stdin\\)= ([a-zA-Z0-9]+)$/\\1/p' | tr -d \"\\n\""
    local stdout_file = io.popen(cmd, "r")
    local password_hash = stdout_file:read("*all")
    stdout_file:close()

    return password_hash
end

local function check_password(b64_password)
    local in_password_hash = get_password_hash(b64_password)

    if not in_password_hash then
        return false
    end

    return m_luardb.get(m_password_hash_rdb) == in_password_hash
end

local function set_password(b64_password)
    if type(b64_password) ~= "string" or #b64_password == 0 or #b64_password > 256 then
        return false
    end

    local in_password_hash = get_password_hash(b64_password)

    if not in_password_hash then
        return false
    end

    m_luardb.set(m_password_hash_rdb, in_password_hash)

    return true
end

return {
    check_password = check_password,
    set_password = set_password
}
