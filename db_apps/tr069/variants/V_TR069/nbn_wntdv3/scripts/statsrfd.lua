#!/usr/bin/env lua
--[[
    statsrf.lua
    Daemon to collect RF statistic data and send them to filerolling server
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
luasyslog.open('statsrf', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
local conf = dofile('/usr/lib/tr-069/config.lua')

luasyslog.log('LOG_INFO', "statsrfd start ..")

local statsclient = assert(loadfile(us..'statsclient.lua'))
collect = statsclient()
collect.init(conf.stats.fileRollPort)

local radio_prefix = 'wwan.0.'
local radio_tr069_prefix = 'rf.'
local radio_stats_name=
{
  ['SignalRSRP'] = 'signal.0.rsrp',
  ['SignalRSRP0'] = 'servcell_info.rsrp.0',
  ['SignalRSRP1'] = 'servcell_info.rsrp.1',
  ['SignalRSRQ'] = 'signal.rsrq',
  ['SignalRSSI'] = 'signal.rssi',
  ['SignalRSSINR'] = 'signal.rssinr',
  ['SignalRXGainInner'] = 'signal.rx_gain.inner',
  ['SignalTxPower'] = 'signal.tx_power_PUSCH',
  ['SignalRxMcsIndex'] = 'servcell_info.rx_mcs_index',
  ['SignalTxMcsIndex'] = 'servcell_info.tx_mcs_index',
  ['StatsBlockErrorRateRxInitialBler'] = 'harq.dl_initial_bler',
  ['StatsBlockErrorRateTxInitialBler'] = 'harq.ul_initial_bler',
  ['StatsBlockErrorRateRxResidualBler'] = 'harq.dl_residual_bler',
  ['StatsBlockErrorRateTxResidualBler'] = 'harq.ul_residual_bler',
  ['StatsHARQDownlinkACK'] = 'harq.dl_ack',
  ['StatsHARQDownlinkNACK'] = 'harq.dl_nack',
  ['StatsHARQUplinkACK'] = 'harq.ul_ack',
  ['StatsHARQUplinkNACK'] = 'harq.ul_nack',
  ['StatsHARQTX1stOK'] = 'harq.1st_tx_ok',
  ['StatsHARQReTX1stOK'] = 'harq.retx1',
  ['StatsHARQReTX2ndOK'] = 'harq.retx2',
  ['StatsHARQReTX3rdOK'] = 'harq.retx3',
  ['StatsHARQReTX4thOK'] = 'harq.retx4',
  ['StatsHARQReTX5thOK'] = 'harq.retx5',
  ['StatsHARQReTX6thOK'] = 'harq.retx6',
  ['StatsHARQReTX7thOK'] = 'harq.retx7',
  ['StatsHARQReTX8thOK'] = 'harq.retx8',
  ['StatsHARQReTX9thOK'] = 'harq.retx9',
  ['StatsHARQReTXMaximum'] = 'harq.retx_max',
}

--[[
    This is main loop, it reads each group of RF stats RDBs every 30s('rfStatsPollInterval')
    and send them to file rolling server.
    The data item name in file-roll-stats.000xx is 'rf.yyyy'and these name are part of tr069 variable
    correspondingly.
]]

while true do
  luardb.wait(conf.stats.rfStatsPollInterval)
  collect.start()
  for id, name in pairs (radio_stats_name) do
    local val = luardb.get(radio_prefix.. name);
    print(name, val);
    collect.set(radio_tr069_prefix..id, val)
  end
  collect.commit()
end

