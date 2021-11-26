-- ---------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2017 NetComm Wireless limited.
--
-- ---------------------------------------------------------------------------------------------------------------------

-- base class for unit test

local UnitTest = require("wmmd.Class"):new()

function UnitTest:setup()
  self.rdbWatch = require("wmmd.RdbWatch")
  self.rdb = require("wmmd.wmmd_rdb"):new()
  self.dConfig = require("wmmd.dconfig"):new()
  self.watcher = require("wmmd.watcher")
  self.config = require("wmmd.config")
  self.smachine = require("wmmd.smachine")
  self.t = require("turbo")
  self.ioLoop = self.t.ioloop.instance()

  self.rdb:setup(self.rdbWatch)
  self.dConfig:setup(self.rdbWatch, self.rdb)
  self.rdb:init()
  self.dConfig:init()

  if self.name == nil then self.name = "UnitTest" end
  self.testTimeoutMs = 5000

  self.stateMachineHandlers = {
    {state="idle",func="stateMachineIdle", execObj=self},
    {state="initiate",func="stateMachineInitiate", execObj=self},
    {state="operate",func="stateMachineOperate", execObj=self},
    {state="finalise",func="stateMachineFinalise", execObj=self},
  }
  self.pass = 0
  self.tests = 0
  self.testList = {}
  self.curTest = nil
  self.timerRef = nil
end

function UnitTest:print(msg)
  print(string.format("[%s] %s", self.name, msg))
end

UnitTest.mockWatcherCbs = {
  -- sys={ },
}

UnitTest.observedRdb = {

}

UnitTest.ExpectType = {
  EXPECT_WATCH_CB = 1,
  EXPECT_RDB_NOTIFIED = 2,
  EXPECT_RDB_WRITTEN = 3,
  EXPECT_USER_DEFINED = 4,
}

-- class represents an expected event where a watcher allback is invoked
UnitTest.WatchCbInvoked = require("wmmd.Class"):new()
UnitTest.WatchCbInvoked.expectType = UnitTest.ExpectType.EXPECT_WATCH_CB

-- setup a WatchCbInvoked instance
-- Parameter arg is a table:
--    type: type of event
--    event: event
--    arg: the argument that callers will pass to the callback
--    retVal: return value that the callback needs to return
--    deepCmp: if true apply deep compare on arguments
function UnitTest.WatchCbInvoked:setup(arg)
  self.type = arg.type
  self.event = arg.event
  self.arg = arg.arg
  self.retVal = arg.retVal
  self.deepCmp = arg.deepCmp
end

-- deep compare parameters
-- return true if parameters are equal
-- Parameters:
--    param1, param2: parameters to compare
--    ignoreMt: if true ignore param1's __eq metamethod
local function compare(param1, param2, ignoreMt)
  local type1 = type(param1)
  local type2 = type(param2)
  if type1 ~= type2 then return false end
  -- directly compare non-table types
  if type1 ~= "table" and type2 ~= "table" then return param1 == param2 end
  if #param1 ~= #param2 then return false end
  -- directly comapre tables which have the metamethod __eq
  local mt = getmetatable(param1)
  if not ignoreMt and mt and mt.__eq then return param1 == param2 end
  -- otherwise
  for k1,v1 in pairs(param1) do
    local v2 = param2[k1]
    if v2 == nil or not compare(v1,v2) then return false end
  end
  return true
end

-- return true if input type, event, and argument match with current instance
function UnitTest.WatchCbInvoked:validate(eventType, event, arg)
  if self.deepCmp then
    return self.type == eventType and self.event == event and compare(self.arg, arg)
  else
    if self.type ~= eventType or self.event ~= event or type(self.arg) ~= type(arg) then
      return false
    end

    if type(self.arg) == "table" then
      for k,v in pairs(self.arg) do
        if self.arg[k] ~= arg[k] then
          return false
        end
      end
      return true
    else
      return self.arg == arg
    end
  end

end

-- class represents an expected event of RDB notification
UnitTest.RdbNotified = require("wmmd.Class"):new()
UnitTest.RdbNotified.expectType = UnitTest.ExpectType.EXPECT_RDB_NOTIFIED

-- Parameter arg:
--    name: name of RDB variable which is subject of notification
--    value: expected RDB value
function UnitTest.RdbNotified:setup(arg)
  self.name = arg.name
  self.value = arg.value
