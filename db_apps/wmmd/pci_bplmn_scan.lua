-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2018 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- AT&T BPLMN network scan module
-- http://pdgwiki/mediawiki/index.php?title=AT%26T_BPLMN_network_scan

local PciBplmnScan = require("wmmd.Class"):new()

function PciBplmnScan:setup(rdbWatch, wrdb)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("pci_bplmn_scan", "LOG_DAEMON") end)

  self.rdb = require("luardb")
  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.rdbWatch = rdbWatch
  self.ffi = require("ffi")
  self.config = require("wmmd.config")
  self.t = require("turbo")
  self.util = require("wmmd.util")

  self.m = self.luaq.m
  self.e = self.luaq.e
  self.sq = nil

  self.rdb_nrb200_attached = nil
  self.rdb_wmmd_mode = nil
  self.rdb_rrc_stat = nil

  -- delay that is used when BPLMN scan successes - 500ms
  self.bplmn_scan_succ_fin_delay = 500
  -- delay that is used to wait for QMI result - 2 seconds
  self.bplmn_scan_wait_delay = 2000
  -- delay that is used after BPLMN scan fails  - 5 seconds
  self.bplmn_scan_fail_fin_delay = 5000

  self.stateMachineHandlers = {
    {state="bplmn:ready", func="state_machine_bplmn_ready", execObj=self},
    {state="bplmn:scan", func="state_machine_bplmn_scan", execObj=self},
    {state="bplmn:final", func="state_machine_bplmn_final", execObj=self},
    {state="bplmn:sleep", func="state_machine_bplmn_sleep", execObj=self},
  }
end

--------------------------------------------------------------------------
-- Helpers
--------------------------------------------------------------------------
-- Returns true if pci scan is allowed to perform, false otherwise.
function PciBplmnScan:scanable()
  -- Do not perform pci scan if the wmmd mode is online.
  if self.rdb_wmmd_mode == "online" then
      return false
  end

  -- Do not perform pci scan if rrc state is not in idle or not inactive
  if not self.rdb_rrc_stat:find("idle", 1) and self.rdb_rrc_stat ~= "inactive" then
      return false
  end

  -- Perform pci scan if the wmmd mode is rf_qualification.
  if self.rdb_wmmd_mode == "rf_qualification" then
    return true
  else
    local hw_switch_rdb_name = string.format("hw.switch.port.%d.status",self.config.poe_ether_port)
    local hw_switch_rdb_val = self.wrdb:get(hw_switch_rdb_name)
    local poe_powered = hw_switch_rdb_val and (string.sub(hw_switch_rdb_val,1,1) == 'u')
    -- Perform pci scan when NRB-0200 is plugged, but LAN is not available.
    if not poe_powered and self.rdb_nrb200_attached == "1" then
      return true
    end
  end

  return false
end

--------------------------------------------------------------------------
-- RDB handlers
--------------------------------------------------------------------------
function PciBplmnScan:change_smachine_by_event(rdb_var,rdb_val)
  local state=self.sq.get_current_state()
  if state == "bplmn:ready" and self:scanable() then
    self.sq.switch_state_machine_to("bplmn:scan")
  else
    self.l.log("LOG_DEBUG",string.format("[pci-scan] keep smachine state=%s",state))
  end
end

function PciBplmnScan:rdb_on_wmmd_mode(rdb_var,rdb_val)
  self.l.log("LOG_DEBUG",string.format("[pci-scan] wmmd mode changed, ['%s' ==> '%s']",self.rdb_wmmd_mode,rdb_val))
  self.rdb_wmmd_mode = rdb_val
  self:change_smachine_by_event(rdb_var,rdb_val)
end

function PciBplmnScan:rdb_on_nrb200_attached(rdb_var,rdb_val)
  self.l.log("LOG_DEBUG",string.format("[pci-scan] nrb200 state changed, ['%s' ==> '%s']",self.rdb_nrb200_attached,rdb_val))
  self.rdb_nrb200_attached = rdb_val
  self:change_smachine_by_event(rdb_var,rdb_val)
end

function PciBplmnScan:rdb_on_rrc_stat(rdb_var,rdb_val)
  self.l.log("LOG_DEBUG",string.format("[pci-scan] rrc idle camped state changed, ['%s' ==> '%s']",self.rdb_rrc_stat,rdb_val))
  self.rdb_rrc_stat = rdb_val
  self:change_smachine_by_event(rdb_var,rdb_val)
