-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- DMS qmi module

local QmiNetcomm = require("wmmd.Class"):new()
local lfs = require("lfs")

QmiNetcomm.message_type_names={
  [0x00] = "voicemail",
  [0x01] = "fax",
  [0x02] = "email",
  [0x03] = "other",
  [0x07] = "videomail",
}

-----------------------------------------------------------------------------
-- copyToStruct
--
-- This function will copy data (typically a byte array) into the specified
-- C structure (struct_type, a string containing the structure name).
-----------------------------------------------------------------------------
function QmiNetcomm:copyToStruct( data, struct_type, len )
  local newStruct = self.ffi.new( struct_type )
  self.ffi.copy( newStruct, data, len )
  return newStruct
end

-----------------------------------------------------------------------------
-- getEnumMember
--
-- This function returns a reference to the requested enum member. To be useful,
-- this reference must be converted to a number; see getEnumValue().
-----------------------------------------------------------------------------
function QmiNetcomm:getEnumMember( enum_type, member_name )
  return self.ffi.new( enum_type, member_name )
end

-----------------------------------------------------------------------------
-- getEnumValue
--
-- This function returns the requested enum member's numeric value.
-----------------------------------------------------------------------------
function QmiNetcomm:getEnumValue( enum_type, member_name )
  return tonumber( self:getEnumMember( enum_type, member_name ) )
end

-----------------------------------------------------------------------------
-- getEnumString
--
-- This function returns the requested enum member's string representation.
-----------------------------------------------------------------------------
function QmiNetcomm:getEnumString( enum_type, member_name )
  return tostring( self:getEnumMember( enum_type, member_name ) )
end

-----------------------------------------------------------------------------
-- buildStructTable
--
-- This function adds a single entry to the designated lookup table. This entry
-- is used to map an enumeration value (member) to a corresponding structure name.
-----------------------------------------------------------------------------
function QmiNetcomm:buildStructTable( table, enum_type, enum_member, struct_type )
  local index = self:getEnumString( enum_type, enum_member )
  table[index] = struct_type
end

-----------------------------------------------------------------------------
-- castUsingTable
--
-- This function uses a lookup table [built using buildStructTable()] to "cast"
-- data (a byte array) to a structure that corresponds to an enumeration member.
-----------------------------------------------------------------------------
function QmiNetcomm:castUsingTable( data, table, enum, len )
  local theStructType = table[tostring(enum)]
  return theStructType and self:copyToStruct( data, theStructType, len )
end


function QmiNetcomm:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_dms", "LOG_DAEMON") end)

  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.m = self.luaq.m
  self.ffi = require("ffi")
  self.wrdb = wrdb
  self.turbo = require("turbo")
  ---partly received SMS go here until we have all parts
  self.message_parts = {}

  self.diagStructTable = {}
  self:buildStructTable( self.diagStructTable, "diagnostic_type_v01", "DIAG_GET_RAT_MEAS_INFO_V01",  "diagnostic_rat_meas_info_t_v01" )
  self:buildStructTable( self.diagStructTable, "diagnostic_type_v01", "DIAG_GET_SIGNAL_INFO_V01",    "diagnostic_signal_info_t_v01" )
  self:buildStructTable( self.diagStructTable, "diagnostic_type_v01", "DIAG_GET_LTE_EMBMS_INFO_V01", "diagnostic_lte_embms_info_t_v01" )

  self.tb = require("wmmd.sms_lookup_tables")
  self.iconv = require("iconv")
  self.config = require("wmmd.config")

  self.coding_scheme = { GSM7=0, DATA_8BIT=1,  UCS2=2 }

  -- Message mode type
  self.message_mode = { MESSAGE_MODE_CDMA=0x02, MESSAGE_MODE_GW=0x01 }

  -- Memory tag type
  self.tag_type = { MT_READ=0, MT_NOT_READ=1, MO_SENT=2, MO_NOT_SENT=3 }

  -- Message format
  self.message_format = { CDMA = 0x00, RESERVED_1 = 0x02, RESERVED_2 = 0x03,
     RESERVED_3 = 0x04, RESERVED_4 = 0x05, GW_PP = 0x06, MWI = 0x08 }

  self.smsc = nil
end

function QmiNetcomm:log_dump(lev, head, msg)
  local pos = 1
  while pos < #msg do
    self.l.log(lev,head..msg:sub(pos, pos+175))
    pos = pos + 176
  end
end

---Turns a FFI char array into a Lua hex string.
--Probably equivalent to lbin2hex(ffi.string(cstr, len)) but avoids a copy.
function QmiNetcomm:cbin2hex(cstr, len)
  local hexdump=""
  for i=0,len-1 do
    hexdump=hexdump..string.format( "%02x",cstr[i])
  end
  return hexdump
end
---Turns a Lua binary string into a Lua hex string.
function QmiNetcomm:lbin2hex(lstr)
  return (lstr:gsub('.', function (byte)
    return string.format('%02x', string.byte(byte))
  end))
end
---Turns a Lua hex string into a Lua binary string
function QmiNetcomm:hex2bin(hexstr)
  return (hexstr:gsub('..', function (hexbyte)
    return string.char(tonumber(hexbyte, 16))
  end))
end

