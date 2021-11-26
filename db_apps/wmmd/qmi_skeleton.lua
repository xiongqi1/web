-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi xxx module

local modname = ...

-- init syslog
local l = require("luasyslog")
pcall(function() l.open("qmi_wms", "LOG_DAEMON") end)

local luaq = require("luaqmi")
local bit = require("bit")
local watcher = require("wmmd.watcher")
local smachine = require("wmmd.smachine")
local config = require("wmmd.config")
local wrdb = require("wmmd.wmmd_rdb")
local ffi = require("ffi")


local m = luaq.m
local e = luaq.e


local cbs={
  QMI_XXX=function(type, event, qm)
    return true
  end,
}


local cbs_system={

    poll=function(type, event, a)
      l.log("LOG_INFO","qmi xxx poll")

      return true
    end,
}

local function init()

  l.log("LOG_INFO", "initiate qmi_xxx")

  -- add watcher for qmi
  for k,v in pairs(cbs) do
    watcher.add("qmi",k,v)
  end

  -- add watcher for system
  for k,v in pairs(cbs_system) do
    watcher.add("sys",k,v)
  end

  local succ,err,res = luaq.req(m.QMI_XXX_REG_EVENTS,{
  })

end

local _m={
  init=init,
}

return _m
