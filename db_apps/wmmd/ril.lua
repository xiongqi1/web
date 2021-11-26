-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- execution module that invoke all of watcher sys commands to WMMD

local Ril = require("wmmd.Class"):new()

function Ril:setup(rdbWatch, wrdb)
  -- initialize Lua syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("ril", "LOG_DAEMON") end)

  -- load essential lua modules
  self.t = require("turbo")
  self.ffi = require("ffi")

  -- load all peripheral modules
  self.luaq = require("luaqmi")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.wrdb = wrdb
  self.rdb = require("luardb")
  self.rdbWatch = rdbWatch
  self.config = require("wmmd.config")

  -- set local alias variables
  local i = self.t.ioloop.instance()

  -- RIL state machine
  self.ril_smachine = nil

  -- rdb variables for RIL
  self.rdb_command = nil
  self.rdb_result = nil

  self.command_result = ''

  self.stateMachineHandlers = {
    {state="ril:init", func="state_machine_init", execObj=self},
    {state="ril:processing", func="state_machine_processing", execObj=self},
    {state="ril:processed", func="state_machine_processed", execObj=self},
    {state="ril:idle", func="state_machine_idle", execObj=self},
  }
end

local table = require("table")

-----------------------------------------------------------------------------------------------------------------------
-- Build watcher parameters from string array arguments
--
-- @param args Array of string parameters in the format of "variable=value,..."
-- @return table { variable=value, ...}
function Ril.build_watcher_params(args)
  local params={}

  for _,v in ipairs(args) do
    local variable,value = v:match("(.*)=(.*)")
    if variable and value then
      params[variable]=value
    end
  end

  return params
end

-- RDB commands for RIL
Ril.rdb_command_handlers={

    -- ril own command
    ["ril"]=function(self, command, args)
      self.command_result = string.format("[ERROR] not implemented.")
      self.ril_smachine.switch_state_machine_to("ril:processed")
      return
    end,

    -- generic sys watcher invoke
    ["sys"]=function(self, command, args)

      -- bypass if no operand is provided.
      if #args < 1 then
        self.command_result = string.format("[ERROR] missing operand.")
        self.ril_smachine.switch_state_machine_to("ril:processed")
        return
      end

      -- build watcher command and parameters
      local watcher_command = args[1]
      local watcher_str_params = {unpack(args,2)} or {}
      local watcher_params = self.build_watcher_params(watcher_str_params)

      self.l.log("LOG_INFO",string.format("[RIL] invoke watcher command (command='%s',wcommand='%s',params='%s')",command,watcher_command,table.concat(watcher_str_params,",")))

      -- invoke watcher command
      local res = self.watcher.invoke(command,watcher_command,watcher_params)

      if res then
        self.command_result = string.format("[DONE] success")
        self.l.log("LOG_INFO",string.format("[RIL] watcher command succeeded (command='%s',params='%s')",command,table.concat(watcher_str_params,",")))
      else
        self.command_result = string.format("[ERROR] fail")
        self.l.log("LOG_INFO",string.format("[RIL] watcher command failed (command='%s',params='%s')",command,table.concat(watcher_str_params,",")))
      end

      -- switch state machine to processed
      self.ril_smachine.switch_state_machine_to("ril:processed")
    end,
}

-----------------------------------------------------------------------------------------------------------------------
-- RIL RDB command main handler
--
-- @param user_rdb_command user RDB command that is written to RIL RDB variable.
function Ril:on_rdb_command(rdb,user_rdb_command)

  -- bypass if the command is a blank string
  if not user_rdb_command or user_rdb_command == "" then
    return
  end

  -- TODO: we may need an escape sequence for parameters with spaces.
  local args = user_rdb_command:split(",")
  local command = args[1]
  local params = {unpack(args, 2)}
  local handler = command and self.rdb_command_handlers[command] or nil

  -- bypass if the RIL command is not supported.
  if not handler then
    self.l.log("LOG_ERR",string.format("[RIL] RIL command not supported (command=%s)",command))
    return
  end

  -- bypass if state machine is not idle
  local current_state = self.ril_smachine.get_current_state()
  if current_state ~= "ril:idle" then
    self.l.log("LOG_ERR",string.format("[RIL] RIL is busy (command='%s',stat='%s',user_rdb_command='%s')",command,current_state,user_rdb_command))
    return
  end

  self.l.log("LOG_INFO",string.format("[RIL] RIL command accepted (command='%s',params='%s')",command,table.concat(params," ")))

  -- immediately switch to processing state
  self.ril_smachine.switch_state_machine_to("ril:processing",0,true)

  -- call command
  handler(self,command,params)
end

-----------------------------------------------------------------------------------------------------------------------
-- State machine callback for Init state
function Ril:state_machine_init(old_stat,new_stat,stat_chg_info)
  self.ril_smachine.switch_state_machine_to("ril:idle")
end

-----------------------------------------------------------------------------------------------------------------------
-- State machine callback for Idle state
function Ril:state_machine_idle(old_stat,new_stat,stat_chg_info)
  -- reset RIL command RDB
  self.wrdb:set(self.rdb_command)
end

-----------------------------------------------------------------------------------------------------------------------
-- State machine callback for Processing state
function Ril:state_machine_processing(old_stat,new_stat,stat_chg_info)
end

-----------------------------------------------------------------------------------------------------------------------
-- State machine callback for Processed state
function Ril:state_machine_processed(old_stat,new_stat,stat_chg_info)
  -- set command result
  self.wrdb:set(self.rdb_result,self.command_result)

  -- switch state machine to idle
  self.ril_smachine.switch_state_machine_to("ril:idle")
end

-- system watcher handlers for ril own commands (not used yet)
Ril.system_watcher_handlers={
}

-- module initializing function
function Ril:init()

  self.l.log("LOG_INFO",string.format("[RIL] module loaded"))

  -- initialize module local variables
  self.rdb_command=self.config.rdb_g_prefix .. self.config.ril_rdb_command
  self.rdb_result=self.config.rdb_g_prefix .. self.config.ril_rdb_result

  self.wrdb:set(self.rdb_command)
  self.wrdb:set(self.rdb_result)

  -- register system watcher handlers
  for _,v in pairs(self.system_watcher_handlers) do
    self.watcher.add("sys", v, self, v)
  end

  -- create main state machine
  self.ril_smachine=self.smachine.new_smachine(
    "ril_smachine", self.stateMachineHandlers
  )

  -- initialize RDB notification handler
  self.rdbWatch:addObserver(self.rdb_command, "on_rdb_command", self)

  -- setup initial state machine
  self.ril_smachine.switch_state_machine_to("ril:init")


end

return Ril