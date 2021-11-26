--[[

Decorators supporting Myna installation assistant UI

Copyright (C) 2019 NetComm Wireless limited.

--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")

-- decorating wmmd
local IaWmmdDecorator = WmmdDecorator:new()
-- decorating qmi_g
local IaQmiGDecorator = WmmdDecorator:new()
-- decorating pci_bplmn_scan
local IaPciBplmnScanDecorator = WmmdDecorator:new()

--[[
Supporting 2 scenarios:
Scenario 1: Main state machine flow of wmmd: idle --> initiate --> powerup --> readmodem
    where powerup state sets operating state online to modem then switches to readmodem state
Scenario 2: powerup state is held until some events (e.g. band is set) trigger to switch to
    readmodem state
--]]


-- RDB trigger handler for installation assistant state change
-- This function is also called when wmmd enters readmodem state to consider to
-- switch installation state and conduct additional actions
-- @param rdb_name RDB name for installation assistant state trigger
-- @param state_val Installation assistant state value
function IaQmiGDecorator:handle_installation_assistant_state(rdb_name, state_val)

  if state_val and #state_val > 0 then
    if self.check_holding_powerup_state_timer_ref then
      self.i:clear_interval(self.check_holding_powerup_state_timer_ref)
      self.check_holding_powerup_state_timer_ref = nil
    end

    local current_state = self.sq.get_current_state()
    if state_val == "data_entry" then
      self.l.log("LOG_NOTICE", string.format("IA UI orders state Data Entry"))
      if current_state == "qmis:install_pre_data_entry"
          or current_state == "qmis:install_data_entry" then
        self.l.log("LOG_NOTICE", string.format("Handle IA UI: Already in Data Entry state"))
        return
      end

      self.hold_powerup_data_entry = false

      -- if powerup is not held while readmodem has not been reached,
      -- the machine state will be switched during readmodem state handling
      if not self.passed_readmodem_state then
        if current_state == "qmis:powerup" then
          self.watcher.invoke("sys","poll_operating_mode")
          if not self.qg.online_operating_mode then
            self.l.log("LOG_NOTICE", string.format("IA UI orders state Data Entry while powerup is being held"))
            -- powerup state is held
            self.hold_powerup_data_entry = true
            -- erase previous feedback
            self.rdb.set("owa.installation.state", "")
            -- enter data_entry directly as preparation steps are unnecessary
            self.sq.switch_state_machine_to("qmis:install_data_entry")

            return
          end
        else
          self.l.log("LOG_NOTICE", string.format("Handle IA UI: Setup polling to check whether powerup is held"))
          self.check_holding_powerup_state_timer_ref = self.i:set_interval(1000, self.check_holding_powerup_state, self)
        end
        self.l.log("LOG_NOTICE", string.format("Handle IA UI: Waiting for readmodem before entering Data Entry state"))
        return
      end

      -- erase previous feedback
      self.rdb.set("owa.installation.state", "")

      self.sq.switch_state_machine_to("qmis:install_pre_data_entry")
    elseif state_val == "scan_connect" then
      self.l.log("LOG_NOTICE", string.format("IA UI orders state Scan Connect"))
      -- Proceed to scan_connect if it is in install_data_entry state
      -- Switch to install_post_data_entry state which will close install_data_entry properly and
      -- transit to install_network_scan_connect
      if current_state == "qmis:install_pre_data_entry" or current_state == "qmis:install_data_entry" then
        -- erase previous feedback
        self.rdb.set("owa.installation.state", "")
        self.sq.switch_state_machine_to("qmis:install_post_data_entry")
      else
        -- either no data_entry or transitting in readmodem invokes this function when powerup has been held
        if self.model == "myna" then
          -- just try to remove quick polling then set feedback
          self:remove_quick_polling()
        end
        self.rdb.set("owa.installation.state", "scan_connect")
      end
    elseif state_val == "normal_operation" then
      -- this state should be transitted from scan_connect
      -- restore quick modem polling if needed
      self:restore_quick_polling()
      self.rdb.set("owa.installation.state", "normal_operation")
      -- for myna lite/sparrow do modem attach so user can proceed to speed test
      if self.model == "myna_lite" or self.model == "sparrow" then self:attach_modem() end
    end
  end
end

-- Decorated init function
function IaQmiGDecorator:init()
  IaQmiGDecorator:__invokeChain("init")(self)
  self.model = self.rdb.get("installation.ui_model")
  self.rdbWatch:addObserver("installation.state", "handle_installation_assistant_state", self)
  self.rdbWatch:addObserver("installation.modem.command", "handle_installation_modem_command", self)
end

-- Decorated handle_network_state_transitions which invokes appropriate functions to handle
-- network state information in current machine state
-- @param network_state_info Network state information object
function IaQmiGDecorator:handle_network_state_transitions(network_state_info)
  IaQmiGDecorator:__invokeChain("handle_network_state_transitions")(self, network_state_info)

  local state = self.sq.get_current_state()
  if state == "qmis:install_pre_data_entry" then
    self:handle_network_state_change_in_install_pre_data_entry(network_state_info)
  elseif state == "qmis:install_network_scan_connect" then
    self:handle_network_state_change_in_install_network_scan_connect(network_state_info)
  end
end

-- Decorated handle_state_transition_in_readmodem_state which is invoked at the end of readmodem state handler
-- Invoke installation state change handler in case readmodem state comes after installation state change is triggered.
function IaQmiGDecorator:handle_state_transition_in_readmodem_state()
  self.passed_readmodem_state = true
  IaQmiGDecorator:__invokeChain("handle_state_transition_in_readmodem_state")(self)
  self:handle_installation_assistant_state(nil, self.wrdb:get("installation.state"))
end

-- Handle network state information in qmis:install_pre_data_entry machine state
-- @param network_state_info Network state information object
function IaQmiGDecorator:handle_network_state_change_in_install_pre_data_entry(network_state_info)
  if not network_state_info.ps_attach_state then
    self.l.log("LOG_NOTICE",string.format("In qmis:install_pre_data_entry: ps detached, switch to qmis:install_data_entry"))
    self.sq.switch_state_machine_to("qmis:install_data_entry")
  else
    self.l.log("LOG_ERR",string.format("In qmis:install_pre_data_entry: ps attached, retry to detach after timeout"))
  end
end

-- Handle network state information in qmis:install_network_scan_connect machine state
-- @param network_state_info Network state information object
function IaQmiGDecorator:handle_network_state_change_in_install_network_scan_connect(network_state_info)
  if network_state_info.ps_attach_state then
    self.l.log("LOG_NOTICE",string.format("In qmis:install_network_scan: ps attached, switch to connect"))
    self.sq.switch_state_machine_to("qmis:connect")
  else
    self.l.log("LOG_INFO",string.format("In qmis:install_network_scan: ps detached, retry to attach after timeout"))
  end
end

-- Handler of qmis:install_pre_data_entry machine state
-- @param old_stat Current machine state
-- @param new_stat New machine state
-- @param stat_chg_info State change info
function IaQmiGDecorator:state_machine_install_pre_data_entry(old_stat, new_stat, stat_chg_info)
  self.l.log("LOG_NOTICE",string.format("Handle state qmis:install_pre_data_entry"))
  self.sq.switch_state_machine_to("qmis:install_pre_data_entry", self.state_pre_data_entry_retry_interval_ms)

  if self.model == "myna_lite" or self.model == "sparrow" then
    self:restore_quick_polling()
  else
    local succ = self:detach_modem()
    if not succ then
      self.l.log("LOG_ERR",string.format("In state qmis:install_pre_data_entry: fail to detach modem"))
    end
  end

  -- query network state, handle of network status will do state change if necessary
  self.watcher.invoke("sys","poll_modem_network_status")
end

-- Handler of qmis:install_data_entry machine state
-- @param old_stat Current machine state
-- @param new_stat New machine state
-- @param stat_chg_info State change info
function IaQmiGDecorator:state_machine_install_data_entry(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_NOTICE",string.format("Handle state qmis:install_data_entry"))

  -- If powerup is being held, polling modem has not been started.
  -- Hence no need to stop polling modem
  if not self.hold_powerup_data_entry then
    -- stop regular polling which includes network scanning
    self:modem_poll_stop()
  end

  if not install_data_entry_poll_timer_ref then
    -- start a minimum polling just to kick watchdog
    self.install_data_entry_poll_timer_ref = self.i:set_interval(self.install_data_entry_poll_interval_ms, self.install_data_entry_poll_exec, self)
  end

  -- inform install tool that Data Entry state has been entered properly
  self.rdb.set("owa.installation.state", "data_entry")
end

-- Handler of qmis:install_post_data_entry machine state
-- @param old_stat Current machine state
-- @param new_stat New machine state
-- @param stat_chg_info State change info
function IaQmiGDecorator:state_machine_install_post_data_entry(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_NOTICE",string.format("Handle state qmis:install_post_data_entry"))
  if self.install_data_entry_poll_timer_ref then
    self.i:clear_interval(self.install_data_entry_poll_timer_ref)
    self.install_data_entry_poll_timer_ref = nil
  end

  if not self.hold_powerup_data_entry then
    -- continue regular polling
    self:modem_poll_start()
  end

  if self.hold_powerup_data_entry then
    -- set back to powerup state. When it transits to readmodem state, handling for scan_connect
    -- will be invoked
    self.sq.switch_state_machine_to("qmis:powerup")
  else
    self.sq.switch_state_machine_to("qmis:install_network_scan_connect")
  end
end

-- Handler of qmis:install_network_scan_connect machine state
-- Notice: normal_operation may be still remain in this state while trying to attach to network
-- @param old_stat Current machine state
-- @param new_stat New machine state
-- @param stat_chg_info State change info
function IaQmiGDecorator:state_machine_install_network_scan_connect(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_NOTICE",string.format("Handle state qmis:install_network_scan_connect"))
  self.sq.switch_state_machine_to("qmis:install_network_scan_connect", self.state_network_scan_connect_retry_interval_ms)

  local current_installation_state = self.wrdb:get("installation.state")
  if current_installation_state == "scan_connect" and self.model == "myna" then
    self:remove_quick_polling()
  end

  if not self.hold_powerup_data_entry then
    local b48str = "LTE Band 48 - TDD 3600"
    local isB48 = self.rdb.get("wwan.0.currentband.config") == b48str
    if not isB48 then
      -- Do attach network
      self.l.log("LOG_NOTICE", string.format("In state qmis:install_network_scan_connect - Perform manual attach"))
      local succ = self.watcher.invoke("sys","attach")
      if succ then
        self.l.log("LOG_NOTICE",string.format("In state qmis:install_network_scan_connect: manual attach requested, wait for attached"))
        -- attach invocation success just means request is sent out
        -- modem_on_network_state will switch state to connect upon attached
      else
        self.l.log("LOG_ERR",string.format("In state qmis:install_network_scan_connect: manual attach failed. retrying shortly"))
        self.watcher.invoke("sys","poll_modem_network_status")
      end
    end
  end

  if current_installation_state == "scan_connect" then
    -- to give feedback to install tool
    self.rdb.set("owa.installation.state", "scan_connect")
  end
end

-- Poll function in intall_data_entry state
function IaQmiGDecorator:install_data_entry_poll_exec()
  -- just to kick watchdog
  self.watcher.invoke("sys","poll_operating_mode")
end

-- Quick polling which sends QMI_NAS_GET_CELL_LOCATION_INFO_REQ_MSG actually
-- conflicts with PCI BPMN scanning.
-- This function removes quick polling
function IaQmiGDecorator:remove_quick_polling()
  if self.modem_poll_quick_ref then
    self.l.log("LOG_INFO","stop modem poll quick")
    self.i:clear_interval(self.modem_poll_quick_ref)
    self.modem_poll_quick_ref = nil
    self.installation_forced_stop_quick_polling = true
  end
end

-- This function restores quick polling
function IaQmiGDecorator:restore_quick_polling()
  if self.installation_forced_stop_quick_polling or self.passed_readmodem_state then
    if not self.modem_poll_quick_ref then
      self.l.log("LOG_NOTICE","restoring modem-poll-quick")
      self.modem_poll_quick_ref = self.i:set_interval(self.config.modem_poll_quick_interval, self.modem_poll_quick_exec, self)
    else
      self.l.log("LOG_NOTICE","modem-poll-quick has already been running")
    end
    self.installation_forced_stop_quick_polling = false
  end
end

-- If readmodem has not been reached, this function will be called periodically to check
-- whether powerup state is being held
function IaQmiGDecorator:check_holding_powerup_state()
  local current_state = self.sq.get_current_state()
  if current_state == "qmis:powerup" then
    self:handle_installation_assistant_state(nil, self.wrdb:get("installation.state"))
  end
end

-- RDB trigger handler for installation modem command
-- @param rdb_name RDB name triggering this call
-- @param rdb_val rdb value
function IaQmiGDecorator:handle_installation_modem_command(rdb_name, rdb_val)
  local current_state = self.rdb.get("installation.state")
  self.l.log("LOG_DEBUG",string.format("handle_installation_modem_command, rdb: %s, command: %s, state: %s.",
      rdb_name, rdb_val, current_state))

  if current_state ~= "scan_connect" then
    if rdb_val and #rdb_val>0 then
      self.rdb.set("installation.modem.command", "")
    end

    return
  end

  if "detach" == rdb_val then
    local succ = self:detach_modem()
    if not succ then
      self.l.log("LOG_NOTICE",string.format("Fail to detach modem in %s.", current_state))
    else
      self.l.log("LOG_ERR",string.format("Detached modem in %s.", current_state))
    end

    self.rdb.set("installation.modem.command", "")
  end
end

-- RDB trigger handler for installation assistant state change
-- @param rdb_name RDB name for installation assistant state trigger
-- @param state_val Installation assistant state value
function IaPciBplmnScanDecorator:handle_installation_assistant_state(rdb_name, state_val)
  self.installation_assistant_state = state_val
  self:change_smachine_by_event()
end

-- Decorated scannable
-- It is scannable if installation state is in scan_connect.
function IaPciBplmnScanDecorator:scanable()
  -- Do not perform pci scan if rrc state is not in idle or not inactive
  if not self.rdb_rrc_stat:find("idle", 1) and self.rdb_rrc_stat ~= "inactive" then
      return false
  end
  return self.installation_assistant_state == "scan_connect"
end

-- Decorated init
function IaPciBplmnScanDecorator:init()
  IaPciBplmnScanDecorator:__invokeChain("init")(self)
  self.rdbWatch:addObserver("installation.state", "handle_installation_assistant_state", self)
  self.installation_assistant_state = self.wrdb:get("installation.state")
end

-- Decorate state_machine_bplmn_scan to add call to get RF information of serving cell
function IaPciBplmnScanDecorator:state_machine_bplmn_scan(old_stat, new_stat, stat_chg_info)
  self.watcher.invoke("sys","poll_sig_info")
  IaPciBplmnScanDecorator:__invokeChain("state_machine_bplmn_scan")(self, old_stat, new_stat, stat_chg_info)
end

-- SETUP DECORATORS

-- Decorate pci_bplmn_scan
function IaPciBplmnScanDecorator.doDecorate()
  local pciBplmnScan = IaPciBplmnScanDecorator.__inputObj__
  IaPciBplmnScanDecorator:__saveChainTbl({
    "init",
    "state_machine_bplmn_scan"
  })
  IaPciBplmnScanDecorator:__changeImplTbl({
    "init",
    "scanable",
    "handle_installation_assistant_state",
    "state_machine_bplmn_scan"
  })
  pciBplmnScan.installation_assistant_state = nil
end

-- Decorate qmi_g
function IaQmiGDecorator.doDecorate()
  local qmiG = IaQmiGDecorator.__inputObj__
  local stateMachineHandlers = {
    {state="qmis:install_pre_data_entry", func="state_machine_install_pre_data_entry", execObj=qmiG},
    {state="qmis:install_data_entry", func="state_machine_install_data_entry", execObj=qmiG},
    {state="qmis:install_post_data_entry", func="state_machine_install_post_data_entry", execObj=qmiG},
    {state="qmis:install_network_scan_connect", func="state_machine_install_network_scan_connect", execObj=qmiG}
  }
  for _,stateHandler in pairs(stateMachineHandlers) do
    table.insert(qmiG.stateMachineHandlers, stateHandler)
  end

  -- interval in ms to retry actions in qmis:install_pre_data_entry state
  qmiG.state_pre_data_entry_retry_interval_ms = 5000
  -- interval in ms to retry actions in qmis:install_network_scan_connect
  qmiG.state_network_scan_connect_retry_interval_ms = 30000
  -- poll interval in ms in data entry state
  qmiG.install_data_entry_poll_interval_ms = 10000
  -- poll timer reference in data entry state
  qmiG.install_data_entry_poll_timer_ref = nil
  -- mark whether readmodem state has been passed
  qmiG.passed_readmodem_state = false
  -- mark whether powerup state is held during Data Entry
  qmiG.hold_powerup_data_entry = false

  qmiG.check_holding_powerup_state_timer_ref = nil

  qmiG.installation_forced_stop_quick_polling = false

  IaQmiGDecorator:__saveChainTbl({
    "init",
    "handle_network_state_transitions",
    "handle_state_transition_in_readmodem_state"
  })
  IaQmiGDecorator:__changeImplTbl({
    "init",
    "handle_network_state_transitions",
    "handle_state_transition_in_readmodem_state",
    "handle_network_state_change_in_install_pre_data_entry",
    "handle_network_state_change_in_install_network_scan_connect",
    "state_machine_install_pre_data_entry",
    "state_machine_install_data_entry",
    "state_machine_install_post_data_entry",
    "state_machine_install_network_scan_connect",
    "handle_installation_assistant_state",
    "handle_installation_modem_command",
    "install_data_entry_poll_exec",
    "remove_quick_polling",
    "restore_quick_polling",
    "check_holding_powerup_state"
  })
end

-- Decorate wmmd
function IaWmmdDecorator.doDecorate()
  IaQmiGDecorator:decorate(IaWmmdDecorator.__inputObj__.qmiG)
  IaPciBplmnScanDecorator:decorate(IaWmmdDecorator.__inputObj__.pciBplmnScan)
end

return IaWmmdDecorator
