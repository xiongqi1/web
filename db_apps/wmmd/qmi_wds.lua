-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi wds module, PDS control

local QmiWds = require("wmmd.Class"):new()

function QmiWds:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_wds", "LOG_DAEMON") end)

  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")
  self.ts = require("turbo.socket_ffi")

  self.m = self.luaq.m
  self.e = self.luaq.e
  self.sq = self.smachine.get_smachine("qmi_smachine")

  self.wds_pdp_types={
    WDS_PDP_TYPE_PDP_IPV4_V01 = "ipv4",
    WDS_PDP_TYPE_PDP_PPP_V01 = "ppp",
    WDS_PDP_TYPE_PDP_IPV6_V01 = "ipv6",
    WDS_PDP_TYPE_PDP_IPV4V6_V01 = "ipv4v6",
  }
  self.wds_pdp_types_reverse=self:reverse_table(self.wds_pdp_types)

  self.wds_apn_type_mask={
     [0x000] = "",  -- unspecified, but leave as blank
     [0x001] = "default",
     [0x002] = "ims",
     [0x004] = "mms",
     [0x008] = "dun",
     [0x010] = "supl",
     [0x020] = "hipri",
     [0x040] = "fota",
     [0x080] = "cbs",
     [0x100] = "ia",
     [0x200] = "emergency",
     [0x400] = "ut",
     [0x800] = "mcx"
  }
  self.wds_apn_type_mask_reverse=self:reverse_table(self.wds_apn_type_mask)
end

-- table that converts a pkt numeric status to human-readable PDP status
QmiWds.pdp_stat_names = {
  [0x01] = "down",
  [0x02] = "up",
-- do not take these states
--[0x03] = "suspended",
--[0x04] = "authenticating",
}

function QmiWds:get_runtime_settings(qmi_lua_sid)
  return self.luaq.req(self.m.QMI_WDS_GET_RUNTIME_SETTINGS,{
    requested_settings=bit.bor(
      0x0001, -- Profile identifier
      0x0002, -- Profile name
      0x0004, -- PDP type
      0x0008, -- APN name
      0x0010, -- DNS address
      0x0020, -- UMTS/GPRS granted QoS
      0x0040, -- User name
      0x0080, -- Authentication protocol
      0x0100, -- IP address
      0x0200, -- Gateway information (address and subnet mask)
      0x0400, -- P-CSCF address using PCO flag
      0x0800, -- P-CSCF server address list
      0x1000, -- P-CSCF domain name list
      0x2000, -- MTU
      0x4000, -- Domain name list
      0x8000, -- IP family
      0x10000, -- IM_CN flag
      0x20000 -- Technology name
    )
  },0,qmi_lua_sid)
end

function QmiWds:set_event_report()

  return self.luaq.req(self.m.QMI_WDS_SET_EVENT_REPORT,{
    request_mask=self.bit.bor(
      0x01, -- RSSI
      0x02, -- ECIO
      0x04, -- IO
      0x08, -- SINR
      0x10, -- RATE
      0x20, -- RSRQ
      0x40, -- LTE_SNR
      0x80, -- LTE_RSRP
      0x00
    )
  })
end

QmiWds.sid_index_to_qie={}


function QmiWds:get_autoconnect_setting()
  return self.luaq.req(self.m.QMI_WDS_GET_AUTOCONNECT_SETTING)
end

function QmiWds:get_profile_list(bsettings,qmi_lua_sid)
  return self.luaq.req(self.m.QMI_WDS_GET_PROFILE_LIST,bsettings,0,qmi_lua_sid)
end

function QmiWds:get_profile_settings(profile_index,qmi_lua_sid)
  local qm = self.luaq.new_msg(self.m.QMI_WDS_GET_PROFILE_SETTINGS,qmi_lua_sid)
  qm.req.profile.profile_index=profile_index

  -- send
  if not self.luaq.send_msg(qm) then
    return
  end

  return self.luaq.ret_qm_resp(qm)
end

function QmiWds:modify_profile_settings(profile_index,bsettings,qmi_lua_sid)
  local qm = self.luaq.new_msg(self.m.QMI_WDS_MODIFY_PROFILE_SETTINGS,qmi_lua_sid)

  qm.req.profile.profile_index=profile_index

  if bsettings then
    self.luaq.copy_bsettings_to_req(qm.req,bsettings)
  end

  -- send
  if not self.luaq.send_msg(qm) then
    return
  end

  return self.luaq.ret_qm_resp(qm)
end

