-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- execution module that manages voice commands from Asterisk channel to modem

local VoiceCommand = require("wmmd.Class"):new()

function VoiceCommand:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("VoiceCommand", "LOG_DAEMON") end)

  self.t = require("turbo")
  self.ffi = require("ffi")

  self.luaq = require("luaqmi")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.wrdb = wrdb
  self.rdb = require("luardb")
  self.rdbWatch = rdbWatch
  self.config = require("wmmd.config")
  self.dconfig = dConfig

  self.i = self.t.ioloop.instance()

  self.v = {}

  self.trans_id = 0

  self.noti_queue = {}
  self.dtmf_stop_timestamp = nil

  self.stateMachineHandlers = {
    {state="voices:idle", func="state_machine_idle", execObj=self},
  }
end

--[[ actions ]]--



--[[ rdb events ]]--

function VoiceCommand.string_split(inputstr)

  local t={}

  for str in string.gmatch(string.upper(inputstr), "([^%s]+)") do
    table.insert(t,str)
  end

  return t
end

function VoiceCommand:process_noti_queue()

  -- remove old noti
  while #self.noti_queue > 100 do
    self.l.log("LOG_ERR",string.format("remove notification (noti='%s')",self.v.rdb_noti,self.noti_queue[1]))
    table.remove(self.noti_queue,1)
  end

  -- bypass if no notification exists
  if #self.noti_queue == 0 then
    return
  end

  -- bypass if notification is pending
  local rdb_val = self.wrdb:get(self.v.rdb_noti)
  if rdb_val and rdb_val ~= "" then
    return
  end

  self.l.log("LOG_DEBUG",string.format("post notification (noti='%s')",self.v.rdb_noti,self.noti_queue[1]))

  -- post first notification
  self.wrdb:set(self.v.rdb_noti,self.noti_queue[1])
  table.remove(self.noti_queue,1)
end

function VoiceCommand:add_noti_to_queue(noti)
  table.insert(self.noti_queue,noti)
  self:process_noti_queue()
end

-----------------------------------------------------------------------------------------------------------------------
-- Sleep for the given period regardless of Linux signals - resume to sleep after processing signal.
--
-- @param ms period in msec to sleep.
function VoiceCommand:msleep_for_dtmf_off_period(ms)

  local now = self.t.util.gettimemonotonic()

  local period_to_sleep
  local start_time = now
  local end_time = start_time + ms

  while true do

    -- sleep until end time
    period_to_sleep = end_time - now
    self.ffi.C.usleep(period_to_sleep*1000)

    -- take current time
    now = self.t.util.gettimemonotonic()

    -- break if now past end time
    if now >= end_time then
      break
    end
  end
end

