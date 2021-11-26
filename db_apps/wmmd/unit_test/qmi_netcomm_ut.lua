-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_netcomm.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiNetcommUt = UnitTest:new()
QmiNetcommUt.name = "QmiNetcomm-UnitTest"

function QmiNetcommUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiNetcommUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:poll_signal_info
    {
      class = "OnWatchCbTest",
      name = "sys:poll_signal_info watcher callback",
      delay = 10000,
      type = "sys", event = "poll_signal_info",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_serials
    {
      class = "OnWatchCbTest",
      name = "sys:poll_serials watcher callback",
      delay = 1000,
      type = "sys", event = "poll_serials",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_quick
    {
      class = "OnWatchCbTest",
      name = "sys:poll_quick watcher callback",
      delay = 1000,
      type = "sys", event = "poll_quick",
      arg = nil,
      expectList = {
      },
    },
  -- ----------------------------------
  -- qmi watcher callbacks
  -- ----------------------------------
  -- commented out as the hander is so complex and I infer valid input data from the code.
  --    -- qmi:QMI_WMS_EVENT_REPORT_IND
  --    {
  --      class = "OnWatchCbTest",
  --      name = "qmi:QMI_WMS_EVENT_REPORT_IND watcher callback",
  --      delay = 1000,
  --      type = "qmi", event = "QMI_WMS_EVENT_REPORT_IND",
  --      arg = {},
  --      expectList = {
  --      },
  --    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiNetcommUt = QmiNetcommUt:new()
  qmiNetcommUt:setup()
  qmiNetcommUt:run()

end

return QmiNetcommUt