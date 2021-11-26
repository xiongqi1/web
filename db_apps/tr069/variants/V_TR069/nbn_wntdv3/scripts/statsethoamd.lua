#!/usr/bin/env lua
--[[
    statsethoamd.lua
    Daemon to collect active dot1ag.{1,2,3,4}.lmp.{ lbrsoutoforder,lbrnomatch} items and send them to filerolling server
    Copyright (C) 2017 NetComm Wireless Limited.
]]--

require('luardb')
require('luasyslog')

-- setup for our local includes
local us = debug.getinfo(1, "S").source:match[[^@?(.*[\/])[^\/]-$]]
if us then
  package.path = us.."?.lua;"..package.path
else
  us = ''
end
-- setup syslog facility
luasyslog.open('statsethoamd', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
local conf = dofile('/usr/lib/tr-069/config.lua')

local dot1ag_status_name='mda.status'

local dot1ag_stats_name=
{
  ['lmp.lbrnomatch'] = 'Lmp.LBRnoMatch',
  ['lmp.lbrsoutoforder'] ='Lmp.LBRsOutOfOrder',
}
local dot1ag_tr069_prefix = 'Dot1ag.'

luasyslog.log('LOG_INFO', "statsethoamd start ..")

local statsclient = assert(loadfile(us..'statsclient.lua'))
collect = statsclient()
collect.init(conf.stats.fileRollPort)

--[[
    this is main loop, it checks each group of Dot1ag RDB every 30s('dot1agPollInterval')
    if the dot1ag is active, then retrieve its lmp.lbrsoutoforder and lmp.lbrnomatch}
    send them to file rolling server.
    the data item name in file-roll-stats.000xx is 'Dot1ag.{1,2,3,4).Lmp.{ LBRsOutOfOrder,LBRnoMatch}'.
    These name are part of tr069 variable correspondingly.
]]
while true do
  luardb.wait(conf.stats.dot1agPollInterval)
  statsroll_status = luardb.get('service.statsroll.status') or ''
  if statsroll_status:find('^starting') then
    local dot1ag_tab={}
    for id = 1, conf.wntd.dot1ag_max_session do
      local prefix =conf.wntd.dot1agPrefix .. "." ..id..".";
      local status = luardb.get(prefix .. dot1ag_status_name) or ''
      if status:find('^Success:') then
        dot1ag_tab[id] = prefix
      end
    end
    if #dot1ag_tab > 0 then
        collect.start()
        for id, prefix in pairs(dot1ag_tab) do
            for name, stats_name in pairs (dot1ag_stats_name) do
                val = luardb.get(prefix .. name);
                collect.set(dot1ag_tr069_prefix..id .."."..stats_name, val)
            end
        end
        collect.commit()
    end
  end
end
