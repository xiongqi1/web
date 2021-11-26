-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi uim module, SIM information

local QmiUim = require("wmmd.Class"):new()

--[[

  * constants of SIM files are from ETSI TS documents.

]]--

function QmiUim:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_uim", "LOG_DAEMON") end)

  self.rdb = require("luardb")
  self.rdbWatch = rdbWatch
  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")

  self.rdbrpc = require("luardbrpc")
  self.turbo = require("turbo")
  self.ioloop = self.turbo.ioloop.instance()

  self.m = self.luaq.m
  self.e = self.luaq.e
  self.sq = self.smachine.get_smachine("qmi_smachine")
  self.efdir={} -- EFdir directories

-- USIM from 3GPP TS 31.102 version 12.9.0 Release 12
-- ISIM from 3GPP TS 31.103 version 12.2.0 Release 12

  self.sim_file_path_collection={
    iccid        ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x2fe2},},
    msisdn       ={m=self.m.QMI_UIM_READ_RECORD,        efdir="USIM", p={0x3f00,0x7fff,0x6f40}},
    mbdn         ={m=self.m.QMI_UIM_READ_RECORD,        efdir="USIM", p={0x3f00,0x7fff,0x6fc7}},
    hpplmn       ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f31}},
    loci         ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f7e}},
    psloci       ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f73}},
    acc          ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f78}},
    ad           ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6fad}},
    fplmn        ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f7b}},
    pnn          ={m=self.m.QMI_UIM_READ_RECORD,        efdir="USIM", p={0x3f00,0x7fff,0x6fc5}},
    opl          ={m=self.m.QMI_UIM_READ_RECORD,        efdir="USIM", p={0x3f00,0x7fff,0x6fc6}},
    plmnsel      ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f30}},
    -- EF-OPLMNwAcT (Operator controlled PLMN selector with AcT)
    oplmnwact    ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f61}},
    ahplmn       ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x5f30,0x4f34}},
    domain       ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="ISIM", p={0x3f00,0x7fff,0x6f03}},
    impu         ={m=self.m.QMI_UIM_READ_RECORD,        efdir="ISIM", p={0x3f00,0x7fff,0x6f04}},
    impi         ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="ISIM", p={0x3f00,0x7fff,0x6f02}},

    -- EF-HPLMNwACT (Home PLMN selector with AcT)
    hplmnwact    ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f62}},
    -- EF-EHPLMN (Equivalent HPLMN)
    ehplmn       ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6fd9}},
    -- EF-EHPLMNPI (Equivalent HPLMN Presentation Indication)
    ehplmnpi     ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6fdb}},
    -- EF-LRPLMNSI (Last RPLMN Selection Indication)
    lrplmnsi     ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6fdc}},
    -- EF-PLMNwAcT (User controlled PLMN selector with Access Technology)
    plmnwact     ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f60}},

    -- service provider name
    spn          ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6f46}},
    -- service provider display info
    spdi         ={m=self.m.QMI_UIM_READ_TRANSPARENT,   efdir="USIM", p={0x3f00,0x7fff,0x6fcd}},
  }
  -- auto pin that has been tried and failed
  self.failed_auto_pin = nil
  self.MAX_PIN_LEN = 8
  self.MAX_PUK_LEN = 8

  self.RDB_MEP_PREFIX = "sim.mep."
  self.RDB_MEP_PREFIX_FULL = self.config.rdb_g_prefix .. self.RDB_MEP_PREFIX
  self.RPC_MEP_SERVICE_NAME = self.RDB_MEP_PREFIX_FULL:sub(1,-2)

  self.rdb_session = nil
  self.rdbfd = -1
  self.rpc_server = nil
end

QmiUim.card_status_names={
  [0x00] = "absent",
  [0x01] = "present",
  [0x02] = "error",
}

QmiUim.app_state_names={
  [0x00] = "unknown",
  [0x01] = "detected",
  [0x02] = "pin1_or_upin_req",
  [0x03] = "puk1_or_puk_req",
  [0x04] = "person_check_req",
  [0x05] = "pin1_perm_blocked",
  [0x06] = "illegal",
  [0x07] = "ready",
}

QmiUim.perso_state_names = {
  [0x00] = "unknown",
  [0x01] = "in_progress",
  [0x02] = "ready",
  [0x03] = "code_req",
  [0x04] = "puk_req",
  [0x05] = "permanently_blocked",
}

QmiUim.perso_feature_names = {
  [0x00] = "gw_network",
  [0x01] = "gw_network_subset",
  [0x02] = "gw_service_provider",
  [0x03] = "gw_corporate",
  [0x04] = "gw_uim",
  [0x05] = "1x_network_type_1",
  [0x06] = "1x_network_type_2",
  [0x07] = "1x_hrpd",
  [0x08] = "1x_service_provider",
  [0x09] = "1x_corporate",
  [0x0a] = "1x_ruim",
  [0x0b] = "unknown",
}

QmiUim.error_code_names={
  [0x00] = "unknown",
  [0x01] = "power down",
  [0x02] = "poll error",
  [0x03] = "no atr received",
  [0x04] = "volt mismatch",
  [0x05] = "parity error",
  [0x06] = "possibly removed",
  [0x07] = "sim technical problems",
  [0x08] = "null bytes",
  [0x09] = "sap connected",
}

QmiUim.pin_state_names={
  [0x00] = "unknown",
  [0x01] = "enabled_not_verified",
  [0x02] = "enabled_verified",
  [0x03] = "disabled",
  [0x04] = "blocked",
  [0x05] = "permanently_blocked",
}

QmiUim.message_type_names={
  [1] = "voicemail",
  [2] = "fax",
  [3] = "email",
  [4] = "other",
  [5] = "videomail",
}

function QmiUim:build_uim_card_state(resp)
  local ia={}

  if self.luaq.is_c_true(resp.card_status_valid) and (resp.card_status.card_info_len > 0) then
    local card_info = resp.card_status.card_info[0]

    self.l.log("LOG_DEBUG",string.format("[uim_card_state] card_state=%02x, error_code=%02x",tonumber(card_info.card_state), tonumber(card_info.error_code)))
    local sim_not_inserted = (card_info.error_code == "UIM_CARD_ERROR_CODE_NO_ATR_RECEIVED_V01") or (card_info.error_code == "UIM_CARD_ERROR_CODE_POSSIBLY_REMOVED_V01")

    -- set card state
    ia.card_state = self.card_status_names[tonumber(card_info.card_state)]
    ia.error_code = self.error_code_names[tonumber(card_info.error_code)]

    -- set app state
    if card_info.app_info_len>0 then
      local app_info = card_info.app_info[0]
      self.l.log("LOG_DEBUG",string.format("[uim_card_state] app_state=%02x, univ_pin=%02x",tonumber(app_info.app_state),tonumber(app_info.univ_pin)))
      ia.app_state = self.app_state_names[tonumber(app_info.app_state)]

      self.l.log("LOG_DEBUG", string.format("[uim_card_state] perso_state=%02x, perso_feature=%02x, retries=%d, unblock_retries=%d", tonumber(app_info.perso_state), tonumber(app_info.perso_feature), app_info.perso_retries, app_info.perso_unblock_retries))
      ia.perso_state = self.perso_state_names[tonumber(app_info.perso_state)]
      ia.perso_feature = self.perso_feature_names[tonumber(app_info.perso_feature)]
      ia.perso_retries = app_info.perso_retries
      ia.perso_unblock_retries = app_info.perso_unblock_retries

      -- set pin state
      -- FIXME: app_info.univ_pin is a number or a string ?
      local pin = app_info.univ_pin == "UIM_UNIV_PIN_PIN1_USED_V01" and
                  app_info.pin1 or card_info.upin
      ia.pin_state = self.pin_state_names[tonumber(pin.pin_state)]
      ia.pin_retries = pin.pin_retries
      ia.puk_retries = pin.puk_retries
    end
  end
  return ia
