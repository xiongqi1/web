--
-- Override configuration for Magpie
--
-- Copyright (C) 2019 NetComm Wireless Limited.
--

return {
    rdb_cell_qty ='wwan.0.manual_cell_meas.qty',

    -- Qualcomm case 03878085 confirms that SNR field of TLV 0x14 (LTE Signal Strength Info)
    -- in QMI_NAS_GET_SIG_INFO_RESP_MSG is actually the same as RS-SINR of serving cell
    rdb_rssinr = 'wwan.0.signal.snr',

    -- RRC stat
    rdb_rrc_stat = 'wwan.0.radio_stack.rrc_stat.rrc_stat',

    rdb_show_non_wll_cells = 'service.ia_web_ui.show_non_wll_cells',
    rdb_bind_all = 'service.ia_web_ui.socket_bind_all',
    rdb_wll_mcc_mnc_prefix = 'service.ia_web_ui.mcc_mnc',
    rdb_rsrp_pass = 'service.ia_web_ui.rsrp_pass',
    rdb_default_plmn = 'service.ia_web_ui.default_plmn',
    -- the user can enter up to this many cell IDs
    MAX_USER_CELLS = 9,
    -- minimum cell IDs used for filtering
    MAX_USER_CELLS_FOR_FILTERING_SCAN = 3,
}
