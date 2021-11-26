-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- singleton class watching rdb and notifying observers

local RdbWatch = {}

RdbWatch.rdb = require("luardb")
RdbWatch.observerList = {}

-- init syslog
local l = require("luasyslog")
pcall(function() l.open("rdbwatch", "LOG_DAEMON") end)

-- class function used as handler to be registered with luardb to watch RDB variables
-- @param rdbName RDB variable of which value changes are subject of notification
-- @param rdbValue RDB variable value
function RdbWatch.watchCallback(rdbName, rdbValue)
  RdbWatch:notifyObserver(rdbName, rdbValue)
end

-- adding an observer to a RDB variable
-- @param rdbName RDB variable to be watched
-- @param funcName Name of member function will be called upon notification of RDB changes
-- @param obj Observer object of which member function will be called upon notification of RDB changes
function RdbWatch:addObserver(rdbName, funcName, obj)
  if self.observerList[rdbName] == nil then
    self.observerList[rdbName] = {}
    self.rdb.watch(rdbName, self.watchCallback)
  end
  table.insert(self.observerList[rdbName], {obj=obj, func=funcName})

  l.log("LOG_DEBUG",string.format("rdbwatch: add observer (rdb=%s,func=%s)",rdbName,funcName))
end

-- notifying observers upon notification of RDB changes
-- @param rdbName RDB variable of which value changes are subject of notification
-- @param rdbValue RDB variable value
function RdbWatch:notifyObserver(rdbName, rdbValue)
  l.log("LOG_DEBUG",string.format("rdbwatch: got event (rdb=%s,val='%s')",rdbName,rdbValue))

  local t = self.observerList[rdbName]
  for _,observer in pairs(t) do
    l.log("LOG_DEBUG",string.format("rdbwatch: notify (rdb=%s,val='%s',func=%s)",rdbName,rdbValue,observer.func))
    observer.obj[observer.func](observer.obj, rdbName, rdbValue)
  end
end

return RdbWatch
