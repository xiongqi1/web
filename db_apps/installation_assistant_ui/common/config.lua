-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2017 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is default configuration which can be overridden by configuration under the variants directory
return {
 -- if set to false, use RDB subscribe/trigger mechanism to run data collection
 -- if set to true, simply poll at a given interval (no need to use RDB triggers)
 -- the interval should be the same as network manager's interval for writing cell info,
 -- currently set in wmmd to 10 seconds
    use_polling = false,

     -- just slightly less (by 1%) than wmmd to avoid complete sync of writing and reading RDBs
     -- from which there would be no escape
    POLL_INTERVAL_MS = 9900,

    -- supports a poll task to monitor the system status (for example, to detect if RF is available)
    STATUS_POLL_INTERVAL_MS = 1000,

    rdb_cell_qty ='wwan.0.cell_measurement.qty',
    rdb_cell_serving_system='wwan.0.cell_measurement.serving_system',
    rdb_cell_measurement_prefix='wwan.0.cell_measurement.',
    rdb_cell_manual_qty = 'wwan.0.manual_cell_meas.qty',
    rdb_cell_manual_seq = 'wwan.0.manual_cell_meas.seq',
    rdb_cell_manual_meas_prefix = 'wwan.0.manual_cell_meas.',

    -- Manual cell measurements are no longer valid if the sequence number is not refreshed within
    -- the following number of seconds.
    MIN_MANUAL_CELL_REFRESH_TIME_SEC = 10,

    rdb_batt_charge_percentage = 'service.nrb200.batt.charge_percentage',
    rdb_batt_status = 'service.nrb200.batt.status',
    rdb_show_non_wll_cells = 'service.nrb200.web_ui.show_non_wll_cells',
    rdb_show_unknown_cells = 'service.nrb200.web_ui.show_unknown_cells',
    rdb_show_ecgi_or_eci = 'service.nrb200.web_ui.show_ecgi_or_eci',
    rdb_bind_all = 'service.nrb200.web_ui.socket_bind_all',
    rdb_wll_mcc_mnc_prefix = 'service.nrb200.web_ui.mcc_mnc',
    rdb_rsrp_pass = 'service.nrb200.web_ui.rsrp_pass',
    rdb_default_plmn = 'service.nrb200.web_ui.default_plmn',

    -- rrc stuff for getting cell id.
    rdb_rrc_info_qty = 'wwan.0.rrc_info.cell.qty',
    rdb_rrc_info_prefix = 'wwan.0.rrc_info.cell.',

    rdb_rssinr = 'wwan.0.signal.rssinr',

    -- to find the current serving cell we need to read these RDBs
    rdb_serving_cell_info_prefix = 'wwan.0.radio_stack.e_utra_measurement_report.',

    rdb_orientation_prefix = 'sensors.orientation.0.',

    -- the user can enter up to this many cell IDs
    MAX_USER_CELLS = 3,
    -- How many cells shall we display ?
    -- if no PCIs have been entered by user, we show this many cells
    MAX_DISPLAYED_CELLS = 16,
    MAX_DISPLAYED_CELLS_STATS = 16,

    RF_HISTORY_SIZE =10,

    RF_NUMBER_OF_SAMPLES_FOR_PASS = 6,

    AGING_TIME_SECONDS = 20, -- this probably needs to be much higher

    -- various versions displayed on the page
    rdb_sw_version = 'sw.version',
    rdb_hw_version = 'wwan.0.hardware_version',
    rdb_fw_version = 'wwan.0.firmware_version',
    rdb_imsi = 'wwan.0.imsi.msin',
    rdb_imei = 'wwan.0.imei',
    rdb_serial_number = 'system.product.sn',
    rdb_class = 'system.product.class',
    rdb_model = 'system.product.model',
    rdb_skin = 'system.product.skin',
    rdb_title = 'system.product.title',
    rdb_modem_hw_ver = 'system.hwver.module',
    rdb_board_hw_ver = 'system.hwver.hostboard',
    rdb_mac = 'system.product.mac',

    rf_limits = {
        RSRP = {
            max = -44,
            min = -140,
            pass =  -98,
            unit = "dBm"
        },
        RSRQ = {
            max =-3,
            min = -20,
            pass = -20, --@TODO - do we need to worry about RSRQ? For now, anything goes..
            unit = "dB"
        },
        RSSI = {
            max = 0,
            min = -120,
            pass = "N/A",
            unit = "dBm"
        }
    },

    listen_port = 80, -- IA on Lark always uses port 80

    -- This table specifies what special capabilities the installation assistant has.
    -- They can be overriden according to the V-Variables.
    capabilities = {
        nitv2 = false,
        nrb200 = false,
        orientation = false
    }
}
