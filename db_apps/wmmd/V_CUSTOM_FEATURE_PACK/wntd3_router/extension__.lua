--[[
  Decorator for WNTDV3

  Copyright (C) 2017 NetComm Wireless limited.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")
local CracknellDecorator = WmmdDecorator:new()
local QmiGDecorator = WmmdDecorator:new()
local LinkProfileDecorator = WmmdDecorator:new()

require("tableutil")

function QmiGDecorator:modem_on_operating_mode(type,event,a)
  QmiGDecorator:__invokeChain("modem_on_operating_mode")(self,type,event,a)

  local state=self.sq.get_current_state()

  if state == "qmis:attach" then
    if a.operating_mode == "online" then
      self.l.log("LOG_INFO", 'switching to attach on operating mode change to online')
      self.sq.switch_state_machine_to("qmis:attach")
    end
  elseif state == "qmis:prelock" then
    if a.operating_mode == "low power" then
      self.l.log("LOG_INFO", 'switch state to prelock on operating mode change to low power')
      self.sq.switch_state_machine_to("qmis:prelock")
    end
  end

  return true
end

-- Handle state transitions at connect state
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiGDecorator:handle_network_state_change_at_connect(a)
  if a.ps_attach_state then
    self.l.log("LOG_DEBUG",string.format("[linkprofile-ctrl] trigger sleeping link profiles"))
    self.watcher.invoke("sys","wakeup_linkprofiles")
  else
    if self.ncdp_in_progress then
      self.l.log("LOG_NOTICE",string.format("ps detached for ncdp"))
    elseif not a.reg_state then
      self.l.log("LOG_NOTICE",string.format("ps detached, switching to prelock"))
      -- Clear the serving physical cell id as it is not valid after detach.
      -- This can result in an false condition after attach if this does not get
      -- updated with the correct value in time.
      -- There is a slight theoretical chance the cell comes back up during the
      -- time the modem goes from low power -> online -> attach, attaches to the
      -- pci cell and the cell id is still blank, which would start recovery.
      -- After recovery timer expiry, we check if the current cell is in pci
      -- list and if so , we will not recover. If there is a handover during
      -- this time, we will perform recovery when we are not supposed to.
      self.rdb.set("wwan.0.radio_stack.e_utra_measurement_report.servphyscellid", "")
      self.sq.switch_state_machine_to("qmis:prelock")
    else
      self.l.log("LOG_NOTICE",string.format("ps detached but still registered, maintaining current state"))
    end
  end
end

-- Handle state transitions on network state change
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiGDecorator:handle_network_state_transitions(a)
  local state=self.sq.get_current_state()

  if state == "qmis:register" then
    self:handle_network_state_change_at_register(a)
  elseif state == "qmis:attach" then
    self:handle_network_state_change_at_attach(a)
  elseif state == "qmis:connect" or state == "qmis:operate" then
    self:handle_network_state_change_at_connect(a)
  end
end

-- parse the pci scan results for reporting
function QmiGDecorator:parse_pci_scan_results(pci_scan_result_unpacked)
  -- get RSRP from rdb (set by qdiag upon getting new values from log packets)
  -- the seq number is incremented on addition, so we can check if the records are new
  local seq = tonumber(self.wrdb:getp("manual_cell_meas.seq"))
  if self.last_seq == seq then
    self.l.log("LOG_WARNING", "ncdp: cell rsrp records not updated, discarding current rsrp samples")
    return false
  end
  self.last_seq = seq

  local num_samples = tonumber(self.wrdb:getp("manual_cell_meas.qty"))
  if not num_samples or num_samples == 0 then
    self.l.log("LOG_NOTICE", "ncdp: no rsrp samples present, trying again...")
    return false
  end
  self.l.log("LOG_NOTICE", string.format("ncdp: rsrp samples = %s, pci scan list(%s): '%s'",
    num_samples, self.ncdp_count, self.pci_scan_result))

  local cell_rsrp_table = {}
  for i=0,num_samples-1,1 do
    -- format: "E,42032,1,-102" (RAT[E=EUTRA,U=UMTS], EARFCN, PCI, RSRP)
    local manual_cell_meas = self.wrdb:getp(string.format("manual_cell_meas.%s", i))
    local manual_cell_meas_split = manual_cell_meas:split(",")
    local pci = manual_cell_meas_split[3]
    local earfcn = manual_cell_meas_split[2]
    cell_rsrp_table[pci.."."..earfcn] = tonumber(manual_cell_meas_split[4])
  end

  for _, cell in ipairs(pci_scan_result_unpacked) do
    local cell_rsrp = cell_rsrp_table[cell.pci.."."..cell.earfcn]
    if cell_rsrp then
      if not self.ncdp_results[cell.global_cell_id] then
        self.ncdp_results[cell.global_cell_id] = {}
        self.ncdp_results[cell.global_cell_id].pci = cell.pci
        self.ncdp_results[cell.global_cell_id].earfcn = cell.earfcn
        self.ncdp_results[cell.global_cell_id].global_cell_id = cell.global_cell_id
        self.ncdp_results[cell.global_cell_id].rsrp_min = cell_rsrp
        self.ncdp_results[cell.global_cell_id].rsrp_max = cell_rsrp
        self.ncdp_results[cell.global_cell_id].rsrp_total = cell_rsrp
        self.ncdp_results[cell.global_cell_id].sample_count = 1
      else
        if self.ncdp_results[cell.global_cell_id].rsrp_min > cell_rsrp then
          self.ncdp_results[cell.global_cell_id].rsrp_min = cell_rsrp
        end
        if self.ncdp_results[cell.global_cell_id].rsrp_max < cell_rsrp then
          self.ncdp_results[cell.global_cell_id].rsrp_max = cell_rsrp
        end
        self.ncdp_results[cell.global_cell_id].rsrp_total = self.ncdp_results[cell.global_cell_id].rsrp_total + cell_rsrp
        self.ncdp_results[cell.global_cell_id].sample_count = self.ncdp_results[cell.global_cell_id].sample_count + 1
      end
    else
      self.l.log("LOG_NOTICE", string.format("ncdp: no rsrp found for cell:earfcn = '%s:%s'",
        cell.pci, cell.earfcn))
    end
  end

  self.ncdp_count = self.ncdp_count + 1

  return true
end

-- save the pci scan list after network scan
-- type  : callback type
-- event : event name
-- a     : argument for the callback
function QmiGDecorator:modem_on_pci_list(type,event,a)
  self.pci_scan_result = self.util.pack_lockmode(a)

  local state = self.sq.get_current_state()
  if state == "qmis:ncdp" then
    if self.ncdp_in_progress then
      self:parse_pci_scan_results(a)
      self.scan_end_time = os.date("%d/%m/%Y %X")
    end
    self.pci_scan_result = nil
  end

  return true
end

function QmiGDecorator:handle_state_transition_in_readmodem_state()
  self.sq.switch_state_machine_to("qmis:prelock")
end

function QmiGDecorator:state_machine_prelock(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:prelock",self.wmmd_prelock_retry_interval)

  local pci_earfcn = self.wrdb:getp("pci_earfcn")

  self.l.log("LOG_NOTICE", string.format(
    "prelock: attached = '%s', self.pci = '%s', pci_earfcn = '%s'",
    self.qg.ps_attached, self.pci, pci_earfcn))

  if self.pci ~= "" and pci_earfcn == "" then
    -- at first boot, issue a detach to avoid any attach in progress.
    -- On bootup the modem may automatically attach but the ps_attach variable
    -- would still be false. This results in a state mismatch and means the
    -- perform_pci_scan call below will incorrectly skip the detach request
    -- that it would normally make. So detach here and exit immediately - the
    -- state machine will timeout and re-enter this state at which time the
    -- detach would have complete.
    if self.wmmd_start == nil then
      self.wmmd_start = true
      self:detach_modem()
      return
    end

    if not self.pci_scan_result or self.pci_scan_result == "" then
      self.invalid_pci_scan_count = self.invalid_pci_scan_count + 1
      if self.reboot_for_recovery and self.invalid_pci_scan_count > self.max_recovery_pci_scan_count then
        self.sq.switch_state_machine_to("qmis:sysreboot")
      else
        self:perform_pci_scan()
      end
      return
    end
    self.l.log("LOG_NOTICE", string.format("prelock: pci scan list: '%s'", self.pci_scan_result))

    -- save all earfcns from pci_scan_result
    local earfcn_list = self.util.merge_unique_lists({
                              self.wrdb:get('pcil_earfcn_hist') or '',
                              self.util.get_earfcn_from_pci_earfcn_list(self.pci_scan_result)
                            })
    self.wrdb:setp("pcil_earfcn_hist", earfcn_list)

    -- get/match earfcn for configured pci list
    -- if no earfcn matching pci, we will attach without pci locking
    local matchedList = self.util.match_pci_in_frequency_scan_list(
      self.pci, self.pci_scan_result, self.max_pci_pair_entry_count)

    -- clear the pci scan results, we will use fresh ones next time
    self.pci_scan_result = nil
    self.frequency_scan_try_count = self.frequency_scan_try_count + 1

    if not matchedList then
      if self.frequency_scan_try_count < self.max_frequency_scan_try_count then
        self.l.log("LOG_NOTICE", "prelock: no earfcn corresponds to pci, rescanning.")
        self:perform_pci_scan()
        return
      end
    end
    self.frequency_scan_try_count = 0

    -- if still nil, do unlock try
    if matchedList == nil then
      self.l.log("LOG_NOTICE", string.format('prelock: pci not matched in freq scan list, going for unlock try'))
      self.pci = ""
    else
      self.l.log("LOG_NOTICE", "prelock: pci matched in freq scan list, going for lock try")
      pci_earfcn = self.util.pack_lockmode(matchedList)
      local pcil_earfcn = self.util.get_earfcn_from_pci_earfcn_list(pci_earfcn)
      self.wrdb:setp("pcil_earfcn", pcil_earfcn)
      self.wrdb:setp("pci_earfcn", pci_earfcn)
    end
  end

  -- we will try 3 times to set lock config, then continue with the existing config
  -- if lock not configured, we will continue below
  if self.lock_config ~= pci_earfcn or pci_earfcn == "" then
    local omode = self.wrdb:getp("operating_mode")

    if omode ~= "low power" then
      self.l.log("LOG_NOTICE", "prelock: putting modem into low power")
      self.watcher.invoke("sys","lowpower")
      return
    end

    if not self:set_lock_in_modem(pci_earfcn) then
      self.l.log("LOG_NOTICE", string.format(
        "prelock: setting lock in modem failed, lock='%s'", pci_earfcn))
      return
    end
  end

  if self.reboot_for_recovery then
    self.sq.switch_state_machine_to("qmis:sysreboot")
  else
    self.l.log("LOG_INFO", string.format("prelock: procedure complete, switching to attach"))
    self.sq.switch_state_machine_to("qmis:attach")
  end
end

-- set the given pci:earfcn lock in the modem
-- lock_to_config : the lock in the form "pci:X1,earfcn:Y1;..."
function QmiGDecorator:set_lock_in_modem(lock_to_config)
  if lock_to_config == nil then
    lock_to_config = ""
  end
  local locksets_to_config = self.util.parse_lockmode(lock_to_config)

  if self.lock_try_count < self.max_lock_try_count then
    self.lock_try_count = self.lock_try_count + 1
    self.l.log("LOG_NOTICE", string.format("set_lock_in_modem: setting cell lock '%s', try %s",
      lock_to_config,self.lock_try_count))

    local succ = self.watcher.invoke("sys","cell_lock", locksets_to_config)
    if not succ then
      return succ
    end

    -- record the lock config in RDB since there is no way to read lock config from modem
    self.lock_config = self.util.pack_lockmode(locksets_to_config)
    self.wrdb:setp("lock_config", self.lock_config)
  else
    self.l.log("LOG_NOTICE", "set_lock_in_modem: max retries reached, cell lock failed in modem")
    if self.lock_config == "" then
      self.l.log("LOG_NOTICE",
        "set_lock_in_modem: no lock currently configured in modem, proceeding with unlocktry")
    else
      self.l.log("LOG_NOTICE", string.format(
        "set_lock_in_modem: currently configured lock in modem: '%s', proceeding with locktry",
        self.lock_config))
    end
  end

  self.lock_try_count = 0
  return true
end

function QmiGDecorator:state_machine_attach(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:attach",self.wmmd_attach_retry_interval)

  local omode = self.wrdb:getp("operating_mode")
  if not self.qg.online_operating_mode then
    self.l.log("LOG_NOTICE", "attach: bringing modem online")
    self.watcher.invoke("sys","online")
    return false
  end

  self.l.log("LOG_INFO", "performing manual attach")

  if self.lock_config ~= "" then
    if self.attach_try_count < self.max_lock_try_attach_count then
      self.attach_try_count = self.attach_try_count + 1
    else
      self.attach_try_count = 0
      -- we tried attach for 3 times with lock, now trying without lock
      self.l.log("LOG_NOTICE", 'attach: attach with lock try failed, now doing unlock try')
      -- clear all PCI/EARFCN list and associated rdb variables
      self.wrdb:setp("pci_earfcn", "")
      self.wrdb:setp("pcil_earfcn", "")
      self.pci = ""
      self.sq.switch_state_machine_to("qmis:prelock")
      return true
    end
  end

  local succ = self.watcher.invoke("sys","attach")
  if not succ then
    self.l.log("LOG_ERR", "manual attach failed. retrying shortly")
    succ = self.watcher.invoke("sys","poll_modem_network_status")
  else
    self.l.log("LOG_NOTICE", "attach: manual attach requested, waiting for attach")
    -- attach invocation success just means request is sent out
    -- modem_on_network_state will switch state to connect upon attached
  end

  return succ
end

function QmiGDecorator:state_machine_connect(old_stat,new_stat,stat_chg_info)
  local pci = self.wrdb:getp("pci")
  local currCellId = self:get_current_pci()
  local pci_valid = self.util.find_value_in_list(currCellId, pci)
  self.attach_try_count = 0

  if pci_valid or pci == "" or self.lock_config ~= "" then
    self:stop_cell_recovery_timer()
    self.wrdb:setp("state", "3GPP")
  else
    self:start_cell_recovery_timer()
    self.wrdb:setp("state", "UNLOCK")
  end

  QmiGDecorator:__invokeChain("state_machine_connect")(self,old_stat,new_stat,stat_chg_info)
end

function QmiGDecorator:state_machine_reboot(old_stat,new_stat,stat_chg_info)
  local pci = self.wrdb:getp("pci") or ""
  local pci_earfcn = self.wrdb:getp("pci_earfcn")
  local pcil_earfcn = self.wrdb:getp("pcil_earfcn")
  local currCellId = self:get_current_pci()

  self.l.log("LOG_NOTICE", string.format(
    "sysreboot: attached='%s', current cell='%s', pci list='%s', pcil_earfcn='%s', self.pci='%s', pci_earfcn='%s'",
    self.qg.ps_attached, currCellId, pci, pcil_earfcn, self.pci, pci_earfcn))

  --something may be wrong, schedule a reboot after 30 sec
  self.l.log("LOG_NOTICE", "sysreboot: rebooting device")
  self.wrdb:set('service.system.reset', '1')

  if self.ncdp_in_progress then
    self.wrdb:setp("celldetectrequested", "0")
  end
end

-- Handles the checks required on pci change
-- rdbKey: the key changed, always "wwan.0.pci"
-- rdbVal: the new value
function QmiGDecorator:pciChanged(rdbKey, rdbVal)
  local state = self.sq.get_current_state()
  local pci_earfcn = self.wrdb:getp("pci_earfcn")
  local currCellId = self:get_current_pci()
  self.pci = rdbVal

  self.l.log("LOG_NOTICE", string.format(
    "pciChanged: current cell='%s', pci(s)='%s', attached=%s, state=%s, pci_earfcn='%s', previous_pci='%s'",
    currCellId, self.pci, self.qg.ps_attached, state, pci_earfcn, self.previous_pci))

  -- validate if pci_earfcn has valid pci(s) from new pci list, else clear it
  local pci_earfcn_list_valid = self.util.validate_pci_earfcn_pairs(self.pci, pci_earfcn)
  if pci_earfcn_list_valid then
    -- keep only valid entries from current pci:earfcn list
    newPciEarfcnList = self.util.match_pci_in_frequency_scan_list(
      self.pci, pci_earfcn, self.max_pci_pair_entry_count)
    pci_earfcn = self.util.pack_lockmode(newPciEarfcnList)
    self.wrdb:setp("pci_earfcn", pci_earfcn)
    local pcil_earfcn = self.util.get_earfcn_from_pci_earfcn_list(pci_earfcn)
    self.wrdb:setp("pcil_earfcn", pcil_earfcn)
  else
    self.l.log("LOG_NOTICE", string.format(
      "pciChanged: pci_earfcn not valid, cleared, will be re-populated upon reboot"))
    self.wrdb:setp("pci_earfcn", "")
    self.wrdb:setp("pcil_earfcn", "")
  end

  -- check if pci lists are same, ignoring pci priority.
  -- changes in pci priority are handled above in validation of pcis(s) in new pci list
  local is_pci_list_same = self.util.compare_pci_lists(self.pci, self.previous_pci)
  self.previous_pci = self.pci

  -- if pci was cleared, or is valid, we do not need to re-connect or disconnect, just clear/set pci
  local pci_valid = self.util.find_value_in_list(currCellId, self.pci)
  if self.pci == "" or pci_valid then
    self:stop_cell_recovery_timer()
    self.wrdb:setp("state", "3GPP")

    if self.pci == "" then
      self.l.log("LOG_NOTICE", "pciChanged: pci list cleared by tr069, stopped cell recovery")
    else
      self.l.log("LOG_NOTICE", string.format(
        "pciChanged: current cell '%s' in pci list, keeping current connection", currCellId))
    end
    return
  end

  -- if new pci list is the same as the previous one, do nothing
  -- we prioritize connection over immediate list change
  -- check this last, as prioritization, pci clearing and validation must be done first
  -- and we need to start recovery in some cases.
  if is_pci_list_same then
    self.l.log("LOG_NOTICE", "pciChanged: new pci list same as last, no action required")
    return
  end

  if not pci_valid then
    self.l.log("LOG_NOTICE",
      "pciChanged: current pci not in pci list, starting cell recovery timer")
    self:start_cell_recovery_timer()
  end
end

-- start the cell recovery timer
function QmiGDecorator:start_cell_recovery_timer()
  if self.cell_recovery_timer_ref ~= nil then
    self.l.log("LOG_NOTICE", string.format("start_cell_recovery_timer: timer '%s' already running",
      self.cell_recovery_timer_ref))
    return
  end

  local pci = self.wrdb:getp("pci")
  if pci ~= "" then
    local timeout = self.t.util.gettimemonotonic() + (self.cell_recovery_time_seconds * 1000)
    self.cell_recovery_timer_ref = self.i:add_timeout(timeout,
      self.handle_cell_recovery_timer_timeout, self)
    self.l.log("LOG_NOTICE", "start_cell_recovery_timer: started")
  else
    self.l.log("LOG_NOTICE", "start_cell_recovery_timer: not enabled")
  end
end

-- stop the cell recovery timer
function QmiGDecorator:stop_cell_recovery_timer()
  if self.cell_recovery_timer_ref == nil then
    return
  end
  self.i:remove_timeout(self.cell_recovery_timer_ref)
  self.l.log("LOG_NOTICE", string.format("stop_cell_recovery_timer: stopped timer '%s'",
    self.cell_recovery_timer_ref))
  self.cell_recovery_timer_ref = nil
end

-- handles the timeout of the cell recovery timer
function QmiGDecorator:handle_cell_recovery_timer_timeout()
  if self.cell_recovery_timer_ref ~= nil then
    self:stop_cell_recovery_timer()
  end

  -- if timer is disabled, do not reboot (disabled when pci is cleared through tr069)
  local pci = self.wrdb:getp("pci")
  if pci == "" then
    self.l.log("LOG_NOTICE",
      "handle_cell_recovery_timer_timeout: not rebooting on cell_recovery_timer timeout, disabled by tr069 server")
    return
  end

  local currCellId = self:get_current_pci()
  local pci_valid = self.util.find_value_in_list(currCellId, pci)
  if pci_valid then
    self.l.log("LOG_NOTICE",
      "handle_cell_recovery_timer_timeout: not rebooting, current cell_found in pci list")
    return
  end

  self.l.log("LOG_NOTICE",
    "handle_cell_recovery_timer_timeout: cell_recovery_timer expired, rebooting")
  self.wrdb:set('service.system.reset_reason', 'WMMD: cell_recovery_timer expired')

  -- reset the pci scan count for recovery
  self.invalid_pci_scan_count = 0

  -- since we are going to reboot, we can do a pci scan as pci is configured and pci:earfcn list
  -- is empty.
  -- The default Qualcomm behaviour is to attempt to attach to any cell on reboot. This is
  -- undesired behaviour for us as we want that to be under full wmmd control for the purposes of
  -- the required PCI locking behaviour. To mitigate this problem, we scan and populate the list
  -- before reboot so that the auto-attach after reboot will have the correct modem pci config.
  self.reboot_for_recovery = true
  self.sq.switch_state_machine_to("qmis:prelock")
end

-- handles the update of the cell recovery timer value
-- rdbKey: the key changed, always "wwan.0.recovery_timer"
-- rdbVal: the new timer value
function QmiGDecorator:cell_recovery_time_update(rdbKey, rdbVal)
  local cell_recovery_time_seconds = tonumber(rdbVal)
  if cell_recovery_time_seconds == nil then
    self.l.log("LOG_WARNING", string.format(
      "cell_recovery_time_update: invalid cell recovery time given, ignoring value: '%s'",
      rdbVal))
    self.wrdb:setp("recovery_timer", self.cell_recovery_time_seconds)
    return
  end
  self.cell_recovery_time_seconds = cell_recovery_time_seconds
  self.l.log("LOG_NOTICE", string.format(
    "cell_recovery_time_update: cell recovery time set to %s seconds", self.cell_recovery_time_seconds))
end

-- reschedule ncdp on first boot/reboot when we do not have correct time
-- this is triggered after a network time update, also caters for any changes to the time
function QmiGDecorator:reschedule_ncdp()
  self:stop_ncdp_reschedule_timer()

  -- reschedule ncdp
  local ncdp_time = tonumber(self.wrdb:getp("celldetectrequested"))
  if ncdp_time and ncdp_time ~= 0 then
    self.l.log("LOG_NOTICE", "reschedule_ncdp: rescheduling ncdp upon time change")
    self:ncdp_scheduler(nil, ncdp_time)
  end
end

-- stop the ncdp rescheduling timer
function QmiGDecorator:stop_ncdp_reschedule_timer()
  if self.ncdp_reschedule_timer_ref == nil then
    return
  end
  self.l.log("LOG_NOTICE", string.format(
    "stop_ncdp_reschedule_timer: removing existing ncdp reschedule timer: %s",
    self.ncdp_reschedule_timer_ref))
  self.i:remove_timeout(self.ncdp_reschedule_timer_ref)
  self.ncdp_reschedule_timer_ref = nil
end

-- stop the ncdp timer
function QmiGDecorator:stop_ncdp_timer()
  if self.ncdp_timer_ref == nil then
    return
  end
  self.l.log("LOG_NOTICE", string.format(
    "stop_ncdp_timer: removing existing ncdp timer: %s",
    self.ncdp_timer_ref))
  self.i:remove_timeout(self.ncdp_timer_ref)
  self.ncdp_timer_ref = nil
end

-- schedule ncdp upon value change in rdb
-- rdbKey: the key changed, always "wwan.0.celldetectrequested"
-- rdbVal: the new scheduling value
function QmiGDecorator:ncdp_scheduler(rdbKey, rdbVal)
  self:stop_ncdp_timer()

  -- cell scan start offset in minutes
  local scan_start_offset = tonumber(rdbVal)

  if scan_start_offset == 0 then
    self.l.log("LOG_NOTICE", "ncdp_scheduler: ncdp scheduling stopped")
    self.ncdp_in_progress = false
    return false
  elseif self.ncdp_in_progress then
    -- technically the value can not change while a scan is active
    self.l.log("LOG_NOTICE", "ncdp_scheduler: ncdp in progress, not scheduling")
    return false
  elseif self.cell_recovery_timer_ref ~= nil then
    -- cell recovery timer active means we are not locked to a cell
    self.l.log("LOG_NOTICE", "ncdp_scheduler: cell lock not active")
  elseif not scan_start_offset then
    self.l.log("LOG_NOTICE", "ncdp_scheduler: invalid input for ncdp scheduling")
    self.wrdb:setp("celldetectrequested", "0")
    return false
  elseif scan_start_offset < 0 then
    self.l.log("LOG_NOTICE", "ncdp_scheduler: negative input for ncdp scheduling")
    self.wrdb:setp("celldetectrequested", "0")
    return false
  elseif scan_start_offset > 1379 then
    self.l.log("LOG_ERR", "ncdp_scheduler: requested time interval more than 23 hours")
    self.wrdb:setp("celldetectrequested", "0")
    return false
  elseif self.rdb.get("system.time.updated") == "" then
    self.l.log("LOG_NOTICE", "ncdp_scheduler: time not updated on device")
    self.wrdb:setp("celldetectrequested", "0")
    return false
  end

  -- Random number generation
  math.randomseed(self.t.util.gettimemonotonic())

  -- time difference in seconds to start of ncdp
  -- device time is always in utc
  local secs_in_day = 24*60*60
  local current_utc_secs = (tonumber(os.time())) % secs_in_day
  local current_aest_secs = (current_utc_secs + (10*60*60)) % secs_in_day
  local targettime_secs = (scan_start_offset * 60) + math.random(10,3600)

  local diff_secs = 0
  if current_aest_secs > targettime_secs then
    diff_secs = secs_in_day - current_aest_secs + targettime_secs
  else
    diff_secs = targettime_secs - current_aest_secs
  end

  -- total time difference in milliseconds to start of ncdp including randomization
  local timeout = self.t.util.gettimemonotonic() + (diff_secs * 1000)
  self.ncdp_timer_ref = self.i:add_timeout(timeout,
    self.handle_ncdp_scheduler_timer_timeout, self)
  self.l.log("LOG_NOTICE", string.format("ncdp timer '%s' started, timeout: %.1f minutes",
    self.ncdp_timer_ref, diff_secs/60))

  return true
end

-- handle ncdp timer timeout, switch to the scan state if required
function QmiGDecorator:handle_ncdp_scheduler_timer_timeout()
  self:stop_ncdp_timer()

  local pci = self.wrdb:getp("pci")
  local ncdp_time = tonumber(self.wrdb:getp("celldetectrequested"))
  if not ncdp_time or ncdp_time == 0 then
    -- if scan cancelled at the very last moment
    self.l.log("LOG_NOTICE", ("handle_ncdp_scheduler_timer_timeout: ncdp cancelled by tr069"))
    return
  elseif self.cell_recovery_timer_ref ~= nil or pci == "" then
    -- cell recovery timer active means we are not locked to a cell
    -- pci empty means wntdv3 is not in locked mode.
    self.l.log("LOG_NOTICE", "handle_ncdp_scheduler_timer_timeout: cell lock not active, cancelling ncdp")
    self.wrdb:setp("celldetectrequested", "0")
    return
  end

  self.l.log("LOG_NOTICE", "handle_ncdp_scheduler_timer_timeout: starting ncdp")
  local state=self.sq.get_current_state()
  if state == "qmis:operate" then
    self.sq.switch_state_machine_to("qmis:ncdp")
  else
    self.l.log("LOG_NOTICE", "handle_ncdp_scheduler_timer_timeout: modem not operational, not scanning")
  end
end

-- Perform a pci scan.
-- It detaches if not already detached.
-- Return: true if scan successful, false otherwise.
function QmiGDecorator:perform_pci_scan()
  -- we can not scan while attached
  if self.qg.ps_attached then
    self:detach_modem()
    return false
  end

  if self.pci_scan_api_fail_count < self.max_pci_scan_api_fail_count then
    local succ = self.watcher.invoke("sys","pci_scan", nil)
    if not succ then
      self.l.log("LOG_WARNING", "perform_pci_scan: invoke returned failure, retrying after timeout")
      self.pci_scan_api_fail_count = self.pci_scan_api_fail_count + 1
      return false
    end
  else
    -- too much api call failures, modem is not behaving correctly or some other problem
    self.l.log("LOG_WARNING", "pci_scan: too many api call failures")
    self.wrdb:set('service.system.reset_reason', 'WMMD: too many pci scan api call failures')

    self.sq.switch_state_machine_to("qmis:sysreboot")
    return false
  end
  self.pci_scan_api_fail_count = 0

  self.l.log("LOG_INFO", "pci_scan: scan successful")
  return true
end

-- Consolidate all ncdp results
function QmiGDecorator:collate_ncdp_results()
  if next(self.ncdp_results) ~= nil then
    local result = self.scan_end_time
    for _, cell in pairs(self.ncdp_results) do
      local rsrp_avg = tonumber(string.format("%.2f", cell.rsrp_total/cell.sample_count))
      result = string.format("%s,%s,%s,%s,%s,%s,%s", result,
        cell.pci, cell.earfcn, cell.global_cell_id,
        cell.rsrp_min, cell.rsrp_max, rsrp_avg)
    end
    self.l.log("LOG_NOTICE", string.format("ncdp: result: '%s'", result))

    self.wrdb:setp("celldata", result)
    os.execute('rdb get wwan.0.celldata > /var/NeighbourCellData.csv')
  else
    self.l.log("LOG_NOTICE", "ncdp: no cells found in network scan")
  end

  self.ncdp_count = 1
  self.last_seq = -1
  self.ncdp_results = {}
  self.wrdb:setp("celldetectrequested", "0")

  return
end

-- state to handle ncdp. It scans for network/cells to gather the information
function QmiGDecorator:state_machine_ncdp(old_stat,new_stat,stat_chg_info)
  -- Fix for the following race condition:
  -- Pre Condition: if pci scan takes more than 'wmmd_ncdp_retry_interval' seconds.
  -- Upon last iteration, state_machine_ncdp will be invoked and pending execution
  -- when this call invocation exits.
  -- Other iterations do not matter as we are going to do a scan anyway and bail if
  -- elapsed time is above the allowed time for scanning.
  -- This will cause switching to prelock to never occur.
  -- This situation may arise anytime, as we can not guarantee pci scan time
  -- Upon completion, 'celldetectrequested' is set to 0, so we know we can now exit.
  local ncdp_time = tonumber(self.wrdb:getp("celldetectrequested"))
  if not ncdp_time or ncdp_time == 0 then
    self.l.log("LOG_NOTICE", "ncdp: race condition detected, switching to prelock")
    self.sq.switch_state_machine_to("qmis:prelock")
    return
  end

  self.sq.switch_state_machine_to("qmis:ncdp", self.wmmd_ncdp_retry_interval)
  self.ncdp_in_progress = true

  if self.ncdp_start_time == nil then
    self.ncdp_start_time = self.t.util.gettimemonotonic()
  end

  if self.last_seq == -1 then
    self.last_seq = tonumber(self.wrdb:getp("manual_cell_meas.seq"))
  end

  local elapsed_time = self.t.util.gettimemonotonic() - self.ncdp_start_time
  if elapsed_time < self.ncdp_max_scan_time_msecs and self.ncdp_count <= self.ncdp_max_scans then
    self:perform_pci_scan()
    return
  end

  -- we reach here after ncdp is complete, either required number of scans done
  -- or time greater than max allowed scan time
  self.ncdp_in_progress = false
  self:collate_ncdp_results()
  self.ncdp_start_time = nil

  self.sq.switch_state_machine_to("qmis:prelock", self.wmmd_ncdp_to_prelock_switch_interval)

  return
end

-- handle timers/variables or any events that depend on time
function QmiGDecorator:handle_time_update()
  -- reschedule ncdp on time update
  if self.ncdp_reschedule_timer_ref ~= nil then
    self:stop_ncdp_reschedule_timer()
  end

  local time_valid = self.rdb.get("system.time.updated")
  if time_valid ~= "" then
    self.l.log("LOG_NOTICE", "handle_time_update: time is valid, rescheduling ncdp")
    local timeout = self.t.util.gettimemonotonic() + self.wmmd_reschedule_ncdp_timer
    self.ncdp_reschedule_timer_ref = self.i:add_timeout(timeout, self.reschedule_ncdp, self)
  end
end

-- pci lock initializations
function QmiGDecorator:init_pci_lock()
  -- constant variables
  -- The prelock retry interval and ncdp retry interval should be greater than
  -- the modem pci scan timeout to avoid recalling the scan api while a scan is in progress
  self.wmmd_prelock_retry_interval=7500
  self.wmmd_ncdp_retry_interval=7500
  self.wmmd_ncdp_to_prelock_switch_interval=1000
  self.wmmd_reschedule_ncdp_timer=10000

  -- set a lower timeout for cracknell
  -- wmmd_prelock_retry_interval and wmmd_ncdp_retry_interval should be more than this value
  -- in observed worst case (detached) pci scan takes ~5 seconds, normally it is less than 1 seconds
  self.config.modem_pci_scan_timeout_msec = self.wmmd_ncdp_retry_interval - 500

  -- add state machine functions
  local temp_table = {
    {state="qmis:ncdp",func="state_machine_ncdp", execObj=self},
    {state="qmis:sysreboot", func="state_machine_reboot", execObj=self},
  }
  for k,v in pairs(temp_table) do table.insert(self.stateMachineHandlers, v) end

  -- add pci list update callback
  table.insert(self.cbs_system, "modem_on_pci_list")

  -- lock/unlock variables for cracknell
  self.pci = self.wrdb:getp("pci") or ""
  self.previous_pci = self.pci
  self.lock_config = self.wrdb:getp("lock_config")
  self.ncdp_count = 1
  self.ncdp_results = {}
  self.ncdp_timer_ref = nil
  self.ncdp_reschedule_timer_ref = nil
  self.ncdp_in_progress = false
  self.ncdp_max_scans = 10
  -- scan limit is 180 seconds, allow 5 secs for some slack time
  self.ncdp_max_scan_time_msecs = 175000
  self.last_seq = -1
  self.cell_recovery_timer_ref = nil
  self.cell_recovery_time_seconds = tonumber(self.wrdb:getp("recovery_timer"))
  self.reboot_for_recovery = false
  self.lock_try_count = 0
  self.attach_try_count = 0
  self.pci_scan_api_fail_count = 0
  self.max_pci_scan_api_fail_count = 10
  -- max 3 lock tries
  self.max_lock_try_attach_count = 3
  self.frequency_scan_try_count = 0
  self.max_pci_pair_entry_count = 10
  self.max_lock_try_count = tonumber(self.wrdb:getp("max_lock_try_count"))
  self.max_frequency_scan_try_count = tonumber(self.wrdb:getp("max_frequency_scan_try_count"))
  self.invalid_pci_scan_count = 0
  self.max_recovery_pci_scan_count = 3

  if not self.cell_recovery_time_seconds then
    -- set default of 10 minutes
    self.cell_recovery_time_seconds = 600
    self.wrdb:setp("recovery_timer", self.cell_recovery_time_seconds)
  end

  if not self.max_lock_try_count or self.max_lock_try_count == nil then
    self.max_lock_try_count = 3
    self.wrdb:setp("max_lock_try_count", self.max_lock_try_count)
  end

  if not self.max_frequency_scan_try_count or self.max_frequency_scan_try_count == nil then
    self.max_frequency_scan_try_count = 3
    self.wrdb:setp("max_frequency_scan_try_count", self.max_frequency_scan_try_count)
  end

  --[[
       PCI lock RDB variables.
       lock_config : the lock written in modem, automatically suspended in modem after attach.
                     It is persistent and re-used upon reboot/detach in the modem.
                     If it is overridden/cleared by sw/firmware update, it will still be set in
                     the modem to the last value written.
                     list of "pci:earfcn;" pairs.
       pci         : persistent and watched, changed externally via TR069 only.
                     list (csv) of pci(s) as per customer spec.
       self.pci    : pci list used locally in code.
                     list (csv) of pci(s) as per customer spec.
       pcil_earfcn : persistent and not watched (set/cleared in specific instances, for reporting).
                     list (csv) of earfcn(s) as per customer spec.
       pci_earfcn  : persistent and not watched (used at boot up, in code and set/cleared in
                     specific instances).
                     list of "pci:earfcn;" pairs.
  --]]

  -- last lock set in modem (pci:earfcn pairs)
  local pci_earfcn = self.wrdb:getp("pci_earfcn")

  -- if no pci, clear pci:earfcn pair lists
  if self.pci == "" then
    self.wrdb:setp("pci_earfcn", "")
    self.wrdb:setp("pcil_earfcn", "")
  else
    -- validate if pci:earfcn pairs have valid pci from pci list
    local result = self.util.validate_pci_earfcn_pairs(self.pci, pci_earfcn)
    if not result then
      self.wrdb:setp("pci_earfcn", "")
      self.wrdb:setp("pcil_earfcn", "")
    end
  end

  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."pci", "pciChanged", self)
  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."recovery_timer",
    "cell_recovery_time_update", self)
  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."celldetectrequested",
    "ncdp_scheduler", self)
  self.rdbWatch:addObserver("system.time.updated", "handle_time_update", self)
end

function QmiGDecorator:doDecorate()
  QmiGDecorator:__saveChain("modem_on_operating_mode")
  QmiGDecorator:__saveChain("state_machine_connect")
  QmiGDecorator:__changeImplTbl({"modem_on_operating_mode",
    "handle_network_state_transitions",
    "handle_network_state_change_at_connect",
    "modem_on_pci_list",
    "handle_state_transition_in_readmodem_state",
    "state_machine_prelock",
    "set_lock_in_modem",
    "state_machine_attach",
    "state_machine_connect",
    "state_machine_reboot",
    "pciChanged",
    "start_cell_recovery_timer",
    "stop_cell_recovery_timer",
    "handle_cell_recovery_timer_timeout",
    "cell_recovery_time_update",
    "reschedule_ncdp",
    "stop_ncdp_reschedule_timer",
    "stop_ncdp_timer",
    "ncdp_scheduler",
    "handle_ncdp_scheduler_timer_timeout",
    "parse_pci_scan_results",
    "collate_ncdp_results",
    "perform_pci_scan",
    "state_machine_ncdp",
    "handle_time_update",
    "init_pci_lock"})
end

LinkProfileDecorator.no_retry_verbose_call_end_reasons = {
  -- 3GPP
  [6] = {
    -- AUTH_FAILED
    [29] = true,
  },
}

function LinkProfileDecorator:doDecorate()
  -- Merge LinkProfileDecorator.no_retry_verbose_call_end_reasons
  -- table into the base default table. New table entries are created if
  -- not existing in the base table otherwise the extension entries override
  -- the base entries.
  local lp_no_retry_reasons = LinkProfileDecorator.__inputObj__.no_retry_verbose_call_end_reasons
  for reason, verbose_reason_table in pairs(LinkProfileDecorator.no_retry_verbose_call_end_reasons) do
    if not lp_no_retry_reasons[reason] then
      lp_no_retry_reasons[reason] = {}
    end

    for verbose_reason, val in pairs(verbose_reason_table) do
      lp_no_retry_reasons[reason][verbose_reason] = val
    end
  end
end

function CracknellDecorator.doDecorate()
  CracknellDecorator.__inputObj__.qmiG = QmiGDecorator:decorate(CracknellDecorator.__inputObj__.qmiG)
  CracknellDecorator.__inputObj__.linkProfile = LinkProfileDecorator:decorate(CracknellDecorator.__inputObj__.linkProfile)
end

return CracknellDecorator
