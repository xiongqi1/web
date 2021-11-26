-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_g.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiGUt = UnitTest:new()
QmiGUt.name = "QmiG-UnitTest"

QmiGUt.mockWatcherCbs = {
  sys = {

  },
}

function QmiGUt:initiateModules()
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiGUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:modem_on_ims_reg_stat
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_ims_reg_stat watcher callback",
      delay = 10000,
      type = "sys", event = "modem_on_ims_reg_stat",
      arg = {
        registered = true,
        reg_failure_error_code = 99,
        reg_stat = "registered",
        reg_error_string = "NO ERROR",
        reg_network = "TEST",
      },
      expectList = {
      },
    },
    -- sys:modem_on_ims_pdp_stat
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_ims_pdp_stat watcher callback",
      type = "sys", event = "modem_on_ims_pdp_stat",
      arg = {
        connected = true,
        failure_error_code = 99,
      },
      expectList = {
      },
    },
    -- sys:modem_on_ims_serv_stat
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_ims_serv_stat watcher callback",
      type = "sys", event = "modem_on_ims_serv_stat",
      arg = {
        sms_service_status = "sms_service_status",
        voip_service_status = "voip_service_status",
        vt_service_status = "voip_service_status",
        ut_service_status = "ut_service_status",
        vs_service_status = "vs_service_status",
        sms_service_rat = "sms_service_rat",
        voip_service_rat = "voip_service_rat",
        vt_service_rat = "vt_service_rat",
        ut_service_rat = "ut_service_rat",
        vs_service_rat = "vs_service_rat",
      },
      expectList = {
      },
    },
    -- sys:modem_on_reg_mgr_config
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_reg_mgr_config watcher callback",
      type = "sys", event = "modem_on_reg_mgr_config",
      arg = {
        primary_cscf = "primary_cscf",
        pcscf_port = "pcscf_port",
      },
      expectList = {
      },
    },
    -- sys:modem_on_uim_card_status
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_uim_card_status watcher callback",
      type = "sys", event = "modem_on_uim_card_status",
      arg = {
        card_state = "present",
        error_code = "",
        app_state = "ready",
        pin_state = "disabled",
        pin_retries = 3,
        puk_retries = 3,
      },
      expectList = {
      },
    },
    -- sys:modem_on_operating_mode
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_operating_mode watcher callback",
      type = "sys", event = "modem_on_operating_mode",
      arg = {
        operating_mode = "online",
      },
      expectList = {
      },
    },
    -- sys:modem_on_simcard_raw_info
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_simcard_raw_info watcher callback",
      type = "sys", event = "modem_on_simcard_raw_info",
      arg = {
        a_string = "a_string",
        a_table = {element1="element1", element2="element2"}
      },
      expectList = {
      },
    },
    -- sys:modem_on_simcard_info
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_simcard_info watcher callback",
      type = "sys", event = "modem_on_simcard_info",
      arg = {
        imsi = "imsi",
        msisdn = "msisdn",
        iccid = "iccid",
        mbn = "mbn",
        mbdn = "mbdn",
        adn = "adn",
        activation = "activation",
      },
      expectList = {
      },
    },
    -- sys:modem_on_additional_serials will reboot systems. Skip this step.
    -- sys:modem_on_modem_info
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_modem_info watcher callback",
      type = "sys", event = "modem_on_modem_info",
      arg = {
        manufacture = "manufacture",
        model = "model",
        firmware_version = "firmware_version",
        hardware_version = "hardware_version",
        imei = "imei",
        imeisv = "imeisv",
        meid = "meid",
        esn = "esn",
      },
      expectList = {
      },
    },
    -- sys:modem_on_rf_band_info
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_rf_band_info watcher callback",
      type = "sys", event = "modem_on_rf_band_info",
      arg = {
        current_band = "current_band",
        active_channel = "active_channel",
      },
      expectList = {
      },
    },
    -- sys:modem_on_netcomm_signal_info
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_netcomm_signal_info watcher callback",
      type = "sys", event = "modem_on_netcomm_signal_info",
      arg = {
        sinr = 120,
      },
      expectList = {
      },
    },
    -- sys:modem_on_signal_info
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_signal_info watcher callback",
      type = "sys", event = "modem_on_signal_info",
      arg = {
        rssi = 99,
        rsrq = 99,
        rsrp = 99,
        snr = 99,
        ecio = 99,
      },
      expectList = {
      },
    },
    -- sys:modem_on_network_time
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_network_time watcher callback",
      type = "sys", event = "modem_on_network_time",
      arg = {
        year = 2017,
        month = 5,
        day = 19,
        hour = 0,
        minute = 9,
        second = 59,
        time_zone = 8,
        daylt_sav_adj = 1,
      },
      expectList = {
      },
    },
    -- sys:modem_on_ext_network_state
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_ext_network_state watcher callback",
      type = "sys", event = "modem_on_ext_network_state",
      arg = {
        eci = "eci",
        enb_id = "enb_id",
        tac = "tac",
        lac = "lac",
        cid = "cid",
        rncid = "rncid",
        lcid = "lcid",
        network_mcc = "network_mcc",
        network_mnc = "network_mnc",
        plmn = "plmn",
        cellid = "cellid",
        srv_status = "srv_status",
      },
      expectList = {
      },
    },
    -- sys:modem_on_network_state
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_network_state watcher callback",
      type = "sys", event = "modem_on_network_state",
      arg = {
        network_desc = "network_desc",
        roaming_status = "roaming_status",
        reg_state_no = "reg_state_no",
        reg_state = true,
        serving_system = "serving_system",
        ps_attach_state = true,
      },
      expectList = {
      },
    },
    -- sys:modem_on_network_state
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_network_state watcher callback",
      type = "sys", event = "modem_on_network_state",
      arg = {
        plmnid = {
          status = "status",
          description = "description",
          mcc = "mcc",
          cns_stat = "9"
        },
      },
      expectList = {
      },
    },
    -- sys:update_cell_lock
    {
      class = "OnWatchCbTest",
      name = "sys:update_cell_lock watcher callback",
      type = "sys", event = "update_cell_lock",
      arg = { {pci=99,earfcn=99} },
      expectList = {
      },
    },
    -- state machine changes, need delay
    -- rdb lockmode
    {
      class = "OnRdbTest",
      name = "Updating RDB lockmode",
      delay = 3000,
      rdbName = self.config.rdb_g_prefix.."lockmode",
      rdbValue = "pci:PCI1,earfcn:EARFCN1,eci:ECI1;pci:PCI2,earfcn:EARFCN2,eci:ECI2;",
      expectList = {
      }
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiGUt = QmiGUt:new()
  qmiGUt:setup()
  qmiGUt:run()

end

return QmiGUt