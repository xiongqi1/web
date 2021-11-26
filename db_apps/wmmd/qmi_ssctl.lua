-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi ssctl (Subsystem control) module

local QmiSsctl = require("wmmd.Class"):new()

function QmiSsctl:setup(rdbWatch, wrdb, dConfig)
  -- load essential lua modules
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_ssctl", "LOG_DAEMON") end)
  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")

  -- setup alias for luaqmi message collection
  self.m = self.luaq.m
end

function QmiSsctl:QMI_SSCTL_RESTART_READY_IND(type, event, qm)
  return true
end

QmiSsctl.cbs={
  "QMI_SSCTL_RESTART_READY_IND",
}


function QmiSsctl:poll(type, event, a)
  self.l.log("LOG_DEBUG","qmi ssctl poll")
  return true
end

-------------------------------------------------------------------------------------------------------------------
-- Shutdown Subsystem.
--
-- @param type Watcher invoke type.
-- @param event Watcher invoke event or command.
-- @param params watcher parameters.
function QmiSsctl:shutdown_subsystem(type, event, params)
  -- FIXME: luaqmi will raise error due to undefined ssctl_shutdown_ready_ind_msg_v01
  return self.luaq.req(self.m.QMI_SSCTL_SHUTDOWN)
end

QmiSsctl.system_watcher_handlers={
  "poll",
  "shutdown_subsystem",
}

-------------------------------------------------------------------------------------------------------------------
-- module initializing function
function QmiSsctl:init()

  self.l.log("LOG_INFO", "initiate qmi_ssctl")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.system_watcher_handlers) do
    self.watcher.add("sys", v, self, v)
  end

end

return QmiSsctl