function QmiWds:stop_network_interface(qmi_lua_sid)

  qmi_lua_sid = tonumber(qmi_lua_sid) or 1
  local qie = self.sid_index_to_qie[qmi_lua_sid]
  if not qie then
    self.l.log("LOG_ERR",string.format("profile not connected to qie (qmi_lua_sid=%d)",qmi_lua_sid))
    return false
  end

  local bsettings

  if not qie.pkt_data_handle or qie.pkt_data_handle == 0 then
    self.l.log("LOG_INFO",string.format("QMI_WDS_STOP_NETWORK_INTERFACE without pkt handle (sname=%s,sid=%d,qmi_lua_sid=%d)",qie.sname,qie.qmi_lua_sid,qmi_lua_sid))
    bsettings = {pkt_data_handle=0xFFFFFFFF,disable_autoconnect=true}
  else
    self.l.log("LOG_INFO",string.format("QMI_WDS_STOP_NETWORK_INTERFACE - pkt_data_handle=%x",qie.pkt_data_handle))
    bsettings = {pkt_data_handle=qie.pkt_data_handle}
  end

  local succ,qerr,resp,qm = self.luaq.req(self.m.QMI_WDS_STOP_NETWORK_INTERFACE,bsettings,0,qie.qmi_lua_sid)

  qie.pkt_data_handle = nil

  return succ,qerr,resp
end

function QmiWds:QMI_WDS_START_NETWORK_INTERFACE(type, event, qm)
  self.l.log("LOG_INFO","got QMI_WDS_START_NETWORK_INTERFACE resp")

  local resp = qm.resp

  -- check qmi result
  local succ = tonumber(resp.resp.result) == 0

  if not succ then
    local service_id = qm.qie.qmi_lua_sid
    self.l.log("LOG_ERR",string.format("start network interface failed (service_id=%d)",service_id))
    self:call_modem_on_rmnet_change_with_down(resp,service_id,"start_network_interface")
    return
  end

  -- store pkt_data_handle
  self.l.log("LOG_INFO",string.format("QMI_WDS_START_NETWORK_INTERFACE - pkt_data_handle=%x",qm.resp.pkt_data_handle))
  qm.qie.pkt_data_handle = qm.resp.pkt_data_handle

  return true
end

function QmiWds:start_network_interface(profile_index,ipv6_enable,timeout,qmi_lua_sid)

  local bsettings={}

  if ipv6_enable then
    bsettings.ip_preference="WDS_IP_FAMILY_IPV6_V01"
  else
    bsettings.ip_preference="WDS_IP_FAMILY_IPV4_V01"
  end

  -- set ip family preference
  local succ,qerr,resp,qm = self.luaq.req(self.m.QMI_WDS_SET_CLIENT_IP_FAMILY_PREF,bsettings,0,qmi_lua_sid)
  if not succ then
    self.l.log("LOG_ERR",string.format("QMI_WDS_SET_CLIENT_IP_FAMILY_PREF_REQ failed (qerr=%d)",qerr))
    return
  end

  bsettings={}

  bsettings.profile_index=profile_index
  bsettings.enable_autoconnect=false

  bsettings.technology_preference=self.bit.bor(
    0x01 -- 3GPP
  )

  if ipv6_enable then
    bsettings.ip_family_preference="WDS_IP_FAMILY_PREF_IPV6_V01"
  else
    bsettings.ip_family_preference="WDS_IP_FAMILY_PREF_IPV4_V01"
  end

  bsettings.call_type = "WDS_CALL_TYPE_EMBEDDED_CALL_V01"

  -- start network interface
  local qm = self.luaq.new_msg(self.m.QMI_WDS_START_NETWORK_INTERFACE,qmi_lua_sid)
  self.luaq.copy_bsettings_to_req(qm.req,bsettings)
  local succ = self.luaq.send_msg_async(qm,timeout)

  self.sid_index_to_qie[qmi_lua_sid]=qm.qie

  if not succ then
    self.l.log("LOG_ERR",string.format("start network interface immediately failed (service_id=%d)",qmi_lua_sid))
    self:call_modem_on_rmnet_change_with_down(nil,qmi_lua_sid,"start_network_interface")
  end

  return succ
end

function QmiWds:bind_mux_data_port(ep_type,epid,mux_id,qmi_lua_sid)
  local qm = self.luaq.new_msg(self.m.QMI_WDS_BIND_MUX_DATA_PORT,qmi_lua_sid)

  -- build
  qm.req.ep_id_valid=true
  qm.req.ep_id.ep_type=ep_type
  qm.req.ep_id.iface_id=epid
  qm.req.mux_id_valid=true
  qm.req.mux_id=mux_id

  -- send
  if not self.luaq.send_msg(qm) then
    return nil
  end

  return self.luaq.ret_qm_resp(qm)
end

