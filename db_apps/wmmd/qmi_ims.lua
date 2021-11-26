-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi ims module

local QmiIms = require("wmmd.Class"):new()

function QmiIms:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_ims", "LOG_DAEMON") end)

  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")

  self.m = self.luaq.m
  self.e = self.luaq.e
  self.ims_reg_stat = nil
  self.serv_stat_ia={}
end

QmiIms.error_code_names={
  [0] = "other failure",
  [1] = "option unsubscribed",
  [2] = "unknown pdp",
  [3] = "reason not specified",
  [4] = "connection bringup failure",
}

QmiIms.reg_stat_names={
  [0] = "not registered",
  [1] = "registering",
  [2] = "registered",
}

QmiIms.reg_network_names={
  [0] = "wlan",
  [1] = "wwan",
  [2] = "iwlan",
}

QmiIms.serv_stat_names={
  [0] = "no service",
  [1] = "limited service",
  [2] = "full service",
}

QmiIms.serv_rat_names={
  [0] = "wlan",
  [1] = "wwan",
  [2] = "iwlan",
}

function QmiIms:build_pdp_stat(resp)
  local ia={}

  -- build ia
  ia.connected = self.luaq.get_resp_c_member(resp,"is_ims_pdp_connected")
  ia.failure_error_code = self.error_code_names[tonumber(self.luaq.get_resp_c_member(resp,"ims_pdp_failure_error_code"))]

  return ia
end

function QmiIms:build_reg_stat(resp)
  local ia={}

  -- build ia
  ia.registered = self.luaq.is_c_true(self.luaq.get_resp_c_member(resp,"ims_registered"))
  ia.reg_failure_error_code=tonumber(self.luaq.get_resp_c_member(resp,"ims_registration_failure_error_code"))
  ia.reg_stat=self.reg_stat_names[tonumber(self.luaq.get_resp_c_member(resp,"ims_reg_status"))]
  -- QC 3.0 rel does not support these variables
  ia.reg_error_string = nil
  ia.reg_network = nil

  -- zero failure error code does not mean anything - do not take
  if ia.reg_failure_error_code == 0 then
    self.l.log("LOG_DEBUG",string.format("ignore zero registration failure code"))
    ia.reg_failure_error_code = nil
  end

  self.l.log("LOG_DEBUG",string.format("ims registered = %s",ia.registered))
  self.l.log("LOG_DEBUG",string.format("ims reg_stat = %s",ia.reg_stat or "(NULL)"))
  self.l.log("LOG_DEBUG",string.format("ims reg_failure_error_code = %d",ia.reg_failure_error_code or -1))

  -- re-read sim
  if ia.reg_stat ~= self.ims_reg_stat then
    self.l.log("LOG_INFO","ims reg status changed, re-read SIM card information")
    self.watcher.invoke("sys","poll_uim_card_status")
  end
  self.ims_reg_stat = ia.reg_stat

  return ia
end

function QmiIms:build_serv_stat(resp)
  local ia={}

  for _,v in ipairs{"sms","voip","vt","ut","vs"} do
    local serv_stat = v .. "_service_status"
    local serv_rat = v .. "_service_rat"

    ia[serv_rat]=self.serv_rat_names[tonumber(self.luaq.get_resp_c_member(resp,serv_rat))]
    ia[serv_stat]=self.serv_stat_names[tonumber(self.luaq.get_resp_c_member(resp,serv_stat))]

    if ia[serv_rat] then
      self.serv_stat_ia[serv_rat]=ia[serv_rat]
    end

    if ia[serv_stat] then
      self.serv_stat_ia[serv_stat]=ia[serv_stat]
    end
  end

  return self.serv_stat_ia
end


function QmiIms:build_reg_mgr_config(resp)
  local ia={
    enable = resp.ims_test_mode_valid and self.luaq.is_c_true(resp.ims_test_mode),
  }

  return ia
end

function QmiIms:QMI_IMSA_PDP_STATUS_IND(type, event, qm)
  local ia=self:build_pdp_stat(qm.resp)
  return self.watcher.invoke("sys","modem_on_ims_pdp_stat",ia)
end

function QmiIms:QMI_IMSA_REGISTRATION_STATUS_IND(type, event, qm)
  --luaq.log_cdata(string.format("[%s] resp", "QMI_IMSA_REGISTRATION_STATUS_IND"), qm.resp)
  self.l.log("LOG_INFO","QMI_IMSA_REGISTRATION_STATUS_IND")

  local ia = self:build_reg_stat(qm.resp)
  return self.watcher.invoke("sys","modem_on_ims_reg_stat",ia)
end

function QmiIms:QMI_IMSA_SERVICE_STATUS_IND(type, event, qm)
  --luaq.log_cdata(string.format("[%s] resp", "QMI_IMSA_SERVICE_STATUS_IND"), qm.resp)
  local ia = self:build_serv_stat(qm.resp)
  return self.watcher.invoke("sys","modem_on_ims_serv_stat",ia)
end

QmiIms.cbs={
  "QMI_IMSA_PDP_STATUS_IND",
  "QMI_IMSA_REGISTRATION_STATUS_IND",
  "QMI_IMSA_SERVICE_STATUS_IND",
}

function QmiIms:enable_ims_registration(type,event,a)
  self.l.log("LOG_INFO",string.format("[IMS-REG] sys command - test mode (en=%s)",a.ims_test_mode))
  local succ,err,res = self.luaq.req(self.m.QMI_IMS_SETTINGS_SET_IMS_SERVICE_ENABLE_CONFIG,{ims_service_enabled=(a.ims_test_mode and 0 or 1)})


  --luaq.log_cdata(string.format("[%s] resp", "m.QMI_IMS_SETTINGS_SET_REG_MGR_CONFIG"), res)

  return self.watcher.invoke("sys","poll_ims_reg_mgr_config")
