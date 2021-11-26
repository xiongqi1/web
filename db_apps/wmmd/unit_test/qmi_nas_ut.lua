-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- This is for qmi_nas.lua. It is not a comprehensive unit test. It only aims to identify issues from refactoring.

local modName = ...

local UnitTest = require("ut_base")

local QmiNasUt = UnitTest:new()
QmiNasUt.name = "QmiNas-UnitTest"

function QmiNasUt:initiateModules()
  -- as this is to identify issues from refactoring, it is still using real QMI Controller which loads qmi_nas
  self.qmiG = require("wmmd.qmi_g"):new()
  self.qmiG:setup(self.rdbWatch, self.rdb, self.dConfig)
  self.qmiG:init()
end

function QmiNasUt:setupTests()

  -- sys watcher callbacks
  local testData = {
    -- sys:poll_lte_cphy_ca_info
    {
      class = "OnWatchCbTest",
      name = "sys:poll_lte_cphy_ca_info watcher callback",
      delay = 10000,
      type = "sys", event = "poll_lte_cphy_ca_info",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_network_time
    {
      class = "OnWatchCbTest",
      name = "sys:poll_network_time watcher callback",
      delay = 3000,
      type = "sys", event = "poll_network_time",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_modem_network_status
    {
      class = "OnWatchCbTest",
      name = "sys:poll_modem_network_status watcher callback",
      delay = 3000,
      type = "sys", event = "poll_modem_network_status",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_ext_modem_network_status
    {
      class = "OnWatchCbTest",
      name = "sys:poll_ext_modem_network_status watcher callback",
      delay = 3000,
      type = "sys", event = "poll_ext_modem_network_status",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_sig_info
    {
      class = "OnWatchCbTest",
      name = "sys:poll_sig_info watcher callback",
      type = "sys", event = "poll_sig_info",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_rf_band_info
    {
      class = "OnWatchCbTest",
      name = "sys:poll_rf_band_info watcher callback",
      delay = 3000,
      type = "sys", event = "poll_rf_band_info",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_quick
    {
      class = "OnWatchCbTest",
      name = "sys:poll_quick watcher callback",
      delay = 3000,
      type = "sys", event = "poll_quick",
      arg = nil,
      expectList = {
      },
    },
    -- sys:poll_plmn_list
    {
      class = "OnWatchCbTest",
      name = "sys:poll_plmn_list watcher callback",
      delay = 3000,
      type = "sys", event = "poll_plmn_list",
      arg = {},
      expectList = {
      },
    },
    -- sys:poll
    {
      class = "OnWatchCbTest",
      name = "sys:poll watcher callback",
      delay = 3000,
      type = "sys", event = "poll",
      arg = nil,
      expectList = {
      },
    },
    -- sys:detach
    {
      class = "OnWatchCbTest",
      name = "sys:detach watcher callback",
      delay = 3000,
      type = "sys", event = "detach",
      arg = nil,
      expectList = {
      },
    },
    -- sys:attach
    {
      class = "OnWatchCbTest",
      name = "sys:attach watcher callback",
      delay = 3000,
      type = "sys", event = "attach",
      arg = nil,
      expectList = {
      },
    },
    -- sys:band_selection
    {
      class = "OnWatchCbTest",
      name = "sys:band_selection watcher callback",
      delay = 3000,
      type = "sys", event = "band_selection",
      arg = {
        bands = "79;a0;"
      },
      expectList = {
      },
    },
    -- sys:cell_lock
    {
      class = "OnWatchCbTest",
      name = "sys:cell_lock watcher callback",
      delay = 3000,
      type = "sys", event = "cell_lock",
      arg = { {pci=99,earfcn=99} },
      expectList = {
      },
    },
    -- ----------------------------------
    -- qmi watcher callbacks
    -- ----------------------------------
    -- qmi:QMI_NAS_GET_RF_BAND_INFO
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_GET_RF_BAND_INFO watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_GET_RF_BAND_INFO",
      arg = {
        resp = {
          resp = {result=0},
          rf_band_info_list_len = 1,
          rf_band_info_list = {
            [0] = {active_band="160", active_channel="160"},
          }
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_GET_SIG_INFO
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_GET_SIG_INFO watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_GET_SIG_INFO",
      arg = {
        resp = {
          resp = {result=0},
          lte_sig_info_valid = 1,
          lte_sig_info = {
            rssi = 99,
            rsrq = 99,
            rsrp = 99,
            snr = 90,
          },
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_PERFORM_NETWORK_SCAN
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_PERFORM_NETWORK_SCAN watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_PERFORM_NETWORK_SCAN",
      arg = {
        resp = {
          resp = {result=0},
          scan_result_valid = 1,
          scan_result = 0,
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_OPERATOR_NAME_DATA_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_OPERATOR_NAME_DATA_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_OPERATOR_NAME_DATA_IND",
      arg = {},
      expectList = {
      },
    },
    -- qmi:QMI_NAS_NETWORK_TIME_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_NETWORK_TIME_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_NETWORK_TIME_IND",
      arg = {
        resp = {
          time_zone_valid = 1,
          time_zone = 8,
          daylt_sav_adj_valid = 1,
          daylt_sav_adj = 1,
          universal_time = {
            year = "2017",
            month = "5",
            day = "19",
            hour = "11",
            minute = "43",
            second = "50",
            day_of_week = "5",
          },
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_SIG_INFO_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_SIG_INFO_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_SIG_INFO_IND",
      arg = {
        resp = {
          resp = {result=0},
          lte_sig_info_valid = 1,
          lte_sig_info = {
            rssi = 99,
            rsrq = 99,
            rsrp = 99,
            snr = 90,
          },
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_SERVING_SYSTEM_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_SERVING_SYSTEM_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_SERVING_SYSTEM_IND",
      arg = {
        resp = {
          current_plmn_valid = 1,
          current_plmn = {network_description="network_description"},
          serving_system = {registration_state=0x01, registration_state="NAS_REGISTERED_V01"},
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_SYS_INFO_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_SYS_INFO_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_SYS_INFO_IND",
      arg = {
        resp = {
          lte_sys_info_valid = 1,
          lte_sys_info = {
            threegpp_specific_sys_info = {},
            lte_specific_sys_info = {tac_valid=0}
          },
          gsm_sys_info_valid = 0,
          wcdma_sys_info_valid = 0,
          lte_srv_status_info_valid = 0,
          lte_srv_status_info = {
            is_pref_data_path = 0,
            srv_status = 2,
            true_srv_status = 2,
          }
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_RF_BAND_INFO_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_RF_BAND_INFO_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_RF_BAND_INFO_IND",
      arg = {
        resp = {
          rf_band_info = {active_band="160", active_channel="160"},
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_LTE_CPHY_CA_IND
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_LTE_CPHY_CA_IND watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_LTE_CPHY_CA_IND",
      arg = {
        resp = {
        },
      },
      expectList = {
      },
    },
    -- qmi:QMI_NAS_GET_CELL_LOCATION_INFO
    {
      class = "OnWatchCbTest",
      name = "qmi:QMI_NAS_GET_CELL_LOCATION_INFO watcher callback",
      delay = 1000,
      type = "qmi", event = "QMI_NAS_GET_CELL_LOCATION_INFO",
      arg = {
        resp = {
          resp = {result=0},
        },
      },
      expectList = {
      },
    },
    -- rdb observer: wwan.0.currentband.cmd.command
    {
      class = "OnRdbTest",
      name = "Observing wwan.0.currentband.cmd.command",
      delay = 1000,
      rdbName = "wwan.0.currentband.cmd.command",
      rdbValue = "get",
      expectList = {
        {
          class = "RdbWritten",
          name = "wwan.0.currentband.cmd.status",
          value = "[done]",
        },
      }
    },
  }

  self:installTestList(testData)
end

if not modName then
  local qmiNasUt = QmiNasUt:new()
  qmiNasUt:setup()
  qmiNasUt:run()

end

return QmiNasUt