function QmiWds:inet_pton(ipv6addr_str)
  local AF_INET6 = self.ts.AF_INET6

  local in6addr = self.ffi.new("struct in6_addr")
  local rc = self.ffi.C.inet_pton(AF_INET6, ipv6addr_str,in6addr)

  if rc == 0 then
    error(string.format("invalid address %s",ipv6addr_str))
  elseif rc == -1 then
    local errno = self.ffi.errno()
    error(string.format("could not parse address - %s",self.ts.strerror(errno)))
  end

  return in6addr
end

function QmiWds:inet_ntop(ipv6addr_in6addr)
  local hin6addr = ipv6addr_in6addr

  local INET6_ADDRSTRLEN = 46
  local AF_INET6 = self.ts.AF_INET6

  local addrbuf = self.ffi.new("char[?]", INET6_ADDRSTRLEN)
  self.ffi.C.inet_ntop(AF_INET6, hin6addr, addrbuf, INET6_ADDRSTRLEN)

  return self.ffi.string(addrbuf)
end

function QmiWds:inet_ntop_with_len(ipv6addr_in6addr,len)
  local ipv6addr_str = self:inet_ntop(ipv6addr_in6addr)

  if len == 0 then
    return ipv6addr_str
  end

  return ipv6addr_str and string.format("%s/%d",ipv6addr_str,len) or nil
end

function QmiWds:inet_ntoa(ipaddr)
  local hipaddr = self.ffi.C.ntohl(ipaddr)
  local inaddr=self.ffi.new("struct in_addr",hipaddr)

  return self.ffi.string(self.ffi.C.inet_ntoa(inaddr))
end

function QmiWds:reverse_table(t)
  local o={}
  for k,v in pairs(t) do
    o[v]=k
  end

  return o
end

QmiWds.call_end_reason_names={
  [0x001] = "unspecified",
  [0x002] = "client end",
  [0x003] = "no srv",
  [0x004] = "fade",
  [0x005] = "rel normal",
  [0x006] = "acc in prog",
  [0x007] = "acc fail",
  [0x008] = "redir or handoff",
  [0x009] = "close in progress",
  [0x00a] = "auth failed",
  [0x00b] = "internal call end",
  [0x1f4] = "cdma lock",
  [0x1f5] = "intercept",
  [0x1f6] = "reorder",
  [0x1f7] = "rel so rej",
  [0x1f8] = "incom call",
  [0x1f9] = "alert stop",
  [0x1fa] = "activation",
  [0x1fb] = "max access probe",
  [0x1fc] = "ccs not supp by bs",
  [0x1fd] = "no response from bs",
  [0x1fe] = "rejected by bs",
  [0x1ff] = "incompatible",
  [0x200] = "already in tc",
  [0x201] = "user call orig during gps",
  [0x202] = "user call orig during sms",
  [0x203] = "no cdma srv",
  [0x3e8] = "conf failed",
  [0x3e9] = "incom rej",
  [0x3ea] = "no gw srv",
  [0x3eb] = "network end",
  [0x3ec] = "llc sndcp failure",
  [0x3ed] = "insufficient resources",
  [0x3ee] = "option temp ooo",
  [0x3ef] = "nsapi already used",
  [0x3f0] = "regular deactivation",
  [0x3f1] = "network failure",
  [0x3f2] = "umts reattach req",
  [0x3f3] = "protocol error",
  [0x3f4] = "operator determined barring",
  [0x3f5] = "unknown apn",
  [0x3f6] = "unknown pdp",
  [0x3f7] = "ggsn reject",
  [0x3f8] = "activation reject",
  [0x3f9] = "option not supp",
  [0x3fa] = "option unsubscribed",
  [0x3fb] = "qos not accepted",
  [0x3fc] = "tft semantic error",
  [0x3fd] = "tft syntax error",
  [0x3fe] = "unknown pdp context",
  [0x3ff] = "filter semantic error",
  [0x400] = "filter syntax error",
  [0x401] = "pdp without active tft",
  [0x402] = "invalid transaction id",
  [0x403] = "message incorrect semantic",
  [0x404] = "invalid mandatory info",
  [0x405] = "message type unsupported",
  [0x406] = "msg type noncompatible state",
  [0x407] = "unknown info element",
  [0x408] = "conditional ie error",
  [0x409] = "msg and protocol state uncompatible",
  [0x40a] = "apn type conflict",
  [0x40b] = "no gprs context",
  [0x40c] = "feature not supported",
  [0x5dc] = "cd gen or busy",
  [0x5dd] = "cd bill or auth",
  [0x5de] = "chg hdr",
  [0x5df] = "exit hdr",
  [0x5e0] = "hdr no session",
  [0x5e1] = "hdr orig during gps fix",
  [0x5e2] = "hdr cs timeout",
  [0x5e3] = "hdr released by cm",

}

QmiWds.verbose_call_end_reason_names={
  [0x00] = "unspecified",
  [0x01] = "mobile ip",
  [0x02] = "internal",
  [0x03] = "call manager defined",
  [0x06] = "3gpp spec defined",
  [0x07] = "ppp",
  [0x08] = "ehrpd",
  [0x09] = "ipv6",
}


