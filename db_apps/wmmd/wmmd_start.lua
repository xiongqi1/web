#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2017 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
--
-- start wmmd

local Wmmd = require("wmmd.wmmd")

local wmmd = Wmmd:new()
wmmd:setup()
wmmd:setupModules()

-- travel thru V-variables customisation to decorate this object
local v_list = require("wmmd.v_extension_list__")
for _,v in ipairs(v_list) do
  local v_obj = require("wmmd." .. v)
  if v_obj ~= nil then
    wmmd = v_obj:decorate(wmmd)
  end
end

wmmd:initiate()
