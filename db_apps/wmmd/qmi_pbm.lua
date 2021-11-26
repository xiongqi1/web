--[[
QMI PhoneBook Manager service

Copyright (C) 2017 NetComm Wireless Limited.
--]]

local QmiPbm = require("wmmd.Class"):new()

function QmiPbm:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_pbm", "LOG_DAEMON") end)

  self.rdb = require("luardb")
  self.rdbrpc = require("luardbrpc")
  self.wrdb = wrdb
  self.rdbWatch = rdbWatch
  self.luaq = require("luaqmi")
  self.watcher = require("wmmd.watcher")
  self.m = self.luaq.m
  self.ffi = require("ffi")
  self.config = require("wmmd.config")
  self.t = require("turbo")
  self.i = self.t.ioloop.instance()

  -- local constants

  -- maximum phonebook entries
  self.DEFAULT_MAX_PB_ENTRIES = 250

  -- phonebook location: MF(3F00)/TELECOM(7F10)/Phonebook(5F3A)

  -- PBM Session type
  self.SESSION_TYPE = {
    GW_PRIMARY             = "PBM_SESSION_TYPE_GW_PRIMARY_V01",
    GW_SECONDARY           = "PBM_SESSION_TYPE_GW_SECONDARY_V01",
    NON_PROVISIONING_SLOT1 = "PBM_SESSION_TYPE_NON_PROVISIONING_SLOT1_V01",
    NON_PROVISIONING_SLOT2 = "PBM_SESSION_TYPE_NON_PROVISIONING_SLOT2_V01",
    GLOBAL_PB_SLOT1        = "PBM_SESSION_TYPE_GLOBAL_PB_SLOT1_V01",
    GLOBAL_PB_SLOT2        = "PBM_SESSION_TYPE_GLOBAL_PB_SLOT2_V01",
  }

  -- Phonebook type
  self.PB_TYPE = {
    ADN    = 0x0001, -- Abbreviated Dialing Number
    FDN    = 0x0002, -- Fixed Dialing Number
    MSISDN = 0x0004, -- Mobile Subscriber Integrated Services Digital Network
    MBDN   = 0x0008, -- Mail Box Dialing Number
    SDN    = 0x0010, -- Service Dialing Number
    BDN    = 0x0020, -- Barred Dialing Number
    LND    = 0x0040, -- Last Number Dialed
    MBN    = 0x0080, -- Mail Box Number
    GAS    = 0x0100, -- Grouping information Alpha String
    AAS    = 0x0200, -- Additional number Alpha String
  }

  -- Number type
  self.NUM_TYPE = {
    UNKNOWN = "PBM_NUM_TYPE_UNKNOWN_V01",
    INTERNATIONAL = "PBM_NUM_TYPE_INTERNATIONAL_V01",
    NATIONAL = "PBM_NUM_TYPE_NATIONAL_V01",
    NETWORK_SPECIFIC = "PBM_NUM_TYPE_NETWORK_SPECIFIC_V01",
    DEDICATED_ACCESS = "PBM_NUM_TYPE_DEDICATED_ACCESS_V01",
  }

  -- Number plan
  self.NUM_PLAN = {
    UNKNOWN = "PBM_NUM_PLAN_UNKNOWN_V01",
    ISDN = "PBM_NUM_PLAN_ISDN_V01",
    DATA = "PBM_NUM_PLAN_DATA_V01",
    TELEX = "PBM_NUM_PLAN_TELEX_V01",
    NATIONAL = "PBM_NUM_PLAN_NATIONAL_V01",
    PRIVATE = "PBM_NUM_PLAN_PRIVATE_V01",
  }

  self.DEFAULT_SESSION_TYPE = self.SESSION_TYPE.GLOBAL_PB_SLOT1
  self.DEFAULT_PB_TYPE = self.PB_TYPE.ADN
  self.DEFAULT_NUM_TYPE = self.NUM_TYPE.UNKNOWN
  self.DEFAULT_NUM_PLAN = self.NUM_PLAN.UNKNOWN

  -- Phonebook event bit mask
  self.PBM_REG_EVENTS = {
    RECORD_UPDATE   = 0x01,
    PHONEBOOK_READY = 0x02,
  }

  self.PB_STATE_NAMES = {
    [0x00] = "ready",
    [0x01] = "not ready",
    [0x02] = "not available",
    [0x03] = "PIN restriction",
    [0x04] = "PUK restriction",
    [0x05] = "invalidated",
    [0x06] = "sync",
  }

  self.RDB_PB_PREFIX = "sim.phonebook."
  self.RDB_PB_PREFIX_FULL = self.config.rdb_g_prefix .. self.RDB_PB_PREFIX
  self.RPC_SERVICE_NAME = self.RDB_PB_PREFIX_FULL:sub(1,-2)

  -- local variables

  -- number of records to read (waiting for pbm_record_read_ind)
  self.num_of_recs_to_read = 0
  -- records that have been read
  self.recs_read = {}

  self.rdb_session = nil
  self.rdbfd = -1
  self.rpc_server = nil