VoiceCommand.rdb_on_commmands={
  ["INIT_VOICE_CALL"] = function(self, args)
    return self.watcher.invoke("sys","init_voice_call")
  end,

  ["FINI_VOICE_CALL"] = function(self, args)
    return self.watcher.invoke("sys","fini_voice_call")
  end,

  ["STOP_DTMF"] = function(self, args)
    -- XXX STOP_DTMF <call-id>
    local a = {
      call_id=tonumber(args[3]),
    }

    local succ = self.watcher.invoke("sys","stop_dtmf",a)

    self.dtmf_stop_timestamp = self.t.util.gettimemonotonic()
  end,

  ["START_DTMF"] = function(self, args)
    -- XXX START_DTMF <call-id> <digit>
    local a = {
      call_id=tonumber(args[3]),
      digit=args[4],
    }

    -- get DTMF minimum interval, otherwise use 120 by default.
    local dtmf_min_interval = tonumber(self.dconfig.dtmf_min_interval) or 140
    -- allow delay less than 1000 msec
    if dtmf_min_interval>1000 then
      dtmf_min_interval=1000
    end
    local now = self.t.util.gettimemonotonic()
    -- get DTMF time difference from the last stop time stamp
    local dtmf_diff = self.dtmf_stop_timestamp and (now - self.dtmf_stop_timestamp) or nil
    -- get DTMF gap to be needed
    local dtmf_gap =  dtmf_diff or dtmf_min_interval

    self.l.log("LOG_DEBUG",string.format("dtmf time difference from last stop time (diff=%d)",dtmf_diff or 0))

    if (0<=dtmf_gap) and (dtmf_gap<dtmf_min_interval) then
      local dtmf_sleep = dtmf_min_interval - dtmf_gap
      self.l.log("LOG_INFO",string.format("wait for DTMF delay (dtmf_sleep=%d)",dtmf_sleep))
      self:msleep_for_dtmf_off_period(dtmf_sleep)
      self.l.log("LOG_INFO",string.format("send DTMF after delay (dtmf_sleep=%d)",dtmf_sleep))
    else
      self.l.log("LOG_INFO",string.format("immediately send DTMF"))
    end

    local succ = self.watcher.invoke("sys","start_dtmf",a)
  end,

  ["HANGUP"] = function(self, args)
    -- XXX HANGUP <call-id>
    local a = {
      call_id=tonumber(args[3]),
      end_cause=args[4],
    }

    local succ = self.watcher.invoke("sys","end_call",a)

    self:add_noti_to_queue(string.format("%d %s %d %s",args[1],args[2],a.call_id,succ and "OK" or "ERR"))
  end,

  -- REJECT command from RDB
  ["REJECT"] = function(self, args)
    -- XXX REJECT <call-id> <reject-cause>
    local a = {
      call_id=tonumber(args[3]),
      reject_cause=args[4],
    }

    -- invoke reject into qmi voice layer and reply to the client
    local succ = self.watcher.invoke("sys","reject_call",a)
    self:add_noti_to_queue(string.format("%d %s %d %s",args[1],args[2],a.call_id,succ and "OK" or "ERR"))
  end,

  ["ANSWER"] = function(self, args)
    -- XXX ANSWER <call-id>
    local a = {
      call_id=tonumber(args[3]),
      reject_cause=args[4],
    }

    local succ = self.watcher.invoke("sys","answer_call",a)
    self:add_noti_to_queue(string.format("%d %s %d %s",args[1],args[2],a.call_id,succ and "OK" or "ERR"))
  end,

  ["SETUP_ANSWER"] = function(self, args)
    -- XXX SETUP_ANSWER <call-id> <reject-cause>
    local a = {
      call_id=tonumber(args[3]),
      reject_cause=args[4],
    }

    local succ = self.watcher.invoke("sys","setup_answer",a)
    self:add_noti_to_queue(string.format("%d %s %d %s",args[1],args[2],a.call_id,succ and "OK" or "ERR"))
  end,

  ["CALL"] = function(self, args)
    -- 5 CALL 0413237592 10
    local a = {
      calling_number=args[3],
      timeout=tonumber(args[4]),
    }

    local succ = self.watcher.invoke("sys","dial_call",a)
    self:add_noti_to_queue(string.format("%d %s %s",args[1],args[2],succ and "OK" or "ERR"))
  end,

  ["PCALL"] = function(self, args)
    -- 5 CALL 0413237592 10
    local a = {
      calling_number=args[3],
      timeout=tonumber(args[4]),
    }

    local succ = self.watcher.invoke("sys","dial_pcall",a)
    self:add_noti_to_queue(string.format("%d %s %s",args[1],args[2],succ and "OK" or "ERR"))
  end,

  ["OCALL"] = function(self, args)
    -- 5 CALL 0413237592 10
    local a = {
      calling_number=args[3],
      timeout=tonumber(args[4]),
    }

    local succ = self.watcher.invoke("sys","dial_ocall",a)
    self:add_noti_to_queue(string.format("%d %s %s",args[1],args[2],succ and "OK" or "ERR"))
  end,

  ["ECALL"] = function(self, args)
    -- 5 CALL 0413237592 10
    local a = {
      calling_number=args[3],
      timeout=tonumber(args[4]),
    }

    local succ = self.watcher.invoke("sys","dial_ecall",a)
    self:add_noti_to_queue(string.format("%d %s %s",args[1],args[2],succ and "OK" or "ERR"))
  end,

  ["MANAGE_CALLS"] = function(self, args)
    -- XXX MANAGE_CALLS <release_held_or_waiting> <cid>

    local a = {
      sups_type=args[3],
      call_id=tonumber(args[4]),
      reject_cause=args[5],
    }

    if a.call_id == 0 then
      a.call_id = nil
    end

    local succ = self.watcher.invoke("sys","manage_calls",a)
    self:add_noti_to_queue(string.format("%d %s %s",args[1],args[2],succ and "OK" or "ERR"))

  end,

  ["MANAGE_IP_CALLS"] = function(self, args)
    -- XXX MANAGE_CALLS <release_held_or_waiting> <cid>

    local a = {
      sups_type=args[3],
      call_id=tonumber(args[4]),
      reject_cause=args[5],
    }

    if a.call_id == 0 then
      a.call_id = nil
    end

    local succ = self.watcher.invoke("sys","manage_ip_calls",a)
    self:add_noti_to_queue(string.format("%d %s %s",args[1],args[2],succ and "OK" or "ERR"))

  end,

  ["SUPPL_FORWARD"] = function(self, args)
    -- XXX SUPPL_FORWARD <UNCONDITIONAL> <REGISTER>

    if args[4] == "STATUS" then

      local a={
        reason=args[3]
      }

      local succ
      if args[3] == "CALLWAITING" then
        succ = self.watcher.invoke("sys","get_call_waiting",a)
      else
        succ = self.watcher.invoke("sys","get_call_forwarding",a)
      end

      local noti=string.format("%d %s %s %s %s",args[1],args[2],args[3],args[4], succ and "OK" or "ERR")

      if succ then

        if a.status then
          noti = noti .. string.format(" %s",a.status)
        end

        if a.number then
          noti = noti .. string.format(" %s", a.number)
        end

        if a.no_reply_timer then
          noti = noti .. string.format(" %d", a.no_reply_timer)
        end
      end

      self:add_noti_to_queue(noti)
    else

      local a = {
        reason=args[3],
        service=args[4],
        number=args[5],
        no_reply_timer=args[6],
      }

      local succ = self.watcher.invoke("sys","set_sups_service",a)
      self:add_noti_to_queue(string.format("%d %s %s %s %s",args[1],args[2],args[3],args[4], succ and "OK" or "ERR"))

    end
  end,


  -------------------------------------------------------------------------------------------------------------------
  -- Set TTY mode.
  --
  -- @param self VoiceCommand.
  -- @param args RDB arguements in the format of "XXX SET_TTY_MODE <TTY_MODE>".
  ["SET_TTY_MODE"] = function(self, args)

    -- build params
    local params = {
      tty_mode=args[3],
    }

    -- invoke set_tty_mode
    self.l.log("LOG_INFO",string.format("invoke set_tty_mode (tty_mode=%s)",params.tty_mode))
    local succ = self.watcher.invoke("sys","set_tty_mode",params)

    -- send response in RDB
    self:add_noti_to_queue(string.format("%d %s %s",args[1],args[2], succ and "OK" or "ERR"))
  end,
}