end

-- return true if input name and value argument match with current instance
function UnitTest.RdbNotified:validate(name, value)
  return self.name == name and self.value == value
end

-- class represents an expected event of RDB variable getting updated
UnitTest.RdbWritten = require("wmmd.Class"):new()
UnitTest.RdbWritten.expectType = UnitTest.ExpectType.EXPECT_RDB_WRITTEN

-- Parameter arg:
--    name: name of RDB variable to be written
--    value: expected RDB value
function UnitTest.RdbWritten:setup(arg)
  self.name = arg.name
  self.value = arg.value
end

-- return true if input name and value argument match with current instance
function UnitTest.RdbWritten:validate()
  return require("luardb").get(self.name) == self.value
end

-- class represent user-defined event
UnitTest.OnUserDefined = require("wmmd.Class"):new()
UnitTest.OnUserDefined.expectType = UnitTest.ExpectType.EXPECT_USER_DEFINED

-- Parameters arg:
--    exec: callback function to be called to validate
--    arg: argument given to exec
function UnitTest.OnUserDefined:setup(arg)
  self.exec = arg.exec
  self.arg = arg
end

-- validate user-defined event
function UnitTest.OnUserDefined:validate()
  return self.exec ~= nil and self.exec(self.arg)
end

-- mock watcher callback
function UnitTest:mockWatcherCbHandler(type, event, arg)
  if self.curTest == nil then
    return
  end

  local expect = self.curTest.expectList[1]

  if expect == nil
    or expect.expectType ~= self.ExpectType.EXPECT_WATCH_CB
    or not expect:validate(type, event, arg) then
  --self:print("Unexpected Watcher event " .. type .. "." .. event )
  else
    table.remove(self.curTest.expectList, 1)
    self:checkResult()
    return expect.retVal
  end

end

-- mock RDB notification handler
function UnitTest:rdbNotify(name, value)
  if self.curTest == nil then return end
  local expect = self.curTest.expectList[1]
  if expect == nil
    or expect.expectType ~= self.ExpectType.EXPECT_RDB_NOTIFIED
    or not expect:validate(name, value) then
  --self:print("Unexpected RDB notification " .. name .. " = " .. value )
  else
    self.rdb:set(name)
    table.remove(self.curTest.expectList, 1)
    self:checkResult()
  end
end

function UnitTest:initiateModules()
-- should add module-under-test here

end

function UnitTest:stateMachineIdle()
  self.sm.switch_state_machine_to("initiate")
end

function UnitTest:stateMachineInitiate()
  self:initiateModules()
  self.sm.switch_state_machine_to("operate")
end

function UnitTest:stateMachineOperate()
  self:setupTests()
  self:runTests()
end

function UnitTest:stateMachineFinalise()
  self.ioLoop:close()
  self:printFinalResult()
end

function UnitTest:run()

  for _,v in pairs(self.observedRdb) do
    self.rdbWatch:addObserver(v, "rdbNotify", self)
  end

  for typeK,typesTable in pairs(self.mockWatcherCbs) do
    for _,event in pairs(typesTable) do
      self.watcher.add(typeK, event, self, "mockWatcherCbHandler")
    end
  end

  self.sm = self.smachine.new_smachine("ut",self.stateMachineHandlers)
  self.sm.switch_state_machine_to("idle")

  -- start turbo
  self.ioLoop:start()
end

function UnitTest:markTest()
  self:print("Testing: " .. self.curTest.name)
  self.tests = self.tests + 1
end

function UnitTest:markPass()
  self.pass = self.pass + 1
end

function UnitTest:activateTimeoutTimer(msec)
  local timeout
  if msec ~= nil then
    timeout = self.t.util.gettimemonotonic() + msec
  else
    timeout = self.t.util.gettimemonotonic() + self.testTimeoutMs
  end
  self.timerRef=self.ioLoop:add_timeout(timeout, self.onTimeout, self)
end

function UnitTest:checkResult()
  if self.curTest ~= nil then
    if #self.curTest.expectList == 0 then
      self:markPass()
      self:print(self.curTest.name .. ": Pass")

      self:runTests()
    else
      local expect = self.curTest.expectList[1]
      if expect.expectType == UnitTest.ExpectType.EXPECT_RDB_WRITTEN
        or expect.expectType == UnitTest.ExpectType.EXPECT_USER_DEFINED then
        self.curTest:checkExpect()
      end
    end
  end
