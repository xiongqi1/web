-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- wmmd own rdb layer

local WmmdRdb = require("wmmd.Class"):new()

function WmmdRdb:setup(rdbWatch)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("wmmd_rdb", "LOG_DAEMON") end)

  self.t = require("turbo")
  self.i = self.t.ioloop.instance()
  self.watcher = require("wmmd.watcher")

  self.lrdb = require("luardb")
  self.rdbWatch = rdbWatch
  self.config = require("wmmd.config")

  -- synchronous mode returns only once, store trig and rfd
  self.trig, self.rfd = self.lrdb.synchronousMode(true)

  self.rdb_chg_table={}
  self.rdb_init_table={}
end

function WmmdRdb:on_rdb()
  --l.log("LOG_INFO",string.format("rdb event received"))

  self.trig()
end

function WmmdRdb:write_watchdog()
  -- avoid tostring() so we get whole 53-bit int.
  if _G.watchdog_val ~= nil then
    self.lrdb.set("service.wmmd.watchdog", string.format("%.0f",_G.watchdog_val))
  end
end

function WmmdRdb:stop_rdb(type,event,arg)
  self.l.log("LOG_INFO","stop rdb")
  self.i:remove_handler(self.rfd)

  return true
end

WmmdRdb.cbs_system={
  "stop_rdb",
}

function WmmdRdb:init(lp_index)

  self.l.log("LOG_INFO","start rdb")

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  self.l.log("LOG_INFO",string.format("add rdb fd into turbo (rfd=%d)",self.rfd))
  self.i:add_handler(self.rfd, self.t.ioloop.READ, self.on_rdb, self)
  self.i:set_interval(1000, self.write_watchdog, self)
end

function WmmdRdb:enum(rdb_pattern,func)

  local cl={}
  local k

  for _,rdb in ipairs(self.lrdb.keys(rdb_pattern)) do

    -- get key
    if func then
      k=func(rdb)
    else
      k=rdb
    end

    if k then
      cl[k]=self.lrdb.get(rdb)
    end
  end

  return cl
end


function WmmdRdb.conv_val_to_rdbval(val)
  if type(val) == "boolean" then
    return val and "1" or "0"
  end

  if not val then
    return ""
  end

  return val
end

-------------------------------------------------------------------------------------------------------------------
-- Write a value to a RDB variable
--
-- @param rdb RDB variable string name to write.
-- @param v RDB value to write to the RDB varaible.
-- @param flags flags to change the RDB flags
--        nil -> keep original flags
--        blank string ("")  -> clear all flags
--        others -> set new flags
-- @return True is returned when it succeeds. Otherwise, False.
function WmmdRdb:set(rdb,v,flags)

  v=self.conv_val_to_rdbval(v)

  self.l.log("LOG_DEBUG",string.format("[RDB] write '%s' ==> [%s]",rdb,v))

  return self.lrdb.set(rdb,v or "",flags)
end

--
-- !!! warning !!!
--
-- Those buffer RDB access functions are reading a copy of RDB variables from memory before reading from RDB itself.
-- Use this feature only for read-only RDB variables. Do not use the RDB variables that are written by other processes
--
-- !!! warning !!!
--

-------------------------------------------------------------------------------------------------------------------
-- Write RDB variable via internal memory.
-- This function writes a given variable to RDB as well as stores into the internal memory.
-- !!! warning !!! do not abuse this function that consumes memory
--
-- @param rdb RDB variable string name to write.
-- @param v RDB value to write to the RDB varaible.
-- @param flags flags to change the RDB flags
--        nil -> keep original flags
--        blank string ("")  -> clear all flags
--        others -> set new flags
-- @return True is returned when it succeeds. Otherwise, False.
function WmmdRdb:set_via_buf(rdb,v,flags)
  v=self.conv_val_to_rdbval(v)

  self.l.log("LOG_DEBUG",string.format("[RDB] buff-write '%s' ==> [%s]",v,rdb))

  self.rdb_init_table[rdb] = true
  self.rdb_chg_table[rdb] = v

  return self.lrdb.set(rdb,v,flags)
end

-------------------------------------------------------------------------------------------------------------------
-- identical function to set_via_buf() except add WMMD prefix to RDB
function WmmdRdb:setp_via_buf(rdb,v,flags)
  local pr = self.config.rdb_g_prefix .. rdb
  return self:set_via_buf(pr,v,flags)
