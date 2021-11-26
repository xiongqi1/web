-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- unit test for dconfig.lua

local modName = ...

local UnitTest = require("ut_base")

local DConfigUt = UnitTest:new()
DConfigUt.name = "DConfig-UnitTest"

DConfigUt.mockWatcherCbs = {
  sys = {
    "mockcb",
  },
}

DConfigUt.DConfigTest = UnitTest.OnRdbTest:new()
function DConfigUt.DConfigTest:run()
  UnitTest.OnRdbTest.run(self)
  local timeout = self.base.t.util.gettimemonotonic() + 1000
  self.base.ioLoop:add_timeout(timeout, self.checkExpect, self)
end

function DConfigUt:setupTests()
  -- luaqmi is not loaded, define watchdog_val here to avoid related error messages
  watchdog_val = 0

  local testData = {
    {
      class = "DConfigTest",
      name = "enable_poll (false)",
      rdbName = self.dConfig.wmmd_rdb_prefix .. "enable_poll",
      rdbValue = "0",
      expectList = {
        {
          class = "OnUserDefined",
          exec = function() return not self.dConfig.enable_poll end,
        },
      },
    },
    {
      class = "DConfigTest",
      name = "enable_poll (true)",
      rdbName = self.dConfig.wmmd_rdb_prefix .. "enable_poll",
      rdbValue = "1",
      expectList = {
        {
          class = "OnUserDefined",
          exec = function() return self.dConfig.enable_poll end,
        },
      },
    },
    {
      class = "DConfigTest",
      name = "enable_log_qmi_req (true)",
      rdbName = self.dConfig.wmmd_rdb_prefix .. "enable_log_qmi_req",
      rdbValue = "1",
      expectList = {
        {
          class = "OnUserDefined",
          exec = function() return self.dConfig.enable_log_qmi_req end,
        },
      },
    },
    {
      class = "DConfigTest",
      name = "enable_log_qmi_req (false)",
      rdbName = self.dConfig.wmmd_rdb_prefix .. "enable_log_qmi_req",
      rdbValue = "0",
      expectList = {
        {
          class = "OnUserDefined",
          exec = function() return not self.dConfig.enable_log_qmi_req end,
        },
      },
    },
    {
      class = "DConfigTest",
      name = "dtmf_min_interval (150)",
      rdbName = self.dConfig.wmmd_rdb_prefix .. "dtmf_min_interval",
      rdbValue = "150",
      expectList = {
        {
          class = "OnUserDefined",
          exec = function() return self.dConfig.dtmf_min_interval == "150" end,
        },
      },
    },
    {
      class = "DConfigTest",
      name = "dtmf_min_interval (140)",
      rdbName = self.dConfig.wmmd_rdb_prefix .. "dtmf_min_interval",
      rdbValue = "140",
      expectList = {
        {
          class = "OnUserDefined",
          exec = function() return self.dConfig.dtmf_min_interval == "140" end,
        },
      },
    },
  }

  self:installTestList(testData)

end

if not modName then
  local dConfigUt = DConfigUt:new()
  dConfigUt:setup()
  dConfigUt:run()

end

return DConfigUt
