#!/usr/bin/lua

require("luardb")
require("stringutil")


-- return ip address type("ipv4"|"ipv6"|nil)
local ip_type = function(addr)
    if not addr or addr == "" then
        return nil
    end
    local remainder, colons = string.gsub(addr, ":", "")
    if colons > 1 then
        return "ipv6"
    end
    if remainder:match("^[%d%.]+$") then
        return "ipv4"
    end
    return nil
end

-- Normalize IPv4
local normalize_ipv4 = function(addr)
    local a,b,c,d = addr:match("^(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)$")
    if not a then
        return nil
    end
    a,b,c,d = tonumber(a), tonumber(b), tonumber(c), tonumber(d)
    if a < 0 or a > 255 or b < 0 or b > 255 or c < 0 or
        c > 255 or d < 0 or d > 255 then
        return nil
    end
    return string.format("%d.%d.%d.%d",a,b,c,d)
end

-- Normalize IPv6
local normalize_ipv6 = function(addr)
    check = addr
    -- check ipv6 format and normalize
    if check:sub(1,1) == ":" then
        check = "0" .. check
    end
    if check:sub(-1,-1) == ":" then
        check = check .. "0"
    end
    if check:find("::") then
        -- expand double colon
        local _, count = string.gsub(check, ":", "")
        local ins = ":" .. string.rep("0:", 8 - count)
        check = string.gsub(check, "::", ins, 1)  -- replace only 1 occurence!
    end
    local a,b,c,d,e,f,g,h = check:match("^(%x%x?%x?%x?):(%x%x?%x?%x?):(%x%x?%x?%x?):(%x%x?%x?%x?):(%x%x?%x?%x?):(%x%x?%x?%x?):(%x%x?%x?%x?):(%x%x?%x?%x?)$")
    if not a then
        -- Check IPv4-mapped IPv6 address
        local ipv4 = check:match("^0:0:0:0:0:0:ffff:(%S+)$")
        if ip_type(ipv4) == "ipv4" then
            return normalize_ipv4(ipv4)
        end

        -- not a valid IPv6 address
        return nil
    end
    local zeros = "0000"
    return string.lower(string.format("%s:%s:%s:%s:%s:%s:%s:%s",
        zeros:sub(1, 4 - #a) .. a,
        zeros:sub(1, 4 - #b) .. b,
        zeros:sub(1, 4 - #c) .. c,
        zeros:sub(1, 4 - #d) .. d,
        zeros:sub(1, 4 - #e) .. e,
        zeros:sub(1, 4 - #f) .. f,
        zeros:sub(1, 4 - #g) .. g,
        zeros:sub(1, 4 - #h) .. h))
end

local normalize_ip = function(addr)
    local addr = (addr or ""):gsub("^%s*(.-)%s*$", "%1")
    local ipType = ip_type(addr)
    if ipType == "ipv4" then
        return normalize_ipv4(addr)
    elseif ipType == "ipv6" then
        return normalize_ipv6(addr)
    end
    return nil
end

-- The return value is reversed to check whether this script exist or not.
rTrue=1
rFalse=0

-- padded with comma
checkIp="," .. (normalize_ip(arg[1]) or "") .. ","
if checkIp == ",," then
    os.exit(rFalse)
end

parsedTbl = string.explode(luardb.get("tr069.request.whitelist") or "", ",")
norTbl = {}

for _, addr in pairs(parsedTbl) do
    local norIp = normalize_ip(addr)
    if norIp then
        table.insert(norTbl, norIp)
    end
end

whiteList = "," .. table.concat(norTbl, ",") .. ","

-- whiteList does not have entries, return true.
if whiteList:gsub(",", "") == "" then
    os.exit(rTrue)
end

if string.match(whiteList,checkIp) then
    os.exit(rTrue)
end

os.exit(rFalse)

