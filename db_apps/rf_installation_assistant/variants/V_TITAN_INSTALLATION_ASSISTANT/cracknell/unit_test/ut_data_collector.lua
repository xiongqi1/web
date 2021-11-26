#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2017 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
-- Unit test for data collector routines.
-- Currently, only tests earfcn to frequency mapping function. This is more or less improvised in the absence of
-- unit test infrastructure
-- Build system doesn't add it to the target, so simply adb push it into /usr/share/installation_assistant and run.
-- MUST be run from /, e.g. cd / and then /usr/share/installation_assistant/ut_data_collector.lua
-- Probably best to not have ia_web_server running, e.g. set service.nrb200.attached to 0.

--[[
as per wwan.0.cfg_earfcn.b40 on V2
38745,38770,38850,38895,38941,38945,38950,38960,38968,38970,39045,39048,39050,39090,39112,39120,39139,39145,39148,39150,39166,39170,39195,39225,39246,39250,39270,39320,39345,39364,39370,39420
as per wwan.0.cfg_earfcn.b42 on V2
41690,41742,41940,42010,42070,42071,42138,42140,42190,42250,42269,42336,42340,42370,42430,42467,42490,42534,42550,42610,42665,42730,42732,42790,42863,42865,42910,43090,43190,43210,43270,43388
]]--

local b40 = {
        -- earfcn, freq
        -- A couple of arbitrary earfcn from Ericsson list. Feel free to add more
        -- Note that formula is linear so no need to test every frequency.
        {38745, 2309.5},
        {39120, 2347.0},
        {39420, 2377.0},
        -- boundary conditions for the band
        {38650, 2300.0},
        {39649, 2399.9},
}

local b42 = {
        -- earfcn, freq
        -- A couple of arbitrary earfcn from Ericsson list
        {41690, 3410.0},
        {42467, 3487.7},
        {43388, 3579.8},
        -- boundary conditions for the band
        {41590, 3400.0},
        {43589, 3599.9},
}

-- directory where IA libraries are located
local g_ia_dir = "/usr/share/installation_assistant/"
require(g_ia_dir .. "support")
data_collector = require(g_ia_dir .. "data_collector")

-- Loads configuration before starting any applications
config = load_overriding_module(g_ia_dir, 'config')

-- Initialises data collector
data_collector.init(config)

-- Environment here is identical to the one in real ia_web_server.lua when it enters its main loop.

local function check_band(band_data)

    for i,elem in ipairs(band_data) do
        earfcn = elem[1]
        freq1 = elem[2]
        band, freq2 = data_collector.earfcn_map_wrapper(earfcn)
        print ("Band ", band, "Earfcn ", earfcn, "Frequency expected ", freq1, "Frequency returned ", freq2)
        if math.abs(freq1 - freq2) > 0.1 then
            print ("Test failed")
            os.exit (1)
        end
    end
end

print("Checking band 40")
check_band(b40)

print("Checking band 42")
check_band(b42)