end

-- delete phonebook RDB variables under .sim.phonebook.{i}. where id_start<=i<=id_end
-- if id_start=nil, delete all rdbs
-- if id_end=nil, delete a single rdb at id_start
function QmiPbm:delete_phonebook_rdbs(id_start, id_end)
  self.l.log("LOG_DEBUG", string.format("[delete_phonebook_rdbs] IDs [%s, %s]", id_start, id_end))
  local max_recs = tonumber(self.wrdb:getp(self.RDB_PB_PREFIX .. "num")) or 0
  self.l.log("LOG_DEBUG", string.format("[delete_phonebook_rdbs] before deletion max_recs=%d", max_recs))

  if id_start then
    if id_start < 1 then
      self.l.log("LOG_ERR", string.format("[delete_phonebook_rdbs] id_start=%d is invalid", id_start))
      return false
    end
    if id_end then
      if id_start > id_end then
        self.l.log("LOG_ERR", string.format("[delete_phonebook_rdbs] id_start=%d is bigger than id_end=%d", id_start, id_end))
        return false
      else
        if id_start <= max_recs and max_recs <= id_end then
          max_recs = id_start -1
        end
      end
    else
      if max_recs == id_start then
        max_recs = id_start - 1
      end
    end
  else
    max_recs = 0
  end
  self.l.log("LOG_DEBUG", string.format("[delete_phonebook_rdbs] after deletion max_recs=%d", max_recs))

  local regex = "^" .. self.RDB_PB_PREFIX_FULL .. "(%d+)."
  -- escape lua regex (replace "." by "%.")
  regex = regex:gsub("%.", "%%%.")
  local rdbs = self.rdb.keys(self.RDB_PB_PREFIX_FULL)
  for _, rdb in ipairs(rdbs) do
    local ix = tonumber(rdb:match(regex))
    if ix and ((not id_start) or (not id_end and id_start == ix) or (id_start and id_end and id_start <= ix and ix <= id_end)) then
      self.l.log("LOG_DEBUG", string.format("[delete_phonebook_rdbs] deleting %s", rdb))
      self.wrdb:unset(rdb)
    end
  end
  self.wrdb:setp(self.RDB_PB_PREFIX .. "num", max_recs)
end

function QmiPbm:update_phonebook_rdbs(a)
  local max_recs = tonumber(self.wrdb:getp(self.RDB_PB_PREFIX .. "num")) or 0
  for id, rec in pairs(a) do
    self.wrdb:setp(self.RDB_PB_PREFIX .. id .. ".name", rec.name)
    self.wrdb:setp(self.RDB_PB_PREFIX .. id .. ".number", rec.number)
    if id > max_recs then
      max_recs = id
    end
  end
  self.wrdb:setp(self.RDB_PB_PREFIX .. "num", max_recs)
end

function QmiPbm:complete_read()
  if self.rpc_server == nil then
    self.l.log("LOG_WARNING", "[complete_read] rpc server is already destroyed")
    return
  end
  self.rpc_server:complete_async(true)
end

--[[ No used at the moment since we use poll (QMI_PBM_GET_PB_STATE)
function QmiPbm:QMI_PBM_PB_READY_IND(type, event, qm)
  local info = qm.resp.phonebook_ready_info
  if info.session_type == self.DEFAULT_SESSION_TYPE and info.pb_type == self.DEFAULT_PB_TYPE then
    self.watcher.invoke("sys", "modem_on_phonebook_ready")
  end
  return true
end
--]]

