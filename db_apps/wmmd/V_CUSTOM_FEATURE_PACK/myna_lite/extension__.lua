--[[
  Decorator for Myna

  Copyright (C) 2019 Casa Systems Inc.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")
local MynaDecorator = WmmdDecorator:new()
local QmiGDecorator = WmmdDecorator:new()

function QmiGDecorator:state_machine_powerup(old_stat,new_stat,stat_chg_info)
  local curr_band = self.rdb.get(self.currBandSelBandRdb) or ""
  local status = self.rdb.get(self.currBandStatusRdb) or ""
  local cmd = self.rdb.get(self.currBandCmdRdb) or ""

  local wmmd_mode = self.rdb.get("wwan.0.operating_mode") or ""
  self.l.log("LOG_NOTICE", string.format("powerup(): wmmd mode:'%s'", wmmd_mode))
  if wmmd_mode == "factory test mode" then
    -- Corner case: when firmware flashed for first time, modem boots into FTM at factory, and
    -- we are stuck in an endless loop here. Modem must be online atleast once to avoid this.
    self.l.log("LOG_NOTICE", "powerup(): modem in factory test mode, bringing online")
    self.watcher.invoke("sys","online")
  end

  self.l.log("LOG_NOTICE", string.format(
    "powerup(): current band:'%s', last band cmd:'%s', status:'%s'", curr_band, cmd, status))

  -- set band to 48
  if curr_band ~= self.b48str then
    self.sq.switch_state_machine_to("qmis:powerup",self.magpie_powerup_band_check_interval)
    self.rdb.set(self.currBandStatusRdb, "")

    -- if set success, or get had any status (i.e. b48 not set)
    if cmd == "set" and status == "[done]" or cmd == "get" and status == "[error]" then
      self.rdb.set(self.currBandCmdRdb, "get")
    else
      self.l.log("LOG_NOTICE", string.format("powerup(): current band='%s', setting to='%s'", curr_band, self.b48str))
      self.rdb.set(self.currBandParamRdb, self.b48str)
      self.rdb.set(self.currBandCmdRdb, "set")
    end
    return
  end

  self.rdb.set(self.currBandCfgRdb, curr_band)
  QmiGDecorator:__invokeChain("state_machine_powerup")(self)
end

function QmiGDecorator:update_attach_type(key, val)
  if self.last_attach_val and self.last_attach_val == val then return end
  self.last_attach_val = val
  if val ~= "1" then return end
  local attach_type = self.rdb.get(self.attachType)
  self.l.log("LOG_NOTICE", string.format("Check and applying requested attach type: '%s'", attach_type))
  self.rdb.set(self.attachType, attach_type)
end

function QmiGDecorator:init()
  QmiGDecorator:__invokeChain("init")(self)
  self.magpie_powerup_band_check_interval = 3000
  self.b48str = "LTE Band 48 - TDD 3600"
  self.optBandstr = "LTE Band 48 - TDD 3600" -- used in ptcrb mode
  self.default_profile_number = {
    [48] = 1,  -- 'band 48, the default profile: cbrssas_apn_number'
  }
  self.currBandCfgRdb = "wwan.0.currentband.config"
  self.currBandSelBandRdb = "wwan.0.currentband.current_selband"
  self.currBandParamRdb = "wwan.0.currentband.cmd.param.band"
  self.currBandCmdRdb = "wwan.0.currentband.cmd.command"
  self.currBandStatusRdb = "wwan.0.currentband.cmd.status"
  self.attachType = "wmmd.config.modem_service_domain"

  self.rdbWatch:addObserver("wwan.0.system_network_status.attached", "update_attach_type", self)
end

function QmiGDecorator:doDecorate()
  QmiGDecorator:__saveChain("init")
  QmiGDecorator:__saveChain("state_machine_powerup")
  QmiGDecorator:__changeImplTbl({
    "init",
    "state_machine_powerup",
    "update_attach_type",
  })
end

function MynaDecorator.doDecorate()
  MynaDecorator.__inputObj__.qmiG = QmiGDecorator:decorate(MynaDecorator.__inputObj__.qmiG)
end

return MynaDecorator
