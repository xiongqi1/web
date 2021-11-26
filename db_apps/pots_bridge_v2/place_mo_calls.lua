#!/usr/bin/env lua

-- Copyright (C) 2019 NetComm Wireless Limited.

--[[

* RDB usage

voice_call.call_status.1        -       end 666
voice_call.call_status.1        -       conversation 666
voice_call.call_status.1        -       dial 666
voice_call.call_status.1        -       hold 666

root:~# rdb_set wwan.0.voice.command.ctrl "0 CALL 666 30"
root:~# rdb_set wwan.0.voice.command.ctrl "0 HANGUP 1"

* requirements

  dial number
  number of calls
  call duration
  wait between calls
  audio

]]--

-- lua basic modules
local table = require("table")
local os = require("os")
-- extended modules
local util = require("wsapi.util")
local turbo = require("turbo")
local ffi = require("ffi")
-- netcomm specific modules
local lrdb = require("luardb")

local prog = "place_mo_calls.lua"
local version = "1.2"

local help_message = [[
<<prog>> version <<version>>

  <<prog>> [options] <phone number to dial> ...

  options>

    -c : number of MO calls to place (default: 1 MO call)
    -d : call duration in seconds (default: 45 seconds)
    -w : wait in seconds between calls (default: 15 seconds)
    -t : call answer timeout (default: 90 seconds)
    -h : print this help screen.

  usage>

    # <<prog>> 666
    # <<prog>> -c 10 -d 60 -w 30 666


  description>
    This script places multiple test voice MO calls. To disable PBX test mode as below when the script finishes.

    # rdb set pbx.test_mode 0
]]

-----------------------------------------------------------------------------------------------------------------------
-- print string into stderr.
--
-- @param str String to print.
local function eprint(str)
  io.stderr:write(str .. "\n")
end

-----------------------------------------------------------------------------------------------------------------------
-- splits string into an array by pat parameter.
--
local function split(str, pat)
  local t = {}  -- NOTE: use {n = 0} in Lua-5.0
  local fpat = "(.-)" .. pat
  local last_end = 1
  local s, e, cap = str:find(fpat, 1)
  while s do
    if s ~= 1 or cap ~= "" then
      table.insert(t,cap)
    end
    last_end = e+1
    s, e, cap = str:find(fpat, last_end)
  end
  if last_end <= #str then
    cap = str:sub(last_end)
    table.insert(t, cap)
  end
  return t
end

-----------------------------------------------------------------------------------------------------------------------
-- sleep
ffi.cdef "unsigned int sleep(unsigned int seconds);"
local function sleep(sec)
  ffi.C.sleep(sec)
end


-----------------------------------------------------------------------------------------------------------------------
-- print usage into stderr.
local function print_eusage()
  eprint(help_message)
end

-----------------------------------------------------------------------------------------------------------------------
-- hang up a call.
local function vcmd_hangup(cid)
  lrdb.set("wwan.0.voice.command.ctrl", string.format("0 HANGUP %d",cid))
end

-----------------------------------------------------------------------------------------------------------------------
-- dial phone number.
local function vcmd_dial(phone_number,timeout)
  lrdb.set("wwan.0.voice.command.ctrl", string.format("0 CALL %s %d",phone_number,timeout))
end

-----------------------------------------------------------------------------------------------------------------------
-- get call status from RDB.
local function vcmd_get_cstat(cid)
  local cstat_rdb = lrdb.get(string.format("voice_call.call_status.%d",cid))

  return split(cstat_rdb or "", " ")[1]
end

-----------------------------------------------------------------------------------------------------------------------
-- wait for call status
local function vcmd_wait_for_cstat(cid,cstat,timeout)

  local stime = turbo.util.gettimemonotonic() / 1000

  while true do
    local cstat_cur = vcmd_get_cstat(cid)

    -- if cstat is matched
    if cstat == cstat_cur then
      return true
    end

    -- if timeout
    local etime = turbo.util.gettimemonotonic() / 1000 - stime
    if etime>=timeout then
      break
    end

    sleep(1)
  end

  return false
end

-----------------------------------------------------------------------------------------------------------------------
-- main function
function main()

  -- initialize help message
  help_message = help_message:gsub("<<prog>>",prog):gsub("<<version>>",version)

  -- parse command line arguments
  local tab,arg = util.getopt(arg,"cdwt")

  -- print usage
  if tab["h"] then
    print(help_message)
    os.exit(0)
  end

  -- check mandatory parameters
  if #arg < 1 then
    eprint("ERR: no phone number is specified\n")
    print_eusage()
    os.exit(-1)
  end

  -- collect parameters
  local number_of_mo_calls = tonumber(tab["c"]) or 1
  local call_duration_in_sec = tonumber(tab["d"]) or 45
  local wait_in_sec_between_calls = tonumber(tab["w"]) or 15
  local phone_number = table.concat(arg," ")
  local answer_timeout = tonumber(tab["t"]) or 90
  local timeout = 30

  print("===== test environment")
  print("* enable PBX test mode")
  lrdb.set("pbx.test_mode","1")
  print("* test settings")
  print(string.format("phone number = '%s'",phone_number))
  print(string.format("number of MO calls = %d",number_of_mo_calls))
  print(string.format("call duration in seconds = %d sec",call_duration_in_sec))
  print(string.format("wait in seconds between calls = %d sec",wait_in_sec_between_calls))
  print(string.format("call answer timeout = %d sec",answer_timeout))

  local call_attempt_no

  call_attempt_no = 1
  while call_attempt_no<=number_of_mo_calls do

    print(string.format("======= MO call #%d/%d",call_attempt_no,number_of_mo_calls))

    print("* end existing voice calls")
    for cid = 1,6 do
      local cstat = vcmd_get_cstat(cid)
      if cstat and cstat ~= "" and cstat ~= "end" then
        print(string.format("ERROR: call exists, hang up - cid=%d",cid))
        vcmd_hangup(cid)
        vcmd_wait_for_cstat(cid,"end",timeout)
      end
    end

    print(string.format("* place an MO call - '%s'",phone_number))
    vcmd_dial(phone_number,timeout)

    print(string.format("* wait for conversation - timeout=%d",answer_timeout))
    if vcmd_wait_for_cstat(1,"conversation",answer_timeout) then

      print("RESULT: call is in conversation")

      -- wait for duration
      print(string.format("* sleep for call duration - %d sec",call_duration_in_sec))
      if vcmd_wait_for_cstat(1,"end",call_duration_in_sec) then
        print("ERROR: call ends before call duration ends")
      else
        print("* hang up")
        vcmd_hangup(1)

        if not vcmd_wait_for_cstat(1,"end",timeout) then
          print("ERROR: call does not get disconnected")
        end
      end

    else
      print(string.format("RESULT: call is not in conversation"))

      print("* hang up")
      vcmd_hangup(1)
    end

    print(string.format("* wait between calls - %d sec",wait_in_sec_between_calls))
    sleep(wait_in_sec_between_calls)

    call_attempt_no = call_attempt_no + 1
  end

  print([[===== done

!!! PBX test mode is still enable. Manually, disable PBX test mode as below if PBX test mode is not required any more.

# rdb set pbx.test_mode 0
]])

end


main()