function VoiceCommand:rdb_on_ctrl()
  local rdb_val = self.wrdb:get(self.v.rdb_ctrl)

  -- pre-processing rdb ctrl
  if not rdb_val or rdb_val == "" then
    return
  end
  self.wrdb:set(self.v.rdb_ctrl)

  local args = self.string_split(rdb_val)
  local command = args[2]
  local func = command and self.rdb_on_commmands[command] or nil

  if not func then
    self.l.log("LOG_ERR",string.format("unknown voice command (command=%s)",command))
    return
  end

  self.l.log("LOG_INFO",string.format("voice control command (command=%s)",rdb_val))

  -- call command
  func(self, args)
end

function VoiceCommand:rdb_on_noti()
  self:process_noti_queue()
end


--[[ state machine ]]--

function VoiceCommand:state_machine_idle(old_stat,new_stat,stat_chg_info)
end

function VoiceCommand:state_machine_transit(old_stat,new_stat,stat_chg_info)
end

--[[ cbs_system ]]--

function VoiceCommand:modem_on_dtmf(type,event,a)
  local dtmf_notify_str

  if a.volume then
    dtmf_notify_str = string.format("%d %s %d %s %s %s",0,"DTMF_NOTIFY",a.call_id,a.action,a.digit,a.volume)
  else
    dtmf_notify_str = string.format("%d %s %d %s %s",0,"DTMF_NOTIFY",a.call_id,a.action)
  end

  self:add_noti_to_queue(dtmf_notify_str)

  return true
end

function VoiceCommand:modem_on_call(type,event,a)

  local call_notify_str = string.format("%d %s %d %s %s %s %s %s",0,"CALL_NOTIFY",a.call_id,a.call_dir,a.call_type,a.call_state,a.alert_type,a.call_end_reasons)

  call_notify_str = string.format("%s %s %s",call_notify_str,a.number_pi or "",a.number or "")

  if a.name and a.name_pi then
    call_notify_str = string.format("%s %s %s",call_notify_str, a.name_pi or "",string.gsub(a.name or ""," ","\\ "))
  end

  self:add_noti_to_queue(call_notify_str)

  return true
end

function VoiceCommand:poll_voice(type, event, a)
  -- poll voice command - backup if RDB notification is lost.
  self:rdb_on_ctrl()
  self:rdb_on_noti()

  return true
end


VoiceCommand.cbs_system={
  "modem_on_dtmf",
  "modem_on_call",
  "poll_voice",
}

function VoiceCommand:init()

  -- load usleep
  self.ffi.cdef "int usleep(unsigned int usec);"

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- create main state machine
  local sl=self.smachine.new_smachine(
    "voice_smachine", self.stateMachineHandlers
  )

  -- init. voice
  self.v.rdb_ctrl=self.config.rdb_g_prefix .. "voice.command.ctrl"
  self.v.rdb_noti=self.config.rdb_g_prefix .. "voice.command.noti"

  -- add rdb
  self.rdbWatch:addObserver(self.v.rdb_ctrl, "rdb_on_ctrl", self)
  self.rdbWatch:addObserver(self.v.rdb_noti, "rdb_on_noti", self)

  sl.switch_state_machine_to("voices:idle")
end

return VoiceCommand
