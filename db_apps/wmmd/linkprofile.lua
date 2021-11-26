-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- execution module that manages link.profile RDBs


require('stringutil')
-----------------------------------------------------------------------------------------------------------------------
-- LinkProfileModemParam is a singleton object of LinkProfileModemParamClass that maintains global modem parameters
-- such as SUPL, UT, Default Profile and etc.

local LinkProfileModemParamClass = {}
LinkProfileModemParamClass.__index = LinkProfileModemParamClass

function LinkProfileModemParamClass:new()

  local o = {
    l = require("luasyslog"),
    watcher = require("wmmd.watcher"),

    -- mapping table from modem parameter to invoke parameters and APN types
    modem_param_info_collection = {
      ["default_profile_number"] = {
        invoke_get="get_default_profile_number",
        invoke_set="set_default_profile_number",
        invoke_param="default_profile_number",
        apn_type="default",
      },

      ["supl_profile_number"] = {
        invoke_get="get_supl_profile_number",
        invoke_set="set_supl_profile_number",
        invoke_param="supl_profile_number",
        apn_type="supl",
      },

      ["ut_profile_apn"] = {
        invoke_get="get_ut_profile_apn",
        invoke_set="set_ut_profile_apn",
        invoke_param="ut_profile_apn",
        apn_type="ut",
      },
    },

  }

  return setmetatable(o, LinkProfileModemParamClass)
end

-----------------------------------------------------------------------------------------------------------------------
-- Write modem parameter to modem
--
-- @param modem_param modem parameter name to change
-- @value new value for modem parameter
--
function LinkProfileModemParamClass:set_param(modem_param,value)
  self.l.log("LOG_DEBUG",string.format("[custom-apn] set_param() is called to set modem param (modem_param='%s',value='%s')",modem_param,value))

  local modem_param_info = self.modem_param_info_collection[modem_param]

  if not modem_param_info then
    self.l.log("LOG_ERR",string.format("[custom-apn] unknown modem parameter to set (modem_param='%s',value='%s')",modem_param,value))
    return
  end

  local invoke = modem_param_info.invoke_set
  local invoke_params={}

  invoke_params[modem_param_info.invoke_param] = value

  if not self.watcher.invoke("sys",invoke,invoke_params) then
    self.l.log("LOG_ERR",string.format("[custom-apn] failed to set modem information via invoke (modem_param='%s',invoke='%s')",modem_param,invoke))
    return
  end

  -- remove memory modem parameter to re-read the parameter for the next read attempt.
  -- we do not rely on what we write. Instead, we read the parameter later.
  modem_param_info.value = nil
end

-----------------------------------------------------------------------------------------------------------------------
-- Read modem parameter from modem
--
-- This function minimizes modem read activity by maintaining modem parameters in memory
--
-- @param modem_param modem parameter name to change
-- @return modem parameter value
--
function LinkProfileModemParamClass:get_param(modem_param)
  self.l.log("LOG_DEBUG",string.format("[custom-apn] get_param() is called to get modem param (modem_param='%s')",modem_param))

  local modem_param_info = self.modem_param_info_collection[modem_param]

  if not modem_param_info then
    self.l.log("LOG_ERR",string.format("[custom-apn] unknown modem parameter to get (modem_param='%s')",modem_param))
    return nil
  end

  local invoke

  if not modem_param_info.value then
    invoke = modem_param_info.invoke_get
    local invoke_params={}

    if not self.watcher.invoke("sys",invoke,invoke_params) then
      self.l.log("LOG_ERR",string.format("[custom-apn] failed to obtain modem information via invoke (modem_param='%s',invoke='%s')",modem_param,invoke))
      return nil
    end

    modem_param_info.value = invoke_params[modem_param_info.invoke_param]
  end

  self.l.log("LOG_DEBUG",string.format("[custom-apn] get modem param result (invoke='%s',modem_param='%s',value='%s')",invoke,modem_param,modem_param_info.value))

  return modem_param_info.value
end

-----------------------------------------------------------------------------------------------------------------------
-- Write RDB APN types to modem
--
-- This function parses comma-separated RDB APN types and modify correct modem parameters.
--
-- @profile_number profile index to change APN types
-- @apn_name APN name to change APN types
-- @rdb_apn_type comma-separated APN types like "default,supl,ut"
--
function LinkProfileModemParamClass:set_apn_type(profile_number,apn_name,rdb_apn_type)

  self.l.log("LOG_DEBUG",string.format("[custom-apn] set apn type (profile_no=%s,apn='%s',apn_type='%s')",profile_number,apn_name,rdb_apn_type))

  -- build apn type flags
  local apn_type = {}
  for _,v in ipairs(rdb_apn_type:split(",")) do
    self.l.log("LOG_DEBUG",string.format("[custom-apn] set each of rdb apn types (value='%s')",v))
    apn_type[v]=true
  end

  -- build modem apn type
  local modem_apn_type = {
    {modem_param="default_profile_number", value=apn_type["default"] and profile_number},
    {modem_param="supl_profile_number", value=apn_type["supl"] and profile_number},
    {modem_param="ut_profile_apn", value=apn_type["ut"] and apn_name},
  }

  -- write modem apn type to modem
  for _,v in ipairs(modem_apn_type) do
    if v.value then
      self.l.log("LOG_DEBUG",string.format("[custom-apn] set modem param (name='%s',value='%s')",v.modem_param,v.value))
      self:set_param(v.modem_param,v.value)
    end
  end

end

-----------------------------------------------------------------------------------------------------------------------
-- read RDB APN types from modem
--
-- This function generates comma-separated RDB APN types from correct modem parameters.
--
-- @profile_number profile index to read APN types
-- @apn_name APN name to read APN types
-- @return comma-separated APN types like "default,supl,ut"
--
function LinkProfileModemParamClass:get_apn_type(profile_number,apn_name)

  self.l.log("LOG_DEBUG",string.format("[custom-apn] get apn type (profile_no=%s,apn='%s')",profile_number,apn_name))

  local modem_param_info_collection = self.modem_param_info_collection

  -- read apn type from modem
  local modem_apn_type = {
    profile_number and (self:get_param("default_profile_number") == profile_number) and modem_param_info_collection["default_profile_number"].apn_type or false,
    profile_number and (self:get_param("supl_profile_number") == profile_number) and modem_param_info_collection["supl_profile_number"].apn_type or false,
    apn_name and (self:get_param("ut_profile_apn") == apn_name) and modem_param_info_collection["ut_profile_apn"].apn_type or false,
  }

  -- build rdb apn type
  local rdb_apn_type = {}
  for k,v in ipairs(modem_apn_type) do
    if v then
      self.l.log("LOG_DEBUG",string.format("[custom-apn] get each of rdb apn types (value='%s')",v))
      table.insert(rdb_apn_type,v)
    end
  end

  local rdb_apn_type_to_write = table.concat(rdb_apn_type,",")
  self.l.log("LOG_DEBUG",string.format("[custom-apn] get apn type result (profile_no=%d,apn='%s',apn_type='%s')",profile_number,apn_name,rdb_apn_type_to_write))

  return rdb_apn_type_to_write
end


-- create the singleton object from LinkProfileModemParamClass
local LinkProfileModemParam = LinkProfileModemParamClass:new()

-----------------------------------------------------------------------------------------------------------------------

local LinkProfile = require("wmmd.Class"):new()

-- set to global for external process to access LinkProfileModemParamClass.
LinkProfile.LinkProfileModemParam = LinkProfileModemParam

function LinkProfile:setup(rdbWatch, wrdb)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("linkprofile", "LOG_DAEMON") end)

  self.t = require("turbo")
  self.ffi = require("ffi")

  -- [pdn-conn-backoff] stat RDB members in "link.profile.x.backoff.stat"
  self.backoff_stat_rdb_members = {"attempt_count","current_conn_delay"}

  self.luaq = require("luaqmi")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.wrdb = wrdb
  self.rdb = require("luardb")
  self.rdbWatch = rdbWatch
  self.config = require("wmmd.config")

  self.util = require "turbo.util"

  self.i = self.t.ioloop.instance()

  self.lps={}

  self.linkprofile_max_profile = 6

  self.stateMachineHandlers = {
    {state="lps:idle",func="state_machine_idle", execObj=self},

    {state="lps:wait_for_connection_conditions",func="state_machine_wait_for_connection_conditions", execObj=self},

    -- connect
    {state="lps:do_backoff_to_connect",func="state_machine_do_backoff_to_connect", execObj=self},
    {state="lps:connect",func="state_machine_connect", execObj=self},
    {state="lps:connect_ipv4",func="state_machine_connect_ipv4", execObj=self},
    {state="lps:post_connect_ipv4",func="state_machine_post_connect_ipv4", execObj=self},
    {state="lps:connect_ipv6",func="state_machine_connect_ipv6", execObj=self},
    {state="lps:post_connect_ipv6",func="state_machine_post_connect_ipv6", execObj=self},
    {state="lps:post_connect",func="state_machine_post_connect", execObj=self},
    {state="lps:online",func="state_machine_online", execObj=self},

    -- disconnect
    {state="lps:disconnect",func="state_machine_disconnect", execObj=self},
    {state="lps:disconnect_ipv6",func="state_machine_disconnect_ipv6", execObj=self},
    {state="lps:post_disconnect_ipv6",func="state_machine_post_disconnect_ipv6", execObj=self},
    {state="lps:disconnect_ipv4",func="state_machine_disconnect_ipv4", execObj=self},
    {state="lps:post_disconnect_ipv4",func="state_machine_post_disconnect_ipv4", execObj=self},
    {state="lps:post_disconnect",func="state_machine_post_disconnect", execObj=self},

    {state="lps:write",func="state_machine_write", execObj=self},
    {state="lps:read",func="state_machine_read", execObj=self},
  }
