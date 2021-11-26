-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_ssctl.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiSsctlUt = UnitTest:new()
QmiSsctlUt.name = "QmiSsctl-UnitTest"

function QmiSsctlUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiSsctlUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:poll
    {
      class = "OnWatchCbTest",
      name = "sys:poll watcher callback",
      delay = 10000,
      type = "sys", event = "poll",
      arg = nil,
      expectList = {
      },
    },
    -- sys:shutdown_subsystem
    {
      class = "OnWatchCbTest",
      name = "sys:shutdown_subsystem watcher callback",
      delay = 1000,
      type = "sys", event = "shutdown_subsystem",
      arg = nil,
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_SSCTL_RESTART_READY_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_SSCTL_RESTART_READY_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_SSCTL_RESTART_READY_IND",
      arg = {},
      expectList = {
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiSsctlUt = QmiSsctlUt:new()
  qmiSsctlUt:setup()
  qmiSsctlUt:run()

end

return QmiSsctlUt