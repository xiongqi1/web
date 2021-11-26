-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- watch module, global callback handler

local modname = ...

local l = require("luasyslog")
pcall(function() l.open("watcher", "LOG_DAEMON") end)

--[[ public member vairables ]]--
local _m

local event_cbinfo_set={} -- event callback information set

local function invoke(type,event,arg)

  local function invoke_cbs(cbinfo_array, type, event, arg)

    local err = false

    for i,cbinfo in ipairs(cbinfo_array) do
      local execObj = cbinfo.execObj
      local funcName = cbinfo.funcName
      local res = execObj[funcName](execObj, type, event, arg, cbinfo.ref)
      err = err or not res
    end

    return not err
  end

  assert(type and event)

  local cevent=type .. ':' .. event
  local gevent=type .. ':'

  local cbinfos = {}

  -- invoke gevent
  if event_cbinfo_set[gevent] then
    l.log("LOG_INFO",string.format("watcher: [%s] invoke global event",event))
    invoke_cbs(event_cbinfo_set[gevent],type,event,arg)
  end

  local succ
  if event_cbinfo_set[cevent] then
    l.log("LOG_DEBUG",string.format("watcher: [%s] invoke request",event))
    succ=invoke_cbs(event_cbinfo_set[cevent],type,event,arg)
  end

  l.log(succ and "LOG_DEBUG" or "LOG_INFO",string.format("watcher: [%s] invoke response = %s",event,succ and "succ" or "fail"))

  return succ
end

local function search(type, event, execObj, funcName)

  local cevent=type .. ':' .. event

  -- get event callback information array
  local cbinfo_array=event_cbinfo_set[cevent]

  -- return nil if no handler exists
  if not cbinfo_array then
    return
  end

  -- search and remove
  local cbinfo_i
  for i,cbinfo in ipairs(cbinfo_array) do
    if cbinfo.execObj == execObj and cbinfo.funcName == funcName then
      cbinfo_i=i
    end
  end

  return cbinfo_array, cbinfo_i
end

local function reset()
  event_cbinfo_set={}
end

-- execObj: object of which member function will be invoked
-- funcName: name of member function of execObj being invoked
local function remove(type, event, execObj, funcName)
  local cbinfo_array, cbinfo_i = search(type, event, execObj, funcName)

  if cbinfo_array and cbinfo_i then
    table.remove(cbinfo_array,cbinfo_i)
  end
end

-- add callback to key
local function add(type, event, execObj, funcName, ref)

  local cevent=type .. ':' .. event

  local cbinfo_array, cbinfo_i = search(type, event, execObj, funcName)
  local cbinfo=cbinfo_array and cbinfo_i and cbinfo_array[cbinfo_i] or nil

  if cbinfo then
    l.log("LOG_ERR",
      string.format("reduplicated event handler not allowed (type=%s, event=%s,cb function name=%s)",
      type, event,funcName))
    return false
  end

  -- allocate a new if not existing
  cbinfo_array=cbinfo_array or {}
  -- chain cb
  table.insert(cbinfo_array,{execObj=execObj, funcName=funcName, ref=ref})

  -- update event callback info set
  event_cbinfo_set[cevent]=cbinfo_array

  return true
end

local function perform_unit_test()

  local onglobal_called
  local onclick_called

  local TestClass = require("wmmd.Class"):new()
  function TestClass:onglobal(type,event,arg,ref)
    print(string.format("onglobal called (type=%s,event=%s,arg=%s,ref=%s)",type,event,arg,ref))

    onglobal_called = true
  end

  function TestClass:onclick(type,event,arg,ref)
    print(string.format("onclick called (type=%s,event=%s,arg=%s,ref=%s)",type,event,arg,ref))

    onclick_called = true
  end

  print("* start unit test")

  local testObj = TestClass:new()

  print("add global handler ['']")
  add("qmi", "", testObj, "onglobal", "global reference")
  print("add click handler ['onclick']")
  add("qmi", "onclick", testObj, "onclick", "click reference")

  print("invoke onclick")
  onglobal_called=false
  onclick_called=false
  invoke("qmi","onclick", "click argument")
  if not (onglobal_called and onclick_called) then
    print("unit test failed")
    return false
  end

  print("invoke global")
  onglobal_called=false
  onclick_called=false
  invoke("qmi","", "global argument")
  if not (onglobal_called and not onclick_called) then
    print("unit test failed")
    return false
  end

  print("remove click handler")
  remove("qmi","onclick", testObj, "onclick")

  print("invoke onclick #2")
  onglobal_called=false
  onclick_called=false
  invoke("qmi","onclick", "click argument #2")
  if not (onglobal_called and not onclick_called) then
    print("unit test failed")
    return false
  end

  print("add click handler ['onclick']")
  add("qmi","onclick", testObj, "onclick", "click reference")
  print("remove global handler")
  remove("qmi", "", testObj, "onglobal")

  print("invoke onclick #3")
  onglobal_called=false
  onclick_called=false
  invoke("qmi","onclick", "click argument #2")
  if not (not onglobal_called and onclick_called) then
    print("unit test failed")
    return false
  end

  print("invoke unknown #3")
  onglobal_called=false
  onclick_called=false
  invoke("qmi","unknown", "click argument #2")
  if not (not onglobal_called and not onclick_called) then
    print("unit test failed")
    return false
  end

  print("* unit test successfully finished")
  return true
end

-- initiate unit test
if not modname then
  l={log=function(logtype,...) print("watcher-unit-test [" .. logtype .. "] " .. ...) end}
end

-- perform unit test
if not modname then
  perform_unit_test()
  return
end

_m={
  invoke=invoke,
  search=search,
  remove=remove,
  add=add,
  reset=reset,
}

return _m
