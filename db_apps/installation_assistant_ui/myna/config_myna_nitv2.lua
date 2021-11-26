--
-- Override configuration for UI server running on NITv2
--
-- Copyright (C) 2019 NetComm Wireless Limited.
--

return {
    capabilities = {
        nitv2 = true
    },

    listen_port = 80,

    rdb_batt_charge_percentage = 'system.battery.capacity',
    rdb_batt_status = 'system.battery.charging_state',

    -- OWA information
    rdb_sw_version = 'owa.sw.version',

    get_rdb_prefix = 'owa.',
}

