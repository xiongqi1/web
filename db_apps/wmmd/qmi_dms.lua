-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- DMS qmi module

local QmiDms = require("wmmd.Class"):new()

function QmiDms:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_dms", "LOG_DAEMON") end)

  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.m = self.luaq.m
  self.ffi = require("ffi")
  self.wrdb = wrdb

end

QmiDms.operating_mode_names={
  [0x00] = "online",
  [0x01] = "low power",
  [0x02] = "factory test mode",
  [0x03] = "offline",
  [0x04] = "resetting",
  [0x05] = "shutting down",
  [0x06] = "persistent low power",
  [0x07] = "mode only low power",
  [0x08] = "net test gw",
}

QmiDms.usim_state_names={
  [0x00] = "initialization completed",
  [0x01] = "initialization failed",
  [0x02] = "not present",
  [-1] = "state unavailable",
}

function QmiDms:build_operating_mode(operating_mode)

  local ia={}

  ia.operating_mode=self.operating_mode_names[tonumber(operating_mode)]

  return ia
end

QmiDms.sim_status_names={
  [0x00] = "not initialized",
  [0x01] = "enabled not verified",
  [0x02] = "enabled verified",
  [0x03] = "disabled",
  [0x04] = "blocked",
  [0x05] = "permanently blocked",
  [0x06] = "unblocked",
  [0x07] = "changed",
}


--[[ QMI commands ]]--

function QmiDms:QMI_DMS_EVENT_REPORT_IND(type, event, qm)

  if self.luaq.is_c_true(qm.resp.operating_mode_valid) then
    local ia=self:build_operating_mode(qm.resp.operating_mode)
    self.watcher.invoke("sys","modem_on_operating_mode",ia)
  end

  return true
end

QmiDms.cbs={
  "QMI_DMS_EVENT_REPORT_IND"
}

function QmiDms:poll_modem_info(type, event)
  local succ,err,resp

  local ia={}

  succ,err,resp=self.luaq.req(self.m.QMI_DMS_GET_DEVICE_MFR)
  if succ then
    ia.manufacture=self.ffi.string(resp.device_manufacturer)
  else
    self.l.log("LOG_ERR","modem manufacturer information not available")
  end

-- This was never used in Titan and may be required elsewhere in Pentecost
-- FIXME
-- It always returns 0 rather than string as defined in QMI specification
-- so as a workaround, use fixed RDB variable value now and debug
-- modem source code later.
--  succ,err,resp=self.luaq.req(self.m.QMI_DMS_GET_DEVICE_MODEL_ID)
--  if succ then
--    ia.model=self.ffi.string(resp.device_model_id)
--  else
--    self.l.log("LOG_ERR","modem model information not available")
--  end

  succ,err,resp=self.luaq.req(self.m.QMI_DMS_GET_SW_VERSION)
  if succ then
#ifdef V_MODULE_MODEL_NAME_CS510M
    if self.luaq.is_c_true(resp.secondary_sw_version_valid) then
        ia.firmware_version = self.ffi.string(resp.secondary_sw_version)
    else
        ia.firmware_version = self.ffi.string(resp.sw_version)
    end
#else
    ia.firmware_version = self.ffi.string(resp.sw_version)
#endif
    self.l.log("LOG_NOTICE",string.format("modem firmware version: %s", ia.firmware_version))
  else
    self.l.log("LOG_ERR","modem firmware version not available")
  end

  succ,err,resp=self.luaq.req(self.m.QMI_DMS_GET_DEVICE_HARDWARE_REV)
  if succ then
    ia.hardware_version =self.ffi.string(resp.hardware_rev)
  else
    self.l.log("LOG_ERR","modem hardware version not available")
  end

  -- When V_HARDCODED_IMEISV is defined, hardcoded imeisv is written
  -- into /lib/firmware/svn file which has higher priority than
  -- runtime imeisv number.
  local fh = io.open("/lib/firmware/svn")
  local svn = nil
  if fh then
      svn = fh:read("*line")
      fh:close()
      ia.imeisv=tonumber(svn,16) or 1
  end

  succ,err,resp=self.luaq.req(self.m.QMI_DMS_GET_DEVICE_SERIAL_NUMBERS)
  if succ then
    if self.luaq.is_c_true(resp.imei_valid) then
      ia.imei=self.ffi.string(resp.imei)
    end

    if self.luaq.is_c_true(resp.imeisv_svn_valid) and svn == nil then
      ia.imeisv=self.ffi.string(resp.imeisv_svn)
    end

  else
    self.l.log("LOG_ERR","IMEI not available")
  end

  if succ and self.luaq.is_c_true(resp.meid_valid) then
    ia.meid=self.ffi.string(resp.meid)
  else
    self.l.log("LOG_ERR","MEID not available")
  end

  if succ and self.luaq.is_c_true(resp.esn_valid) then
    ia.esn=self.ffi.string(resp.esn)
  else
    self.l.log("LOG_ERR","ESN not available")
  end

  return self.watcher.invoke("sys","modem_on_modem_info",ia)
