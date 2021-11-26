-----------------------------------------------------------------------------------------------------------------------
-- This is the Magpie specific configuration file for the RGIF modules to avoid the need to inject V-variables
-- directly into the running lua code, and to capture other variant configurations.
--
-- Copyright (C) 2018 NetComm Wireless limited.
-----------------------------------------------------------------------------------------------------------------------

require('variants')

local _magpie_config = {
    -- RGIF configuration details
    api_version = "2",
    has_voice = false,
    has_sas = true,
    has_pcell_scell = true,
    -- Other variant specific details
    thermal_zone_first = 5,     -- /sys/devices/virtual/thermal/thermal_zone#
    thermal_zone_last = 9,
    thermal_zone_scaling = 10,  -- scaling factor for temperature readings
    rgif_model = variants.V_RGIF,
    pdpcontext_indices = {4, 3, 0}, -- CBRSSAS, EMS, data
    apn_indices = {1, 4, 5}, -- data, EMS, CBRSSAS
    apn_numbers = 3,
	update_engine = variants.V_UPDATE_ENGINE,	-- innopath-DM or Netcomm-DM
}
return _magpie_config