end

--[[ actions ]]--
function LinkProfile:invoke_stop_rmnet(lp,ipv6_enable)

  local module_profile_idx = lp.rdb_members.module_profile_idx

  if not module_profile_idx then
    self.l.log("LOG_DEBUG",string.format("[pf-mapping] module profile index not assigned. ignore stop command (lidx=%d)",lp.lp_index))
    return false
  end

  local succ = self.watcher.invoke("sys","stop_rmnet",{
    profile_index=module_profile_idx,
    service_id=ipv6_enable and (lp.lp_index+self.linkprofile_max_profile) or lp.lp_index,
  })

  return succ
end

--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Write backoff stat variables into RDB
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_update_stat_rdb(lp)
  local backoff_stat = lp.rdb_backoff_stat

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] write backoff stat variables into RDB",lp.lp_index))

  for _,rdb in pairs(self.backoff_stat_rdb_members) do
    self.wrdb:set(lp.rdb_prefix_backoff_stat .. rdb,tonumber(backoff_stat[rdb]) or 0)
  end
end

--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Cancel backoff online timer
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_online_timer_cancel(lp)
  if not lp.backoff_online_timer then
    return
  end

  local backoff_conf = lp.rdb_backoff_conf
  local sec = tonumber(backoff_conf.minimum_online_sec)

  -- remove timer
  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] cancel backoff minimum online timer (timer=%d sec)",lp.lp_index,sec))
  self.i:remove_timeout(lp.backoff_online_timer)

  lp.backoff_online_timer=nil
end

--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Reset backoff stat variables and write the variables into RDB
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_reset_stat_and_update_rdb(lp)
  local backoff_stat = lp.rdb_backoff_stat

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] reset backoff stat variables",lp.lp_index))

  backoff_stat.attempt_count = 0
  backoff_stat.current_conn_delay = nil

  backoff_stat.backoff_suppress = false
  backoff_stat.backoff_suppress_count = 0

  self:backoff_update_stat_rdb(lp)
end

--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Schedule Online timer
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_online_timer_schedule(lp)
  local backoff_conf = lp.rdb_backoff_conf
  -- calculate timeout
  local sec = tonumber(backoff_conf.minimum_online_sec)
  local timeout = self.t.util.gettimemonotonic() + sec*1000

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] schedule minimum online timer (timer=%d sec)",lp.lp_index,sec))
  -- schedule timeout
  lp.backoff_online_timer = self.i:add_timeout(timeout,
    function()
      self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] minimum online timer got expired (timer=%d sec)",lp.lp_index,sec))
      lp.backoff_online_timer=nil

      self:backoff_reset_stat_and_update_rdb(lp)
    end
  )
end

--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Advance PDN connection attempt count
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_increase_attempt_and_update_rdb(lp)
  local backoff_stat = lp.rdb_backoff_stat

  backoff_stat.attempt_count = backoff_stat.attempt_count + 1
  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] increase attempt count (new count=%d)",lp.lp_index,backoff_stat.attempt_count))

  self:backoff_update_stat_rdb(lp)
end

--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Advance PDN connection attempt delay
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_increase_delay(lp)
  local backoff_conf = lp.rdb_backoff_conf
  local backoff_stat = lp.rdb_backoff_stat
  local sec = backoff_stat.current_conn_delay or 0
  local maximum_cap_delay = tonumber(backoff_conf.maximum_cap_delay)

  -- use initial delay if it is the first failure
  if not backoff_stat.current_conn_delay then
    self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] prepare next backoff delay - initial backoff delay (%d sec)",lp.lp_index,backoff_conf.initial_retry_delay))
    backoff_stat.current_conn_delay = backoff_conf.initial_retry_delay
    return
  end

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] increase attempt and delay",lp.lp_index))

  -- increase delay
  local increase_valid = string.match(backoff_conf.increase,"[%d/*+^]*")
  local increase_func = loadstring(string.format("return (%d %s)", backoff_stat.current_conn_delay, backoff_conf.increase))
  local increase_result = increase_func and increase_func()

  if increase_valid and increase_result then
    backoff_stat.current_conn_delay = (maximum_cap_delay and (increase_result>maximum_cap_delay)) and maximum_cap_delay or increase_result
    self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] prepare next backoff delay (delay=%d,param=%s)",lp.lp_index,backoff_stat.current_conn_delay,backoff_conf.increase))
  else
    self.l.log("LOG_NOTICE",string.format("[pdn-conn-backoff#%d] failed to prepare next backoff delay (delay=%d,param=%s)",lp.lp_index,sec,backoff_conf.increase))
  end
end


--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Set default value to backoff parameters
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_sanitize_settings(lp)
  local backoff_conf = lp.rdb_backoff_conf
  local backoff_stat = lp.rdb_backoff_stat

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] * sanitize backoff settings",lp.lp_index))


  -- if backoff is not configured, use legacy constant delay settings
  if not backoff_conf.initial_retry_delay then
    -- use legacy constant delay settings
    backoff_conf.initial_retry_delay = tonumber(lp.rdb_members.reconnect_delay)
    self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] .initial_retry_delay is not configured. use reconnect_delay(%d)",lp.lp_index,backoff_conf.initial_retry_delay or 0))
  end

  if not backoff_conf.increase then
    backoff_conf.increase = "+0"
    self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] .increase is not configured. use %s",lp.lp_index,backoff_conf.increase))
  end

  if not backoff_conf.maximum_consecutive_failure_count then
    -- use legacy constant delay settings
    backoff_conf.maximum_consecutive_failure_count = tonumber(lp.rdb_members.reconnect_retries)
    self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] .maximum_consecutive_failure_count not configured. use %s",lp.lp_index,backoff_conf.maximum_consecutive_failure_count))
  end

  -- if backoff is not yet configured, use default settings
  backoff_conf.initial_retry_delay = tonumber(backoff_conf.initial_retry_delay) or 30
  backoff_conf.maximum_cap_delay = tonumber(backoff_conf.maximum_cap_delay) or 120
  backoff_conf.maximum_consecutive_failure_count = tonumber(backoff_conf.maximum_consecutive_failure_count) or 0
  backoff_conf.minimum_online_sec = tonumber(backoff_conf.minimum_online_sec) or 10
  backoff_conf.request_timeout = tonumber(backoff_conf.request_timeout) or 30

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] * use following backoff settings",lp.lp_index))
  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] initial_retry_delay=%d",lp.lp_index,backoff_conf.initial_retry_delay))
  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] increase=%s",lp.lp_index,backoff_conf.increase))
  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] maximum_cap_delay=%d",lp.lp_index,backoff_conf.maximum_cap_delay))
  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] maximum_consecutive_failure_count=%d",lp.lp_index,backoff_conf.maximum_consecutive_failure_count))
  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] request_timeout=%d",lp.lp_index,backoff_conf.request_timeout))

  -- convert pf_enable to boolean if nil - for a readable debug message
  backoff_stat.pf_enable = backoff_stat.pf_enable or false
end

--------------------------------------------------------------------------------
-- [pdn-conn-backoff] Update profile enable stat
--
-- @param lp per-profile information (link-dot-profile object)
function LinkProfile:backoff_update_pf_enable(lp)
  local backoff_stat = lp.rdb_backoff_stat
  local new_pf_enable = lp.rdb_members.enable == "1"

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] check profile enable flag (%s ==> %s)",lp.lp_index,backoff_stat.pf_enable,new_pf_enable))

  if not backoff_stat.pf_enable and new_pf_enable then
    self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] update profile enable flag (%s ==> %s)",lp.lp_index,backoff_stat.pf_enable,new_pf_enable))
    self:backoff_reset_stat_and_update_rdb(lp)
  end

  backoff_stat.pf_enable = new_pf_enable
end

--------------------------------------------------------------------------------
-- Get module profile index candidate of Link.profile
--
-- @param lp per-profile information (link-dot-profile object)
-- @return return module profile index candidate
--
function LinkProfile:get_module_profile_idx(lp)

  if lp.rdb_members.module_profile_idx then
    self.l.log("LOG_DEBUG",string.format("[pf-mapping] use RDB module profile (lidx=%d,midx=%d)",lp.lp_index,lp.rdb_members.module_profile_idx))
    return lp.rdb_members.module_profile_idx
  end

  local module_profile_idx_candidate = lp.lp_index

  for lp_index = 1,self.linkprofile_max_profile do
    local l = self.lps[lp_index]

    self.l.log("LOG_DEBUG",string.format("[pf-mapping] link.profile to module profile mapping info (lidx=%d,midx=%s)",l.lp_index,l.rdb_members.module_profile_idx))

    if (l.lp_index ~= lp.lp_index) and (l.rdb_members.module_profile_idx == module_profile_idx_candidate) then
      self.l.log("LOG_DEBUG",string.format("[pf-mapping] candidate module profile not available. owned by link.profile.%d (lidx=%d,midx=%d)",l.lp_index,lp.lp_index,module_profile_idx_candidate))
      return nil
    end
  end

  self.l.log("LOG_DEBUG",string.format("[pf-mapping] try candidate module profile (lidx=%d,midx=%d)",lp.lp_index,module_profile_idx_candidate))
  return module_profile_idx_candidate
end