end

function QmiDms:poll(type, event)
  self.l.log("LOG_DEBUG","qmi dms poll")

  return true
end

function QmiDms:stop(type, event)
  self.l.log("LOG_INFO","qmidms stop")
end

function QmiDms:poll_operating_mode(type, event)
  local succ,qerr,resp=self.luaq.req(self.m.QMI_DMS_GET_OPERATING_MODE)

  local ia=self:build_operating_mode(resp.operating_mode)
  return self.watcher.invoke("sys","modem_on_operating_mode",ia)
end

function QmiDms:online(type, event)
  self.l.log("LOG_INFO","qmidms online")

  local succ,err,res=self.luaq.req(self.m.QMI_DMS_SET_OPERATING_MODE,{operating_mode="DMS_OP_MODE_ONLINE_V01"})

  return succ
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Perform lowpower and online in sequence
--
-- @param type invoke type
-- @param event invoke event name
-- @return true when it succeeds. Otherwise, false.
--
function QmiDms:reonline(type, event)
  self.l.log("LOG_INFO","qmidms restart")

  if not self:lowpower(type, event) then
    return false
  end

  if not self:online(type, event) then
    return false
  end

  return true
end

-- Change modem operation mode to "lowpower"
function QmiDms:lowpower(type, event, params)
  self.l.log("LOG_INFO","qmidms lowpower")
  return self.luaq.req(self.m.QMI_DMS_SET_OPERATING_MODE,{operating_mode="DMS_OP_MODE_LOW_POWER_V01"})
end

-- Change modem operation mode to "shutdown"
function QmiDms:shutdown(type, event)
  self.l.log("LOG_INFO","qmidms shutdown")
  return self.luaq.req(self.m.QMI_DMS_SET_OPERATING_MODE,{operating_mode="DMS_OP_MODE_SHUTTING_DOWN_V01"})
end

-- Change modem operation mode to "offline"
function QmiDms:offline(type, event)
  self.l.log("LOG_INFO","qmidms offline")
  return self.luaq.req(self.m.QMI_DMS_SET_OPERATING_MODE,{operating_mode="DMS_OP_MODE_OFFLINE_V01"})
end

-- Change modem operation mode to "resetting"
function QmiDms:reset(type, event)
  self.l.log("LOG_INFO","qmidms reset")
  return self.luaq.req(self.m.QMI_DMS_SET_OPERATING_MODE,{operating_mode="DMS_OP_MODE_RESETTING_V01"})
end

QmiDms.cbs_system={

    --[[
    -- this function is moved to qmi_uim

    poll_simcard_info=function(type, event)
      local succ,err,resp
      local ia={}

      -- get imsi
      succ,err,resp=luaq.req(m.QMI_DMS_UIM_GET_IMSI)
      if succ  then
        ia.imsi = ffi.string(resp.imsi,ffi.sizeof(resp.imsi))
      else
        l.log("LOG_ERR","IMSI not available")
      end

      -- get ICCID
      succ,err,resp=luaq.req(m.QMI_DMS_UIM_GET_ICCID)
      if succ then
        ia.uim_id=ffi.string(resp.uim_id,ffi.sizeof(resp.uim_id))
      else
        l.log("LOG_ERR","ICCID not available")
      end

      -- get msisdn
      succ,err,resp=luaq.req(m.QMI_DMS_GET_MSISDN)
      if succ then
        if luaq.is_c_true(resp.mobile_id_number_valid) then
          ia.msisdn = ffi.string(resp.mobile_id_number,ffi.sizeof(resp.mobile_id_number))
        end

        -- get imsi again
        if luaq.is_c_true(resp.imsi_valid) then
          ia.imsi = ffi.string(resp.imsi,ffi.sizeof(resp.imsi))
        end
      else
        l.log("LOG_ERR","MSISDN and IMSI not available")
      end

      return watcher.invoke("sys","modem_on_simcard_info",ia)
    end,
]]--

  "poll_modem_info",
  "poll",
  "stop",
  "poll_operating_mode",
  "online",
  -- Change modem operation mode to "lowpower"
  "lowpower",
  -- Change modem operation mode to "shutdown"
  "shutdown",
  -- Change modem operation mode to "offline"
  "offline",
  -- Change modem operation mode to "resetting"
  "reset",
  "reonline",
}

function QmiDms:init()

  self.l.log("LOG_INFO", "initiate qmi_nas")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- initiate qmi
  self.luaq.req(self.m.QMI_DMS_SET_EVENT_REPORT,{
    report_oprt_mode_state=1,
  })

end

return QmiDms
