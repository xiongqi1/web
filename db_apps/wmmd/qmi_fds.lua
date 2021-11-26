-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi fds module

local QmiFds = require("wmmd.Class"):new()

function QmiFds:setup(rdbWatch, wrdb, dConfig)
  -- load essential lua modules
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_fds", "LOG_DAEMON") end)
  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.dconfig = dConfig
  self.ffi = require("ffi")

  -- setup alias for luaqmi message collection
  self.m = self.luaq.m
end

-------------------------------------------------------------------------------------------------------------------
-- indication when scrub apps start to scrub
--
function QmiFds:QMI_FDS_SCRUB_APPS_START_SCRUB_IND(type, event, qm)

  self.l.log("LOG_DEBUG",string.format("[scrub] apps start scrub ind received."))

  if not self.scrub_enable then
    self.l.log("LOG_DEBUG",string.format("[scrub] scrub sync is not activated"))
    return true
  end

  self.l.log("LOG_INFO","[scrub] scrub apps start to scrub")
  return true
end

QmiFds.cbs={
  "QMI_FDS_SCRUB_APPS_START_SCRUB_IND",
}


function QmiFds:poll(type, event, a)
  self.l.log("LOG_INFO","qmi fds poll")

  if not self.scrub_enable then
    self.l.log("LOG_DEBUG",string.format("[scrub] scrub sync is not activated"))
    return true
  end

  self.l.log("LOG_DEBUG",string.format("[scrub] md5sum sync trigger = %s",self.dconfig.trigger_to_sync_md5sum_for_scrub))

  -- invoke sync procedure when RDB trigger is set
  if self.dconfig.trigger_to_sync_md5sum_for_scrub then
    -- invoke sync procedure
    self.l.log("LOG_INFO","[scrub] trigger to synchronize scrub md5sum detected, start md5sum sync.")
    self.watcher.invoke("sys","update_scrub_information")

    -- clear trigger flag
    self.l.log("LOG_INFO","[scrub] clear trigger flag")
    self.dconfig:update_config("trigger_to_sync_md5sum_for_scrub",false)
  end

  --[[

  --
  -- TODO: log scrub information
  --

  local succ,err,resp=self.luaq.req(self.m.QMI_FDS_SCRUB_GET_SYS_INFO)

  if self.luaq.is_c_true(resp.fds_internal_scrub_timer_secs_valid) then
    self.l.log("LOG_INFO",string.format("[scrub] fds_internal_scrub_timer_secs = %d",resp.fds_internal_scrub_timer_secs)

  ]]--

  return true
end

-------------------------------------------------------------------------------------------------------------------
-- Recalculate md5sum of partitions for scrub and update scrub master NV state.
--
-- @param type Watcher invoke type.
-- @param event Watcher invoke event or command.
-- @param params not used.
function QmiFds:update_scrub_information(type, event, params)
  self.l.log("LOG_NOTICE","[scrub] update scrub information in NV")

  if not self.scrub_enable then
    self.l.log("LOG_DEBUG",string.format("[scrub] scrub sync is not activated"))
    return true
  end

  return self.luaq.req(self.m.QMI_FDS_HANDLE_FOTA_UPDATE)
end

QmiFds.system_watcher_handlers={
  "poll",
  "update_scrub_information",
}

-------------------------------------------------------------------------------------------------------------------
-- module initializing function
function QmiFds:init()

  self.l.log("LOG_INFO", "initiate qmi_fds")

  -- enable scrub based on QMI FDS define
  self.scrub_enable = self.m.QMI_FDS_INDICATION_REGISTER and true or false

  -- do not register if FDS is missing
  if not self.scrub_enable  then
    self.l.log("LOG_ERR", "[scrub] QMI IDL header for FDS (Flash Driver Service) missing, FDS is not used.")
    return
  end

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.system_watcher_handlers) do
    self.watcher.add("sys", v, self, v)
  end

  -- add app start indication
  self.luaq.req(self.m.QMI_FDS_INDICATION_REGISTER,{need_apps_start_scrub_indication=1})

end

return QmiFds

