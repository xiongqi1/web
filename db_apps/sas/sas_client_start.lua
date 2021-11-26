#!/usr/bin/env lua
--
-- start sas_client
--
-- Copyright (C) 2019 NetComm Wireless limited.
--

local SasClient = require("sas.sas"):new()

local sasClient = SasClient:new()
sasClient:setup()
sasClient:setupModules()
-- travel thru V-variables customisation to decorate this object
local v_list = require("sas.v_extension_list__")
for _,v in ipairs(v_list) do
  local v_obj = require("sas." .. v)
  if v_obj ~= nil then sasClient = v_obj:decorate(sasClient) end
end
sasClient:initiate()

