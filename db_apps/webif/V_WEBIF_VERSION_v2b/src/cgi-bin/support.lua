--
-- This module provides supporting functions for applications to use
--
-- Copyright (C) 2017 NetComm Wireless Limited.
--
--
require('lfs')

-- Merges the overriding module to the base module.
-- @param base_module Base module to be merged from the overriding module
-- @param override_module Overriding module to merge to the base module
-- @return A module that merges all attributes in the overriding module
local function merge_module(base_module, override_module)
    if override_module then
        for k, v in pairs(override_module) do
            if base_module[k] then
                if type(base_module[k]) == 'table' then
                    merge_module(base_module[k], override_module[k])
                else
                    base_module[k] = v
                end
            else
                base_module[k] = v
            end
        end
    end
    return base_module
end

-- Loads the module that merges all attributes based on the precedence
-- of the overriding modules.
-- @param path The file path where the module is located
-- @param name Name of the module to load
-- @return A module that merges all attributes
function load_overriding_module(path, name)
    local full_filepath = path .. '/' .. name .. '.lua'
    if not lfs.attributes(full_filepath, 'mode') then
        return nil
    end
    local module = dofile(full_filepath)
    for filename in lfs.dir(path) do
        if filename:match(name .. '_.+.lua$') then
            module = merge_module(module, dofile(path .. '/' .. filename))
        end
    end
    return module
end