function QmiWds:convert_str_to_wds_pdp_type(pdp_type)
  if not pdp_type or pdp_type == "" then
    return
  end

  return self.wds_pdp_types_reverse[pdp_type]
end

function QmiWds:convert_wds_pdp_type_to_str(wds_pdp_type)
  for k,v in pairs(self.wds_pdp_types) do
    if k == wds_pdp_type then
      return v
    end
  end
end

function QmiWds:convert_str_to_wds_apn_type_mask(apn_type)
  return self.wds_apn_type_mask_reverse[apn_type]
end

function QmiWds:convert_wds_apn_type_mask_to_str(wds_apn_type)
  return self.wds_apn_type_mask[tonumber(wds_apn_type)]
end

function QmiWds:convert_auth_pref_to_str(wds_auth)
  local wds_auth_no=tonumber(wds_auth)

  if wds_auth_no == 0 then
    return "none"
  elseif wds_auth_no == 1 then
    return "pap"
  elseif wds_auth_no == 2 then
    return "chap"
  elseif wds_auth_no == 3 then
    return "pap|chap"
  end
end

function QmiWds:convert_str_to_auth_pref(auth)

  local auth_pref = 0

  local auth_lower = string.lower(auth)

  if string.find(auth_lower,"pap") then
    auth_pref=auth_pref+1
  end

  if string.find(auth_lower,"chap") then
    auth_pref=auth_pref+2
  end

  return auth_pref
end

function QmiWds:build_call_end_reason(resp,ind)
  local ia={}

  if self.luaq.is_c_true(resp.call_end_reason_valid) then
    ia.call_end_reason=self.call_end_reason_names[tonumber(resp.call_end_reason)]
  end

  if self.luaq.is_c_true(resp.verbose_call_end_reason_valid) then
    ia.verbose_call_end_reason_type_num = tonumber(resp.verbose_call_end_reason.call_end_reason_type)
    ia.verbose_call_end_reason_type = self.verbose_call_end_reason_names[ia.verbose_call_end_reason_type_num]
    ia.verbose_call_end_reason = tonumber(resp.verbose_call_end_reason.call_end_reason)
  end

  if not ind then
    ia.error = resp.resp.error
  end

  return ia
end


-----------------------------------------------------------------------------------------------------------------------
-- Invoke modem_on_rmnet with parameters
--
-- @param resp QMI response structure.
-- @param service_id QMI service id.
-- @param func_name Related function name.
-- @return True is returned when it success. Otherwise, false.
function QmiWds:call_modem_on_rmnet_change_with_down(resp,service_id,func_name)
  -- add call end reason
  local ia={}

  if resp then
    ia=self:build_call_end_reason(resp,false)
  end

  self.l.log("LOG_NOTICE",string.format("[dual-stack] end_reason_reason=%s #%d",ia.call_end_reason or "none",service_id or -1))
  self.l.log("LOG_NOTICE",string.format("[dual-stack] verbose_call_end_reason_type=%s #%d",ia.verbose_call_end_reason_type or "none",service_id or -1))
  self.l.log("LOG_NOTICE",string.format("[dual-stack] verbose_call_end_reason=%s #%d",ia.verbose_call_end_reason or "none",service_id or -1))

  ia.service_id=service_id
  ia.status="down"
  -- store failure function name
  ia.func_name=func_name

  return self.watcher.invoke("sys","modem_on_rmnet_change",ia)
end

function QmiWds:QMI_WDS_PKT_SRVC_STATUS_IND(type, event, qm)

  local ip_family

  self.l.log("LOG_DEBUG","got QMI_WDS_PKT_SRVC_STATUS_IND")

  if self.luaq.is_c_true(qm.resp.ip_family_valid) then
    ip_family = tonumber(qm.resp.ip_family)
  end

  -- build call end reason
  local ia=self:build_call_end_reason(qm.resp,true)

  -- get numeric pkt stat
  local pkt_stat = tonumber(qm.resp.status.connection_status)
  local pdp_stat = self.pdp_stat_names[pkt_stat]

  self.l.log("LOG_NOTICE",string.format("WDS PDP '%s' indication received (service_id=%d,ip_family=%d,stat=%d,end_reason=%s,vtype=%s,vreason=%d)",pdp_stat,qm.qie.qmi_lua_sid,ip_family or -1,pkt_stat,ia.call_end_reason,ia.verbose_call_end_reason_type,ia.verbose_call_end_reason or -1))

  -- accept connected and disconnected state only, bypass if any other state is received.
  if not pdp_stat then
    self.l.log("LOG_DEBUG",string.format("skip WDS PDP indication (service_id=%d,ip_family=%d,stat=%d)",qm.qie.qmi_lua_sid,ip_family or -1,pkt_stat))
    return true
  end

  -- override PDP state, disconnect the connection if reconfiguration is required by WDS
  if self.luaq.is_c_true(qm.resp.status.reconfiguration_required) then
    self.l.log("LOG_NOTICE",string.format("apply WDS PDP 'down' state to reconfigure PDP session (service_id=%d,ip_family=%d,stat=%d)",qm.qie.qmi_lua_sid,ip_family or -1,pkt_stat))
    pdp_stat = "down"
  end


  -- set status information
  ia.ip_family=ip_family
  ia.service_id=qm.qie.qmi_lua_sid
  ia.status=pdp_stat

  self.l.log("LOG_DEBUG",string.format("process WDS PDP '%s' indication received (service_id=%d,ip_family=%d,pkt_stat=%d)",pdp_stat,qm.qie.qmi_lua_sid,ip_family or -1,pkt_stat))

  self.watcher.invoke("sys","modem_on_rmnet_change",ia)

  return true
