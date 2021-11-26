--
-- SAS Client entry point. It starts the SAS client operation.
--
-- Copyright (C) 2019 NetComm Wireless Limited.
--
-- Refactor started at revision 89083.

local SasClient = require("wmmd.Class"):new()
local snextra = require("snextra")
require('tableutil')

function SasClient:setup(p)
  self.l = p.l
  self.watcher = p.watcher
  self.rdb = p.rdbWatch.rdb
  self.rdbWatch = p.rdbWatch
  self.util = p.util
  self.json = p.json
  self.wutil = require("wmmd.util")

  self.t = p.turbo
  self.i = self.t.ioloop.instance()

  self.smachine = require("wmmd.smachine")
  self.ssm = self.smachine.get_smachine("sas_smachine")
  self.sas_started = false

  self.stateMachineHandlers = {
    -- Default handlers
    {state="sas:idle", func="state_machine_idle", execObj=self},
    {state="sas:attach", func="state_machine_attach", execObj=self},
    {state="sas:initiate",func="state_machine_initiate", execObj=self},
    {state="sas:operate", func="state_machine_operate", execObj=self},
    {state="sas:finalise",func="state_machine_finalise", execObj=self},
    -- SAS specific state handles
    {state="sas:post_attach", func="state_machine_post_attach", execObj=self},
    {state="sas:wait_ip_interface", func="state_machine_wait_ip_interface", execObj=self},
    {state="sas:unregistered", func="state_machine_unregistered", execObj=self},
    {state="sas:registered", func="state_machine_registered", execObj=self},
    {state="sas:no_grant", func="state_machine_no_grant", execObj=self},
    {state="sas:granted", func="state_machine_granted", execObj=self},
    {state="sas:authorized", func="state_machine_authorized", execObj=self},
    {state="sas:suspended", func="state_machine_suspended", execObj=self}
  }

  self.ecgi_rdb = 'wwan.0.manual_cell_meas.ecgi'
  self.attach_rdb = 'wwan.0.system_network_status.attached'
  -- the qdiagtxctrl allowed to tx or not
  self.tx_state_rdb = 'wwan.0.txctrl.tx_state'
  self.registrationCmdKey = "sas.registration.cmd"
  self.registration_state_rdb = "sas.registration.state"
  self.pEarfcnKey = "wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq"
  self.dl_bandwidth_rdb = "wwan.0.radio_stack.e_utra_measurement_report.dl_bandwidth"
  self.serv_pci_rdb = "wwan.0.radio_stack.e_utra_measurement_report.servphyscellid"
  self.cellListKey = "sas.inter_cells"
  self.install_cell_list = ""
  self.cell_lock_list_retry_cnt = 0
  self.PCI_LOCK_LIST_MAX_RETRY = tonumber(self.rdb.get('sas.config.pci_lock_list.max_retry')) or 30
  self.nit_connected_rdb = "nit.connected"
  self.nit_connected = self.rdb.get(self.nit_connected_rdb) == "1"
  self.ip_state_rdb = "sas.ip_state"
  self.exclude_weak_cell = self.rdb.get("sas.exclude_weak_cells") == "1"
  self.intra_cells_rdb = "sas.intra_cells"
  self.lock_list_rdb = "wwan.0.neighbour_lock_list"

  self.MIN_RSRP             = -120      -- Do not use cells with RSRP less than -120
  self.NO_RSRP              = -200      -- use -200 to indicate no rsrp recorded
  self.MEGA                 = 1000000
  self.INVALID_VALUE        = 103       -- One or more parameters have invalid value
  self.UNSUPPORTED_SPECTRUM = 300       -- Frequency outside CBRS range
  self.INTERFERENCE         = 400       -- Requested operation parameters cause too much interference
  self.GRANT_CONFLICT       = 401       -- Conflict with an existing Grant of the same CBSD
  self.TERMINATED_GRANT     = 500       -- Grant is terminated, remove grant from local & rdb
  self.SUSPENDED_GRANT      = 501       -- Grant is suspended, continue heartbeating.
  self.UNSYNC_OP_PARAM      = 502       -- Grant state is out of sync between the CBSD and the SAS.
  self.MAX_FILTER_CELLS     = 9
  self.SAS_MAX_HEARTBEAT_TIME = 200     -- maximum time for sas heartbeat, do not schedule beyond this
  self.MAX_JOB_EXECUTION_TIME = 30      -- job taking longer than this many seconds will be purged
  self.MAX_PCI_LOCK_LIST_SIZE = 30      -- as per Qualcomm doc (multi cell lock enhancement)
  self.MAX_PCI_PER_CELL       = 10      -- max intra-frequency per cell
  self.DETACH_TIME_LIMIT      = 10      -- period in sec that network can be detached without reset PCI lock
  self.MIN_HEARTBEAT_INTERVAL = 60      -- minimum time between 2 consecutive heartbeats (see. <OWA-4000>)
  self.REG_PENDING            = 200     -- response code for registration pending in multi-step registration

  self.RBsToMHz               = {[100]=20, [75]=15, [50]=10, [25]=5, [15]=3, [6]=1.4}
  self.CBRS_START             = 3550    -- standard CBRS range in Mhz
  self.CBRS_END               = 3700
  self.GRANT_SIZE             = 10      -- grant bandwidth size in Mhz
  self.MAX_SAS_GRANT_INDEX    = 15      -- maximum number of grants, depending on GRANT_SIZE
  self.cbrs_grants            = {}      -- grant table indexed by cbrs 10Mhz segment count (1-15)
  self.cells                  = {}      -- all cells, use pci_earfcn as key
  self.active_cells           = {}      -- cells that are currently visible
  self.rateLimits             = {}      -- table to keep old values & timestamps for rate limiting function
  self:cell_set_rdb()                   -- clear all cell info rdbs

  -- Our maximum Eirp capability
  -- capability/1MHz should be same as capability/10MHz but we have to
  -- artificially limit it due to Part 96 PSD limit and a SAS server bug
  -- Early on we decided we would report 32/42 to the server and we were never
  -- able to fix the error. The server has a similar bug that prevents us
  -- asking for 37 if cap isn't 47 IIRC.
  -- Further Note: the usual +/- 10 to convert 1M->10M doesn't hold for
  -- capability because we run out of juice. Strictly speaking it is 37dBm/1MHz
  -- and min(41,self.eirpCapability_perMHz+10)
  self.eirpCapability = tonumber(self.rdb.get('sas.antenna.eirp_cap')) or 41 -- per 10 MHz
  self.eirpCapability_perMHz = self.eirpCapability - 10 -- per 1 MHz

  -- Our antenna gain
  self.antenna_gain = tonumber(self.rdb.get('sas.antenna.gain')) or 19

  -- Our minimum Tx power
  self.minEirp = tonumber(self.rdb.get('sas.antenna.eirp_min')) or 24 -- per 10 MHz
  self.minEirp_perMHz = self.minEirp - 10 -- per 1 MHz

  -- SAS gives radiated (EIRP) dBm/1MHz but modem needs conducted wideband dBm. This conversion is as follows:
  -- 1) check against the min/max below
  -- 2) We subtract the antenna gain from it to get conducted/1MHz
  -- 3) Since the power spectrum is flat we can add 10dB to convert dBm/1MHz -> dBm/10MHz (this is the worst case when eNB is scheduling <= 55RBs.
  -- 4) We just "forget" the bandwidth part of the unit. It's safe to use the same number as a wideband value since if more RBs are scheduled the power /10MHz will drop, not rise.
  -- The next two lines just reverse this process to set the max that can be used in step 1.
  self.maxEirp = tonumber(self.rdb.get('sas.antenna.eirp_max')) or self.eirpCapability -- per 10 MHz
  self.maxEirp_perMHz = self.maxEirp - 10 -- per 1 MHz

  self.measCapStr = {"", "RECEIVED_POWER_WITHOUT_GRANT", "RECEIVED_POWER_WITH_GRANT"}
  self.measCapIndex = (tonumber(self.rdb.get('sas.config.measCapability')) or 0) + 1
  self.measReport10Mhz = self.rdb.get("sas.config.measReport10Mhz") == '1'

  self.hbeatbundle = {['grants'] = {}, ['enabled'] = self.rdb.get('sas.heartbeat_bundling_enabled') == '1' }
  self.next_should_fail = {}
  self.cell_meas_cnt = ""
  self.manual_cell_meas_seq = ""
  self.meas_report_tbl = {}
  self.apnTimer = nil -- handle to a timer used before turning off sas transmission
  self.rdb_registration_strs = {
     ['cbsdid'] = 'cbsdId',
     ['state'] =  'operationState',
  }
  self.rdb_grant_strs = {
    ['id'] = 'grantId',
    ['freq_range_low'] = 'lowFrequency',
    ['freq_range_high'] = 'highFrequency',
    ['max_eirp'] =  'maxEirp',
    ['expire_time'] =  'grantExpireTime',
    ['channel_type'] = 'channelType',
    ['state'] =  'operationState',
    ['reason'] = 'responsecode',
    ['response_message'] = 'responseMessage',
    ['response_data'] = 'responseData',
    ['next_heartbeat'] = 'nextHeartbeat',
    ['grantRequiredFor'] ='grantRequiredFor',
    ['legacyTime'] ='legacyTime',
  }
  self.ssl_opts = {
    verify_ca = true,
    ca_dir    = self.rdb.get('sas.config.cadir'),
    cert_file = self.rdb.get('sas.config.certfile'),
    priv_file = self.rdb.get('sas.config.keyfile'),
    priv_pass = snextra.generate('cbsd')
  }

  self.timeout_values = {
    ['deregister_retry']          = { 60 * 1000, function(self) self:deregisterRetry() end },
    ['register_retry']            = { 60 * 1000, function(self) self:registerRetry() end },
    ['spectrumInquiry_retry']     = { 60 * 1000, function(self) self:spectrumInquiryRetry() end },
    ['grant_retry']               = { 60 * 1000, function(self, grant, id) self:grantRetry(grant, id) end},
    ['relinquish_retry']          = { 60 * 1000, function(self, grant) self:relinquishRetry(grant) end },
    ['grant_relinquish_wait_time']= { 60 * 1000, function(self, grant) self:grantRelinquishWaitTime(grant) end },
    ['heartbeat_retry']           = { 60 * 1000, function(self, grant) self:heartbeatRetry(grant) end },
    ['heartbeat_bundle_retry']    = { 60 * 1000, function(self) self:heartbeatBundleRetry() end },
  }

  self.l.log("LOG_NOTICE", string.format("using sas url %s", self.rdb.get("sas.config.url")))

  -- qmi modules
  self.l.log("LOG_NOTICE", "Loading SAS modules")

  self.services = {
    sas_m = "sas.sas_machine",
  }

  self.qs = {}
  for i,v in pairs(self.services) do
     self.l.log("LOG_NOTICE", "Loading [" .. i .. "]=" .. v)
     self.qs[i] = require(v):new()
     self.qs[i]:setup(p)
  end
  self.sas = self.qs["sas_m"]

  self.state_schedule = 'pause'
  self.saved_func = false

  self.wait_cbrs_up_config_count = 0
end

function SasClient:sas_stop_services()
  self.l.log("LOG_NOTICE", "stopping sas services")
  self.watcher.invoke("sys","stop")
  self.watcher.reset()
end

function SasClient:sas_start_services()
  self.l.log("LOG_NOTICE", "initializing sas services")
  for _,qe in pairs(self.qs) do
    if qe.init then
      qe:init()
    end
  end
end

-- Set the modem's tx power such that no 10MHz chunk will have average dBm/MHz above the given value
function SasClient:setModemTxPower(band)
  local disableCutback = 0  -- eanble the cutback

  if not band then
     local rdbBand = self.rdb.get("wwan.0.currentband.config")
     if rdbBand and rdbBand ~='' then
        band = tonumber(string.match(rdbBand, "%d+"))
     end
  end
  if not band then  -- if the rdb config band is invalid, give a error
     self.l.log("LOG_ERR", 'Invalid rdb config: wwan.0.currentband.config')
     return false
  end

  -- use the max value in the grants collection for ServingCell
  local servingCellEirp = 0
  for _,g in pairs(self:serving_cell_grants()) do
    servingCellEirp = servingCellEirp < tonumber(g.maxEirp) and tonumber(g.maxEirp) or servingCellEirp
  end

  -- if there is no grant (for example, the first time power on), set the txPower to the device eirp capability
  local txPower = servingCellEirp ~= 0 and servingCellEirp or self.eirpCapability_perMHz

  -- set the modem's tx power according to the request via rdb service.luaqmi.command
  -- The final power value to set should be in dBm per 10 MHz.
  local command = string.format("setTxPower,%d,%d,%d", band, txPower - self.antenna_gain + 10, disableCutback)
  self.rdb.set('service.luaqmi.command', command)
  return true
end

function SasClient:grant_table(grant, force)
  if not force then
    return self.ctx.grants and self.ctx.grants[grant] or false,
           self.ctx.state_machine.grants and self.ctx.state_machine.grants[grant] or nil
  end

  -- force creating default entry in ctx.grants & ctx.state_machine.grants if not exist
  if not self.ctx.grants then self.ctx.grants = {} end
  if not self.ctx.grants[grant] then
    self.ctx.grants[grant] = {
      ['timeouts'] = {},
      ['previous_expires'] = {},
      ['req_tab']  = {
          ['cbsdId'] = self.ctx.state_machine.user_values.cbsdId,
        },
    }
  end

  if not self.ctx.state_machine.grants then self.ctx.state_machine.grants = {} end
  if not self.ctx.state_machine.grants[grant] then
    self.ctx.state_machine.grants[grant] = {
        ['cbsdId'] = self.ctx.state_machine.user_values.cbsdId,
        ['user_values'] = {},
    }
  end

  return self.ctx.grants[grant], self.ctx.state_machine.grants[grant]
end

function SasClient:set_state(rstate, action)
  if rstate then
    self.ctx.state_machine.rstate = rstate
  end
  if action then
    self.ctx.state_machine.action = action
  end
end

function SasClient:set_grant_state(grant, gstate, action)
  if not grant then
    self.ctx.state_machine.action = action
    return
  end
  local _, uv = self:grant_table(grant, true)
  if uv then
    uv.gstate = gstate
  end
  if action then
    uv.action = action
  end
end

function SasClient:erase_timeout(name, grant)
  local place
  local handle = nil
  if grant then
    place = self:grant_table(grant, false)
    if place then
        place = place.timeouts
    end
  else
    place = self.ctx.timeouts
  end
  if place and place[name] and place[name].handle then
    handle = place[name].handle
    place[name] = nil
  end
  return handle
end

function SasClient:stop_timeout(name, grant, id)
  if id then name = name..id end
  local handle = self:erase_timeout(name, grant)
  if handle then
    self.l.log("LOG_DEBUG", string.format("stop timeout, handle:%s name:%s grant:%s id:%s", handle, name, grant, id))
    self.i:remove_timeout(handle)
  end
end

function SasClient:start_timeout(time_ms, event, name, grant, id)
  self:stop_timeout(name, grant, id)
  local place
  local cbdata = {
    ['grant'] = grant,
    ['event'] = event,
    ['name'] = id and name..id or name,
    ['id'] = id,
  }
  if not grant then
    if not self.ctx.timeouts then
      self.ctx.timeouts = {}
    end
    place = self.ctx.timeouts
  else
    place = self:grant_table(grant, true).timeouts
  end
  local timeout_event = function(cbdata)
    self.l.log("LOG_DEBUG", "the timer["..cbdata.name.."] for grant["..(cbdata.grant and cbdata.grant or 'none').."] time out")
    self:erase_timeout(cbdata.name, cbdata.grant)
    if cbdata.event then cbdata.event(self, cbdata.grant, cbdata.id) end
  end
  local handle = self.i:add_timeout(time_ms + self.t.util.gettimemonotonic(), timeout_event, cbdata)
  if handle then
    cbdata.handle = handle
    place[cbdata.name] = cbdata
  end
  self.l.log("LOG_DEBUG", string.format("start_timeout, handle:%s name:%s grant:%s id:%s", handle, name, grant, id))
end

function SasClient:start_heartbeatbundle_timer(user_tab, timeout, grant)
  self.sas:set_values(self.ctx, user_tab, grant)
  local single_hbeat = function()
    if timeout == 0 then self:heartbeat(user_tab, nil, grant) return end
    local heartbeat_expire = function(self, grant) self:heartbeatExpire(grant) end
    self:start_timeout(timeout * 1000, heartbeat_expire, 'heartbeat_expire', grant)
  end
  if not self.hbeatbundle.enabled then single_hbeat() return end

  -- don't bundle if heartbeat need to be sent earlier than the next bundle by 10s
  local timeouts = self.ctx.timeouts or {}
  if timeouts.heartbeatbundle_expire and self.hbeatbundle.scheduled and timeout > 0 then
    if  (os.time() + timeout) < (self.hbeatbundle.scheduled - 10) then single_hbeat() return end
  end

  self.hbeatbundle.grants[grant] = timeout
  if timeouts.heartbeatbundle_expire then return end

  local heartbeatbundle_expire = function(self)
    for g,_ in pairs(self.hbeatbundle.grants) do
      self:start_std_timeout('heartbeat_retry', g)
      self:set_grant_state(g, false, 'heartbeat')
    end
    self.hbeatbundle.scheduled = nil
    self:schedule_action('heartbeat', nil, nil, self.hbeatbundle)
  end
  self.hbeatbundle.scheduled = os.time() + timeout
  self:start_timeout(timeout * 1000, heartbeatbundle_expire, 'heartbeatbundle_expire')
end

-- watch the timer, if changed, reload it
function SasClient:timer(key, val)
  local timerLength = tonumber(val)
  if timerLength and timerLength > 0 then
    local timerName = string.sub(key,string.len("sas.timer.")+1)
    self.timeout_values[timerName][1] = timerLength*1000
  end
end

-- get the timer length(second) from the rdb, if not valid, keep the default values.
-- add the timer watcher
function SasClient:read_sas_timer_settings()
  for timerName,_ in pairs(self.timeout_values) do
    local timerLength = tonumber(self.rdb.get('sas.timer.'..timerName))
    if timerLength and timerLength > 0 then
        self.l.log("LOG_DEBUG", "read timer from rdb " .. timerName .. ', its value:'..timerLength)
        self.timeout_values[timerName][1] = timerLength*1000
    end
    self.rdbWatch:addObserver("sas.timer."..timerName, "timer", self)
  end
end

function SasClient:start_std_timeout(name, grant, id)
  if not self.timeout_values[name] then
    error('Code error timeout value '..name..' does not exist')
    return
  end
  self.l.log("LOG_DEBUG", "start the standard timer["..name.."] for grant(" .. (grant and grant or 'none').. "), timer length is:"..self.timeout_values[name][1])
  self:start_timeout(self.timeout_values[name][1], self.timeout_values[name][2], name, grant, id)
end

