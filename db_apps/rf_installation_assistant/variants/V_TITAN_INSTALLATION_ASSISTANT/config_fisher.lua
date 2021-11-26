--
-- Override installation web service configuration
--
-- Copyright (C) 2018 NetComm Wireless Limited.
--

return {

    rdb_band = 'wwan.0.system_network_status.current_band',

    -- TODO: Should be measured RSRP on each port, but they are not available
    -- since a customised QMI message is not ready to collect them from modem.
    -- Until it's ready, use main RSRP for both ports.
    rdb_rsrp_0 = 'wwan.0.signal.0.rsrp',
    rdb_rsrp_1 = 'wwan.0.signal.0.rsrp',

}