end

local rate_unit = {
  [0] = "bps",	--bits per second
  [1] = "Kbps",
  [2] = "Mbps",
  [3] = "Gbps"
}

function QmiWds:QMI_WDS_GET_CURRENT_CHANNEL_RATE(type, event, qm)
  self.l.log("LOG_INFO","QMI_WDS_GET_CURRENT_CHANNEL_RATE Response received")

  if qm.resp.resp.error == 0 then
    if self.luaq.is_c_member_and_true(qm.resp, "rates_ex_valid") then
      local rate_unit_val = rate_unit[tonumber(qm.resp.rates_ex.rate_type)]
      self.l.log("LOG_INFO",string.format("QMI_WDS_GET_CURRENT_CHANNEL_RATE max_tx_rate = %d, max_rx_rate = %d in %s",tonumber(qm.resp.rates_ex.max_channel_tx_rate),tonumber(qm.resp.rates_ex.max_channel_rx_rate),rate_unit_val))
      -- set rdb
      self.wrdb:setp("max_channel_tx_rate", tonumber(qm.resp.rates_ex.max_channel_tx_rate)..rate_unit_val)
      self.wrdb:setp("max_channel_rx_rate", tonumber(qm.resp.rates_ex.max_channel_rx_rate)..rate_unit_val)
    else
      self.l.log("LOG_ERR",string.format("QMI_WDS_GET_CURRENT_CHANNEL_RATE returned error = %d",tonumber(qm.resp.resp.error)))
    end
  end
end

QmiWds.cbs={
  "QMI_WDS_PKT_SRVC_STATUS_IND",
  "QMI_WDS_START_NETWORK_INTERFACE",
  "QMI_WDS_GET_CURRENT_CHANNEL_RATE",
}

function QmiWds:poll_current_channel_rate()
  local qm = self.luaq.new_msg(self.m.QMI_WDS_GET_CURRENT_CHANNEL_RATE)

  self.l.log("LOG_INFO", "send QMI_WDS_GET_CURRENT_CHANNEL_RATE Request")
  local succ = self.luaq.send_msg_async(qm,self.config.modem_generic_poll_timeout)

  return succ
end


function QmiWds:poll(type, event, a)
  self.l.log("LOG_DEBUG","qmi wds poll")

#ifdef V_POLL_WDS_CHAN_RATE_y
  self:poll_current_channel_rate()
#endif

  return true
end

function QmiWds:read_rmnet(type,event,a)

  local bsettings={}

  -- set profile index
  bsettings.profile_index=a.profile_index

  local succ,qerr,resp

  if not a.profile_index then
    self.l.log("LOG_DEBUG",string.format("[pf-mapping] profile index not specified"))
  else
    -- set profile settings
    succ,qerr,resp = self:get_profile_settings(a.profile_index)
  end

  bsettings.ref=a.ref
  bsettings.valid = succ

  if succ then
    bsettings.apn=resp.apn_name_valid ~= 0 and self.ffi.string(resp.apn_name)
    bsettings.user=resp.username_valid ~= 0 and self.ffi.string(resp.username)
    bsettings.pass=resp.password_valid ~= 0 and self.ffi.string(resp.password)
    bsettings.pdp_type=resp.pdp_type_valid ~= 0 and self:convert_wds_pdp_type_to_str(resp.pdp_type) or nil
    bsettings.auth_type=resp.authentication_preference_valid ~= 0 and self:convert_auth_pref_to_str(resp.authentication_preference) or nil
    bsettings.emergency_calls=resp.support_emergency_calls_valid ~= 0 and tostring(resp.support_emergency_calls) or nil
    bsettings.apn_type_mask=resp.apn_type_mask_valid ~= 0 and self:convert_wds_apn_type_mask_to_str(resp.apn_type_mask) or nil
  end

  -- invoke read_rmnets_on_read
  return self.watcher.invoke("sys","read_rmnet_on_read",bsettings)
