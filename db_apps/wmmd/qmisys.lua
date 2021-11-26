#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- load wsapi util
local util = require("wsapi.util")

-- initialize Lua syslog
local l = require("luasyslog")
pcall(function() l.open("ril", "LOG_DAEMON") end)

-- load essential lua modules
local os = require("os")
local io = require("io")
local turbo = require("turbo")
local table = require("table")
local rdb = require("luardb")
local wrdb = require("wmmd.wmmd_rdb"):new()
local config = require("wmmd.config")

local prog = "qmisys"
local version = "1.0"

local turbo_instance = turbo.ioloop.instance()

local help_message = [[
<<prog>> version <<version>>

  <<prog>> [options] <WMMD sys command> [parameter#1] [parameter#2] ...

  options>

    -t : timeout for the result from WMMD.
         if this option is specified and the timeout is 0, <<prog>> immediately ends without checking the result.
         if this option is not specified, <<prog>> waits until the result arrives.
         if this option is specified, <<prog>> waits for a period up to the specified timeout.

    -h : print this help screen.

  WMMD sys commands>

    modem operating mode selection:

      online      : Power on Modem (AT+CFUN=1)
      lowpower    : Put Modem to low power mode (AT+CFUN=0)
      reset       : Reset Modem
      offline     : Offline
      shutdown(*) : Shutdown Modem. Reboot or power-cycle is required after this procedure.

    subsystem controls:

      shutdown_subsystem(*) : Shutdown Modem Subsystem after detaching from network.

    (*)  These 2 shutdown commands are not so much different. shutdown is to shutdown by changing modem operating mode and
        shutdown_subsystem is to shutdown by requesting Modem subsystem. shutdown_subsystem will send EMM detach but shutdown
        immediately shuts down Modem.

  usage>

    # <<prog>> -t 3 lowpower
    # <<prog>> -t 3 online
    # <<prog>> reset
]]

-----------------------------------------------------------------------------------------------------------------------
-- Write string into stderr.
--
-- @param str String to print.
local function eprint(str)
  io.stderr:write(str .. "\n")
end

-----------------------------------------------------------------------------------------------------------------------
-- Print usage into stdout.
local function print_usage()
  print(help_message)
end

-----------------------------------------------------------------------------------------------------------------------
-- Print usage into stderr.
local function print_eusage()
  eprint(help_message)
end


-- WMMD result
local rdb_result_from_wmmd = "[ERROR] timeout"

local onRdbObj = {}
-----------------------------------------------------------------------------------------------------------------------
-- Callback for RDB notification
--
-- @param rdb RDB that triggers the callback
-- @param val RDB Value.
function onRdbObj:on_rdb_result(rdb,val)
  rdb_result_from_wmmd = val
  turbo_instance:close()
end

-----------------------------------------------------------------------------------------------------------------------
-- Callback for Turbo timeout
local function on_timeout()
  turbo_instance:close()
end

-----------------------------------------------------------------------------------------------------------------------
-- Start point of the module
local function main()

  -- initialize help message
  help_message = help_message:gsub("<<prog>>",prog):gsub("<<version>>",version)

  -- parse command line arguments
  local tab,arg = util.getopt(arg,"ht")

  if #arg < 1 then
    eprint("error: no sys command specified.\n")
    print_eusage()
    os.exit(-1)
  end

  -- initialize wrdb
  local rdbWatch = require("wmmd.RdbWatch")
  wrdb:setup(rdbWatch)
  wrdb:init()

  -- clear any previous notification
  wrdb:setp(config.ril_rdb_result)
  wrdb:getp(config.ril_rdb_result)
  -- register for notification
  wrdb:watchp(config.ril_rdb_result, "on_rdb_result", onRdbObj)
  -- send command
  wrdb:setp(config.ril_rdb_command, string.format("%s,%s","sys",table.concat(arg,",")))

  local wait_timeout = tonumber(tab["t"])

  -- add timer if there is a parameter
  if wait_timeout ~= nil then
    -- immediately return if it does not require result
    if wait_timeout == 0 then
      os.exit(0)
    end

    local timeout = turbo.util.gettimemonotonic()+wait_timeout*1000
    turbo_instance:add_timeout(timeout,on_timeout)
  end

  -- wait for notification
  turbo_instance:start()

  local result,msg = rdb_result_from_wmmd:match("%[(.*)%] (.*)")

  -- print result
  print(msg)

  os.exit(result == "DONE" and 0 or -1)
end


main()