--allow indexing of strings (adapted from http://lua-users.org/wiki/StringIndexing)
--THIS MEANS:
--str[4] gives the value of the byte at position 4 (1-based) - i.e. a number
--str(4) gives a one-character string - i.e. str:sub(4,4)
--str(4,5) is str:sub(4,5)
getmetatable('').__index = function(str,i)
  if type(i) == 'number' then
    return string.byte(string.sub(str,i,i))
  else
    return string[i]
  end
end
getmetatable('').__call = function(str,i,j)
  return string.sub(str,i,j or i)
end

--These functions are all involved in decoding WAP PUSH.
--See OMA-WAP-TS-WSP-V1_0-20110315-A
function QmiNetcomm:read_uintvar(str, start)
  start = start or 1
  local val = 0
  repeat
    local b = str[start]
    start = start + 1
    val = self.bit.lshift(val, 7) + self.bit.band(b, 0x7f)
  until self.bit.band(b, 0x80) == 0
  return val, start
end
function QmiNetcomm:read_integer(str, start)
  start = start or 1
  local b = str[start]
  if self.bit.band(b, 0x80) == 0 then
    local intlen, ind = self:read_valuelength(str, start, true)
    if intlen then
      local val = 0
      for i=ind,ind+intlen-1 do
        val = self.bit.lshift(val, 8) + b
      end
      return val, ind+intlen
    else
      return false, start
    end
  else
    return self.bit.band(b, 0x7f), start+1
  end
end
function QmiNetcomm:read_valuelength(str, start, deny_escape)
  start = start or 1
  local b = str[start]
  if b < 31 then
    return b, start + 1
  elseif b == 31 and not deny_escape then
    return self:read_uintvar(str, start + 1)
  else
    return false, start
  end
end
function QmiNetcomm:read_nullstring(str, start, max)
  start = start or 1
  max = max or #str
  for i=start, max do
    if str[i] == 0 then
      return str:sub(start, i-1), i+1
    end
  end
  return str, #str+1
end

---Well-known content types
QmiNetcomm.c_types={
  [0x36] = "application/vnd.wap.connectivity-wbxml",
  [0x44] = "application/vnd.syncml.notification"
}
--Just causes unknown content types to get a useful string instead of "nil"
setmetatable(QmiNetcomm.c_types, {__index=function(t,k) return "Unknown ctype "..(k or "nil") end})

---Well-known parameters and the function to get the value
QmiNetcomm.p_types={
  [0x11] = {"SEC", "read_integer"},
  [0x12] = {"MAC", "read_nullstring"}
}
--Just causes unknown parameters to get a useful string instead of "nil"
--and attempts to guess the value encoding (if possible) so it can skip to the next param
setmetatable(QmiNetcomm.p_types, {__index=function(t,k)
  if type(k) ~= "number" then
    return "Ptype token?", "notNumberPType"
  else
    return "Unknown ptype "..(k or "nil"), "unknownPType"
  end
end})

function QmiNetcomm:notNumberPType(str, start)
  return false, start + 1
end

function QmiNetcomm:unknownPType(str, start, max)
  local v, ind = self:read_integer(str, start)
  if v then
    return v,ind
  else
    return self:read_nullstring(str, start, max)
  end
end

--QMI commands

-------------------------------------------------------------------------------
--  getDiagnosticData
--
--  This function will retieve diagnostic data from the modem. The specific data
--  type is specified using the string "diagnosticType" (an member of the enum
--  diagnostic_type_v01.
--
--  Returns:
--     success - success flag
--     error   - error code
--     resp    - the QMI response message (type is netcomm_diagnostics_resp_msg)
--     table   -
--     cast    - The results cast to the structure that corresponds to the
--               diagnostic_type.
-------------------------------------------------------------------------------

function QmiNetcomm:getDiagnosticData( diagnosticType, radioType )
  local success, error, resp, table, cast

  -- Send the QMI message and receive the synchronous response
  local theDiagType  = self:getEnumValue( "diagnostic_type_v01",      diagnosticType )
  local theRadioType = self:getEnumValue( "radio_interface_type_v01", radioType )

  success, error, resp, table = self.luaq.req( self.m.QMI_NETCOMM_DIAGNOSTICS, { type=theDiagType, radio_type=theRadioType } )
  if success then
    -- Cast to the target structure type
    cast = self:castUsingTable( resp.data, self.diagStructTable, resp.type, resp.data_len )
  end

  return success, error, resp, table, cast

end



function QmiNetcomm:QMI_WMS_EVENT_REPORT_IND(type, event, qm)
  self.l.log("LOG_INFO", "**************GOT WMS IND**************")
  self.luaq.log_cdata("WMS", qm.resp)

  if qm.resp.mt_message_valid == 1 then
    local read_raw_qm = self:raw_read_sms(qm.resp.mt_message.storage_type, qm.resp.mt_message.storage_index)
    self:parse_mt_message(
      read_raw_qm.resp.raw_message_data.data,
      read_raw_qm.resp.raw_message_data.data_len,
      read_raw_qm.resp.raw_message_data.format,
      nil,
      true) -- wms_raw_read_resp always contains SMSC address
    self:delete_sms_at_index(qm.resp.mt_message.storage_type, qm.resp.mt_message.storage_index)

  elseif qm.resp.transfer_route_mt_message_valid == 1 then
    self:parse_transfer_route_mt_message(qm.resp.transfer_route_mt_message)

  elseif qm.resp.etws_message_valid == 1 then
    self:parse_etws_message(qm)

  elseif qm.resp.call_control_info_valid == 1 then
    self:parse_call_control_info(qm)
  end

  return true
end

function QmiNetcomm:parse_transfer_route_mt_message(transfer_route_msg)
  local ack_info = {}
  ack_info.ack_ind = transfer_route_msg.ack_indicator
  ack_info.transaction_id = transfer_route_msg.transaction_id

  return self:parse_mt_message(transfer_route_msg.data,
    transfer_route_msg.data_len,
    transfer_route_msg.format,
    ack_info,
    false) -- transfer_route_mt_message does not contain SMSC address
end

-- TODO test with actual WAP message over the air (Amari or CMW)
function QmiNetcomm:parse_wap_data(userdata)
  -- Allocate message index. Locking isn't needed if we're the only writer to service.messaging.index
  local msg_ind = (tonumber(self.wrdb:get("service.messaging.index")) or 0) + 1
  local rdbpre = "service.messaging."..msg_ind.."."
  self.wrdb:set(rdbpre.."userdata",userdata)

  --IF WAP PUSH
  if userdata[2] == 6 then
    -- local tid = userdata[1] -- don't really care!
    -- local type = userdata[2] -- already checked!
    local headerlen, ctlen, ct, ct_params, body, headers, ind

    -- throughout this code ind tracks the byte position we are up to
    -- headerslen field is always present. use it to calculate start of body
    headerlen, ind = self:read_uintvar(userdata, 3)
    local ind_body = ind + headerlen

    -- content type is always present, but has two encodings. Try the general form first.
    ctlen, ind = self:read_valuelength(userdata, ind)

    -- both encodings can be either int or string. Try int first and fall-back
    local ctv, ind_ct = self:read_integer(userdata, ind)
    if ctv then
      ct = self.c_types[ctv]
    else
      ct, ind_ct = self:read_nullstring(userdata, ind)
    end

    self.wrdb:set(rdbpre.."wap_push_ct",ct)

    -- only the general form has parameters.
    if ctlen then
      ind = ind+ctlen
      ct_params = userdata:sub(ind_ct, ind-1)
      self.l.log("LOG_WARNING","WMS WAP PUSH CTPARAMS="..self:lbin2hex(ct_params))

      -- attempt to decode the parameters. If we get something we don't recognise it will do its best,
      -- but will probably end up eating the whole remainder as a string.
      while ind_ct < ind do
        local ht, hv
        ht, ind_ct = self:read_integer(userdata, ind_ct)
        local hd, hf = unpack(self.p_types[ht])
        hv, ind_ct = self[hf](self, userdata, ind_ct)
        self.l.log("LOG_INFO","WMS WAP PUSH CTPARAM '"..hd.."'("..ht..")="..hv)
        self.wrdb:set(rdbpre.."wap_push_ct_"..hd,hv)
      end
    else
      -- the short form ends just after the CT - there is no ctlen to use.
      ind = ind_ct
      ct_params = ""
    end

    -- whatever is left up to the body must be headers.
    headers = userdata:sub(ind, ind_body-1)
    body = userdata:sub(ind_body)
    self.wrdb:set(rdbpre.."wap_push_body",body)
    self.wrdb:set("service.messaging.index", msg_ind)

    self.l.log("LOG_WARNING","WMS WAP PUSH CT="..ct)
    self.l.log("LOG_WARNING","WMS WAP PUSH HEADERS="..self:lbin2hex(headers))
    self:log_dump("LOG_WARNING","WMS BODY ",self:lbin2hex(body))
  else
    self.l.log("LOG_WARNING","WMS Not a WAP PUSH")
  end
end

--[[
  data      : complete sms pdu (raw data).
  data_len  : length of the complete sms pdu inclusive of all headers.
  format    : Message format. Value:
    - 0x00 - MESSAGE_FORMAT_CDMA - CDMA
    - 0x02 to 0x05 - Reserved
    - 0x06 - MESSAGE_FORMAT_GW_PP - GW_PP
    - 0x08 - MESSAGE_FORMAT_MWI - MWI
  ack_info  : table containing message ack information required for replying to
              the received message, or nil.
  has_smsc  : whether pdu contains smsc address (SCA)
  Returns parsed sms, partially parsed or binary data, as per the given case.
--]]
function QmiNetcomm:parse_mt_message(data, data_len, format, ack_info, has_smsc)
  if format == self.message_format.CDMA then
    self.l.log("LOG_WARNING", "CDMA Message format 0x00 not supported.")
  elseif format == self.message_format.GW_PP then
    if ack_info ~= nil then
      if ack_info.ack_ind == 0 then
        self.l.log("LOG_INFO","WMS sending ACK...")
        local ack = self.luaq.new_msg(self.m.QMI_WMS_SEND_ACK)
        ack.req.ack_information.message_protocol = 1 --MESSAGE_PROTOCOL_WCDMA
        ack.req.ack_information.transaction_id = ack_info.transaction_id
        ack.req.ack_information.success = 1
        self.luaq.send_msg(ack)
        self.luaq.log_cdata("WMSACK", ack.resp)
      end
    end
    return self:parse_mt_message_fmt_6(data, data_len, has_smsc)
  elseif format == self.message_format.MWI then
    self.l.log("LOG_WARNING",
      "MWI Message format 0x08 not supported. it is handled elsewhere and we shouldn't be here")
  else
    self.l.log("LOG_WARNING", string.format("SMS Message format %d not supported.", format))
  end

  return nil
end


--[[
  Create new message file for incoming SMS
  fn: filename to create
      complete message file format : rxmsg_yymmddhhmmss_unread
      partial message file format : partmsg_xx_yy_zz
              where xx : reference number
                    yy : total message number
                    zz : message index
  return: file handler
--]]
function QmiNetcomm:create_new_rxmsg_file(fn)
  local msg_path = self.config.incoming_sms_dir
  local full_path = msg_path .. "/" .. fn
  local fh, err = io.open(full_path, "wb") -- read as binary file for special characters
  if fh == nil then
    self.l.log("LOG_ERR", "WMS - Could not create a message file, err = " .. err)
  end
  self.l.log("LOG_INFO", "WMS - Created a file " .. fn)
  return fh
end

-- Delete oldest message file if messages exceeds 100
function QmiNetcomm:delete_oldest_message()
  os.execute("/usr/bin/delete_oldest_message.sh")
end

--[[
  Write binary/string data to file.
  fh: file handler to write into
  data : string or UTF-8 binary data table
--]]
function QmiNetcomm:write_to_file(fh, data)
  if type(data) == "table" then
    for _, v in ipairs(data) do
      fh:write(string.char(v))
    end
  else
    fh:write(data)
  end
end

--[[
  Write message contents into message file
  fh: file handler to write into
  oa : origination address
  scts : time stamp
  local_timestamp: received UTC timestamp of the system in seconds(where the epoch is 00:00:00 UTC, January 1, 1970)
  local_timeoffset: time offset between UTC time and local time.("UTC time" - "local time")
  coding_scheme : GSM7 or UCS2
  wap : true for WAP message otherwise false
  message : message body
--]]
function QmiNetcomm:write_rx_msg_file(fh, oa, scts, local_timestamp, local_timeoffset, coding_scheme, wap, message)
  if oa ~= nil then
    fh:write("From : ")
    self:write_to_file(fh, oa)
    fh:write("\n")
  end
  if scts ~= nil then
    fh:write("Time : " .. scts .. "\n")
  end
  if local_timestamp then
    fh:write("LocalTimestamp : " .. local_timestamp .. "\n")
  end
  if local_timeoffset then
    fh:write("LocalTimeOffset : " .. local_timeoffset .. "\n")
  end
  if coding_scheme == self.coding_scheme.GSM7 then
    fh:write("Coding : GSM7\n")
  elseif coding_scheme == self.coding_scheme.DATA_8BIT then
    fh:write("Coding : 8BIT\n")
  elseif coding_scheme == self.coding_scheme.UCS2 then
    fh:write("Coding : UCS2\n")
  end
  if wap ~= nil then
    fh:write(string.format("Wap : %s\n", wap))
  end
  if message ~= nil then
    self:write_to_file(fh, message)
  end
  fh:flush()
end

--[[
  Byte order conversion
  byte: the byte to swap
  return: The byte with nibbles swapped
--]]
function QmiNetcomm:swap_nibbles(byte)
  return self.bit.rshift(byte, 4) + self.bit.lshift(self.bit.band(byte, 0x0F), 4)
end

--[[
  Parse the SMSC/sender/receiver address in the sms message
  data: the data to parse
  data_start: starting index for parsing
  length: the length of data to parse
  return: The SMSC/OA address
--]]
function QmiNetcomm:parse_address(data, start, length)
  local number = ""
  local data_end = start + length
  while start < data_end do
    upper_nibble = self.bit.rshift(data[start], 4)
    lower_nibble = self.bit.lshift(self.bit.band(data[start], 0x0F), 4)
    if upper_nibble ~= 0x0F then
      number = string.format("%s%02x", number, upper_nibble + lower_nibble)
    else
      number = string.format("%s%x", number, self.bit.band(data[start], 0x0F))
    end
    start = start + 1
  end
  return number
end


--[[
  Concatenate message
  fn      : new file name to concatenate
  sms_ref : SMS reference number
  Returns file handler of new file, return nil if fail to create the file
          if auto deletion is indicated then do not concatenate file, just
          delete all partial message files
--]]
function QmiNetcomm:concatenate_message(fn, sms_ref)
  local fh = self:create_new_rxmsg_file(fn)
  -- if failed to create message file, just ignore then the remaining partial messages
  -- will be read and processed when WMMD restart later
  if fh ~= nil then
    local msg_path = self.config.incoming_sms_dir
    for k=1, self.message_parts[sms_ref].total_parts do
      local pfn
      local pfh
      pfn = string.format("partmsg_%s_%s_%s", sms_ref,
                          self.message_parts[sms_ref].total_parts, k)
      local full_path = msg_path .. "/" .. pfn
      pfh = io.open(full_path, "rb")
      if pfh then
        self:write_rx_msg_file(fh, nil, nil, nil, nil, nil, nil, pfh:read("*a"))
        pfh:close()
        os.remove(full_path)
      end
    end

    -- parse concatenated WAP message
    fh:close()
    if self.message_parts[sms_ref].wap == true then
      local msg = {}
      local full_path = msg_path .. "/" .. fn
      fh = io.open(full_path, "rb")
      if fh then
        msg.number = string.sub(fh:read(), 8)
        msg.time = string.sub(fh:read(), 8)
        msg.localtime = string.sub(fh:read(), 18)
        msg.localtimeoffset = string.sub(fh:read(), 19)
        msg.coding = string.sub(fh:read(), 10)
        msg.wap = string.sub(fh:read(), 7)
        msg.text = fh:read("*a")
        self:parse_wap_data(msg.text)
        fh:close()
        -- delete concated WAP message file which is not necessary to keep
        -- neccessary information was saved to RDB variable already
        os.remove(full_path)
      end
    end
    self.message_parts[sms_ref] = nil
  end
  return fh
end

--[[
  Delete all partial message files
  sms_ref : SMS reference number
  Returns None
--]]
function QmiNetcomm:delete_all_partial_messages(sms_ref)
  local msg_path = self.config.incoming_sms_dir
  for k=1, self.message_parts[sms_ref].total_parts do
    local full_path = string.format("%s/partmsg_%s_%s_%s", msg_path, sms_ref,
                                    self.message_parts[sms_ref].total_parts, k)
    os.remove(full_path)
  end
  self.message_parts[sms_ref] = nil
end

--[[
  Parses data for sms, wap, binary data.
  sms_data      : complete sms pdu (raw data).
  sms_data_len  : length of the complete sms pdu inclusive of all headers.
  has_smsc  : whether pdu contains smsc address (SCA)
  Returns parsed sms, partially parsed or binary data, as per the given case.
--]]
function QmiNetcomm:parse_mt_message_fmt_6(sms_data, sms_data_len, has_smsc)
  local i=0
  local smsc = {}
  local INT_NUM_TYPE = 0x90     -- international number type
  local GSM7_ALPHA_TYPE = 0xD0  -- GSM 7-bit default alphabet type
  local hexdump = self:cbin2hex(sms_data, sms_data_len)
  self:log_dump("LOG_INFO", string.format("WMS Message length: %d, WMS Message: ", sms_data_len), hexdump)

  if has_smsc then
    -- SMSC length is in number of bytes (including smsc number type)
    local smsc_len = sms_data[i]
    local gsm7_smsc_len = math.ceil(((sms_data[i]-1)/2*8-7)/7)
    self.l.log("LOG_INFO", string.format("WMS - SMSC len: %d", smsc_len))
    if smsc_len ~= 0 and smsc_len < 13 then
      i=i+1
      smsc.smsc_type = sms_data[i]
      i=i+1
      -- GSM 7-bit alphabet type address has normal byte order so
      -- shouldn't be swapped high and low nibble.
      if ((bit.band(smsc.smsc_type, 0xF0) == GSM7_ALPHA_TYPE) == true) then
        local utf8_addr = self:parse_gsm_7bit_sms(sms_data, i, gsm7_smsc_len, 0)
        smsc.address = ""
        for _, v in ipairs(utf8_addr) do
          smsc.address = smsc.address .. string.char(v)
        end
      else
        smsc.address = self:parse_address(sms_data, i, smsc_len-1)
        if ((bit.band(smsc.smsc_type, 0xF0) == INT_NUM_TYPE) == true) then
          smsc.address = '+' .. smsc.address
        end
      end
      i=i+smsc_len-1
    end
    self.l.log("LOG_INFO", string.format("WMS - SMSC Type 0x%02X", smsc.smsc_type))
    self.l.log("LOG_INFO", string.format("WMS - SMSC: %s", smsc.address))
  else
    self.l.log("LOG_INFO", "WMS - SMSC Not present in message")
    smsc = self.smsc or self:get_smsc_info() or {}
    if not self.smsc then
      smsc.smsc_type = 0
      smsc.address = 0
    end
  end

  local pdu_type = sms_data[i]
  local rp  = self.bit.band(self.bit.rshift(pdu_type, 7), 0x01)     -- Reply Path
  local udhi = self.bit.band(self.bit.rshift(pdu_type, 6), 0x01)    -- UDH present?
  local sri = self.bit.band(self.bit.rshift(pdu_type, 5), 0x01)     -- Status Report Indication
  local lp = self.bit.band(self.bit.rshift(pdu_type, 3), 0x01)      -- Loop prevention
  local mms = self.bit.band(self.bit.rshift(pdu_type, 2), 0x01)     -- More messages to send (inverted)
  local mti = self.bit.band(pdu_type, 0x03)                         -- 00 => SMS-DELIVER
  self.l.log("LOG_INFO", string.format("WMS - rp:%d, udhi:%d, sri:%d, lp:%d, mms:%d, mti:%d",
    rp, udhi, sri, lp, mms, mti))
  i=i+1

  if mti ~= 0 then
    self.l.log("LOG_ERR", string.format("Not a SMS DELIVER message, mti: %d", mti))
    return nil
  end

  -- OA length is in number of bcd digits (excluding OA number type), convert to number of bytes
  local oa_len = math.ceil(sms_data[i]/2)
  local gsm7_oa_len = math.ceil((sms_data[i]/2*8-7)/7)
  self.l.log("LOG_INFO",string.format("WMS - OA len: %d digits, %d bytes", sms_data[i]/2, oa_len))
  i=i+1
  local oa_type = sms_data[i]
  self.l.log("LOG_INFO",string.format("WMS - OA Type: 0x%02X", oa_type))
  i=i+1
  local oa
  -- GSM 7-bit alphabet type address has normal byte order so
  -- shouldn't be swapped high and low nibble.
  if ((bit.band(oa_type, 0xF0) == GSM7_ALPHA_TYPE) == true) then
    -- recalculate address length for GSM7 encoded address
    self.l.log("LOG_INFO",string.format("WMS - GSM7 encoded OA len: %d", gsm7_oa_len))
    oa = self:parse_gsm_7bit_sms(sms_data, i, gsm7_oa_len, 0)
  else
    oa = self:parse_address(sms_data, i, oa_len)
    if ((bit.band(oa_type, 0xF0) == INT_NUM_TYPE) == true) then
      oa = '+' .. oa
    end
  end
  self.l.log("LOG_INFO",string.format("WMS - OA: %s ", oa))
  i=i+oa_len

  --[[ TP-PID   Meaning
       0        Default store and forward short message
       1–31     no telematic interworking, but SME to SME protocol
       32       implicit telemetic device
       33       Telex or teletex reduced to telex format
       34       Group 3 telefax
       35       Group 4 telefax
       36       Voice telephone
       37       ERMES (European Radio Messaging System)
       38       National Paging system (known to the SC)
       39       Videotex (T.100 [20] /T.101 [21])
       40       Teletex, carrier unspecified
       41       Teletex, in PSPDN
       42       Teletex, in CSPDN
       43       Teletex, in analog PSTN
       44       Teletex, in digital ISDN
       45       UCI (Universal Computer Interface, ETSI DE/PS 3 01 3)
       46–47    Reserved
       48       A message handling facility (known to the SC)
       49       Any public X.400 based message handling system
       50       Internet Electronic Mail
       51–55    Reserved
       56–62    SC-specific; usage based on mutual agreement between the SME and the SC
       63       A GSM/UMTS mobile station.
       64       Short Message Type 0
       65       Replace Short Message Type 1
       66       Replace Short Message Type 2
       67       Replace Short Message Type 3
       68       Replace Short Message Type 4
       69       Replace Short Message Type 5
       70       Replace Short Message Type 6
       71       Replace Short Message Type 7
       72       Device Triggering Short Message
       73–93    Reserved
       94       Enhanced Message Service (Obsolete)
       95       Return Call Message
       96–123   Reserved
       124      ANSI-136 R-DATA
       125      ME Data download
       126      ME De personalization Short Message
       127      (U)SIM Data download
       128–191  reserved
       192–255  Assigns bits 0 5 for SC specific use
  ]]--
  -- TODO All decoding below is based on assuming PID 0x00
  local pid = sms_data[i]
  self.l.log("LOG_INFO",string.format("WMS - PID: 0x%02X ", pid))
  i=i+1

  -- DCS Parsing and class based sorting/operations here, e.g. whether
  -- to save sms or not, display/show notification, turn icons on/off (e.g. in
  -- case of MWI, or may be normal sms), etc.
  -- This info will mostly be used after completion of parsing
  local dcs = sms_data[i]
  self.l.log("LOG_INFO",string.format("WMS - DCS: 0x%02X ", dcs))
  i=i+1

  local class = nil
  local dcs_mwi_type = nil      -- MWI type in DCS fields
  local mwi_indi = false
  local mwi_count = nil
  local mwi_name = nil
  local mwi = {}                -- MWI information container in User Data fields
  local coding_scheme = nil
  local is_compressed = false   -- no specific action for this field, ignored.
  local auto_deletion = false
  local coding_group = self.bit.band(self.bit.rshift(dcs, 4), 0x0F)
  local coding_subgroup = self.bit.band(self.bit.rshift(dcs, 6), 0x03)
  if coding_subgroup ~= 3 then
    --[[ 00xx : General Data Coding indication
      Bits 5..0 indicate the following:

      Bit 5, if set to 0, indicates the text is uncompressed
      Bit 5, if set to 1, indicates the text is compressed using the  compression algorithm
      defined in 3GPP TS 23.042 [13]

      Bit 4, if set to 0, indicates that bits 1 to 0 are reserved and have no message class meaning
      Bit 4, if set to 1, indicates that bits 1 to 0 have a message class meaning::

      Bit 1 Bit 0 Message Class
      0     0     Class 0
      0     1     Class 1   Default meaning: ME-specific.
      1     0     Class 2   (U)SIM specific message
      1     1     Class 3   Default meaning: TE specific (see 3GPP TS 27.005 [8])

      Bits 3 and 2 indicate the character set being used, as follows :
      Bit 3 Bit2  Character set:
      0     0     GSM 7 bit default alphabet
      0     1     8 bit data
      1     0     UCS2 (16bit) [10]
      1     1     Reserved

      NOTE: The special case of bits 7..0 being 0000 0000 indicates the GSM 7 bit default
      alphabet with no message class
    --]]

    -- 1000-1011 : Reserved coding groups, treat as 00000000
    if coding_subgroup == 2 then
      coding_scheme = self.coding_scheme.GSM7
    else
      if self.bit.band(coding_group, 0x02) ~= 0 then
        is_compressed = true
      end

      if self.bit.band(coding_group, 0x01) ~= 0 then
        class = self.bit.band(dcs, 0x03)
      end
      coding_scheme = self.bit.band(self.bit.rshift(dcs, 2), 0x03)
    end

    if coding_subgroup == 1 then
      --[[ 01xx :  Message Marked for Automatic Deletion Group
        This group can be used by the SM originator to mark the message (stored in the ME
        or (U)SIM ) for deletion after reading irrespective of the message class.
        The way the ME will process this deletion should be manufacturer specific but shall
        be done without the intervention of the End User or the targeted application. The
        mobile manufacturer may optionally provide a means for the user to prevent this
        automatic deletion.

        Bit 5..0 are coded exactly the same as Group 00xx
      --]]
      auto_deletion = true
    end
  else
    -- 11xx :
    local class_mwi_subgroup = self.bit.band(coding_group, 0x03)
    if class_mwi_subgroup ~= 3 then
      if class_mwi_subgroup == 0 then
        --[[ 1100 : Message Waiting Indication Group: Discard Message
          The specification for this group is exactly the same as for Group 1101, except that:
          after presenting an indication and storing the status, the ME may discard the contents
          of the message.

          The ME shall be able to receive, process and acknowledge messages in this group,
          irrespective of memory availability for other types of short message.
        --]]
        auto_deletion = true
      end

      --[[ 1101 : Message Waiting Indication Group: Store Message
        This Group defines an indication to be provided to the user about the status of
        types of message waiting on systems connected to the GSM/UMTS PLMN. The ME should
        present this indication as an icon on the screen, or other MMI indication. The ME
        shall update the contents of the Message Waiting Indication Status on the SIM (see
        3GPP TS 51.011 [18]) or USIM (see 3GPP TS 31.102 [17]) when present or otherwise
        should store the status in the ME. In case there are multiple records of EFMWIS
        this information shall be stored within the first record. The contents of the
        Message Waiting Indication Status should control the ME indicator. For each indication
        supported, the mobile may provide storage for the Origination Address. The ME may take
        note of the Origination Address for messages in this group and group 1100.

        Text included in the user data is coded in the GSM 7 bit default alphabet.
        Where a message is received with bits 7..4 set to 1101, the mobile shall store the text
        of the SMS message in addition to setting the indication. The indication setting should
        take place irrespective of memory availability to store the short message.

          Bits 3 indicates Indication Sense:

          Bit 3
          0     Set Indication Inactive
          1     Set Indication Active

          Bit 2 is reserved, and set to 0

          Bit 1 Bit 0 Indication Type:
          0     0     Voicemail Message Waiting
          0     1     Fax Message Waiting
          1     0     Electronic Mail Message Waiting
          1     1     Other Message Waiting*

        * Mobile manufacturers may implement the "Other Message Waiting" indication as an
          additional indication without specifying the meaning
      --]]
      mwi_indi = (self.bit.band(self.bit.rshift(dcs, 3), 0x01) == 1)
      dcs_mwi_type = self.bit.band(dcs, 0x03)
      mwi_name = self.message_type_names[dcs_mwi_type]
      if mwi_name then
        mwi[mwi_name] = {
          active = true,
          count = 1
        }
      else
        self.l.log("LOG_ERR",string.format("WMS - DCS - invalid message indication type 0x%02x", tonumber(dcs_mwi_type)))
      end
      coding_scheme = self.coding_scheme.GSM7

      if class_mwi_subgroup == 2 then
        --[[ 1110 : Message Waiting Indication Group: Store Message
          The coding of bits 3..0 and functionality of this feature are the same as for the
          Message Waiting Indication Group above, (bits 7..4 set to 1101) with the exception
          that the text included in the user data is coded in the uncompressed UCS2 character set.
        --]]
        is_compressed = false
        coding_scheme = self.coding_scheme.UCS2
      end
    else
      --[[ 1111 : Data coding/message class
        Bit 3 is reserved, set to 0.

        Bit 2  Message coding:
        0      GSM 7 bit default alphabet
        1      8-bit data

        Bit 1  Bit 0  Message Class:
        0      0      Class 0
        0      1      Class 1  default meaning: ME-specific.
        1      0      Class 2  (U)SIM-specific message.
        1      1      Class 3  default meaning: TE specific (see 3GPP TS 27.005 [8])
      --]]
      class = self.bit.band(dcs, 0x03)
      coding_scheme = self.bit.band(self.bit.rshift(dcs, 2), 0x01)
    end
  end
  self.l.log("LOG_INFO", string.format("WMS - DCS: class: %s, mwi_type: %s, mwi_indi: %s,"..
    " coding_scheme: %s, is_compressed: %s, auto_deletion: %s, coding_group: %s",
    class, dcs_mwi_type, mwi_indi, coding_scheme, is_compressed, auto_deletion, coding_group))

  -- extract the time information
  local year = string.format("%02X", self:swap_nibbles(sms_data[i]))
  i=i+1
  local month = string.format("%02X", self:swap_nibbles(sms_data[i]))
  i=i+1
  local day = string.format("%02X", self:swap_nibbles(sms_data[i]))
  i=i+1
  local hour = string.format("%02X", self:swap_nibbles(sms_data[i]))
  i=i+1
  local minute = string.format("%02X", self:swap_nibbles(sms_data[i]))
  i=i+1
  local second = string.format("%02X", self:swap_nibbles(sms_data[i]))
  i=i+1

  local tz = self.bit.band(sms_data[i], 0x07) * 10 + self.bit.rshift(sms_data[i], 4)
  local direction = "+"
  if self.bit.band(sms_data[i], 0x08) ~= 0 then
    direction = "-"
  end
  local gmt_hour_offset = string.format(math.floor(tz/4))
  local gmt_minute_offset = string.format((tz%4)*15)
  i=i+1
  local scts = string.format("%02d/%02d/%02d - %02d:%02d:%02d - gmt: %s%02d:%02d",
                             day, month, year, hour, minute, second,
                             direction, gmt_hour_offset, gmt_minute_offset)
  self.l.log("LOG_INFO",string.format("WMS - SCTS: %s", scts))
  local local_timestamp = os.time() -- utc timestamp of local system.
  -- time difference between UTC and system local time("UTC time" - "system local time")
  local local_timeoffset = tonumber(self.wrdb:get("system.ipq.timeoffset")) or 0

  -- user data
  local udl = sms_data[i]
  self.l.log("LOG_INFO",string.format("WMS - UDL: %d ", udl))
  i=i+1

  local message = ""
  local fn = ""
  if udhi == 0 then
    if auto_deletion then
      self.l.log("LOG_INFO","WMS - auto_deletion is true, do not save message")
    else
      -- full text sms present
        message = self:parse_sms(sms_data, i, udl, 0, coding_scheme)
      -- create message file with time stamp as its file name
      fn = string.format("rxmsg_%02d%02d%02d%02d%02d%02d_unread",
                        year, month, day, hour, minute, second)
      local fh = self:create_new_rxmsg_file(fn)
      if fh ~= nil then
        -- write message contents to the file
        self:write_rx_msg_file(fh, oa, scts, local_timestamp, local_timeoffset, coding_scheme, false, message)
        fh:close()
#ifdef V_CUSTOM_FEATURE_PACK_whp
        -- Delete oldest message file if messages exceeds 100
        self:delete_oldest_message()
#endif
      end
    end
  else
    -- See for IEI types: https://en.wikipedia.org/wiki/User_Data_Header
    -- could be full sms (including MWI or any other IEI), concatenated sms, MWI only,
    -- WAP PUSH, or any other combination of UDH headers.
    local udh_hlen = sms_data[i]
    local this_part = 0
    local total_parts = 0
    local sms_ref = -1
    local isConcatenated = false
    local isWap = false
    self.l.log("LOG_INFO",string.format("WMS - UDH - UDH Len, octets: %d", udh_hlen))
    i=i+1

    local j=0
    while j < udh_hlen do
      local iei = sms_data[i+j]
      j=j+1
      self.l.log("LOG_INFO",string.format("WMS - UDH - IEI1: 0x%02X", iei))

      -- IEI 00 : Concatenated short messages, 8-bit reference number
      -- IEI 08 : Concatenated short message, 16-bit reference number
      if iei == 0 or iei == 8 then
        -- concatenated sms
        isConcatenated = true
        local iei_len = sms_data[i+j]
        j=j+1
        self.l.log("LOG_INFO",string.format("WMS - UDH - IEI Len: %d", iei_len))

        sms_ref = string.format("%02X", sms_data[i+j])
        j=j+1
        if iei == 8 then
          sms_ref = string.format("%s%02X", sms_ref, sms_data[i+j])
          j=j+1
        end
        self.l.log("LOG_INFO",string.format("WMS - UDH - SMS REF: 0x%s, %s", sms_ref, sms_ref))

        total_parts = sms_data[i+j]
        j=j+1
        self.l.log("LOG_INFO",string.format("WMS - UDH - SMS Total Parts: %d", total_parts))

        -- internal sms ref is a combination of src number and total parts
        local ref_oa = ""
        if type(oa) == "table" then
          for _, v in ipairs(oa) do
            ref_oa = ref_oa ..string.char(v)
          end
        else
          ref_oa = oa
        end

        sms_ref = string.format("%s%s%s", ref_oa, sms_ref, total_parts)
        self.l.log("LOG_INFO",string.format("WMS - UDH - Internal SMS REF: %s", sms_ref))

        if self.message_parts[sms_ref] == nil  then
          self.message_parts[sms_ref] = {}
          self.message_parts[sms_ref].parts = 0
          -- There are two cases;
          -- 1) isWap = true : Already received WAP IEI before receiving concatenate IEI that means the WAP
          --                   message could be long.
          -- 2) isWap = false : This is the first concatenate IEI but there could be WAP IEI could follow or not.
          self.message_parts[sms_ref].wap = isWap
          self.message_parts[sms_ref].auto_deletion = false
          for k=1, total_parts do
            self.message_parts[sms_ref][k] = {}
          end
        end
        this_part = sms_data[i+j]
        j=j+1
        self.l.log("LOG_INFO",string.format("WMS - UDH - This Part: %d", this_part))

        self.message_parts[sms_ref].total_parts = total_parts
        self.message_parts[sms_ref].parts = self.message_parts[sms_ref].parts + 1

      -- IEI 05 : Application port addressing scheme, 16 bit address
      elseif iei == 5 then
        self.l.log("LOG_INFO",string.format("WMS - UDH - WAP IEI %d", iei))
        local iei_len = sms_data[i+j]
        self.l.log("LOG_INFO",string.format("WMS - UDH - IEI Len: %d", iei_len))
        j=j+1

        --[[ Application Port Addressing Scheme (WAP or other)
          For WAP Destination ports (from WAP-200-WDP-20000219-p.pdf):
            - Appendix B: Port Number Definitions
            WAP has registered the ports in table C.1 with IANA (Internet Assigned Numbers Authority).
            - Table B.1: WAP Port Number
            2805 WAP WTA secure connection-less session service
            2923 WAP WTA secure session service
            2948 WAP Push connectionless session service (client side)
            2949 WAP Push secure connectionless session service (client side)
            9200 WAP connectionless session service
            9201 WAP session service
            9202 WAP secure connectionless session service
            9203 WAP secure session service
            9204 WAP vCard
            9205 WAP vCal
            9206 WAP vCard Secure
            9207 WAP vCal Secure
        --]]
        -- check for WAP ports, we only support port 2948
        local dport = tonumber(string.format("%02X%02X",sms_data[i+j],sms_data[i+j+1]), 16)
        local sport = tonumber(string.format("%02X%02X",sms_data[i+j+2],sms_data[i+j+3]), 16)
        self.l.log("LOG_INFO",string.format("WMS - WAP sport %d", sport))
        if dport == 2948 then
          if isConcatenated == true then
            self.message_parts[sms_ref].wap = true
          else
            isWap = true
          end
          self.l.log("LOG_INFO",string.format("WMS - Supported WAP dport %d", dport))
        elseif dport == 2805 or dport == 2923 or dport == 2949 or (dport >= 9200 and dport <= 9207) then
          self.l.log("LOG_INFO",string.format("WMS - Unsupported WAP dport %d", dport))
        end
        j=j+iei_len

      -- IEI 01 : Special SMS Message Indication
      elseif iei == 1 then
        self.l.log("LOG_INFO",string.format("WMS - UDH - Special SMS Message Indication IEI %d", iei))
        local iei_len = sms_data[i+j]
        if iei_len > 2 then
          self.l.log("LOG_ERR",string.format("WMS - UDH - IEI Len: %d is too large, process 2 octets only",
                     iei_len))
        else
          self.l.log("LOG_INFO",string.format("WMS - UDH - IEI Len: %d", iei_len))
        end
        j=j+1
        local iei_octet1 = sms_data[i+j]
        local iei_octet2 = sms_data[i+j+1]
        -- Below storage indication, profile ID are not
        -- used here, leaving just for reference
        local ud_mwi_store = self.bit.band(self.bit.rshift(iei_octet1, 7), 0x01)
        local ud_mwi_prof_id = self.bit.band(self.bit.rshift(iei_octet1, 5), 0x03)
        -- There could be no case when MWI indication IEI is mixed with concatenation IEI
        -- in real world but no restriction in the specification so we have to consider that
        -- case as well.
        auto_deletion = (ud_mwi_store == 0)
        if isConcatenated then
          self.message_parts[sms_ref].auto_deletion = auto_deletion
        end
        --[[
            message indication type
            ------------------------
            bit 432    bit 10
            ------------------------
            000        00  Voice Message Waiting
            000        01  Fax Message Waiting
            000        10  Email Message Waiting
            000        11  Other type of Waiting
            001        11  Video Message Waiting
        ]]--
        local ud_mwi_type = self.bit.band(iei_octet1, 0x1f)
        mwi_count = tonumber(iei_octet2)
        mwi_indi = true
        self.l.log("LOG_INFO",string.format("WMS - UDH - auto deletion %s, MWI type %d, MWI count %d, mwi_indi %s",
                   auto_deletion, ud_mwi_type, mwi_count, mwi_indi))
        mwi_name = self.message_type_names[ud_mwi_type]
        if mwi_name then
          mwi[mwi_name] = {
            active = (mwi_count > 0),
            count = mwi_count
          }
        else
          self.l.log("LOG_ERR",string.format("WMS - UDH - invalid message indication type 0x%02x", tonumber(ud_mwi_type)))
        end
        j=j+iei_len

      else
        local iei_len = sms_data[i+j]
        j=j+iei_len+1
        self.l.log("LOG_INFO",string.format("WMS - UDH - unsupported IEI %d, length: %d", iei, iei_len))
      end
    end
    i = i+udh_hlen

    message = self:parse_sms(sms_data, i, udl, udh_hlen + 1, coding_scheme)
    if isConcatenated == true then
      self.message_parts[sms_ref][this_part] = message

      -- Save partial message to file in order to keep the message over power reset or failure.
      -- partial message file format : partmsg_xx_yy_zz
      --        where xx : reference number
      --              yy : total message number
      --              zz : message index
      fn = string.format("partmsg_%s_%s_%s", sms_ref, total_parts, this_part)
      local fh = self:create_new_rxmsg_file(fn)
      -- TO DO : There is no good idea to handle remaining partial messages when
      -- failed to create message file at the moment whether deleting all other partial
      -- messages including the messages not arrived yet or just ignoring a message then
      -- try to combine with other messages.
      if fh ~= nil then
        -- Write OA, SMSC address, coding scheme only for 1st partial message file
        if this_part == 1 then
          self:write_rx_msg_file(fh, oa, scts, local_timestamp, local_timeoffset,coding_scheme, self.message_parts[sms_ref].wap,
                                self.message_parts[sms_ref][this_part])
        else
          self:write_rx_msg_file(fh, nil, nil, nil, nil, nil, nil, self.message_parts[sms_ref][this_part])
        end
        fh:close()
      else
        self.l.log("LOG_ERR",string.format("WMS - failed to create " .. fn))
      end

      -- Concatenate all partial message files when received all messages
      if self.message_parts[sms_ref].parts == self.message_parts[sms_ref].total_parts then
        self.l.log("LOG_INFO",string.format("WMS - recevied all %d messages",
                    self.message_parts[sms_ref].total_parts))
        if auto_deletion or self.message_parts[sms_ref].auto_deletion then
          self.l.log("LOG_INFO","WMS - auto_deletion is true, delete all partial message files")
          self:delete_all_partial_messages(sms_ref)
        else
          -- create message file with time stamp as its file name
          fn = string.format("rxmsg_%02d%02d%02d%02d%02d%02d_unread",
                            year, month, day, hour, minute, second)
          fh = self:concatenate_message(fn, sms_ref)
          if fh == nil then
            self.l.log("LOG_ERR",string.format("WMS - failed to create " .. fn))
          else