end

function QmiWds:stop_rmnet(type,event,a)
  local succ = self:stop_network_interface(a.service_id)
  if not succ then
    self.watcher.invoke("sys","modem_on_rmnet_change",{service_id=a.service_id,status="down"})
  end

  return succ
end

function QmiWds:write_rmnet(type,event,a)
  -- build bsettings
  local bsettings={
    apn_name=a.apn or "",
    username=a.user,
    password=a.pass,
    pdp_type=self:convert_str_to_wds_pdp_type(a.pdp_type),
    authentication_preference=self:convert_str_to_auth_pref(a.auth_type),
    support_emergency_calls=tonumber(a.emergency_calls),
    apn_type_mask=self:convert_str_to_wds_apn_type_mask(a.apn_type_mask)
  }

  local profile_index = a.profile_index
  local found = false

  if profile_index then
    -- As QMI IMS can be an optional module, check its availability before using IMS.
    if not self.luaq.qis['ims'] then
      self.l.log("LOG_DEBUG", "QMI IMS service is not available. UT or IMS settings are not changed.")
    elseif not self.luaq.qis['netcomm'] then
      self.l.log("LOG_DEBUG", "QMI Netcomm service is not available. UT or IMS settings are not changed.")
    else
      -- Only do this if QMI_IMS message is supported
      if profile_index == 1  and self.m.QMI_IMS_SETTINGS_SET_UT_CONFIG then
        local succ,qerr,resp = self.luaq.req(self.m.QMI_IMS_SETTINGS_SET_UT_CONFIG,{ut_apn_name=a.apn})
        if succ then
          self.l.log("LOG_INFO", "Changed UT APN to "..a.apn)
        else
          self.l.log("LOG_ERR",string.format("Failed to change UT APN - %d,%d",tonumber(resp.resp.result), tonumber(resp.resp.error)))
        end
      elseif profile_index == 2 and self.m["QMI_NETCOMM_CHANGE_IMS_APN"] then
        local succ,qerr,resp = self.luaq.req(self.m.QMI_NETCOMM_CHANGE_IMS_APN,{apn=a.apn})
        if succ then
          self.l.log("LOG_INFO", "Changed IMS APN to "..a.apn)
        else
          self.l.log("LOG_ERR",string.format("Failed to change IMS APN - %d,%d",tonumber(resp.resp.result), tonumber(resp.resp.error)))
        end
      end
    end

    -- search profile_index
    local succ,qerr,resp = self:get_profile_list({},a.service_id)
    if succ then
      for i = 0,resp.profile_list_len-1 do
        if resp.profile_list[i].profile_index == profile_index then
          found=true
        end
      end
    end
  end

  if found then
    succ,qerr,resp = self:modify_profile_settings(profile_index,bsettings,a.service_id)
    if not succ then
      self.l.log("LOG_ERR",string.format("failed to modify profile - %d",profile_index))
      return
    end
  else

    succ,qerr,resp = self.luaq.req(self.m.QMI_WDS_CREATE_PROFILE,bsettings,0,a.service_id)
    if not succ then
      self.l.log("LOG_ERR",string.format("failed to create profile - ifn=%s",a.network_interface))
      return
    end

    a.module_profile_idx=resp.profile.profile_index
  end

  -- invoke write_rmnets_on_write
  return self.watcher.invoke("sys","write_rmnet_on_write",a)
end