function SasClient:stop_all_grant_timeouts(grant)
  local place
  if grant then
    place = self:grant_table(grant, false)
    if place then
        place = place.timeouts
    end
    -- remove grant from the next hearbeat bundle
    self.hbeatbundle.grants[grant]=nil
    if next(self.hbeatbundle.grants) == nil then
      self:stop_timeout('heartbeat_bundle_retry')
      self:stop_timeout('heartbeatbundle_expire')
    end
  else
    place = self.ctx.timeouts
  end
  for k,v in pairs(place) do
    if v.handle then
      self.l.log("LOG_DEBUG", "Stop Timeout "..v.name)
      self.i:remove_timeout(v.handle)
      place[k] = nil
    end
  end
end

function SasClient:stop_all_timeouts()
  if self.ctx.grants then
    for k,_ in pairs(self.ctx.grants) do
      self:stop_all_grant_timeouts(k)
    end
  end
  self:stop_all_grant_timeouts()
  self.hbeatbundle.grants = {}
end

function SasClient:no_xmit(grant)
  if grant then
    local _, uv = self:grant_table(grant, false)
    if uv and uv['can_xmit'] then
      uv['can_xmit'] = false

      if uv.operationState == 'AUTHORIZED' then
        uv.operationState = 'GRANTED'
        self:grant_set_rdb(uv)
      end
    end
  end
end

function SasClient:can_xmit(grant)
  if grant then
    local _, uv = self:grant_table(grant, false)
    if uv and not uv['can_xmit'] then
      uv['can_xmit'] = true
    end
  end
end

function SasClient:transmitExpire(grant)
  local _, uv = self:grant_table(grant, false)
  local state = uv and uv.operationState or 'unknown'
  self:no_xmit(grant)
  self:stop_all_grant_timeouts(grant)
  self:remove_schedule(grant)
  self.l.log("LOG_NOTICE", string.format("transmit expire for g:%s state:%s", grant, state))
  if not uv then self.l.log("LOG_ERR", string.format("transmitExpire() no %s in grant_table", grant)) return end
  if state == 'AUTHORIZED' then
    self.l.log("LOG_ERR", string.format("transmitExpire() sending heartbeat g:%s", grant))
    self:heartbeat(uv, nil, grant)
  else
    self:start_heartbeatbundle_timer(uv, self:seconds_to_heartbeat(grant), grant)
  end
  if uv.grantRequiredFor == 'Legacy' then self:resumeLegacyGrantRelinquishTimer(uv) end
end

function SasClient:remove_grant(grant)
  self:stop_all_grant_timeouts(grant)
  local _, uv = self:grant_table(grant, false)
  local index = uv and uv.index or nil
  if self.ctx.grants and self.ctx.grants[grant] then
    self:remove_grant_rdb(grant)
    self.ctx.grants[grant] = nil
  end
  if self.ctx.state_machine.grants[grant] then
    self.ctx.state_machine.grants[grant] = nil
  end
  if index then self.cbrs_grants[index] = nil end
end

function SasClient:remove_all_grants()
  if self.ctx.grants then
    for k,_ in pairs(self.ctx.grants) do
      self:remove_grant(k)
    end
  end
  self.cbrs_grants = {}
end

local saver_vals = {
  "grantExpireTime",
  "grantId",
  "heartbeatInterval",
  "cbsdId",
  "transmitExpireTime",
  "lowFrequency",
  "highFrequency",
  "maxEirp",
  "grantRenew",
  "operationState",
  "channelType",
  "grantRequiredFor",
  "responseMessage",
  "index",
}

function SasClient:seconds_to_event(te)
  -- print('event date and time is '..te)
  local now = os.time()
  local later = self.util.parse_json_date(te)
  return later - now
end

function SasClient:seconds_to_heartbeat(grant)
  if grant then
    local _, uv = self:grant_table(grant, false)
    if uv and uv.heartbeatInterval and uv.heartbeatInterval > 0 then
      return uv.heartbeatInterval > self.SAS_MAX_HEARTBEAT_TIME and
             self.SAS_MAX_HEARTBEAT_TIME or uv.heartbeatInterval -- max value set by <OWA-4000>
    end
    if uv and uv.transmitExpireTime then
      local timeout = self:seconds_to_event(uv.transmitExpireTime)
      if timeout and timeout > 0 then
        return timeout
      end
    end
  end
  return 60
end

function SasClient:grantExpire(grant)
  -- grant has expired, removed from table. request again if any cell needs it.
  self.l.log("LOG_NOTICE", 'grant_expire '..self.util.fstr(grant)..', removed now')
  self:remove_grant(grant)
end

function SasClient:sas_timers(grant)
  if grant then
    local cv, uv = self:grant_table(grant, false)
    if not cv.previous_expires then
      cv.previous_expires = {}
    end
    local pv = cv.previous_expires
    if uv.transmitExpireTime and (pv.transmitExpireTime ~= uv.transmitExpireTime) then
      pv['transmitExpireTime'] = uv.transmitExpireTime
      local timeout = self:seconds_to_event(uv.transmitExpireTime)
      if not timeout or timeout < 60 then timeout = 60 end
      self:can_xmit(grant)
      local transmit_expire = function(self, grant) self:transmitExpire(grant) end
      self:start_timeout(timeout * 1000, transmit_expire, 'transmit_expire', grant)
    end
    if uv.grantExpireTime and (pv.grantExpireTime ~= uv.grantExpireTime) then
      pv['grantExpireTime'] = uv.grantExpireTime
      local timeout = self:seconds_to_event(uv.grantExpireTime)
      if not timeout or timeout < 0 then timeout = 60 end
      local grant_expire = function(self, grant) self:grantExpire(grant) end
      self:start_timeout(timeout * 1000, grant_expire, 'grant_expire', grant)
    end
    return
  end
end

function SasClient:deregister()
  -- erase registration, timer etc.
  self.l.log("LOG_NOTICE", "Deregistered!")
  self:set_state('Unregistered', nil)
  self:stop_all_timeouts()
  self:remove_all_grants()
  self:cancel_apn_timer()
  self:update_sas_transmit(false)
  self.ctx.scheduled = {}
  self.ctx.state_machine.user_values = {}
  self.ctx.req_tab = {}
  self.rdb.set(self.registration_state_rdb, 'Unregistered')
  self.rdb.set('sas.registration.response_code', '')
  self.ctx.current_response.cbsdId = ''
  self.ctx.current_response.operationState = 'Unregistered'
  self:registration_to_rdb(self.ctx.current_response)
  self:restart()
  -- resume registration action if it is pending
  self.rdb.set(self.registrationCmdKey, self.rdb.get(self.registrationCmdKey))
end

