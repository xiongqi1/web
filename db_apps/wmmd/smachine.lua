-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- state machine object

local modname = ...

-- init syslog
local l = require("luasyslog")
pcall(function() l.open("smachine", "LOG_DAEMON") end)

local t = require("turbo")
local il = t.ioloop.instance()

local _m
local _sm_tab={}

local function remove_state(sm,state)
  l.log("LOG_DEBUG",string.format("remove state (state=%s)",state))
  sm.state_handlers[state]=nil
end

local function get_current_state(sm)
  return sm.current_state
end

local function add_state(sm,state,func, execObj)
  l.log("LOG_DEBUG",string.format("add state (state=%s)",state))
  sm.state_handlers[state]={}
  sm.state_handlers[state].func=func
  sm.state_handlers[state].execObj=execObj
end

local function get_smachine(smname)
  if not _sm_tab[smname] then
    _sm_tab[smname]={}
  end

  return _sm_tab[smname]
end

local function cancel_pending_switch_requests(sm)

  -- cancel pending switch request
  for _,v in ipairs(sm.timeout_refs) do
    l.log("LOG_INFO",string.format("smachine %s remove timeout #%s",sm.smname,v))
    il:remove_timeout(v)
  end

  sm.timeout_refs={}
end


-----------------------------------------------------------------------------------------------------------------------
-- Change state of a machine.
--
-- @param sm State machine.
-- @param new_stat The new state of state machine to change to.
-- @param msec Delay in milliseconds to switch.
-- @param immediately Immediately change the state with no delay.
--
-- Even when msec is 0, the switching is deferred by 0 timeout unless immediately is true. In result,
-- Any following state change can override msec 0. But "immediately"-ed state change cannot be overridden.
local function switch_state_machine_to(sm,new_stat,msec,immediately)

  assert(sm.state_handlers[new_stat],string.format("no state handler connected (sm=%s,stat=%s)",sm.smname,new_stat))

  if not msec then
    msec = 0
  end

  local function switch_state_machine_to_delayed()
    -- cancel any pending request when triggered
    cancel_pending_switch_requests(sm)

    local mt = t.util.gettimemonotonic()
    if not msec or (msec == 0) then
      l.log("LOG_INFO",string.format("smachine %s [%s ==> %s], monotonic=%d",sm.smname,sm.current_state, new_stat,mt))
    else
      l.log("LOG_INFO",string.format("smachine %s [%s ==> %s] triggered (timeout %d ms), monotonic=%d",sm.smname,sm.current_state, new_stat,msec,mt))
    end

    local old_stat = sm.current_state

    sm.current_state = new_stat

    local handler = sm.state_handlers[new_stat]
    local execObj = handler.execObj
    local func = handler.func
    if execObj ~= nil then
      execObj[func](execObj, old_stat,new_stat,(old_stat == new_stat) and "continue" or "new",sm.ref)
    else
      -- TODO: check type of func, must be a function
      func(old_stat,new_stat,(old_stat == new_stat) and "continue" or "new",sm.ref)
    end

  end

  -- immediately cancel any pending request when requested
  if msec == 0 then
    cancel_pending_switch_requests(sm)
  end

  if immediately then
    switch_state_machine_to_delayed()
  else
    local timeout = t.util.gettimemonotonic()+msec
    local ref=il:add_timeout(timeout,switch_state_machine_to_delayed)
    l.log("LOG_INFO",string.format("smachine %s schedule in %d ms (#%s) %s ==> %s [expiring monotonic=%d]",sm.smname,msec,ref,sm.current_state,new_stat,timeout))

    table.insert(sm.timeout_refs,ref)
  end

end

local function new_smachine(smname,cbs,ref)
  if not _sm_tab[smname] then
    _sm_tab[smname]={}
  end

  local sm = _sm_tab[smname]

  sm.state_handlers={}
  sm.smname=smname
  sm.timeout_refs={}
  sm.ref=ref

  for _,o in ipairs(cbs) do
    add_state(sm,o.state,o.func,o.execObj)
  end

  sm.add_state=function(...) return add_state(sm,...) end
  sm.remove_state=function(...) return remove_state(sm,...) end
  sm.get_current_state=function(...) return get_current_state(sm,...) end
  sm.switch_state_machine_to=function(...) return switch_state_machine_to(sm,...) end

  return sm
end

local function perform_unit_test()

  local count_for_idle_mode = 0

  local sm

  local stateMachineObj = {}

  function stateMachineObj:state_machine_idle(old_stat,new_stat,stat_chg_info)

    local actions={}

    actions[0]=function()
      print("[idle] switch to idle in 1 sec #1")
      sm.switch_state_machine_to("idle",1000)
    end

    actions[1]=function()
      print("[idle] switch to idle in 1 sec #2")
      sm.switch_state_machine_to("idle",1000)
    end

    actions[2]=function()
      print("[idle] switch to initiate in 3 sec")
      sm.switch_state_machine_to("initiate",3000)
    end

    print(string.format("[idle] old=%s,new=%s,chg=%s", old_stat,new_stat,stat_chg_info))

    if actions[count_for_idle_mode] then
      actions[count_for_idle_mode]()
    end

    count_for_idle_mode=count_for_idle_mode+1
  end

  function stateMachineObj:state_machine_initiate(old_stat,new_stat,stat_chg_info)
    print(string.format("[initiate] old=%s,new=%s,chg=%s", old_stat,new_stat,stat_chg_info))

    print("switch to idle in 1 sec")
    sm.switch_state_machine_to("idle",1000)
    -- adding same 1000 for 1 second here will make it unable to switch to "operate" state because the switch-state "idle" will be time-out first
    -- and switch_state_machine_to_delayed will delete all pending switching state
    print("cancel, switch to operate in 100 ms")
    sm.switch_state_machine_to("operate",100)
  end

  local count_for_operate_mode = 0

  function stateMachineObj:state_machine_operate(old_stat,new_stat,stat_chg_info)
    print(string.format("[operate] old=%s,new=%s,chg=%s,cnt=%d", old_stat,new_stat,stat_chg_info,count_for_operate_mode))

    count_for_operate_mode=count_for_operate_mode+1
    if count_for_operate_mode<10 then
      sm.switch_state_machine_to("operate",0)
    else
      sm.switch_state_machine_to("finalise",10)
    end
  end

  function stateMachineObj:state_machine_finalise(old_stat,new_stat,stat_chg_info)
    print(string.format("[finalise] old=%s,new=%s,chg=%s", old_stat,new_stat,stat_chg_info))

    il:close()
  end

  -- setup smachine
  sm=_m.new_smachine("main",{
    {state="idle",func="state_machine_idle",execObj=stateMachineObj},
    {state="initiate",func="state_machine_initiate",execObj=stateMachineObj},
    {state="operate",func="state_machine_operate",execObj=stateMachineObj},
    {state="finalise",func="state_machine_finalise",execObj=stateMachineObj},
  })

  sm.switch_state_machine_to("idle")

  print("* unit test starts")
  il:start()

  print("* unit test successfully finished")


  return true
end

-- initiate unit test
if not modname then
  l={log=function(logtype,...) print("smachine-unit-test [" .. logtype .. "] " .. ...) end}
end

-- create main object
_m={
  new_smachine=new_smachine,
  get_smachine=get_smachine,
}

-- perform unit test
if not modname then
  perform_unit_test()
  return
end

return _m