-- query IP information of rmnet
function QmiWds:get_rmnet_stat(type,event,a)

  -- query runtime settings
  local succ, qerr, resp = self:get_runtime_settings(a.service_id)

  if succ then
    local info = a.info

    -- get IPv4 IP information
    info.dns1 = self.luaq.is_c_true(resp.primary_DNS_IPv4_address_preference_valid) and self:inet_ntoa(resp.primary_DNS_IPv4_address_preference) or nil
    info.dns2 = self.luaq.is_c_true(resp.secondary_DNS_IPv4_address_preference_valid) and self:inet_ntoa(resp.secondary_DNS_IPv4_address_preference) or nil
    info.gw = self.luaq.is_c_true(resp.ipv4_gateway_addr_valid) and self:inet_ntoa(resp.ipv4_gateway_addr) or nil
    info.mask = self.luaq.is_c_true(resp.ipv4_subnet_mask_valid) and self:inet_ntoa(resp.ipv4_subnet_mask) or nil
    info.iplocal = self.luaq.is_c_true(resp.ipv4_address_preference_valid) and self:inet_ntoa(resp.ipv4_address_preference) or nil
    info.mtu = self.luaq.is_c_true(resp.mtu_valid) and resp.mtu or nil

    -- get IPv6 IP information
    info.ipv6_dns1 = self.luaq.is_c_true(resp.ipv6_addr_valid) and self:inet_ntop(resp.primary_dns_IPv6_address) or nil
    info.ipv6_dns2 = self.luaq.is_c_true(resp.secondary_dns_IPv6_address_valid) and self:inet_ntop(resp.secondary_dns_IPv6_address) or nil
    info.ipv6_gw = self.luaq.is_c_true(resp.ipv6_gateway_addr_valid) and self:inet_ntop_with_len(resp.ipv6_gateway_addr.ipv6_addr,resp.ipv6_gateway_addr.ipv6_prefix_length) or nil
    info.ipv6_ipaddr = self.luaq.is_c_true(resp.ipv6_addr_valid) and self:inet_ntop_with_len(resp.ipv6_addr.ipv6_addr,resp.ipv6_addr.ipv6_prefix_length) or nil

    -- print debug information
    self.l.log("LOG_DEBUG",string.format("* runtime settings available - #%d",a.service_id))
    for m,v in pairs(info) do
      self.l.log("LOG_DEBUG",string.format("runtime settings [%s] = %s - #%d",m,v,a.service_id))
    end
  else
    self.l.log("LOG_DEBUG",string.format("failed get runtime settings - #%d",a.service_id))
  end

  return succ

end

