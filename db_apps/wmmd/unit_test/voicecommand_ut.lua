-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- unit test for voicecommand.lua

local modName = ...

local UnitTest = require("ut_base")

local VoiceCommandUt = UnitTest:new()
VoiceCommandUt.name = "VoiceCommand-UnitTest"

VoiceCommandUt.mockWatcherCbs = {
  sys = {
    "stop_dtmf",
    "start_dtmf",
    "end_call",
    "reject_call",
    "answer_call",
    "dial_call",
    "dial_ecall",
    "manage_calls",
    "manage_ip_calls",
    "get_call_forwarding",
    "set_sups_service",
  },
}

VoiceCommandUt.observedRdb = {
  require("wmmd.config").rdb_g_prefix .. "voice.command.noti",
}

function VoiceCommandUt:initiateModules()
  self.voiceCommand = require("wmmd.voicecommand"):new()
  self.voiceCommand:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.voiceCommand:init()
end

function VoiceCommandUt:setupTests()
  -- "voice.command.ctrl" commands
  local testData = {
    {
      class = "OnRdbTest",
      name = "STOP_DTMF rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "voice STOP_DTMF 99",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "stop_dtmf",
          arg = {call_id = 99},
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "START_DTMF rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "voice START_DTMF 99 77",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "start_dtmf",
          arg = {call_id = 99, digit = "77"},
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "HANGUP rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 HANGUP 99",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "end_call",
          arg = {call_id = 99},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 HANGUP 99 ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "REJECT rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 REJECT 99 88",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "reject_call",
          arg = {call_id = 99, reject_cause = "88"},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 REJECT 99 ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "ANSWER rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 ANSWER 99",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "answer_call",
          arg = {call_id = 99},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 ANSWER 99 ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "CALL rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 CALL 99 88",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "dial_call",
          arg = {calling_number = "99", timeout = 88},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 CALL ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "ECALL rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 ECALL 99 88",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "dial_ecall",
          arg = {calling_number = "99", timeout = 88},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 ECALL ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "MANAGE_CALLS rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 MANAGE_CALLS sometype 88 99",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "manage_calls",
          arg = {sups_type = "SOMETYPE", call_id = 88, reject_cause = "99"},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 MANAGE_CALLS ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "MANAGE_IP_CALLS rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 MANAGE_IP_CALLS sometype 88 99",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "manage_ip_calls",
          arg = {sups_type = "SOMETYPE", call_id = 88, reject_cause = "99"},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 MANAGE_IP_CALLS ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "SUPPL_FORWARD rdb command",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 SUPPL_FORWARD reason STATUS",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "get_call_forwarding",
          arg = {reason = "REASON"},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 SUPPL_FORWARD REASON STATUS ERR",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "SUPPL_FORWARD rdb command (set_sups_service)",
      rdbName = self.config.rdb_g_prefix .. "voice.command.ctrl",
      rdbValue = "1 SUPPL_FORWARD reason service number noreplytimer",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "set_sups_service",
          arg = {reason = "REASON", service = "SERVICE", number = "NUMBER", no_reply_timer = "NOREPLYTIMER"},
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "1 SUPPL_FORWARD REASON SERVICE ERR",
        },
      },
    },
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_dtmf watcher callback",
      type = "sys", event = "modem_on_dtmf",
      arg = {call_id=99,action="ACTION",digit="DIGIT"},
      expectList = {
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "0 DTMF_NOTIFY 99 ACTION DIGIT",
        },
      },
    },
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_call watcher callback",
      type = "sys", event = "modem_on_call",
      arg = {call_id=88,call_dir="DIR",call_type="TYPE",call_state="STATE",alert_type="ATYPE"},
      expectList = {
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. "voice.command.noti",
          value = "0 CALL_NOTIFY 88 DIR TYPE STATE ATYPE  ",
        },
      },
    },
  }
  self:installTestList(testData)

end

if not modName then
  local voiceCommandUt = VoiceCommandUt:new()
  voiceCommandUt:setup()
  voiceCommandUt:run()

end

return VoiceCommandUt
