-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_wms.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiWmsUt = UnitTest:new()
QmiWmsUt.name = "QmiWms-UnitTest"

function QmiWmsUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiWmsUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:poll_mwi
    {
      class = "OnWatchCbTest",
      name = "sys:poll_mwi watcher callback",
      delay = 10000,
      type = "sys", event = "poll_mwi",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll
    {
      class = "OnWatchCbTest",
      name = "sys:poll watcher callback",
      delay = 1000,
      type = "sys", event = "poll",
      arg = nil,
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_WMS_MESSAGE_WAITING_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_WMS_MESSAGE_WAITING_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_WMS_MESSAGE_WAITING_IND",
      arg = {
        resp = {
          message_waiting_info_len = 1,
          message_waiting_info = {
            [0] = {
              message_type = 0,
              active_ind = 1,
              message_count = 0,
            }
          }
        },
      },
      expectList = {
      },
    },
  -- commented out as qmi_wms.lua does nothing in receipt of QMI_WMS_EVENT_REPORT_IND, however qmi_netcomm.lua will process it.
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
  local qmiWmsUt = QmiWmsUt:new()
  qmiWmsUt:setup()
  qmiWmsUt:run()

end

return QmiWmsUt