#ifdef V_CUSTOM_FEATURE_PACK_whp
            -- Delete oldest message file if messages exceeds 100
            self:delete_oldest_message()
#endif
          end
        end
      end

    -- isConcatenated ~= true, single complete WAP message or
    -- normal text message
    else
      if isWap == true then
        self:parse_wap_data(message)
      else
        if auto_deletion then
          self.l.log("LOG_INFO","WMS - auto_deletion is true, do not create message file")
        else
          -- create message file with time stamp as its file name
          fn = string.format("rxmsg_%02d%02d%02d%02d%02d%02d_unread",
                            year, month, day, hour, minute, second)
          local fh = self:create_new_rxmsg_file(fn)
          if fh ~= nil then
            -- write message contents to the file
            self:write_rx_msg_file(fh, oa, scts, local_timestamp, local_timeoffset, coding_scheme, false, message)
            fh:close()
#ifdef V_CUSTOM_FEATURE_PACK_whp
            -- Delete oldest message file if messages exceeds 100
            self:delete_oldest_message()
#endif
          end
        end
      end
    end
  end

  -- 3GPP TS 023-040
  -- If MWI indication in DCS or UDH indicates any type of message indication received then
  -- set to RDB variable to notify any SMS clients and update SIM file.
  if mwi_indi then
    self.l.log("LOG_INFO","WMS - got message indication, invoke modem_on_mwi and update SIM file")
    self.watcher.invoke("sys","write_sim_file_mwi",mwi)
    self.watcher.invoke("sys","modem_on_mwi",mwi)
  end

  if isWap == false or (isConcatenated == true and self.message_parts[sms_ref].wap == false) then
    self.l.log("LOG_INFO",string.format("WMS - SMS Message: %s", message))
  end
  return message
