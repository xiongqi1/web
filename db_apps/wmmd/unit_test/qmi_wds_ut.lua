-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_wds.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiWdsUt = UnitTest:new()
QmiWdsUt.name = "QmiWds-UnitTest"

function QmiWdsUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiWdsUt:setupTests()

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
    -- sys:read_rmnet
    {
      class = "OnWatchCbTest",
      name = "sys:read_rmnet watcher callback",
      type = "sys", event = "read_rmnet",
      arg = {
        profile_index = 3,
        ref = {
          lp_index = 3,
        }
      },
      expectList = {
      },
    },
    -- sys:stop_rmnet
    {
      class = "OnWatchCbTest",
      name = "sys:stop_rmnet watcher callback",
      type = "sys", event = "stop_rmnet",
      arg = {service_id=1},
      expectList = {
      },
    },
    -- sys:write_rmnet
    {
      class = "OnWatchCbTest",
      name = "sys:write_rmnet watcher callback",
      type = "sys", event = "write_rmnet",
      arg = {
        profile_index=3,
        apn="",
        pdp_type="ipv4v6",
        auth_type="",
        user="",
        pass="",
      },
      expectList = {
      },
    },
    -- sys:get_rmnet_stat
    {
      class = "OnWatchCbTest",
      name = "sys:get_rmnet_stat watcher callback",
      type = "sys", event = "get_rmnet_stat",
      arg = {
        service_id = 1,
        info = {},
      },
      expectList = {
      },
    },
    -- sys:start_rmnet
    {
      class = "OnWatchCbTest",
      name = "sys:start_rmnet watcher callback",
      type = "sys", event = "start_rmnet",
      arg = {
        network_interface="interface",
        profile_index=3,
        service_id=3,

        apn="",
        pdp_type="ipv4v6",
        auth_type="",
        user="",
        pass="",

        ipv6_enable=false,
      },
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_WDS_PKT_SRVC_STATUS_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_WDS_PKT_SRVC_STATUS_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_WDS_PKT_SRVC_STATUS_IND",
      arg = {
        resp = {
          ip_family_valid = 1,
          ip_family = 4,
          status = {
            connection_status = 0x02,
            reconfiguration_required = 0,
          },
          call_end_reason_valid = 1,
          call_end_reason = 0x001,
          verbose_call_end_reason_valid = 1,
          verbose_call_end_reason = {
            call_end_reason_type = 0x01,
            call_end_reason = 1,
          }
        },
        qie = {
          qmi_lua_sid = 1,
        }
      },
      expectList = {
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiWdsUt = QmiWdsUt:new()
  qmiWdsUt:setup()
  qmiWdsUt:run()
end

return QmiWdsUt