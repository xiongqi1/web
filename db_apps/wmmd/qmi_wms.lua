-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi wms module

local QmiWms = require("wmmd.Class"):new()

function QmiWms:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_wms", "LOG_DAEMON") end)

  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")
  self.dconfig = dConfig

  self.m = self.luaq.m
  self.e = self.luaq.e
end

QmiWms.message_type_names={
  [0x00] = "voicemail",
  [0x01] = "fax",
  [0x02] = "email",
  [0x03] = "other",
  [0x04] = "videomail",
}

function QmiWms:build_mwi_arg(resp)
  local ia = {}

  local info
  local message_type
  for i=0,resp.message_waiting_info_len-1 do

    info = resp.message_waiting_info[i]

    message_type = self.message_type_names[tonumber(info.message_type)]

    ia[message_type]={}
    ia[message_type].active = self.luaq.is_c_true(info.active_ind)
    ia[message_type].count = tonumber(info.message_count)
  end

  return ia
end

function QmiWms:QMI_WMS_MESSAGE_WAITING_IND(type, event, qm)
  local ia = self:build_mwi_arg(qm.resp)
  return self.watcher.invoke("sys","modem_on_mwi",ia)
end

function QmiWms:QMI_WMS_EVENT_REPORT_IND(type, event, qm)
  return true
end

QmiWms.cbs={
  "QMI_WMS_MESSAGE_WAITING_IND",
  "QMI_WMS_EVENT_REPORT_IND",
}

function QmiWms:poll_mwi(type,event,a)
  local succ,err,res = self.luaq.req(self.m.QMI_WMS_GET_MESSAGE_WAITING)

  local ia = self:build_mwi_arg(res)
  return self.watcher.invoke("sys","modem_on_mwi",ia)
end

function QmiWms:poll(type, event, a)
  self.l.log("LOG_DEBUG","qmi wms poll")

  --[[

  Although AT&T is believed to use an unknown type of MWI SMS that Qualcomm stack fails to interpret to MWI indiciation,
  by USA field testing, it is confirmed that polling method still works.

  as every 10 second poll is good enough for the MWI, we are polling until Qualcomm stack is ready or we figure out AT&T MWI SMS text message type.

 ]]--

  if self.dconfig.enable_poll_mwi then
    self.watcher.invoke("sys","poll_mwi")
  end

  return true
end

QmiWms.cbs_system={
  "poll_mwi",
  "poll",
}

function QmiWms:init()

  self.l.log("LOG_INFO", "initiate qmi_wms")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  local succ,err,res = self.luaq.req(self.m.QMI_WMS_SET_EVENT_REPORT,{
    report_mwi_message=1, -- MWI message indicator
  -- report_mt_message=1, -- new MT message
  -- report_call_control_info=1, -- MO SMS call control
  })

end

return QmiWms