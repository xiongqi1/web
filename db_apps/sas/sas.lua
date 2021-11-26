-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2019 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

local SasMain = require("wmmd.Class"):new()

function SasMain:setup()
  self.l = require("luasyslog")
  pcall(function() self.l.open("sas", "LOG_DAEMON") end)

  _G.TURBO_SSL=true
  _G.TURBO_STATIC_MAX=-1
  self.t = require("turbo")
  self.ioLoop = self.t.ioloop.instance()

  -- state machines
  self.smachine = require("wmmd.smachine")
  self.sm = nil
  self.sq = self.smachine.get_smachine("sas_smachine")
  self.stateMachineHandlers = {
    {state="mains:idle",func="state_machine_idle", execObj=self},
    {state="mains:initiate",func="state_machine_initiate", execObj=self},
    {state="mains:operate",func="state_machine_operate", execObj=self},
    {state="mains:finalise",func="state_machine_finalise", execObj=self},
  }
end

function SasMain:state_machine_idle(old_stat,new_stat,stat_chg_info)
  self.sm.switch_state_machine_to("mains:initiate")
end

function SasMain:state_machine_initiate(old_stat,new_stat,stat_chg_info)
  -- start main modules
  self:initiateModules()
end

function SasMain:state_machine_operate(old_stat,new_stat,stat_chg_info)
end

function SasMain:state_machine_finalise(old_stat,new_stat,stat_chg_info)
end

function SasMain:setupModules()
  self.params = {}
  -- all of these modules are subclass of Module class which includes init() method
  self.params.rdbWatch = require("wmmd.RdbWatch")
  self.params.l = self.l
  self.params.turbo = self.t
  self.params.rdb = require("luardb")
  self.params.util = require("sas.util")
  self.params.json = require('turbo.3rdparty.JSON')
  self.params.watcher = require("wmmd.watcher")

  -- SAS Client
  self.sasc = require("sas.sas_client")
  self.sasc:setup(self.params)
end

function SasMain:initiateModules()
  if self.sasc then self.sasc:init() end
end

function SasMain:initiate()
  -- create main state macine
  self.sm=self.smachine.new_smachine("main_smachine",self.stateMachineHandlers)

  -- start initial state machine
  self.sm.switch_state_machine_to("mains:idle")

  -- start turbo
  self.ioLoop:start()
end

return SasMain
