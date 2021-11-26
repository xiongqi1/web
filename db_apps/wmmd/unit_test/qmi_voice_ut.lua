-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_voice.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiVoiceUt = UnitTest:new()
QmiVoiceUt.name = "QmiVoice-UnitTest"

function QmiVoiceUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiVoiceUt:setupTests()
  self.ffi = require("ffi")

  -- sys watcher callbacks
  local testData = {
    -- sys:stop_dtmf
    {
      class = "OnWatchCbTest",
      name = "sys:stop_dtmf watcher callback",
      delay = 10000,
      type = "sys", event = "stop_dtmf",
      arg = {call_id=1},
      expectList = {
      },
    },
    -- sys:start_dtmf
    {
      class = "OnWatchCbTest",
      name = "sys:start_dtmf watcher callback",
      delay = 1000,
      type = "sys", event = "start_dtmf",
      arg = {call_id=1, digit="1245678"},
      expectList = {
      },
    },
    -- sys:set_sups_service
    {
      class = "OnWatchCbTest",
      name = "sys:set_sups_service watcher callback",
      delay = 1000,
      type = "sys", event = "set_sups_service",
      arg = {
        qmi_lua_sid = 1,
        reason = "UNCONDITIONAL",
        service = "ACTIVATE",
      },
      expectList = {
      },
    },
    -- sys:get_call_forwarding
    {
      class = "OnWatchCbTest",
      name = "sys:get_call_forwarding watcher callback",
      delay = 1000,
      type = "sys", event = "get_call_forwarding",
      arg = {
        qmi_lua_sid = 1,
        reason = "UNCONDITIONAL",
      },
      expectList = {
      },
    },
    -- sys:reject_call
    {
      class = "OnWatchCbTest",
      name = "sys:reject_call watcher callback",
      delay = 1000,
      type = "sys", event = "reject_call",
      arg = {call_id=1},
      expectList = {
      },
    },
    -- sys:answer_call
    {
      class = "OnWatchCbTest",
      name = "sys:answer_call watcher callback",
      delay = 1000,
      type = "sys", event = "answer_call",
      arg = {call_id=1, qmi_lua_sid=1},
      expectList = {
      },
    },
    -- sys:end_call
    {
      class = "OnWatchCbTest",
      name = "sys:end_call watcher callback",
      delay = 1000,
      type = "sys", event = "end_call",
      arg = {call_id=1, qmi_lua_sid=1},
      expectList = {
      },
    },
    -- sys:dial_call
    {
      class = "OnWatchCbTest",
      name = "sys:dial_call watcher callback",
      delay = 1000,
      type = "sys", event = "dial_call",
      arg = {
        calling_number = "123456",
        timeout = 3,
        qmi_lua_sid = 1,
      },
      expectList = {
      },
    },
    -- sys:dial_ecall
    {
      class = "OnWatchCbTest",
      name = "sys:poll dial_ecall callback",
      delay = 1000,
      type = "sys", event = "dial_ecall",
      arg = {
        calling_number = "123456",
        timeout = 3,
        qmi_lua_sid = 1,
      },
      expectList = {
      },
    },
    -- sys:manage_calls
    {
      class = "OnWatchCbTest",
      name = "sys:manage_calls watcher callback",
      delay = 1000,
      type = "sys", event = "manage_calls",
      arg = {
        call_id = 1,
        sups_type = "RELEASE_HELD_OR_WAITING",
        reject_cause = "USER_BUSY",
      },
      expectList = {
      },
    },
    -- sys:manage_ip_calls
    {
      class = "OnWatchCbTest",
      name = "sys:manage_ip_calls watcher callback",
      delay = 1000,
      type = "sys", event = "manage_ip_calls",
      arg = {
        call_id = 1,
        sups_type = "RELEASE_HELD_OR_WAITING",
        reject_cause = "USER_BUSY",
      },
      expectList = {
      },
    },
    -- sys:poll
    {
      class = "OnWatchCbTest",
      name = "sys:poll watcher callback",
      type = "sys", event = "poll",
      arg = nil,
      expectList = {
      },
    },
    -- sys:stop
    {
      class = "OnWatchCbTest",
      name = "sys:cell_lock watcher callback",
      type = "sys", event = "cell_lock",
      arg = nil,
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_VOICE_DTMF_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_VOICE_DTMF_IND watcher callback",
      type = "qmi", event = "QMI_VOICE_DTMF_IND",
      arg = {
        resp = {
          dtmf_info = {
            dtmf_event = "a",
            digit_buffer_len = 1,
            digit_buffer = "1",
            call_id = 1,
          }
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_VOICE_ALL_CALL_STATUS_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_VOICE_ALL_CALL_STATUS_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_VOICE_ALL_CALL_STATUS_IND",
      arg = {
        resp = {
          remote_party_number_valid = 1,
          remote_party_number_len = 1,
          remote_party_number = {
            [0] = {
              call_id = 1,
              number = self.ffi.new("char[10]","12345"),
              number_len = 5,
              number_pi = 0,
            }
          },
          remote_party_name_valid = 1,
          remote_party_name_len = 1,
          remote_party_name = {
            [0] = {
              call_id = 1,
              name = self.ffi.new("char[10]","Name"),
              name_len = 4,
              name_pi = 0,

            }
          },
          ip_caller_name_valid = 1,
          ip_caller_name_len = 1,
          ip_caller_name = {
            [0] = {
              call_id = 1,
              ip_caller_name = { 84, 101, 115, 116, 105, 110, 103, 32, 68, 101, 109, 111, 44, 32, 65 },
              ip_caller_name_len = 15,
            }
          },
          alerting_type_valid = 1,
          alerting_type_len = 1,
          alerting_type = {
            [0] = {
              call_id = 1,
              alerting_type = 1,
            }
          },
          call_info_len = 1,
          call_info = {
            [0] = {
              call_id = 1,
              call_state = 1,
              call_type = 0,
              direction = 1,
            }
          }
        },
      },
      expectList = {
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiVoiceUt = QmiVoiceUt:new()
  qmiVoiceUt:setup()
  qmiVoiceUt:run()

end

return QmiVoiceUt