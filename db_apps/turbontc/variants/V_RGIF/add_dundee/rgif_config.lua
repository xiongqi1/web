-----------------------------------------------------------------------------------------------------------------------
-- This is the Titan (dundee) specific configuration file for the RGIF modules to avoid the need to inject V-variables
-- directly into the running lua code, and to capture other variant configurations.
--
-- Copyright (C) 2018 NetComm Wireless limited.
-----------------------------------------------------------------------------------------------------------------------

require('variants')

local _dundee_config = {
    -- RGIF configuration details
    api_version = "1",
    has_voice = true,
    has_sas = false,
    has_pcell_scell = false,
    -- Other variant specific details
    thermal_zone_first = 0,     -- /sys/devices/virtual/thermal/thermal_zone#
    thermal_zone_last = 4,
    thermal_zone_scaling = 1,   -- scaling factor for temperature readings
    rgif_model = variants.V_RGIF,
	update_engine = variants.V_UPDATE_ENGINE,	-- innopath-DM or Netcomm-DM
}
return _dundee_config
