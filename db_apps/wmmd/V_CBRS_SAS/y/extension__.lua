--[[
  Decorator for Cbrs

  Copyright (C) 2019 NetComm Wireless limited.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")
local CbrsDecorator = WmmdDecorator:new()
local QmiGDecorator = WmmdDecorator:new()
local QmiNasDecorator = WmmdDecorator:new()

-- the config band changed, set the default profile accordingly
-- rdbKey: the key changed, always "wwan.0.currentband.config"
-- rdbVal: the new rdb value
function QmiGDecorator:set_default_profile(rdbKey, rdbVal)
  local profile_type = {
    ['embedded'] = 0,
    ['socket'] = 1,
  }
  self.l.log("LOG_NOTICE", string.format("band config:'%s'", rdbVal))
  if not rdbVal then
    self.l.log("LOG_ERR", 'set default profile, rdbVal is nil')
    return
  end

  local ptcrb = self.rdb.get("sw.ptcrb") or "0"
  local sas_enabled_ia = self.rdb.get("service.sas_client.enabled_by_ia") or "0"
  if sas_enabled_ia == "0" then ptcrb = 1 end
  if ptcrb == "1" then
    rdbVal = self.optBandstr
  end

  if rdbVal ~= self.optBandstr and rdbVal ~= self.b48str then
    self.l.log("LOG_ERR", string.format("invalid band config '%s'", rdbVal))
    return
  end

  local band_num = tonumber(string.match(rdbVal, "%d+"))
  local profile_index = self.default_profile_number[band_num]
  -- TODO the profiles below should not be hard coded, but based on variables, and preferably
  -- the profile values should be based on the variant.
  -- This does not affect myna, but is incorrect if we have any variant except magpie.
  -- This is a magpie specific condition.
  local restore_index = profile_index == 1 and 5 or 1

  local apn_type = self.rdb.get(string.format("link.profile.%d.apn_type", profile_index))
  local apn_type1 = apn_type:split(",")[1]
  self.l.log("LOG_NOTICE", string.format("profile:'%d', apn type:'%s', apn type[1]:'%s'", profile_index, apn_type, apn_type1))
  if apn_type1 ~= "default" then
    self.l.log("LOG_NOTICE", string.format("setting default profile to %d", profile_index))
    -- set the apn as default first, then modify the supl profile
    -- revoke the previous default apn
    self.rdb.set(string.format("link.profile.%d.apn_type", restore_index), "")
    self.rdb.set(string.format("link.profile.%d.writeflag", restore_index), 1)

    -- set the new default apn
    self.rdb.set(string.format("link.profile.%d.apn_type", profile_index), "default")
    self.rdb.set(string.format("link.profile.%d.writeflag", profile_index), 1)
    -- Enabling/disabling profile 1 and 5 is done by external template, 50_cbrs_sas.template
    -- which is triggered by same RDB variable, wwan.0.currentband.config

    -- now modify the supl profile
    self.l.log("LOG_NOTICE", string.format("set supl profle number, setting %d as supl", profile_index))
    self.luaq.set_default_profile_num(profile_type['embedded'], profile_index)
    self.luaq.set_default_profile_num(profile_type['socket'], profile_index)
  end

  -- enable sas client on band 48
  local enable = band_num == 48 and '1' or '0'
  if self.rdb.get('service.sas_client.enable') ~= enable then
    self.rdb.set('service.sas_client.enable', enable)
  end
end

function QmiGDecorator:attach_changed(key, val)
  -- continue with qmicommand setCAmode
  local cmd_key = "service.luaqmi.command"
  local cmd_val = self.rdb.get(cmd_key)
  if val == '0' and cmd_val:match("setCAmode") then
    self:qmiCommand(cmd_key, cmd_val)
  end
end

function QmiGDecorator:qmiCommand(key, val)
  local handlers = {
    ['setCAmode'] = function(args)
      -- Change CA mode via rdb eg. service.luaqmi.command="setCAmode,1"
      local enable = tonumber(args[2])
      local ca_enabled = tonumber(self.rdb.get("wwan.0.system_network_status.ca.enabled"))
      if enable == ca_enabled then
        self.l.log("LOG_NOTICE", string.format("setCAmode(%s), last set: %s. Ignore.", enable, ca_enabled))
        self.rdb.set(key, "")
        return
      end

      -- currently setting CA mode only allowed when modem is detached
      if self.rdb.get("wwan.0.system_network_status.attached") == '1' then
        self.l.log("LOG_NOTICE", "setCAmode() temporary detach modem to change CA mode")
        self.attach_required = true
        self:detach_modem()
        return  -- continue when attach state changes
      end

      local qm = self.luaq.new_msg(self.luaq.m.QMI_NAS_LTE_UE_CONFIG)
      qm.req.disable_ca = enable == 0
      qm.req.disable_ca_valid = true
      if not self.luaq.send_msg(qm) then
        self.l.log("LOG_ERR", "setCAmode: failed to post QMI QMI_NAS_LTE_UE_CONFIG")
        return
      end
      local succ, qerr = self.luaq.ret_qm_resp(qm)
      if not succ then
        self.l.log("LOG_ERR", string.format("setCAmode: QMI_NAS_LTE_UE_CONFIG request failed, error 0x%04x", qerr))
      else
        self.rdb.set("wwan.0.system_network_status.ca.enabled", enable)
      end
      self.l.log("LOG_NOTICE", string.format("setCAmode() done, enable:%s succ:%s", enable, succ))
      if self.attach_required then self:attach_modem() self.attach_required = nil end
      self.rdb.set(key, "")
    end,

    ['setTxPower'] = function(args)
      local command = args[1]
      local theBand = tonumber(args[2])
      local power_dBm = tonumber(args[3])
      local disableCutback = tonumber(args[4])
      if not theBand or not power_dBm or not disableCutback then
        self.l.log("LOG_ERR", string.format("%s: invalid args %s", command, table.concat(args,",")))
        return
      end
      local err, errcode, resp, msg = self.qs.qmi_netcomm:setCbrsTxPowerCutback(theBand, power_dBm, disableCutback)
      if not err then
        self.l.log("LOG_ERR", 'set cbrs tx power failed, error code: '..tostring(errcode))
      else
        local curr_max_tx_power = self.rdb.get("sas.antenna.last_eirp") or 0
        if curr_max_tx_power ~= 0 then
            self.rdb.set("sas.antenna.last_eirp", power_dBm)
        end
      end
      self.l.log("LOG_NOTICE",
        string.format("%s: %s, current max eirp: %s", table.concat(args,","),
          err and 'success' or 'failed', tostring(resp.power_cutback_actual_db10)))
    end
  }

  local args = val:split(',')
  local command = args[1]
  if not handlers[command] then
    if val ~= "" then self.l.log("LOG_ERR", 'QmiNetComm, command unknown:'..val) end
    return
  end

  handlers[command](args)
end


function QmiGDecorator:state_machine_powerup(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:powerup",self.magpie_powerup_retry_interval)

  local bandConfig = self.rdb.get(self.currBandCfgRdb) or 'notset'
  self.l.log("LOG_NOTICE", string.format("current band config: [%s]", bandConfig))

  local ptcrb = self.rdb.get("sw.ptcrb") or "0"

  -- if sas client service is to be disabled then piggy back on ptcrb mode
  local sas_enabled_ia = self.rdb.get("service.sas_client.enabled_by_ia") or "0"
  if sas_enabled_ia == "0" then ptcrb = 1 end

  if bandConfig ~= self.optBandstr and bandConfig ~= self.b48str and ptcrb ~= "1" then
    -- to kick watchdog
    self.watcher.invoke("sys","poll_operating_mode")
    self.l.log("LOG_NOTICE", string.format("Band '%s' is not supported, modem will remain offline", bandConfig))
    return
  end

  self:set_default_profile(nil, bandConfig)

  -- switch Modem to online,
  -- do not attach if on band 48
  -- attach on band 30, but check band config in modem first to avoid incorrect attach to B48
  -- if ptcrb mode enabled, attach and proceed
  local wmmd_mode = self.rdb.get("wwan.0.operating_mode") or ""
  if wmmd_mode ~= "online" then
    self.watcher.invoke("sys","online")
  end
  self:detach_modem()

  if ptcrb ~= "1" then
    -- ensure modem selected band is same as rdb
    local modemBand = self.qs.qmi_nas:bandPrefGet()
    self.l.log("LOG_NOTICE", string.format('modem band: [%s]', modemBand))

    if modemBand ~= bandConfig then
      local result = self.qs.qmi_nas:bandPrefSet(bandConfig)
      self.l.log("LOG_NOTICE",
        string.format('setting band pref from [%s] to [%s], result: %s', modemBand, bandConfig, result))
    end

    if bandConfig == self.b48str then
        self.l.log("LOG_NOTICE", "[B48] requested modem detach after online")
        return true
    end
  end
  self:attach_modem()

  return true
end


function QmiGDecorator:tx_control(rdbKey, rdbValue)
  local tx_state = tonumber(rdbValue)
  -- change the tx power before detach or attach
  local antenna_gain = self.rdb.get('sas.antenna.gain')
  local disableCutback = 0  -- eanble the cutback
  local txPower = 0
  local band = 48
  if tx_state == 1 then
     -- read the tx power from the rdb
     txPower = tonumber(self.rdb.get('wwan.0.txctrl.tx_power_high'))
  else
     txPower = tonumber(self.rdb.get('wwan.0.txctrl.tx_power_low'))
  end
  txPower = txPower - antenna_gain

  local rdbBand = self.rdb.get("wwan.0.currentband.config")
  if rdbBand and rdbBand ~='' then
     band = tonumber(string.match(rdbBand, "%d+"))
  end

  if band then
     -- set the modem's tx power
     local err, errcode, resp, msg = self.qs.qmi_netcomm:setCbrsTxPowerCutback(band, txPower, disableCutback)
     if not err then
        self.l.log("LOG_ERR", 'tx_control, set cbrs tx power failed, error code: '..tostring(errcode))
     end
     self.l.log("LOG_NOTICE",string.format("tx_control, %s, current max eirp: %s",
          err and 'success' or 'failed', tostring(resp.power_cutback_actual_db10)))
  end

  -- make sure the value only can be 0 or 1
  local tx_state = tonumber(rdbValue)
  if tx_state == 0 then
     self:detach_modem()
  end
end

-- convert the bandwidth in RBs into Mhz
function convertRbsIntoMhz(rbs)
  local rbtab ={[100]=20, [75]=15, [50]=10, [25]=5, [15]=3, [6]=1.4}
  return rbtab[rbs]
end

-- Convert B48 earfcn to low and high frequncy for the given cell
-- cell : table of {earfcn, bw, <any others>}
-- return : the low and high frequency for the earfcn
function get_freq_range(cell)
  -- For B48
  local freq = 3550 + 0.1 * (cell.earfcn - 55240)
  return (freq - cell.bw/2), (freq + cell.bw/2)
end

-- update inter-frequency cell list
-- It first sorts/removes the cells based on criteria provided by AT&T and the requests grants
-- cell_list : the list of cells to request grant on
function QmiNasDecorator:update_inter_cell_list(cell_list)
  local grant_req_list = {}
  for i = 1, #cell_list do
    local f_low, f_high = get_freq_range(cell_list[i])
    table.insert(grant_req_list, {pci=cell_list[i].pci, earfcn=cell_list[i].earfcn,
      bw=cell_list[i].bw, l=f_low, h=f_high})
  end

  -- save the list of neighbour cells filtered for grant request
  local last_neighbour_grant_req = self.rdb.get("sas.inter_cells") or ""
  local curr_neighbour_grant_req = ""
  local sep = ""
  for i=1,#grant_req_list  do
    curr_neighbour_grant_req = curr_neighbour_grant_req..sep..string.format("%s,%s,%s,%s,%s",
      grant_req_list[i].pci, grant_req_list[i].earfcn, grant_req_list[i].bw,
      grant_req_list[i].l, grant_req_list[i].h)
    sep = ';'
  end
  if last_neighbour_grant_req ~= curr_neighbour_grant_req then
    self.l.log("LOG_NOTICE", string.format("last neighbours = '%s'", last_neighbour_grant_req))
    self.l.log("LOG_NOTICE", string.format("curr neighbours = '%s'", curr_neighbour_grant_req))
  end
  -- set neighbour list even if it does not change
  self.rdb.set("sas.inter_cells", curr_neighbour_grant_req)
end

-- construct the neighbour list  according to RDB: wwan.0.cell_measurement.ncell.current
-- format of ncell.current:  earfcn,pci,bw,rsrp,rsrq|earfcn,pci,bw,rsrp,rsrq
-- here is an example: 55540,234,50,-70.5,-4.5|55690,317,50,-70.8,-4.8|55390,0,0,-69.5,-3.6
function QmiNasDecorator:construct_neighbour_cell_list(rdbKey, rdbVal)
  if not rdbVal or rdbVal == '' then
    self.l.log("LOG_NOTICE",string.format("No Neighbour cells seen by modem"))
    rdbVal = ''
  end
  local p_earfcn = self.rdb.get("wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq")
  local p_pci = self.rdb.get("wwan.0.radio_stack.e_utra_measurement_report.servphyscellid")
  local intra_cells = {} -- cells with same EARFCN as serving cell
  local inter_cells = {} -- inter-frequency cells, aka sib5 cells
  local ctable = rdbVal:split('|')
  for i=1, #ctable do
    local cell = ctable[i]:split(',')
    if #cell == 5 then
      local bw_freq = convertRbsIntoMhz(tonumber(cell[3]))
      local serving_cell = p_earfcn == cell[1] and p_pci == cell[2]
      if bw_freq and not serving_cell then
        local cells = (cell[1] == p_earfcn and cell[2] ~= p_pci) and intra_cells or inter_cells
        table.insert(cells, {earfcn=tonumber(cell[1]), pci=tonumber(cell[2]), bw=bw_freq})
      end
    end
  end

  self:update_inter_cell_list(inter_cells)

  -- update intra cell list
  local new_intra = ""
  for i=1, #intra_cells do
    new_intra = new_intra..(i==1 and '' or ';')..string.format("%s,%s,%s,%s,%s",
      intra_cells[i].pci, intra_cells[i].earfcn, intra_cells[i].bw, get_freq_range(intra_cells[i]))
  end
  local intra_rdb = 'sas.intra_cells'
  local old_intra = self.rdb.get(intra_rdb)
  if old_intra ~= new_intra then
    self.l.log("LOG_NOTICE", string.format("%s: '%s' -> '%s'", intra_rdb, old_intra, new_intra))
    self.rdb.set(intra_rdb, new_intra)
  end
end

-- get bandwidth given cell frequency, this implementation supports intra-cells
function QmiNasDecorator:getBandwidth(freq, table)
  local bw = QmiNasDecorator:__invokeChain("getBandwidth")(self, freq, table)
  if bw ~= "0" then return bw end

  -- intra-cell has no entry in the table which was built from SIB5 message, get bandwidth via other rdb
  local pcell_bw = self.rdb.get("wwan.0.radio_stack.e_utra_measurement_report.dl_bandwidth")
  local pcell_freq = tonumber(self.rdb.get("wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq"))
  if freq == pcell_freq and pcell_bw then return pcell_bw end

  -- other source of cell bandwidth
  local MhzToRBs = {[20]=100, [15]=75, [10]=50, [5]=25, [3]=15, [1.4]=6}
  if freq == tonumber(self.rdb.get("wwan.0.system_network_status.lte_ca_scell.freq")) then
    local mhz = tonumber(string.match(self.rdb.get("wwan.0.system_network_status.lte_ca_scell.bandwidth"), "(%d+)"))
    bw = MhzToRBs[mhz]
    if bw then return bw end
  end

  for i=0,4 do
    local prefix = string.format("wwan.0.radio_stack.e_utra_measurement_report.scell[%s].",i)
    local sfreq = tonumber(self.rdb.get(prefix..'freq'))
    local sMHz = tonumber(self.rdb.get(prefix..'dl_bandwidth'))
    if sMHz and sfreq== freq then
      bw = MhzToRBs[sMHz]
      if bw then return bw end
    end
  end

  -- if still not having bandwidth then workout by using cell offset with a known cell, see:
  -- https://confluence.netcommwireless.com/display/MAG/SAS+CBSD+Client+Grant+Management+For+ULCA
  -- @f - earfcn of unknown cell
  -- @cell_f, @cell_bw - earfcn and bandwidth of known cell
  local bw_by_offset = function(f, cell_f, cell_bw)
    if not cell_f or not cell_bw then return end
    local to_freq = function(earfcn) return 3550 + (0.1 * (earfcn - 55240)) end

    -- offset or diff between center frequencies of the 2 cells
    local offset = string.format("%.2f",math.abs(to_freq(f) - to_freq(cell_f)))

    local bw
    if cell_bw == "50" then
      bw = offset == "9.90" and "50" or offset == "14.40" and "100" or nil
    elseif cell_bw == "100" then
      bw = offset == "14.40" and "50" or offset == "19.80" and "100" or nil
    end
    if bw then
      self.l.log("LOG_DEBUG", string.format("bw_by_offset(f:%s|%s c[%s|%s, %s]) offset:%s bw=%s",
        f, to_freq(f), cell_f, to_freq(cell_f), cell_bw, offset, bw))
    end
    return bw
  end

  -- bandwidth from offset with serving cell
  bw = bw_by_offset(freq, pcell_freq, pcell_bw)
  if bw then return bw end

  -- bandwidth from offset with existing neighbour cells
  local ncell = self.rdb.get("wwan.0.cell_measurement.ncell.current") or ''
  local ctable = ncell ~= '' and ncell:split('|') or {}
  for i=1, #ctable do
    local cell = ctable[i]:split(',')
    bw = bw_by_offset(freq, tonumber(cell[1]), cell[3])
    if bw then return bw end
  end

  return "0"
end

function QmiGDecorator:init()
  QmiGDecorator:__invokeChain("init")(self)
  self.currBandCfgRdb = "wwan.0.currentband.config"
  self.magpie_powerup_retry_interval=15000
  self.optBandstr = "LTE Band 41 - TDD 2500"
  self.b48str = "LTE Band 48 - TDD 3600"
  self.default_profile_number = {
    [41] = 1,  -- 'band 41, the default profile: cbrssas_apn_number'
    [48] = 1,  -- 'band 48, the default profile: cbrssas_apn_number'
  }
  self.rdbWatch:addObserver(self.currBandCfgRdb, "set_default_profile", self)
  self.rdbWatch:addObserver("service.luaqmi.command", "qmiCommand", self)
  self.rdbWatch:addObserver("wwan.0.txctrl.tx_state", "tx_control", self)
  self.rdbWatch:addObserver("wwan.0.system_network_status.attached", "attach_changed", self)
end

function QmiGDecorator:doDecorate()
  QmiGDecorator:__saveChain("init")
  QmiGDecorator:__changeImplTbl({
    "init",
    "set_default_profile",
    "state_machine_powerup",
    "qmiCommand",
    "attach_changed",
    "tx_control"})
end

function QmiNasDecorator:init()
  -- add SIB Indication callback
  QmiNasDecorator:__invokeChain("init")(self)
  -- clear the pci lock
  self.rdb.set("wwan.0.neighbour_lock_list", "")
  -- monitor the SIB5 neighbour cell info
  self.rdbWatch:addObserver("wwan.0.cell_measurement.ncell.current", "construct_neighbour_cell_list", self)
end

function QmiNasDecorator:doDecorate()
  QmiNasDecorator:__saveChain("init")
  QmiNasDecorator:__saveChain("getBandwidth")
  QmiNasDecorator:__changeImplTbl({
    "init",
    "update_inter_cell_list",
    "construct_neighbour_cell_list",
    "getBandwidth"})
end

function CbrsDecorator.doDecorate()
  CbrsDecorator.__inputObj__.qmiG = QmiGDecorator:decorate(CbrsDecorator.__inputObj__.qmiG)
  CbrsDecorator.__inputObj__.qmiG.qs.qmi_nas = QmiNasDecorator:decorate(CbrsDecorator.__inputObj__.qmiG.qs.qmi_nas)
end

return CbrsDecorator
