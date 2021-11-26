-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- main QMI module that initiates QMI_*.lua

require("tableutil")
require("rdbobject")

local QmiController = require("wmmd.Class"):new()

-- Whether to start WMMD GPS session or not. Is overridden by the V_WMMD_GPS
-- extention if enabled. Note that this is only for app level reading of GPS
-- data to be stored in RDB. Modem initiated GPS fixes, notably for E911, is
-- not affected regardless of this setting.
QmiController.start_gps = false

function QmiController:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_dms", "LOG_DAEMON") end)

  self.t = require("turbo")
  self.ffi = require("ffi")

  self.luaq = require("luaqmi")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.dconfig = dConfig
  self.wrdb = wrdb
  self.rdb = require("luardb")
  self.rdbWatch = rdbWatch
  self.util = require("wmmd.util")

  self.i = self.t.ioloop.instance()
  self.sq = self.smachine.get_smachine("qmi_smachine")

  -- qmi modules
  self.l.log("LOG_INFO", "Loading QMI modules")

#ifdef V_PROCESSOR_sdx20
  self.services = {
    qmi_nas = "wmmd.qmi_nas",
    qmi_wds = "wmmd.qmi_wds",
    qmi_dms = "wmmd.qmi_dms",
    qmi_uim = "wmmd.qmi_uim",
    qmi_wms = "wmmd.qmi_wms",
  }
  self.essential_services = {"wds", "nas", "dms", "uim", "wms"}
#elif defined V_PROCESSOR_sdx55
  self.services = {
    qmi_nas = "wmmd.qmi_nas",
    qmi_wds = "wmmd.qmi_wds",
    qmi_dms = "wmmd.qmi_dms",
    qmi_uim = "wmmd.qmi_uim",
#ifndef V_QMI_WMS_none
    qmi_wms = "wmmd.qmi_wms",
#endif
    qmi_dsd = "wmmd.qmi_dsd",
    qmi_loc = "wmmd.qmi_loc",
  }

#ifndef V_QMI_WMS_none
  self.essential_services = {"wds", "nas", "dms", "uim", "wms", "dsd", "loc"}
#else
  self.essential_services = {"wds", "nas", "dms", "uim", "dsd", "loc"}
#endif

#else
  self.services = {
    qmi_nas = "wmmd.qmi_nas",
    qmi_wds = "wmmd.qmi_wds",
    qmi_dms = "wmmd.qmi_dms",

#ifndef V_QMI_VOICE_none
    qmi_voice = "wmmd.qmi_voice",
#endif

#ifndef V_QMI_CSD_none
    qmi_csd = "wmmd.qmi_csd",
#endif

    qmi_uim = "wmmd.qmi_uim",
    qmi_loc = "wmmd.qmi_loc",

#ifndef V_QMI_WMS_none
    qmi_wms = "wmmd.qmi_wms",
#endif

#ifndef V_QMI_IMS_none
    qmi_ims = "wmmd.qmi_ims",
#endif

    qmi_netcomm = "wmmd.qmi_netcomm",
    qmi_tmd = "wmmd.qmi_tmd",
    qmi_ssctl = "wmmd.qmi_ssctl",

#ifndef V_QMI_PBM_none
    qmi_pbm = "wmmd.qmi_pbm",
#endif

    qmi_fds = "wmmd.qmi_fds",
  }

  self.essential_services = {
    "wds",
    "nas",
#ifndef V_QMI_VOICE_none
    "voice",
#endif
    "dms"
  }
#endif

  self.blackmap ={
    ims = 'qmi_ims',
    netcomm = 'qmi_netcomm',
    csd = 'qmi_csd',
  }

  self.exclusions = {}

  for _,v in ipairs(global_blacklist) do
    if (self.blackmap[v]) then
      self.l.log("LOG_INFO", "Blacklisting (" .. v .. "|" .. self.blackmap[v] .. ")")
      self.exclusions[self.blackmap[v]] = 1
    else
      self.l.log("LOG_INFO", "No blackmap entry for " .. v)
    end
  end

  self.qs = {}

  for i,v in pairs(self.services) do
    if not self.exclusions[i] then
      self.l.log("LOG_INFO", "Loading [" .. i .. "]=" .. v)
      self.qs[i] = require(v):new()
      self.qs[i]:setup(self.rdbWatch, self.wrdb, self.dconfig)
    end
  end

  -- constant variables
  self.qmi_connect_retry_interval = 10000 -- qmi connection retry period
  self.qmi_idle_interval = 10000 -- periodic timer in idle state

  self.wmmd_register_retry_interval = 30000
  self.wmmd_online_retry_interval=30000
  self.wmmd_attach_retry_interval=30000
  self.wmmd_deatach_retry_interval=30000
  self.wmmd_lock_detach_retry_interval=5000
  self.wmmd_lock_retry_interval=30000
  self.wmmd_connect_retry_interval=30000
  self.wmmd_read_retry_interval=30000
  self.wmmd_powerup_retry_interval=30000
  self.wmmd_simpin_retry_interval=10000
  self.wmmd_phonebook_retry_interval=10000

  self.qmi_fd_qies={} -- self-pipe handles of qmi services
  self.modem_poll_ref = nil -- modem poll ref of turbo
  self.modem_poll_quick_ref = nil -- modem poll ref of turbo
  self.qg={}

  self.ext_network_state_rdbs={
    -- lte only
    ["eci"]="system_network_status.ECI",
    ["enb_id"]="system_network_status.eNB_ID",
    ["tac"]="radio.information.tac",
    -- gsm and umts
    ["lac"]="system_network_status.LAC",
    ["rac"]="system_network_status.RAC",
    ["cid"]="system_network_status.CID",
    -- umts only
    ["rncid"]="system_network_status.RNC_ID",
    ["lcid"]="system_network_status.LCID",
    -- nr5g only
    ["ncgi"]="radio_stack.nr5g.cgi",
    ["nr5g_tac"]="radio_stack.nr5g.tac",
    -- common
    ["network_mcc"]="system_network_status.MCC",
    ["network_mnc"]="system_network_status.MNC",
    ["plmn"]="system_network_status.PLMN",
    ["cellid"]="system_network_status.CellID", -- general cell id
    ["endc"]="system_network_status.endc_avail",
  }

  self.signal_info_rdbs={
    ["rssi"]="signal.rssi",
    ["rsrq"]="signal.rsrq",
    ["rsrp"]="signal.0.rsrp",
    ["snr"]="signal.snr",
    ["ecio"]="radio.information.ecio",
    -- matching to the exsiting R&D qdiag RDBs
    ["nr5g_rsrp"]="radio_stack.nr5g.rsrp",
    ["nr5g_snr"]="radio_stack.nr5g.snr",
    ["nr5g_rsrq"]="radio_stack.nr5g.rsrq"
  }

  self.app_state_to_sim_status = {
    ["unknown"]="SIM BUSY",
    ["detected"]="SIM BUSY",
    ["pin1_or_upin_req"]="SIM PIN",
    ["puk1_or_puk_req"]="SIM PUK",
    ["person_check_req"]="SIM ERR",
    ["pin1_perm_blocked"]="SIM BLOCKED",
    ["illegal"]="SIM ERR",
    ["ready"]="SIM OK",
  }

  --[[ RDBs ]]--
  self.rdb_ims_reg_cmd = self.config.rdb_g_prefix .. "ims.register.command"
  self.rdb_ims_test_mode = self.config.rdb_g_prefix .. "ims.register.test_mode"

  -- ims reset state
  self.performing_ims_reset = nil
  -- ims test mode
  self.ims_test_mode_enable = false

  -- current ims registered status
  self.ims_registered=false
  self.modem_ims_reg_stats = {}

  self.stateMachineHandlers = {
    {state="qmis:idle", func="state_machine_idle", execObj=self},
    {state="qmis:initiate",func="state_machine_initiate", execObj=self},
    {state="qmis:prereadmodem",func="state_machine_prereadmodem", execObj=self},
    {state="qmis:simpin",func="state_machine_simpin", execObj=self},
    {state="qmis:phonebook",func="state_machine_phonebook", execObj=self},
    {state="qmis:powerup",func="state_machine_powerup", execObj=self},
    {state="qmis:readmodem",func="state_machine_readmodem", execObj=self},
    {state="qmis:register",func="state_machine_register", execObj=self},
    {state="qmis:prelock",func="state_machine_prelock", execObj=self},
    {state="qmis:lock",func="state_machine_lock", execObj=self},
    {state="qmis:postlock",func="state_machine_postlock", execObj=self},
    {state="qmis:attach",func="state_machine_attach", execObj=self},
    {state="qmis:connect",func="state_machine_connect", execObj=self},
    {state="qmis:operate",func="state_machine_operate", execObj=self},
    {state="qmis:disconnect",func="state_machine_disconnect", execObj=self},
    {state="qmis:detach",func="state_machine_detach", execObj=self},
    {state="qmis:unregister",func="state_machine_unregister", execObj=self},
    {state="qmis:shutdown",func="state_machine_shutdown", execObj=self},
    {state="qmis:finalise",func="state_machine_finalise", execObj=self},

    -- reattach states
    {state="qmis:detach_to_reattach",func="state_machine_detach_to_reattach", execObj=self},
    {state="qmis:attach_to_reattach",func="state_machine_attach_to_reattach", execObj=self},
    {state="qmis:wait_to_reattach",func="state_machine_wait_to_reattach", execObj=self},
  }