end

--[[
  Scan partial messages in incoming message folder and concatenate
  if received all messages already otherwise build SMS message
  structure for coming partial messages.
--]]
function QmiNetcomm:scan_message_folder()
  -- When qmi_netcomm.lua is called independantly before WMMD starts
  -- after newly flashing the memory then SMS incoming folder is not
  -- created yet and it crashes so need to check.
  local dir_obj = lfs.attributes(self.config.incoming_sms_dir)
  if dir_obj == nil then
    self.l.log("LOG_INFO", "WMS - message folder does not exist")
    return
  end
  local year, month, day, hour, minute, second
  self.l.log("LOG_INFO", "--------------------------------------------------------------------")
  self.l.log("LOG_INFO", "WMS - scan_message_folder()")
  for file in lfs.dir(self.config.incoming_sms_dir) do
    local fn = self.config.incoming_sms_dir.."/"..file
    local prefix = string.sub(file, 1, 7)
    local sms_ref, total_cnt, msg_idx
    if lfs.attributes(fn, "mode") == "file" and prefix == "partmsg" then
      local msg = {}
      self.l.log("LOG_INFO", "WMS - open file : " ..fn)
      local fh = io.open(fn, "rb")
      if fh == nil then
        logger.logErr("WMS - failed to open file " .. fn)
      else
        local read_partmsg = function()
          sms_ref, total_cnt, msg_idx = string.match(file, "partmsg_([+]?%w+)_(%d+)_(%d+)")
          self.l.log("LOG_INFO",string.format("WMS - sms ref %s, total cnt %d, msg idx %d",
                     sms_ref, total_cnt, msg_idx))
          if self.message_parts[sms_ref] == nil  then
            self.message_parts[sms_ref] = {}
            self.message_parts[sms_ref].parts = 0
            self.message_parts[sms_ref].wap = false
            self.message_parts[sms_ref].auto_deletion = false
            for k=1, total_cnt do
              self.message_parts[sms_ref][k] = {}
            end
            self.message_parts[sms_ref].total_parts = tonumber(total_cnt)
          end
          -- read time stamp from first partial message
          if msg_idx == '1' then
            self.l.log("LOG_INFO", "WMS - read time stamp from first partial message")
            msg.number = string.sub(fh:read(), 8)
            msg.time = string.sub(fh:read(), 8)
            msg.localtime = string.sub(fh:read(), 18)
            msg.localtimeoffset = string.sub(fh:read(), 19)
            msg.coding = string.sub(fh:read(), 10)
            self.message_parts[sms_ref].wap = (string.sub(fh:read(), 7) == "true")
            day, month, year = string.match(msg.time, "(%d+)/(%d+)/(%d+)")
            hour, minute, second = string.match(msg.time, "(%d+):(%d+):(%d+)")
          end
          self.message_parts[sms_ref][msg_idx] = fh:read("*a")
          self.message_parts[sms_ref].parts = self.message_parts[sms_ref].parts + 1
          fh:close()
        end
        if not pcall(read_partmsg) then self.l.log("LOG_ERR", string.format("error processing file:%s", file)) end
      end

      -- Concatenate all partial message files when received all messages
      if sms_ref and self.message_parts[sms_ref].parts == self.message_parts[sms_ref].total_parts then
        self.l.log("LOG_INFO",string.format("WMS - recevied all %d messages",
                    self.message_parts[sms_ref].total_parts))
        -- create message file with time stamp as its file name
        fn = string.format("rxmsg_%02d%02d%02d%02d%02d%02d_unread",
                          year, month, day, hour, minute, second)
        fh = self:concatenate_message(fn, sms_ref)
        if fh == nil then
          self.l.log("LOG_ERR",string.format("WMS - failed to create " .. fn))
        end
        self.l.log("LOG_INFO", "--------------------------------------------------------------------")
      end
    end
  end