function LinkProfile:update_lp_rdb_members(lp)
  -- build rdb members
  lp.rdb_members = self.wrdb:enum(lp.rdb_prefix,
    function(k)
      local match_str = "^" .. string.gsub(lp.rdb_prefix,"%.","%%.") .. "([%w_%d]+)$"
      return string.match(k,match_str)
    end
  )

  -- build rdb members for backoff conf
  lp.rdb_backoff_conf = self.wrdb:enum(lp.rdb_prefix_backoff_conf,
    function(k)
      local match_str = "^" .. string.gsub(lp.rdb_prefix_backoff_conf,"%.","%%.") .. "([%w_%d]+)$"
      return string.match(k,match_str)
    end
  )

  -- use lp index
  lp.rdb_members.module_profile_idx = tonumber(lp.rdb_members.module_profile_idx)
  self.l.log("LOG_DEBUG",string.format("[pf-mapping] module profile index from RDB (lidx=%d,midx=%s)",lp.lp_index,lp.rdb_members.module_profile_idx))


  local rdb_pf_enable = lp.rdb_members.enable == "1"

  -- read link.profile settings
  self.l.log("LOG_DEBUG",string.format("* %s rdb members",lp.rdb_prefix))
  for k,v in pairs(lp.rdb_members) do
    self.l.log("LOG_DEBUG",string.format("%s%s='%s'",lp.rdb_prefix,k,v))
  end

  -- read backoff settings
  local backoff_conf = lp.rdb_backoff_conf
  self.l.log("LOG_DEBUG",string.format("* %s rdb backoff conf members",lp.rdb_prefix))
  for k,v in pairs(backoff_conf) do
    self.l.log("LOG_DEBUG",string.format("%s%s='%s'",lp.rdb_prefix_backoff_conf,k,v))
  end

  -- load default settings
  self:backoff_sanitize_settings(lp)

  -- update profile enable flag
  self:backoff_update_pf_enable(lp)

  lp.enable_ipv4 = rdb_pf_enable and ((lp.rdb_members.pdp_type == "ipv4v6") or (lp.rdb_members.pdp_type == "ipv4"))
  lp.enable_ipv6 = rdb_pf_enable and ((lp.rdb_members.pdp_type == "ipv4v6") or (lp.rdb_members.pdp_type == "ipv6"))

  -- log debug messages and error for special cases - one for profile disabled and one for profile enabled without ip stack configured
  if not rdb_pf_enable then
    self.l.log("LOG_DEBUG", string.format("!!! link.profile %d is disabled !!!" ,lp.lp_index))
  elseif rdb_pf_enable and not (lp.enable_ipv4 or lp.enable_ipv6) then
    self.l.log("LOG_ERR", string.format("!!! profile %d is enabled with no IP stack is configured, assume the profile is ipv4v6 !!!",lp.lp_index))

    lp.enable_ipv4 = true
    lp.enable_ipv6 = true
  end
end

LinkProfile.ALinkProfile = require("wmmd.Class"):new()
function LinkProfile.ALinkProfile:init(base, lp)
  self.lp = lp
  self.baseLinkProfile = base
end

--[[ rdb events ]]--

function LinkProfile.ALinkProfile:rdb_on_change_enable()
  local sl = self.lp.sl

  self.baseLinkProfile:update_lp_rdb_members(self.lp)

  self.baseLinkProfile:state_machine_perform_transit(self.lp)
end

function LinkProfile.ALinkProfile:rdb_on_change()
  local sl = self.lp.sl

  local new_rdb_writeflag
  local new_rdb_trigger
  local new_rdb_readflag

  -- catch signal
  self.baseLinkProfile.rdb.lock()
  do
    new_rdb_writeflag = self.baseLinkProfile.wrdb:get(self.lp.rdb_writeflag) == "1"
    self.lp.writeflag = self.lp.writeflag or new_rdb_writeflag

    new_rdb_trigger = self.baseLinkProfile.wrdb:get(self.lp.rdb_trigger) == "1"
    self.lp.trigger = self.lp.trigger or new_rdb_trigger

    -- read rdb readflag and maintain the information in LinkProfile
    new_rdb_readflag = self.baseLinkProfile.wrdb:get(self.lp.rdb_readflag) == "1"
    self.lp.readflag = self.lp.readflag or new_rdb_readflag

    self.baseLinkProfile:update_lp_rdb_members(self.lp)

    -- reset writeflag after actual write (see LinkProfile:state_machine_write)

    -- reset trigger
    if new_rdb_trigger then
      self.baseLinkProfile.wrdb:set(self.lp.rdb_trigger, "0")
    end

    -- reset readflag
    if new_rdb_readflag then
      self.baseLinkProfile.wrdb:set(self.lp.rdb_readflag, "0")
    end

    self.baseLinkProfile.rdb.unlock()
  end

  -- transit when we receive triggers
  if self.lp.writeflag or self.lp.trigger or self.lp.readflag then
    self.baseLinkProfile:state_machine_perform_transit(self.lp)
  end

end

--[[ state machine ]]--


function LinkProfile:state_machine_idle(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] idle - %s", lp.network_interface))

  self:state_machine_perform_transit(lp)
end

function LinkProfile:state_machine_perform_transit(lp)
  local sl = lp.sl
  local backoff_stat = lp.rdb_backoff_stat
  local sl_stat = sl.get_current_state()

  self.l.log("LOG_INFO",string.format("[transit#%d] sl=%s,midx=%s,on_4=%s,on_v6=%s,disa_4=%s,disa_6=%s,ena_4=%s,ena_6=%s",
    lp.lp_index,
    sl_stat,
    lp.rdb_members.module_profile_idx,
    lp.online_ipv4,
    lp.online_ipv6,
    lp.disallowed_ipv4, -- disallowed by network
    lp.disallowed_ipv6, -- disallowed by network
    lp.enable_ipv4,
    lp.enable_ipv6
  ))

  self.l.log("LOG_INFO",string.format("[transit#%d] sl=%s,hold=%s,irflag=%s,rflag=%s,wflag=%s,trig=%s",
    lp.lp_index,
    sl_stat,
    lp.onhold,
    lp.initial_readflag,
    lp.readflag,
    lp.writeflag,
    lp.trigger
  ))

  local wait_states = {
    ["lps:idle"]=true,
    ["lps:online"]=true,
    ["lps:do_backoff_to_connect"]=true,
    ["lps:wait_for_connection_conditions"]=true,
  }

  local connect_states = {
    ["lps:connect"]=true,
    ["lps:connect_ipv4"]=true,
    ["lps:post_connect_ipv4"]=true,
    ["lps:connect_ipv6"]=true,
    ["lps:post_connect"]=true,
    ["lps:online"]=true,
  }

  local to_disconnect_ipv4 = lp.online_ipv4 and not (lp.enable_ipv4 and not lp.disallowed_ipv4)
  local to_disconnect_ipv6 = lp.online_ipv6 and not (lp.enable_ipv6 and not lp.disallowed_ipv6)
  self.l.log("LOG_DEBUG",string.format("[dual-stack] profile to newly disconnect (ipv4=%s,ipv6=%s) - %s",to_disconnect_ipv4,to_disconnect_ipv6,lp.network_interface))

  -- disconnect before performing write, initial_readflag or trigger flags
  if connect_states[sl_stat] and (lp.initial_readflag or lp.writeflag or lp.trigger or lp.onhold or to_disconnect_ipv4 or to_disconnect_ipv6) then
    self.l.log("LOG_INFO",string.format("[dual-stack] re-start profile, disconnect - %s",lp.network_interface))
    sl.switch_state_machine_to("lps:disconnect")
    return
  end

  if not wait_states[sl_stat] then
    self.l.log("LOG_DEBUG",string.format("[dual-stack] state machine busy, retry later (stat=%s) - %s ",sl_stat,lp.network_interface))
    return
  end

  if lp.initial_readflag or lp.trigger or lp.writeflag then
    -- reset backoff stat as writeflag or trigger is written
    self.l.log("LOG_DEBUG",string.format("reset backoff timer"))
    self:backoff_reset_stat_and_update_rdb(lp)
  end

  lp.trigger = false

  -- do initial read
  if lp.initial_readflag then
    self.l.log("LOG_DEBUG",string.format("[transit#%d] switch to read for initial_readflag",lp.lp_index))
    sl.switch_state_machine_to("lps:read")
    luardb.set("service.wmmd.state", "initial_read")
    return
  end

  -- write
  if lp.writeflag then
    self.l.log("LOG_DEBUG",string.format("[transit#%d] switch to write",lp.lp_index))
    sl.switch_state_machine_to("lps:write")
    return
  end

  -- read
  if lp.readflag then
    self.l.log("LOG_DEBUG",string.format("[transit#%d] switch to read",lp.lp_index))
    sl.switch_state_machine_to("lps:read")
    return
  end

  -- do not process if onhold
  if lp.onhold then
    if sl_stat ~= "lps:idle" then
      self.l.log("LOG_DEBUG",string.format("[transit#%d] switch to idle.",lp.lp_index))
      sl.switch_state_machine_to("lps:idle")
    else
      self.l.log("LOG_DEBUG",string.format("[transit#%d] stay in idle.",lp.lp_index))
    end
    return
  end

  -- connect
  local to_connect_ipv4 = not lp.online_ipv4 and lp.enable_ipv4 and not lp.disallowed_ipv4
  local to_connect_ipv6 = not lp.online_ipv6 and lp.enable_ipv6 and not lp.disallowed_ipv6
  self.l.log("LOG_DEBUG",string.format("[dual-stack] profile to newly connect (ipv4=%s,ipv6=%s) - %s",to_connect_ipv4,to_connect_ipv6,lp.network_interface))

  -- connect
  if to_connect_ipv4 or to_connect_ipv6 then
    self.l.log("LOG_INFO",string.format("[dual-stack] profile enabled, connect - %s",lp.network_interface))
    local backoff_ps_recovered = backoff_stat.backoff_ps_recovered
    local backoff_suppress = backoff_stat.backoff_suppress
    local backoff_force_to_immediately_connect = backoff_stat.backoff_force_to_immediately_connect

    -- reset ps recovered and suppress flag
    backoff_stat.backoff_ps_recovered = false
    backoff_stat.backoff_suppress = false
    backoff_stat.backoff_force_to_immediately_connect = false

    if backoff_force_to_immediately_connect then
      -- directly switch to connect
      self.l.log("LOG_NOTICE",string.format("[pdn-conn-backoff#%d] force to immediately connect", lp.lp_index))

      sl.switch_state_machine_to("lps:connect")
    elseif backoff_ps_recovered then
      -- directly switch to connect as backoff delay is already applied
      self.l.log("LOG_NOTICE",string.format("[pdn-conn-backoff#%d] PS recovered, connect immediately", lp.lp_index))

      sl.switch_state_machine_to("lps:connect")
    elseif backoff_suppress and (backoff_stat.backoff_suppress_count < self.backoff_suppress_count_max) then
      -- previous connect failure has backoff suppressed
      backoff_stat.backoff_suppress_count = backoff_stat.backoff_suppress_count + 1
      self.l.log("LOG_NOTICE",string.format("[pdn-conn-backoff#%d] backoff suppressed %d times, connect immediately", lp.lp_index, backoff_stat.backoff_suppress_count))

      sl.switch_state_machine_to("lps:connect")
    else
      sl.switch_state_machine_to("lps:do_backoff_to_connect")
    end
  end

