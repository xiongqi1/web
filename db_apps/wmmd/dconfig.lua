-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- RDB based dynamic configuration

local DConfig = require("wmmd.Class"):new()

-- RDB global default settings for WMMD
DConfig.default_rdb_settings_proto = {
  -- enable or disable 10 second global poll only for debugging
  enable_poll = true,
  -- enable or disable QMI request logging
  enable_log_qmi_req = false,
  -- enable or disable qmi response logging
  enable_log_qmi_resp = false,
  -- enable or disable MWI polling - only for the modems that do not support MWI indication.
  enable_poll_mwi = false,
  -- enable or disable Background PLMN scan - only to collect SIB1 (part of AT&T requirement)
  enable_poll_bplmn = true,
  -- minimum off length of each DTMF
  dtmf_min_interval = 140,
  -- force to manually attach
  enable_manual_attach = false,
  -- multiple band selection
  enable_multi_band_sel = false,
  -- hide unwanted band
  hidden_bands = "",

  -- issue a qualcomm specific command to synchronize md5sum in EFS with md5sum in NAND flash partitions
  trigger_to_sync_md5sum_for_scrub = false,
}

function DConfig:setup(rdbWatch, wrdb)
  self.l = require("luasyslog")
  self.rdbWatch = rdbWatch
  self.wrdb = wrdb
  self.wmmd_rdb_prefix = "wmmd.config."
  for k,v in pairs(self.default_rdb_settings_proto) do
    self[k] = v
  end
end

function DConfig:change_wmmd_config_rdb(rdb,val)

  local member = rdb:match(string.gsub(self.wmmd_rdb_prefix,"%.","%%.") .. "(.+)")

  -- bypass unknown format
  if not member then
    self.l.log("LOG_INFO",string.format("incorrect configuration rdb name format (rdb=%s)",rdb))
    return false
  end

  if self[member] == nil then
    self.l.log("LOG_INFO",string.format("unknown wmmd configuration (member=%s)",member))
    return false
  end

  local oldval = self[member]

  if type(self[member]) == "boolean" then
    self[member]=(val=="1")
  else
    self[member]=val
  end

  self.l.log("LOG_INFO",string.format("config %s [ '%s' ==> '%s' ]",member,oldval,self[member]))

  return true
end

--
--
-------------------------------------------------------------------------------------------------------------------
-- Change a config member value in rdb - write a value to config RDB
--
-- @param member config member name - without RDB prefix
-- @param val RDB value to write
function DConfig:update_config(member,val)
  local wmmd_config_rdb

  wmmd_config_rdb = self.wmmd_rdb_prefix .. member

  -- change config memeber in memory first
  if not self:change_wmmd_config_rdb(wmmd_config_rdb,val) then
    self.l.log("LOG_ERR",string.format("unknown config access (member=%s)",member))
    return
  end

  -- change config memeber in RDB
  -- after setting this RDB, one recursive change RDB notification is expected.
  self.wrdb:set(wmmd_config_rdb,val)

end

function DConfig:init()
  local wmmd_config_rdb
  local val

  -- read RDB variables into settings if the RDB variables exists. Otherwise, set RDB variables to default settings
  for k,_ in pairs(self.default_rdb_settings_proto) do

    wmmd_config_rdb = self.wmmd_rdb_prefix .. k
    val = self.wrdb:get(wmmd_config_rdb)
    if not val then
      self.wrdb:set(wmmd_config_rdb,self[k])
    else
      self:change_wmmd_config_rdb(wmmd_config_rdb,val)
    end

    self.rdbWatch:addObserver(wmmd_config_rdb, "change_wmmd_config_rdb", self)
  end

end

return DConfig