end

--[[ turbo ]]--

function QmiController.on_qmi(arg)
  local self = arg[1]
  local qie = arg[2]
  -- receive QMI message
  local qm = self.luaq.recv_msg_async(qie)

  -- bypass if not received
  if not qm then
    self.l.log("LOG_DEBUG","no message received")
    return
  end

  self.l.log("LOG_DEBUG",string.format("QMI received - %s",qm.me.name))

  -- feed qmi_xxx modules
  self.watcher.invoke("qmi",qm.me.name,qm)
end

function QmiController:turbo_disconnect_qmi()
  self.l.log("LOG_INFO", "disconnect QMI services")

  -- disconnect QMI FDS
  for _, fd in ipairs(self.qmi_fd_qies) do
    self.i:remove_handler(fd)
  end
end

function QmiController:turbo_connect_rdb()
end

function QmiController:turbo_disconnect_rdb()
end

function QmiController:turbo_connect_qmi()
  self.l.log("LOG_INFO", "connect QMI services")

  self.qmi_fd_qies=self.luaq.get_async_fds()

  -- connect QMI FDS
  for _, fd_qie in ipairs(self.qmi_fd_qies) do
    self.i:add_handler(fd_qie.fd,self.t.ioloop.READ,self.on_qmi,{self, fd_qie.qie})
  end
end

--[[ modem poll ]]--

function QmiController:modem_poll_exec()
  local timeout = self.t.util.gettimemonotonic()
  self.l.log("LOG_DEBUG",string.format("exec modem poll (monotonic time = %d)",timeout))

  -- run poll
  if self.dconfig.enable_poll then
    self.watcher.invoke("sys","poll")
  end
end

function QmiController:modem_poll_quick_exec()
  local timeout = self.t.util.gettimemonotonic()
  self.l.log("LOG_DEBUG",string.format("exec modem poll quick (monotonic time = %d)",timeout))

  -- run poll
  if self.dconfig.enable_poll then
    self.watcher.invoke("sys","poll_quick")
  end
end

function QmiController:modem_poll_voice_exec()
  local timeout = self.t.util.gettimemonotonic()
  self.l.log("LOG_DEBUG",string.format("exec modem poll voice (monotonic time = %d)",timeout))

  -- run poll
  if self.dconfig.enable_poll then
    self.watcher.invoke("sys","poll_voice")
  end
end

function QmiController:modem_poll_start()

  self.l.log("LOG_INFO","run initial modem poll")
  self:modem_poll_exec()
  self:modem_poll_quick_exec();
  self:modem_poll_voice_exec();

  self.l.log("LOG_INFO","start modem poll")
  self.modem_poll_ref = self.i:set_interval(self.config.modem_poll_interval,self.modem_poll_exec,self)

  local modem_poll_quick_interval = self.config.modem_poll_interval
  -- poll modem quickly if system starts up on RF qualification mode
  if self.wrdb:get("service.wmmd.mode") == 'rf_qualification' then
    modem_poll_quick_interval = self.config.modem_poll_quick_interval
  end
  self.l.log("LOG_INFO","start modem poll quick")
  self.modem_poll_quick_ref = self.i:set_interval(modem_poll_quick_interval,self.modem_poll_quick_exec,self)

  -- start poll voice
  local modem_poll_voice_interval = self.config.modem_poll_quick_interval or 1000
  self.l.log("LOG_INFO","start modem poll voice")
  self.modem_poll_voice_ref = self.i:set_interval(modem_poll_voice_interval,self.modem_poll_voice_exec,self)
end

function QmiController:modem_poll_stop()
  -- stop voice poll
  if self.modem_poll_voice_ref then
    self.l.log("LOG_INFO","stop modem poll voice")
    self.i:clear_interval(self.modem_poll_voice_ref)
    self.modem_poll_voice_ref = nil
  end

  if self.modem_poll_ref then
    self.l.log("LOG_INFO","stop modem poll")
    self.i:clear_interval(self.modem_poll_ref)
    self.modem_poll_ref = nil
  end

  if self.modem_poll_quick_ref then
    self.l.log("LOG_INFO","stop modem poll quick")
    self.i:clear_interval(self.modem_poll_quick_ref)
    self.modem_poll_quick_ref = nil
  end

end

--[[ watcher ]]--

function QmiController:qmi_watcher_stop()
  self.watcher.invoke("sys","stop")
  self.watcher.reset()
end

function QmiController:qmi_watcher_start()
  self.l.log("LOG_INFO", "initiate watcher")
  for _,qe in pairs(self.qs) do
    if qe.init then
      qe:init()
    end
  end
end

--[[ RDB trigger callback ]]--

function QmiController:rdb_on_ims_test_mode(rdb,val)

  -- get test mode enable flag
  self.ims_test_mode_enable = val == "1"

  if self.ims_test_mode_enable then
    self.l.log("LOG_NOTICE",string.format("[IMS-REG] !!!!!!! ims test mode enabled !!!!!!!"))
  else
    self.l.log("LOG_NOTICE",string.format("[IMS-REG] ims test mode disabled"))
  end

  local a={}
  a.ims_test_mode = self.ims_test_mode_enable
  self.watcher.invoke("sys","enable_ims_registration",a)
end

function QmiController:rdb_on_ims_reg_cmd(rdb,val)
  self.l.log("LOG_INFO",string.format("[IMS-REG] rdb triggered (rdb='%s',val='%s')",rdb,val))

  local commands={
    ["reset"]=true,
    ["enable"]=true,
    ["disable"]=true,
  }

  -- bypass if not command
  if not commands[val] then
    return
  end

  if self.ims_test_mode_enable then
    self.l.log("LOG_ERR",string.format("[IMS-REG] !!!!!!! ims test mode enabled, ignore ims register command !!!!!!!",rdb,val))
  else

    -- set ims reset flag
    self.performing_ims_reset = val == "reset"

    local a={}

    self.l.log("LOG_INFO",string.format("[IMS-REG] de-register IMS"))
    a.ims_test_mode = val == "reset" or val == "disable"
    self.watcher.invoke("sys","enable_ims_registration",a)
  end

  self.wrdb:set(rdb,"")
end

-- registering to watcher --