end

function LinkProfile:state_machine_write(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl
  local backoff_conf = lp.rdb_backoff_conf

  -- build bsettings to write
  local bsettings={
    ref=lp,
    profile_index=lp.rdb_members.module_profile_idx,
    apn=lp.rdb_members.apn,
    pdp_type=lp.rdb_members.pdp_type,
    auth_type=lp.rdb_members.auth_type,
    user=lp.rdb_members.user,
    pass=lp.rdb_members.pass,
    apn_type=lp.rdb_members.apn_type,
    emergency_calls=lp.rdb_members.emergency_calls,
    apn_type_mask=lp.rdb_members.apn_type_mask
  }

  -- write
  local succ = self.watcher.invoke("sys","write_rmnet",bsettings)
  if not succ then
    self.l.log("LOG_ERR",string.format("failed to write profile (index=%d)",lp.lp_index))
  end

  lp.writeflag = false

  -- reset writeflag (if set)
  if self.wrdb:get(lp.rdb_writeflag) == "1" then
      self.wrdb:set(lp.rdb_writeflag, "0")
  end

  sl.switch_state_machine_to(old_stat)
end

-------------------------------------------------------------------------------------------------------------------
-- linkprifile state - waiting for signal to reconnect
function LinkProfile:state_machine_wait_for_connection_conditions(old_stat,new_stat,stat_chg_info,lp)
  self.l.log("LOG_NOTICE",string.format("[linkprofile-ctrl] wait for PS attach #%d",lp.lp_index))
  self.wrdb:set(lp.rdb_prefix .. "connect_progress", "attaching")
end

function LinkProfile:state_machine_do_backoff_to_connect(old_stat,new_stat,stat_chg_info,lp)
  local backoff_conf = lp.rdb_backoff_conf
  local backoff_stat = lp.rdb_backoff_stat
  local sl = lp.sl
  local sec = backoff_stat.current_conn_delay or 0

  -- check maximum failure count
  local max_limit = tonumber(backoff_conf.maximum_consecutive_failure_count) or 0
  if (max_limit>0) and (lp.rdb_backoff_stat.attempt_count >= max_limit) then
    self.l.log("LOG_NOTICE",string.format("[pdn-conn-backoff#%d] failure count exceeded maximum limit, stop attempting to connect (max_limit=%d)",lp.lp_index,max_limit))
    return
  end

  -- update RDB
  self.wrdb:set(lp.rdb_prefix .. "connect_progress", "waiting")
  self:backoff_update_stat_rdb(lp)

  -- schedule to switch to connect
  sl.switch_state_machine_to("lps:connect",sec * 1000)

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] connect in %d sec, backoff and wait", lp.lp_index,sec))
end

