-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- unit test for ril.lua

local modName = ...

local UnitTest = require("ut_base")

local RilUt = UnitTest:new()
RilUt.name = "Ril-UnitTest"

RilUt.mockWatcherCbs = {
  sys = {
    "mockcb",
  },
}

function RilUt:setup()
  UnitTest.setup(self)
  self.observedRdb = {
    self.config.rdb_g_prefix .. self.config.ril_rdb_result,
    self.config.rdb_g_prefix .. self.config.ril_rdb_command,
  }
end

function RilUt:initiateModules()
  self.ril = require("wmmd.ril"):new()
  self.ril:setup(self.rdbWatch, self.rdb)
  self.ril:init()
end

function RilUt:onReady()
  self.sm.switch_state_machine_to("operate")
end

function RilUt:setupTests()

  local testData = {
    {
      class = "OnRdbTest",
      name = "Rdb command (result success)",
      delay = 10000, -- wait a little bit for Ril to enter idle state
      rdbName = self.ril.rdb_command,
      rdbValue = "sys,mockcb,param1=1,param2=2",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "mockcb",
          arg = {param1 = "1", param2 = "2"},
          retVal = true,
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. self.config.ril_rdb_result,
          value = "[DONE] success",
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. self.config.ril_rdb_command,
          value = "",
        },
      },
    },
    {
      class = "OnRdbTest",
      name = "Rdb command (result fail)",
      rdbName = self.ril.rdb_command,
      rdbValue = "sys,mockcb,param1=1,param2=2",
      expectList = {
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "mockcb",
          arg = {param1 = "1", param2 = "2"},
          retVal = false,
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. self.config.ril_rdb_result,
          value = "[ERROR] fail",
        },
        {
          class = "RdbNotified",
          name = self.config.rdb_g_prefix .. self.config.ril_rdb_command,
          value = "",
        },
      },
    },
  }

  self:installTestList(testData)

end

if not modName then
  local rilUt = RilUt:new()
  rilUt:setup()
  rilUt:run()

end

return RilUt
