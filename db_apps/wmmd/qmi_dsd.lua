-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2020 Casa Systems.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi dsd module

local QmiDsd = require("wmmd.Class"):new()

function QmiDsd:setup(rdbWatch, wrdb, dConfig)
  -- load essential lua modules
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_dsd", "LOG_DAEMON") end)
  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.dconfig = dConfig
  self.ffi = require("ffi")

  -- setup alias for luaqmi message collection
  self.m = self.luaq.m

  self.so_mask_name = {
    -- 3GPP SO Mask:
    [0] = "WCDMA",
    [1] = "HSDPA",
    [2] = "HSUPA",
    [3] = "HSDPAPLUS",
    [4] = "DC HSDPAPLUS",
    [5] = "64 QAM",
    [6] = "HSPA",
    [7] = "GPRS",
    [8] = "EDGE",
    [9] = "GSM",
    [10] = "S2B",
    [11] = "LTE Limited Service",
    [12] = "LTE FDD",
    [13] = "LTE TDD",
    [14] = "TDSCDMA",
    [15] = "DC HSUPA",
    [16] = "LTE CA DL",
    [17] = "LTE CA UL",
    [18] = "S2B Limited Service",
    [19] = "4.5G",
    [20] = "4.5G+",
    -- 3GPP2 SO Mask:
    [24] = "1X IS95",
    [25] = "1X IS2000",
    [26] = "1X IS2000 REL A",
    [27] = "HDR REV0 DPA",
    [28] = "HDR REVA DPA",
    [29] = "HDR REVB DPA",
    [30] = "HDR REVA MPA",
    [31] = "HDR REVB MPA",
    [32] = "HDR REVA EMPA",
    [33] = "HDR REVB EMPA",
    [34] = "HDR REVB MMPA",
    [35] = "HDR EVDO FMC",
    [36] = "1X Circuit Switched",
    -- 5G SO Mask:
    [40] = "5G TDD",
    [41] = "5G SUB6",
    [42] = "5G MMWAVE",
    [43] = "5G NSA",
    [44] = "5G SA",
  }
end

-- convert a uint64_t so mask to comma delimited name in string.
function QmiDsd:so_mask_to_string(mask)
  local nameLo, nameHi = {}, {}
  local del = ','
  local mid_del = ""
  local lo=tonumber(self.ffi.cast("uint32_t",mask)) -- low 32 bits
  local hi=tonumber(self.ffi.cast("uint32_t",mask/2^32)) -- high 32 bits

  for i=0,31 do
    if self.bit.band(2^i, lo) ~= 0 and self.so_mask_name[i] then
      table.insert(nameLo, self.so_mask_name[i])
    end
    if self.bit.band(2^i, hi) ~= 0 and self.so_mask_name[i+32] then
      table.insert(nameHi, self.so_mask_name[i+32])
    end
  end

  if #nameLo > 0 and #nameHi > 0 then
    mid_del = del
  end

  return table.concat(nameLo, del) .. mid_del .. table.concat(nameHi, del)
end

-- Update current system status
function QmiDsd:modem_on_system_status(resp)
  if self.luaq.is_c_member_and_true(resp, "global_pref_sys_valid") then
    self.l.log("LOG_DEBUG",string.format("QMI_DSD_SYSTEM_STATUS global_pref_sys_valid."))
    local curr_pref_sys = resp.global_pref_sys.curr_pref_sys
    local so_mask_str = self:so_mask_to_string(curr_pref_sys.so_mask)
    self.wrdb:setp( "system_network_status.current_system_so", so_mask_str)
    -- qdiagd updates ECGI upon receiving LTE Serving Cell Info message which does not
    -- exist in SA mode so need to be cleared here.
    if string.find(so_mask_str, "5G SA") then
      self.wrdb:setp( "system_network_status.ECGI", "")
    end
  end
end

-------------------------------------------------------------------------------------------------------------------
-- Indicates the system status state changes
--
function QmiDsd:QMI_DSD_SYSTEM_STATUS_IND(type, event, qm)
  self.l.log("LOG_DEBUG",string.format("QMI_DSD_SYSTEM_STATUS_IND received."))
  self:modem_on_system_status(qm.resp)
  return true
end

QmiDsd.cbs={
  "QMI_DSD_SYSTEM_STATUS_IND",
}

-------------------------------------------------------------------------------------------------------------------
-- Get the system status
--
function QmiDsd:poll_system_status(type,event,a)
  self.l.log("LOG_INFO",string.format("send QMI_DSD_GET_SYSTEM_STATUS_REQ"))
  local succ,qerr,resp=self.luaq.req(self.m.QMI_DSD_GET_SYSTEM_STATUS)
  if not succ then
    return false
  end
  self:modem_on_system_status(resp)
  return true
end

QmiDsd.system_watcher_handlers={
  "poll_system_status",
  "poll",
}

function QmiDsd:poll(type, event, a)
  self.watcher.invoke("sys","poll_system_status")
end

-------------------------------------------------------------------------------------------------------------------
-- module initializing function
function QmiDsd:init()

  self.l.log("LOG_INFO", "initiate qmi_dsd")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.system_watcher_handlers) do
    self.watcher.add("sys", v, self, v)
  end

  -- add app start QMI_DSD_SYSTEM_STATUS_IND
  self.luaq.req(self.m.QMI_DSD_SYSTEM_STATUS_CHANGE,{report_data_system_status_changes=1})

end

return QmiDsd

