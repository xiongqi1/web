#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
-- simulator.lua is an executable script that simulates RF environment
-- relevant to Titan Installation assistant. Basically, it simulates the output of
-- network manager if there were multiple cells reported, but can be run in a test
-- environment such as CMW or Amari with a single real cell configured
--
-- This is a very basic version and it is expected that eventually this will be adapted to:
-- 1) Produce more intelligent data - e.g. pci/frequency should be unique
-- 2) Allow to simulate good and bad cells
--
-- Most parameters should be passed through the command line
--
local socket = require("socket") -- so we can have sub-second timer resolution
local luarrb = require("luardb")
local llog = require("luasyslog")
pcall(function() llog.open("simulator for installation assistant", "LOG_DAEMON") end)

-- configuration data
local cfg=
{
    rdb_cell_qty ='wwan.0.cell_measurement.qty',
    rdb_cell_status='wwan.0.cell_measurement.status',
    rdb_cell_measurement_prefix='wwan.0.cell_measurement.',
    rdb_batt_charge_percentage = 'service.nrb200.batt.charge_percentage',
    rdb_batt_status = 'service.nrb200.batt.status',

    rdb_rrc_qty ='wwan.0.rrc_info.cell.qty',
    rdb_rrc_prefix='wwan.0.rrc_info.cell.',

    rdb_orientation_accuracy = 'sensors.orientation.0.accuracy',
    rdb_orientation_azimuth = 'sensors.orientation.0.azimuth',
    rdb_orientation_elevation = 'sensors.orientation.0.pitch',

-- must NOT be greater than the size of data_freq_pci_cell
    num_cell_measurements = 3,
    num_rrc_measurements = 7,
}

-- contains freq, pci and cell id.
-- Makes sure that this doesn't clash with our Amari setup.
local data_freq_pci_cell =
{
    { 901, 2, 27447298,},
    { 902, 3, 27447299,},
    { 903, 4, 27447300,},
    { 904, 5, 27447301,},
    { 905, 6, 27447302,},
    { 906, 7, 27447303,},
    { 907, 8, 27447304,},
    { 908, 9, 27447305,},
    { 909, 10, 27447306,},
    { 910, 11, 27447307,},
}

-- create some random RF data
function create_rf_data(i)

    local random_data = {}
    local index

    -- lookup a random element of data_freq_pci_cell table
    if i == -1 or i > #data_freq_pci_cell then
        index = math.random(1,#data_freq_pci_cell)
    else
        index = i
    end
    random_data.channel = data_freq_pci_cell[index][1]
    random_data.pci = data_freq_pci_cell[index][2]
    random_data.cell_id = data_freq_pci_cell[index][3]
    random_data.rsrp = math.random(-44, -140)
    random_data.rsrq = math.random(-3, -20)

    random_data.mcc = "001" -- matches Amari setup
    random_data.mnc = "01"

    return random_data
end

-- create orientation data
function create_orientation_data()
    return {
        azimuth = math.random(0, 360),
        elevation = math.random(-180, 180),
        accuracy = math.random(0, 3)
    }
end

-- write the rdb values with given index
-- note we never overwrite index 0 assuming wmmd will take care of this
function write_rdb_values_cell_measurements(index, value)
    luardb.set(cfg.rdb_cell_measurement_prefix..index, value)
end

function write_rdb_values_rrc(index, value)
    luardb.set(cfg.rdb_rrc_prefix..index, value)
end

function write_rdb_values_orientation(azimuth, elevation, accuracy)
    local accuracy_levels = {
        'unreliable', 'low', 'medium', 'high'
    }
    luardb.set(cfg.rdb_orientation_azimuth, azimuth)
    luardb.set(cfg.rdb_orientation_elevation, elevation)
    luardb.set(cfg.rdb_orientation_accuracy, accuracy_levels[accuracy+1])
end

if (#data_freq_pci_cell < cfg.num_cell_measurements) or (#data_freq_pci_cell < cfg.num_rrc_measurements) then
    print "Config constants mismatch"
    os.exit(1)
end



-- externally accessible callback (called from turbo io loop)
while(true) do

    -- write more often than the network manager as both are writing same structures
    socket.select(nil, nil, 0.1)
    local fixed_data = {}

    -- this only works correctly if at least one "Real" cell is there (e.g. Amari is connected)
    -- create fixed data
    for i = 1, #data_freq_pci_cell do
        fixed_data[i] = create_rf_data(i)
    end

    -- write rrc stuff first
    for i = 1, cfg.num_rrc_measurements do
        str = string.format("%s,%s,%d,%d,%d", fixed_data[i].mcc, fixed_data[i].mnc,
            fixed_data[i].channel, fixed_data[i].pci, fixed_data[i].cell_id)
        write_rdb_values_rrc(i, str)
    end

    luardb.set(cfg.rdb_rrc_qty, cfg.num_rrc_measurements+1)

    -- write cell measurements stuff
    for i = 1, cfg.num_cell_measurements do
        str = string.format("E,%d,%d,%d,%d", fixed_data[i].channel, fixed_data[i].pci, fixed_data[i].rsrp, fixed_data[i].rsrq)
        write_rdb_values_cell_measurements(i, str) -- starts from 1 (0 is the real data)
    end
    luardb.set(cfg.rdb_cell_qty, cfg.num_cell_measurements)

    -- write battery stuff
    local batt_data = {}

    batt_data.status = "Battery Status, err "..math.random(1,10)
    batt_data.percentage = math.random(0,100)

    luardb.set(cfg.rdb_batt_charge_percentage, batt_data.percentage)
    luardb.set(cfg.rdb_batt_status, batt_data.status)

    orientation = create_orientation_data()
    write_rdb_values_orientation(orientation.azimuth, orientation.elevation, orientation.accuracy)
end