function QmiController:modem_on_ims_reg_stat(type,event,a)

  -- maintain ims register status
  if not self.ims_registered and a.registered then
    self.wrdb:setp("ims.register.time",os.time())
  end
  self.ims_registered = a.registered

  for _,v in ipairs{"registered","reg_failure_error_code","reg_stat","reg_error_string","reg_network"} do
    if self.modem_ims_reg_stats[v] ~= a[v] then

      self.modem_ims_reg_stats[v]=a[v]

      -- do not write blank failure code
      if (v~="reg_failure_error_code") or a[v] then
        self.wrdb:setp("ims.register." .. v,a[v])
      end

      if v == "reg_stat" then
        self.l.log("LOG_INFO","reg status changed, re-read SIM card information")
        self.watcher.invoke("sys","poll_uim_card_status")
      end
    end
  end

  -- re-register
  if (a.reg_stat == "not registered") and self.performing_ims_reset then
    self.l.log("LOG_INFO",string.format("[IMS-REG] register IMS"))
    a.ims_test_mode = false
    self.watcher.invoke("sys","enable_ims_registration",a)

    -- clear ims reset flag
    self.performing_ims_reset = false
  end

  return true
end

function QmiController:modem_on_ims_pdp_stat(type,event,a)
  for _,v in ipairs{"connected","failure_error_code"} do
    self.wrdb:setp("ims.pdp." .. v,a[v])
  end

  return true
end

function QmiController:modem_on_ims_serv_stat(type,event,a)
  for _,v in ipairs{"sms","voip","vt","ut","vs"} do
    for _,v2 in ipairs{"_service_status","_service_rat"} do
      self.wrdb:setp("ims.service." .. v .. v2,a[v .. v2])
    end
  end

  return true
end

function QmiController:modem_on_reg_mgr_config(type,event,a)

  for _,v in ipairs{"primary_cscf","pcscf_port"} do
    self.wrdb:setp("ims.reg_config." .. v,a[v])
  end

  self.l.log("LOG_DEBUG",string.format("[IMS-REG] got noti - ims_test_mode = %s",a.ims_test_mode))

  return true
end

function QmiController:modem_on_uim_card_status(type,event,a)

  -- store card status
  if a.card_state then
    self.qg.card_state = a.card_state
    self.qg.error_code = a.error_code
  end

  -- store app status
  if a.app_state then
    self.qg.app_state = a.app_state
  end

  local sim_status

  if self.qg.card_state then

    if self.qg.card_state == "absent" or ((self.qg.card_state == "error") and ((self.qg.error_code == "no atr received") or (self.qg.error_code == "possibly removed"))) then
      sim_status = "SIM not inserted"

      self.l.log("LOG_INFO",string.format("no SIM found (error_code='%s')",self.qg.error_code))
    elseif self.qg.card_state == "present" then
      if self.qg.app_state then
        sim_status = self.app_state_to_sim_status[self.qg.app_state]

        self.l.log("LOG_INFO",string.format("SIM status detected (app_state='%s')",self.qg.app_state))
      end
    else
      sim_status = "SIM ERR"

      self.l.log("LOG_ERR",string.format("SIM ERR detected (error_code='%s'",self.qg.error_code))
    end

    self.wrdb:setp("sim.status.status",sim_status)
  end

  local pin_enabled
  if a.pin_state=="enabled_not_verified" or a.pin_state=="enabled_verified" then
    pin_enabled = "Enabled"
  elseif a.pin_state=="disabled" then
    pin_enabled = "Disabled"
  end
  self.wrdb:setp("sim.status.pin_enabled",pin_enabled)
  self.wrdb:setp("sim.cmd.param.verify_left",a.pin_retries)
  self.wrdb:setp("sim.cmd.param.unlock_left",a.puk_retries)
  -- WebUI relies on the following RDBs for retries
  self.wrdb:setp("sim.status.retries_remaining",a.pin_retries)
  self.wrdb:setp("sim.status.retries_puk_remaining",a.puk_retries)

  local autopin = self.wrdb:getp("sim.autopin")
  if a.pin_state == "enabled_not_verified" and autopin == "1" then
    self.watcher.invoke("sys", "auto_pin_verify")
  end

  -- mep status
  self.wrdb:setp("sim.mep.state", a.perso_state)
  self.wrdb:setp("sim.mep.feature", a.perso_feature)
  self.wrdb:setp("sim.mep.retries", a.perso_retries)
  self.wrdb:setp("sim.mep.unblock_retries", a.perso_unblock_retries)

  self.l.log("LOG_INFO","SIM card status changed, re-read modem information")
  self.watcher.invoke("sys","poll_simcard_info")

  self.l.log("LOG_INFO","SIM card status changed, re-read simcard raw info")
  self.watcher.invoke("sys","poll_simcard_raw_info")

  -- always return true here. poll_simcard(_raw)_info might fail due to missing sim files, which is normal
  return true
end

function QmiController:modem_on_operating_mode(type,event,a)

  -- update rdb
  self.wrdb:setp("operating_mode",a.operating_mode)

  -- update internal state
  self.qg.online_operating_mode = a.operating_mode == "online"

  local state=self.sq.get_current_state()

  if state == "qmis:powerup" then
    if self.qg.online_operating_mode then
      self.l.log("LOG_INFO","online operating mode detected, switch to readmodem state")
      self.sq.switch_state_machine_to("qmis:readmodem")
    else
      self.l.log("LOG_INFO","invalid operating mode detected, retry powerup state")
    end
  end

  return true
end

function QmiController:modem_on_simcard_raw_info(_type,event,a)

  for k,v in pairs(a) do

    local rdb_raw_info_name = self.config.rdb_g_prefix .. "sim.raw_data." .. k

    -- enumerate all existing rdb
    local remain_rdbs = self.wrdb:enum(rdb_raw_info_name,
      function(rdb)
        local match_str = "^" .. string.gsub(rdb_raw_info_name,"%.","%%.") .. "%."

        self.l.log("LOG_INFO",string.format("detect existing RDB variable (r=%s)",rdb))

        if not string.match(rdb,match_str) then
          return
        end

        return rdb
      end
    )

    -- set rdb
    if type(v) == "string" then
      self.wrdb:set(rdb_raw_info_name,v)

    elseif type(v) == "table" then

      for i,c in ipairs(v) do

        local new_rdb_var = string.format("%s.%d",rdb_raw_info_name,i)
        self.wrdb:set(new_rdb_var,c)

        remain_rdbs[new_rdb_var]=nil
      end
    end

    -- clear remains
    for r,_ in pairs(remain_rdbs) do
      self.l.log("LOG_INFO",string.format("remove existing RDB variables (r=%s)",r))
      self.wrdb:unset(r)
    end
  end

end

function QmiController:modem_on_simcard_info(type,event,a)

  self.wrdb:setp("imsi.msin",a.imsi)
  self.wrdb:setp("sim.data.msisdn",a.msisdn)
  self.wrdb:setp("system_network_status.simICCID",a.iccid)

  self.wrdb:setp("sim.data.mbn",a.mbn)
  self.wrdb:setp("sim.data.mbdn",a.mbdn)
  self.wrdb:setp("sim.data.adn",a.adn)

  self.wrdb:setp("sim.data.activation",a.activation)

  return true
end

function QmiController:modem_on_additional_serials(type,event,a)
  local changed = false
  for k,v in pairs(a) do
    local writeout = true
    local fn = "/usr/local/keep/system.product."..k
    local f=io.open(fn, 'r')
    if f then
      local fv = f:read('*line') or ''
      if fv == v then
        writeout = false
      end
      f:close()
    end
    if writeout then
      self.l.log("LOG_WARNING", "Writing new value '"..v.."' to "..fn)
      f=io.open(fn, 'w')
      f:write(v)
      f:close()
      changed = true
    end
  end
  if changed then
    -- Do not overide rebooting reason with this serialization procedure
    local reboot_reason = self.wrdb:get('system.outdoor_reboot_state') or ''
    if reboot_reason ~= '' then
      reboot_reason = reboot_reason..'; '
    end
    self.wrdb:set('service.system.reset_reason', reboot_reason..'WMMD: additional serials added/changed')
    self.wrdb:set('service.system.reset', '1')
  end
