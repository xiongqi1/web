#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2017 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
-- This script simulates RF environment relevant to Titan Installation assistant.
-- Can be run without ANY RF (but of course speed test and some other features will not be available).
--
-- This is in addition to the standard simulator - this one specifically simulates cells
-- setup with delegated home PLMN (dual PLMN).
--
local socket = require("socket") -- so we can have sub-second timer resolution
local luarrb = require("luardb")
local llog = require("luasyslog")
pcall(function() llog.open("simulator for installation assistant (titan)", "LOG_DAEMON") end)

-- externally accessible callback (called from turbo io loop)
while(true) do

    -- write more often than the network manager as both are writing same structures
    socket.select(nil, nil, 0.1)

    cell_rsrp = {}
    cell_rsrp[1] = math.random(-70, -90)
    cell_rsrp[2] = math.random(-70, -130)
    cell_rsrp[3] = math.random(-70, -130)
    cell_rsrp[4] = math.random(-70, -130)

    cell_rsrq = {}
    cell_rsrq[1] = math.random(-3, -20)
    cell_rsrq[2] = math.random(-3, -20)
    cell_rsrq[3] = math.random(-3, -20)
    cell_rsrq[4] = math.random(-3, -20)

    str = string.format("E,9820,491,%d,%d",cell_rsrp[1],cell_rsrq[1])
    luardb.set("wwan.0.cell_measurement.0", str)
    str = string.format("E,9820,495,%d,%d",cell_rsrp[2],cell_rsrq[2])
    luardb.set("wwan.0.cell_measurement.1", str)
    str = string.format("E,9820,493,%d,%d",cell_rsrp[3],cell_rsrq[3])
    luardb.set("wwan.0.cell_measurement.2", str)
    str = string.format("E,1175,22,%d,%d",cell_rsrp[4],cell_rsrq[4])
    luardb.set("wwan.0.cell_measurement.3", str)
    luardb.set("wwan.0.cell_measurement.qty", "4")
    luardb.set("wwan.0.rrc_info.cell.0", "310,410,9820,495,89303189") -- in data entry, enter extra 0 - e.g. 089303189
    luardb.set("wwan.0.rrc_info.cell.1", "312,680,9820,495,89303189")
    luardb.set("wwan.0.rrc_info.cell.2", "312,420,1175,22,111222333") -- non WLL cell
    luardb.set("wwan.0.rrc_info.cell.3", "310,410,9820,491,89293975")
    luardb.set("wwan.0.rrc_info.cell.4", "312,680,9820,491,89293975")
    luardb.set("wwan.0.rrc_info.cell.5", "310,410,9820,493,12345678") -- non dual cell
    luardb.set("wwan.0.rrc_info.cell.qty", 6)
    luardb.set("wwan.0.sim.status.status", "SIM OK")

    luardb.set("wwan.0.system_network_status.ECGI", "310410089303189")
    luardb.set("wwan.0.system_network_status.attached", "1")
    luardb.set("link.profile.1.status", "up")

    luardb.set("service.nrb200.batt.charge_percentage", "90")
    luardb.set("service.nrb200.batt.status", "")
end