end

--[[
  data  : GSM 7-bit decoded buffer
  Returns Unicode decoded buffer
--]]
function QmiNetcomm:decode_unicode_from_gsm7(data)
  local decode_buf = {}
  local unicode = nil
  local extended = false
  for i, v in ipairs(data) do
    if v ~= 0x1B then
      if extended then
        unicode = self.tb.gsm7tounicode_ext[v]
      else
        unicode = self.tb.gsm7tounicode[v]
      end
      if not unicode then
        self.l.log("LOG_ERR", string.format(
          "Cannot convert GSM character %s0x%02x to Unicode",
          extended and "0x1B " or "", v))
      else
        self.l.log("LOG_DEBUG", string.format("%03d: 0x%04X", i, unicode))
        table.insert(decode_buf, unicode)
      end
      extended = false
    else
      extended = true
    end
  end
  return decode_buf
end

--[[
  data  : Unicode decoded buffer
  Returns UTF-8 decoded buffer
--]]
function QmiNetcomm:decode_utf8_from_unicode(data)
  local decode_buf = {}
  local utf8code = nil
  local _NXT, _SEQ2, _SEQ3 = 0x80, 0xc0, 0xe0
  local _BOM = 0xfeff
  local n, upper, lower = 0, nil, nil

  for i, v in ipairs(data) do
    -- ignore forbitten code
    if v >= 0xd800 and v <= 0xdfff then
      self.l.log("LOG_ERR", string.format("Skip forbitten code 0x%04x", v))

    -- ignore BOM(Byte Order Mark) code
    elseif v == 0xfeff then
      self.l.log("LOG_ERR", string.format("Skip BOM code 0x%04x", v))

    -- process normal unicode
    else
      if v <= 0x007f then
        n = 1
      elseif v <= 0x07ff then
        n = 2
      else
        n = 3
      end

      upper = self.bit.band(self.bit.rshift(v, 8), 0xff)
      lower = self.bit.band(v, 0xff)
      self.l.log("LOG_DEBUG", string.format("n %d, upper 0x%02x, lower 0x%02x", n, upper, lower))
      if n == 1 then
        utf8code = lower
        table.insert(decode_buf, utf8code)
      elseif n == 2 then
        utf8code = self.bit.bor(_SEQ2, self.bit.rshift(lower, 6), self.bit.lshift(self.bit.band(upper, 0x07), 2))
        table.insert(decode_buf, utf8code)
        utf8code = self.bit.bor(_NXT, self.bit.band(lower, 0x3f))
        table.insert(decode_buf, utf8code)
      elseif n == 3 then
        utf8code = self.bit.bor(_SEQ3, self.bit.rshift(self.bit.band(upper, 0xf0), 4))
        table.insert(decode_buf, utf8code)
        utf8code = self.bit.bor(_NXT, self.bit.rshift(lower, 6), self.bit.lshift(self.bit.band(upper, 0x0f), 2))
        table.insert(decode_buf, utf8code)
        utf8code = self.bit.bor(_NXT, self.bit.band(lower, 0x3f))
        table.insert(decode_buf, utf8code)
      end
    end
  end
  return decode_buf