end

function QmiController:modem_on_modem_info(type,event,a)
  for k,v in pairs(a) do
    self.wrdb:setp(k,v)
  end
  return true
end

function QmiController:modem_on_rf_band_info(type,event,a)
  self.wrdb:setp_if_chg("system_network_status.current_band",a.current_band)
  self.wrdb:setp("system_network_status.channel",a.active_channel)
  self.wrdb:setp_if_chg("system_network_status.current_rf_bandwidth",a.rf_bandwidth)
  return true
end

function QmiController:modem_on_netcomm_signal_info(type,event,a)
  -- According to 80-N89218-1 F and QC-03487698,
  -- Logarithmic value of SINR values are in 1/5th of a dB. Range: 0 to 250
  -- which translates to -20dB to +30dB. The conversion formula is as follows:
  -- sinrDbValue = (sinrLogValue / 5) - 20
  self.wrdb:setp_if_chg("signal.rssinr",a.sinr and string.format("%.1f",((a.sinr/5)-20) ))
  return true
end

function QmiController:modem_on_signal_info(type,event,a)
  for k,v in pairs(self.signal_info_rdbs) do
    self.wrdb:setp(v,a[k])
  end

  -- Signal strength is needed by webui: RSRP for LTE and RSSI otherwise.
  self.wrdb:setp("radio.information.signal_strength",
    (a.nr5g_rsrp and a.nr5g_rsrp ~= "" and string.format("%ddBm",a.nr5g_rsrp)) or
    (a.rsrp and string.format("%ddBm",a.rsrp)) or
    (a.rssi and string.format("%ddBm",a.rssi)))

  return true
end

function QmiController:modem_on_network_time(type,event,a)

  self.wrdb:setp("networktime.datetime",string.format("%04d-%02d-%02d %02d:%02d:%02d",a.year,a.month,a.day,a.hour,a.minute,a.second))

  local timezone = string.format("%+g",a.time_zone*15/60)
  if a.daylt_sav_adj and a.daylt_sav_adj>0 then
    timezone = timezone .. string.format(" DST %d",a.daylt_sav_adj)
  end
  self.wrdb:setp("networktime.timezone",timezone)
  os.execute("TZ=UTC date -s > /dev/null 2> /dev/null " .. string.format('"%04d-%02d-%02d %02d:%02d:%02d"', a.year,a.month,a.day,a.hour,a.minute,a.second))

  return true
end

function QmiController:modem_on_ext_network_state(type,event,a)
  for k,v in pairs(self.ext_network_state_rdbs) do
    self.wrdb:setp(v,a[k])
  end

  -- report service status
  if a.srv_status then
    self.wrdb:setp("system_network_status.system_mode",a.srv_status)
  end

  -- reject cause(Refer to Annex G of 3GPP TS 24.008 to get reject code details)
  -- reject cause is instant value, which the modem reports when it meets specific conditions.
  -- So it is not possible to maintain with modem report because the modem usually reports invalid value.
  -- That is why last_reject_cause is added instead of reject_cause.
  if a.last_reject_cause then
    self.wrdb:setp("system_network_status.last_reject_cause", a.last_reject_cause)
  end

  -- disable / no difference between srv_status and true_srv_status
  --[[
  -- report true service status
  if a.true_srv_status then
    wrdb.setp("system_network_status.true_system_mode",a.true_srv_status)
  end
  ]]--

  return true
end

-- Handle state transitions at register state
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiController:handle_network_state_change_at_register(a)
  if a.reg_state then
    self.l.log("LOG_INFO",string.format("registered, switch to attach"))
    self.sq.switch_state_machine_to("qmis:attach")
  else
    self.l.log("LOG_INFO",string.format("not registered, retry to register after timeout"))
  end
end

-----------------------------------------------------------------------------------------------------------------------
-- Handle network state change for "detach_to_reattach" state
--
-- @param a invoke arguement from modem_on_network_state
--
function QmiController:handle_network_state_change_at_detach_to_reattach(a)
  -- bypass if PS attach state not available
  if a.ps_attach_state == nil then
    return
  end

  if not a.ps_attach_state then
    self.l.log("LOG_NOTICE",string.format("[custom-apn] ps detached, switch to attach_to_reattach"))
    self.sq.switch_state_machine_to("qmis:attach_to_reattach")
  else
    self.l.log("LOG_NOTICE",string.format("[custom-apn] ps still attached, retry to detach after timeout"))
  end
end

-----------------------------------------------------------------------------------------------------------------------
-- Handle network state change for "attach_to_reattach" state
--
-- @param a invoke arguement from modem_on_network_state
--
function QmiController:handle_network_state_change_at_attach_to_reattach(a)
  -- bypass if PS attach state not available
  if a.ps_attach_state == nil then
    return
  end

  if a.ps_attach_state then
    self.l.log("LOG_INFO",string.format("[custom-apn] ps attached, switch to operate"))

    self.l.log("LOG_NOTICE", "[custom-apn] === reattach procedure ends in manual attach mode")
    self.sq.switch_state_machine_to("qmis:operate")
  else
    self.l.log("LOG_INFO",string.format("[custom-apn] ps not attached yet, retry to attach after timeout"))
  end
end

-----------------------------------------------------------------------------------------------------------------------
-- Handle network state change for "wait_to_reattach" state
--
-- @param a invoke arguement from modem_on_network_state
--
function QmiController:handle_network_state_change_at_wait_to_reattach(a)
  if a.ps_attach_state then
    self.l.log("LOG_INFO",string.format("[custom-apn] ps attached, switch to operate in auto attach mode"))
    self.l.log("LOG_NOTICE", "[custom-apn] === reattach procedure done in auto attach mode")

    self.sq.switch_state_machine_to("qmis:operate")
  end
end

-- Handle state transitions at attach state
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiController:handle_network_state_change_at_attach(a)
  if a.ps_attach_state then
    self.l.log("LOG_INFO",string.format("ps attached, switch to connect"))
    self.sq.switch_state_machine_to("qmis:connect")
  else
    self.l.log("LOG_INFO",string.format("ps detached, retry to attach after timeout"))
  end
end

-- Handle state transitions at prelock state
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiController:handle_network_state_change_at_prelock(a)
  if not a.ps_attach_state then
    self.l.log("LOG_INFO",string.format("ps detached, switch to lock"))
    self.sq.switch_state_machine_to("qmis:lock")
  else
    self.l.log("LOG_INFO",string.format("ps attached, retry to detach after timeout"))
  end
end

-- Handle state transitions at postlock state
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiController:handle_network_state_change_at_postlock(a)
  if a.ps_attach_state then
    self.l.log("LOG_INFO",string.format("ps attached, switch to connect"))
    self.sq.switch_state_machine_to("qmis:connect")
  else
    self.l.log("LOG_INFO",string.format("ps detached, retry to attach after timeout"))
  end
end

-- Handle state transitions at connect state
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiController:handle_network_state_change_at_connect(a)
  if a.ps_attach_state then
    self.l.log("LOG_DEBUG",string.format("[linkprofile-ctrl] trigger sleeping link profiles"))
    self.watcher.invoke("sys","wakeup_linkprofiles")
  end
end

local function get_sysuptime()
    local uptime = 0
    local fd = io.open("/proc/uptime", 'r')
    if not fd then
        return uptime
    end
    uptime= math.floor(fd:read('*n'))
    fd.close()
    return uptime
end