end

function QmiUim:min(a,b)
  return (a<b) and a or b
end

function QmiUim:monitor_sim_file_raw(sim_file_path,sim_aid,sim_session_type)
  local qm = self.luaq.new_msg(self.m.QMI_UIM_REFRESH_REGISTER)

  -- build session information
  local session_information = qm.req.session_information

  -- build session information - session type
  session_information.session_type=sim_session_type or "UIM_SESSION_TYPE_PRIMARY_GW_V01"

  -- build session information - aid
  if sim_aid then
    local raw_aid=self.ffi.new("uint8_t[?]",#sim_aid,sim_aid)
    local raw_aid_size=self.ffi.sizeof(raw_aid)
    session_information.aid_len=self:min(raw_aid_size,self.ffi.sizeof(session_information.aid))
    self.ffi.copy(session_information.aid,raw_aid,session_information.aid_len)
  else
    session_information.aid_len=0
  end

  -- build register fresh
  local register_refresh = qm.req.register_refresh
  local sim_file_id=sim_file_path[#sim_file_path]
  register_refresh.register_flag=1
  register_refresh.vote_for_init=0
  register_refresh.files_len=1
  register_refresh.files[0].file_id=sim_file_id

  -- build register fresh - sim file path
  local rfile = register_refresh.files[0]
  local sim_path_only = {unpack(sim_file_path,1,#sim_file_path-1)}
  local file_path_raw=self.ffi.new("short[?]",#sim_path_only,sim_path_only)
  local file_path_raw_size=self.ffi.sizeof(file_path_raw)
  rfile.path_len=self:min(file_path_raw_size,self.ffi.sizeof(rfile.path))
  self.ffi.copy(rfile.path,file_path_raw,rfile.path_len)

  return self.luaq.send_msg(qm)
end

function QmiUim:monitor_sim_file(sim_file_path,efdir_slot)
  local aid
  local session_type

  -- get aid and session type
  if efdir_slot and self.efdir[efdir_slot] then
    aid = self.efdir[efdir_slot].aid
    session_type = self.efdir[efdir_slot].session_type
  end

  return self:monitor_sim_file_raw(sim_file_path,aid,session_type)
end

function QmiUim:write_sim_file(qmi_cmd,sim_file_path,content,sim_record_idx,sim_aid,sim_session_type)
  local qm = self.luaq.new_msg(qmi_cmd)

  local sim_file_id=sim_file_path[#sim_file_path]

  -- build request
  qm.req.session_information.session_type=sim_session_type or "UIM_SESSION_TYPE_PRIMARY_GW_V01"
  qm.req.file_id.file_id=sim_file_id

  self.l.log("LOG_DEBUG",string.format("write sim file (sim_file_id=0x%04x)",sim_file_id))

  -- build aid
  if sim_aid then
    local raw_aid=self.ffi.new("uint8_t[?]",#sim_aid,sim_aid)
    local raw_aid_size=self.ffi.sizeof(raw_aid)
    qm.req.session_information.aid_len=self:min(raw_aid_size,self.ffi.sizeof(qm.req.session_information.aid))
    self.ffi.copy(qm.req.session_information.aid,raw_aid,qm.req.session_information.aid_len)
  else
    qm.req.session_information.aid_len=0
  end

  local raw_data=self.ffi.new("uint8_t[?]",#content,content)
  local raw_data_size = self.ffi.sizeof(raw_data)

  if qm.c_struct_req == "uim_write_record_req_msg_v01" then
    qm.req.write_record.record=sim_record_idx or 1
    qm.req.write_record.data_len=self:min(raw_data_size,self.ffi.sizeof(qm.req.write_record.data))
    self.ffi.copy(qm.req.write_record.data,raw_data,qm.req.write_record.data_len)
  elseif qm.c_struct_req == "uim_write_transparent_req_msg_v01" then
    qm.req.write_transparent.offset=0
    qm.req.write_transparent.data_len=self:min(raw_data_size,self.ffi.sizeof(qm.req.write_transparent.data))
    self.ffi.copy(qm.req.write_transparent.data,raw_data,qm.req.write_transparent.data_len)
  end

  -- build sim file path
  local sim_path_only = {unpack(sim_file_path,1,#sim_file_path-1)}
  local file_path_raw=self.ffi.new("short[?]",#sim_path_only,sim_path_only)
  local file_path_raw_size=self.ffi.sizeof(file_path_raw)
  qm.req.file_id.path_len=self:min(file_path_raw_size,self.ffi.sizeof(qm.req.file_id.path))
  self.ffi.copy(qm.req.file_id.path,file_path_raw,qm.req.file_id.path_len)

  -- send sync
  return self.luaq.send_msg(qm)
end

function QmiUim:read_sim_file(qmi_cmd,sim_file_path,sim_record_idx,sim_aid,sim_session_type)
  local qm = self.luaq.new_msg(qmi_cmd)

  local sim_file_id=sim_file_path[#sim_file_path]

  -- build request
  qm.req.session_information.session_type=sim_session_type or "UIM_SESSION_TYPE_PRIMARY_GW_V01"
  qm.req.file_id.file_id=sim_file_id

  self.l.log("LOG_DEBUG",string.format("read sim file (sim_file_id=0x%04x)",sim_file_id))

  -- build aid
  if sim_aid then
    local raw_aid=self.ffi.new("uint8_t[?]",#sim_aid,sim_aid)
    local raw_aid_size=self.ffi.sizeof(raw_aid)
    qm.req.session_information.aid_len=self:min(raw_aid_size,self.ffi.sizeof(qm.req.session_information.aid))
    self.ffi.copy(qm.req.session_information.aid,raw_aid,qm.req.session_information.aid_len)
  else
    qm.req.session_information.aid_len=0
  end

  if qm.c_struct_req == "uim_read_record_req_msg_v01" then
    qm.req.read_record.record=sim_record_idx or 1
    qm.req.read_record.length=0
  elseif qm.c_struct_req == "uim_read_transparent_req_msg_v01" then
    qm.req.read_transparent.offset=0
    qm.req.read_transparent.length=0
  end

  -- build sim file path
  local sim_path_only = {unpack(sim_file_path,1,#sim_file_path-1)}
  local file_path_raw=self.ffi.new("short[?]",#sim_path_only,sim_path_only)
  local file_path_raw_size=self.ffi.sizeof(file_path_raw)
  qm.req.file_id.path_len=self:min(file_path_raw_size,self.ffi.sizeof(qm.req.file_id.path))
  self.ffi.copy(qm.req.file_id.path,file_path_raw,qm.req.file_id.path_len)

  -- send sync
  self.luaq.send_msg(qm)

  local content

  if qm.resp.resp.result == "QMI_RESULT_SUCCESS_V01" then
    content={}

    for i=0,qm.resp.read_result.content_len-1 do
      table.insert(content,tonumber(qm.resp.read_result.content[i]))
    end
  end


  return content
end


function QmiUim:convert_to_string(content)
  local str={}

  for _,v in ipairs(content) do
    table.insert(str,string.char(v))
  end

  return table.concat(str)
end

function QmiUim:convert_to_hexadecimal(content)
  local hex={}

  if content and type(content) == "table" then
    for _,v in ipairs(content) do
      table.insert(hex,string.format("%02X",v))
    end
  end

  return table.concat(hex)
end

function QmiUim:sim_select(cmd_rdb, cmd)
  local UIM_SLOT_DEFAULT = "1"
  local qm
  local rdb_selection = self.wrdb:getp("sim.slot_select")
  local sim_selection
  local slot_priority = {}
  local sim_present = {}

  -- create the user rdb prioritization array
  if rdb_selection == nil or rdb_selection == "" or
     rdb_selection < "1" or rdb_selection > "5" then
    -- selection invalid, use default
    rdb_selection = UIM_SLOT_DEFAULT
    self.wrdb:setp("sim.slot_select",UIM_SLOT_DEFAULT)
  end
  local i = 1
  for slot in string.gmatch(rdb_selection, "([^,]+)") do
    slot_priority[i] = tonumber(slot)
    i = i+1
  end

  -- get SIM slot status from modem for up to 5 SIMs
  local qm2 = self.luaq.new_msg(self.luaq.m.QMI_UIM_GET_SLOTS_STATUS)
  if not self.luaq.send_msg(qm2) then
    return false
  end
  local resp = qm2.resp
  -- iccid not present indicates SIM is not present
  if resp.resp.result == "QMI_RESULT_SUCCESS_V01" then
    for slot = 0,4 do
      if resp.physical_slot_status[slot].iccid_len ~= 0 then
        sim_present[slot+1] = true
      end
    end
  else
    return false
  end

  -- use sim_present[] and slot_priority[] to decide which SIM to use
  -- default to highest priority SIM if no SIMs are present
  sim_selection = slot_priority[1]
  for _,v in ipairs(slot_priority) do
    if sim_present[v] then
      sim_selection = v
      break
    end
  end

  qm = self.luaq.new_msg(self.luaq.m.QMI_UIM_SWITCH_SLOT)
  if type(qm) == "table" then
    qm.req.logical_slot = 1
    qm.req.physical_slot = sim_selection
    if not self.luaq.send_msg(qm) then
      self.l.log("LOG_ERR","Failed to send QMI_UIM_SWITCH_SLOT req")
      return false
    end
    resp = qm.resp
    if resp.resp.result == "QMI_RESULT_SUCCESS_V01" then
      self.l.log("LOG_DEBUG","[activate_sim] succeeded")
      return true
    end
    self.l.log("LOG_ERR","Failed to change SIM via QMI_UIM_SWITCH_SLOT req")
    return false
  else
    self.l.log("LOG_ERR","Failed to send QMI_UIM_SWITCH_SLOT req")
    return false
  end
end

function QmiUim:init_efdir()

  local rec_idx

  self.efdir={}

  rec_idx=1
  while true do
    local content = self:read_sim_file(self.m.QMI_UIM_READ_RECORD,{0x3f00,0x2f00},rec_idx)
    if not content then
      break
    end

    -- if application template
    if (content[1] == 0x61) and (content[3] == 0x4f) then
      local aid = {unpack(content,5, 5 + content[4] - 1)}
      local aid_str = self:convert_to_hexadecimal(aid)

      if aid_str:match("^A0000000871004") then
        self.efdir["ISIM"]={aid=aid,session_type="UIM_SESSION_TYPE_NONPROVISIONING_SLOT_1_V01"}
        self.l.log("LOG_DEBUG",string.format("ISIM aid = %s",aid_str))
      end


    end

    rec_idx=rec_idx+1
  end

end


function QmiUim:read_sim_file_hexadecimal(qmi_command,file_path,efdir_slot)

  local aid
  local session_type

  -- get aid and session type
  if efdir_slot and self.efdir[efdir_slot] then
    aid = self.efdir[efdir_slot].aid
    session_type = self.efdir[efdir_slot].session_type
  end

  self.l.log("LOG_DEBUG",string.format("[raw-sim-info] got AID - file_id=0x%04x,aid=%s",file_path[#file_path],aid and self:convert_to_hexadecimal(aid) or "N/A" ))

  if qmi_command == self.m.QMI_UIM_READ_RECORD then
    local rec_idx = 1
    local records={}

    while true do
      local content =  self:read_sim_file(qmi_command,file_path,rec_idx,aid,session_type)
      if not content then
        break
      end

      table.insert(records,self:convert_to_hexadecimal(content))
      rec_idx = rec_idx + 1
    end

    return records
  else

    return self:convert_to_hexadecimal(self:read_sim_file(qmi_command,file_path,0,aid,session_type) or {})
  end

end


function QmiUim:break_to_nibbles(content)

  local nibbles={}

  local l,h

  for _,v in ipairs(content) do
    l = self.bit.band(v,0x0f)
    h = self.bit.band(self.bit.rshift(v,4),0x0f)

    if l ~= 0x0f then
      table.insert(nibbles,l)
    end

    if h ~= 0x0f then
      table.insert(nibbles,h)
    end

  end

  return nibbles
end



function QmiUim:convert_nibbles_to_digits(nibbles)

  -- convert BCD digit to char
  local function convert_bcd_nibble_to_digit_char(digit)
    if digit<0x0a then
      digit=digit + string.byte("0")
    else
      digit=digit + string.byte("A")
    end

    return string.char(digit)
  end

  local digits={}

  -- convert nibbles to digits
  for _,v in ipairs(nibbles) do
    table.insert(digits,convert_bcd_nibble_to_digit_char(v))
  end

  return table.concat(digits)
end

function QmiUim:read_iccid()
  local content=self:read_sim_file(self.m.QMI_UIM_READ_TRANSPARENT,{0x3f00,0x2fe2})
  if not content then
    return
  end

  local iccid_nibbles = self:break_to_nibbles(content)

  return table.concat(iccid_nibbles)
end

function QmiUim:read_imsi()
  local content=self:read_sim_file(self.m.QMI_UIM_READ_TRANSPARENT,{0x3f00,0x7fff,0x6F07})
  if not content then
    return
  end

  -- reassemble after removing length
  local length = content[1]
  local imsi_content = {unpack(content,2,length+1)}

  -- break into imsi nibbles
  local imsi_nibbles = self:break_to_nibbles(imsi_content)
  -- remove parity TS 31.102
  table.remove(imsi_nibbles,1)

  return table.concat(imsi_nibbles)
end

function QmiUim:write_sim_file_mwi(type,event,newmwi)
  local content={}

  local mwi_stat = 0
  local mwi_mask = 1
  local mwi = self:read_sim_file_mwi() or {}

  -- put mwi state
  table.insert(content,mwi_stat)
  -- build rest part of content and update changed message types only
  -- while keeping unchanged message type records
  for _,k in ipairs(self.message_type_names) do
    if newmwi[k] then
      mwi[k] = newmwi[k]
    end
    if mwi[k] and mwi[k].active then
      mwi_stat = self.bit.bor(mwi_stat, mwi_mask)
    end

    local count = mwi[k] and mwi[k].count or 0
    table.insert(content, count)

    mwi_mask = self.bit.lshift(mwi_mask,1)
  end

  content[1] = mwi_stat

  self.l.log("LOG_DEBUG",string.format("#content=%d",#content))
  for i,v in ipairs(content) do
    self.l.log("LOG_DEBUG",string.format("msg[%d]=0x%02x",i,v))
  end

  return self:write_sim_file(self.m.QMI_UIM_WRITE_RECORD,{0x3f00,0x7fff,0x6fca},content)
end

function QmiUim:read_sim_file_mwi()
  local content = self:read_sim_file(self.m.QMI_UIM_READ_RECORD,{0x3f00,0x7fff,0x6fca})
  if not content then
    return nil
  end

  for i,v in ipairs(content) do
    self.l.log("LOG_DEBUG",string.format("msg[%d]=0x%02x",i,v))
  end

  -- 3gpp EFmwis specification
  local mwi={}

  local mwi_stat = tonumber(content[1])
  local mwi_mask = 1
  for i,k in ipairs(self.message_type_names) do

    mwi[k]={
      active = self.bit.band(mwi_stat,mwi_mask) ~= 0,
      count = tonumber(content[i+1] or 0)
    }

    self.l.log("LOG_DEBUG",string.format("mwi[%s] active=%s, count=%d",k,mwi[k].active,mwi[k].count))

    mwi_mask = self.bit.lshift(mwi_mask,1)
  end

  return mwi

end

function QmiUim:read_sim_file_mbdn_related(qmi_command,file_path)
  local content=self:read_sim_file(qmi_command,file_path)
  if not content then
    return
  end

  --[[
  local content={
    0x00,0x00,0x00,0x00,
    0x00,
    0x01,
    0x04,0x13,0x23,0x75,0x92,0xff,0xff,0xff,0xff,0xff,
    0x00,
    0x00,
    }
]]--

  local x = #content - 14
  local ton = tonumber(content[x+2])
  local dial_number = {unpack(content,x+3,x+12)}
  local dial_str = self:convert_nibbles_to_digits(self:break_to_nibbles(dial_number))

  if dial_str == "" then
    return
  end

  local prefix = (self.bit.band(ton,0x01) ~= 0) and "+" or ""

  return prefix .. dial_str
end

function QmiUim:read_msisdn()
  return self:read_sim_file_mbdn_related(self.m.QMI_UIM_READ_RECORD,{0x3f00,0x7fff,0x6f40})
end

function QmiUim:read_mbn()
  return self:read_sim_file_mbdn_related(self.m.QMI_UIM_READ_RECORD,{0x3f00,0x7fff,0x6f17})
end

function QmiUim:read_mbdn()
  return self:read_sim_file_mbdn_related(self.m.QMI_UIM_READ_RECORD,{0x3f00,0x7fff,0x6fc7})
end

function QmiUim:read_adn()
  return self:read_sim_file_mbdn_related(self.m.QMI_UIM_READ_RECORD,{0x3f00,0x7fff,0x6f3a})
end

function QmiUim:QMI_UIM_REFRESH_IND(type,event,qm)
  self.l.log("LOG_INFO","mbdn changed, re-read SIM card information")
  self.watcher.invoke("sys","poll_uim_card_status")
  self.watcher.invoke("sys","poll_simcard_info")

  if not self.luaq.is_c_true(qm.resp.refresh_event_valid) then
    self.l.log("LOG_INFO","QMI_UIM_REFRESH_IND skip REFRESH_COMPLETE - no valid refresh event")
    return true
  end

  -- REFRESH_COMPLETE may need to be sent when this indication is received.
  -- See Qualcomm doc 80-NV615-12 Table C-1 for the full conditions where
  -- REFRESH_COMPLETE needs to be sent.
  local refresh_event=qm.resp.refresh_event
  local mode=refresh_event.mode
  local sess_type=refresh_event.session_type
  local is_stage_start=(refresh_event.stage == "UIM_REFRESH_STAGE_START_V01")
  local is_nonprov_sess=(sess_type == "UIM_SESSION_TYPE_NONPROVISIONING_SLOT_1_V01" or
                         sess_type == "UIM_SESSION_TYPE_NONPROVISIONING_SLOT_2_V01")

  if is_stage_start and (mode == "UIM_REFRESH_MODE_FCN_V01" or
     (is_nonprov_sess and (mode == "UIM_REFRESH_MODE_INIT_V01" or
     mode == "UIM_REFRESH_MODE_INIT_FCN_V01" or
     mode == "UIM_REFRESH_MODE_INIT_FULL_FCN_V01" or
     mode == "UIM_REFRESH_MODE_3G_RESET_V01"))) then

    self.l.log("LOG_INFO",string.format("QMI_UIM_REFRESH_IND send REFRESH_COMPLETE for stage=0x%02x mode=0x%02x session=0x%02x",
               tonumber(refresh_event.stage), tonumber(mode), tonumber(refresh_event.session_type)))

    local qm2 = self.luaq.new_msg(self.luaq.m.QMI_UIM_REFRESH_COMPLETE)
    qm2.req.session_information.session_type=refresh_event.session_type
    qm2.req.session_information.aid_len=refresh_event.aid_len
    qm2.req.session_information.aid=refresh_event.aid
    qm2.req.refresh_success=1

    if not self.luaq.send_msg(qm2) then
      self.l.log("LOG_ERR", "QMI_UIM_REFRESH_COMPLETE send_msg failed")
    else
      local succ, qerr, resp = self.luaq.ret_qm_resp(qm2)
      if not succ then
        self.l.log("LOG_ERR", string.format("QMI_UIM_REFRESH_COMPLETE failed, error=0x%02x", qerr))
      else
        self.l.log("LOG_INFO", "QMI_UIM_REFRESH_COMPLETE succeeded")
      end
    end
  else
    self.l.log("LOG_INFO",string.format("QMI_UIM_REFRESH_IND skip REFRESH_COMPLETE - not needed for stage=0x%02x mode=0x%02x session=0x%02x",
               tonumber(refresh_event.stage), tonumber(mode), tonumber(refresh_event.session_type)))
  end

  return true
end

function QmiUim:QMI_UIM_CARD_ACTIVATION_STATUS_IND(type,event,qm)
  self.l.log("LOG_INFO","OTA activation changed, re-read SIM card information")
  self.watcher.invoke("sys","poll_uim_card_status")
  self.watcher.invoke("sys","poll_simcard_info")

  return true
end

function QmiUim:QMI_UIM_SLOT_STATUS_CHANGE_IND(type,event,qm)
  return true
end

function QmiUim:QMI_UIM_STATUS_CHANGE_IND(type,event,qm)
  local ia=self:build_uim_card_state(qm.resp)
  return self.watcher.invoke("sys","modem_on_uim_card_status",ia)
end

QmiUim.cbs={
  "QMI_UIM_REFRESH_IND",
  "QMI_UIM_CARD_ACTIVATION_STATUS_IND",
  "QMI_UIM_SLOT_STATUS_CHANGE_IND",
  "QMI_UIM_STATUS_CHANGE_IND",
}

function QmiUim:pin_control(pin, action)
  local qm
  if #pin > self.MAX_PIN_LEN then
    self.l.log("LOG_ERR",string.format("[pin_control] PIN is too long (len=%d)", #pin))
    return false
  end
  self.l.log("LOG_DEBUG",string.format("[pin_control] pin=%s, action=%s",pin,action))
  if action == "verifypin" then
    qm = self.luaq.new_msg(self.luaq.m.QMI_UIM_VERIFY_PIN)
    qm.req.session_information.session_type="UIM_SESSION_TYPE_PRIMARY_GW_V01"
    qm.req.session_information.aid_len=0
    qm.req.verify_pin.pin_id="UIM_PIN_ID_PIN_1_V01"
    qm.req.verify_pin.pin_value_len=#pin
    self.ffi.copy(qm.req.verify_pin.pin_value,pin,#pin)
  else -- enable or disable pin
    qm = self.luaq.new_msg(self.luaq.m.QMI_UIM_SET_PIN_PROTECTION)
    qm.req.session_information.session_type="UIM_SESSION_TYPE_PRIMARY_GW_V01"
    qm.req.session_information.aid_len=0
    qm.req.set_pin_protection.pin_id="UIM_PIN_ID_PIN_1_V01"
    qm.req.set_pin_protection.pin_operation = action=="enable" and "UIM_PIN_OPERATION_ENABLE_V01" or "UIM_PIN_OPERATION_DISABLE_V01"
    qm.req.set_pin_protection.pin_value_len=#pin
    self.ffi.copy(qm.req.set_pin_protection.pin_value,pin,#pin)
  end
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR",string.format("Failed to send req %s", action=="verifypin" and "QMI_UIM_VERIFY_PIN" or "QMI_UIM_SET_PIN_PROTECTION"))
    return false
  end
  local resp = qm.resp
  if resp.resp.result == "QMI_RESULT_SUCCESS_V01" then
    self.l.log("LOG_INFO",string.format("[pin_control] %s succeeded",action))
    return true
  end
  if self.luaq.is_c_true(resp.retries_left_valid) then
    self.l.log("LOG_WARNING",string.format("[pin_control] %s failed. error=0x%04X, verify_left=%d, unblock_left=%d",action,tonumber(resp.resp.error),resp.retries_left.verify_left,resp.retries_left.unblock_left))
    return false, resp.resp.error, resp.retries_left.verify_left, resp.retries_left.unblock_left
  end
  self.l.log("LOG_WARNING",string.format("[pin_control] %s failed. error=0x%04X",action,tonumber(resp.resp.error)))
  return false, resp.resp.error
end

function QmiUim:pin_change(old_pin, new_pin)
  local qm
  if #old_pin > self.MAX_PIN_LEN then
    self.l.log("LOG_ERR",string.format("[pin_change] old PIN is too long (len=%d)", #old_pin))
    return false
  end
  if #new_pin > self.MAX_PIN_LEN then
    self.l.log("LOG_ERR",string.format("[pin_change] new PIN is too long (len=%d)", #new_pin))
    return false
  end

  qm = self.luaq.new_msg(self.luaq.m.QMI_UIM_CHANGE_PIN)
  qm.req.session_information.session_type="UIM_SESSION_TYPE_PRIMARY_GW_V01"
  qm.req.session_information.aid_len=0
  qm.req.change_pin.pin_id="UIM_PIN_ID_PIN_1_V01"
  qm.req.change_pin.old_pin_value_len=#old_pin
  self.ffi.copy(qm.req.change_pin.old_pin_value,old_pin,#old_pin)
  qm.req.change_pin.new_pin_value_len=#new_pin
  self.ffi.copy(qm.req.change_pin.new_pin_value,new_pin,#new_pin)
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR","Failed to send req QMI_UIM_CHANGE_PIN")
    return false
  end
  local resp = qm.resp
  if resp.resp.result == "QMI_RESULT_SUCCESS_V01" then
    self.l.log("LOG_INFO","[pin_change] succeeded")
    return true
  end
  if self.luaq.is_c_true(resp.retries_left_valid) then
    self.l.log("LOG_WARNING",string.format("[pin_change] failed. verify_left=%d, unblock_left=%d",resp.retries_left.verify_left,resp.retries_left.unblock_left))
    return false, resp.retries_left.verify_left, resp.retries_left.unblock_left
  end
end

function QmiUim:pin_unblock(puk, new_pin)
  local qm
  if #new_pin > self.MAX_PIN_LEN then
    self.l.log("LOG_ERR",string.format("[pin_unblock] new PIN is too long (len=%d)", #new_pin))
    return false
  end
  if #puk > self.MAX_PUK_LEN then
    self.l.log("LOG_ERR",string.format("[pin_unblock] PUK is too long (len=%d)", #puk))
    return false
  end

  qm = self.luaq.new_msg(self.luaq.m.QMI_UIM_UNBLOCK_PIN)
  qm.req.session_information.session_type="UIM_SESSION_TYPE_PRIMARY_GW_V01"
  qm.req.session_information.aid_len=0
  qm.req.unblock_pin.pin_id="UIM_PIN_ID_PIN_1_V01"
  qm.req.unblock_pin.puk_value_len=#puk
  self.ffi.copy(qm.req.unblock_pin.puk_value,puk,#puk)
  qm.req.unblock_pin.new_pin_value_len=#new_pin
  self.ffi.copy(qm.req.unblock_pin.new_pin_value,new_pin,#new_pin)
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR","Failed to send req QMI_UIM_UNBLOCK_PIN")
    return false
  end
  local resp = qm.resp
  if resp.resp.result == "QMI_RESULT_SUCCESS_V01" then
    self.l.log("LOG_INFO","[pin_unblock] succeeded")
    return true
  end
  if self.luaq.is_c_true(resp.retries_left_valid) then
    self.l.log("LOG_WARNING",string.format("[pin_unblock] failed. verify_left=%d, unblock_left=%d",resp.retries_left.verify_left,resp.retries_left.unblock_left))
    return false, resp.retries_left.verify_left, resp.retries_left.unblock_left
  end
end

function QmiUim:poll_simcard_raw_info(_type, event)

  local ia={}
  local content

  self.l.log("LOG_INFO","* poll simcard raw info.")

  for k,v in pairs(self.sim_file_path_collection) do

    self.l.log("LOG_DEBUG",string.format("[raw-sim-info] * EFdir=%s,file_id=0x%04x,file_name=%s",v.efdir,v.p[#v.p],k))

    content = self:read_sim_file_hexadecimal(v.m, v.p, v.efdir)
    ia[k] = content

    self.l.log("LOG_DEBUG",string.format("[raw-sim-info] got SIM data - EFdir=%s,file_id=0x%04x,file_name=%s,len=%d,type=%s",v.efdir,v.p[#v.p],k,#content,type(content)))
  end

  return self.watcher.invoke("sys","modem_on_simcard_raw_info",ia)
end

function QmiUim:poll_simcard_info(type, event)

  local msisdn = self:read_msisdn()
  local activation = msisdn ~= nil

  local ia ={
    iccid=self:read_iccid(),
    imsi=self:read_imsi(),
    msisdn=msisdn,
    mbn=self:read_mbn(),
    mbdn=self:read_mbdn(),
    adn=self:read_adn(),
    activation=activation,
  }

  self.l.log("LOG_INFO",string.format("* sim information"))
  self.l.log("LOG_INFO",string.format("iccid = %s",ia.iccid))
  self.l.log("LOG_INFO",string.format("imsi = %s",ia.imsi))
  self.l.log("LOG_INFO",string.format("msisdn = %s",ia.msisdn))

  self.watcher.invoke("sys","modem_on_simcard_info",ia)

  -- read MWI from SIM and update MWI status
  self.l.log("LOG_INFO","* read mwi")
  local mwi = self:read_sim_file_mwi()
  if mwi then
    self.l.log("LOG_INFO",string.format("voicemail sim file %s,%d",mwi.voicemail.active,mwi.voicemail.count))
    self.watcher.invoke("sys","modem_on_mwi",mwi)
  else
    --[[ ATT FR-21606:
    The handset is not required to maintain the message count if the SIM does not
    support the message count capability (i.e., 3G MWI files). There exists the possibility that
    the SIM could be removed, placed in another handset, the voice mail number is changed,
    and then the SIM is reinserted back in to the original handset. After the handset is power
    cycled, it shall just indicate the presence of voice mails only.
    ]]--
    self.l.log("LOG_INFO", "Failed to read MWI sim file")
    self.watcher.invoke("sys","sync_mwi_count")
  end
  return true
end

-------------------------------------------------------------------------------
--power down or up all SIM slots
--
--@param power_stat true to power on and false to power off
--@return true when it succeeds. Otherwise, false.
function QmiUim:sim_power_up_down(power_stat)

  -- get all of phyiscal sim card status
  local succ,err,resp=self.luaq.req(self.m.QMI_UIM_GET_SLOTS_STATUS)

  if not succ then
    self.l.log("LOG_ERR", "failed to get SIM card status")
    return false
  end

  local no_of_slots = self.luaq.is_c_true(resp.physical_slot_status_valid) and tonumber(resp.physical_slot_status_len) or 0
  self.l.log("LOG_DEBUG", string.format("total number of read SIM card : %d", no_of_slots))

  if no_of_slots>0 then
    for i = 0,no_of_slots-1 do
      local slot = resp.physical_slot_status[i]
      self.l.log("LOG_DEBUG", string.format("sim slot info (slot=%d,card_stat=%d)",i,tonumber(slot.physical_card_status)))
      self.l.log("LOG_DEBUG", string.format("sim slot info (slot=%d,slot_stat=%d)",i,tonumber(slot.physical_slot_state)))
      self.l.log("LOG_DEBUG", string.format("sim slot info (slot=%d,ccid_len=%d)",i,tonumber(slot.iccid_len)))

      succ = self.luaq.req(power_stat and self.m.QMI_UIM_POWER_UP or self.m.QMI_UIM_POWER_DOWN,{slot=i+1})
      if not succ then
        self.l.log("LOG_ERR", string.format("failed to power %s sim card (slot=%d)",power_stat and "up" and "down",i))
      end
    end
  end

  return succ
end

-------------------------------------------------------------------------------
--[invoke] power up all physical SIM slots
--
--@param type watcher invoke type. it is always "sys" for this function
--@return true when it succeeds. Otherwise, false.
function QmiUim:sim_power_up(type,event)
  return self:sim_power_up_down(true)
end

-------------------------------------------------------------------------------
--[invoke] power down all physical SIM slots
--
--@param type watcher invoke type. it is always "sys" for this function
--@return true when it succeeds. Otherwise, false.
function QmiUim:sim_power_down(type,event)
  return self:sim_power_up_down(false)
end

function QmiUim:poll_uim_card_status(type,event)
  local succ,err,resp=self.luaq.req(self.m.QMI_UIM_GET_CARD_STATUS)

  local ia=self:build_uim_card_state(resp)

  return self.watcher.invoke("sys","modem_on_uim_card_status",ia)
end

function QmiUim:poll(type,event)
  self.l.log("LOG_DEBUG","qmi uim poll")

  -- poll for SIM PIN
  -- As to support SIM card hot-swap, we need to poll and lock SIM PIN.
  local sim_status = self.wrdb:getp("sim.status.status")
  if sim_status ~= "SIM OK" then
    self.watcher.invoke("sys","poll_uim_card_status")
  end

  return true
end

function QmiUim:auto_pin_verify(type,event)
  local pin = self.wrdb:getp("sim.pin")
  if not pin or pin == "" then
    self.l.log("LOG_WARNING","No stored pin. Auto pin verify skipped")
    return false
  end
  if pin == self.failed_auto_pin then
    self.l.log("LOG_INFO","This stored pin has been tried and failed before. Skipped")
    return false
  end
  local succ, err = self:pin_control(pin,"verifypin")
  if succ then
    self.failed_auto_pin = nil
    return succ
  end
  if err == "QMI_ERR_INCORRECT_PIN_V01" then
    -- mark so that we do not try wrong pin again
    self.failed_auto_pin = pin
  end
  return false
end

QmiUim.cbs_system={
  "poll_simcard_raw_info",
  "poll_simcard_info",
  "poll_uim_card_status",
  "auto_pin_verify",
  "write_sim_file_mwi",
  "sim_power_down",
  "sim_power_up",
}

function QmiUim:pin_ops(cmd_rdb, cmd)
  local succ, pin, new_pin, puk
  self.l.log("LOG_DEBUG","SIM command '"..tostring(cmd).."' triggered")
  if cmd == "enablepin" then -- both 'enablepin' and 'enable' are acceptable
    cmd = "enable"
  elseif cmd == "disablepin" then
    cmd = "disable"
  end
  if cmd == "check" then
    succ = self.watcher.invoke("sys","poll_uim_card_status")
  elseif cmd == "enable" or cmd == "disable" or cmd == "verifypin" then
    pin = self.wrdb:getp("sim.cmd.param.pin")
    local pin_enabled = self.wrdb:getp("sim.status.pin_enabled")
    if cmd == "enable" and pin_enabled == "Enabled" then
      -- if already enabled, disable first to check pin correctness
      -- enable an already enabled PIN is illegal
      succ = self:pin_control(pin,"disable")
      if succ then
        succ = self:pin_control(pin,"enable")
      end
    elseif cmd == "disable" and pin_enabled == "Disabled" then
      -- if already disabled, enable first to check pin correctness
      -- disable an already disabled PIN is illegal
      succ = self:pin_control(pin,"enable")
      if succ then
        succ = self:pin_control(pin,"disable")
      end
    else
      succ = self:pin_control(pin,cmd)
    end
  elseif cmd == "changepin" then
    pin = self.wrdb:getp("sim.cmd.param.pin")
    new_pin = self.wrdb:getp("sim.cmd.param.newpin")
    succ = self:pin_change(pin, new_pin)
  elseif cmd == "verifypuk" then
    puk = self.wrdb:getp("sim.cmd.param.puk")
    new_pin = self.wrdb:getp("sim.cmd.param.newpin")
    succ = self:pin_unblock(puk, new_pin)
  else
    self.l.log("LOG_WARNING","Unknown SIM command '"..tostring(cmd).."'")
  end
  self.wrdb:setp("sim.cmd.status", succ and "[done]" or "[error]")
end

--[[
    Parse a MCCMNC string into a pair (MCC, MNC)

    @param mccmnc A string of 5 or 6 characters where the first 3 are MCC
    and the last 2 or 3 are MNC
    @return MCC (3 characters), MNC (2 or 3 characters).
    If input string is invalid, nil is returned
--]]
local function parse_mccmnc(mccmnc)
  if #mccmnc == 5 or #mccmnc == 6 then
    return mccmnc:sub(1,3), mccmnc:sub(4)
  end
end

--[[
    activate MEP lock (personalization)

    @param nck Network control key
    @param mccmnc_list A comma separated list of MCC/MNC codes to lock to
    @param retries The max number of unlocking attempts before blocking device
    @return true for success; false or nil for failure
    @note Qualcomm does not support personalization from AP (QC#03656775), even
    though QMI UIM spec contains the command. So this function will always
    fail. Personalization needs to be done using UIM DIAG interface, e.g. using
    QxDM.
--]]
local mep_lock_timeout = 15000 -- 15 seconds.
function QmiUim:mep_activate(nck, mccmnc_list, retries)
  local succ, qerr, resp
  retries = retries or 0
  if #nck > 16 then
    self.l.log("LOG_ERR", string.format("nck is too long: %d", #nck))
    return
  end

  local qm = self.luaq.new_msg(self.m.QMI_UIM_PERSONALIZATION)
  qm.req.ck_value_len = #nck
  self.ffi.copy(qm.req.ck_value, nck, #nck)
  local plmns = mccmnc_list:split(",")
  if #plmns > 85 then
    -- 85 is qualcomm implemented limitation
    self.l.log("LOG_ERR", "Too many mccmnc: " .. tostring(mccmnc_list))
    return
  end
  qm.req.feature_gw_network_perso_valid = 1
  qm.req.feature_gw_network_perso_len = #plmns
  for i = 1, #plmns do
    local mcc, mnc = parse_mccmnc(plmns[i])
    if not mcc then
      self.l.log("LOG_ERR", "Illegal mccmnc: " .. plmns[i])
      return
    end
    self.ffi.copy(qm.req.feature_gw_network_perso[i-1].mcc, mcc, #mcc)
    qm.req.feature_gw_network_perso[i-1].mnc_len = #mnc
    self.ffi.copy(qm.req.feature_gw_network_perso[i-1].mnc, mnc, #mnc)
  end
  qm.req.num_retries = retries

  if not self.luaq.send_msg(qm, mep_lock_timeout) then
    self.l.log("LOG_ERR", "QMI_UIM_PERSONALIZATION send_msg failed")
    succ = false
  else
    succ, qerr, resp = self.luaq.ret_qm_resp(qm)
    if not succ then
      self.l.log("LOG_ERR", string.format("QMI_UIM_PERSONALIZATION failed, error=0x%02x", qerr))
    else
      self.l.log("LOG_NOTICE", "QMI_UIM_PERSONALIZATION succeeded")
    end
  end
  return succ
end

--[[
    deactivate MEP lock (depersonalization)

    @param nck Network control key
    @param feature The personalization feature to deactivate or unblock: 0-10.
    Please refer to QmiUim.perso_feature_names for details.
    Default to 0 - GW network personalization
    @param operation The operation to perform: 0 for deactivating;
    1 for unblocking. Default to 0
    @param slot The UIM slot number to be used: 1-5, default to 1
    @return true for success; false or nil for failure
--]]
function QmiUim:mep_deactivate(nck, feature, operation, slot)
  local succ, qerr, resp
  if #nck > 16 then
    self.l.log("LOG_ERR", string.format("nck is too long: %d", #nck))
    return
  end
  local qm = self.luaq.new_msg(self.m.QMI_UIM_DEPERSONALIZATION)

  qm.req.depersonalization.feature = feature or "UIM_PERSO_FEATURE_GW_NETWORK_V01"
  qm.req.depersonalization.operation = operation or "UIM_PERSO_OPERATION_DEACTIVATE_V01"
  qm.req.depersonalization.ck_value_len = #nck
  self.ffi.copy(qm.req.depersonalization.ck_value, nck, #nck)
  if slot then
    qm.req.slot_valid = 1
    qm.req.slot = slot
  end
  if not self.luaq.send_msg(qm, mep_lock_timeout) then
    self.l.log("LOG_ERR", "QMI_UIM_DEPERSONALIZATION send_msg failed")
    succ = false
  else
    succ, qerr, resp = self.luaq.ret_qm_resp(qm)
    if not succ then
      self.l.log("LOG_ERR", string.format("QMI_UIM_DEPERSONALIZATION failed, error=0x%02x", qerr))
      if self.luaq.is_c_true(resp.retries_left_valid) then
        self.wrdb:setp("sim.mep.retries", resp.retries_left.verify_left)
        self.wrdb:setp("sim.mep.unblock_retries", resp.retries_left.unblock_left)
      end
    else
      self.l.log("LOG_NOTICE", "QMI_UIM_DEPERSONALIZATION succeeded")
    end
  end
  return succ
end

--[[
    command handler for MEP locking RDB RPC

    @param cmd 'activate' or 'deactivate'
    @param params A dictionary of 3 keys: nck, mccmnc and retries. Only nck is
    needed for deactivate.
    @param result_len An integer for the max length of result. Not used here.
    @return true for success; false or nil for failure.
--]]
function QmiUim:mep_ops(cmd, params, result_len)
  self.l.log("LOG_DEBUG", string.format("[mep_ops] cmd=%s, params=%s, result_len=%d", cmd, table.tostring(params), result_len))
  local nck = string.format("%s", params.nck) -- this removes trailing null
  if cmd == "activate" then
    return self:mep_activate(nck, string.format("%s", params.mccmnc),
                             tonumber(params.retries))
  end
  if cmd == "deactivate" then
    return self:mep_deactivate(nck)
  end
  self.l.log("LOG_ERR", "[mep_ops] unknown cmd " .. cmd)
  return false
end

-- delete all RDBs under RDB_MEP_PREFIX.cmd.
function QmiUim:cleanup_mep_service()
  local rdbs = self.rdb.keys(self.RDB_MEP_PREFIX_FULL .. "cmd")
  for _, rdb in pairs(rdbs) do
    self.rdb.unset(rdb)
  end
end

function QmiUim:start_rpc_server()
  self.l.log("LOG_DEBUG", "start mep rdbrpc server")
  self:cleanup_mep_service()
  local succ, ret = pcall(self.rdb.new_rdb_session)
  if not succ then
    -- This should not happen. But if it does, skip the rest.
    self.l.log("LOG_ERR", string.format("[start_rpc_server] %s", ret))
    return
  end
  self.rdb_session = ret
  self.rdbfd = self.rdb_session:get_fd()

  succ, ret = pcall(self.rdbrpc.new, self.RPC_MEP_SERVICE_NAME)
  if not succ then
    -- This should not happen. But if it does, skip the rest.
    self.l.log("LOG_ERR", string.format("[start_rpc_server] %s", ret))
    self.rdb_session:destroy()
    self.rdb_session = nil
    self.rdbfd = -1
    return
  end
  self.rpc_server = ret
  self.rpc_server:add_command("activate", {"nck", "mccmnc", "retries"}, "sync", QmiUim.mep_ops, self)
  self.rpc_server:add_command("deactivate", {"nck"}, "sync", QmiUim.mep_ops, self)
  self.rpc_server:run(self.rdb_session)

  local function on_rdb()
    local rdb_names = self.rdb_session:get_names("", self.rdb.TRIGGERED)
    self.rpc_server:process_commands(rdb_names)
  end
  self.ioloop:add_handler(self.rdbfd, self.turbo.ioloop.READ, on_rdb)
end

function QmiUim:end_rpc_server()
  self.l.log("LOG_DEBUG", "end mep rdbrpc server")
  self.ioloop:remove_handler(self.rdbfd)
  self.rdbfd = -1
  self.rpc_server:stop()
  self.rpc_server:destroy()
  self.rpc_server = nil
  self.rdb_session:destroy()
  self.rdb_session = nil
end

function QmiUim:mep_enable(rdbKey, rdbVal)
  -- destroy existing mep service if any
  if self.rpc_server then
    self:end_rpc_server()
  end
  if rdbVal == '1' then
    self:start_rpc_server()
  end
end

function QmiUim:init()

  self.l.log("LOG_INFO", "initiate qmi_uim")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- collect efdir for ISIM
  self:init_efdir()

  -- monitor msisdn
  self:monitor_sim_file({0x3f00,0x7fff,0x6f40})

--[[

  -- write to EFmbdn
  local content=read_sim_file(m.QMI_UIM_READ_RECORD,{0x3f00,0x7fff,0x6fc7})
  content[#content]=bit.bxor(content[#content],1)
  local x = #content - 14
  content[x+3]=(content[x+3]+1) % 256
  write_sim_file(m.QMI_UIM_WRITE_RECORD,{0x3f00,0x7fff,0x6fc7},content)

  -- write to lrplmnsi
  local content=read_sim_file(m.QMI_UIM_READ_TRANSPARENT,{0x3f00,0x7fff,0x6fdc})
  l.log("LOG_INFO", string.format("read LRPLMN = %d",content[1]))
  content[1]=0
  write_sim_file(m.QMI_UIM_WRITE_TRANSPARENT,{0x3f00,0x7fff,0x6fdc},content)

--]]

  -- initiate qmi
  self.luaq.req(self.m.QMI_UIM_EVENT_REG,{
    event_mask=self.bit.bor(
      0x01, -- card status
      --0x02, -- SAP connection
      0x04, -- Extended card status
      --0x08, -- Close of provisioning sessions
      --0x10, -- Physical slot status
      0x20, -- SIM busy status
      --0x40, -- Reduced card status
      --0x80, -- Recovery complete
      --0x100, -- Supply voltage Vcc status
      0x200, -- Card activation status
      --0x400, -- Remote simlock configuration
      0x00
    )
  })

  -- sim command RDB watch
  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."sim.cmd.command", "pin_ops", self)
  -- sim slot select RDB watch
  self:sim_select()
  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."sim.slot_select", "sim_select", self)

  -- start rdb rpc server
  local mep_enabled = self.wrdb:getp(self.RDB_MEP_PREFIX .. "enable")
  if mep_enabled == '1' then
    self:start_rpc_server()
  end

  -- register rdb watcher for enable/disable change
  self.rdbWatch:addObserver(self.RDB_MEP_PREFIX_FULL .. "enable", "mep_enable", self)

end

return QmiUim
