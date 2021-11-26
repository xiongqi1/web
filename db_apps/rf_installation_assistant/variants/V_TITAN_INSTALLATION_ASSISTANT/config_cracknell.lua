--
-- Override NRB-0200 connection related configuration
--
-- Copyright (C) 2017 NetComm Wireless Limited.
--

return {

    use_polling = true,


    -- @TODO - review these timings for Cracknell as requirement is for 0.5 second update
    -- Also need to adjust web socket data sending
    POLL_INTERVAL_MS = 1000,

    -- Expected to update RF measurements within the following interval
    MAX_MEASUREMENT_INTERVAL_MS = 15000,

    rdb_turbo_use_https = 'service.nrb200.web_ui.use_https',
    rdb_crt_file = 'service.nrb200.web_ui.servercrtfile',
    rdb_key_file = 'service.nrb200.web_ui.serverkeyfile',

    -- in addition to cell measurements available via neighbour cell info, Cracknell
    -- implements a special mechanism to scan any cells via PLMN scan
    -- refer to http://pdgwiki/mediawiki/index.php/Pre_Qual_on_NBN_V3-_RSRP_only
    rdb_cell_manual_qty ='wwan.0.manual_cell_meas.qty',
    rdb_cell_manual_meas_prefix='wwan.0.manual_cell_meas.',

    -- various versions displayed on the page
    rdb_sw_version = 'sw.version',
    rdb_hw_version = 'wwan.0.hardware_version',
    rdb_fw_version = 'wwan.0.firmware_version',
    rdb_imsi = 'wwan.0.imsi.msin',
    rdb_imei = 'wwan.0.imei',
    rdb_serial_number = 'systeminfo.serialnumber',

    rf_limits = {
        RSRP = {
            max = -60,
            min = -120,
            pass =  -96,
            warning = -99,
            unit = "dBm"
        },
        RSRQ = {
            max =-3,
            min = -20,
            pass = -20, --@TODO - do we need to worry about RSRQ? For now, anything goes..
            unit = "dB"
        },
        CINR = {
            max = 40,
            min = 0,
            pass = "N/A",
            unit = "dB"
        },
        RSRP_Delta = {
            max = 30,
            min = -10,
            pass = 7,
            unit = "dB"
        }
    },

    -- plmn based filtering (3 digit zero prepended mnc to cater for all cases)
    filter_plmn = 'wwan.0.mode1_plmn',
    rdb_plmn_filtering = 'wwan.0.mode1_plmn_filter',

    -- rdb for rfq table
    rdb_rfq_tab = 'wwan.0.rfq_tab',

    -- mode2 parameters
    -- used to check if we are to launch in mode 1 or mode 2
    rdb_wmmd_mode = 'service.wmmd.mode',

    -- Connected Cell ARFCN, PCI, SINR 0/1 and RSRP 0/1, Updated in real time.
    rdb_attached = 'wwan.0.system_network_status.attached',
    rdb_serving_cell_pci = 'wwan.0.radio_stack.e_utra_measurement_report.servphyscellid',
    rdb_serving_cell_pci = 'wwan.0.system_network_status.eci_pci_earfcn',
    rdb_serving_cell_earfcn = 'wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.ul_freq',
    rdb_serving_cell_rsrp = 'wwan.0.signal.0.rsrp',
    rdb_serving_cell_rsrp0 = 'wwan.0.servcell_info.rsrp.0',
    rdb_serving_cell_rsrp1 = 'wwan.0.servcell_info.rsrp.1',
    rdb_serving_cell_sinr = 'wwan.0.signal.rssinr',
    rdb_serving_cell_band = 'wwan.0.radio_stack.e_utra_measurement_report.freqbandind',

    -- Neighbour Cell ARFCN, PCI, CINR and RSRP, For V3, Radio readings from
    -- last neighbour scan sorted in descending RSRP.
    rdb_ncdp_data = 'wwan.0.celldata',

    -- Transmit power (TXPWR), Updated in real time
    rdb_serving_cell_tx_power = 'wwan.0.signal.tx_power_PUSCH',

    -- For State calculation (Unlock/Lock/3GPP)
    -- "Lock" state is not applicable as modem transitions to 3GPP immediately after lock
    rdb_state = "wwan.0.state" ,

    -- Provisioned PCIs and Band
    -- Note: we use the serving cell band above, no concept of provisioned band in V3 as of 23 OCT 2018.
    rdb_pci = 'wwan.0.pci',

    -- UNID Status, Ports 1 to 4. D(isabled), E(rror) Red, N(oLink) Yellow,
    -- U(p) green. Updated in real time
    rdb_unid1_status = 'unid.1.status',
    rdb_unid2_status = 'unid.2.status',
    rdb_unid3_status = 'unid.3.status',
    rdb_unid4_status = 'unid.4.status',

    -- AVC Status, Up to 8 AVCs. D(isabled), E(rror) Red, U(p) Green. Updated in real time
    rdb_avc_index = "avc._index",

    ---[[
    -- IDU LED - Status:
    -- Red indicator of IDU-ODU communications error (includes timer for detected errors as described in F165)
    -- Blinking Green Normal operation
    -- Blinking Amber Device booting / initialization (including ACS Synching, Software Downloading)
    -- Solid Green Test mode
    -- Solid Red System fault (including wrong cell)
    -- Blinking Red ODU-IDU Communications Error
    rdb_idu_led_status = 'indoor.led_status',
    -- IDU LED - Status Led Overrides:
    -- Red indicator of IDU-ODU communications error (includes timer for detected errors as described in F165)
    rdb_idu_comms_failed = 'indoor.comms_failed',
    rdb_idu_cable_fault = 'indoor.recent_cable_fault',

    -- IDU LED - ODU
    rdb_idu_led_odu = 'indoor.led_wwan_status',

    -- IDU LED - Signal
    rdb_idu_led_signal_low = 'indoor.led_signal_low',
    rdb_idu_led_signal_med = 'indoor.led_signal_med',
    rdb_idu_led_signal_high = 'indoor.led_signal_high',

    -- Active alarms (n Alarm Message), Updated in Real time. Concatenated for legibility if required
    rdb_alarms = 'alarms._index',
    rdb_max_alarm_entries = 4,

    -- number of log entries to display
    rdb_max_log_entries = 10
}