function LinkProfile:state_machine_connect(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl
  local backoff_stat = lp.rdb_backoff_stat

  -- get PS attach state
  local ps_attach_state = self.wrdb:getp_via_buf("system_network_status.attached") == "1"

  -- switch to sleep if no reg or no ps attach
  if not ps_attach_state then
    self.l.log("LOG_NOTICE",string.format("[linkprofile-ctrl] PS not attached, immediately switch to 'sleep' state - %s / ps_attach_state=%s",lp.network_interface,ps_attach_state))

    sl.switch_state_machine_to("lps:wait_for_connection_conditions",0,true)
    return
  end

  self.l.log("LOG_DEBUG",string.format("[pdn-conn-backoff#%d] backoff expired, connect now (timer=%d)", lp.lp_index,backoff_stat.current_conn_delay or 0))

  self.l.log("LOG_INFO",string.format("[dual-stack] start connect - %s",lp.network_interface))
  self.wrdb:set(lp.rdb_prefix .. "connect_progress", "establishing")

  -- increase attempt count and update rdb
  self:backoff_increase_attempt_and_update_rdb(lp)

  -- increase delay for the next attempt
  self:backoff_increase_delay(lp)

  -- reset noeffect flags
  lp.online_ipv6_noeffect = false
  lp.online_ipv4_noeffect = false

  sl.switch_state_machine_to("lps:connect_ipv4")
end

function LinkProfile:state_machine_connect_ipv4(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] connect ipv4 - %s",lp.network_interface))

  -- skip if ipv4 is not enable
  if not lp.enable_ipv4 then
    self.l.log("LOG_INFO",string.format("[dual-stack] ipv4 is not enable, skip ipv4 - %s",lp.network_interface))
    sl.switch_state_machine_to("lps:post_connect_ipv4")
    return
  end

  -- skip if ipv4 is already online
  if lp.online_ipv4 then
    self.l.log("LOG_INFO",string.format("[dual-stack] ipv4 is already online, skip ipv4 - %s",lp.network_interface))
    sl.switch_state_machine_to("lps:post_connect_ipv4")
    return
  end

  local delay = lp.rdb_backoff_conf.request_timeout*1000

  -- do not attempt to connect if we don't know module profile index yet
  if not lp.rdb_members.module_profile_idx then
    self.l.log("LOG_DEBUG",string.format("[pf-mapping] module profile index not assigned. failed to connect ipv4 (lidx=%d)",lp.lp_index))
  else
    --[[
    -- !!! debug !!!
    --
    -- generate QMI timeout for start rmnet
    --

    if not one_time_lose_pkt_handle and lp.lp_index == 1 then

      one_time_lose_pkt_handle = true

      self.l.log("LOG_DEBUG",string.format("[dual-stack] !!!!! debug: getnerate QMI timeout (idx=%d)",lp.lp_index))

      delay = 1
    end
    --]]

    local succ = self.watcher.invoke("sys","start_rmnet",{
      network_interface=lp.network_interface,
      profile_index=lp.rdb_members.module_profile_idx,
      service_id=lp.lp_index,

      apn=lp.rdb_members.apn,
      pdp_type=lp.rdb_members.pdp_type,
      auth_type=lp.rdb_members.auth_type,
      user=lp.rdb_members.user,
      pass=lp.rdb_members.pass,
      timeout=delay,

      ipv6_enable=false,
    })

    --[[

    -- !!! debug !!!
    --
    -- generate RDB trigger event right after queuing start_rmnet but before receiving rmnet pkt handle.
    --

    if not one_time_trigger_set and lp.lp_index == 1 then
      one_time_trigger_set = true

      self.l.log("LOG_DEBUG",string.format("[dual-stack] !!!!! debug: trigger is set (idx=%d)",lp.lp_index))
      lp.trigger = true
      self:state_machine_perform_transit(lp)

      return
    end
    --]]
  end

  sl.switch_state_machine_to("lps:post_connect_ipv4",delay)
end

function LinkProfile:state_machine_post_connect_ipv4(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] connect ipv4 done (online_ipv4=%s) - %s",lp.online_ipv4,lp.network_interface))

  sl.switch_state_machine_to("lps:connect_ipv6")
end

function LinkProfile:state_machine_connect_ipv6(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] connect ipv6 - %s",lp.network_interface))

  -- skip if ipv6 is not enable
  if not lp.enable_ipv6 then
    self.l.log("LOG_INFO",string.format("[dual-stack] ipv6 is not enable, skip ipv6 - %s",lp.network_interface))
    sl.switch_state_machine_to("lps:post_connect_ipv6")
    return
  end

  -- skip if ipv6 is already online
  if lp.online_ipv6 then
    self.l.log("LOG_INFO",string.format("[dual-stack] ipv6 is already online, skip ipv6 - %s",lp.network_interface))
    sl.switch_state_machine_to("lps:post_connect_ipv6")
    return
  end

  local delay = lp.rdb_backoff_conf.request_timeout*1000

  -- do not attempt to connect if we don't know module profile index yet
  if not lp.rdb_members.module_profile_idx then
    self.l.log("LOG_DEBUG",string.format("[pf-mapping] module profile index not assigned. failed to connect ipv6 (lidx=%d)",lp.lp_index))
  else
    local succ = self.watcher.invoke("sys","start_rmnet",{
      network_interface=lp.network_interface,
      profile_index=lp.rdb_members.module_profile_idx,
      service_id=lp.lp_index+self.linkprofile_max_profile,

      apn=lp.rdb_members.apn,
      pdp_type=lp.rdb_members.pdp_type,
      auth_type=lp.rdb_members.auth_type,
      user=lp.rdb_members.user,
      pass=lp.rdb_members.pass,
      timeout=delay,

      ipv6_enable=true,
    })

  end

  sl.switch_state_machine_to("lps:post_connect_ipv6",delay)
end

function LinkProfile:state_machine_post_connect_ipv6(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] connect ipv6 done (online_ipv6=%s) - %s",lp.online_ipv6,lp.network_interface))

  sl.switch_state_machine_to("lps:post_connect")
end

function LinkProfile:state_machine_online(old_stat,new_stat,stat_chg_info,lp)
  self.l.log("LOG_INFO",string.format("[dual-stack] online - %s", lp.network_interface))

  -- reset last error result when we are online
  self.wrdb:set(lp.rdb_prefix .. "pdp_result")
  self.wrdb:set(lp.rdb_prefix .. "pdp_result_verbose")

  self:state_machine_perform_transit(lp)
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

function LinkProfile:state_machine_post_connect(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  -- bypass if no connection is up
  if not lp.online_ipv6 and not lp.online_ipv4 then
    self.l.log("LOG_INFO",string.format("[dual-stack] failed to connect, retry (online_ipv4=%s,online_ipv6=%s) - %s",lp.online_ipv4,lp.online_ipv6,lp.network_interface))
    self:state_machine_perform_transit(lp)

    return
  end

  --[[
  -- !!! debug !!!
  --
  -- generate NO EFFECT
  --

  if not one_time_no_effect and lp.lp_index == 1 then

    one_time_no_effect = true

    self.l.log("LOG_DEBUG",string.format("[dual-stack] !!!!! debug: generate NO EFFECT (idx=%d)",lp.lp_index))

    lp.online_ipv6 = false
    lp.online_ipv4 = false

    self:backoff_reset_stat_and_update_rdb(lp)
    sl.switch_state_machine_to("lps:idle")
    return
  end
  --]]

  -- IP stack information
  local ipstack={
    ipv4={
      serv_id=lp.lp_index,
      online=lp.online_ipv4,
      linkprofile_rdbs={"dns1","dns2","gw","mask","iplocal","mtu"},
      info={},
    },

    ipv6={
      serv_id=lp.lp_index+self.linkprofile_max_profile,
      online=lp.online_ipv6,
      linkprofile_rdbs={"ipv6_dns1","ipv6_dns2","ipv6_gw","ipv6_ipaddr"},
      info={},
    },
  }

  -- total succ flag
  local total_succ = true

  -- query IPv4 and IPv6 status
  for _,i in ipairs{"ipv4","ipv6"} do
    -- only if the stack is online
    if ipstack[i].online then
      self.l.log("LOG_DEBUG",string.format("[dual-stack] %s online - %s",i,lp.network_interface))

      local succ = self.watcher.invoke("sys","get_rmnet_stat",{
        service_id=ipstack[i].serv_id,
        info=ipstack[i].info,
      })

      if not succ then
        self.l.log("LOG_INFO",string.format("[dual-stack] failed to get %s IP information - %s",i,lp.network_interface))
      end

      -- collect succ flag
      total_succ = total_succ and succ
    else
      self.l.log("LOG_DEBUG",string.format("[dual-stack] %s offline - %s",i,lp.network_interface))
    end
  end

  -- populate ipv4 information
  lp.iplocal = ipstack.ipv4.info.iplocal
  lp.gw = ipstack.ipv4.info.gw

  local user_configured_mtu = tonumber(self.wrdb:get(lp.rdb_prefix .. "mtu")) or 1430
  if ipstack.ipv4.info.mtu ~= nil then
    self.l.log("LOG_DEBUG",string.format("Network MTU size - %s", ipstack.ipv4.info.mtu))
    -- Ensure user configured MTU is always within the network supported MTU size.
    ipstack.ipv4.info.mtu = (user_configured_mtu < ipstack.ipv4.info.mtu) and user_configured_mtu or ipstack.ipv4.info.mtu
  else
    ipstack.ipv4.info.mtu = user_configured_mtu
    self.l.log("LOG_DEBUG", string.format("Network MTU size unavailable, using default MTU - %s", ipstack.ipv4.info.mtu))
  end

  -- disconnect to reconnect if any of IP stack is failing to get information
  if not total_succ then
    self.l.log("LOG_INFO",string.format("[dual-stack] immediately, reconnect due to absence of IP information (online_ipv4=%s,online_ipv6=%s) - %s",lp.online_ipv4,lp.online_ipv6,lp.network_interface))
    sl.switch_state_machine_to("lps:disconnect")
    return
  end

  -- set interface name
  self.wrdb:set(lp.rdb_prefix .. "interface",lp.network_interface)

  -- update link.profile rdb variables
  for _,i in ipairs{"ipv4","ipv6"} do
    local info = ipstack[i].info
    for _,v in ipairs(ipstack[i].linkprofile_rdbs) do
      self.wrdb:set(lp.rdb_prefix .. v ,info[v])
    end
  end

  -- update status RDB variables
  local parms={}

--[[
#    $1   Interface name                ppp0
#    $2   The tty                       ttyS1
#    $3   The link speed                38400
#    $4   Local IP number               12.34.56.78
#    $5   Peer  IP number               12.34.56.99
#    $6   Optional ``ipparam'' value    foo
--]]

  self.wrdb:set(lp.rdb_prefix .. "connect_progress", "established")
  self.wrdb:set(lp.rdb_prefix .. "status_ipv4", lp.online_ipv4 and "up" or "down")
  self.wrdb:set(lp.rdb_prefix .. "status_ipv6", lp.online_ipv6 and "up" or "down")
  self.wrdb:set(lp.rdb_prefix .. "status","up")

  local sysuptime = get_sysuptime()
  local ipv4_sysuptime = string.trim(self.wrdb:get(lp.rdb_prefix .. "sysuptime_at_ipv4_up"))
  local ipv6_sysuptime = string.trim(self.wrdb:get(lp.rdb_prefix .. "sysuptime_at_ipv6_up"))
  local ifdev_sysuptime = string.trim(self.wrdb:get(lp.rdb_prefix .. "sysuptime_at_ifdev_up"))
  if lp.online_ipv4 and ipv4_sysuptime == "" then
    self.wrdb:set(lp.rdb_prefix .. "sysuptime_at_ipv4_up", sysuptime)
  end
  if lp.online_ipv6 and ipv6_sysuptime == "" then
    self.wrdb:set(lp.rdb_prefix .. "sysuptime_at_ipv6_up", sysuptime)
  end
  if ifdev_sysuptime == "" then
    self.wrdb:set(lp.rdb_prefix .. "sysuptime_at_ifdev_up", sysuptime)
  end


  table.insert(parms,self.config.ipup_script)
  table.insert(parms,"'" .. lp.network_interface .. "'")
  table.insert(parms,"''")
  table.insert(parms,"''")
  table.insert(parms,"'" .. (lp.iplocal or "") .. "'")
  table.insert(parms,"'" .. (lp.gw or "") .. "'")
  table.insert(parms,"'" .. lp.lp_index .. "'")
  local ipcmd = table.concat(parms," ")

  self.l.log("LOG_NOTICE",string.format("run ipchg script - %s",ipcmd))
  os.execute(ipcmd .. " 2> /dev/null > /dev/null")

  -- schedule online timer
  self:backoff_online_timer_schedule(lp)

  sl.switch_state_machine_to("lps:online")
end

function LinkProfile:state_machine_disconnect(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] start disconnect - %s",lp.network_interface))

  self.wrdb:set(lp.rdb_prefix .. "connect_progress", "disconnecting")

  -- reset disallowed flags
  self.l.log("LOG_INFO",string.format("[dual-stack] reset disallowed flags - %s",lp.network_interface))
  lp.disallowed_ipv4 = false
  lp.disallowed_ipv6 = false

  -- cancel online timer if existing
  self:backoff_online_timer_cancel(lp)

  sl.switch_state_machine_to("lps:disconnect_ipv6")
end

function LinkProfile:state_machine_disconnect_ipv6(old_stat,new_stat,stat_chg_info,lp)

  local sl = lp.sl
  local backoff_conf = lp.rdb_backoff_conf

  self.l.log("LOG_INFO",string.format("[dual-stack] disconnect ipv6 - %s",lp.network_interface))

  -- * disconnect ipv6
  --
  -- If the connection is up, the disconnect attempt will succeed and we will wait for disconnect indiciation. Otherwise,
  -- we assume that there is no connection up.

  if self:invoke_stop_rmnet(lp,true) then
    self.l.log("LOG_INFO",string.format("[dual-stack] disconnect ipv6, wait for disconnection - %s",lp.network_interface))
    sl.switch_state_machine_to("lps:post_disconnect_ipv6",backoff_conf.request_timeout)
    return
  end

  sl.switch_state_machine_to("lps:post_disconnect_ipv6")

end

function LinkProfile:state_machine_post_disconnect_ipv6(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] disconnect ipv6 done (online_ipv6=%s) - %s",lp.online_ipv6,lp.network_interface))

  sl.switch_state_machine_to("lps:disconnect_ipv4")
end


function LinkProfile:state_machine_disconnect_ipv4(old_stat,new_stat,stat_chg_info,lp)

  local sl = lp.sl
  local backoff_conf = lp.rdb_backoff_conf

  self.l.log("LOG_INFO",string.format("[dual-stack] disconnect ipv4 - %s",lp.network_interface))

  -- * disconnect ipv4
  --
  -- If the connection is up, the disconnect attempt will succeed and we will wait for disconnect indiciation. Otherwise,
  -- we assume that there is no connection up.

  if self:invoke_stop_rmnet(lp,false) then
    self.l.log("LOG_INFO",string.format("[dual-stack] disconnect ipv4, wait for disconnection - %s",lp.network_interface))
    sl.switch_state_machine_to("lps:post_disconnect_ipv4",backoff_conf.request_timeout)
    return
  end

  sl.switch_state_machine_to("lps:post_disconnect_ipv4")
end

function LinkProfile:state_machine_post_disconnect_ipv4(old_stat,new_stat,stat_chg_info,lp)

  local sl = lp.sl

  self.l.log("LOG_INFO",string.format("[dual-stack] disconnect ipv4 done (online_ipv4=%s) - %s",lp.online_ipv4,lp.network_interface))

  sl.switch_state_machine_to("lps:post_disconnect")
end

function LinkProfile:state_machine_post_disconnect(old_stat,new_stat,stat_chg_info,lp)
  local sl = lp.sl

  local parms={}

  --[[
#    $1   Interface name                ppp0
#    $2   The tty                       ttyS1
#    $3   The link speed                38400
#    $4   Local IP number               12.34.56.78
#    $5   Peer  IP number               12.34.56.99
#    $6   Optional ``ipparam'' value    foo
--]]

  self.l.log("LOG_INFO",string.format("[dual-stack] disconnect done (online_ipv6=%s,online_ipv4=%s) - %s",lp.online_ipv6,lp.online_ipv4,lp.network_interface))

  self.wrdb:set(lp.rdb_prefix .. "connect_progress", "disconnected")
  self.wrdb:set(lp.rdb_prefix .. "status_ipv4", lp.online_ipv4 and "up" or "down")
  self.wrdb:set(lp.rdb_prefix .. "status_ipv6", lp.online_ipv6 and "up" or "down")
  self.wrdb:set(lp.rdb_prefix .. "status","down")

  local ipv4_sysuptime = string.trim(self.wrdb:get(lp.rdb_prefix .. "sysuptime_at_ipv4_up"))
  local ipv6_sysuptime = string.trim(self.wrdb:get(lp.rdb_prefix .. "sysuptime_at_ipv6_up"))
  local ifdev_sysuptime = string.trim(self.wrdb:get(lp.rdb_prefix .. "sysuptime_at_ifdev_up"))
  if not lp.online_ipv4 and ipv4_sysuptime ~= "" then
    self.wrdb:set(lp.rdb_prefix .. "sysuptime_at_ipv4_up", "")
  end
  if not lp.online_ipv6 and ipv6_sysuptime ~= "" then
    self.wrdb:set(lp.rdb_prefix .. "sysuptime_at_ipv6_up", "")
  end
  if not lp.online_ipv4 and not lp.online_ipv6 and ifdev_sysuptime ~= "" then
    self.wrdb:set(lp.rdb_prefix .. "sysuptime_at_ifdev_up", "")
  end

  table.insert(parms,self.config.ipdn_script)
  table.insert(parms,"'" .. lp.network_interface .. "'")
  table.insert(parms,"''")
  table.insert(parms,"''")
  table.insert(parms,"''")
  table.insert(parms,"''")
  table.insert(parms,"'" .. lp.lp_index .. "'")
  local ipcmd = table.concat(parms," ")

  self.l.log("LOG_NOTICE",string.format("run ipchg script - %s",ipcmd))
  os.execute(ipcmd .. " 2> /dev/null > /dev/null")

  sl.switch_state_machine_to("lps:idle")
end

-----------------------------------------------------------------------------------------------------------------------
-- Read profile settings from modem and populate link.profile RDBs
--
-- @param lp link.profile RDB object
-- @return true when it succeeds. Otherwise, false.
--
function LinkProfile:read_link_profile(lp)
  self.l.log("LOG_DEBUG",string.format("[pf-mapping] read module profile (lidx=%d)",lp.lp_index))
  local module_profile_idx = self:get_module_profile_idx(lp)

  -- read rmnet profile
  self.l.log("LOG_DEBUG",string.format("read link profile from modem (lp_idx=%d)",lp.lp_index))
  if not self.watcher.invoke("sys","read_rmnet",{ref=lp,profile_index=module_profile_idx}) then
    self.l.log("LOG_ERR",string.format("failed to read profile (index=%d)",lp.lp_index))
    return false
  end

  -- update rdb members
  self:update_lp_rdb_members(lp)

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- Handle link.profile readflag RDB
--
-- @param old_stat previous state of the state machine
-- @param new_stat current state of the state machine
-- @param stat_chg_info indication is true when the function is called for transit
-- @param lp link.profile RDB object
--
function LinkProfile:state_machine_read(old_stat,new_stat,stat_chg_info,lp)
  local sl=lp.sl

  if not self:read_link_profile(lp) then
    self.l.log("LOG_ERR",string.format("failed to read profile in state_machine_read (index=%d)",lp.lp_index))
  end

  -- clear read flags
  lp.initial_readflag = false
  lp.readflag = false

  -- switch back to the previous state.
  sl.switch_state_machine_to(old_stat)
end

--[[ cbs_system ]]--
-- watcher's handlers --

-- read profiles from modem
function LinkProfile:read_linkprofiles(type, event)
  local sl

  for _,lp in pairs(self.lps) do
    sl=lp.sl

    lp.initial_readflag = true
    sl.switch_state_machine_to("lps:idle")
  end

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- Log profile setting argument
--
-- @param log_name string for log heading
-- @param lp_index link.profile object index number
-- @param a profile setting arguments to log
--
function LinkProfile:log_rmnet_arguments(log_name,lp_index,a)
  self.l.log("LOG_DEBUG",string.format("* %s #%d",log_name,lp_index))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] profile_index=%s",log_name,lp_index,a.profile_index))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] module_profile_idx=%s",log_name,lp_index,a.module_profile_idx))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] apn=%s",log_name,lp_index,a.apn or "[NULL]"))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] pdp_type='%s'",log_name,lp_index,a.pdp_type or "[NULL]"))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] auth_type='%s'",log_name,lp_index,a.auth_type or "[NULL]"))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] user='%s'",log_name,lp_index,a.user or "[NULL]"))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] pass='%s'",log_name,lp_index,a.pass and string.gsub(a.pass,"^(.).*","%1****") or "[NULL]"))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] apn_type='%s'",log_name,lp_index,a.apn_type or "[NULL]"))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] emergency_calls='%s'",log_name,lp_index,a.emergency_calls or "[NULL]"))
  self.l.log("LOG_DEBUG",string.format("[%s #%d] apn_type_mask='%s'",log_name,lp_index,a.apn_type_mask or "[NULL]"))
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Peform post procedure of write_rmnet invoke
--
-- When QMI WDS finishes "write_rmnet", this function is invoked as an event. The function writes APN types to modem and
-- re-attach if required.
--
-- @param type invoke type
-- @param event event name
-- @param a argument
-- @return true when succeeds. Otherwise, false.
--
function LinkProfile:write_rmnet_on_write(type,event,a)
  local lp=a.ref

  -- write_rmnet is able to change profile index if the profile is newly created
  local profile_index = a.module_profile_idx or a.profile_index

  self.l.log("LOG_DEBUG",string.format("[custom-apn] write_rmnet_on_write() (profile_no=%s,apn='%s',apn_type='%s')",profile_index,a.apn,a.apn_type))

  -- log arguments
  self.l.log("LOG_DEBUG",string.format("[custom-apn] set apn type to modem (profile_no=%s,apn='%s',apn_type='%s')",profile_index,a.apn,a.apn_type))
  self:log_rmnet_arguments("write_rmnet_on_write",lp.lp_index,a)

  -- set apn type
  LinkProfileModemParam:set_apn_type(profile_index,a.apn,a.apn_type)

  -- update profile index
  self.l.log("LOG_DEBUG",string.format("[pf-mapping] module profile is updated by on_write (lidx=%d,midx=%s)",lp.lp_index,profile_index))
  self.wrdb:set(lp.rdb_prefix .. "module_profile_idx", profile_index, "p")

  -- re-attach is required if the modified profile is default profile
  if profile_index and (LinkProfileModemParam:get_param("default_profile_number") == profile_index) then
    self.l.log("LOG_DEBUG",string.format("[custom-apn] default profile is changed, do reattach (profile_no=%s,apn='%s',apn_type='%s')",profile_index,a.apn,a.apn_type))
    self.watcher.invoke("sys","reattach")
  end

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- [INVOKE FUNCTION] Peform post procedure of read_rmnet invoke
--
-- When QMI WDS finishes "read_rmnet", this function is invoked as an event. The function updates link.profile RDBs
--
-- @param type invoke type
-- @param event event name
-- @param a argument
-- @return true when succeeds. Otherwise, false.
--
function LinkProfile:read_rmnet_on_read(type,event,a)
  local lp=a.ref

  local profile_index = a.valid and a.profile_index or nil

  if a.valid then
    self.l.log("LOG_DEBUG",string.format("[custom-apn] read_rmnet_on_read() - profile available (lp_index=%d,profile_no=%s,apn='%s')",lp.lp_index,profile_index,a.apn))
  else
    self.l.log("LOG_DEBUG",string.format("[custom-apn] read_rmnet_on_read() - profile not available (lp_index=%d,profile_no=%s,apn='%s')",lp.lp_index,profile_index,a.apn))
  end

  -- build apn type
  a.apn_type = a.valid and LinkProfileModemParam:get_apn_type(profile_index,a.apn) or nil
  self.l.log("LOG_DEBUG",string.format("[custom-apn] get apn type from modem (profile_no=%s,apn='%s',apn_type='%s')",profile_index,a.apn,a.apn_type))

  local rdb_prefix = string.format("link.profile.%d.",lp.lp_index)

  if lp.rdb_members.overwrite_modem == "1" then
    -- always overwrite modem with RDB
    for _, v in ipairs{"apn","user","pass","pdp_type","auth_type","apn_type","emergency_calls","apn_type_mask"}  do
      self.l.log("LOG_DEBUG", string.format("lp%d.%s: %s vs %s", lp.lp_index, v, lp.rdb_members[v], a[v]))
      if lp.rdb_members[v] ~= a[v] then
        self.l.log("LOG_NOTICE", string.format("lp%d overwrite modem due to %s", lp.lp_index, v))
        -- mark writeflag so that next state_machine_perform_transit call will pick up the job
        lp.writeflag = true
        break
      end
    end
  else
    -- write to rdb
    for _,v in ipairs{"apn","user","pass","pdp_type","auth_type","apn_type","emergency_calls","apn_type_mask"} do
      self.wrdb:set(rdb_prefix .. v, a[v])
    end
  end

  -- disable emergency call support on all profiles by default as we don't support voice
  if a["emergency_calls"] and a["emergency_calls"] ~= "0" then
    self.wrdb:set(rdb_prefix .. "emergency_calls","0")
    lp.writeflag = true
  end

  -- update profile index
  self.l.log("LOG_DEBUG",string.format("[pf-mapping] module profile is updated by on_read (lidx=%d,midx=%s)",lp.lp_index,profile_index))
  self.wrdb:set(rdb_prefix .. "module_profile_idx",profile_index, "p")

  -- log arguments
  self:log_rmnet_arguments("read_rmnet_on_read",lp.lp_index,a)

  return true
