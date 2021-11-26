-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_tmd.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiTmdUt = UnitTest:new()
QmiTmdUt.name = "QmiTmd-UnitTest"

function QmiTmdUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiTmdUt:setupTests()
  self.ffi = require("ffi")
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
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_TMD_MITIGATION_LEVEL_REPORT_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_TMD_MITIGATION_LEVEL_REPORT_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_TMD_MITIGATION_LEVEL_REPORT_IND",
      arg = {
        resp = {
          mitigation_device = {
            mitigation_dev_id = self.ffi.new("char[10]", "123456789")
          },
          current_mitigation_level = 1,
        },
      },
      expectList = {
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiTmdUt = QmiTmdUt:new()
  qmiTmdUt:setup()
  qmiTmdUt:run()

end

return QmiTmdUt