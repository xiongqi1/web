-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- unit test for sms.lua

local modName = ...

local UnitTest = require("ut_base")

local SmsUt = UnitTest:new()
SmsUt.name = "Sms-UnitTest"

function SmsUt:initiateModules()
  self.sms = require("wmmd.sms"):new()
  self.sms:setup(self.rdb)
  self.sms:init()
end

function SmsUt:setupTests()
  -- watcher callbacks

  local arg = {}

  -- mwi rdb keys
  local mwi_rdb_keys = {
    "voicemail",
    "fax",
    "email",
    "other",
    "videomail"
  }

  -- mwi rdb subkeys
  local mwi_rdb_subkeys = {
    "count",
    "active",
  }

  for _,key in pairs(mwi_rdb_keys) do
    arg[key] = {}
    for _,subkey in pairs(mwi_rdb_subkeys) do
      arg[key][subkey] = key .. subkey .. "data"
    end
  end

  local expects = {}
  for k,v in pairs(arg) do
    for _,sk in ipairs(mwi_rdb_subkeys) do
      local exp = {}
      exp.class  = "RdbWritten"
      exp.name = string.format("%s.%s.%s",self.config.rdb_g_prefix .. self.sms.mwi_rdb_prefix,k,sk)
      exp.value = v[sk]
      table.insert(expects, exp)
    end
  end

  -- set voice mail for backward compatibility
  if arg.voicemail then
    local exp = {}
    exp.class  = "RdbWritten"
    exp.name = self.config.rdb_g_prefix .. "sms.received_message.vmstatus"
    exp.value = arg.voicemail.active and "active" or "inactive"
    table.insert(expects, exp)
  end


  local watcherTestData = {
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_mwi watcher callback",
      type = "sys", event = "modem_on_mwi",
      arg = arg,
      expectList = expects,
    },
  }
  self:installTestList(watcherTestData)


end

if not modName then
  local smsUt = SmsUt:new()
  smsUt:setup()
  smsUt:run()

end

return SmsUt