end

--------------------------------------------------------------------------
-- state machine handlers
--------------------------------------------------------------------------
function PciBplmnScan:state_machine_bplmn_ready(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_DEBUG","[pci-scan] bplmn:ready state with no RDB notification")
  self:change_smachine_by_event()
end

function PciBplmnScan:state_machine_bplmn_scan(old_stat,new_stat,stat_chg_info)

  local succ = self.watcher.invoke("sys","pci_scan")

  if not succ then
    self.l.log("LOG_DEBUG","[pci-scan] failed to queue scan command, switch smachine [bplmn:scan ==> bplmn:sleep]")
    self.sq.switch_state_machine_to("bplmn:sleep")
    return
  end

  self.l.log("LOG_DEBUG",string.format("[pci-scan] schedule switch smachine [bplmn:scan ==> bplmn:sleep] with delay %d",self.bplmn_scan_wait_delay))
  self.sq.switch_state_machine_to("bplmn:sleep",self.bplmn_scan_wait_delay)
end

function PciBplmnScan:state_machine_bplmn_final(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_DEBUG",string.format("[pci-scan] schedule to switch smachine [bplmn:final ==> bplmn:ready] with succ delay %d",self.bplmn_scan_succ_fin_delay))
  self.sq.switch_state_machine_to("bplmn:ready",self.bplmn_scan_succ_fin_delay)
end

function PciBplmnScan:state_machine_bplmn_sleep(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_DEBUG",string.format("[pci-scan] switch smachine [bplmn:sleep ==> bplmn:ready] with fail delay %d",self.bplmn_scan_fail_fin_delay))
  self.sq.switch_state_machine_to("bplmn:ready",self.bplmn_scan_succ_fail_delay)
end

--------------------------------------------------------------------------
-- cbs_system
--------------------------------------------------------------------------
function PciBplmnScan:modem_on_bplmn_received(type,event,a)
    local state=self.sq.get_current_state()

    self.l.log("LOG_DEBUG",string.format("[pci-scan] bplmn received, state=%s",state))

    if state == "bplmn:scan" then
      self.l.log("LOG_DEBUG",string.format("[pci-scan] bplmn received and switch smachine [bplmn:scan ==> bplmn:final], state=%s",state))
      self.sq.switch_state_machine_to("bplmn:final")
    end

    return true
end

PciBplmnScan.cbs_system={
  "modem_on_bplmn_received",
}

function PciBplmnScan:init()

  -- 1. create state machine
  self.l.log("LOG_DEBUG","[pci-scan] initiate state machine")
  self.sq=self.smachine.new_smachine("pci_bplmn_scan", self.stateMachineHandlers)

  -- 2. add invoke handlers
  self.l.log("LOG_DEBUG","[pci-scan] initiate invoke handlers")
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- 3. add rdb handlers
  self.l.log("LOG_DEBUG","[pci-scan] initiate rdb handlers")
  self.rdbWatch:addObserver("service.wmmd.mode", "rdb_on_wmmd_mode", self)
  self.rdbWatch:addObserver("service.nrb200.attached", "rdb_on_nrb200_attached", self)
  self.rdbWatch:addObserver("wwan.0.radio_stack.rrc_stat.rrc_stat", "rdb_on_rrc_stat", self)

  -- initially, switch to bplm:ready
  self.l.log("LOG_DEBUG","[pci-scan] initially, switch smachine ['' ==> bplmn:ready]")
  self.sq.switch_state_machine_to("bplmn:ready")

  -- feed initial RDB variables into handlers
  self.l.log("LOG_DEBUG","[pci-scan] feed initial RDB variables")
  local rdb_init_val_wmmd_mode = self.wrdb:get("service.wmmd.mode")
  local rdb_init_val_nbr200_attached = self.wrdb:get("service.nrb200.attached")
  local rdb_init_val_rrc_stat = self.wrdb:get("wwan.0.radio_stack.rrc_stat.rrc_stat")
  self:rdb_on_wmmd_mode(nil,rdb_init_val_wmmd_mode)
  self:rdb_on_nrb200_attached(nil,rdb_init_val_nbr200_attached)
  self:rdb_on_rrc_stat(nil,rdb_init_val_rrc_stat)
end

return PciBplmnScan
