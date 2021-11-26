-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_ims.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiImsUt = UnitTest:new()
QmiImsUt.name = "QmiIms-UnitTest"

function QmiImsUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiImsUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:enable_ims_registration
    {
      class = "OnWatchCbTest",
      name = "sys:enable_ims_registration watcher callback",
      delay = 10000,
      type = "sys", event = "enable_ims_registration",
      arg = {
        ims_test_mode = "",
      },
      expectList = {
      },
    },
    -- sys:poll_ims_pdp_stat
    {
      class = "OnWatchCbTest",
      name = "sys:poll_ims_pdp_stat watcher callback",
      delay = 1000,
      type = "sys", event = "poll_ims_pdp_stat",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_ims_reg_stat
    {
      class = "OnWatchCbTest",
      name = "sys:poll_ims_reg_stat watcher callback",
      delay = 1000,
      type = "sys", event = "poll_ims_reg_stat",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_ims_serv_stat
    {
      class = "OnWatchCbTest",
      name = "sys:poll_ims_serv_stat watcher callback",
      delay = 1000,
      type = "sys", event = "poll_ims_serv_stat",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_ims_reg_mgr_config
    {
      class = "OnWatchCbTest",
      name = "sys:poll_ims_reg_mgr_config watcher callback",
      delay = 1000,
      type = "sys", event = "poll_ims_reg_mgr_config",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_ims
    {
      class = "OnWatchCbTest",
      name = "sys:poll_ims watcher callback",
      delay = 1000,
      type = "sys", event = "poll_ims",
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
    -- qmi:QMI_IMSA_PDP_STATUS_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_IMSA_PDP_STATUS_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_IMSA_PDP_STATUS_IND",
      arg = {
        resp = {
          is_ims_pdp_connected_valid = 1,
          is_ims_pdp_connected = 1,
          ims_pdp_failure_error_code = 0,
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_IMSA_REGISTRATION_STATUS_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_IMSA_REGISTRATION_STATUS_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_IMSA_REGISTRATION_STATUS_IND",
      arg = {
        resp = {
          ims_registered = "test",
          ims_registration_failure_error_code = 0,
          ims_reg_status = 2,
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_IMSA_SERVICE_STATUS_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_IMSA_SERVICE_STATUS_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_IMSA_SERVICE_STATUS_IND",
      arg = {
        resp = {
          sms_service_rat = 1,
          sms_service_status = 2,
        },
      },
      expectList = {
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiImsUt = QmiImsUt:new()
  qmiImsUt:setup()
  qmiImsUt:run()

end

return QmiImsUt