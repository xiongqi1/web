-- Copyright (C) 2018 NetComm Wireless limited.
--
-- Various utility functions.

local util = {}

-- Creates a string representation for a given variable (called recursively for tables)
--
-- @param thing A variable to stringify
-- @param indentLevel For internal use (recursive calls). Optional.
-- @return string representation of the variable
function util.toPrettyString(thing, indentLevel)
    if type(thing) ~= "table" then
        if type(thing) == "string" then
            return "\"" .. thing .. "\""
        end
        return tostring(thing)
    end

    if indentLevel == nil then
        indentLevel = 0
    elseif indentLevel == 10 then -- limit depth (avoid circles)
        return tostring(thing)
    end


    local linePrefix = string.rep(" ", indentLevel * 4)
    local nextLinePrefix = string.rep(" ", (indentLevel + 1) * 4)

    local s = "{\n"
    for k,v in pairs(thing) do
        s = s .. nextLinePrefix .. util.toPrettyString(k, indentLevel + 1) .. " = " ..
        util.toPrettyString(v, indentLevel + 1) .. ",\n"
    end
    s = s .. linePrefix ..  "}"
    return s
end

return util