end

--[[
  data  : complete input data buffer
  i     : index from where sms data starts (after UDL and UDH)
  udl   : data length including UDH
  udh_hlen : UDH length
  Returns UTF-8 decoded buffer
--]]
function QmiNetcomm:parse_gsm_7bit_sms(data, i, udl, udh_hlen)
  local gsm7_buf = {}
  local unicode_buf = {}
  local utf8_buf = {}
  local char_data = nil
  local char_count = 0

  local data_start = 0
  local udh_hlen_septets = math.ceil((udh_hlen)*8/7) -- in spetets
  local data_length = udl - udh_hlen_septets

  local padding = (7 - udh_hlen) % 7
  if padding ~= 0 then
    data_start = 7 - padding
    i = i - data_start
  end

  self.l.log("LOG_INFO", string.format("WMS - UDH - udl:%d, udh (octets):%d, udh(septets):%d, data len:%d, padding:%d",
    udl, udh_hlen, udh_hlen_septets, data_length, padding))

  for j=data_start, data_start+data_length do
    local lshift = j%7
    local decoded_ch = nil
    char_count = char_count + 1

    if lshift == 0 then
      char_data = self.bit.band(data[i+j], 0x7F)
      table.insert(gsm7_buf, char_data)
      self.l.log("LOG_DEBUG", string.format("0 j: %03d:0x%02X, char_data:0x%02X", j, data[i+j], char_data))
    else
      local rshift = 8-lshift
      if padding ~= 0 then
        -- if padding is present, we skip it, there is no previous byte yet
        padding = 0
        char_count = char_count - 1
      else
        local lmask = self.bit.rshift(0x3F, lshift-1)
        -- create data from last and current byte
        char_data = self.bit.bor(
          self.bit.rshift(data[i+j-1], rshift),
          self.bit.lshift(self.bit.band(data[i+j], lmask), lshift))
        table.insert(gsm7_buf, char_data)
        self.l.log("LOG_DEBUG", string.format("1 j: %03d:0x%02X, rshift: %d, lshift: %d, lmask: 0x%02X, char_data:0x%02X",
          j, data[i+j], rshift, lshift, lmask, char_data))

        if char_count >= data_length then break end
      end

      -- we get two characters here
      if rshift == 2 then
        char_count = char_count + 1
        char_data = self.bit.rshift(data[i+j], 1)
        table.insert(gsm7_buf, char_data)
        self.l.log("LOG_DEBUG", string.format("2 j: %03d:0x%02X, char_data:0x%02X", j, data[i+j], char_data))
      end
    end

    if char_count >= data_length then break end
  end

  self.l.log("LOG_DEBUG", string.format("*** GSM7 decoded buffer ***"))
  for i, v in ipairs(gsm7_buf) do
    self.l.log("LOG_DEBUG", string.format("%03d: 0x%02X %c", i, v, v))
  end

  -- convert GSM7 decoded buffer to Unicode
  unicode_buf = self:decode_unicode_from_gsm7(gsm7_buf)
  self.l.log("LOG_DEBUG", string.format("*** Unicode decoded buffer ***"))
  for i, v in ipairs(unicode_buf) do
    self.l.log("LOG_DEBUG", string.format("%03d: 0x%04X %c", i, v, v))
  end

  -- convert Unicode buffer to UTF-8
  utf8_buf = self:decode_utf8_from_unicode(unicode_buf)
  self.l.log("LOG_DEBUG", string.format("*** UTF8 decoded buffer ***"))
  for i, v in ipairs(utf8_buf) do
    self.l.log("LOG_DEBUG", string.format("%03d: 0x%02x %c", i, v, v))
  end

  return utf8_buf
