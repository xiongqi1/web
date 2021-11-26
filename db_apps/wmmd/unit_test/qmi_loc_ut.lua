-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_loc.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiLocUt = UnitTest:new()
QmiLocUt.name = "QmiLoc-UnitTest"

function QmiLocUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiLocUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:start_gps
    {
      class = "OnWatchCbTest",
      name = "sys:start_gps watcher callback",
      delay = 10000,
      type = "sys", event = "start_gps",
      arg = nil,
      expectList = {
      },
    },
    -- sys:stop_gps
    {
      class = "OnWatchCbTest",
      name = "sys:stop_gps watcher callback",
      delay = 1000,
      type = "sys", event = "stop_gps",
      arg = nil,
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_LOC_EVENT_POSITION_REPORT_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_LOC_EVENT_POSITION_REPORT_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_LOC_EVENT_POSITION_REPORT_IND",
      arg = {
        resp = {
          sessionId = self.qmiG.qs.qmi_loc.session_id,
          technologyMask_valid = 1,
          technologyMask = 0x0003,
          latitude_valid = 1,
          longitude_valid = 1,
          sessionStatus = 1,
          latitude = 999,
          longitude = 999,
          timestampUtc = 123456,
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_LOC_EVENT_GNSS_SV_INFO_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_LOC_EVENT_GNSS_SV_INFO_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_LOC_EVENT_GNSS_SV_INFO_IND",
      arg = {},
      expectList = {
      },
    },
    -- qmi:QMI_LOC_EVENT_NMEA_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_LOC_EVENT_NMEA_IND watcher callback",
      type = "qmi", event = "QMI_LOC_EVENT_NMEA_IND",
      arg = {},
      expectList = {
      },
    },
    -- qmi:QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND watcher callback",
      type = "qmi", event = "QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND",
      arg = {},
      expectList = {
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiLocUt = QmiLocUt:new()
  qmiLocUt:setup()
  qmiLocUt:run()

end

return QmiLocUt