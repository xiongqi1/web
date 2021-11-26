-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi tmd module

local QmiTmd = require("wmmd.Class"):new()

function QmiTmd:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_tmd", "LOG_DAEMON") end)

  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")

  self.m = self.luaq.m
  self.e = self.luaq.e
end

function QmiTmd:QMI_TMD_MITIGATION_LEVEL_REPORT_IND(type, event, qm)

  local mitigation_device = self.ffi.string(qm.resp.mitigation_device.mitigation_dev_id)
  local mitigation_level = tonumber(qm.resp.current_mitigation_level)

  self.l.log("LOG_NOTICE", string.format("[mitigation] mitigation level report (device='%s',level=%d)",mitigation_device,mitigation_level))

  return true
end

QmiTmd.cbs={
  "QMI_TMD_MITIGATION_LEVEL_REPORT_IND",
}

function QmiTmd:poll(type, event, a)
  self.l.log("LOG_DEBUG","qmi tmd poll")

  return true
end

QmiTmd.cbs_system={
  "poll",
}

function QmiTmd:init()

  self.l.log("LOG_INFO", "initiate qmi_tmd")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- get mitigation device list
  local succ,code,resp=self.luaq.req(self.m.QMI_TMD_GET_MITIGATION_DEVICE_LIST)

  -- check result
  if not succ then
    self.l.log("LOG_ERR", "[mitigation] failed to get mitigation device list")
    return
  end

  -- check list validation
  if not self.luaq.is_c_true(resp.mitigation_device_list_valid) then
    self.l.log("LOG_ERR", "[mitigation] mitigation device list not valid")
    return
  end

  -- register notification of mitigation level for all device
  for i=0,resp.mitigation_device_list_len-1 do

    local dev = self.ffi.string(resp.mitigation_device_list[i].mitigation_dev_id.mitigation_dev_id);
    local maxlevel = tonumber(resp.mitigation_device_list[i].max_mitigation_level);

    self.l.log("LOG_INFO", string.format("[mitigation] register mitigation device (dev='%s',maxlevel=%d)",dev,maxlevel))
    local qm = self.luaq.new_msg(self.m.QMI_TMD_REGISTER_NOTIFICATION_MITIGATION_LEVEL)
    qm.req.mitigation_device.mitigation_dev_id = resp.mitigation_device_list[i].mitigation_dev_id.mitigation_dev_id

    if not self.luaq.send_msg(qm) or not self.luaq.ret_qm_resp(qm) then
      self.l.log("LOG_ERR", "[mitigation] failed to send QMI message for notification mitigation level")
      return
    end
  end

end

return QmiTmd