end

--[[
  data  : complete input data buffer
  i     : index from where sms data starts (after UDL and UDH)
  udl   : data length including UDH
  udh   : UDH length
  coding_scheme: data coding scheme:
    0 : GSM 7 bit
    1 : 8 bit data
    2 : UCS-2 Big Endian encoding
  Returns UTF8 encoded string or binary data table
--]]
function QmiNetcomm:parse_sms(data, i, udl, udh_hlen, coding_scheme)
  local sms_text = ""

  if coding_scheme == self.coding_scheme.GSM7 then
     sms_text = self:parse_gsm_7bit_sms(data, i, udl, udh_hlen)
  else
    local enc_data = {}
    local data_length = udl - udh_hlen

    -- we can not concat cdata, so copy to lua table
    for j=1, data_length do
      enc_data[j] = string.char(data[i+j-1])
    end

    if coding_scheme == self.coding_scheme.UCS2 then
      local ic, err = self.iconv:new("UTF8", "UCS-2BE")
      if ic == nil then
        self.l.log("LOG_ERR", err)
      else
        sms_text, err = ic:convert(table.concat(enc_data))
        ic:finish()
      end
    else
      -- sms with 8 bit data
      -- we do not convert the data as it can be binary/mixed data for another
      -- sms type, e.g WAP messages
      -- for text it can be considered as UTF8, but text is either GSM7 bit or
      -- UCS-2 encoded, unless a carrier explicitly specifies a format for 8 bit sms.
      sms_text = table.concat(enc_data)
    end
  end

  return sms_text
end

-- stub to parse etws message
function QmiNetcomm:parse_etws_message(qm)
  local hexdump=self:cbin2hex(qm.resp.transfer_route_mt_message.data, qm.resp.transfer_route_mt_message.data_len)
  self.l.log("LOG_WARNING","ETWS Message: "..hexdump)

  if qm.resp.etws_plmn_info_valid == 1 then
    self.l.log("LOG_WARNING", string.format("ETWS PLMN INFO: MCC:%s, MNC:%s",
      qm.resp.etws_plmn_info.mobile_country_code, qm.resp.etws_plmn_info.mobile_network_code))
  end
end

-- stub to parse call control info
function QmiNetcomm:parse_call_control_info(qm)
  local hexdump=self:cbin2hex(qm.resp.transfer_route_mt_message.data, qm.resp.transfer_route_mt_message.data_len)
  self.l.log("LOG_WARNING","CALL CONTROL INFO: "..hexdump)
end

-- get the smsc type and address configured in the modem
function QmiNetcomm:get_smsc_info()
  local qm = self.luaq.new_msg(self.luaq.m.QMI_WMS_GET_SMSC_ADDRESS)
  self.luaq.send_msg(qm)

  if qm.resp.resp.result ~= 0 then
    self.l.log("LOG_WARNING", string.format("failed to get smsc address, result: %s, error: %s",
      tonumber(qm.resp.resp.result), tonumber(qm.resp.resp.error)))
    self.smsc = nil
  else
    self.smsc = {smsc_type = self.ffi.string(qm.resp.smsc_address.smsc_address_type),
                 address = self.ffi.string(qm.resp.smsc_address.smsc_address_digits,
                   qm.resp.smsc_address.smsc_address_digits_len)}
  end
  return self.smsc
end

QmiNetcomm.cbs={
  "QMI_WMS_EVENT_REPORT_IND"
}

function QmiNetcomm:poll_signal_info(type, event)
  if self.m["QMI_NETCOMM_GET_SIGNAL_INFO"] then
    -- request netcomm signal info
    local succ,err,resp=self.luaq.req(self.m.QMI_NETCOMM_GET_SIGNAL_INFO, {
      radio_if=0x08, -- LTE
    })

    -- get result
    if succ then
      self.l.log("LOG_DEBUG",string.format("QMI_NETCOMM_GET_SIGNAL_INFO : valid = %d, sinr = %d",resp.sinr_valid,resp.sinr))
    else
      self.l.log("LOG_DEBUG","QMI_NETCOMM_GET_SIGNAL_INFO failed") -- happens regularly if disconnected
    end

    -- build ia
    local sinr_valid = self.luaq.is_c_true(resp.sinr_valid) and succ

    local ia={
      sinr_valid=sinr_valid,
      sinr=sinr_valid and resp.sinr or nil
    }

    -- invoke
    return self.watcher.invoke("sys","modem_on_netcomm_signal_info",ia)
  end
end

function QmiNetcomm:poll_rat_meas_info(type, event, a)
  self.l.log("LOG_DEBUG","QMI_NETCOMM_DIAGNOSTICS: GET_RAT_MEAS_INFO")

  local success, error, table, resp, cast

  success, error, resp, table, cast = self:getDiagnosticData("DIAG_GET_RAT_MEAS_INFO_V01", "CMAPI_SYS_MODE_LTE_V01")

  self.l.log("LOG_DEBUG", "error       = "..tostring(error))
  self.l.log("LOG_DEBUG", "success     = "..tostring(success))
  self.l.log("LOG_DEBUG", "cmapi_error = "..tonumber(resp.cmapi_error))

  if not success or not cast then
    return false
  end

  self.watcher.invoke("sys", "update_channel_quality_indicator", {value = cast.cqi})
  self.watcher.invoke("sys", "update_transmission_mode",         {value = cast.transmission_mode})

  -- This condition is to satisfy JIRA MAG-545.
  -- Arris requires that DLBandwidth and ULBandwidth display the number of resource blocks
  -- instead of the MHz. QDIAG writes the expected values to RDB upon network connection, but gets
  -- overwritten by wmmd  with the 2 lines below. Thus, it has been conditionalized, to not
  -- overwrite the values for magpie/myna/cci (SAS devices).
#if !defined V_CUSTOM_FEATURE_PACK_myna && !defined V_CUSTOM_FEATURE_PACK_magpie && !defined V_CUSTOM_FEATURE_PACK_cci
  self.watcher.invoke("sys", "update_download_bandwidth",        {value = cast.dl_bandwidth})
  self.watcher.invoke("sys", "update_upload_bandwidth",          {value = cast.ul_bandwidth})
#endif

  self.watcher.invoke("sys", "update_scell_parameters",          {channel="servcell_info.scell.rsrq", value = cast.scell_rsrq})

  return true
end