end

-- !!! warning !!! do not abuse this set_if_chg as the function consumes memory
function WmmdRdb:set_if_chg(rdb,v,flags)
  if not self.rdb_init_table[rdb] or (self.rdb_chg_table[rdb] ~= v) then

    self.l.log("LOG_DEBUG",string.format("[RDB] write[cond] '%s' ==> [%s]",rdb,v))

    return self:set_via_buf(rdb,v,flags)
  end

  return false
end

function WmmdRdb:get(rdb)
  local v = self.lrdb.get(rdb)
  self.l.log("LOG_DEBUG",string.format("[RDB] read '%s' <== [%s]",v,rdb))
  return v
end

-------------------------------------------------------------------------------------------------------------------
-- Write a value to a WMMD RDB variable
--
-- @param rdb RDB variable string name to write. WMMD prefix will be added to this RDB.
-- @param v RDB value to write to the RDB varaible.
-- @param flags flags to change the RDB flags
--        nil -> keep original flags
--        blank string ("")  -> clear all flags
--        others -> set new flags
-- @return True is returned when it succeeds. Otherwise, False.
function WmmdRdb:setp(rdb,v,flags)

  v=self.conv_val_to_rdbval(v)

  local pr = self.config.rdb_g_prefix .. rdb

  self.l.log("LOG_DEBUG",string.format("[RDB] write '%s' ==> [%s]",v,pr))
  return self.lrdb.set(pr,v,flags)
end

-------------------------------------------------------------------------------------------------------------------
-- Read RDB variable via internal memory.
-- This function tries to read the RDB variable only when the RDB variable does not exist in the internal memory.
-- !!! warning !!! do not abuse this function that consumes memory
--
-- @param rdb RDB variable string name to read.
-- @return True is returned when it succeeds. Otherwise, False.
function WmmdRdb:get_via_buf(rdb)
  local v

  if self.rdb_init_table[rdb] then
    v = self.rdb_chg_table[rdb]
  else
    v = self.lrdb.get(rdb)
  end

  self.l.log("LOG_DEBUG",string.format("[RDB] buff-read '%s' <== [%s]",v,rdb))

  return v
end

-------------------------------------------------------------------------------------------------------------------
-- identical function to get_via_buf() except add WMMD prefix to RDB
function WmmdRdb:getp_via_buf(rdb)
  local pr = self.config.rdb_g_prefix .. rdb
  return self:get_via_buf(pr)
end

-- !!! warning !!! do not abuse this set_if_chg as the function consums memory
function WmmdRdb:setp_if_chg(rdb,v,flags)
  v=self.conv_val_to_rdbval(v)

  local pr = self.config.rdb_g_prefix .. rdb

  if not self.rdb_init_table[pr] or (self.rdb_chg_table[pr] ~= v) then

    self.l.log("LOG_DEBUG",string.format("[RDB] write[cond] '%s' ==> [%s]",v,pr))
    return self:set_via_buf(pr,v,flags)
  end

  return false
end


function WmmdRdb:unset(rdb)
  local pr = rdb

  self.l.log("LOG_DEBUG",string.format("[RDB] unset [%s]",pr))
  return self.lrdb.unset(pr)
end

function WmmdRdb:unsetp(rdb)
  local pr = self.config.rdb_g_prefix .. rdb

  self.l.log("LOG_DEBUG",string.format("[RDB] unset [%s]",pr))
  return self.lrdb.unset(pr)
end

function WmmdRdb:getp(rdb)

  local pr = self.config.rdb_g_prefix .. rdb
  local v = self.lrdb.get(self.config.rdb_g_prefix .. rdb)

  self.l.log("LOG_DEBUG",string.format("[RDB] read '%s' <== [%s]",v,pr))
  return v
end

---------------------------------------------------------------------------------------------------------------------
-- register a RDB for notification
--
-- @param rdb RDB variable to monitor.
-- @param noti Name of callback member function of obj to be called.
-- @param obj object on which member function will be called
function WmmdRdb:watchp(rdb, noti, obj)
  local pr = self.config.rdb_g_prefix .. rdb
  self.rdbWatch:addObserver(pr, noti, obj)

  self.l.log("LOG_DEBUG",string.format("[RDB] monitor [%s]",pr))
end

return WmmdRdb