-- Handle state transitions on network state change
-- a: Network status argument (the same argument passed on to "modem_on_network_state())
function QmiController:handle_network_state_transitions(a)
  local state=self.sq.get_current_state()

  local state_handlers={
    ["qmis:register"] = self.handle_network_state_change_at_register,
    ["qmis:attach"] = self.handle_network_state_change_at_attach,
    ["qmis:prelock"] = self.handle_network_state_change_at_prelock,
    ["qmis:postlock"] = self.handle_network_state_change_at_postlock,
    ["qmis:connect"] = self.handle_network_state_change_at_connect,
    ["qmis:operate"] = self.handle_network_state_change_at_connect,
    ["qmis:detach_to_reattach"] = self.handle_network_state_change_at_detach_to_reattach,
    ["qmis:attach_to_reattach"] = self.handle_network_state_change_at_attach_to_reattach,
    ["qmis:wait_to_reattach"] = self.handle_network_state_change_at_wait_to_reattach,
  }

  if state and state_handlers[state] then
    self.l.log("LOG_DEBUG",string.format("call network state handler (state='%s')",state))
    state_handlers[state](self,a)
  else
    self.l.log("LOG_DEBUG",string.format("network state handler not available (state='%s'",state))
  end

end

function QmiController:modem_on_network_state(type,event,a)
  --[[
  network_desc never works due to QC bug QC#03275023 (deprecated NAS_GET_SERVING_SYSTEM)
  this is now done in modem_on_operator_name
  if a.network_mcc then
    self.wrdb:setp("system_network_status.network", a.network_desc)
    --self.l.log("LOG_DEBUG","       system_network_status.network = "..a.network_desc)
  end
  --]]
  if a.roaming_status then
    self.wrdb:setp("system_network_status.roaming", a.roaming_status)
  end

  if a.reg_state_no then
    self.wrdb:setp("system_network_status.reg_stat",a.reg_state_no)
    self.wrdb:setp("system_network_status.registered",a.reg_state)

    -- Set system uptime when the modem starts network registration.
    if a.reg_state_no > 0 and not tonumber(self.wrdb:getp("system_network_status.sysuptime_at_reg_start")) then
        self.wrdb:setp("system_network_status.sysuptime_at_reg_start", get_sysuptime())
    end
  end

  if a.serving_system then
    self.wrdb:setp("system_network_status.service_type",a.serving_system)
  end

  self.wrdb:setp_via_buf("system_network_status.attached",a.ps_attach_state)

  -- update state
  self.qg.regsitered = a.reg_state
  self.qg.ps_attached =a.ps_attach_state

  -- handle state transitions
  self:handle_network_state_transitions(a)

  return true
end

function QmiController:modem_on_operator_name(type,event,a)
  if a.spn then
    self.wrdb:setp("system_network_status.network", a.spn)
  end
  if a.short_name then
    self.wrdb:setp("system_network_status.nw_name.short", a.short_name)
  end
  if a.long_name then
    self.wrdb:setp("system_network_status.nw_name.long", a.long_name)
  end
  if a.name_source then
    self.wrdb:setp("system_network_status.nw_name.source", a.name_source)
  end
  return true
end

function QmiController:modem_on_plmn_list(type,event,a)
  local i = 0
  local plmnArray = {}
  for plmnid, network_info in pairs(a) do
    i = i + 1
    self.wrdb:setp("plmn.plmn_list."..i..".plmnid", network_info.mcc..network_info.mnc)
    self.wrdb:setp("plmn.plmn_list."..i..".status", network_info.status or "")
    self.wrdb:setp("plmn.plmn_list."..i..".description", network_info.description or "")
    local desc = network_info.description or ""
    local rat = network_info.rat or 0 -- 0 for unknown
    plmnArray[i]=string.format("%s,%s,%s,%d,%d",desc,network_info.mcc,network_info.mnc,network_info.cns_stat,rat)
  end
  self.wrdb:setp("plmn.plmn_list.num", i) -- Update PLMN list number
  self.wrdb:setp("PLMN_list", table.concat(plmnArray,'&'))
  return true
end

function QmiController:update_cell_lock(type,event,a)
  self.qg.locksets = a
  self.sq.switch_state_machine_to("qmis:prelock")
  return true
end

function QmiController:update_channel_quality_indicator(type,event,a)

  local session_index = self.wrdb:getp( "rrc_session.index" )

  if session_index and (a.value > 0) then
    self.wrdb:setp("rrc_session."..session_index..".avg_cqi", a.value)
    self.wrdb:setp("system_network_status."..session_index..".avg_cqi", a.value)
    self.wrdb:setp("servcell_info.pcell."..session_index..".avg_cqi", a.value)
  end

  return true
end

function QmiController:update_transmission_mode(type,event,a)

  self.wrdb:setp("transmission_mode", a.value)

  return true
end

function QmiController:update_transmission_power(type,event,a)

  self.wrdb:setp("signal.tx_power_"..a.channel, a.value)

  return true
end

function QmiController:update_scell_parameters(type,event,a)

  self.wrdb:setp(a.channel, a.value)

  return true
end

function QmiController:update_mcs(type,event,a)
  self.l.log("LOG_INFO",string.format("JB: array_len: %d", a.length))

  for i=0, a.length-1 do
    self.wrdb:setp("mcs."..tostring(i), a.value[i])
  end

  return true
end

function QmiController:update_download_bandwidth(type,event,a)

  self:update_bandwidth(type,event,a,"dl")

  return true
end

function QmiController:update_upload_bandwidth(type,event,a)

  self:update_bandwidth(type,event,a,"ul")

  return true
end

function QmiController:update_bandwidth(type,event,a,up_down)

  -- need to convert the range 0 to 5 to 1.4 MHz to 20 MHz range
  -- According to qc doc 80-n9218-1, transmission bandwidth configuration of
  -- the serving cell on the downlink/uplink is a range from 0 to 5.
  local bandwidth = 1.4  -- default to 1.4 MHz
  if a.value == 5 then
    bandwidth = 20
  elseif a.value == 4 then
    bandwidth = 15
  elseif a.value == 3 then
    bandwidth = 10
  elseif a.value == 2 then
    bandwidth = 5
  elseif a.value == 1 then
    bandwidth = 3
  elseif a.value == 0 then
    bandwidth = 1.4
  end
  self.wrdb:setp("radio_stack.e_utra_measurement_report."..up_down.."_bandwidth", bandwidth)

  return true
end

function QmiController:modem_on_phonebook_state(type, event, a)
  if not a or not a.state then
    self.wrdb:setp("sim.phonebook.status", "unknown")
    return
  end
  self.wrdb:setp("sim.phonebook.status", a.state)
  return true
end

function QmiController:modem_on_phonebook_capabilities(type, event, a)
  self.wrdb:setp("sim.phonebook.max_records", a.max_records)
  self.wrdb:setp("sim.phonebook.used_records", a.used_records)
  if self.sq.get_current_state() == "qmis:phonebook" then
    if not self.qg.online_operating_mode then
      self.sq.switch_state_machine_to("qmis:powerup")
    else
      self.sq.switch_state_machine_to("qmis:readmodem")
    end
  end
  return true
end

local scellRdbObjConf = {persist = false, idSelection= "smallestUnused"}

function QmiController:modem_on_lte_cphy_ca_ind(wtype, event, a)
  self.wrdb:setp("radio_stack.e_utra_measurement_report.ca_ind.freq_pci_state", a and a.scell_freq_pci_state or '')

  -- LTE Primary Cell
  self.wrdb:setp_if_chg("system_network_status.lte_ca_pcell.pci", a and a.pcell_info_pci or "")
  self.wrdb:setp_if_chg("system_network_status.lte_ca_pcell.freq", a and a.pcell_info_freq  or "")
  self.wrdb:setp_if_chg("system_network_status.lte_ca_pcell.bandwidth", a and a.pcell_info_cphy_ca_dl_bandwidth or "")
  self.wrdb:setp_if_chg("system_network_status.lte_ca_pcell.band", a and a.pcell_info_band or "")

  -- LTE Secondary Cell
  local scellRdbObj = rdbobject.getClass(self.wrdb.config.rdb_g_prefix .. "system_network_status.lte_ca_scell.list", scellRdbObjConf)

  local scellList = scellRdbObj:getAll()
  local scellQmiNum = 0 -- the number of scell of qmi response
  local scellRdbNum = #scellList -- the number of instance of scell rdb object
  if a and type(a.scell_list) == "table" then
    scellQmiNum = #(a.scell_list)
  end

  for i=1, math.max(scellQmiNum, scellRdbNum) do
    if scellRdbNum >= i and i > scellQmiNum then -- #rdb_instance > #qmi_instance ==> delete rdb instance
      scellRdbObj:delete(scellList[i])
    elseif scellQmiNum >= i and i > scellRdbNum then -- #qmi_instance > #rdb_instance ==> create rdb instance
      local newRdbInst = scellRdbObj:new()
      newRdbInst.pci = a.scell_list[i].pci
      newRdbInst.freq = a.scell_list[i].freq
      newRdbInst.bandwidth = a.scell_list[i].cphy_ca_dl_bandwidth
      newRdbInst.band = a.scell_list[i].band
      newRdbInst.ul_configured = a.scell_list[i].ul_configured
      newRdbInst.scell_idx = a.scell_list[i].scell_idx
    else
      scellList[i].pci = a.scell_list[i].pci
      scellList[i].freq = a.scell_list[i].freq
      scellList[i].bandwidth = a.scell_list[i].cphy_ca_dl_bandwidth
      scellList[i].band = a.scell_list[i].band
      scellList[i].ul_configured = a.scell_list[i].ul_configured
      scellList[i].scell_idx = a.scell_list[i].scell_idx
    end
  end

  return true
end

function QmiController:modem_on_lte_cphy_ca_info(type, event, a)
  -- maximum scell info array size : 4
  for i=0,4 do
    self.wrdb:setp(string.format("radio_stack.e_utra_measurement_report.scell[%d].scell_idx", i),
      a and a.scell_info[i] and a.scell_info[i].scell_idx or '')
    self.wrdb:setp(string.format("radio_stack.e_utra_measurement_report.scell[%d].pci", i),
      a and a.scell_info[i] and a.scell_info[i].pci or '')
    self.wrdb:setp(string.format("radio_stack.e_utra_measurement_report.scell[%d].freq", i),
      a and a.scell_info[i] and a.scell_info[i].freq or '')
    self.wrdb:setp(string.format("radio_stack.e_utra_measurement_report.scell[%d].dl_bandwidth", i),
      a and a.scell_info[i] and a.scell_info[i].cphy_ca_dl_bandwidth or '')
    self.wrdb:setp(string.format("radio_stack.e_utra_measurement_report.scell[%d].band", i),
      a and a.scell_info[i] and a.scell_info[i].band or '')
    self.wrdb:setp(string.format("radio_stack.e_utra_measurement_report.scell[%d].scell_state", i),
      a and a.scell_info[i] and a.scell_info[i].scell_state or '')
  end
  self.wrdb:setp("radio_stack.e_utra_measurement_report.scell.pci",
    a and a.scell_pci or '')
  self.wrdb:setp("radio_stack.e_utra_measurement_report.scell.freq",
    a and a.scell_freq or '')
  self.wrdb:setp("radio_stack.e_utra_measurement_report.scell.dl_bandwidth",
    a and a.scell_dl_bandwidth or '')
  self.wrdb:setp("radio_stack.e_utra_measurement_report.scell.dl_band",
    a and a.scell_dl_band or '')
  self.wrdb:setp("radio_stack.e_utra_measurement_report.scell.scell_idx",
    a and a.scell_idx or '')
  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Perform reattach - detach and attach in sequence.
--
-- @param type invoke type
-- @param event invoke event name
-- @return true when it succeeds. Otherwise, false.
--
function QmiController:reattach(type,event)

  local state=self.sq.get_current_state()

  -- start reattach procedure immediately when we are in "qmis:operate"
  if state == "qmis:operate" then
    self.l.log("LOG_NOTICE", "[custom-apn] === reattach procedure starts by invoke in 'qmis:operate' state")

    self.sq.switch_state_machine_to("qmis:detach_to_reattach")
    return true
  end

  if self.qg.ps_attached then
    self.l.log("LOG_NOTICE", string.format("[custom-apn] === reattach procedure is scheduled (state='%s')",state))
    self.reattach_scheduled = true
  else
    self.l.log("LOG_NOTICE", string.format("[custom-apn] === ps not attached, not scheduling reattach (state='%s')",state))
  end
  return true
end

QmiController.cbs_system={
  "modem_on_ims_reg_stat",
  "modem_on_ims_pdp_stat",
  "modem_on_ims_serv_stat",
  "modem_on_reg_mgr_config",
  "modem_on_uim_card_status",
  "modem_on_operating_mode",
  "modem_on_simcard_raw_info",
  "modem_on_simcard_info",
  "modem_on_additional_serials",
  "modem_on_modem_info",
  "modem_on_rf_band_info",
  "modem_on_netcomm_signal_info",
  "modem_on_signal_info",
  "modem_on_network_time",
  "modem_on_ext_network_state",
  "modem_on_network_state",
  "modem_on_operator_name",
  "modem_on_plmn_list",
  "modem_on_network_scan_list",
  "update_cell_lock",
  "update_channel_quality_indicator",
  "update_transmission_mode",
  "update_transmission_power",
  "update_scell_parameters",
  "update_mcs",
  "update_download_bandwidth",
  "update_upload_bandwidth",
  "update_bandwidth",
  "modem_on_phonebook_state",
  "modem_on_phonebook_capabilities",
  "modem_on_lte_cphy_ca_info",
  "modem_on_lte_cphy_ca_ind",
  "reattach",
}

--[[ main state machine ]]--

function QmiController:state_machine_idle(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:initiate")
end

function QmiController:state_machine_initiate(old_stat,new_stat,stat_chg_info)

  if stat_chg_info == "continue" then
    self.l.log("LOG_NOTICE", "QMI services not ready - re-initiate")
    self.luaq.reinit()
  end

  -- re-do initiate if QMI not ready
  self.l.log("LOG_NOTICE", "check QMI services")
  if not self.luaq.check_qmi_services(self.essential_services) then
    self.l.log("LOG_NOTICE", "QMI services are not ready, retrying.")
    self.sq.switch_state_machine_to("qmis:initiate",self.qmi_connect_retry_interval)
    return
  end

  -- start
  self:turbo_connect_qmi()
  self:qmi_watcher_start()

  self.sq.switch_state_machine_to("qmis:prereadmodem")
end

function QmiController:state_machine_prereadmodem(old_stat,new_stat,stat_chg_info)
  -- subscribe ims
  self.l.log("LOG_INFO",string.format("[IMS-REG] subscribe trigger (trigger=%s)",self.rdb_ims_reg_cmd))
  self.rdbWatch:addObserver(self.rdb_ims_test_mode, "rdb_on_ims_test_mode", self)
  local init_test_mode = self.wrdb:get(self.rdb_ims_test_mode)

  self.l.log("LOG_INFO",string.format("[IMS-REG] poll ims test mode (test_mode='%s')",init_test_mode))
  self:rdb_on_ims_test_mode(self.rdb_ims_test_mode,init_test_mode)

  -- read modem
  self.watcher.invoke("sys","poll_serials")
  self.watcher.invoke("sys","poll_modem_info")
  self.watcher.invoke("sys","poll_operating_mode")

  self.sq.switch_state_machine_to("qmis:simpin")
end

-- Check whether IMSI is locked or not
--
-- FR-13175 : SIM IMSI Lock
-- The device shall read IMSI from SIM and compare it with the allowed PLMN
-- list. If match is found, then it shall be accepted, otherwise SIM shall be
-- rejected.
--
-- Return true if IMSI is locked, false otherwise
function QmiController:is_imsi_locked()
  local allowed_plmn_str = self.wrdb:getp("sim.imsi_lock.allowed_plmn_list")

  -- Ignore IMSI lock if the allowed plmn list is empty.
  if allowed_plmn_str and allowed_plmn_str ~= "" then
    local allowed_plmn_list = allowed_plmn_str:split(',')

    if #allowed_plmn_list > 0 then
      local imsi = self.wrdb:getp("imsi.msin")

      for _, allowed_plmn in ipairs(allowed_plmn_list) do
        if imsi:sub(1, allowed_plmn:len()) == allowed_plmn then
          self.wrdb:setp("sim.imsi_lock.status", 'unlocked')
          self.l.log("LOG_INFO",string.format("IMSI Unlocked: IMSI=%s, Allowed PLMN=%s",imsi,allowed_plmn))
          return false
        end
      end
      self.wrdb:setp("sim.imsi_lock.status", 'locked')
      self.l.log("LOG_NOTICE",string.format("IMSI Locked: IMSI=%s, Allowed PLMNs=%s",imsi,allowed_plmn_str))
      return true
    end
  end

  self.wrdb:setp("sim.imsi_lock.status", 'no lock')
  self.l.log("LOG_INFO",string.format("IMSI Lock is not enabled"))
  return false
end

function QmiController:state_machine_simpin(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:simpin",self.wmmd_simpin_retry_interval)

  self.watcher.invoke("sys","poll_uim_card_status")

  if not self:is_imsi_locked() then
    local pb_enabled = self.wrdb:getp("sim.phonebook.enable")
    if pb_enabled == '1' then
      self.sq.switch_state_machine_to("qmis:phonebook")
    elseif not self.qg.online_operating_mode then
      self.sq.switch_state_machine_to("qmis:powerup")
    else
      self.sq.switch_state_machine_to("qmis:readmodem")
    end
  end
end

function QmiController:state_machine_phonebook(old_stat, new_stat, stat_chg_info)
  self.sq.switch_state_machine_to("qmis:phonebook", self.wmmd_phonebook_retry_interval)
  self.watcher.invoke("sys", "poll_phonebook_status")
  -- modem_on_phonebook_capabilities will move the state forward
end

function QmiController:state_machine_powerup(old_stat,new_stat,stat_chg_info)

  self.sq.switch_state_machine_to("qmis:powerup",self.wmmd_powerup_retry_interval)

  -- switch Modem to online
  return self.watcher.invoke("sys","online")
end

function QmiController:state_machine_readmodem(old_stat,new_stat,stat_chg_info)
  -- read modem
  self.watcher.invoke("sys","poll_network_time")
  self.watcher.invoke("sys","poll_sig_info")
  self.watcher.invoke("sys","poll_rf_band_info")
  self.watcher.invoke("sys","poll_modem_network_status")
  self.watcher.invoke("sys","poll_ext_modem_network_status")
  self.watcher.invoke("sys","poll_lte_cphy_ca_info")
  self.watcher.invoke("sys","poll_mwi")

  -- poll ims
  self.watcher.invoke("sys","poll_ims_reg_stat")
  self.watcher.invoke("sys","poll_ims_serv_stat")

  -- subscribe ims
  self.l.log("LOG_INFO",string.format("[IMS-REG] subscribe trigger (trigger=%s)",self.rdb_ims_reg_cmd))
  self.rdbWatch:addObserver(self.rdb_ims_reg_cmd, "rdb_on_ims_reg_cmd", self)

  if self.start_gps then
    self.watcher.invoke("sys","start_gps")
  end

  -- poll ims
  self.watcher.invoke("sys","poll_ims")

  -- read profiles
  local succ = self.watcher.invoke("sys","read_linkprofiles")
  self.sq.switch_state_machine_to("qmis:readmodem",self.wmmd_read_retry_interval)

  if not succ then
    return
  end

  -- start modem poll
  self:modem_poll_start()

  -- handle the state specifics for lock configuration
  self:handle_state_transition_in_readmodem_state()
end

function QmiController:handle_state_transition_in_readmodem_state()
  -- lock mode requested by user
  local locksets = self.util.parse_lockmode(self.wrdb:getp("lockmode"))

  -- existing modem lock configuration
  local lock_config = self.util.parse_lockmode(self.wrdb:getp("lock_config"))

  if locksets then
    self.l.log("LOG_INFO",string.format("locksets=%s", table.tostring(locksets)))
  else
    self.l.log("LOG_ERR",string.format("illegal lockmode, skipped locking"))
  end
  if lock_config then
    self.l.log("LOG_INFO",string.format("lock_config=%s", table.tostring(lock_config)))
  else
    self.l.log("LOG_ERR",string.format("illegal lock_config, overwritting"))
  end

  if locksets and not self.util.lock_equal(locksets, lock_config) then
    self.l.log("LOG_INFO",string.format("lockmode changed, switch to prelock"))
    self.qg.locksets = locksets
    self.sq.switch_state_machine_to("qmis:prelock")
  elseif not self.qg.regsitered then
    self.sq.switch_state_machine_to("qmis:register")
  elseif not self.qg.ps_attached then
    self.sq.switch_state_machine_to("qmis:attach")
  else
    self.sq.switch_state_machine_to("qmis:connect")
  end
end

function QmiController:state_machine_prelock(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:prelock",self.wmmd_lock_detach_retry_interval)
  -- make sure we are detached here
  self.l.log("LOG_INFO",string.format("prelock: detaching now"))

  local succ = self:detach_modem()
  if not succ then
    self.watcher.invoke("sys","poll_modem_network_status")
  end
  -- modem_on_network_state will switch state to lock upon detach success
end

function QmiController:state_machine_lock(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:prelock",self.wmmd_lock_detach_retry_interval)
  -- at the moment, we ignore eci in lock mode. this might be extended
  local succ = self.watcher.invoke("sys","cell_lock",self.qg.locksets)
  if succ then
    self.l.log("LOG_INFO",string.format("lock: cell_lock succeeded, switch to postlock"))
    -- record the lock config in RDB since there is no way to read lock config from modem
    self.wrdb:setp("lock_config", self.util.pack_lockmode(self.qg.locksets))
    self.sq.switch_state_machine_to("qmis:postlock")
    return
  end
  self.l.log("LOG_ERR",string.format("lock: cell_lock failed, switch to prelock after timeout"))
  self.watcher.invoke("sys","poll_modem_network_status")
end

function QmiController:state_machine_postlock(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:postlock",self.wmmd_attach_retry_interval)
  self.l.log("LOG_INFO",string.format("postlock: perform manual attach"))
  local succ = self:attach_modem()
  if succ then
    self.l.log("LOG_INFO",string.format("postlock: manual attach requested, wait for attached"))
    -- attach invocation success just means request is sent out
    -- modem_on_network_state will switch state to connect upon attached
  else
    self.l.log("LOG_ERR",string.format("postlock: manual attach failed. retrying shortly"))
    self.watcher.invoke("sys","poll_modem_network_status")
  end
end

function QmiController:state_machine_register(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:register",self.wmmd_register_retry_interval)

  -- TODO: do register procedure

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- Handle detach_to_reattach state
--
-- The function invokes "detach" to detach PS and waits until PS gets detached.
--
-- @param old_stat previous state of the state machine
-- @param new_stat current state of the state machine
-- @param stat_chg_info indication is true when the function is called for transit
-- @param lp link.profile RDB object
--
function QmiController:state_machine_detach_to_reattach(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:detach_to_reattach",self.wmmd_deatach_retry_interval)

  self.l.log("LOG_DEBUG", "[custom-apn] invoke 'detach' to reattach")

  local succ = self:detach_modem()
  if not succ then
    self.l.log("LOG_ERR", "[custom-apn] failed to detach")
  end
end

-----------------------------------------------------------------------------------------------------------------------
-- Handle attach_to_reattach state
--
-- The function invokes "attach" to attach PS and switches to operate state.
--
-- @param old_stat previous state of the state machine
-- @param new_stat current state of the state machine
-- @param stat_chg_info indication is true when the function is called for transit
-- @param lp link.profile RDB object
--
function QmiController:state_machine_attach_to_reattach(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:attach_to_reattach",self.wmmd_attach_retry_interval)

  self.l.log("LOG_DEBUG", "[custom-apn] invoke 'attach' to reattach")
  local succ = self:attach_modem()
  if not succ then
    self.l.log("LOG_ERR", "failed to attach to reattach")
    return
  end

  -- keep invoking "attach" if manual attach is required. Otherwise, switch to operate state.
  if not self.dconfig.enable_manual_attach then
    self.l.log("LOG_DEBUG", "[custom-apn] auto attach mode detected, switch to 'wait_to_reattach' to wait until UE gets attached")

    self.sq.switch_state_machine_to("qmis:wait_to_reattach")
  end
end

-----------------------------------------------------------------------------------------------------------------------
-- Handle state_machine_wait_to_reattach state
--
-- The function waits until UE gets attached
--
-- @param old_stat previous state of the state machine
-- @param new_stat current state of the state machine
-- @param stat_chg_info indication is true when the function is called for transit
-- @param lp link.profile RDB object
--
function QmiController:state_machine_wait_to_reattach(old_stat,new_stat,stat_chg_info)
  self.sq.switch_state_machine_to("qmis:operate",self.wmmd_attach_retry_interval)
  self.l.log("LOG_NOTICE", "[custom-apn] === reattach procedure will be done after timeout in auto attach mode ")
end

function QmiController:state_machine_attach(old_stat,new_stat,stat_chg_info)

  self.sq.switch_state_machine_to("qmis:attach",self.wmmd_attach_retry_interval)

  local succ
  -- perform manual attach only when manual attach is enabled
  if self.dconfig.enable_manual_attach then

    self.l.log("LOG_INFO", "perform manual attach")

    succ = self:attach_modem()
    if not succ then
      succ = self.watcher.invoke("sys","poll_modem_network_status")
    end
  else
    succ = self.watcher.invoke("sys","poll_modem_network_status")
  end

  return succ
end

function QmiController:state_machine_connect(old_stat,new_stat,stat_chg_info)
  self.l.log("LOG_NOTICE", "connect: attach successful")

  local succ = self.watcher.invoke("sys","start_linkprofiles")

  -- retry to connect if start_linkprofiles fails
  if not succ then
    self.l.log("LOG_INFO", string.format("failed to connect linkprofile, retry in %d ms",self.wmmd_connect_retry_interval))
    self.sq.switch_state_machine_to("qmis:connect",self.wmmd_connect_retry_interval)
    return
  end

  -- switch operate state
  self.sq.switch_state_machine_to("qmis:operate")
end

function QmiController:state_machine_operate(old_stat,new_stat,stat_chg_info)
  self.watcher.invoke("sys","poll_network_time")
  self.l.log("LOG_NOTICE", "operate: modem now operational")

  if self.reattach_scheduled then
    self.l.log("LOG_NOTICE", "[custom-apn] === reattach procedure starts by schedule")
    self.sq.switch_state_machine_to("qmis:detach_to_reattach")
    return
  end

end

function QmiController:state_machine_disconnect(old_stat,new_stat,stat_chg_info)
  local succ = self.watcher.invoke("sys","stop_linkprofiles")
end

function QmiController:state_machine_finalise(old_stat,new_stat,stat_chg_info)
  -- stop
  self:modem_poll_stop()
  self:qmi_watcher_stop()
  self.i:close()
end

function QmiController:state_machine_unregister(old_stat,new_stat,stat_chg_info)
end

function QmiController:attach_modem()

  local succ = self.watcher.invoke("sys","attach")
  if succ then
    self.l.log("LOG_NOTICE", "attach_modem: attach_modem requested, waiting for attach")
  else
    self.l.log("LOG_ERR", "attach_modem: attach failed")
  end

  -- immediately connect link.profiles
  succ = self.watcher.invoke("sys","reset_and_wakeup_linkprofiles")
  if succ then
    self.l.log("LOG_NOTICE", "attach_modem: signaled to reset and wake up link.profiles")
  else
    self.l.log("LOG_ERR", "attach_modem: failed to reset and wake up link.profiles")
  end

  return succ
end

function QmiController:detach_modem()

  local succ = self.watcher.invoke("sys","detach")
  if succ then
    self.l.log("LOG_NOTICE", "detach_modem: detach requested, waiting for detach")
    -- detach invocation success just means request is sent out
  else
    self.l.log("LOG_ERR", "detach_modem: detach failed")
  end

  if self.reattach_scheduled then
    self.l.log("LOG_NOTICE", "[custom-apn] manually dettached, cancel scheduled reattach not to attempt to re-attach")
    self.reattach_scheduled = false
  end

  return succ
end

function QmiController:state_machine_detach(old_stat,new_stat,stat_chg_info)
end

function QmiController:state_machine_shutdown(old_stat,new_stat,stat_chg_info)
end

function QmiController:cellLockChanged(rdbKey, rdbVal)
  -- If we are already attached, do nothing here since the monitoring logic will kick in.
  local attached = self.wrdb:getp("system_network_status.attached")
  local state = self.sq.get_current_state()
  self.l.log("LOG_INFO", string.format("[cellLockChanged] lockmode=%s, attached=%s, state=%s", rdbVal, attached, state))
  if attached == "1" and state ~= "qmis:prelock" then
    self.l.log("LOG_INFO", "lockmode changed while attached. cell locking monitoring will kick in shortly")
    return
  end
  local locksets = self.util.parse_lockmode(rdbVal)
  if not locksets then
    self.l.log("LOG_ERR", string.format("Illegal lockmode %s ignored",rdbVal))
    return
  end
  -- check if this lockmode is already set in modem
  local lock_config = self.util.parse_lockmode(self.wrdb:getp("lock_config"))
  if self.util.lock_equal(locksets, lock_config) then
    self.l.log("LOG_INFO", string.format("lockmode %s is already set in modem, skipped", rdbVal))
    return
  end
  if state == "qmis:lock" or state == "qmis:postlock" or state == "qmis:register" or state == "qmis:attach" then
    self.qg.locksets = locksets
    self.sq.switch_state_machine_to("qmis:prelock")
  end
end

-- get the current pci
-- return the current pci if attached, nil otherwise.
function QmiController:get_current_pci()
  if not self.qg.ps_attached then
    return nil
  end

  local currCellId = self.rdb.get("wwan.0.radio_stack.e_utra_measurement_report.servphyscellid")
  if currCellId then
    return currCellId
  end
end

-- pci lock initializations
function QmiController:init_pci_lock()
  --[[
       PCI lock RDB watch.
       lockmode is non-persistent and watched.
       cfg_lockmode is persistent but not watched.
  --]]
  local lockmode = self.wrdb:getp("lockmode")
  local cfg_lockmode = self.wrdb:getp("cfg_lockmode")
  if (not lockmode or lockmode == "") and (cfg_lockmode and cfg_lockmode ~= "") then
    self.wrdb:setp("lockmode", cfg_lockmode)
  end
  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."lockmode", "cellLockChanged", self)
end

function QmiController:init()
  -- init pci lock
  self:init_pci_lock()

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- init rdbs
  self.wrdb:setp("ims.register.time")

  -- create main state macine
  self.sq=self.smachine.new_smachine("qmi_smachine",self.stateMachineHandlers)

  -- start initial state machine
  self.sq.switch_state_machine_to("qmis:idle")
end

return QmiController
