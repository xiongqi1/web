-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

local Wmmd = require("wmmd.Class"):new()

function Wmmd:setup()
  self.l = require("luasyslog")

  self.t = require("turbo")
  self.ioLoop = self.t.ioloop.instance()

  -- turbo reopens the syslog with different parameters
  -- open after turbo is loaded.
  self.l.open("wmmd", "LOG_DAEMON")
  -- turbo sets the logmask to LOG_INFO or higher
  -- reset to allow all levels
  local ffi = require("ffi")
  ffi.cdef[[
  int setlogmask(int mask);
  ]]
  ffi.C.setlogmask(255)

  -- state machines
  self.smachine = require("wmmd.smachine")
  self.sm = nil
  self.sq = self.smachine.get_smachine("qmi_smachine")
  self.stateMachineHandlers = {
    {state="mains:idle",func="state_machine_idle", execObj=self},
    {state="mains:initiate",func="state_machine_initiate", execObj=self},
    {state="mains:operate",func="state_machine_operate", execObj=self},
    {state="mains:finalise",func="state_machine_finalise", execObj=self},
  }
end

function Wmmd:state_machine_idle(old_stat,new_stat,stat_chg_info)
  self.sm.switch_state_machine_to("mains:initiate")
end

function Wmmd:state_machine_initiate(old_stat,new_stat,stat_chg_info)
  -- start main modules
  self:initiateModules()
end

function Wmmd:state_machine_operate(old_stat,new_stat,stat_chg_info)
end

function Wmmd:state_machine_finalise(old_stat,new_stat,stat_chg_info)
end

function Wmmd:setupModules()

  -- all of these modules are subclass of Module class which includes init() method
  self.rdbWatch = require("wmmd.RdbWatch")
  -- WMMD RDB interface - global RDB access point
  self.rdb = require("wmmd.wmmd_rdb"):new()
  -- Dynamic configuration - global dynamic setttings
  self.dConfig = require("wmmd.dconfig")

  -- [execution module]  link.profile state machines
  self.linkProfile = require("wmmd.linkprofile"):new()

#ifndef V_QMI_VOICE_none
  -- [execution module] voice call handler
  self.voiceCommand = require("wmmd.voicecommand"):new()
#endif

  -- [execution module] main state machine + miscellaneous QMI handlers
  self.qmiG = require("wmmd.qmi_g"):new()

#ifndef V_QMI_IMS_none
  -- [execution module] SMS handler
  self.sms = require("wmmd.sms"):new()
#endif

  -- [execution module] Radio Interface Layer performing user commands for radio
  self.ril = require("wmmd.ril"):new()

  -- [execution module] PCI bplmn scan
  self.pciBplmnScan = require("wmmd.pci_bplmn_scan"):new()

  self.rdb:setup(self.rdbWatch)
  self.dConfig:setup(self.rdbWatch, self.rdb)
  self.linkProfile:setup(self.rdbWatch, self.rdb)

#ifndef V_QMI_VOICE_none
  self.voiceCommand:setup(self.rdbWatch, self.rdb, self.dConfig)
#endif

  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)

#ifndef V_QMI_IMS_none
  self.sms:setup(self.rdb)
#endif

  self.ril:setup(self.rdbWatch, self.rdb)
  self.pciBplmnScan:setup(self.rdbWatch, self.rdb)
end

function Wmmd:initiateModules()
  if self.dConfig then self.dConfig:init() end
  if self.rdb then self.rdb:init() end
  if self.linkProfile then self.linkProfile:init() end
  if self.voiceCommand then self.voiceCommand:init() end
  if self.qmiG then self.qmiG:init() end
  if self.sms then self.sms:init() end
  if self.ril then self.ril:init() end
  if self.pciBplmnScan then self.pciBplmnScan:init() end
end

function Wmmd:initiate()
  -- create main state macine
  self.sm=self.smachine.new_smachine("main_smachine",self.stateMachineHandlers)

  -- start initial state machine
  self.sm.switch_state_machine_to("mains:idle")

  -- start turbo
  self.ioLoop:start()
end

return Wmmd
