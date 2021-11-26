-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_dms.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiDmsUt = UnitTest:new()
QmiDmsUt.name = "QmiDms-UnitTest"

function QmiDmsUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiDmsUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:poll_modem_info
    {
      class = "OnWatchCbTest",
      name = "sys:poll_modem_info watcher callback",
      delay = 10000,
      type = "sys", event = "poll_modem_info",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll
    {
      class = "OnWatchCbTest",
      name = "sys:poll watcher callback",
      delay = 3000,
      type = "sys", event = "poll",
      arg = nil,
      expectList = {
      },
    },
    -- sys:stop
    {
      class = "OnWatchCbTest",
      name = "sys:stop watcher callback",
      type = "sys", event = "stop",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_operating_mode
    {
      class = "OnWatchCbTest",
      name = "sys:poll_operating_mode watcher callback",
      delay = 1000,
      type = "sys", event = "poll_operating_mode",
      arg = nil,
      expectList = {
      },
    },
    -- sys:online
    {
      class = "OnWatchCbTest",
      name = "sys:online watcher callback",
      delay = 1000,
      type = "sys", event = "online",
      arg = nil,
      expectList = {
      },
    },
    -- sys:lowpower
    {
      class = "OnWatchCbTest",
      name = "sys:lowpower watcher callback",
      delay = 1000,
      type = "sys", event = "lowpower",
      arg = nil,
      expectList = {
      },
    },
    -- sys:shutdown
    {
      class = "OnWatchCbTest",
      name = "sys:shutdown watcher callback",
      delay = 1000,
      type = "sys", event = "shutdown",
      arg = nil,
      expectList = {
      },
    },
    -- sys:offline
    {
      class = "OnWatchCbTest",
      name = "sys:offline watcher callback",
      delay = 1000,
      type = "sys", event = "offline",
      arg = {},
      expectList = {
      },
    },
    -- sys:reset
    {
      class = "OnWatchCbTest",
      name = "sys:reset watcher callback",
      delay = 1000,
      type = "sys", event = "reset",
      arg = nil,
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_DMS_EVENT_REPORT_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_DMS_EVENT_REPORT_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_DMS_EVENT_REPORT_IND",
      arg = {
        resp = {
          operating_mode_valid = 1,
          operating_mode = 0x00,
        },
      },
      expectList = {
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiDmsUt = QmiDmsUt:new()
  qmiDmsUt:setup()
  qmiDmsUt:run()

end

return QmiDmsUt