function QmiNetcomm:poll_signal_info2(type, event, a)
  self.l.log("LOG_DEBUG","QMI_NETCOMM_DIAGNOSTICS: GET_SIGNAL_INFO")

  local success, error, table, resp, cast

  success, error, resp, table, cast = self:getDiagnosticData("DIAG_GET_SIGNAL_INFO_V01", "CMAPI_SYS_MODE_LTE_V01")

  self.l.log("LOG_DEBUG", "error       = "..tostring(error))
  self.l.log("LOG_DEBUG", "success     = "..tostring(success))
  self.l.log("LOG_DEBUG", "cmapi_error = "..tonumber(resp.cmapi_error))

  if not success or not cast then
    return false
  end

  -- Modem reports value in 1/10th of dB. Report RDB value in dB.
  if cast.tx_power_PUCCH then
    self.watcher.invoke("sys", "update_transmission_power", {channel="PUCCH", value = cast.tx_power_PUCCH / 10})
  end
  if cast.tx_power_PUSCH then
    self.watcher.invoke("sys", "update_transmission_power", {channel="PUSCH", value = cast.tx_power_PUSCH / 10})
  end
  if cast.tx_power_PRACH then
    self.watcher.invoke("sys", "update_transmission_power", {channel="PRACH", value = cast.tx_power_PRACH / 10})
  end
  if cast.tx_power_SRS then
    self.watcher.invoke("sys", "update_transmission_power", {channel="SRS",   value = cast.tx_power_SRS / 10})
  end

  -- do for each RX path (4 total)
  if cast.pcell_rsrp then
    for i=0,3 do
      local rsrp_val
      if cast.pcell_rsrp[i] == -440 or cast.pcell_rsrp[i] == 0 then
        rsrp_val = ''
      else
        rsrp_val = cast.pcell_rsrp[i] / 10  -- 1/10 dBm resolution
      end
      self.watcher.invoke("sys", "update_scell_parameters", {channel=string.format("servcell_info.pcell.rsrp[%d]", i), value = rsrp_val})
    end
  end

  if cast.scell_cell_id
      and cast.scell_earfcn
      and cast.scell_sinr
      and cast.scell_tx_power_PUCCH
      and cast.scell_tx_power_PUSCH
      and cast.scell_rsrp then
    -- do for each secondary cell (5 total)
    for i=0,4 do
      self.watcher.invoke("sys", "update_scell_parameters", {channel=string.format("servcell_info.scell[%d].cell_id", i), value = cast.scell_cell_id[i]})
      self.watcher.invoke("sys", "update_scell_parameters", {channel=string.format("servcell_info.scell[%d].earfcn", i), value = cast.scell_earfcn[i]})
      self.watcher.invoke("sys", "update_scell_parameters", {channel=string.format("servcell_info.scell[%d].sinr", i), value = cast.scell_sinr[i] / 5 - 20})  -- range: -20 to 30 dBm, 1/5 dBm resolution
      self.watcher.invoke("sys", "update_scell_parameters", {channel=string.format("servcell_info.scell[%d].tx_power_PUCCH", i), value = cast.scell_tx_power_PUCCH[i] / 10})  -- 1/10 dBm resolution
      self.watcher.invoke("sys", "update_scell_parameters", {channel=string.format("servcell_info.scell[%d].tx_power_PUSCH", i), value = cast.scell_tx_power_PUSCH[i] / 10})  -- 1/10 dBm resolution

      -- do for each RX path (4 total)
      for j=0,3 do
        local rsrp_val
        if cast.scell_rsrp[5*i+j] == -440 or cast.scell_rsrp[5*i+j] == 0 then
          rsrp_val = ''
        else
          rsrp_val = cast.scell_rsrp[5*i+j] / 10  -- 1/10 dBm resolution
        end
        self.watcher.invoke("sys", "update_scell_parameters", {channel=string.format("servcell_info.scell[%d].rsrp[%d]", i, j), value = rsrp_val})
      end
    end
  end

  return true
end

function QmiNetcomm:poll_lte_embms_info(type, event, a)
  self.l.log("LOG_DEBUG","QMI_NETCOMM_DIAGNOSTICS: GET_LTE_EMBMS_INFO")

  local success, error, table, resp, cast

  success, error, resp, table, cast = self:getDiagnosticData("DIAG_GET_LTE_EMBMS_INFO_V01", "CMAPI_SYS_MODE_LTE_V01")

  self.l.log("LOG_DEBUG", "error       = "..tostring(error))
  self.l.log("LOG_DEBUG", "success     = "..tostring(success))
  self.l.log("LOG_DEBUG", "cmapi_error = "..tonumber(resp.cmapi_error))

  if not success or not cast then
    return false
  end

  self.watcher.invoke("sys", "update_mcs", {value = cast.mcs, length = cast.mcs_count})

  return true
end

function QmiNetcomm:poll_serials(type, event)
  if self.m["QMI_NETCOMM_SERIAL_READ"] then
    local succ,err,resp

    local ia={}

    local f, svn = io.open("/lib/firmware/svn"), 1
    if f then
      svn = f:read("*line")
      f:close()
    end

    succ,err,resp=self.luaq.req(self.m.QMI_NETCOMM_VERSION_WRITE, {
      apps_ver=self.wrdb:get("sw.version"),
      modem_svn=tonumber(svn,16) or 1
    })
    self.l.log("LOG_NOTICE","IMEISV_SVN="..svn..", res "..err)

    succ,err,resp=self.luaq.req(self.m.QMI_NETCOMM_SERIAL_READ)
    if resp.additional_valid==1 and resp.length>0 and resp.length<self.ffi.sizeof(resp.additional) then
      ia=self.turbo.escape.json_decode(
        self.ffi.string(resp.additional, resp.length))
    else
      self.l.log("LOG_ERR","serial information not available")
    end

    return self.watcher.invoke("sys","modem_on_additional_serials",ia)
  end
end

function QmiNetcomm:poll(type, event, a)
  self.l.log("LOG_DEBUG","qmi netcomm  poll")

  self.watcher.invoke("sys","poll_rat_meas_info", a)
  self.watcher.invoke("sys","poll_signal_info2", a)
  -- self.watcher.invoke("sys","poll_lte_embms_info", a)

  return true
end

function QmiNetcomm:poll_quick(type, event, a)
  self.l.log("LOG_DEBUG","qmi netcomm quick poll")

  return self.watcher.invoke("sys","poll_signal_info", a)
end

QmiNetcomm.cbs_system={
  "poll_signal_info",
  "poll_rat_meas_info",
  "poll_signal_info2",
  "poll_lte_embms_info",
  "poll_serials",
  "poll",
  "poll_quick"
}

function QmiNetcomm:init()

  self.l.log("LOG_INFO", "initiate qmi_netcomm")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- initiate qmi
  self.luaq.req(self.m.QMI_WMS_SET_EVENT_REPORT,{
    report_mt_message=1,
  })

  -- scan leftover partial SMS messages
  self:scan_message_folder()

end


function QmiNetcomm:configurePowerCut( gpio, activeLevel )
  return self.luaq.req( self.m.QMI_NETCOMM_CONFIGURE_POWER_CUT,{power_cut_gpio=gpio, power_cut_active_level=activeLevel } )
end

function QmiNetcomm:setCbrsTxPowerCutback( theBand, power_dBm, disableCutback )
  return self.luaq.req( self.m.QMI_NETCOMM_SET_CBRS_TX_POWER_CUTBACK,{ band               = theBand,
                                                                       power_cutback_db10 = power_dBm*10,
                                                                       disable_cutback    = disableCutback } )
end


--[[
  Read a single sms from the specified storage at the specified index.
  var storage_type: Memory storage. Values:
  - 0x00 - STORAGE_TYPE_UIM
  - 0x01 - STORAGE_TYPE_NV
  var storage_index: index to read sms from
]]--
function QmiNetcomm:raw_read_sms(storage_type, storage_index)
  local qm = self.luaq.new_msg(self.luaq.m.QMI_WMS_RAW_READ)
  qm.req.message_memory_storage_identification.storage_type = storage_type
  qm.req.message_memory_storage_identification.storage_index = storage_index
  qm.req.message_mode_valid = 1
  qm.req.message_mode = self.message_mode.MESSAGE_MODE_GW
  self.luaq.send_msg(qm)

  if qm.resp.resp.result == 0 then
    return qm
  end

  return false
end


--[[
  Write a sms to the specified storage.
  var storage_type: Memory storage. Values:
  - 0x00 - STORAGE_TYPE_UIM
  - 0x01 - STORAGE_TYPE_NV
  var data: Complete valid sms pdu inclusive of all headers and data
]]--
function QmiNetcomm:write_sms(storage_type, data)
  local qm = self.luaq.new_msg(self.luaq.m.QMI_WMS_RAW_WRITE)
  qm.req.tag_type_valid = 1
  qm.req.tag_type = self.tag_type.MT_NOT_READ
  qm.req.raw_message_write_data.storage_type = storage_type
  qm.req.raw_message_write_data.format = self.message_format.GW_PP
  qm.req.raw_message_write_data.raw_message_len = #data

  -- raw_message is of type char[255]
  for i=0,254 do
    qm.req.raw_message_write_data.raw_message[i] = data[i+1] or 0
    --self.l.log("LOG_DEBUG", string.format("%d : %02X: %02X", i, data[i+1] or 0,
    --  qm.req.raw_message_write_data.raw_message[i]))
  end
  self.luaq.send_msg(qm)

  if qm.resp.resp.result ~= 0 then
    self.l.log("LOG_WARNING", string.format("write sms failed, result: %s, error: %s",
      tonumber(qm.resp.resp.result), tonumber(qm.resp.resp.error)))
    return false
  end

  return true
end


--[[
  Delete sms from the specified storage at the specified index.
  var storage_type: Memory storage. Values:
  - 0x00 - STORAGE_TYPE_UIM
  - 0x01 - STORAGE_TYPE_NV
  var storage_index: index to read sms from
  Refer to "QMI_WMS_DELETE REQ" in Qualcomm WMS spec for more detail.
]]--
function QmiNetcomm:delete_sms_at_index(storage_type, storage_index)
  local qm = self.luaq.new_msg(self.luaq.m.QMI_WMS_DELETE)
  qm.req.storage_type = storage_type
  qm.req.message_mode_valid = 1
  qm.req.message_mode = self.message_mode.MESSAGE_MODE_GW
  qm.req.index_valid = 1
  qm.req.index = storage_index
  self.luaq.send_msg(qm)
end


return QmiNetcomm