end

-- wake up any sleeping link.profiles
function LinkProfile:wakeup_linkprofiles(type, event)

  for _,lp in pairs(self.lps) do
    local sl=lp.sl

    local backoff_stat = lp.rdb_backoff_stat
    local sl_stat = sl.get_current_state()

    if sl_stat == "lps:wait_for_connection_conditions" then
      -- set ps recovered flag
      backoff_stat.backoff_ps_recovered = true

      self.l.log("LOG_DEBUG",string.format("[linkprofile-ctrl] wake up profile (service_id=%d)",lp.lp_index))
      self:state_machine_perform_transit(lp)
    end
  end

  return true
end

-----------------------------------------------------------------------------------------------------------------------
-- [invoke] Reset back-off timers of all profiles and wake up the profiles
--
-- @param type invoke type. it is always "sys" for this function
-- @param event event name.
-- @return true when it succeeds. Otherwise, false.
--
function LinkProfile:reset_and_wakeup_linkprofiles(type, event)

  self.l.log("LOG_DEBUG","reset and wake up link.profiles")

  for _,lp in pairs(self.lps) do
    local sl=lp.sl

    local backoff_stat = lp.rdb_backoff_stat
    local sl_stat = sl.get_current_state()

    self.l.log("LOG_DEBUG",string.format("[custom-apn] reset profile back-off #%d",lp.lp_index))
    self:backoff_reset_stat_and_update_rdb(lp)

    if sl_stat == "lps:wait_for_connection_conditions" or sl_stat == "lps:do_backoff_to_connect" then
      backoff_stat.backoff_force_to_immediately_connect = true

      self.l.log("LOG_DEBUG",string.format("[custom-apn] ignore back-off timer and switch to 'transit' #%d",lp.lp_index))
      self:state_machine_perform_transit(lp)
    end
  end

  return true