end

function QmiIms:poll_ims_pdp_stat(type,event,a)
  -- FIXME: QMI_IMSA_GET_PDP_STATUS is not defined in *.pch files, so luaqmi will raise error here !
  local succ,err,res = self.luaq.req(self.m.QMI_IMSA_GET_PDP_STATUS)

  local ia=self:build_pdp_stat(res)
  return self.watcher.invoke("sys","modem_on_ims_pdp_stat",ia)
end

function QmiIms:poll_ims_reg_stat(type,event,a)
  local succ,err,res = self.luaq.req(self.m.QMI_IMSA_GET_REGISTRATION_STATUS)

  local ia = self:build_reg_stat(res)
  return self.watcher.invoke("sys","modem_on_ims_reg_stat",ia)
end

function QmiIms:poll_ims_serv_stat(type,event,a)
  local succ,err,res = self.luaq.req(self.m.QMI_IMSA_GET_SERVICE_STATUS)

  local ia = self:build_serv_stat(res)
  return self.watcher.invoke("sys","modem_on_ims_serv_stat",ia)
end

function QmiIms:poll_ims_reg_mgr_config(type,event,a)

  local succ,err,res = self.luaq.req(self.m.QMI_IMS_SETTINGS_GET_REG_MGR_CONFIG)

  local ia={
    primary_cscf=self.luaq.is_c_true(res.regmgr_primary_cscf_valid) and self.ffi.string(res.regmgr_primary_cscf) or "",
    pcscf_port=self.luaq.is_c_true(res.regmgr_config_pcscf_port_valid) and tonumber(res.regmgr_config_pcscf_port) or nil,
    ims_test_mode=self.luaq.is_c_true(res.ims_test_mode_valid) and (tonumber(res.ims_test_mode) == 1),
  }

  return self.watcher.invoke("sys","modem_on_reg_mgr_config",ia)
end

function QmiIms:poll_ims(type,event,a)
  -- QC 3.0 release does not support IMS PDP status
  --self.watcher.invoke("sys","poll_ims_pdp_stat")
  self.watcher.invoke("sys","poll_ims_reg_mgr_config")

  return true
end

function QmiIms:poll(type, event, a)
  self.l.log("LOG_DEBUG","qmi ims poll")

  self.watcher.invoke("sys","poll_ims")


  --[[
      local succ,err,res = luaq.req(m.QMI_IMS_SETTINGS_GET_SIP_CONFIG)
      local succ,err,res = luaq.req(m.QMI_IMSP_GET_ENABLER_STATE)
]]--

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Get UT (supplementary) profile APN
--
-- @param type invoke type
-- @param event invoke event name
-- @param a invoke argument
-- @return true when it succeeds. Otherwise, false.
function QmiIms:get_ut_profile_apn(type,event,a)

  self.l.log("LOG_DEBUG",string.format("[custom-apn] get Ut APN"))

  -- request QMI_IMS_SETTINGS_GET_XCAP_NEW_CONFIG
  local succ,err,resp = self.luaq.req(self.m.QMI_IMS_SETTINGS_GET_XCAP_NEW_CONFIG)
  if not succ then
    self.l.log("LOG_ERR",string.format("failed from QMI_IMS_SETTINGS_GET_XCAP_NEW_CONFIG"))
    return false
  end

  -- get Ut Profile APN
  local xcap_apn_wwan = self.luaq.is_c_true(resp.xcap_apn_wwan_valid) and self.ffi.string(resp.xcap_apn_wwan) or nil
  a.ut_profile_apn = xcap_apn_wwan

  self.l.log("LOG_DEBUG",string.format("[custom-apn] get Ut APN result (apn='%s')",a.ut_profile_apn or '[NULL]'))

  return (a.ut_profile_apn ~= nil)
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Set UT (supplementary) profile APN
--
-- @param type invoke type
-- @param event invoke event name
-- @param a invoke argument
-- @return true when it succeeds. Otherwise, false.
function QmiIms:set_ut_profile_apn(type,event,a)
  self.l.log("LOG_DEBUG",string.format("[custom-apn] set Ut APN (apn='%s')",a.ut_profile_apn))

  -- request QMI_IMS_SETTINGS_GET_XCAP_NEW_CONFIG
  local succ,err,resp = self.luaq.req(self.m.QMI_IMS_SETTINGS_SET_XCAP_NEW_CONFIG,{xcap_apn_wwan=a.ut_profile_apn})
  if not succ then
    self.l.log("LOG_ERR",string.format("failed from QMI_IMS_SETTINGS_SET_XCAP_NEW_CONFIG"))
    return false
  end

  return true
end

QmiIms.cbs_system={
  "enable_ims_registration",
  "poll_ims_pdp_stat",
  "poll_ims_reg_stat",
  "poll_ims_serv_stat",
  "poll_ims_reg_mgr_config",
  "poll_ims",
  "poll",
  "get_ut_profile_apn",
  "set_ut_profile_apn",
}

function QmiIms:init()

  self.l.log("LOG_INFO", "initiate qmi_ims")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  local succ,err,res = self.luaq.req(self.m.QMI_IMSA_IND_REG,{
    reg_status_config=1,
    service_status_config=1,

  --[[
  -- QC 3.0 release does not support these variables
  rat_handover_config=1,
  pdp_status_config=1,
  acs_status_config=1,
  lte_attach_params_config=1,
  subscription_status_config=1,
  network_provisioning_status_config=1,

  ]]--

  })


end

return QmiIms