end

function UnitTest:printFinalResult()
  self:print("---------------------------")
  self:print("Pass/Total tests: " .. self.pass .. "/" .. self.tests)
  self:print("===========================")
end

function UnitTest:onTimeout()
  self:print(self.curTest.name .. ": Timeout")
  self.timerRef = nil
  self:runTests()
end

function UnitTest:setupTests()
-- setup tests here
end

function UnitTest:installTestList(testList)
  for _,v in pairs(testList) do
    local test = self[v.class]:new()
    test:setup(self, v)
    for _,expect in pairs(v.expectList) do
      local expObj = self[expect.class]:new()
      expObj:setup(expect)
      table.insert(test.expectList, expObj)
    end
    table.insert(self.testList, test)
  end
end

function UnitTest:runTests()
  if self.timerRef ~= nil then
    self.ioLoop:remove_timeout(self.timerRef)
    self.timerRef = nil
  end
  self.curTest = nil

  if #self.testList ~= 0 then
    local test = self.testList[1]
    if test.delay then
      self.ioLoop:add_timeout(self.t.util.gettimemonotonic() + test.delay, self.runTests, self)
      test.delay = nil
      return
    end

    self.curTest = table.remove(self.testList, 1)
    self:markTest()
    self.curTest:initiate()
    self.curTest:run()
    self.curTest:checkExpect()
  else
    self:finish()
  end
end

function UnitTest:finish()
  self.sm.switch_state_machine_to("finalise")
end

-- Class represents a test or a step of a test
UnitTest.Test = require("wmmd.Class"):new()

-- setup Test instance
-- Parameter:
--  base: reference to parent unit test instance
--  arg: a table:
--      name: name of test/step
--      delay: nil or delay in ms
function UnitTest.Test:setup(base, arg)
  self.base = base
  self.expectList = {}
  self.name = arg.name
  self.delay = arg.delay
end
function UnitTest.Test:initiate()
end
function UnitTest.Test:run()
end

-- check expected result
function UnitTest.Test:checkExpect()
  if self.checkingExpectTimer ~= nil then
    self.ioLoop:remove_timeout(self.checkingExpectTimer)
    self.checkingExpectTimer = nil
  end

  while #self.expectList ~= 0 do
    local expect = self.expectList[1]
    if expect.expectType == UnitTest.ExpectType.EXPECT_RDB_WRITTEN or expect.expectType == UnitTest.ExpectType.EXPECT_USER_DEFINED then
      if expect:validate() then
        table.remove(self.expectList, 1)
      else
        self.base.checkingExpectTimer = self.base.ioLoop:add_timeout(self.base.t.util.gettimemonotonic() + 1000, self.checkExpect, self)
        return
      end
    else
      break
    end
  end
  self.base:checkResult()
end

-- Class represent a test/step where RDB variable is written
UnitTest.OnRdbTest = UnitTest.Test:new()

-- Parameter:
--  base: reference to parent unit test
--  arg: table:
--        rdbName: rdb variable name to be written
--        rdbValue: value
function UnitTest.OnRdbTest:setup(base, arg)
  UnitTest.Test.setup(self, base, arg)
  self.rdbName = arg.rdbName
  self.rdbValue = arg.rdbValue
end
function UnitTest.OnRdbTest:run()
  self.base:activateTimeoutTimer()
  self.base.rdb:set(self.rdbName, self.rdbValue)
end

-- Class represent a test/step where a watcher callback is invoked
UnitTest.OnWatchCbTest = UnitTest.Test:new()
-- Parameter:
--  base: reference to parent unit test
--  arg: table:
--        type, event: type, event of watcher callback
--        arg: argument passed to the callback
function UnitTest.OnWatchCbTest:setup(base, arg)
  UnitTest.Test.setup(self, base, arg)
  self.type = arg.type
  self.event = arg.event
  self.arg = arg.arg
end

function UnitTest.OnWatchCbTest:run()
  self.base:activateTimeoutTimer()
  self.base.watcher.invoke(self.type, self.event, self.arg)
end

return UnitTest