end

-- start link.profiles
function LinkProfile:start_linkprofiles(type, event)

  for _,lp in pairs(self.lps) do
    local sl=lp.sl

    lp.onhold = false

    self:state_machine_perform_transit(lp)
  end

  return true
end

function LinkProfile:modem_on_rmnet_change(type,event,a)

  local ipv6_enable = nil

  -- take IPv6 indication from ip family in QMI notification
  if a.ip_family == 4 or a.ip_family == 6 then
    ipv6_enable = a.ip_family == 6
  end

  -- use QMI service ID as IPv6 indication only when IPv6 indication does not exist in QMI notification - in this case, ipv6_enable is still nil.
  if ipv6_enable == nil then
    self.l.log("LOG_DEBUG",string.format("IP family information not found, use QMI service ID instead (service_id=%d)",a.service_id))
    ipv6_enable = a.service_id > self.linkprofile_max_profile
  end

  local lp_index = ipv6_enable and (a.service_id - self.linkprofile_max_profile) or a.service_id
  local lp = self.lps[lp_index]

  if not lp then
    self.l.log("LOG_INFO",string.format("linkprofile not found (service_id=%d)",lp_index))
    return
  end

  local sl = lp.sl
  local sl_stat = sl.get_current_state()

  local online_ipv6
  local online_ipv4

  -- get online status
  if ipv6_enable then
    online_ipv4 = lp.online_ipv4
    online_ipv6 = a.status == "up"
  else
    online_ipv4 = a.status == "up"
    online_ipv6 = lp.online_ipv6
  end

  -- set noeffect flag
  local noeffect = a.error == "QMI_ERR_NO_EFFECT_V01"
  if noeffect then
    if ipv6_enable then
      lp.online_ipv6_noeffect = noeffect
    else
      lp.online_ipv4_noeffect = noeffect
    end
  end

  local raising_edge = (not lp.online_ipv6 and online_ipv6) or (not lp.online_ipv4 and online_ipv4)
  local falling_edge = (lp.online_ipv6 and not online_ipv6) or (lp.online_ipv4 and not online_ipv4)

  self.l.log("LOG_INFO",string.format("modem_on_rmnet_change (online_ipv6=%s,online_ipv4=%s,new_online_ipv6=%s,new_online_ipv4=%s,ne6=%s,ne4=%s)",
    lp.online_ipv6,
    lp.online_ipv4,
    online_ipv6,
    online_ipv4,
    lp.online_ipv6_noeffect,
    lp.online_ipv4_noeffect
  ))

  -- update online status
  lp.online_ipv6 = online_ipv6
  lp.online_ipv4 = online_ipv4


  self.l.log("LOG_INFO",string.format("modem_on_rmnet_change (index=%d,online_ipv4=%s,online_ipv6=%s,onhold=%s,rdb_enable_ipv4=%s,rdb_enable_ipv6=%s,sl_stat=%s)",
    lp.lp_index,
    lp.online_ipv4,
    lp.online_ipv6,
    lp.onhold,
    lp.enable_ipv4,
    lp.enable_ipv6,
    sl_stat
  ))

  -- RDB pdp last error messages
  local pdp_result
  local pdp_result_verbose

  -- keep last error
  if a.call_end_reason then
    pdp_result = a.call_end_reason
  end

  -- get dualstack
  local dualstack = lp.enable_ipv4 and lp.enable_ipv6
  self.l.log("LOG_DEBUG",string.format("[dual-stack] dual stack support (flag=%s)",dualstack))

  -- if unspecified internal errors
  if a.call_end_reason == "unspecified" then
    -- WDS_VCER_INTERNAL_ERR_PDN_IPV4_CALL_DISALLOWED_V01 or IP_V6_ONLY_ALLOWED
    if (a.verbose_call_end_reason_type == "internal" and a.verbose_call_end_reason == 208)
      or (a.verbose_call_end_reason_type == "3gpp spec defined" and a.verbose_call_end_reason == 51) then
      lp.disallowed_ipv4 = dualstack
      self.l.log("LOG_NOTICE",string.format("[dual-stack] IPv4 disallowed #%d",lp.lp_index))
      -- WDS_VCER_INTERNAL_ERR_PDN_IPV6_CALL_DISALLOWED_V01 or IP_V4_ONLY_ALLOWED
    elseif (a.verbose_call_end_reason_type == "internal" and a.verbose_call_end_reason == 210)
      or (a.verbose_call_end_reason_type == "3gpp spec defined" and a.verbose_call_end_reason == 50) then
      lp.disallowed_ipv6 = dualstack
      self.l.log("LOG_NOTICE",string.format("[dual-stack] IPv6 disallowed #%d",lp.lp_index))
    else
      self.l.log("LOG_ERR",string.format("[dual-stack] unknown internal error found (error=%d)",a.verbose_call_end_reason or -1))
    end

  end

  if a.verbose_call_end_reason_type or a.verbose_call_end_reason then
    pdp_result_verbose = string.format("%s #%d",a.verbose_call_end_reason_type or "",a.verbose_call_end_reason or -1)

    local suppress_end_reasons = self.backoff_suppress_verbose_call_end_reasons[a.verbose_call_end_reason_type_num]
    local no_retry_end_reasons = self.no_retry_verbose_call_end_reasons[a.verbose_call_end_reason_type_num]
    if suppress_end_reasons and suppress_end_reasons[a.verbose_call_end_reason] then
      -- This connect failure should be re-tried immediately.
      lp.rdb_backoff_stat.backoff_suppress = true
    end
    if no_retry_end_reasons and no_retry_end_reasons[a.verbose_call_end_reason] then
      -- This connect failure should stop all retries for this PDN.
      lp.onhold = true
      self.l.log("LOG_INFO", string.format("[dual-stack] no retry for connect failure type=%d reason=%d, put profile on hold - %s", a.verbose_call_end_reason_type_num, a.verbose_call_end_reason, lp.network_interface))
    end
  end

  -- set pdp result
  if pdp_result or pdp_result_verbose then
    self.wrdb:set(lp.rdb_prefix .. "pdp_result",pdp_result)
    self.wrdb:set(lp.rdb_prefix .. "pdp_result_verbose",pdp_result_verbose)
  end

  -- set lp
  local next_sls={
    -- connect
    ["lps:connect"]="",
    ["lps:connect_ipv4"]="lps:post_connect_ipv4",
    ["lps:post_connect_ipv4"]="",
    ["lps:connect_ipv6"]="lps:post_connect_ipv6",
    ["lps:post_connect_ipv6"]="",
    ["lps:post_connect"]="",

    -- disconnect
    ["lps:disconnect"]="",
    ["lps:disconnect_ipv6"]="lps:post_disconnect_ipv6",
    ["lps:post_disconnect_ipv6"]="",
    ["lps:disconnect_ipv4"]="lps:post_disconnect_ipv4",
    ["lps:post_disconnect_ipv4"]="",
    ["lps:post_disconnect"]="",
  }

  local disconnect_stats={
    ["lps:disconnect"]=true,
    ["lps:disconnect_ipv6"]=true,
    ["lps:post_disconnect_ipv6"]=true,
    ["lps:disconnect_ipv4"]=true,
    ["lps:post_disconnect_ipv4"]=true,
    ["lps:post_disconnect"]=true,
  }

  -- when connection attempt fails due to failure in biding or in starting network interface
  local start_failure = a.func_name == "bind_mux_data_port" or a.func_name == "start_network_interface"

  -- immediately, disconnect and reconnect if "noeffect" occurs
  if (sl_stat == "lps:connect_ipv6" and lp.online_ipv6_noeffect) or ( sl_stat == "lps:connect_ipv4" and lp.online_ipv4_noeffect ) then
    self.l.log("LOG_INFO",string.format("[dual-stack] immediately, reconnect due to 'noeffect' (noeffect_ipv6=%s,noeffect_ipv4=%s)",lp.online_ipv6_noeffect, lp.online_ipv4_noeffect))
    sl.switch_state_machine_to("lps:disconnect")

    -- immediately, disconnect and reconnect if it fails to bind or to start ipv4 or ipv6
  elseif start_failure and ( (sl_stat == "lps:connect_ipv4" and not lp.disallowed_ipv4) or (sl_stat == "lps:connect_ipv6" and not lp.disallowed_ipv6) ) then
    self.l.log("LOG_INFO",string.format("[dual-stack] immediately, reconnect due to %s (online_ipv4=%s,online_ipv6=%s) - %s",a.func_name,lp.online_ipv4,lp.online_ipv6,lp.network_interface))
    sl.switch_state_machine_to("lps:disconnect")
  elseif falling_edge and not disconnect_stats[sl_stat] then

    self.l.log("LOG_NOTICE",string.format("[dual-stack] dropping detected, start disconnect (stat=%s) - %s / %s",sl_stat,lp.network_interface, ipv6_enable and "ipv6" or "ipv4"))
    sl.switch_state_machine_to("lps:disconnect")
  else
    if next_sls[sl_stat] == nil then
      self.l.log("LOG_INFO",string.format("[dual-stack] ignore indiciation (stat=%s) - %s",sl_stat,lp.network_interface))
    elseif next_sls[sl_stat] ~= "" then
      -- if ipv4 events are expected, ignore ipv6 events
      if ( sl_stat == "lps:connect_ipv4" or sl_stat == "lps:disconnect_ipv4") and ipv6_enable then
        self.l.log("LOG_INFO",string.format("[dual-stack] ignore unexpected ipv6 pdp events (stat=%s) - %s",sl_stat,lp.network_interface))
        -- if ipv6 events are expected, ignore ipv4 event
      elseif ( sl_stat == "lps:connect_ipv6" or sl_stat == "lps:disconnect_ipv6" ) and not ipv6_enable then
        self.l.log("LOG_INFO",string.format("[dual-stack] ignore unexpected ipv4 pdp event (stat=%s) - %s",sl_stat,lp.network_interface))
        -- accept pdp events to switch to the next stage.
      else
        self.l.log("LOG_INFO",string.format("[dual-stack] new pdp event detected (stat=%s) - %s",sl_stat,lp.network_interface))
        sl.switch_state_machine_to(next_sls[sl_stat])
      end
    end
  end

  return true
