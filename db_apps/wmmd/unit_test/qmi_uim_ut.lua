-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_uim.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiUimUt = UnitTest:new()
QmiUimUt.name = "QmiUim-UnitTest"

function QmiUimUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiUimUt:setupTests()
  self.ffi = require("ffi")

  -- sys watcher callbacks
  local testData = {
    -- sys:poll_simcard_raw_info
    {
      class = "OnWatchCbTest",
      name = "sys:poll_simcard_raw_info watcher callback",
      delay = 10000,
      type = "sys", event = "poll_simcard_raw_info",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_simcard_info
    {
      class = "OnWatchCbTest",
      name = "sys:poll_simcard_info watcher callback",
      delay = 3000,
      type = "sys", event = "poll_simcard_info",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_uim_card_status
    {
      class = "OnWatchCbTest",
      name = "sys:poll_uim_card_status watcher callback",
      delay = 3000,
      type = "sys", event = "poll_uim_card_status",
      arg = nil,
      expectList = {
      },
    },
    -- sys:auto_pin_verify
    {
      class = "OnWatchCbTest",
      name = "sys:auto_pin_verify watcher callback",
      type = "sys", event = "auto_pin_verify",
      arg = nil,
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_UIM_REFRESH_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_UIM_REFRESH_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_UIM_REFRESH_IND",
      arg = {},
      expectList = {
      },
    },
    -- qmi:QMI_UIM_CARD_ACTIVATION_STATUS_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_UIM_CARD_ACTIVATION_STATUS_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_UIM_CARD_ACTIVATION_STATUS_IND",
      arg = {},
      expectList = {
      },
    },
    -- qmi:QMI_UIM_SLOT_STATUS_CHANGE_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_UIM_SLOT_STATUS_CHANGE_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_UIM_SLOT_STATUS_CHANGE_IND",
      arg = {},
      expectList = {
      },
    },
    -- qmi:QMI_UIM_STATUS_CHANGE_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_UIM_STATUS_CHANGE_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_UIM_STATUS_CHANGE_IND",
      arg = {
        resp = {
          card_status_valid = 1,
          card_status = {
            card_info_len = 1,
            card_info = {
              [0] = {
                card_state = 1,
                error_code = 1,
                app_info_len = 1,
                app_info = {
                  [0] = {
                    app_state = 1,
                    univ_pin = 1,
                    pin1 = {
                      pin_state = 0,
                      pin_retries = 3,
                      puk_retries = 3,
                    }
                  }
                },
                upin = {
                  pin_state = 0,
                  pin_retries = 3,
                  puk_retries = 3,
                }
              }
            }
          }
        }
      },
      expectList = {
      },
    },
    -- rdb observer: sim.cmd.command
    {
      class = "OnRdbTest",
      name = "Observing prefix .. sim.cmd.command",
      delay = 1000,
      rdbName = self.config.rdb_g_prefix.."sim.cmd.command",
      rdbValue = "check",
      expectList = {

      }
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiUimUt = QmiUimUt:new()
  qmiUimUt:setup()
  qmiUimUt:run()

end

return QmiUimUt