function QmiWds:start_rmnet(type,event,a)
  -- get mux info
  local conn_id,ep_type,epid,mux_id = self.luaq.get_conn_id_by_name(a.network_interface)
  self.l.log("LOG_INFO",string.format("bind interface - ifn=%s,conn_id=%d,ep_type=%d,epid=%d,mux_id=%d",a.network_interface,conn_id,ep_type,epid,mux_id))

  local bind_succ
  local succ,err,resp

  -- bind and start
  bind_succ=self:bind_mux_data_port(ep_type,epid,mux_id,a.service_id)
  if not bind_succ then
    self.l.log("LOG_ERR",string.format("failed to bind network interface (ifs=%s)",a.network_interface))

    self:call_modem_on_rmnet_change_with_down(nil,a.service_id,"bind_mux_data_port")
    return
  end

  -- connect ipv4
  self.l.log("LOG_INFO",string.format("connect %s (ifs=%s)",(a.ipv6_enable and "ipv6" or "ipv4"),a.network_interface))
  succ = self:start_network_interface(a.profile_index,a.ipv6_enable,a.timeout,a.service_id)

  return succ
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Get SUPL profile APN
--
-- @param type invoke type
-- @param event invoke event name
-- @param a invoke argument
-- @return true when it succeeds. Otherwise, false.
function QmiWds:get_supl_profile_number(type,event,a)
  -- request QMI_WDS_GET_DEFAULT_PROFILE_NUM
  --
  -- !! note !!
  --
  -- Qualcomm SUPL uses Default profile for SUPL.
  --
  local qm = self.luaq.new_msg(self.m.QMI_WDS_GET_DEFAULT_PROFILE_NUM)

  qm.req.profile.profile_type="WDS_PROFILE_TYPE_3GPP_V01"
  qm.req.profile.profile_family="WDS_PROFILE_FAMILY_EMBEDDED_V01"

  local succ = self.luaq.send_msg(qm)
  local resp = qm.resp
  local qres = tonumber(qm.resp.resp.result)

  local extended_error_code = self.luaq.is_c_true(resp.extended_error_code_valid) and tonumber(resp.extended_error_code) or 0;

  if not succ or (qres~=0) or (extended_error_code ~=0) then
    self.l.log("LOG_ERR",string.format("failed from QMI_WDS_GET_DEFAULT_PROFILE_NUM req (qres=%d,ext_err=%d)",qres,extended_error_code))
    return false
  end

  a.supl_profile_number = tonumber(resp.profile_index)

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Set SUPL profile APN
--
-- @param type invoke type
-- @param event invoke event name
-- @param a invoke argument
-- @return true when it succeeds. Otherwise, false.
function QmiWds:set_supl_profile_number(type,event,a)
  self.l.log("LOG_DEBUG",string.format("[custom-apn] set supl apn (profile_no=%d)",a.supl_profile_number))

  -- request QMI_WDS_SET_DEFAULT_PROFILE_NUM
  local qm = self.luaq.new_msg(self.m.QMI_WDS_SET_DEFAULT_PROFILE_NUM)

  qm.req.profile_identifier.profile_type="WDS_PROFILE_TYPE_3GPP_V01"
  qm.req.profile_identifier.profile_family="WDS_PROFILE_FAMILY_EMBEDDED_V01"
  qm.req.profile_identifier.profile_index=a.supl_profile_number

  local succ = self.luaq.send_msg(qm)

  local resp = qm.resp
  local qres = tonumber(qm.resp.resp.result)
  local extended_error_code = self.luaq.is_c_true(resp.extended_error_code_valid) and tonumber(resp.extended_error_code) or 0;

  if not succ or (qres~=0) or (extended_error_code ~=0) then
    self.l.log("LOG_ERR",string.format("failed from QMI_WDS_SET_DEFAULT_PROFILE_NUM req (qerr=%d,ext_err=%d)",qerr,extended_error_code))
    return false
  end

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Set default profile number
--
-- This function configures Default APN for LTE attach procedure.
--
-- @param type invoke type
-- @param event invoke event name
-- @param a invoke argument
-- @return true when it succeeds. Otherwise, false.
function QmiWds:set_default_profile_number(type,event,a)

  self.l.log("LOG_DEBUG",string.format("[custom-apn] set default profile (profiel_no=%d)",a.default_profile_number))

  --
  -- !! note !!
  --
  -- As this function configures Default APN for LTE attach procedure, QMI_WDS_SET_LTE_ATTACH_PDN_LIST is used.
  --
  local qm = self.luaq.new_msg(self.m.QMI_WDS_SET_LTE_ATTACH_PDN_LIST)

  qm.req.attach_pdn_list_len = 1
  qm.req.attach_pdn_list[0] = a.default_profile_number

  local succ = self.luaq.send_msg(qm)
  local resp = qm.resp
  local qres = tonumber(qm.resp.resp.result)

  if not succ or (qres ~= 0) then
    self.l.log("LOG_ERR",string.format("failed from QMI_WDS_SET_LTE_ATTACH_PDN_LIST"))
    return false
  end

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Get default profile number
--
-- @param type invoke type
-- @param event invoke event name
-- @param a invoke argument
-- @return true when it succeeds. Otherwise, false.
function QmiWds:get_default_profile_number(type,event,a)
  self.l.log("LOG_DEBUG",string.format("[custom-apn] get default profile"))

  -- request QMI_WDS_GET_LTE_ATTACH_PDN_LIST
  local qm = self.luaq.new_msg(self.m.QMI_WDS_GET_LTE_ATTACH_PDN_LIST)
  local succ = self.luaq.send_msg(qm)
  local resp = qm.resp
  local qres = tonumber(qm.resp.resp.result)

  if not succ or (qres ~=0) then
    self.l.log("LOG_ERR",string.format("failed from QMI_WDS_GET_LTE_ATTACH_PDN_LIST"))
    return false
  end

  -- collect attach PDNs
  if self.luaq.is_c_true(resp.attach_pdn_list_valid) then
    self.l.log("LOG_DEBUG",string.format("[custom-apn] total number of LTE attach PDN total = %d ",resp.attach_pdn_list_len))
    for i=0,tonumber(resp.attach_pdn_list_len)-1 do
      self.l.log("LOG_DEBUG",string.format("[custom-apn] LTE attach PDN #%d = '%s'",i,tonumber(resp.attach_pdn_list[i])))
    end
  end

  -- collect pending attach PDNs
  if self.luaq.is_c_true(resp.pending_attach_pdn_list_valid) then
    self.l.log("LOG_DEBUG",string.format("[custom-apn] total number of pending LTE attach PDN total = %d ",resp.pending_attach_pdn_list_len))
    for i=0,tonumber(resp.pending_attach_pdn_list_len)-1 do
      self.l.log("LOG_DEBUG",string.format("[custom-apn] LTE pending attach PDN #%d = '%s'",i,tonumber(resp.pending_attach_pdn_list[i])))
    end
  end

  -- get current default profile
  local default_profile_number = self.luaq.is_c_true(resp.pending_attach_pdn_list_valid) and (resp.pending_attach_pdn_list_len>0) and resp.pending_attach_pdn_list[0] or nil
  if not default_profile_number then
    default_profile_number = self.luaq.is_c_true(resp.attach_pdn_list_len) and (resp.attach_pdn_list_len>0) and resp.attach_pdn_list[0] or nil
  end

  if not default_profile_number then
    self.l.log("LOG_DEBUG",string.format("[custom-apn] default LTE attach PDN not found"))
    return false
  end

  a.default_profile_number = default_profile_number

  return true
end

QmiWds.cbs_system={
  "poll",
  "read_rmnet",
  "stop_rmnet",
  "write_rmnet",
  -- query IP information of rmnet
  "get_rmnet_stat",
  "start_rmnet",

  "get_default_profile_number",
  "set_default_profile_number",
  "get_supl_profile_number",
  "set_supl_profile_number",
}

function QmiWds:init()

  self.l.log("LOG_INFO", "initiate qmi_wds")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- initiate qmi


end

return QmiWds