end

function LinkProfile:stop_linkprofiles(type, event)
  for _,lp in pairs(self.lps) do
    local sl=lp.sl

    lp.onhold = true

    self:state_machine_perform_transit(lp)
  end

  return true
end



LinkProfile.cbs_system={
  -- read profiles from modem
  "read_linkprofiles",
  -- read profile per profile
  "read_rmnet_on_read",
  "write_rmnet_on_write",
  -- wake up any sleeping link.profiles
  "wakeup_linkprofiles",
  -- start link.profiles
  "start_linkprofiles",
  "modem_on_rmnet_change",
  "stop_linkprofiles",
  "reset_and_wakeup_linkprofiles",
}

-- Some connect/pdp errors should not result in backoff.
-- In particular, some errors are transitory internal modem
-- errors which should result in an immediate connect retry
-- without backoff. This dictionary lists all the verbose call
-- end reasons that should result in a suppressed backoff.
-- Refer to Qualcomm document 80-NV6515-5 (QMI Wireless Data
-- Service Spec) for the call end reasons.
LinkProfile.backoff_suppress_verbose_call_end_reasons = {
  -- call manager defined
  [3] = {
    -- NO_SRV
    [2001] = true,
  },
}

-- Some connect/pdp failures are either permanent or should not be retried
-- for other reasons. This table lists the reasons for which no retry should
-- occur. It is a two level dictionary:
--   level 1: key=call end reason code, value=second level dictionary
--   level 2: key=verbose call end reason code, value=true
-- This default value can be overridden or extended by v_var extension code.
LinkProfile.no_retry_verbose_call_end_reasons = {}

-- For safety, only allow a maximum number of backoff suppressions.
-- The backoff_suppress_count is reset together with all other backoff
-- stats (ie, some time after a successful connect).
LinkProfile.backoff_suppress_count_max = 3

function LinkProfile:init()

  self.l.log("LOG_INFO",string.format("* initiate profiles (max_profile=%d)",self.linkprofile_max_profile))

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  for lp_index = 1,self.linkprofile_max_profile do

    self.l.log("LOG_INFO",string.format("* initiate profile (index=%d)",lp_index))
    local smachine_name = string.format("lp_smachine%d",lp_index)
    local lp={}

    -- create main state machine
    local sl=self.smachine.new_smachine(
      smachine_name, self.stateMachineHandlers, lp
    )

    -- build lp
    lp.sl=sl
    lp.lp_index=lp_index -- taken as a service id
    lp.rdb_prefix=string.format("link.profile.%d.",lp_index)
    -- set backoff conf and stat prefix
    lp.rdb_prefix_backoff_conf = lp.rdb_prefix .. "backoff.conf."
    lp.rdb_prefix_backoff_stat = lp.rdb_prefix .. "backoff.stat."
    lp.rdb_trigger=lp.rdb_prefix .. "trigger"
    lp.rdb_enable=lp.rdb_prefix .. "enable"
    lp.rdb_writeflag=lp.rdb_prefix .. "writeflag"
    lp.rdb_readflag=lp.rdb_prefix .. "readflag"
    lp.network_interface=string.format("rmnet_data%d",lp.lp_index - 1)
    lp.rdb_members={}
    lp.onhold=true
    lp.aLinkProfileObj = self.ALinkProfile:new()
    lp.aLinkProfileObj:init(self, lp)

    lp.rdb_backoff_conf={}
    lp.rdb_backoff_stat={
      backoff_ps_recovered = false,

      -- Some connect failures may suppress backoff (up to a max number of
      -- times). See comment on
      -- LinkProfile.backoff_suppress_verbose_call_end_reasons
      -- for more details.
      backoff_suppress = false,
      backoff_suppress_count = 0,
    }

    self.lps[lp_index] = lp

    -- subscribe trigger
    self.l.log("LOG_INFO",string.format("subscribe trigger (trigger=%s)",lp.rdb_trigger))
    self.l.log("LOG_INFO","subscribe " .. lp.rdb_prefix)
    self.rdbWatch:addObserver(lp.rdb_trigger, "rdb_on_change", lp.aLinkProfileObj)
    self.rdbWatch:addObserver(lp.rdb_enable, "rdb_on_change_enable", lp.aLinkProfileObj)
    self.rdbWatch:addObserver(lp.rdb_writeflag, "rdb_on_change", lp.aLinkProfileObj)
    self.rdbWatch:addObserver(lp.rdb_readflag, "rdb_on_change", lp.aLinkProfileObj)

    -- read initial rdb
    self:update_lp_rdb_members(lp)

    -- start initial state machine
    sl.switch_state_machine_to("lps:idle")
  end
end

return LinkProfile
