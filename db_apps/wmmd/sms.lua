-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- execution module that manages voice commands from Asterisk channel to modem

local Sms = require("wmmd.Class"):new()

function Sms:setup(wrdb)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("voicecommand", "LOG_DAEMON") end)

  self.t = require("turbo")
  self.ffi = require("ffi")

  self.luaq = require("luaqmi")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.wrdb = wrdb
  self.rdb = require("luardb")
  self.config = require("wmmd.config")

  self.i = self.t.ioloop.instance()

  self.v = {}

  self.mwi_rdb_prefix = "mwi"

  -- mwi rdb keys
  self.mwi_rdb_keys = {
    "voicemail",
    "fax",
    "email",
    "other",
    "videomail"
  }

  -- mwi rdb subkeys
  self.mwi_rdb_subkeys = {
    "count",
    "active",
  }
end

--[[ cbs_systrem ]]--

function Sms:modem_on_mwi(type,event,a)

  for k,v in pairs(a) do
    for _,sk in ipairs(self.mwi_rdb_subkeys) do
      self.wrdb:setp(string.format("%s.%s.%s",self.mwi_rdb_prefix,k,sk),v[sk])
    end
  end

  -- set voice mail for backward compatibility
  if a.voicemail then
    self.wrdb:setp("sms.received_message.vmstatus",a.voicemail.active and "active" or "inactive")
  end

  return true
end

--[[ ATT FR-21606:
The handset is not required to maintain the message count if the SIM does not
support the message count capability (i.e., 3G MWI files). There exists the possibility that
the SIM could be removed, placed in another handset, the voice mail number is changed,
and then the SIM is reinserted back in to the original handset. After the handset is power
cycled, it shall just indicate the presence of voice mails only.

This CBS function is called when failed to read MWI SIM file which may not exist.
If MWI is active with some count number then set the count to 255 to indicate
MWI with unknown number state in WEBUI.
]]--
function Sms:sync_mwi_count(type,event)
  local voice_mwi_active = self.wrdb:getp(string.format("%s.voicemail.active",self.mwi_rdb_prefix)) or '0'
  local voice_mwi_count = self.wrdb:getp(string.format("%s.voicemail.count",self.mwi_rdb_prefix)) or '0'
  if voice_mwi_active == '1' and voice_mwi_count ~= '255' then
    self.l.log("LOG_INFO", "Synchronise voice MWI count to 255")
    self.wrdb:setp(string.format("%s.voicemail.count",self.mwi_rdb_prefix),'255')
  end
  return true
end

Sms.cbs_system={
  "modem_on_mwi",
  "sync_mwi_count",
}

function Sms:init()

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- create SMS incoming message folder
  os.execute("mkdir -p " .. self.config.incoming_sms_dir)
end

return Sms