-- our model is one request at a time, the first must complete
-- before the next can be initiated (more for turbo's sake)
function SasClient:add_schedule_action(action, grant, reqtabId, bundle)
  if not self.ctx.scheduled then
    self.ctx.scheduled = {}
  end
  if action then
    -- do not add duplicated action
    for _,v in pairs(self.ctx.scheduled) do
      if v.action == action and v.grant == grant and v.reqtabId == reqtabId and v.bundle == bundle then return end
    end
    table.insert(self.ctx.scheduled, {['action']=action, ['grant']=grant, ['reqtabId']=reqtabId, ['bundle']=bundle})
  end
end

-- remove all schedules related to the given grant
function SasClient:remove_schedule(grant)
  -- NB. Search from position 2, position 1 is the task being executed.
  -- When the task is completed, request_complete() will remove pos 1 unconditionally.
  for i=#self.ctx.scheduled, 2, -1 do
    if self.ctx.scheduled[i].grant == grant then
      self.l.log("LOG_INFO", string.format("REMOVE SCHEDULE:%s a:%s g:%s", i, self.ctx.scheduled[i].action, grant))
      table.remove(self.ctx.scheduled, i)
    end
  end
end

-- check if schedule exist for the given action
function SasClient:has_schedule(action, grant, reqtabId)
  for _,v in pairs(self.ctx.scheduled) do
    if v.action == action and v.grant == grant and v.reqtabId == reqtabId then return true end
  end
  return false
end

-- get measurement report if the request needs it
function SasClient:updateMeasReport(request)
  if request == 'grant' then
    local req_tab = self.ctx.req_tab or {}
    req_tab.measReport = (self.ctx.state_machine.user_values.measReportConfig
      and self.ctx.state_machine.user_values.measReportConfig[1] == 'RECEIVED_POWER_WITHOUT_GRANT'
      and not self.measReported) and self:get_meas_report() or nil
    return
  end

  if request == 'heartbeat' then
    local grants = self.ctx.bundle and self.hbeatbundle.grants or {[self.ctx.grant]=0}
    for g,_ in pairs(grants) do
      local req_tab = self.ctx.grants[g].req_tab
      req_tab.measReport = (req_tab.measReportConfig == 'RECEIVED_POWER_WITH_GRANT') and self:get_meas_report() or nil
    end
  end
end

function SasClient:schedule_action(action, grant, reqtabId, bundle)
  -- schedule action to self.ctx.scheduled[] stack
  if action then
    self.l.log("LOG_INFO", string.format('schedule_action: adding action:%s g:%s id:%s, bundle:%s',
      action, grant, reqtabId, bundle))
    self:add_schedule_action(action, grant, reqtabId, bundle)
  end
  -- if nothing to do, return
  if not self.ctx.scheduled or #self.ctx.scheduled < 1 then
    self.l.log("LOG_INFO", "schedule_action: nothing to do")
    return
  end

  -- if already performing a task, don't confuse things
  if self.ctx.action and self.ctx.action ~= 'wait_user' then
    local executed_time = os.time() - self.util.parse_json_date(self.ctx.action_ts or os.time())
    self.l.log("LOG_INFO", string.format("schedule_action: a[%s|%s|%s] queue pos:%s, a[%s|%s|%s] started %ss ago",
      action, grant, reqtabId, #self.ctx.scheduled, self.ctx.action, self.ctx.grant, self.ctx.bundle, executed_time))
    if executed_time > self.MAX_JOB_EXECUTION_TIME then
      self:errorResponse(self.ctx.action, '[ABORT]', '[taking too long, thread maybe dead]')
    end
    return
  end

  -- take the first task and perform it. NB poping the task is done
  -- in request_complete
  local tbd = self.ctx.scheduled[1]
  self.ctx.action = tbd.action
  self.ctx.grant = tbd.grant
  self.ctx.action_ts = self.util.zulu(os.time())
  self.ctx.bundle = tbd.bundle
  if tbd.reqtabId then
    -- use custom request table for this request
    self.util.shallow_copy(self.ctx.req_tab, self.ctx.queued_req_tabs[tbd.reqtabId])
  end

  -- update measReport now, just prior to sending the request to get latest measurement info
  self:updateMeasReport(self.ctx.action)

  self.l.log("LOG_INFO", string.format("schedule_action: execute action[%s|%s|%s]", self.ctx.action, self.ctx.grant, tbd.reqtabId))
  if self.schedule ~= nil then
    self.l.log("LOG_DEBUG", "schedule_action: creating do_schedule() and callback do_action()")
    local do_schedule = function() self.i:add_callback(self.sas.do_action, self.sas) end
    self:schedule(do_schedule)
  else
    self.l.log("LOG_DEBUG", "schedule_action: adding callback do_action()")
    self.i:add_callback(self.sas.do_action, self.sas)
  end
end

function SasClient:logResponse(msg, responsecode, grant)
  local smsg
  if grant then
    local _, uv = self:grant_table(grant, false)
    smsg = ' Rstate '..(self.ctx.state_machine.rstate or 'nil')..
      ' Gstate '..((uv and uv.gstate) or 'nil')..
      ' grant '..grant
  else
    smsg = ' Rstate '..(self.ctx.state_machine.rstate or 'nil')..
      ' Gstate '..(self.ctx.state_machine.gstate or 'nil')..
      ' Action '..(self.ctx.state_machine.action or 'nil')
  end
  local rmsg = self.ctx.current_response.responseMessage or ''
  local dmsg = self.ctx.current_response.responseData and self.ctx.current_response.responseData[1] or ''
  local rcode = responsecode or 'no response code'
  self.l.log("LOG_ERR", msg..smsg..' response '..rcode..' responseMessage '..rmsg..' responseData '..dmsg)
end

function SasClient:non_zero_response(responsecode, grant)
  self:logResponse('non_zero_response', responsecode, grant)
  local rc = tonumber(responsecode)
  local cbsdInvalid = false
  if rc == 103 then
    local rdata = self.ctx.current_response.responseData and self.ctx.current_response.responseData[1] or ''
    cbsdInvalid = rdata:match('cbsdId')
  end
  if rc == 105 or cbsdInvalid then
    local do_re_register = function()
      self:deregister()
      self:set_state(nil, 'registration')
      self:start_std_timeout('register_retry')
      self:schedule_action('registration', nil)
    end
    self.ctx.scheduled = {}
    -- deregister immediately will clear data & crash current thread, do it in next ioloop instead
    self.i:add_callback(do_re_register, self)
   end
end

function SasClient:heartbeatExpire(grant)
  -- time to send a new heartbeat. After the response, we will set the
  -- interval timer for the next heartbeat
  -- Set the error timer
  -- ask for mearurement report
  self.l.log("LOG_INFO", 'heartbeat_expire '..self.util.fstr(grant))
  self:start_std_timeout('heartbeat_retry', grant)
  self:set_grant_state(grant, false, 'heartbeat')
  self:schedule_action('heartbeat', grant)
end

function SasClient:heartbeatBundleRetry()
  self:start_std_timeout('heartbeat_bundle_retry')
  for g,_ in pairs(self.hbeatbundle.grants) do self:set_grant_state(g, false, 'heartbeat') end
  self:schedule_action('heartbeat', nil, nil, self.hbeatbundle)
end

function SasClient:heartbeatRetry(grant)
  -- heartbeat send attempt has timed out
  -- retry
  -- keep count
  self.l.log("LOG_WARNING", 'heartbeat_retry '..self.util.fstr(grant))
  self:start_std_timeout('heartbeat_retry', grant)
  self:set_grant_state(grant, false, 'heartbeat')
  self:schedule_action('heartbeat', grant)
end

function SasClient:heartbeat(user_tab, dtab, grant)
  self.sas:set_values(self.ctx, user_tab, grant)
  if dtab then self.sas:set_values_nil(self.ctx, dtab, grant) end
  self:set_grant_state(grant, 'heartbeat', 'heartbeat')
  self:start_std_timeout('heartbeat_retry', grant)
  self:schedule_action('heartbeat', grant)
end

function SasClient:heartbeatResponse(responsecode)
  self.l.log("LOG_DEBUG", string.format("heartbeatResponse(rc:%s) g:%s b:%s", responsecode, self.ctx.grant, self.ctx.bundle))
  local bundle = self.ctx.bundle
  local grant = self.ctx.grant
  responsecode = self:request_complete(responsecode)

  if bundle then
    -- iterate through the bundle of heartbeat responses and process one by one
    for i=1, #bundle.responses do
      self.ctx.current_response = bundle.responses[i]
      grant = self.ctx.current_response.grantId
      if grant then
        local responsecode = self:swap_responsecode(grant, 'heartbeat', bundle.responses[i].responseCode)
        self:processHeartbeatResponse(grant, responsecode)
      end
    end
    bundle.responses = nil
  else
    self:processHeartbeatResponse(grant, responsecode)
  end

  -- special indication to do next scheduled task
  self:schedule_action(nil, nil)
end

function SasClient:processHeartbeatResponse(grant, responsecode)
  self.l.log("LOG_DEBUG", 'process heartbeat response for grant '..self.util.fstr(grant))
  local responsecodenum = tonumber(responsecode)

  -- Debug code, negative code can only be injected by test command eg. sas.command='fail -1:heartbeat:<grant>'
  if responsecodenum == -1 then
    self.l.log("LOG_NOTICE", string.format("TEST: simulate heartbeat timeout for g:%s", grant))
    return
  end

  self:stop_timeout('heartbeat_retry', grant)
  local _,uv = self:grant_table(grant, false)
  if not uv or uv.operationState == 'REMOVED' then
    self.l.log("LOG_NOTICE", string.format('g:%s state=REMOVED, ignore heartbeat response', grant))
    return
  end
  local osval = (uv and uv['operationState']) or 'GRANTED'
  local newstate = responsecodenum == 0 and 'AUTHORIZED' or 'GRANTED'
  local diff = self.util.copy_only_different(uv, self.ctx.current_response, saver_vals)
  uv['responsecode'] = responsecodenum
  if diff or newstate ~= osval then
    uv['operationState'] = newstate
    self.sas:set_values(self.ctx, uv, grant)
  end

  uv.rdata = self.ctx.current_response.responseData and self.ctx.current_response.responseData[1] or ''
  uv.rmesg = self.ctx.current_response.responseMessage or ''

  self:informIA()

  local relinquish = function(uv)
    self.l.log("LOG_NOTICE", string.format('heartbeatResponse() error:%s relinquish g:%s', responsecodenum, uv.grantId))
    self:relinquish_grant(uv)
  end

  -- if heartbeat fails and we have no authorized grant, attach on other cells.
  -- TODO : currently applied to install mode only, further testing required for
  -- normal operation cases, we need to cater for handover/re-selection/resume/suspension cases
  local any_authorized = function() for key,_ in pairs(self.active_cells) do if self:is_authorized(key) then return true end end end
  if self.nit_connected and responsecodenum ~= 0  and not any_authorized() then
    self.l.log("LOG_NOTICE", string.format("heartbeatResponse(): responsecodenum:%s grant:%s for install", responsecodenum, uv.grantId))
    self:change_grant_request_lock()
  end
  if responsecodenum == self.UNSYNC_OP_PARAM then
    -- <OWA-5400>: if serving cell grant then relinquish & renew all grants
    if uv.grantRequiredFor == 'ServingCell' then
      self.l.log("LOG_NOTICE", 'serving cell grant has 502, relinquish all grants')
      self:renew_all_grants(uv.cbsdId)
    else
      relinquish(uv) -- for neighbour cell, just relinquish that single grant
    end
  elseif responsecodenum == self.TERMINATED_GRANT then
    if uv.index then self.cbrs_grants[uv.index] = nil end
    self:grant_set_rdb(uv)
    if self.apnTimer then
      self.apnTimerPostAction = function() self:remove_grant(grant) end
    else
      self:remove_grant(grant)
    end
  elseif responsecodenum == 0 or responsecodenum == self.SUSPENDED_GRANT then
    self.sas:set_values_nil(self.ctx, {measReportConfig="", measReport=""}, grant)
    uv.measReportConfig = self.ctx.current_response.measReportConfig and self.ctx.current_response.measReportConfig[1]
    self:sas_timers(grant)
    local heartbeat_sec = self:seconds_to_heartbeat(grant) - 15 -- to hbeat well before TX expire time (MYNA-472)
    uv.sec_to_heartbeat = heartbeat_sec
    if responsecodenum == 0 then
      -- set timer for next heartbeat, must allow time for retry in case next heartbeat fails
      uv.sec_to_tx_expired = self:seconds_to_event(uv.transmitExpireTime)
      local tx_sec_after_next_hbeat = uv.sec_to_tx_expired - uv.sec_to_heartbeat
      local sec_hbeat_retry = self.timeout_values.heartbeat_retry[1] / 1000 + 10 -- allow at least 10s before tx expires
      if tx_sec_after_next_hbeat < sec_hbeat_retry then
        uv.sec_to_heartbeat = uv.sec_to_tx_expired - sec_hbeat_retry
        local min = function(a,b) return (a<b) and a or b end
        local min_interval = min(heartbeat_sec, self.MIN_HEARTBEAT_INTERVAL)
        if uv.sec_to_heartbeat <= min_interval then uv.sec_to_heartbeat = min_interval end
      end
    end
    uv.nextHeartbeat = self.util.zulu(os.time() +  uv.sec_to_heartbeat)
    self:grant_set_rdb(uv)
    self:start_heartbeatbundle_timer(uv, uv.sec_to_heartbeat, grant)
  elseif responsecodenum == 102 and uv.rdata:match('measReport') then
    uv.measReportConfig = 'RECEIVED_POWER_WITH_GRANT'
    self:heartbeat(uv, nil, grant)
  elseif responsecodenum == self.INVALID_VALUE and uv.rdata:match('grantId') then
    relinquish(uv)
  else
    self:non_zero_response(responsecode, grant)
  end
end

function SasClient:relinquishRetry(grant)
  -- grant request has timed out
  -- retry
  self.l.log("LOG_WARNING", 'relinquish_retry '..self.util.fstr(grant))
  self:set_grant_state(grant, false, 'relinquishment')
  self:start_std_timeout('relinquish_retry', grant)
  self:schedule_action('relinquishment', grant)
end

-- relinquish the specified grant
function  SasClient:relinquishment(user_tab, dtab, grant)
  if self:has_schedule('relinquishment', grant) then
    self.l.log("LOG_NOTICE", string.format("relinquishment request for %s already scheduled", grant))
    return
  end
  self:stop_all_grant_timeouts(grant)
  self:remove_schedule(grant)
  self.sas:set_values(self.ctx, user_tab, grant)
  if dtab then self.sas:set_values_nil(self.ctx, dtab, grant) end
  if self.ctx.state_machine.grants[grant].operationState == 'WAIT-RETRY' then
    self:remove_grant(grant)
    return
  end
  self:start_std_timeout('relinquish_retry', grant)
  self:set_grant_state(grant, 'relinquishment', 'relinquishment')
  self:schedule_action('relinquishment', grant)
end

-- wait before relinquishing an existing grant for a frequency that is no longer in the SIB5 neighbor list.
function SasClient:grantRelinquishWaitTime(grant)
  local utab, dtab = self:user_values('relinquishment',
        {
          ['grantId'] = grant,
          ['cbsdId']  = self.rdb.get('sas.cbsdid'),
        })
  local _, uv = self:grant_table(grant, false)
  if uv and uv.grantRequiredFor == 'Legacy' then
    self:relinquishment(utab, dtab, grant)
  end
end

function SasClient:relinquishmentResponse(responsecode)
  local grant = self.ctx.grant
  responsecode = self:request_complete(responsecode)
  self:stop_timeout('relinquish_retry', grant)
  self.l.log("LOG_DEBUG", 'relinquish '..self.util.fstr(grant))
  if tonumber(responsecode) == 103 then
      local rdata = self.ctx.current_response.responseData and self.ctx.current_response.responseData[1] or ''
      if rdata:match('cbsdId') then
          self.l.log("LOG_ERR", string.format('relinquishmentResponse error [%s], setting state Unregistered now', rdata))
          self:non_zero_response(responsecode) return
      end
  end

  if grant and self.ctx.state_machine.grants[grant] then
    local post_action = self.ctx.state_machine.grants[grant].post_action
    self:remove_grant(grant)
    if post_action == 'renew_on_nogrant' then
      if next(self.ctx.state_machine.grants) == nil then
        local utab, dtab = self:user_values('spectrumInquiry')
        self:spectrumInquiry(utab, dtab)
      end
    elseif post_action == 'pCellChanged' then
      self:pCellChanged()
    end
  end

  self:schedule_action(nil, nil)
end

-- the timeout tables are all closures, so the grant id entry must also
-- be changed
function SasClient:rename_grant(old_name, new_name)
  local gv, uv = self:grant_table(old_name)
  if gv then
    self.ctx.grants[new_name] = gv
    self.ctx.grants[old_name] = nil
    for _,v in pairs(gv.timeouts) do
      v.grant = new_name
    end
  end
  if uv then
    self.ctx.state_machine.grants[new_name] = uv
    self.ctx.state_machine.grants[old_name] = nil
  end
end

function SasClient:grantRetry(grant, id)
  -- grant request has timed out
  -- retry
  self.l.log("LOG_WARNING", 'grant_retry '..self.util.fstr(id))
  self:set_grant_state(grant, false, 'grant')
  self:start_std_timeout('grant_retry', nil, id)
  self:schedule_action('grant', nil, id)
end

function SasClient:grant(user_tab)
  -- create a unique id and store request data in self.ctx.queued_req_tabs[id]
  -- to avoid request values overwritten by other grant requests
  local id = string.format("%s:%s:%s:%s",
    user_tab.lowFrequency, user_tab.highFrequency, user_tab.maxEirp, user_tab.grantRequiredFor)
  if self:has_schedule('grant', nil, id) then
    self.l.log("LOG_NOTICE", string.format("grant request for %s already scheduled", id))
    return
  end
  self.ctx.queued_req_tabs[id] = {['id'] = id}
  self.util.shallow_copy(self.ctx.queued_req_tabs[id], user_tab)
  self:start_std_timeout('grant_retry', nil, id)
  self:set_state(false, 'grant')
  self:schedule_action('grant', nil, id)
end

-- set data for Installation Assistant UI
-- use sas.grant.0.* to communicate SAS status to IA
function SasClient:informIA()
  local set_ui_data = function(g)
    self.rdb.set('sas.grant.0.request_error_code', '')
    self.rdb.set('sas.grant.0.request_error_message', '')
    self.rdb.set('sas.grant.0.reason', g.responsecode or '')
    self.rdb.set('sas.grant.0.response_message', g.rmesg or '')
    self.rdb.set('sas.grant.0.response_data', g.rdata or '')
    self.rdb.set('sas.grant.0.state', g.operationState or '')
  end

  local grants = self:serving_cell_grants()
  local grant = {}
  -- only report first grant with non-zero response code
  for _,g in pairs(grants) do
    if g.responsecode ~= 0 then set_ui_data(g) return end
    grant = g
  end
  set_ui_data(grant)
end

-- the grant request could be an attempt to renew an expired grant,
-- which is likely to have a new name
function SasClient:grantResponse(responsecode)
  local grant = self.ctx.grant
  responsecode = self:request_complete(responsecode)
  local responseCode = tonumber(responsecode)
  local user_tab = {}

  local req = self.util.shallow_copy(nil, self.ctx.req_tab)
  local invars = self.ctx.current_response
  local rdata = invars.responseData and invars.responseData[1] or ''

  if responseCode == 0 or responseCode == self.SUSPENDED_GRANT then
    self.util.copy_only_vars(user_tab, self.sas:requested_values(self.ctx, grant), saver_vals)
    self.util.copy_only_vars(user_tab, self.ctx.current_response, saver_vals)
    self.util.shallow_copy(user_tab, self:user_values('heartbeat', self.ctx.current_response))
    if not user_tab['grantId'] then
      -- nothing will work
      -- wait for timeout retry
      return
    end

    if grant and user_tab['grantId'] and grant ~= user_tab['grantId'] then
      self:remove_grant_rdb(grant)
      self:rename_grant(grant,  user_tab['grantId'])
    end
    grant = user_tab['grantId']
    local _, uv = self:grant_table(grant, true)
    self.util.shallow_copy(uv, user_tab)
    uv.measReportConfig = self.ctx.current_response.measReportConfig and self.ctx.current_response.measReportConfig[1]
    -- self.util.copy_only_different(uv, self.ctx.current_response, user_tab)
    uv.operationState = 'GRANTED'
    self.sas:set_values(self.ctx, uv, grant)
    self.cbrs_grants[uv.index] = uv
    self:grant_set_rdb(uv)
    self.measReported = true -- SAS has granted a grant, no need to send more measReport
  elseif self.ctx.req_tab.index then
    -- set data for UI
    local g = self.cbrs_grants[self.ctx.req_tab.index] or {}
    g.responsecode = responseCode
    g.rmesg = invars.responseMessage or ''
    g.rdata = rdata
  end

  self:stop_timeout('grant_retry', self.ctx.grant, self.ctx.req_tab.id)

  -- 'self.ctx.req_tab.id' is set only when sending grant request.
  -- When, an existing grant expires, the 'id' is not set while sending the grant renew request.
  -- Accessing a table with a nil index leads to an exception, aborting the function execution.
  if self.ctx.req_tab.id then
    self.ctx.queued_req_tabs[self.ctx.req_tab.id] = nil
    self.ctx.req_tab.id = nil
    req.id = nil
  end

  self:informIA()

  if responseCode == 0 or responseCode == self.SUSPENDED_GRANT then
    -- process intra cells now as we might have ignored them due to no serving grant
    self:intra_cells_changed(self.intra_cells_rdb, self.rdb.get(self.intra_cells_rdb))
    self.util.shallow_copy(user_tab, uv)
    self:heartbeat(uv, nil, grant)
  elseif responseCode == self.INTERFERENCE then
    self.l.log("LOG_NOTICE", string.format('INTERFERENCE SAS[%s,%s|%s] OWA[%s,%s|%s] minEirp_perMHz:%s',
      invars.lowFrequency and tonumber(invars.lowFrequency)/self.MEGA or "_",
      invars.highFrequency and tonumber(invars.highFrequency)/self.MEGA or "_", invars.maxEirp,
      req.lowFrequency/self.MEGA, req.highFrequency/self.MEGA, req.maxEirp, self.minEirp_perMHz))

    self.l.log("LOG_NOTICE", string.format('serving cell[pci:%s,earfcn:%s] PCI lock[%s] neighbour[%s]',
      self.rdb.get(self.serv_pci_rdb),
      self.rdb.get(self.pEarfcnKey), self.rdb.get(self.lock_list_rdb),
      self.rdb.get(self.cellListKey)))

    self.cbrs_grants[self.ctx.req_tab.index] = nil
    -- check if SAS responded frequency range matches with request
    if (not invars.lowFrequency or invars.lowFrequency ~= req.lowFrequency) or
       (not invars.highFrequency or invars.highFrequency ~= req.highFrequency) then
      if req.grantRequiredFor == 'ServingCell' then
        -- <OWA-1100>, move to new cell
        if self.nit_connected then
          self:change_grant_request_lock() return
        end
        -- normal case relying on PCI lock to move to next neighbour cell
      else
        --<OWA-1250>, <OWA-1270> don't request grant for same frequency grant until timeout
        local utab = self:grant_retry_wait_time()
        self:grantRetryWaitTimerStart(utab, utab.grantId)
      end
    -- check if maxEirp less than the minimum acceptable transmit power
    -- NOTE: The 'minimum acceptable transmit power' is determined by the OWA
    -- based on available RF data; it is not a static or configured value.
    elseif invars.maxEirp >= self.minEirp_perMHz and tonumber(req.maxEirp) ~= invars.maxEirp then
      req.maxEirp = invars.maxEirp    --<OWA-1300> re-try with new maxEirp value
      self.cbrs_grants[self.ctx.req_tab.index] = req
      self:grant(req) return
    elseif req.grantRequiredFor == 'ServingCell' then
      --<OWA-1200> installation case: move to next cell by remove current serving cell from the install list
      if self.nit_connected then
        self:change_grant_request_lock() return
      end
      -- normal case relying on PCI lock to move to next neighbour cell
    else
      local utab = self:grant_retry_wait_time() --<OWA-1270>
      self:grantRetryWaitTimerStart(utab, utab.grantId)
    end
  elseif responseCode == self.GRANT_CONFLICT then
    self.cbrs_grants[self.ctx.req_tab.index] = nil
    if not invars.responseData then invars.responseData = {} end
    for _,id in ipairs(invars.responseData) do
      local _, uv = self:grant_table(id, true)
      self.l.log("LOG_ERR", string.format('responseCode 401, relinquish %sexisting conflicting grant:%s, requiredFor:%s',
        uv.grantId and '' or 'non-', id, req.grantRequiredFor))
      uv.operationState = 'REMOVED'
      if uv.grantId then
        -- update rdb if we have grant context
        self:grant_set_rdb(uv)
      else
        -- no context, grant table will only have state & id info to facilitate request sending & retry.
        uv.grantId = id
      end
      local utab, dtab = self:user_values('relinquishment', {['grantId'] = id, ['cbsdId'] = invars.cbdsdId })
      self:relinquishment(utab, dtab, id)
    end
  elseif responseCode == self.UNSUPPORTED_SPECTRUM then
    -- retry won't work, wait for cell change event
    self.cbrs_grants[self.ctx.req_tab.index] = nil
  else
    -- log any un-expected responseCode
    self.l.log("LOG_ERR", string.format("Unexpected grant responseCode:%d, data[%s]", responseCode, rdata))
    if rdata:match("measReport") then
      self.measReported = false
      self.ctx.state_machine.user_values.measReportConfig = {'RECEIVED_POWER_WITHOUT_GRANT'}
      self.l.log("LOG_NOTICE", 'next request will include measReport')
    end
    self:non_zero_response(responsecode, grant)
    self.rateLimits = {}  -- to force re-evaluate of neighbour grants on next rdb changed event
    self.cbrs_grants[self.ctx.req_tab.index] = nil
  end

  self:schedule_action()
end

-- start grant retry wait timer
function SasClient:grantRetryWaitTimerStart(utab, id)
  local timeout = self.util.parse_json_date(utab.grantExpireTime) - os.time()
  timeout = (timeout < 0) and 30 or timeout
  self.l.log("LOG_NOTICE", 'retry-wait-timer for id:'..id..', expires in '..timeout..' seconds')
  local onExpired = function(self, id) self:grantRetryWaitTimerExpired(id) end
  self:start_timeout(timeout * 1000, onExpired, 'grant_retry_wait_time', id)
  self.cbrs_grants[utab.index] = utab
end

-- grant retry wait time is over, send grant request now
function SasClient:grantRetryWaitTimerExpired(id)
  local exist, uv = self:grant_table(id, false)
  if not exist then
    self.l.log("LOG_ERR", 'id:'..id..' lost at grant wait timer expiry') return
  end
  self:remove_grant(id)
  self:grant(uv)
end

function SasClient:spectrumInquiryRetry()
  -- spectrumInquiry has timed out
  -- retry
  self.l.log("LOG_WARNING", 'spectrumInquiry_retry')
  self:set_state(false, 'spectrumInquiry')
  self:start_std_timeout('spectrumInquiry_retry')
  self:schedule_action('spectrumInquiry', nil)
end

function SasClient:spectrumInquiry(user_tab, dtab)
  self.measReported = false -- measReport required in spectrum Inquiry or first grant request
  local spectrumInquiryEnabled = tonumber(self.rdb.get('sas.spectrum_inquiry_enabled') or 0)
  if spectrumInquiryEnabled ~= 1 then
    self:pCellChanged()
    return
  end
  self.sas:set_values(self.ctx, user_tab)
  if dtab then self.sas:set_values_nil(self.ctx, dtab) end
  self:set_state(nil, 'spectrumInquiry')
  self:start_std_timeout('spectrumInquiry_retry')
  self:schedule_action('spectrumInquiry', nil)
end

function SasClient:spectrumInquiryResponse(responsecode)
  responsecode = self:request_complete(responsecode)
  if tonumber(responsecode) ~= 0 then
    self:non_zero_response(responsecode)
    return
  end
  self:stop_timeout('spectrumInquiry_retry')
  local spectrum = self.ctx.res_body and self.ctx.res_body.spectrumInquiryResponse or nil
  self.sasChannel = spectrum and spectrum[1].availableChannel or nil
  self:pCellChanged()
  -- special indication to do next scheduled task
  self:schedule_action(nil, nil)
end

-- minimum eirp value between ODU default or from spectrumInquiryResponse if exists
function SasClient:getMaxEirp(uv)
  if self.sasChannel then
    for i=1,#self.sasChannel do
      local channel = self.sasChannel[i]
      if uv.lowFrequency == channel.frequencyRange.lowFrequency and
        uv.highFrequency == channel.frequencyRange.highFrequency then
        if channel.maxEirp and channel.maxEirp < self.maxEirp_perMHz then return channel.maxEirp end
        break
      end
    end
  end
  return self.maxEirp_perMHz
end

function SasClient:deregisterRetry()
  -- deregistration has timed out
  -- retry
  self.l.log("LOG_WARNING", 'deregister_retry')
  self:set_state(false, 'deregistration')
  self:start_std_timeout('deregister_retry')
  self:schedule_action('deregistration', nil)
end

function SasClient:deregistration(user_tab, dtab)
  self.sas:set_values(self.ctx, user_tab)
  if dtab then self.sas:set_values_nil(self.ctx, dtab) end
  self:set_state(false, 'deregistration')
  self:stop_all_timeouts() -- eg. avoid stray heartbeat after deregistration
  if self.ctx.scheduled and #self.ctx.scheduled >= 2 then
    self.l.log("LOG_NOTICE", 'deregistration() drop '..(#self.ctx.scheduled - 1)..' pending action(s)')
    for i=#self.ctx.scheduled,2,-1 do table.remove(self.ctx.scheduled,i) end
  end
  self:start_std_timeout('deregister_retry')
  self:schedule_action('deregistration', nil)
end

function SasClient:deregistrationResponse(responsecode)
  if tonumber(responsecode) ~= 0 then
     -- log the error then set CBSD to Unregistered according to CBRS standard
     self:logResponse('Deregistration has failed', responsecode)
  end
  self:deregister()
  self:request_complete(responsecode)
end

function SasClient:registerRetry()
  -- registration has timed out
  -- retry
  self.l.log("LOG_WARNING", 'register_retry')
  self:set_state(false, 'registration')
  self:start_std_timeout('register_retry')
  self:schedule_action('registration', nil)
end

function SasClient:registration(user_tab, dtab)
  self.rdb.set(self.registration_state_rdb, 'Registering')
  self:remove_all_grants()
  if user_tab then self.sas:set_values(self.ctx, user_tab) end
  if dtab then self.sas:set_values_nil(self.ctx, dtab) end
  self.l.log("LOG_NOTICE", 'Registration request')
  self:set_state(false, 'registration')
  self:start_std_timeout('register_retry')
  self:schedule_action('registration', nil)
end

-- setup a timer to retry multi-step registration, disconnect modem to preserve TX time
function SasClient:setRegPendingRetryTimer(retry_sec)
    if self.ssm.get_current_state() ~= "sas:idle" then
      os.execute("qmisys detach")
    end
    local event = function()
      self.ms_reg_retry_timer = nil
      local rstate = self.rdb.get(self.registration_state_rdb)
      local action = self.rdb.get(self.registrationCmdKey)
      if rstate ~= "Registered" and rstate ~= "Registering" and action == "" then
        self.rdb.set(self.registrationCmdKey, "register")
      else
        self.l.log("LOG_NOTICE",
          string.format("stop multi-step auto-retry, nit_connected:%s, rstate:%s, action:%s",
            self.nit_connected, rstate, action))
      end
    end
    if not retry_sec then retry_sec = tonumber(self.rdb.get("sas.config.regPendingRetry")) or 300 end
    local timeout = retry_sec * 1000 + self.t.util.gettimemonotonic()
    self.ms_reg_retry_timer=self.i:add_timeout(timeout, event)
    self.l.log("LOG_NOTICE", string.format("retry register again in %s sec, timer:%s", retry_sec, self.ms_reg_retry_timer))
end

function SasClient:registrationResponse(responsecode)
  responsecode = self:request_complete(responsecode)
  self:stop_timeout('register_retry')

  self.rdb.set('sas.registration.response_code', self.ctx.current_response.responseCode or '')
  self.rdb.set('sas.registration.response_message', self.ctx.current_response.responseMessage or '')
  self.rdb.set('sas.registration.response_data', self.ctx.current_response.responseData and self.ctx.current_response.responseData[1] or '')
  if tonumber(responsecode) == 0 then
    self.l.log("LOG_NOTICE", "Registered!")

    self.util.copy_only_vars(self.ctx.state_machine.user_values, self.ctx.current_response, {'cbsdId', 'measReportConfig'})
    self.ctx.state_machine.user_values['operationState'] = "AUTHORIZED"
    local user_tab = { ['cbsdId'] = self.ctx.current_response['cbsdId'] }
    self.sas:set_values(self.ctx, user_tab)

    self:set_state('Registered', nil)
    self.rdb.set(self.registration_state_rdb, 'Registered')
    self:registration_to_rdb(self.ctx.current_response)
    self:spectrumInquiry(self:user_values('spectrumInquiry', nil))
    if self.rdb.get("sas.config.fullspectrum") == "1" then
      -- request all grants for full CBRS spectrum
      self:setup_grants({reqFor='PRE-ORDER', freq={lo_Mhz=self.CBRS_START, hi_Mhz=self.CBRS_END}})
    end
    self.rateLimits = {}
  else
    self.rdb.set(self.registration_state_rdb, 'Unregistered')
    self:set_state('Unregistered', nil)
    self:logResponse('Registration has failed', responsecode)
  end

  if tonumber(responsecode) == self.REG_PENDING then self:setRegPendingRetryTimer() end
end

-- A sever error has occurred, e.g. server not present, bad certificate,
-- request can't be handled
-- The strategy here is to let the retry and other timeouts do their work
-- Maybe it's a temporary condition and the retry will succeed
function SasClient:errorResponse(request, error_code, status_code, error_message)
  self:request_complete('')
  self.l.log("LOG_ERR", string.format('Error called. Request:%s, error code:%s, status code:%s, error messages:%s', request, error_code, status_code,  error_message))
  self.ctx.current_response = {}
  local set_IA_ecode = function(rdb_var, e_code)
    local attached = tonumber(self.rdb.get(self.attach_rdb)) or 0
    if attached == 0 and self.attach_rejected then
      e_code = '-99'
    end
    self.rdb.set(rdb_var, e_code)
  end

  local swfun = {
    ['registration'] = function()
      self.l.log("LOG_ERR", 'Registration has failed, wait for auto retry')
      -- 'register_retry' will happen

      -- inform IA UI
      set_IA_ecode('sas.registration.request_error_code', self.ctx.res_tab['error_code'] or '')
      set_IA_ecode('sas.registration.request_error_message', self.ctx.res_tab['error_message'] or '')
    end,
    ['spectrumInquiry'] = function()
      self.l.log("LOG_ERR", 'spectrumInquiry has failed, wait for auto retry')
      -- 'spectrumInquiry_retry' will happen
    end,
    ['grant'] = function()
      self.l.log("LOG_ERR", 'grant has failed, wait for auto retry')
      -- 'grant_retry' will happen

      -- inform IA UI
      if self.ctx.req_tab.grantRequiredFor == 'ServingCell' then
        set_IA_ecode('sas.grant.0.request_error_code', self.ctx.res_tab['error_code'] or '')
      end
    end,
    ['heartbeat'] = function()
      self.l.log("LOG_ERR", 'heartbeat has failed, wait for auto retry')
      -- 'heartbeat_retry' will happen

      -- inform IA UI
      if self.ctx.req_tab.grantRequiredFor == 'ServingCell' then
        set_IA_ecode('sas.grant.0.request_error_code', self.ctx.res_tab['error_code'] or '')
        set_IA_ecode('sas.grant.0.request_error_message', self.ctx.res_tab['error_message'] or '')
      end
    end,
    ['deregistration'] = function()
      if self.ctx.req_tab.force_deregister == true then
        self.l.log("LOG_ERR", 'deregistration has failed, force deregister')
        responsecode = self:request_complete(responsecode)
        self:deregister()
      else
        self.l.log("LOG_ERR", 'deregistration has failed, wait for auto retry')
        -- 'deregisterRetry' will happen
      end
    end,
  }
  if swfun[request] then
    swfun[request]()
  end
  self:schedule_action(nil, nil)
end

function SasClient:restart()
  -- copy user values and setup grant tables
  self.sas:set_values(self.ctx, self.ctx.state_machine.user_values)
  local utab, dtab = self:user_values('registration', nil)
  if dtab then self.sas:set_values_nil(self.ctx, dtab) end
  self.sas:set_values(self.ctx, utab)
  if self.ctx.state_machine.grants then
    for k,_ in pairs(self.ctx.state_machine.grants) do
      local _, uv = self:grant_table(k, true)
      if uv then uv['can_xmit'] = false end
      self.sas:set_values(self.ctx, uv, k)
    end
  end

  -- reset all previous error state that may have been informed to IA UI
  self.rdb.set('sas.registration.request_error_code', '')
  self.rdb.set('sas.registration.request_error_message', '')
  self.rdb.set('sas.registration.response_code', '')
  self.rdb.set('sas.registration.response_message', '')
  self.rdb.set('sas.registration.response_data', '')
  self:informIA()
end

-- resume the timer to reliquish a legacy grant
function SasClient:resumeLegacyGrantRelinquishTimer(v)
  local name = 'grant_relinquish_wait_time'
  local reqlinquishTime = (self.timeout_values[name][1]/1000) - (os.time() - self.util.parse_json_date(v.legacyTime))
  if reqlinquishTime <= 0 then reqlinquishTime = 30 end -- ensure schedule in future
  self:start_timeout(reqlinquishTime*1000, self.timeout_values[name][2], name, v.grantId)
end

function SasClient:resumeHeartbeat()
  -- if already registered, check grant and send heartbeat
  local rstate = self.ctx.state_machine.rstate
  if rstate == 'Registered' and self.ctx.state_machine.grants then
    local serving_cell_grant, scell_uv = nil, nil
    for k,v in pairs(self.ctx.state_machine.grants) do
      if v.operationState == 'GRANTED' or v.operationState == 'AUTHORIZED' then
        if v.operationState == 'AUTHORIZED' and not v.transmitExpireTime then
          -- transmitExpireTime is not saved into RDB, derive from nextHeartbeat as 10s later
          v.tx_expire_sec = self:seconds_to_event(v.nextHeartbeat) + 10
          v.g_expire_sec = self:seconds_to_event(v.grantExpireTime)
          if v.tx_expire_sec < v.g_expire_sec then
            v.transmitExpireTime = self.util.zulu(os.time() +  v.tx_expire_sec)
          end
          self.l.log("LOG_NOTICE", string.format("g:%s tx_expire_sec:%s g_expire_sec:%s txExpire:%s",
            k, v.tx_expire_sec, v.g_expire_sec, v.transmitExpireTime))
        end
        self:sas_timers(k)
        self:start_heartbeatbundle_timer(v, 0, k)
        if v.grantRequiredFor == 'Legacy' then self:resumeLegacyGrantRelinquishTimer(v) end
      elseif v.operationState == 'WAIT-RETRY' then
        self:grantRetryWaitTimerStart(v, k)
      elseif v.operationState == 'REMOVED' then
        -- sas client restarted with grants that are not completely relinquished, resume relinquishing
        self:relinquish_grant(v, 'renew_on_nogrant')
      else
        self.l.log("LOG_ERR", 'SasClient:restart() grant:'..k..', unknown state:'..v.operationState)
      end
      if v.grantRequiredFor == 'ServingCell' then serving_cell_grant, scell_uv = k, v end
    end
    self:onGrantStateChange(serving_cell_grant, scell_uv)
  end
  self:schedule_action(nil, nil)
end

-- debug helper swap response code if set
function SasClient:swap_responsecode(grant, action, responsecode)
 if self.next_should_fail.active == true then
    local matched_grant  = not self.next_should_fail.grant  and true or self.next_should_fail.grant  == grant
    local matched_action = not self.next_should_fail.action and true or self.next_should_fail.action == action
    if matched_action and matched_grant then
      self.l.log("LOG_WARNING", 'TEST: swap responseCode:'..responsecode..' with '..self.next_should_fail.code)
      responsecode = self.next_should_fail.code
      if not self.next_should_fail.persist then self.next_should_fail = {} end
    end
  end
  return responsecode
end

-- previously requested http request is complete
-- next can progress
function SasClient:request_complete(responsecode)
  table.remove(self.ctx.scheduled, 1)
  responsecode = self:swap_responsecode(self.ctx.grant, self.ctx.action, responsecode)
  self.ctx.grant = nil
  self.ctx.action = 'wait_user'
  self.ctx.bundle = nil
  return responsecode
end

function SasClient:unknownResponse(str)
  self:request_complete(str)
  local rs = self.ctx.state_machine.rstate or ''
  local gs = self.ctx.state_machine.gstate or ''
  local ac = self.ctx.state_machine.action or ''
  self.l.log("LOG_WARNING", "state is unknown rstate: "..  rs ..' gstate[1]: '.. gs ..' action: '.. ac)
end

-- We start in the unregistered state, and do a registration.
function SasClient:state_machine_unregistered(old_stat,new_stat,stat_chg_info)
  self.ssm.switch_state_machine_to("sas:unregistered", 10000)
  self.l.log("LOG_NOTICE", 'unregistered()')
end

-- After registration, we move to idle, we do not have any grants yet
function SasClient:state_machine_registered(old_stat,new_stat,stat_chg_info)
  self.ssm.switch_state_machine_to("sas:registered", 10000)
  self.l.log("LOG_NOTICE", 'registered()')
end

-- we will request for grant on the serving cell frequency
function SasClient:state_machine_no_grant(old_stat,new_stat,stat_chg_info)
  self.ssm.switch_state_machine_to("sas:no_grant", 10000)
  self.l.log("LOG_NOTICE", 'no_grant()')
end

-- we have the grant, now we wait for authoriztion
function SasClient:state_machine_granted(old_stat,new_stat,stat_chg_info)
  self.ssm.switch_state_machine_to("sas:granted", 10000)
  self.l.log("LOG_NOTICE", 'granted()')
end

-- we are authorized to transmit.
--  We can not enable wlldata and wllems
--  We can request grnats for neighbour cells.
--  We set cell lock accordingly
function SasClient:state_machine_authorized(old_stat,new_stat,stat_chg_info)
  self.ssm.switch_state_machine_to("sas:authorized", 10000)
  self.l.log("LOG_NOTICE", 'authorized()')
end

-- Special state: There are grnats, but none is authorized.
function SasClient:state_machine_suspended(old_stat,new_stat,stat_chg_info)
  self.ssm.switch_state_machine_to("sas:suspended", 10000)
  self.l.log("LOG_NOTICE", 'suspended()')
end

-- we are not attached, and waiting for attach
function SasClient:state_machine_attach(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_NOTICE", 'state_machine_attach(): SAS client waiting for network attach')

  local attached = tonumber(self.rdb.get(self.attach_rdb)) or 0
  if attached == 1 then
    self.l.log("LOG_NOTICE", 'state_machine_attach(): OWA attached')
    self.ssm.switch_state_machine_to("sas:wait_ip_interface")
    return
  end

  self.ssm.switch_state_machine_to("sas:attach", 10000)
  local tx_enable = tonumber(self.rdb.get(self.tx_state_rdb)) or 0
  if tx_enable == 0 then
    self.l.log("LOG_NOTICE", 'state_machine_attach(): waiting for the permission from the tx control, will try later')
    return
  end

  if self.pci_lock_pending then
    self.l.log("LOG_NOTICE", 'state_machine_attach(): waiting for pci_lock_pending to clear')
    self.rdb.set("service.luaqmi.command", "setCAmode,0")
    return
  end

  -- note the current attach attempt & failuare
  local attach_attempt = tonumber(self.rdb.get("wwan.0.pdpcontext.attach_attempts")) or 0
  local attach_failure = tonumber(self.rdb.get("wwan.0.pdpcontext.attach_failures")) or 0

  if not self.attach_failure then self.attach_failure = attach_failure end
  if self.attach_failure ~= attach_failure then
    self.attach_failure = attach_failure
    self.attach_rejected = true -- this flag will clear when latter attach is successful
    self.l.log("LOG_NOTICE", string.format("Attach rejected, attempt:%s, failure:%s", attach_attempt, attach_failure))
  end

  self.l.log("LOG_NOTICE", string.format("state_machine_attach(): calling qmisys attach (attempt:%s, failure:%s)", attach_attempt, attach_failure))
  os.execute("qmisys attach")
  local reg_state = self.rdb.get(self.registration_state_rdb) or ''
  if reg_state == 'Registered' then return end

  -- inform IA if it orders registration and network has problem
  local cmd = self.rdb.get(self.registrationCmdKey) or ''
  local new_reg_state = cmd == 'register' and "Attaching" or ''
  local e_code = (cmd == 'register' and self.attach_rejected) and '-99' or ''
  if new_reg_state ~= reg_state then self.rdb.set(self.registration_state_rdb, new_reg_state) end
  local reg_e_code_rdb = "sas.registration.request_error_code"
  if self.rdb.get(reg_e_code_rdb) ~= e_code then self.rdb.set(reg_e_code_rdb, e_code) end
end

-- attached, now wait for IPv4 ready
function SasClient:state_machine_wait_ip_interface(old_stat,new_stat,stat_chg_info)
  local ip_state = self.rdb.get(self.ip_state_rdb)
  local cbrs_up_config_stage = self.rdb.get("sas.cbrs_up_config_stage")
  self.wait_cbrs_up_config_count = self.wait_cbrs_up_config_count + 1
  -- switch to post_attach only when ip_state == "1" AND either CBRS networking config is done or already wait for 5 minutes
  if ip_state == '1' and (cbrs_up_config_stage == "2" or self.wait_cbrs_up_config_count > 6*5) then
    if cbrs_up_config_stage ~= "2" then
      self.l.log("LOG_NOTICE", "CBRS networking config has not done, force switching to post_attach")
    end
    self.wait_cbrs_up_config_count = 0
    self.cell_lock_list_retry_cnt = 0
    self.ssm.switch_state_machine_to("sas:post_attach")
  else
    self.ssm.switch_state_machine_to("sas:wait_ip_interface", 10000)
  end
end

function SasClient:state_machine_post_attach(old_stat,new_stat,stat_chg_info)
  local reg_state = self.rdb.get(self.registration_state_rdb) or ''
  self.l.log("LOG_DEBUG", string.format('post_attach(): nit_connected:%s reg_state:%s', self.nit_connected, reg_state))
  if self.nit_connected and reg_state ~= "Registered" then
    local lock_list = self:prepare_install_cell_lock_list()
    if not lock_list then
      self.l.log("LOG_DEBUG", 'post_attach(): switching to post_attach')
      self.ssm.switch_state_machine_to("sas:post_attach", 1000)
      return
    end
  end
  self.cell_lock_list_retry_cnt = 0
  self.ssm.switch_state_machine_to("sas:operate")
end

function SasClient:state_machine_idle(old_stat,new_stat,stat_chg_info)
  local band = self.rdb.get("wwan.0.currentband.config")
  if band ~= "LTE Band 48 - TDD 3600" then
    self.ssm.switch_state_machine_to("sas:idle", 30000)
    return
  end
  self:prepare_install_cell_lock_list()

  local attached = tonumber(self.rdb.get(self.attach_rdb)) or 0
  local tx_enable = tonumber(self.rdb.get(self.tx_state_rdb)) or 0
  local reg_state = self.rdb.get(self.registration_state_rdb)
  if reg_state ~= "Registered" and reg_state ~= "Registering" then
    reg_state = ''
    self.rdb.set(self.registration_state_rdb, reg_state)
  end

  -- start registration retry timer if device is rebooted during REG_PENDING
  local is_reg_pending = tonumber(self.rdb.get("sas.registration.response_code")) == self.REG_PENDING
  if is_reg_pending and not self.ms_reg_retry_timer then self:setRegPendingRetryTimer(30) end

  if attached == 1 or reg_state == "Registered" or (reg_state == "Registering" and tx_enable == 1) then
    self.l.log("LOG_NOTICE",
      string.format('idle(): switching to attach, attached:%s reg_state:%s', attached, reg_state))
    self.cell_lock_list_retry_cnt = 0
    self.ssm.switch_state_machine_to("sas:attach")
    return
  end

  self.ssm.switch_state_machine_to("sas:idle", 10000)
  if old_stat ~= new_stat then self.l.log("LOG_NOTICE", 'idle(): SAS client waiting for network attach') end
end

function SasClient:state_machine_initiate(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_NOTICE", "sas initiate")
  --[[
    Check default profile, if not CBRS, we need to set it to CBRS, and reboot if required.
    Now we cylce through register, spectrum inquiry, grant and relinquish and the http states
  --]]
end

function SasClient:state_machine_operate(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_NOTICE", "state machine in operate()")
  self:start_sas_client()
end

-- Do cleanup here, this should be called when we want to exit the sas client
function SasClient:state_machine_finalise(old_stat,new_stat,stat_chg_info)
  self:sas_stop_services()
  self.ssm.switch_state_machine_to("sas:finalise")
end

-- Start or resume sas client
function SasClient:start_sas_client()
  if not self.sas_started then
    -- TODO move to init(), after functions moved into sas_client.
    self:sas_start_services()
    self.rdb.set("sas.run", '1')
    self.sas_started = true
    self.l.log("LOG_NOTICE", 'start_sas_client(): starting for first time')
  else
    self.l.log("LOG_NOTICE", 'start_sas_client(): resuming from last state')
    self.ctx.client = nil
  end

  self:stop_all_timeouts()
  self.ctx.scheduled = {}
  self.ctx.state_machine = {}
  self.ctx.grant = nil
  self.ctx.action = 'wait_user'
  self.cbrs_grants = {}

  -- Initialized the modem tx power according to the rdb config
  self:setModemTxPower()

  self.watcher.invoke("sas", "start_machine", {['ctx']=self.ctx, ['saved_state']=self:state_vars()})

  self.rdb.set(self.registration_state_rdb, self.ctx.state_machine.rstate)

  -- save the registration and grants
  -- check how many grants are needed
  if self.ctx.user_values and self.ctx.user_values['cbsdId'] then
    self:registration_to_rdb(self.ctx.user_values)
  end
  self:restart()
  self:pCellChanged()
  self:cell_list_changed(self.cellListKey, self.rdb.get(self.cellListKey) or '')
  self:intra_cells_changed(self.intra_cells_rdb, self.rdb.get(self.intra_cells_rdb))
  self:resumeHeartbeat()

  local reg_state = self.rdb.get(self.registration_state_rdb)
  if reg_state == "Registering" then
    self.rdb.set(self.registrationCmdKey, "register")
  elseif reg_state ~= "Registered" then
    self.rdb.set(self.registrationCmdKey, self.rdb.get(self.registrationCmdKey))
  end
end

    -- TODO stop sas timers, operations. we will resume when we attach
function SasClient:stop_sas_client()
end

-- TODO add state based checking if response type is valid for that state
-- add any other checks required to ensure the validity of the message
-- handle sas responses
function SasClient:on_sas_response(type, event, a)
  local state = self.ssm.get_current_state()
  self.l.log("LOG_NOTICE", string.format("on_sas_response(), state:%s, msg:%s", state, a.r))

  if     a.r == 'heartbeat'       then self:heartbeatResponse(a.rc)
  elseif a.r == 'relinquishment'  then self:relinquishmentResponse(a.rc)
  elseif a.r == 'grant'           then self:grantResponse(a.rc)
  elseif a.r == 'spectrumInquiry' then self:spectrumInquiryResponse(a.rc)
  elseif a.r == 'deregistration'  then self:deregistrationResponse(a.rc)
  elseif a.r == 'registration'    then self:registrationResponse(a.rc)
  elseif a.r == 'error'           then self:errorResponse(a.req, a.ec, a.sc, a.em)
  else self:unknownResponse(a.req, a.ec, a.sc) end
  return true
end

function SasClient:sas_register()
  if self.ctx.state_machine.rstate ~= 'Registered' then
    self.ctx.req_tab = {}
    self:restart() -- to refresh data from RDB
    self:registration(self.ctx.state_machine.user_values)
  else
    self.l.log("LOG_ERR", 'register command ignored for rstate='..tostring(self.ctx.state_machine.rstate))
  end
end

function SasClient:sas_deregister()
  local utab, dtab = self:user_values('deregistration',
       {['cbsdId']  = self.rdb.get('sas.cbsdid') or '', })
  self.l.log("LOG_NOTICE", 'deregister command received, rstate='..tostring(self.ctx.state_machine.rstate))
  if utab.cbsdId == '' then
    self.l.log("LOG_ERR", 'no sas.cbsdid, not able to send deregistrationRequest, reset local data only')
    self.ctx.current_response = {}
    self:deregister()
  else
    self:deregistration(utab, dtab)
  end
end

function SasClient:sas_fail_next(val)
  -- optional val, eg. 501:heartbeat:3570-3580-39,
  -- will simulate error 501 for heartbeat response with grant 3570-3580-39.
  local gtab = (val or ''):split(':')
  self.next_should_fail.code   = tonumber(gtab[1]) or 105
  self.next_should_fail.action = gtab[2]
  self.next_should_fail.grant  = (gtab[3] and gtab[3] ~= '') and gtab[3] or nil
  self.next_should_fail.persist= gtab[4] == 'p'
  self.next_should_fail.active = true
  self.l.log("LOG_NOTICE",
    string.format('next response for action:%s [grant:%s], should fail with response code:%s',
    self.next_should_fail.action or "any", self.next_should_fail.grant or "any", self.next_should_fail.code))
end

function SasClient:sas_grant(val)
  local state = self.ssm.get_current_state()
  if not self.ctx or state ~= 'sas:operate' then
    self.l.log("LOG_ERR", string.format("command: grant request ignored for state: %s", state))
    return
  end
  if not val or val:len() < 2 then
    return
  end
  local grant = val
  local gtab = grant:split('-')
  -- the command should be: lowfreq-highfreq-maxeirp-grantRequiredFor-index
  if #gtab < 3 then
    self.l.log("LOG_ERR", "Invalid parameters")
    return
  end
  local gmin = tonumber(gtab[1])
  local gmax = tonumber(gtab[2])
  local geirp = tonumber(gtab[3])
  -- if not specify the grantRequiredFor, the default set to 'Neighbor'
  local grantRequiredFor = gtab[4] or 'Neighbor'
  if not gmin or gmin < 200  or
    not gmax or gmax < 200  or
    gmin >= gmax or
    not geirp or geirp < 2 then
    return
  end
  -- check the eirp from the server, correct it if needed
  geirp = (geirp > self.minEirp_perMHz ) and geirp or self.minEirp_perMHz
  geirp = (geirp < self.maxEirp_perMHz ) and geirp or self.maxEirp_perMHz

  gmin = gmin * self.MEGA
  gmax = gmax * self.MEGA
  local utab = {
     ['lowFrequency'] = gmin,
     ['highFrequency'] = gmax,
     ['maxEirp'] = geirp,
     ['grantRequiredFor'] = grantRequiredFor,
     ['index']=tonumber(gtab[5]) or 20
  }
  self:grant(utab)
  self.cbrs_grants[utab.index] = utab
end

function SasClient:sas_relinquish(val)
  -- relinquish which grant
  if not val or val:len() < 2 then
    self.l.log("LOG_WARNING", "relinquish failed, paramter grantid is required")
    return
  end
  local _, g = self:grant_table(val, false)
  if not g then
    self.l.log("LOG_WARNING", "relinquish failed, grantid("..val..") is not existed")
    return
  end
  self:relinquish_grant(g, 'pCellChanged')
end

-- print internal data
function SasClient:print_sas_client_data(val)
  local function syslog_tprint(t, indent)
    if type(t) ~= 'table' then self.l.log("LOG_NOTICE", tostring(t)) return end
    for line in self.util.splitlines(self.util.tprint(t, indent)) do
      self.l.log("LOG_NOTICE", line)
    end
  end

  local function get_member(val)
    local v, p = val:split("."), self
    for i=1, #v do if p[v[i]] then p = p[v[i]] else p = nil break end end
    return p
  end

  -- default, print variables that most likely to help debugging
  if not val or val == '' then
    for _,v in pairs({ 'ctx.grants', 'ctx.state_machine', 'hbeatbundle', 'apnTimer'}) do
      self.l.log("LOG_NOTICE", string.format("%s:----------------------------", v))
      syslog_tprint(get_member(v))
    end return
  end

  -- any self member can be printed, eg: print ssl_opts.priv_pass
  self.l.log("LOG_NOTICE", string.format("print data for %s =", val))
  syslog_tprint(get_member(val))
end

-- print grant's pci list for debug purpose
-- usage example: rdb_set sas.command 'pci lock' to print the current lock list
function SasClient:print_pci(val)
  local show_pci = function(key)
    local cell = self.cells[key]
    if not cell then return end

    self.l.log("LOG_NOTICE", string.format("PCI list for cell:%s|%s:", key, cell.info.reqFor))
    local list = cell.info.pci_list:split('|') -- format: pci,timestamp|pci,timestamp|...
    local tab={}
    for i=1, #list do
      local info = list[i]:split(',')
      local pci, ts = tonumber(info[1]), tonumber(info[2])
      table.insert(tab, {['pci']=pci, ['ts']=ts})
    end
    table.sort(tab, function(a, b) return a.ts > b.ts end)
    for i=1, #tab do
      self.l.log("LOG_NOTICE", string.format("%s\t%s\t%s",
        tab[i].pci, tab[i].ts, self.util.zulu(tab[i].ts)))
    end
  end

  if not val or val == '' then
    -- no argument, print pci for all cells
    for key,_ in pairs(self.cells) do show_pci(key) end
  elseif val == "lock" then
    -- show PCI lock list
    local lock_list, logs = self:get_pci_lock_list()
    for _,v in ipairs(logs) do
      self.l.log("LOG_NOTICE", v)
    end
    -- print each pci individually to avoid syslog clipping long message
    local lock = lock_list:split(';')
    for i=1,#lock do self.l.log("LOG_NOTICE", string.format("%s", lock[i])) end
  else
    -- show PCI for specific cell
    show_pci(val)
  end
end

-- usage: rdb_set sas.command cell, dump cell info to syslog
function SasClient:cell_dump(val)
  self:print_sas_client_data('cells')
  self:cell_set_rdb()
end

-- Internal debug interface to manipulate sas client
function SasClient:command(key, val)
  if tonumber(val) ~= 0 then
    if type(val) ~= "string" or val:len() < 2 then
      return false
    end
    local tab = val:split(" ")
    local state = self.ssm.get_current_state()

    if tab[1] == 'fail' then
      self:sas_fail_next(tab[2])
    elseif tab[1] == 'register' then
      self:sas_register(state)
    elseif tab[1] == 'deregister' then
      self:sas_deregister(state)
    elseif tab[1] == 'spectrum' then
      self:spectrumInquiry(self:user_values('spectrumInquiry'))
    elseif tab[1] == 'grant' then
      self:sas_grant(tab[2])
    elseif tab[1] == 'relinquish' then
      self:sas_relinquish(tab[2])
    elseif tab[1] == 'print' then
      self:print_sas_client_data(tab[2])
    elseif tab[1] == 'pci' then
      self:print_pci(tab[2])
    elseif tab[1] == 'cell' then
      self:cell_dump(tab[2])
    else
      self.l.log("LOG_ERR", string.format("Invalid operation: %s in state: %s", tab[1], state))
    end
    self.rdb.set(key, '0')
  end
end

-- If we receive a suspended grant as the first grant, change the lock to
-- request grant on another cell
function SasClient:change_grant_request_lock()
  local lock_table = self.wutil.parse_lockmode(self.install_cell_list)

  if #lock_table > 0 then
    -- get current cell
    local pci = tonumber(self.rdb.get(self.serv_pci_rdb)) or -1
    local earfcn = tonumber(self.rdb.get(self.pEarfcnKey)) or -1
    self.l.log("LOG_NOTICE", string.format("change_grant_request_lock(): serving cell: %s, %s", pci, earfcn))

    -- remove current cell from install cell lock list
    for i=1,#lock_table do
      self.l.log("LOG_DEBUG", string.format("%d - pci:%s,earfcn:%s;", i, lock_table[i].pci, lock_table[i].earfcn))
      if lock_table[i].pci == pci and lock_table[i].earfcn == earfcn then
        self.l.log("LOG_NOTICE", string.format("change_grant_request_lock(): removing %s, %s", lock_table[i].pci, lock_table[i].earfcn))
        table.remove(lock_table, i)
        break
      end
    end
    self.install_cell_list = self.wutil.pack_lockmode(lock_table)
    self.l.log("LOG_NOTICE", string.format("change_grant_request_lock(): new install lock list - %s", self.install_cell_list))
    self:set_pcilock(self.install_cell_list)
    return true
  end
  return false
end

-- Prepare the cell lock list for attaching for first registration/grant
function SasClient:prepare_install_cell_lock_list()
  local seq = self.rdb.get("wwan.0.manual_cell_meas.seq")
  local state = self.ssm.get_current_state()

  if self.last_seq == seq then
    if self.cell_lock_list_retry_cnt < self.PCI_LOCK_LIST_MAX_RETRY then
      self.cell_lock_list_retry_cnt = self.cell_lock_list_retry_cnt + 1
      self.l.log("LOG_DEBUG", string.format("SAS:%s Retry count(%d): No new RF measurement updates received", state, self.cell_lock_list_retry_cnt))
      return false
    else
      self.l.log("LOG_DEBUG", string.format("SAS:%s Retry count exceeded! Continue with existing PCI cell lock list - %s", state, self.install_cell_list))
      self.cell_lock_list_retry_cnt = 0
      return true
    end
  end

  -- initialize last cell scan sequence number
  self.last_seq = seq
  local temp_table = {}     -- all cells
  local cell_table = {}
  local cell_table_lock = {}-- installer cells for locking sorted on rsrp, assuming the required cell has best rsrp
  local dup_table = {}      -- to avoid duplicate addition
  local cell_rsrp_tab = {}  -- cell measurement data, for rsrp sorting
  local cell_meas_data = {}

  -- get rsrp for the cells
  local qty = tonumber(self.rdb.get("wwan.0.manual_cell_meas.qty")) or 0
  for j=0,qty-1 do
    -- type,earfcn,pci,rsrp,rsrq
    local cell_data = self.rdb.get(string.format("wwan.0.manual_cell_meas.%s", j))
    self.l.log("LOG_DEBUG", string.format("manual_cell_meas cell data : %s", cell_data))
    local cell = cell_data:split(",")
    cell_rsrp_tab[cell[3]..cell[2]] = tonumber(cell[4])
    table.insert(cell_meas_data, cell)
  end

  local qty = tonumber(self.rdb.get("wwan.0.rrc_info.cell.qty")) or 0
  for j=0,qty-1 do
    -- mcc,mnc,earfcn,pci,cellId
    local cell_data = self.rdb.get(string.format("wwan.0.rrc_info.cell.%s", j))
    self.l.log("LOG_DEBUG", string.format("rrc_info cell data : %s", cell_data))
    local cell = cell_data:split(",")
    table.insert(temp_table, cell)
    dup_table[cell[4]..cell[3]] = true
  end

  for j=1,#temp_table do
    cell = temp_table[j]
    local plmn = cell[1]..cell[2]
    local cid = tonumber(cell[5])
    -- ecgi should be 15 digits
    local str = plmn.."%0"..(15 - #plmn).."d"
    local ecgi = string.format(str, cid)

    self.l.log("LOG_DEBUG", string.format("index %d: ecgi:%s, erfcn:%s, pci:%s", j, ecgi, cell[3], cell[4]))
    -- make sure it has not been added before, and has a rsrp value
    if dup_table[cell[4]..cell[3]] and cell_rsrp_tab[cell[4]..cell[3]] then
      -- pci, earfcn, rsrp, heading
      table.insert(cell_table, {cell[4], cell[3], cell_rsrp_tab[cell[4]..cell[3]], ""})
      dup_table[cell[4]..cell[3]] = false
    end
  end

  if not cell_table[1] then
    self.l.log("LOG_ERR", string.format("SAS:%s Unable to construct the PCI lock list!", state))
    local meas_tab = {}
    for k,v in pairs(cell_meas_data) do
      meas_tab[k] = table.concat(v, ",")
    end
    self.l.log("LOG_ERR", string.format("manual_cell_meas: %s", table.concat(meas_tab, "|")))

    local rrc_tab = {}
    for k,v in pairs(temp_table) do
      rrc_tab[k] = table.concat(v, ",")
    end
    self.l.log("LOG_ERR", string.format("rrc_info.cell: %s", table.concat(rrc_tab, "|")))
    return false
  end

  -- sort based on rsrp, max at top
  table.sort(cell_table, function(a,b) return a[3] > b[3] end)

  -- use the first cell as the reference cell and set lock for all cells with same heading
  table.insert(cell_table_lock, "pci:"..cell_table[1][1]..",earfcn:"..cell_table[1][2])
  for i=2,#cell_table do
    if cell_table[1][4] == cell_table[i][4] then
      table.insert(cell_table_lock, "pci:"..cell_table[i][1]..",earfcn:"..cell_table[i][2])
    end
  end

  self.install_cell_list = table.concat(cell_table_lock, ";")
  self.l.log("LOG_NOTICE", string.format("install lock list - %s", self.install_cell_list))
  return true
end


function SasClient:delRegPendingRetryTimer()
  if self.ms_reg_retry_timer then
    self.l.log("LOG_NOTICE", string.format('cancel REG_PENDING retry timer: %s', self.ms_reg_retry_timer))
    self.i:remove_timeout(self.ms_reg_retry_timer)
    self.ms_reg_retry_timer = nil
  end
end

function SasClient:registration_cmd(key, val)
  if type(val) ~= "string" or val:len() < 2 then
    if val ~= "" then
      self.l.log("LOG_NOTICE", string.format("registration_cmd(): Invalid action '%s'", val))
    end
    return
  end

  local state = self.ssm.get_current_state()
  local attached = tonumber(self.rdb.get(self.attach_rdb)) or 0

  self.l.log("LOG_NOTICE", string.format("registration_cmd(): %s, state:%s, attached:%s", val, state, attached))

  -- delete any reg_pending timer if exists because now we have a new registration command
  self:delRegPendingRetryTimer()

  local action = val:lower()
  if action == "register" and state == "sas:idle" then
    self.l.log("LOG_NOTICE", 'registration_cmd() cmd=register in state=idle, switching to attach state')
    self.ssm.switch_state_machine_to("sas:attach")
    return
  end

  if state ~= "sas:operate" then
    if action == 'force_deregister' then
      -- allow force deregister in non-operate state to clear internal data & rdb
      if self.sas_started and self.ctx.state_machine.rstate == 'Registered' then
        self.ctx.current_response = {}
        self:deregister()
      end
      self.rdb.set(key, "")
      return
    end
    self.l.log("LOG_ERR", string.format("registration_cmd(): cannot %s in state: '%s'", val, state))
    return
  end

  if action == 'register' then
    if self.ctx.state_machine.rstate == 'Registered' then
      self:sas_deregister() -- deregister first
      return                -- resume after deregistration is done
    end
    -- do restart to refresh data from rdb then start registration
    self.ctx.req_tab = {}
    self:restart()
    self:registration(self.ctx.state_machine.user_values)
  elseif action == 'deregister' or action == 'force_deregister' then
    self.rdb.set(key, "")
    self:sas_deregister()
    if action == 'force_deregister' then self.ctx.req_tab.force_deregister = true end
  else
    self.l.log("LOG_ERR", 'not supported ' .. key .. '=' .. action)
  end
  self.rdb.set(key, "")
end

-- load and re-construct the saved state
function SasClient:read_saved_data()
  local is_unregistered = self.rdb.get(self.registration_state_rdb) ~= "Registered"
  if is_unregistered then
    return false
  end

  -- sanity check all grant properties for missing and invalid value, remove grant from rdb if check fails
  local validate = function(items, index)
    -- all required properties must exist
    local required = {'cbsdId', 'channelType', 'grantRequiredFor', 'highFrequency', 'lowFrequency', 'maxEirp', 'operationState'}
    for _,v in ipairs(required) do
      if not items[v] then
        self.l.log("LOG_ERR", string.format("INVALID grant:%s index:%s missing:%s", items.grantId, index, v))
        self:delete_grant_from_rdb(index) return nil
      end
    end

    -- validate time stamp
    if items.grantRequiredFor == 'Legacy' then
      local total = {}
      if items.legacyTime then
        local pattern = "(%d+)%-(%d+)%-(%d+)%a(%d+)%:(%d+)%:([%d%.]+)([Z%+%-])(%d?%d?)%:?(%d?%d?)"
        local year, month, day, hour, minute, seconds, offsetSign, offsetHour, offsetMin = items.legacyTime:match(pattern)
        total = {year, month, day, hour, minute, seconds, offsetSign, offsetHour, offsetMin}
      end
      if #total ~= 9 then -- all 9 items in the total array must exist
        self.l.log("LOG_ERR", string.format("INVALID legacy grant:%s index:%s legacyTime:%s",
          items.grantId, index, items.legacyTime or 'missing'))
        self:delete_grant_from_rdb(index) return nil
      end
    end

    -- bandwidth must be 10 Mhz (same as self.GRANT_SIZE)
    local bw = (items.highFrequency - items.lowFrequency)/self.MEGA
    if bw ~= self.GRANT_SIZE then
      self.l.log("LOG_ERR",string.format("INVALID grant:%s index:%s bandwidth %s", items.grantId, items.index, bw))
      self:delete_grant_from_rdb(index) return nil
    end

    -- grant must start at 10 Mhz boundary
    if not self:at_boundary(items.lowFrequency/self.MEGA, 10) or
       not self:at_boundary(items.highFrequency/self.MEGA, 10) then
      self.l.log("LOG_ERR", string.format("INVALID grant:%s index:%s boundary alignment %s-%s",
        items.grantId, items.index, items.lowFrequency/self.MEGA, items.highFrequency/self.MEGA))
      self:delete_grant_from_rdb(index) return nil
    end

    -- index 0 is not used for any grant
    if index == 0 then
      self:delete_grant_from_rdb(index) return nil
    end

    items.measReportConfig = self.measCapIndex == 3 and self.measCapStr[self.measCapIndex] or nil

    -- lowFrequency must be same as cbrs_grants at the index number
    local start_freq = self.CBRS_START + self.GRANT_SIZE * (index-1)
    if start_freq ~= items.lowFrequency/self.MEGA then
      self.l.log("LOG_ERR", string.format("INVALID grant:%s at index:%s expect:%s got lowFrequency:%s",
        items.grantId, items.index, start_freq, items.lowFrequency/self.MEGA))
      self:delete_grant_from_rdb(index) return nil
    end
    self.cbrs_grants[index] = items

    return items
  end

  local grants = {}
  for index = 0, self.MAX_SAS_GRANT_INDEX do
    local grantId = self.rdb.get(string.format('sas.grant.%d.id', index))
    if not grantId then
      local partial_keys = self.rdb.keys(string.format('sas.grant.%d.', index))
      if #partial_keys > 0 then
        self.l.log("LOG_ERR", string.format("grantId is missing for index:%s, removing %s partial keys", index, #partial_keys))
        self:delete_grant_from_rdb(index)
      end
    else
      local grant_item = {
          ['cbsdId'] =  self.rdb.get('sas.cbsdid'),
          ['channelType'] = self.rdb.get(string.format('sas.grant.%d.channel_type', index)),
          ['grantExpireTime'] = self.rdb.get(string.format('sas.grant.%d.expire_time', index)),
          ['grantId'] = grantId,
          ['grantRenew'] =  true,
          ['grantRequiredFor'] = self.rdb.get(string.format('sas.grant.%d.grantRequiredFor', index)),
          ["highFrequency"] = tonumber(self.rdb.get(string.format('sas.grant.%d.freq_range_high', index))),
          ["lowFrequency"] = tonumber(self.rdb.get(string.format('sas.grant.%d.freq_range_low', index))),
          ["maxEirp"] = tonumber(self.rdb.get(string.format('sas.grant.%d.max_eirp', index))),
          ["nextHeartbeat"] = self.rdb.get(string.format('sas.grant.%d.next_heartbeat', index)),
          ["operationState"] = self.rdb.get(string.format('sas.grant.%d.state', index)),
          ["legacyTime"] = self.rdb.get(string.format('sas.grant.%d.legacyTime', index)),
          ["index"] = index,
      }
      grants[grantId] = validate(grant_item, index)
    end
  end
  local saved_state = {
    ['grants']    = grants,
    ['rstate']    = self.rdb.get(self.registration_state_rdb),
    ['url']       = self.rdb.get('sas.config.url'),
    ['user_values'] = {['cbsdId'] = self.rdb.get('sas.cbsdid')},
    ['ver_major'] = 0,
    ['ver_minor'] = 0,
  }

  return saved_state
end

-- get saved state or default
-- translate version as required
function SasClient:read_saved_state()
  -- AT&T url https://sas47.sascms.net:8443/v1.2/
  -- test url https://netcomm.sascms.net:8443/v1.2/
  local reg_state = self.rdb.get(self.registration_state_rdb) or ""
  if reg_state == "" then reg_state = "Unregistered" end
  local default_state = {
    ['rstate']     = reg_state,
    ['url']        = self.rdb.get("sas.config.url"),
    ['ver_major']  = 0,
    ['ver_minor']  = 0,
    ['ssl_opts']   = self.ssl_opts,
    user_values    = {},
  }
  local v = self:read_saved_data()
  if not v then
    return default_state
  end
  if not v.ver_major or not v.ver_minor then
    self.l.log("LOG_ERR", 'rdb saved state does not contain valid data')
    return default_state
  end

  self.l.log("LOG_NOTICE", "Read saved state:")
  for line in self.util.splitlines(self.util.tprint(v)) do
    self.l.log("LOG_NOTICE", line)
  end

  return v
end

function SasClient:state_vars(_)
 local ss = self:read_saved_state()
 if not ss['user_values'] then
   ss['user_values'] = {}
 end
 if not ss.ssl_opts then
    ss.ssl_opts = self.ssl_opts
 end
 return ss
end

-- caculate the frequency from the earfcn
function SasClient:GetFrequencyByEarfcn(earfcn)
   -- only b48 was supported
   local earfcn_cfg = {['B48'] = {['earfcn_start'] = 55240, ['earfcn_end'] = 56739, ['freq_start'] = 3550 },}
   for band, tab in pairs(earfcn_cfg) do
       if earfcn >= tab['earfcn_start'] and earfcn <= tab['earfcn_end'] then
          return tab['freq_start'] + (0.1 * (earfcn - tab['earfcn_start']))
       end
   end
   self.l.log("LOG_ERR", string.format('GetFrequencyByEarfcn, invalid earfcn(%d), only B48 supported', earfcn))
   return nil
end

-- generate measurement report for the freq range with given power
function SasClient:genMeasReport(tab, f_start, f_end, power)
  local has_measurment = function(t, f)
    for _,item in ipairs(t) do if f == item.measFrequency then return item end end
  end

  local MAX_BW = 10 -- SAS requires max bandwidth of 10Mhz per measurement
  local bw = MAX_BW
  if self.measReport10Mhz then
    local at_10Mhz = function(f) local _, frac = math.modf(f/10) return frac == 0.0 end
    local pin = function(f) return at_10Mhz(f) and f or (math.floor(f/10))*10 end
    f_start, f_end = pin(f_start),pin( f_end)
  end
  while f_start < f_end do
    if not self.measReport10Mhz then
      bw = f_start + MAX_BW <= f_end and MAX_BW or f_end - f_start
    end
    if not has_measurment(tab, f_start * self.MEGA) then
      table.insert(tab, { ['measFrequency'] = f_start * self.MEGA,
                          ['measBandwidth'] = bw * self.MEGA,
                          ['measRcvdPower'] = power and power or -100})
    end
    f_start = f_start + bw
  end
end

-- add measurement report from RDB data
function SasClient:addMeasItem(tab, bwLookup, item_rdb)
  local meas_item = self.rdb.get(item_rdb) or ''
  local ctable = meas_item:split(',')
  -- expected in this format: RadioType,earfcn,pci,rsrp,rsrq
  if #ctable ~= 5 then
    if meas_item ~= '' then
      self.l.log("LOG_NOTICE", string.format('rdb:"%s" ,unknown format:"%s"', item_rdb, meas_item))
    end
    return
  end

  local earfcn = tonumber(ctable[2])
  local freq = self:GetFrequencyByEarfcn(earfcn)
  if not freq then return end

  local bandwidth = bwLookup[earfcn]
  if not bandwidth then
    self.l.log("LOG_ERR", string.format("No bandwidth for earfcn:%s, bwLookup:%s", earfcn, bwLookup))
    return
  end

  local measRcvdPower = tonumber(ctable[4])
  -- limited the measRcvdPower to -100 ~ -25
  measRcvdPower = (measRcvdPower > -100 ) and measRcvdPower or -100
  measRcvdPower = (measRcvdPower < -25 ) and  measRcvdPower or -25

  local range = {['lo_Mhz']= (freq - 0.5 * bandwidth), ['hi_Mhz']=(freq + 0.5 * bandwidth)}
  local pin = self:pin_freq(range, 5)
  self:genMeasReport(tab, pin.lo_Mhz, pin.hi_Mhz, measRcvdPower)
end

-- prepare a table to lookup bandwidth
function SasClient:buildBandwidthTable()
  local bwLookup = {} -- bandwith in Mhz
  local freq_to_bandwidth = self.rdb.get("wwan.0.cell_measurement.freq_to_bandwidth") or ''
  local serv_earfcn = self.rdb.get(self.pEarfcnKey)
  local serv_bw  = self.rdb.get(self.dl_bandwidth_rdb)
  freq_to_bandwidth = freq_to_bandwidth .. serv_earfcn .. ':' .. serv_bw -- include serving cell
  for k,v in pairs(freq_to_bandwidth:split(',')) do
    local t = v:split(':')
    if #t == 2 then bwLookup[tonumber(t[1])] = self.RBsToMHz[tonumber(t[2])] end
  end

  -- load bandwidth info from ncell.current
  local ncell = self.rdb.get("wwan.0.cell_measurement.ncell.current") or ''
  for k,v in pairs(ncell:split('|')) do
    local t = v:split(',')
    if #t == 5 then bwLookup[tonumber(t[1])] = self.RBsToMHz[tonumber(t[3])] end
  end

  -- iterate given rdb prefix to find cell with no bandwidth
  local find_cells_with_no_bandwidth = function(tab, rdb_prefix)
    local count = tonumber(self.rdb.get(rdb_prefix..'qty')) or 0
    for i=0, count do
      local cells = self.rdb.get(rdb_prefix..tostring(i)) or ''
      local c = cells:split(',')
      if #c == 5 then
        local earfcn = tonumber(c[2])
        if not bwLookup[earfcn] then tab[earfcn]=0 end
      end
    end
  end

  -- find bandwidth by using earfcn diff with a reference cell
  local bw_by_earfcn_diff = function(earfcn, ref)
    local diff = string.format("%.2f",
      math.abs(self:GetFrequencyByEarfcn(earfcn) - self:GetFrequencyByEarfcn(ref.earfcn)))
    if ref.bandwidth == 10 then
      return diff == "9.90" and 10 or diff == "14.40" and 20 or nil
    elseif ref.bandwidth == 20 then
      return diff == "14.40" and 10 or diff == "19.80" and 20 or nil
    end
  end

  local unknown_cells = {} -- cell with no bandwidth info yet
  find_cells_with_no_bandwidth(unknown_cells, 'wwan.0.manual_cell_meas.')
  find_cells_with_no_bandwidth(unknown_cells, 'wwan.0.cell_measurement.')

  local found_new_bw
  repeat
    found_new_bw = false
    for k,_ in pairs(unknown_cells) do
      for earfcn, bandwidth in pairs(bwLookup) do
        local bw = bw_by_earfcn_diff(k, {['earfcn']=earfcn, ['bandwidth']=bandwidth})
        if bw then
          bwLookup[k] = bw
          unknown_cells[k] = nil
          found_new_bw = true
          break
        end
      end
    end
  until found_new_bw == false
  return bwLookup
end

function SasClient:getRcvdPowerMeasReports()
  local bwLookup = self:buildBandwidthTable()
  local tab = {}

  -- load the manual measurements produced by qdiag
  local manual_meas_qty = tonumber(self.rdb.get('wwan.0.manual_cell_meas.qty')) or 0
  for i= 0, manual_meas_qty do
    self:addMeasItem(tab, bwLookup, 'wwan.0.manual_cell_meas.'..tostring(i))
  end

  -- load the cell measurement produced by qmi_nas
  local cell_meas_qty = tonumber(self.rdb.get('wwan.0.cell_measurement.qty')) or 0
  for i= 0, cell_meas_qty do
    self:addMeasItem(tab, bwLookup, 'wwan.0.cell_measurement.'..tostring(i))
  end

  table.sort(tab, function(a,b) return a.measFrequency < b.measFrequency end)

  -- wall through CBRS entire frequency range and insert default data for segment with no measurement
  local MIN_FREQ, MAX_FREQ = 3550, 3700 -- CBRS freq range
  local freq = MIN_FREQ
  local report = {}

  for k,v in ipairs(tab) do
    local meas_freq = v.measFrequency / self.MEGA
    if meas_freq > freq then
      self:genMeasReport(report, freq, meas_freq)
    end

    self.l.log("LOG_DEBUG", string.format("MEASUREMENT: %02s: freq:%s bw:%s pwr:%s",
      k, v.measFrequency/self.MEGA, v.measBandwidth/self.MEGA, v.measRcvdPower))

    table.insert(report, v)
    freq = meas_freq + v.measBandwidth/self.MEGA
  end

  if freq < MAX_FREQ then
    self:genMeasReport(report, freq, MAX_FREQ)
  end

  table.sort(report, function(a,b) return a.measFrequency < b.measFrequency end)
  return report
end

-- get the measurement report
function SasClient:get_meas_report()
 if self.measCapStr[self.measCapIndex] == "" then return nil end

 local rdb_cell_meas_cnt = self.rdb.get('wwan.0.cell_measurement.cnt')
 local rdb_manual_cell_meas_seq = self.rdb.get('wwan.0.manual_cell_meas.seq')
 if self.manual_cell_meas_seq ~= rdb_manual_cell_meas_seq or self.cell_meas_cnt ~= rdb_cell_meas_cnt then
    -- the measurements were updated, need to reload from the rdb
    self.meas_report_tbl = { ["rcvdPowerMeasReports"] = self:getRcvdPowerMeasReports() }
    self.manual_cell_meas_seq = rdb_manual_cell_meas_seq
    self.cell_meas_cnt = rdb_cell_meas_cnt
 end
 return self.meas_report_tbl
end

function SasClient:utab_for_grant(rtab, i, grantRequiredFor)
  local function freq(j) return 3550 * self.MEGA  + j * 10 * self.MEGA  end
  i = i or 0
  rtab = rtab or {}
  rtab['lowFrequency'] = freq(i)
  rtab['highFrequency'] = freq(i+1)
  rtab['maxEirp'] = self.eirpCapability_perMHz
  rtab['grantRequiredFor'] = grantRequiredFor
  return rtab
end

function SasClient:user_values(action, options)
  local utab = nil
  local dtab = nil
  local swfun = {
    ['registration'] = function()
      utab = {
         ["fccId"]           = self.rdb.get('sas.config.fccid'),
         ["cbsdCategory"]    = "B",
         ["callSign"]        = self.rdb.get('sas.config.callSign') or "CB987",
         ["cbsdSerialNumber"]= self.rdb.get('sas.config.cbsdSerialNumber'),
         ["userId"]          = self.rdb.get('sas.config.userId'),
         ["channelType"]     = "PAL",
         ["radioTechnology"] = "E_UTRA",
         ["measCapability"]  =  { self.measCapStr[self.measCapIndex] },
         -- include cbsdInfo
         ["vendor"] = self.rdb.get('system.product.vendor') or 'Casa Systems Inc',
         ["model"] = self.rdb.get('system.product.model') or 'undefined',
         ["softwareVersion"] = self.rdb.get('sw.version') or 'undefined',
         ["hardwareVersion"] = self.rdb.get('wwan.0.hardware_version') or 'undefined',
         ["firmwareVersion"] = self.rdb.get('wwan.0.firmware_version') or 'undefined',
      }

      local reg_method = self.rdb.get("sas.regMethod")
      if not reg_method or reg_method == '' then
         reg_method = self.rdb.get("sas.config.regMethod") or "single"
      end
      self.l.log('LOG_NOTICE', string.format("registration method [%s] selected", reg_method))
      -- for single-step registration, we need installationParam which is inside cpiSignatureData
      -- for multi-step, the CPI will sign the data for the device using differeent method.
      if reg_method == "single" then
        -- include cpiSignatureData object if exists
        local signatureData = self.rdb.get('sas.config.cpiSignatureData')
        if signatureData ~= nil and signatureData ~= '' then
          utab['cpiSignatureData'] = signatureData
        end
      end

      -- check optional parameter groupingParam
      local val = self.rdb.get('sas.config.groupingParam')
      if val ~= nil and val ~= '' then
        utab['groupingParam'] = val
      end
     end,
    ['spectrumInquiry'] = function()
      utab = {
       ['measReport'] =  self:get_meas_report(),
       ['lowFrequency'] = 3550000000,
       ['highFrequency'] = 3700000000,
       ['maxEirp'] = self.eirpCapability_perMHz,
       }
     end,
    ['relinquishment'] = function()
      utab = { ['grantId'] = options['grantId'],
               ['cbsdId']  = options['cbsdId'],
             }
     end,
    ['deregistration'] = function()
      utab = { ['cbsdId']  = options['cbsdId'],
             }
     end,
    ['grant'] = function()
      utab = self:utab_for_grant({}, options['index'], options['grantRequiredFor'])
     end,
    ['heartbeat'] = function()
      utab = {
       ['grantRenew'] = true,
      }
     end
  }
  if swfun[action] then
      swfun[action]()
  end
  return utab, dtab
end

-- lookup rdb index for a grant (neighbour grant only)
function SasClient:find_rdb_grantid(grantid)
  if type(grantid) ~= 'string' or grantid:len() < 2 then return false end
  for i = 0, self.MAX_SAS_GRANT_INDEX do
    local id = self.rdb.get('sas.grant.' .. i .. '.id')
    if id and id == grantid then
      return i
    end
  end
  return false
end

-- save current cell info to rdb
function SasClient:cell_set_rdb()
  local to_rdb = function(key)
    local prefix = string.format("sas.cell.%s.", key)
    local info = self.cells[key].info
    local set = function(k,v) if self.rdb.get(k) ~= v then self.rdb.set(k,v) end end
    set(prefix.."authorized", self:is_authorized(key) and "YES" or "NO")
    set(prefix.."freq", string.format("%s-%s|%s-%s",info.freq.lo_Mhz,
                            info.freq.hi_Mhz, info.pin.lo_Mhz, info.pin.hi_Mhz))
    set(prefix.."grants", table.concat(self.cells[key].indexes,","))
    set(prefix.."pci", info.pci_list or '')
    set(prefix.."ecgi_list", info.ecgi_list or '')
    set(prefix.."type", info.reqFor)
    set(prefix.."pci_earfcn", string.format("%s:%s", info.pci, key))
  end

  -- remove old cells
  for _,k in ipairs(self.rdb.keys("sas.cell.")) do
    local key = k:split(".")[3]
    if not self.active_cells[key] then self.rdb.unset(k) end
  end

  -- save current cells
  for key,_ in pairs(self.active_cells) do to_rdb(key) end
end

-- save grant info to rdb
function SasClient:grant_set_rdb(uv)
  local rtab = {}
  if not uv.grantId or not uv.index then
    self.l.log("LOG_ERR", "grant_set_rdb() INVALID grant table ---------------------:")
    for l in self.util.splitlines(self.util.tprint(uv)) do self.l.log("LOG_ERR", l) end
    return
  end
  local prefix = 'sas.grant.'..uv.index..'.'
  local stateChanged = false
  self.util.copy_named_vars(prefix, rtab, self.rdb_grant_strs, uv)
  for k,v in pairs(rtab) do
    local old_value = self.rdb.get(k) or false
    local new_value = tostring(v)
    if v and (not old_value or old_value ~= new_value) then
      self.l.log("LOG_DEBUG", string.format("%s [%s] -> [%s]", k, old_value, new_value))
      self.rdb.set(k, new_value, 'p')
      if k:match('state') or k:match('grantRequiredFor') then
        stateChanged = true
        if old_value == 'AUTHORIZED' and new_value == 'GRANTED' then
          uv.txOffTime = self.util.zulu(os.time() + 60)
        end
      end
    end
  end
  if stateChanged then self:onGrantStateChange(uv.grantId, uv) end
end

-- delete a grant from rdb
function SasClient:delete_grant_from_rdb(index)
  -- remove all RDB variables for this grant
  local prefix = 'sas.grant.'..index..'.'
  for _,k in ipairs(self.rdb.keys(prefix)) do
    self.rdb.unset(k)
  end
end

function SasClient:remove_grant_rdb(grant)
  if self.ctx.state_machine.grants[grant].operationState ~= 'REMOVED' then
    self.ctx.state_machine.grants[grant].operationState = 'REMOVED'
    self:onGrantStateChange(grant, self.ctx.state_machine.grants[grant])
  end
  local index = self.ctx.state_machine.grants[grant].index
  if not index then self.l.log("LOG_ERR", string.format("NO INDEX for grant:%s", grant.grantId)) return end
  self:delete_grant_from_rdb(index)
end

-- true if the frequency f (in Mhz) is at the given boundary
function SasClient:at_boundary(f, boundary)
  if boundary and boundary > 0 then
    local _, frac = math.modf(f/boundary)
    return frac == 0.0
  end
  return true
end

-- pin to nearest desired frequency boundary
function SasClient:pin_freq(freq, boundary)
  local expand = function(f, boundary, up)
    if not self:at_boundary(f, boundary) then
      return (math.floor(f/boundary) + up)*boundary
    end
    return f
  end
  return {['lo_Mhz']=expand(freq.lo_Mhz, boundary, 0), ['hi_Mhz']=expand(freq.hi_Mhz, boundary, 1)}
end

-- get frequency range for primary serving cell (raw)
function SasClient:serving_cell_freq()
  local ndl = tonumber(self.rdb.get(self.pEarfcnKey)) or 0
  local bw_rbs  = tonumber(self.rdb.get(self.dl_bandwidth_rdb))
  local bw_freq = self.RBsToMHz[bw_rbs]
  if not bw_freq then
    bw_freq = 0
    self.l.log("LOG_ERR", string.format("Invalid %s in RBs value:%s -> MHz bw_freq:%s",
      self.dl_bandwidth_rdb, bw_rbs, bw_freq))
  end
  if ndl < 55240 or ndl > 56739 then
    -- TODO: invalid ndl, probably wrong band, we should close sas_client
    self.l.log("LOG_ERR",
      string.format("Invalid %s value:%s, replace with 55490 for testing", self.pEarfcnKey, ndl))
    ndl = 55490
  end
  local freq = 3550 + (0.1 * (ndl - 55240))
  return {['lo_Mhz'] = (freq - 0.5*bw_freq), ['hi_Mhz']=(freq + 0.5*bw_freq)}
end

-- save registration info to rdb
function SasClient:registration_to_rdb(response)
  local rtab = {}
  self.util.copy_named_vars('sas.', rtab, self.rdb_registration_strs, response)
  for k,v in pairs(rtab) do
    if k and v then
      self.rdb.set(k, ''..v, 'p')
    end
  end
end

function SasClient:set_pcilock(lock, logs, enable_CA)
  -- CA mode: disable CA before changing PCI lock list or modem will crash.
  local ca_enabled = tonumber(self.rdb.get("wwan.0.system_network_status.ca.enabled"))
  if (not enable_CA or enable_CA == 0) and (ca_enabled ~= 0) then
    self.l.log("LOG_NOTICE", "set_pcilock() wait for disable CA")
    self.rdb.set("service.luaqmi.command", "setCAmode,0")
    self.pci_lock_pending = { lock = lock, logs = logs }
    return
  end

  if self.rdb.get(self.lock_list_rdb) ~= lock or lock == '' then
    self.l.log("LOG_NOTICE", 'New PCI locking list: '..lock)
    self.rdb.set(self.lock_list_rdb, lock)
    -- dump the logs when PCI lock changes to help debugging
    if logs then for _,v in ipairs(logs) do self.l.log("LOG_NOTICE", v) end end
  end

  if enable_CA == 1 and ca_enabled ~= enable_CA then
    self.rdb.set("service.luaqmi.command", "setCAmode,1")
  end
end

-- CA mode has changed, update PCI lock list
function SasClient:ca_mode_changed(key, val)
  if val == "0" and self.pci_lock_pending then
    self.l.log("LOG_NOTICE", string.format("ca_mode_changed() update PCI lock:%s", self.pci_lock_pending.lock))
    self:set_pcilock(self.pci_lock_pending.lock, self.pci_lock_pending.logs, 0)
    self.pci_lock_pending = nil
  end
end

-- get latest cell measurements
function SasClient:get_cell_measurements()
  local cell_meas = {}   -- cell measurement table
  local rrc_stat = self.rdb.get("wwan.0.radio_stack.rrc_stat.rrc_stat") or "empty"

  -- get bplmn scan data first, then override with neighbour cell measurements (if rrc connected)
  local num_cells = tonumber(self.rdb.get("wwan.0.manual_cell_meas.qty")) or 0
  for i=0, num_cells-1 do
    local cell_data = self.rdb.get("wwan.0.manual_cell_meas."..i)
    local cell = cell_data:split(",")
    -- add the rsrp for the pci-earfcn combination
    cell_meas[cell[3]..cell[2]] = cell[4]
  end

  -- if rrc is connected, override any existing values
  if rrc_stat == "connected" then
    num_cells = tonumber(self.rdb.get("wwan.0.cell_measurement.qty")) or 0
    for i=0, num_cells-1 do
      local cell_data = self.rdb.get("wwan.0.cell_measurement."..i)
      local cell = cell_data:split(",")
      -- add the rsrp for the pci-earfcn combination
      cell_meas[cell[3]..cell[2]] = cell[4]
    end
  end

  return cell_meas
end

-- get PCI lock, return 3 values:
-- lock_list: PCI lock list in format 'pci:<value>earfcn:<value>;pci:<value>earfcn:<value>;...'
-- logs: an array of log messges with info on each pci in the lock_list
-- enable CA: to enable or disable carrier aggregation mode
function SasClient:get_pci_lock_list()
  self.update_pci_lock_pending = nil
  -- Enable CA mode if all cells (>1) seen in last 3hrs are authorized
  local ca = { cells = 0, authorized = 0, limit = 3*60*60, now = os.time(), vis = {} }
  local pci_list = {}
  for k,v in pairs(self.cells) do
    local authorized = self:is_authorized(k)
    local last_seen = v.info.last_seen and (ca.now - v.info.last_seen) or nil
    if last_seen and last_seen < ca.limit then
      ca.cells = ca.cells + 1
      ca.authorized = ca.authorized + (authorized and 1 or 0)
      table.insert(ca.vis, {cell = k, sec = last_seen}) -- for debug: use sas.command='print ca' to inspect
    end
    if authorized then
      local list = v.info.pci_list:split('|') -- format: pci,timestamp|pci,timestamp|...
      for i = #list, 1, -1 do
        local info = list[i]:split(',')
        local pci, ts = tonumber(info[1]), tonumber(info[2])
        if pci and ts then
          table.insert(pci_list, {['pci']=pci, ['earfcn']=tonumber(v.info.earfcn), ['ts']=ts, ['reqFor']=v.info.reqFor})
        end
      end
    end
  end

  -- sort pci_earfcn_list in reverse chronological then prune old cells
  table.sort(pci_list, function(a, b) return a.ts > b.ts end)
  while #pci_list > self.MAX_PCI_LOCK_LIST_SIZE do table.remove(pci_list) end

  -- sort PCI lock list in pci & earfcn order, this will minimize changes to RDB,
  -- eg. known cells can appear/dispear without the need to update PCI lock
  table.sort(pci_list, function(a,b) return a.pci == b.pci and a.earfcn < b.earfcn or a.pci < b.pci end)

  local lock_list, sep = '', ''
  local cell_measurements = self:get_cell_measurements()
  local logs = {}
  for k, v in ipairs(pci_list) do
    local pci, earfcn = v.pci, v.earfcn
    local rsrp = tonumber(cell_measurements[v.pci..v.earfcn]) or self.NO_RSRP
    table.insert(logs, string.format('pci:%s, earfcn:%s, rsrp:%s %s', v.pci, v.earfcn, rsrp, v.reqFor))
    if not self.exclude_weak_cell or rsrp > self.MIN_RSRP then
      lock_list = lock_list..sep..string.format("pci:%s,earfcn:%s",pci,earfcn)
      sep = ';'
    end
  end

  ca.enable = (ca.cells > 1 and ca.cells == ca.authorized) and 1 or 0
  if not self.ca or self.ca.enable ~= ca.enable then
    self.l.log("LOG_NOTICE", string.format("CA mode changed: %s -> %s",
      self.ca and self.enable or "?", ca.enable))
  end
  self.ca = ca
  return lock_list, logs, ca.enable
end

function SasClient:update_pci_lock_list()
  -- do update in the next turbo IO loop to allow ctx.state_machine.grants to settle to final value.
  -- for example, if heartbeat bundle has 5 authorized grants we want update the modem with all 5 in one go.
  if not self.update_pci_lock_pending then
    self.update_pci_lock_pending = true
    local do_update = function()
      self:set_pcilock( self:get_pci_lock_list() )
    end
    self.i:add_callback(do_update, self)
  end
end

-- enable/disable sas transmission
function SasClient:update_sas_transmit(enable)
  enable = enable and '1' or '0'
  local key = "sas.transmit_enabled"
  if self.rdb.get(key) ~= enable then
    self.l.log("LOG_NOTICE", key .. " -> " .. enable)
    self.rdb.set(key, enable)
  end
end

-- cancel the timer that is used to update sas transmission
function SasClient:cancel_apn_timer()
  if self.apnTimer then
    self.l.log("LOG_NOTICE", 'cancel apnTimer:'..tostring(self.apnTimer))
    self.i:remove_timeout(self.apnTimer)
    self.apnTimer = nil
    if self.apnTimerPostAction then
      self:apnTimerPostAction()
      self.apnTimerPostAction = nil
    end
  end
end

-- start a timer to turn off sas transmission 60s after transmit expire time
function SasClient:start_apn_timer(transmitExpireTime)
  local disabled = self.rdb.get('sas.transmit_enabled') == '0'
  if disabled then
    -- already disabled, nothing todo
    self.l.log("LOG_DEBUG", 'already disabled, no need for a timer!')
    return
  end

  self:cancel_apn_timer()  -- cancel pending timer if exists

  local event = function()
    if not self:is_authorized() then
      self.l.log("LOG_NOTICE", "APN timer expired and not authorised => disable transmission now!")
      self:update_sas_transmit(false)
    end
    self.apnTimer = nil
    if self.apnTimerPostAction then
      self:apnTimerPostAction()
      self.apnTimerPostAction = nil
    end
  end

  local seconds_to_expire = self.util.parse_json_date(transmitExpireTime) - os.time()
  local timeout = seconds_to_expire > 0 and (seconds_to_expire + 58) or 58 -- 2s budget for processing delay
  self.l.log("LOG_NOTICE", 'turning off sas transmission in '..timeout..' seconds')
  timeout = timeout * 1000 + self.t.util.gettimemonotonic()  -- ms from now
  self.apnTimer = self.i:add_timeout(timeout, event)
end

-- setup grants for given cell
function SasClient:setup_grants(cell)
  local indexes={} -- index of grants used by this cell
  local pin = self:pin_freq(cell.freq, self.GRANT_SIZE)
  if pin.lo_Mhz >= pin.hi_Mhz or pin.lo_Mhz < self.CBRS_START or pin.hi_Mhz > self.CBRS_END then
    self.l.log("LOG_ERR", string.format("INVALID freq[%s-%s] -> pin[%s-%s] boundary:%s",
      cell.freq.lo_Mhz, cell.freq.hi_Mhz, pin.lo_Mhz, pin.hi_Mhz, self.GRANT_SIZE))
    return indexes
  end
  cell.pin = pin

  local order_grant = function(i)
    local g = {['index'] = i, grantRequiredFor = cell.reqFor}
      g.lowFrequency = (self.CBRS_START + self.GRANT_SIZE * (i-1)) * self.MEGA
      g.highFrequency = g.lowFrequency + (self.GRANT_SIZE * self.MEGA)
    g.maxEirp = self:getMaxEirp(g)
    self:grant(g)
    self.l.log("LOG_NOTICE",
      string.format("%s:%s freq[%s-%s]->[%s-%s] index:%s, NO GRANT send request[%s-%s] %s",
      cell.pci, cell.earfcn, cell.freq.lo_Mhz, cell.freq.hi_Mhz, pin.lo_Mhz, pin.hi_Mhz,
      i, g.lowFrequency/self.MEGA, g.highFrequency/self.MEGA, cell.reqFor))
    return g
  end

  local start = 1 + (pin.lo_Mhz - self.CBRS_START)/self.GRANT_SIZE
  local count = (pin.hi_Mhz - pin.lo_Mhz)/self.GRANT_SIZE
  for i = start, start + count - 1 do
    if not self.cbrs_grants[i] then
      self.cbrs_grants[i] = order_grant(i)
    end
    table.insert(indexes, i)
  end
  return indexes
end

function SasClient:set_cell_reqFor(cinfo, reqFor)
  if not cinfo.reqFor or cinfo.reqFor ~= reqFor then
    self.l.log("LOG_NOTICE", string.format("cell[%s:%s] reqFor:%s->%s",
      cinfo.pci, cinfo.earfcn, cinfo.reqFor, reqFor))
    cinfo.reqFor = reqFor
    self:set_ecgi_list(cinfo)
  end
end

-- update cbrs grants for all cells
function SasClient:update_grants()
  self.active_cells = {}

  -- grants for serving cell
  local pci, earfcn = self.rdb.get(self.serv_pci_rdb), self.rdb.get(self.pEarfcnKey)
  local key = earfcn
  local cell = self.cells[key] and self.cells[key].info or
                {pci = pci, earfcn = earfcn, pci_list = string.format("%s,%s",pci,os.time())}
  self:set_cell_reqFor(cell, 'ServingCell')
  self.active_cells[key] = 'ServingCell'
  if not self:is_authorized(key) then
    cell.freq = self:serving_cell_freq()
    self.cells[key] = {['info'] = cell, ['indexes'] = self:setup_grants(cell)}
    if self.nit_connected then
      self:set_grant_required_for()
      self.rateLimits = {} return
    end
  end
  cell.last_seen = os.time()

  local request_neighbour_grant = function(c)
    local key = c[2]
    local cell = self.cells[key] and self.cells[key].info or
      { pci = c[1], earfcn = c[2], pci_list = string.format("%s,%s",c[1],os.time())}
    self:set_cell_reqFor(cell, 'Neighbor')
    self.active_cells[key] = 'Neighbor'
    if not self:is_authorized(key) then
      cell.freq = {lo_Mhz = tonumber(c[4]), hi_Mhz = tonumber(c[5])}
      self.cells[key] = {['info'] = cell, ['indexes'] = self:setup_grants(cell)}
    end
    cell.last_seen = os.time()
  end

  -- grants for neighbor cells, sas.inter_cells, eg: 3,56528,10,3673.8,3683.8;2,56384,20,3654.4,3674.4
  local neighbour_cells = (self.rdb.get(self.cellListKey) or ''):split(';')
  for i=1, #neighbour_cells do
    local c = neighbour_cells[i]:split(',')
    if #c == 5 then request_neighbour_grant(c) end
  end

  -- cell on manual scanning rdb, wwan.0.manual_cell_meas.[x] eg. E,55856,1,-110.88,-23.00
  local seq = tonumber(self.rdb.get("wwan.0.manual_cell_meas.seq")) or 0
  if self.manual_cell_meas_seq ~= seq then
    self.manual_cell_meas_seq = seq
    local bwLookup = self:buildBandwidthTable()
    local qty = tonumber(self.rdb.get("wwan.0.manual_cell_meas.qty")) or 0
    for i = 0, qty - 1 do
      local ctable = (self.rdb.get("wwan.0.manual_cell_meas."..i) or ''):split(",")
      local key = ctable[2]
      if key and not self.active_cells[key] then
        local earfcn = tonumber(ctable[2])
        local freq = self:GetFrequencyByEarfcn(earfcn)
        local bandwidth = bwLookup[earfcn]
        if freq and bandwidth then
          ctable[1] = ctable[3]
          ctable[4] = freq - 0.5 * bandwidth
          ctable[5] = freq + 0.5 * bandwidth
          self.l.log("LOG_NOTICE", string.format("manual_cell_meas: earfcn:%s, freq:%s-%s", earfcn, ctable[4], ctable[5]))
          request_neighbour_grant(ctable)
        end
      end
    end
  end

  self:set_grant_required_for()
end

-- update grantRequiredFor, if overlaps, high priority to serving cell
function SasClient:set_grant_required_for()
  self.used_grants = {}

  local req_for = function(key, reqFor)
    local cell = self.cells[key] if not cell then return end
    for _,i in pairs(cell.indexes) do
      local g = self.cbrs_grants[i]
      if g.grantRequiredFor == 'Legacy' then
        self:stop_timeout('grant_relinquish_wait_time', g.grantId)
      end
      g.grantRequiredFor = reqFor
      self.used_grants[i] = true
    end
  end

  -- grantRequiredFor will be ServingCell if are used by both serving & neighbor cells
  for _,type_ in ipairs({'Neighbor', 'ServingCell'}) do
    for key,t in pairs(self.active_cells) do
      if t == type_ then req_for(key, type_) end
    end
  end

  -- old cells become Legacy
  for key,_ in pairs(self.cells) do
    if not self.active_cells[key] then
      self:set_cell_reqFor(self.cells[key].info, 'Legacy')
    end
  end

  -- unused grants become Legacy
  for _,g in pairs(self.cbrs_grants) do
    if g.index and not self.used_grants[g.index] then
      if g.grantId and g.grantRequiredFor ~= 'Legacy' then
        g.legacyTime = self.util.zulu(os.time())
        self.l.log("LOG_NOTICE", string.format("grant %s|%s|%s changed: %s -> Legacy",
          g.index, g.grantId, g.operationState, g.grantRequiredFor))
        g.grantRequiredFor = 'Legacy'
        self:start_std_timeout('grant_relinquish_wait_time', g.grantId)
      end
    end
    if g.grantId then self:grant_set_rdb(g) end
  end
end

-- relinquish all grants then restart spectrum inquiry procedure
function SasClient:renew_all_grants(cbsdId)
  if not self.ctx.state_machine.grants or next(self.ctx.state_machine.grants) == nil then
    -- grant table empty, start spectrum inquiry
    local utab, dtab = self:user_values('spectrumInquiry', nil)
    self:spectrumInquiry(utab, dtab)
    return
  end

  -- relinquish all grants, spectrum inquiry will start when all relinquished
  local utab, dtab = self:user_values('relinquishment', {['cbsdId'] = cbsdId})
  for grantId, v in pairs(self.ctx.state_machine.grants) do
    if v.operationState ~= 'REMOVED' then
      self:relinquish_grant(v, 'renew_on_nogrant')
    end
  end
end

-- setup for grant retry after wait time
function SasClient:grant_retry_wait_time()
  local expiry = os.time() + (self.rdb.get('sas.timer.grant_retry_wait_time') or '43200')
  local lowfreq = tonumber(self.ctx.req_tab.lowFrequency)/self.MEGA
  local highfreq = tonumber(self.ctx.req_tab.highFrequency)/self.MEGA
  local id = string.format("%s-%s-%s-%s", lowfreq, highfreq, self.ctx.req_tab.maxEirp, self.ctx.req_tab.grantRequiredFor)
  -- create a dummy grant to store request details
  local _, uv = self:grant_table(id, true)
  uv.grantId = id
  uv.grantExpireTime = os.date("!%Y-%m-%dT%TZ", expiry)
  uv.operationState = 'WAIT-RETRY'
  uv.lowFrequency = self.ctx.req_tab.lowFrequency
  uv.highFrequency = self.ctx.req_tab.highFrequency
  uv.maxEirp = self.ctx.req_tab.maxEirp
  uv.index = self.ctx.req_tab.index
  uv.grantRequiredFor = self.ctx.req_tab.grantRequiredFor
  self:grant_set_rdb(uv)
  return uv
end

function SasClient:onGrantStateChange(grant, uv)
  self:cell_set_rdb()
  self:update_pci_lock_list()

  -- update sas.grant._index, a csv list of valid grant indices.
  local grant_indices = ''
  local sep = ''
  local authorization_count = 0
  for k,v in pairs(self.ctx.state_machine.grants) do
    if v.operationState == 'AUTHORIZED' or v.operationState == 'GRANTED' then
      local index = self:find_rdb_grantid(k)
      if index then
        grant_indices = grant_indices .. sep .. tostring(index)
        if sep == '' then sep = ',' end

        if v.operationState == 'AUTHORIZED' then
          authorization_count = authorization_count + 1
        end
      end
    end
  end
  local grant_indices_key = 'sas.grant._index'
  if self.rdb.get(grant_indices_key) ~= grant_indices then
    self.rdb.set(grant_indices_key, grant_indices)
  end

  -- update authorization_count
  local rdb_authorization = 'sas.authorization_count'
  authorization_count = tostring(authorization_count)
  if self.rdb.get(rdb_authorization) ~= authorization_count then
    self.rdb.set(rdb_authorization, authorization_count)
  end

  if not uv or uv.grantRequiredFor ~= 'ServingCell' then return end

  -- change the tx power according to the new grant
  self:setModemTxPower()

  -- enable/disable sas transmission if serving cell becomes au-authorized
  if self:is_authorized() then
    self:cancel_apn_timer()
    self:update_sas_transmit(true)
  elseif not self.apnTimer and self.rdb.get("sas.transmit_enabled") == "1" then
    if uv.operationState == 'REMOVED' then self:update_sas_transmit(false) return end
    -- SAS transmission is to be turn off 60s after transmit expire time
    local transmitExpireTime = uv.transmitExpireTime or os.date("!%Y-%m-%dT%TZ")
    self:start_apn_timer(transmitExpireTime)
  end
end

function SasClient:nit_connected_status_change(key, val)
  self.nit_connected = val == "1"
end

-- keep all history of cell's pci values in grant.pci_list
function SasClient:update_pci_list(pci, earfcn)
  if not self.cells[earfcn] then return end
  pci = tonumber(pci)

  -- remove old entry of pci and convert pci string list to an array
  local cell = self.cells[earfcn].info
  local pci_list = {}
  local list = (cell.pci_list or ''):split('|') -- format: pci,timestamp|pc,timestamp|...
  for i = #list, 1, -1 do
    local info = list[i]:split(',')
    local old_pci, ts = tonumber(info[1]), tonumber(info[2])
    if old_pci and (old_pci ~= pci) and ts then
      table.insert(pci_list, {['pci']=old_pci, ['ts']=ts})
    end
  end

  -- time stamp the new pci and add it to 1st position of the array
  table.insert(pci_list, 1, {['pci']=pci, ['ts']=os.time()})

  -- remove old entries if pci list is too long
  table.sort(pci_list, function(a, b) return a.ts > b.ts end)
  while #pci_list > self.MAX_PCI_PER_CELL do table.remove(pci_list) end

  -- convert pci_list back to string so it can be saved to rdb
  cell.pci_list = ''
  for i,v in ipairs(pci_list) do
    cell.pci_list = cell.pci_list..(i==1 and '' or '|')..string.format("%s,%s", v.pci, v.ts)
  end

  self:update_pci_lock_list()
end

-- prepare data then order grant relinquishment
function SasClient:relinquish_grant(g, post_action)
  g.operationState = 'REMOVED'
  g.post_action = post_action
  self:grant_set_rdb(g)
  local u,d = self:user_values('relinquishment', {['grantId'] = g.grantId, ['cbsdId'] = self.rdb.get('sas.cbsdid')})
  self:relinquishment(u, d, g.grantId)
end

-- true if all given grants are authorized
function SasClient:grants_authorized(grants)
  local authorized = false
  for _,g in pairs(grants) do
    if g.operationState ~= 'AUTHORIZED' then return false end
    authorized = true
  end
  return authorized
end

-- all grants that a cell use
function SasClient:cell_grants(key)
  local grants={}
  local indexes = self.cells[key] and self.cells[key].indexes or {}
  for _,i in pairs(indexes) do
    local g = self.cbrs_grants[i]
    if g then table.insert(grants, g) end
  end
  return grants
end

-- all grants used by serving cell
function SasClient:serving_cell_grants()
  return self:cell_grants(self.rdb.get(self.pEarfcnKey))
end

-- true if all the cell with id or servining cell is authorized
function SasClient:is_authorized(key)
  local authorized = false
  key = key and key or self.rdb.get(self.pEarfcnKey)
  for _,i in pairs(self.cells[key] and self.cells[key].indexes or {}) do
    local g = self.cbrs_grants[i]
    if not g or g.operationState ~= 'AUTHORIZED' then return false end
    authorized = true
  end
  return authorized
end

-- true if 'val' is not changed for last 'limit' seconds
function SasClient:rateLimit(key, val, limit)
  if not self.rateLimits[key] then self.rateLimits[key] = {ts=os.time()} end
  if self.rateLimits[key].val ~= val then
    self.l.log("LOG_NOTICE", string.format("%s() %s -> %s", key, self.rateLimits[key].val, val))
  elseif os.time() - self.rateLimits[key].ts < limit then
    return true
  end
  self.rateLimits[key] = { val = val, ts = os.time() }
end

-- primary cell changed
function SasClient:pCellChanged(key, val)
  if self.ssm.get_current_state() ~= "sas:operate" then return end
  if not self.ctx.state_machine or self.ctx.state_machine.rstate ~= 'Registered' then return end

  if self:rateLimit("pCellChanged", val, 30) then return end
  self:update_grants()

  -- turn off transmission if not authorized
  if not self:is_authorized() and not self.apnTimer then
    self:update_sas_transmit(false)
  end
end

-- neighbour cells changed
function SasClient:cell_list_changed(key, val)
  if self.ssm.get_current_state() ~= "sas:operate" then return end

  if not self.ctx.state_machine or self.ctx.state_machine.rstate ~= 'Registered' then return end

  -- Sending request during ip handover will fail, check and wait until it's ready.
  local sas_config_ip_handover = self.rdb.get('sas.config.ip_handover') == '1'
  local transmit_enabled = self.rdb.get('sas.transmit_enabled') == '1'
  if transmit_enabled and sas_config_ip_handover then
    local last_wwan_ip = self.rdb.get("service.ip_handover.last_wwan_ip") or ''
    local ip_handover_ready = self.rdb.get("service.ip_handover.enable") == "1" and last_wwan_ip ~= ''
    if not ip_handover_ready then
      self.l.log("LOG_NOTICE", "postpone neighbour cell grant request until IP handover is ready")
      return
    end
  end

  if self:rateLimit("cell_list_changed", val, 30) then return end
  self:update_grants()
end

-- re-initialise HTTPClient if CBRS APN IP address has been handed over to LAN side
function SasClient:ip_handover_completed(key, val)
  local sas_config_ip_handover = self.rdb.get('sas.config.ip_handover') == '1'
  local handover_profile = self.rdb.get("service.ip_handover.profile_index")
  local sas_pdp_profile = self.rdb.get("sas.config.pdp_profile")
  if sas_config_ip_handover and handover_profile == sas_pdp_profile then
    local last_wwan_ip = self.rdb.get("service.ip_handover.last_wwan_ip") or ''
    if self.ip_handover_last_wwan_ip ~= last_wwan_ip and self.ctx.client then
      self.l.log("LOG_NOTICE", string.format("IP handover: last_wwan_ip:%s->%s, rebuild http client",
        self.ip_handover_last_wwan_ip, last_wwan_ip))
      self.ctx.client = nil
      self.sas:init_client_http(self.ctx)
    end
    self.ip_handover_last_wwan_ip = last_wwan_ip
  end
end

-- change grant state rdb from authorized to granted
function SasClient:set_rdb_grant_authorised_to_granted(reason)
  -- update internal grant table state if exists, to ensure it's consistent with rdb
  local grants = self.ctx and self.ctx.state_machine and self.ctx.state_machine.grants or {}
  for k,v in pairs(grants) do self:no_xmit(k) end

  for i = 0, self.MAX_SAS_GRANT_INDEX do
    local state = self.rdb.get(string.format('sas.grant.%d.state', i))
    if state == 'AUTHORIZED' then
      self.rdb.set(string.format('sas.grant.%d.state', i), 'GRANTED')
      self.l.log("LOG_NOTICE", string.format('sas.grant.%s.state AUTHORIZED -> GRANTED, reason:%s', i, reason))
    end
  end
end

-- network attached status changed
function SasClient:network_attach_changed(key, val)
  local state = self.ssm.get_current_state()
  if self:rateLimit("network_attach_changed", val, 30) then return end

  if val == '1' then
    self.attach_rejected = nil
    self.last_detached_timestamp = nil
    if state == "sas:idle" or state == "sas:attach" then
      self.ssm.switch_state_machine_to("sas:wait_ip_interface")
    else self:pCellChanged() end
    return
  end

  -- detached, check & clear PCI lock if network remains detached after 10s
  if not self.last_detached_timestamp then
    self.last_detached_timestamp = os.time()
  else
    local detach_period = os.time() - self.last_detached_timestamp
    local pci_lock = self.rdb.get(self.lock_list_rdb) or ''
    if detach_period > self.DETACH_TIME_LIMIT then
      if pci_lock ~= '' then
        self.l.log("LOG_NOTICE", string.format("network_attach_changed() detach_period=%s", detach_period))
        self:set_pcilock('')
        self.rateLimits = {}
      end
      -- reset grant state to GRANTED if serving cell has missed the scheduled heartbeat
      if self:is_authorized() then
        for _,g in pairs(self:serving_cell_grants()) do
          if not g.nextHeartbeat or self:seconds_to_event(g.nextHeartbeat) < 0 then
            self:set_rdb_grant_authorised_to_granted(string.format("detached more than %ss", detach_period))
            break
          end
        end
      end
    end
  end

  if state ~= "sas:idle" and state ~= "sas:attach" then
    self.ssm.switch_state_machine_to("sas:idle")
  end
end

-- set ecgi list info for given cell
function SasClient:set_ecgi_list(cinfo, ecgi)
  if not cinfo then return end

  if not ecgi then ecgi = self.rdb.get(self.ecgi_rdb) or '' end
  ecgi = ecgi:split('|')

  -- find pci and earfcn of the cell
  local pci, earfcn = cinfo.pci, cinfo.earfcn

  -- search param ecgi for matching pci & earfcn then extract ecgi info
  if pci and earfcn and #ecgi > 0 then
    local ecgi_list = ''
    local sep = ''
    for i=1, #ecgi do
      local info = ecgi[i]:split(',') -- info={earfcn,pci,ecgi}
      if #info == 3 and earfcn == info[1] and pci == info[2] then
        ecgi_list = ecgi_list .. sep .. info[3]
        sep = ','
      end
    end
    if not cinfo.ecgi_list or cinfo.ecgi_list ~= ecgi_list then
      cinfo.ecgi_list = ecgi_list
    end
  end
end

-- ecgi info changed, update cell's ecgi_list
function SasClient:ecgi_changed(key, val)
  if not val or val == '' then return end

  for _,cell in pairs(self.cells) do
    self:set_ecgi_list(cell.info, val)
  end
end

function SasClient:cbrs_up_config_stage_change(key, val)
  local state = self.ssm.get_current_state()
  self.l.log("LOG_NOTICE", string.format("cbrs_up_config_stage_change(key:%s, val:%s): state:%s", key, val, state))
  if state == 'sas:wait_ip_interface' then self:state_machine_wait_ip_interface() end
end

-- cell measurement changed, update PCI list to include/exclude cell base on RSRP
function SasClient:cell_measurement_change(key, val)
  if self.ssm.get_current_state() ~= "sas:operate" then return end
  self:update_pci_lock_list()
end

function SasClient:intra_cells_changed(key, val)
  if self.ssm.get_current_state() ~= "sas:operate" then return end
  if not self.ctx.state_machine or self.ctx.state_machine.rstate ~= 'Registered' then return end

  local p_pci, p_earfcn = self.rdb.get(self.serv_pci_rdb), self.rdb.get(self.pEarfcnKey)
  local intra_cells = (val or ''):split(';')
  if #intra_cells < 1 then return end
  for i=1, #intra_cells do
    local cell = intra_cells[i]:split(',')
    local pci, earfcn = cell[1], cell[2]  -- format: pci,earfcn,bw,low,high
    if p_earfcn ~= earfcn then return end -- invalid: by def intra cell must have same earfcn with serving cell
    self:update_pci_list(pci, earfcn)
  end

  -- update timestamp on serving cell's PCI to avoid being outdated by intra cells
  self:update_pci_list(p_pci, p_earfcn)

  -- force inter-cells to update their timestamps on next rdb update
  self.rateLimits = {}
  self:cell_set_rdb()
end

function SasClient:schedule(func)
  if self.state_schedule == 'pause' then
    self.saved_func = func
  else
    func()
  end
end

function SasClient:run(key, val)
  if tonumber(val) == 1 then
    self.state_schedule = 'run'
    if self.saved_func then
       self.saved_func()
       self.saved_func = false
    end
    self.rdb.set(key, '0')
  end
end

SasClient.cbs_system={
  "on_sas_response",
}

function SasClient:init()
  -- 1. create state machine
  -- create main state machine
  self.ssm = self.smachine.new_smachine("sas_smachine", self.stateMachineHandlers)

  -- 2. add invoke handlers
  -- add sas client callbacks
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sas", v, self, v)
  end

  -- 3. initial setting & add rdb handlers
  -- set device serial to product serial number
  local productSn=self.rdb.get('system.product.sn')
  self.rdb.set('sas.config.cbsdSerialNumber', productSn)

  -- clear lock at startup
  self:set_pcilock('')
  -- all authorised grants downgrade to granted at boots up (mag-1070)
  self:set_rdb_grant_authorised_to_granted('sas client starts up')

  -- if we were in the middle of registering and rebooted or got disabled/enabled
  -- restart from scratch
  local reg_state = self.rdb.get(self.registration_state_rdb)
  if reg_state == "Registering" then
    self.rdb.set(self.registration_state_rdb, "Unregistered")
  end

  self.rdb.set(self.cellListKey, '')
  self.rdb.set('sas.authorization_count', '0')
  self.rdb.set("sas.transmit_enabled", '0')

  self:read_sas_timer_settings()
  self.sas.ctx['timeouts'] = {}
  self.ctx = self.sas.ctx

  -- 4. add rdb handlers
  self.rdbWatch:addObserver("sas.run", "run", self)
  self.rdbWatch:addObserver("sas.command", "command", self)
  self.rdbWatch:addObserver(self.registrationCmdKey, "registration_cmd", self)
  self.rdbWatch:addObserver(self.cellListKey, "cell_list_changed", self)
  self.rdbWatch:addObserver(self.attach_rdb, "network_attach_changed", self)
  self.rdbWatch:addObserver(self.ecgi_rdb, "ecgi_changed", self)
  self.rdbWatch:addObserver(self.nit_connected_rdb, "nit_connected_status_change", self)
  self.rdbWatch:addObserver("sas.cbrs_up_config_stage", "cbrs_up_config_stage_change", self)
  self.rdbWatch:addObserver(self.intra_cells_rdb, "intra_cells_changed", self)
  self.rdbWatch:addObserver("wwan.0.system_network_status.ca.enabled", "ca_mode_changed", self)
  if self.exclude_weak_cell then
    self.rdbWatch:addObserver("wwan.0.manual_cell_meas.qty", "cell_measurement_change", self)
    self.rdbWatch:addObserver("wwan.0.cell_measurement.qty", "cell_measurement_change", self)
  end

  -- watch bandwidth rdb to ensure all cell info are ready (see qdiagd.c)
  self.rdbWatch:addObserver(self.dl_bandwidth_rdb, "pCellChanged", self)

  -- ip handover template will trigger alias ip rdb at the very end, observe it to update http client
  self.rdbWatch:addObserver("service.alias_ip_trigger", "ip_handover_completed", self)


  -- 5. switch to initial state
  -- start initial state machine
  self.ssm.switch_state_machine_to("sas:idle")
end

return SasClient