function QmiPbm:QMI_PBM_RECORD_READ_IND(type, event, qm)
  local data = qm.resp.basic_record_data
  --data.seq_num, data.session_type, data.pb_type, data.record_instance_len, data.record_instances
  --record.number, record.name
  if data.session_type ~= self.DEFAULT_SESSION_TYPE or data.pb_type ~= self.DEFAULT_PB_TYPE then
    self.l.log("LOG_WARNING", "[pbm_record_read_ind] session or pb type mismatch. ignored")
    return true
  end
  if self.num_of_recs_to_read == 0 then
    self.l.log("LOG_WARNING", "[pbm_record_read_ind] unexpected record. ignored")
    return true
  end
  local num_recs = tonumber(data.record_instances_len)
  self.l.log("LOG_DEBUG", string.format("[pbm_record_read_ind] num_recs=%d", num_recs))
  for i = 0, num_recs-1 do
    local record = data.record_instances[i]
    local record_id = tonumber(record.record_id)
    local name = self.ffi.string(record.name, record.name_len)
    local number = self.ffi.string(record.number, record.number_len)
    self.recs_read[record_id] = {name = name, number = number}
    self.l.log("LOG_DEBUG", string.format("[pbm_record_read_ind] id=%d, #name=%d, #number=%d", record_id, #name, #number))
  end
  self.num_of_recs_to_read = self.num_of_recs_to_read - num_recs
  if self.num_of_recs_to_read <= 0 then
    -- all indications have arrived
    self:update_phonebook_rdbs(self.recs_read)
    self:complete_read()
    -- we are done. prepare for further reading
    self.num_of_recs_to_read = 0
    self.recs_read = {}
  end
  return true
end

-- QMI callbacks
QmiPbm.cbs={
  -- "QMI_PBM_PB_READY_IND",
  "QMI_PBM_RECORD_READ_IND",
}

function QmiPbm:poll_phonebook_status(type, event)
  -- two things to be queried: pb_state, pb_capabilities
  self.l.log("LOG_DEBUG", "qmi pbm status polling")
  local qm = self.luaq.new_msg(self.m.QMI_PBM_GET_PB_STATE)
  qm.req.phonebook_info.session_type = self.DEFAULT_SESSION_TYPE
  qm.req.phonebook_info.pb_type = self.DEFAULT_PB_TYPE
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", "QMI_PBM_GET_PB_STATE send_msg failed")
    return
  end
  local succ, qerr, resp = self.luaq.ret_qm_resp(qm)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_PBM_GET_PB_STATE failed, error=0x%02x", qerr))
    return succ
  end
  if not self.luaq.is_c_true(resp.phonebook_state_valid) then
    self.l.log("LOG_ERR", "phonebook_state is not available")
    return
  end
  local state = tonumber(resp.phonebook_state.state)
  local ia = {state = self.PB_STATE_NAMES[state]}
  succ = self.watcher.invoke("sys", "modem_on_phonebook_state", ia)
  if not succ then
    return succ
  end
  if ia.state ~= "ready" then
    return
  end
  succ = self.watcher.invoke("sys", "get_pb_capabilities")
  return succ
end

function QmiPbm:get_pb_capabilities(type, event)
  self.l.log("LOG_DEBUG", "qmi pbm get pb capabilities")
  local qm = self.luaq.new_msg(self.m.QMI_PBM_GET_PB_CAPABILITIES)
  qm.req.phonebook_info.session_type = self.DEFAULT_SESSION_TYPE
  qm.req.phonebook_info.pb_type = self.DEFAULT_PB_TYPE
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", "QMI_PBM_GET_PB_CAPABILITIES send_msg failed")
    return
  end
  local succ, qerr, resp = self.luaq.ret_qm_resp(qm)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_PBM_GET_PB_CAPABILITIES failed, error=0x%02x", qerr))
    return succ
  end
  if not self.luaq.is_c_true(resp.capability_basic_info_valid) then
    self.l.log("LOG_WARNING", "PBM capability basic info unavailable")
    return
  end
  local info = resp.capability_basic_info
  local ia = {max_records = info.max_records,
              used_records = info.used_records}
  return self.watcher.invoke("sys", "modem_on_phonebook_capabilities", ia)
end

-- system callbacks
QmiPbm.cbs_system={
  "poll_phonebook_status",
  "get_pb_capabilities",
}

-- read phonebook records from id_start to id_end
-- all pb rdbs within the range will be cleared first
-- return false on failure; true on success
-- the number of records awaiting indication will be stored in
-- self.num_of_recs_to_read
function QmiPbm:pb_read(id_start, id_end)
  self.l.log("LOG_DEBUG", "qmi pbm read pb records")
  if self.num_of_recs_to_read > 0 then
    self.l.log("LOG_NOTICE", "previous reading is in progress. please retry later")
	return false
  end
  self.recs_read = {}
  local qm = self.luaq.new_msg(self.m.QMI_PBM_READ_RECORDS)
  qm.req.record_info.session_type = self.DEFAULT_SESSION_TYPE
  qm.req.record_info.pb_type = self.DEFAULT_PB_TYPE
  qm.req.record_info.record_start_id = id_start
  qm.req.record_info.record_end_id = id_end
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", "QMI_PBM_READ_RECORDS send_msg failed")
    return false
  end
  local succ, qerr, resp = self.luaq.ret_qm_resp(qm)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_PBM_READ_RECORDS failed, error=0x%02x", qerr))
    return succ
  end
  if self.luaq.is_c_true(resp.num_of_recs_valid) then
    self.num_of_recs_to_read = tonumber(resp.num_of_recs)
    self.l.log("LOG_DEBUG", string.format("[pb_read] num_of_recs=%d", self.num_of_recs_to_read))
    -- only delete existing RDBs when we are successful
    self:delete_phonebook_rdbs(id_start, id_end)
    -- read and readall are asynchronous commands. Normally, complete_async will
    -- be called in RECORD_READ_IND processing.
    -- But if there is no record in SIM, there will be no RECORD_READ_IND,
    -- and we need to call complete_async after the handler returns true.
    if self.num_of_recs_to_read == 0 then
      self.i:add_callback(QmiPbm.complete_read, self)
    end
  else
    self.l.log("LOG_ERR", "[pb_read] num_of_recs missing from resp")
    self.num_of_recs_to_read = 0
    succ = false
  end
  return succ
end

-- read all phonebook records
-- all pb rdbs will be cleared first
-- return false on failure; true on success
-- the number of records awaiting indication will be stored in
-- self.num_of_recs_to_read
function QmiPbm:pb_readall()
  local max_records = tonumber(self.wrdb:getp(self.RDB_PB_PREFIX .. "max_records")) or self.DEFAULT_MAX_PB_ENTRIES
  return self:pb_read(1, max_records)
end

-- write phonebook records from id_start to id_end from rdbs to sim
-- return true on success; false on failure
function QmiPbm:pb_write(id_start, id_end)
  self.l.log("LOG_DEBUG", "qmi pbm write pb records")
  local succ, qerr, resp
  local max_recs = tonumber(self.wrdb:getp(self.RDB_PB_PREFIX .. "num")) or 0
  local qm = self.luaq.new_msg(self.m.QMI_PBM_WRITE_RECORD)
  qm.req.record_information.session_type = self.DEFAULT_SESSION_TYPE
  qm.req.record_information.phonebook_type = self.DEFAULT_PB_TYPE
  qm.req.record_information.num_type = self.DEFAULT_NUM_TYPE
  qm.req.record_information.num_plan = self.DEFAULT_NUM_PLAN

  local ret = true
  -- qmi-pbm only supports writing one record at a time
  for ix = id_start, id_end do
    local rdb = self.RDB_PB_PREFIX .. ix
    local name = self.wrdb:getp(rdb .. ".name")
    local number = self.wrdb:getp(rdb .. ".number")
    if name and number then
      -- both name and number are mandatory, skip if not
      qm.req.record_information.record_id = ix
      qm.req.record_information.number_len = #number
      self.ffi.copy(qm.req.record_information.number, number)
      qm.req.record_information.name_len = #name
      self.ffi.copy(qm.req.record_information.name, name)
      if not self.luaq.send_msg(qm) then
        self.l.log("LOG_ERR", string.format("QMI_PBM_WRITE_RECORD send_msg failed, id=%d", ix))
        succ = false
      else
        succ, qerr, resp = self.luaq.ret_qm_resp(qm)
        if not succ then
          self.l.log("LOG_ERR", string.format("QMI_PBM_WRITE_RECORD failed, id=%d, error=0x%02x", ix, qerr))
        else
          self.l.log("LOG_DEBUG", string.format("QMI_PBM_WRITE_RECORD succeeded, id=%d", ix))
          if ix > max_recs then
            max_recs = ix
          end
        end
      end
      ret = ret and succ
    else
      self.l.log("LOG_WARNING", string.format("[pb_write] Record %d does not have name or number. Skipped", ix))
    end
  end
  -- update num even for partial success
  self.wrdb:setp(self.RDB_PB_PREFIX .. "num", max_recs)
  return ret
end

-- delete phonebook records from id_start to id_end
-- return true on success; false on failure
function QmiPbm:pb_delete(id_start, id_end)
  self.l.log("LOG_DEBUG", "qmi pbm delete pb records")
  local succ, qerr, resp
  local max_recs = tonumber(self.wrdb:getp(self.RDB_PB_PREFIX .. "num")) or 0
  local qm = self.luaq.new_msg(self.m.QMI_PBM_DELETE_RECORD)
  qm.req.record_info.session_type = self.DEFAULT_SESSION_TYPE
  qm.req.record_info.pb_type = self.DEFAULT_PB_TYPE

  local ret = true
  -- qmi-pbm only supports deleting one record at a time
  for ix = id_start, id_end do
    qm.req.record_info.record_id = ix
    if not self.luaq.send_msg(qm) then
      self.l.log("LOG_ERR", string.format("QMI_PBM_DELETE_RECORD send_msg failed, id=%d", ix))
      succ = false
    else
      succ, qerr, resp = self.luaq.ret_qm_resp(qm)
      if not succ then
        if qm.resp.resp.error == "QMI_ERR_INVALID_ID_V01" then
          self.l.log("LOG_INFO", string.format("QMI_PBM_DELETE_RECORD, invalid id=%d ignored", ix))
          succ = true
        else
          self.l.log("LOG_ERR", string.format("QMI_PBM_DELETE_RECORD failed, id=%d, error=0x%02x", ix, qerr))
        end
      else
        self.l.log("LOG_DEBUG", string.format("QMI_PBM_DELETE_RECORD succeeded, id=%d", ix))
      end
    end
    if succ then
      local rdb_rec_prefix = self.RDB_PB_PREFIX .. ix .. "."
      self.wrdb:unsetp(rdb_rec_prefix .. "name")
      self.wrdb:unsetp(rdb_rec_prefix .. "number")
      if ix == max_recs then
        max_recs = ix - 1
      end
    else
      ret =  succ
    end
  end
  -- update num even for partial success
  self.wrdb:setp(self.RDB_PB_PREFIX .. "num", max_recs)
  return ret
end

-- delete all phonebook records
-- return true on success; false on failure
function QmiPbm:pb_deleteall()
  self.l.log("LOG_DEBUG", "qmi pbm delete all pb records")
  local qm = self.luaq.new_msg(self.m.QMI_PBM_DELETE_ALL_PB_RECORDS)
  qm.req.phonebook_info.session_type = self.DEFAULT_SESSION_TYPE
  qm.req.phonebook_info.pb_type = self.DEFAULT_PB_TYPE
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", "QMI_PBM_DELETE_ALL_PB_RECORDS send_msg failed")
    return false
  end
  local succ, qerr, resp = self.luaq.ret_qm_resp(qm)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_PBM_DELETE_ALL_PB_RECORDS failed, error=0x%02x", qerr))
    return succ
  end
  self.l.log("LOG_INFO", string.format("QMI_PBM_DELETE_ALL_PB_RECORDS succeeded"))
  self:delete_phonebook_rdbs()
  return succ
end

-- rdb rpc handler
function QmiPbm:pb_ops(cmd, params, result_len)
  local succ
  local id_start = tonumber(params["start"])
  local id_end = tonumber(params["end"])
  self.l.log("LOG_DEBUG", string.format("[pb_ops] %s, IDs [%s, %s], result_len %s", cmd, id_start, id_end, result_len))

  if cmd == "read" or cmd == "delete" or cmd == "write" then
    if not id_start or id_start <= 0 or not id_end or id_end < id_start then
      self.l.log("LOG_ERR", string.format("[pb_ops] %s wrong IDs [%s, %s]", cmd, id_start, id_end))
      return false
    end
  end

  if cmd == "readall" then
    succ = self:pb_readall()
  elseif cmd == "read" then
    succ = self:pb_read(id_start, id_end)
  elseif cmd == "deleteall" then
    succ = self:pb_deleteall()
  elseif cmd == "delete" then
    succ = self:pb_delete(id_start, id_end)
  elseif cmd == "write" then
    succ = self:pb_write(id_start, id_end)
  else
    self.l.log("LOG_ERR", string.format("[pb_ops] unknown command=%s", cmd))
    succ = false
  end

  return succ
end

-- delete all RDBs under RDB_PB_PREFIX.cmd.
function QmiPbm:cleanup_phonebook_service()
  local rdbs = self.rdb.keys(self.RDB_PB_PREFIX_FULL .. "cmd")
  for _, rdb in pairs(rdbs) do
    self.rdb.unset(rdb)
  end
end

function QmiPbm:start_rpc_server()
  self:cleanup_phonebook_service()
  local succ, ret = pcall(self.rdb.new_rdb_session)
  if not succ then
    -- This should not happen. But if it does, skip the rest.
    self.l.log("LOG_ERR", string.format("[start_rpc_server] %s", ret))
    return
  end
  self.rdb_session = ret
  self.rdbfd = self.rdb_session:get_fd()

  succ, ret = pcall(self.rdbrpc.new, self.RPC_SERVICE_NAME)
  if not succ then
    -- This should not happen. But if it does, skip the rest.
    self.l.log("LOG_ERR", string.format("[start_rpc_server] %s", ret))
    self.rdb_session:destroy()
    self.rdb_session = nil
    self.rdbfd = -1
    return
  end
  self.rpc_server = ret
  self.rpc_server:add_command("read", {"start", "end"}, "async", QmiPbm.pb_ops, self)
  self.rpc_server:add_command("readall", {}, "async", QmiPbm.pb_ops, self)
  self.rpc_server:add_command("write", {"start", "end"}, "sync", QmiPbm.pb_ops, self)
  self.rpc_server:add_command("delete", {"start", "end"}, "sync", QmiPbm.pb_ops, self)
  self.rpc_server:add_command("deleteall", {}, "sync", QmiPbm.pb_ops, self)

  self.rpc_server:run(self.rdb_session)

  local function on_rdb()
    local rdb_names = self.rdb_session:get_names("", self.rdb.TRIGGERED)
    self.rpc_server:process_commands(rdb_names)
  end
  self.i:add_handler(self.rdbfd, self.t.ioloop.READ, on_rdb)
end

function QmiPbm:end_rpc_server()
  self.i:remove_handler(self.rdbfd)
  self.rdbfd = -1
  self.rpc_server:stop()
  self.rpc_server:destroy()
  self.rpc_server = nil
  self.rdb_session:destroy()
  self.rdb_session = nil
end

function QmiPbm:pb_enable(rdbKey, rdbVal)
  -- destroy existing phonebook service if any
  if self.rpc_server then
    self:end_rpc_server()
  end
  if rdbVal == '1' then
    self:poll_phonebook_status()
    self:start_rpc_server()
  end
end

function QmiPbm:init()

  self.l.log("LOG_INFO", "initiate qmi_pbm")

  -- add watcher for qmi
  for _, v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _, v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- start rdb rpc server
  local pb_enabled = self.wrdb:getp(self.RDB_PB_PREFIX .. "enable")
  if pb_enabled == '1' then
    self:start_rpc_server()
  end

  -- register rdb watcher for enable/disable change
  self.rdbWatch:addObserver(self.config.rdb_g_prefix .. self.RDB_PB_PREFIX .. "enable", "pb_enable", self)

  --[[ register indication
  self.luaq.req(self.m.QMI_PBM_INDICATION_REGISTER, {
    req_mask=self.PBM_REG_EVENTS.PHONEBOOK_READY,
  })
  --]]
end

return QmiPbm
