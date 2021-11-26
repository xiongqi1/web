--[[
    LPPe WiFi AP scan and injection via QMI

    The basic logic is:
    1) set proper location protocol configurations for LPPe
    2) register for INJECT_WIFI_AP_DATA_REQ event
    3) upon receipt of QMI_LOC_EVENT_INJECT_WIFI_AP_DATA_REQ_IND, start WiFi AP
    scan and inject the scan result into modem via QMI_LOC_INJECT_WIFI_AP_DATA

    Copyright (C) 2018 NetComm Wireless Limited.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")

local LppeDecorator = WmmdDecorator:new()
local QmiLocDecorator = WmmdDecorator:new()

-- callback for QMI_LOC_EVENT_INJECT_WIFI_AP_DATA_REQ_IND
function QmiLocDecorator:QMI_LOC_EVENT_INJECT_WIFI_AP_DATA_REQ_IND(type, event, qm)
  self.l.log("LOG_INFO", string.format("QMI_LOC_EVENT_INJECT_WIFI_AP_DATA_REQ_IND: e911Mode = %d", self.luaq.is_c_true(qm.resp.e911Mode_valid) and qm.resp.e911Mode or -1))
  if self.ap_scan_in_progress then
    self.l.log("LOG_WARNING", "QMI_LOC a previous AP scanning is in progress")
    return false
  end

  self.l.log("LOG_INFO", "QMI_LOC start wifi scanning ...")
  self.ap_scan_in_progress = true
  -- We pass timeout-1 to scan_ap.sh since scan_ap.sh could go a bit longer
  -- than the passed in argument.
  local cmd = string.format('rdb invoke service.wifi_ap_scan scan %d 64 timeout %d &> /dev/null &',
                            self.ap_scan_inject_timeout_secs, self.ap_scan_inject_timeout_secs - 1)
  self.l.log("LOG_DEBUG", "QMI_LOC running cmd: " .. cmd)
  os.execute(cmd)
  return true
end

-- callback for QMI_LOC_INJECT_WIFI_AP_DATA_IND
function QmiLocDecorator:QMI_LOC_INJECT_WIFI_AP_DATA_IND(type, event, qm)
  self.l.log("LOG_INFO", string.format("QMI_LOC_INJECT_WIFI_AP_DATA_IND: status=%d", tonumber(qm.resp.status)))
  self.ind_expiry = nil
  return true
end

-- callback for QMI_LOC_GET_OPERATION_MODE_IND
function QmiLocDecorator:QMI_LOC_GET_OPERATION_MODE_IND(type, event, qm)
  self.l.log("LOG_INFO", string.format("QMI_LOC_GET_OPERATION_MODE_IND: status=%d, mode=%d", tonumber(qm.resp.status), self.luaq.is_c_true(qm.resp.operationMode_valid) and tonumber(qm.resp.operationMode) or -1))
  return true
end

-- callback for QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND
function QmiLocDecorator:QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND(type, event, qm)
  self.l.log("LOG_INFO", string.format("QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND: status=%d, failed=0x%04x", tonumber(qm.resp.status), self.luaq.is_c_true(qm.resp.failedProtocolConfigParamMask_valid) and tonumber(qm.resp.failedProtocolConfigParamMask) or 0))
  return true
end

-- callback for QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS_IND
function QmiLocDecorator:QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS_IND(type, event, qm)
  self.l.log("LOG_INFO", string.format("QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS_IND: status=%d", tonumber(qm.resp.status)))
  return true
end

-- Get location engine operation mode
function QmiLocDecorator:get_operation_mode()
  local succ, qerr, resp
  succ, qerr, resp = self.luaq.req(self.m.QMI_LOC_GET_OPERATION_MODE)

  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_LOC_GET_OPERATION_MODE failed, error=0x%02x", qerr))
  else
    self.l.log("LOG_NOTICE", "QMI_LOC_GET_OPERATION_MODE succeeded")
  end
end

-- Set location protocol configuration parameters.
-- Alternatively, this can also be done from modem_config_manager.
function QmiLocDecorator:set_proto_config_params()
  local succ, qerr, resp
  local bsettings = {
    suplVersion = "eQMI_LOC_SUPL_VERSION_2_0_2_V02",
    lppConfig = 3, -- LPP UP & CP
    assistedGlonassProtocolMask = 12, -- LPP UP & CP
    emergencyProtocol = "eQMI_LOC_EMERGENCY_PROTOCOL_WCDMA_UP_V02",
    wifiScanInjectTimeout = self.ap_scan_inject_timeout_secs,
    lppeUpConfig = 3, -- DBH + WiFi
    lppeCpConfig = 3, -- DBH + WiFi
  }
  -- QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS only supports one param at a time
  for k, v in pairs(bsettings) do
    succ, qerr, resp = self.luaq.req(self.m.QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS, {[k] = v})
    if not succ then
      self.l.log("LOG_ERR", string.format("QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS (%s) failed, error=0x%02x", k, qerr))
    else
      self.l.log("LOG_NOTICE", string.format("QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS (%s) succeeded", k))
    end
  end
end

-- Get location protocol configuration parameters
function QmiLocDecorator:get_proto_config_params()
  local succ, qerr, resp
  local masks = {
    0x001, -- SUPL security
    0x004, -- SUPL version
    0x008, -- LPP config
    0x010, -- assisted GLONASS
    0x020, -- SUPL hash algo
    0x040, -- SUPL TLS version
    0x080, -- emergency protocol
    0x100, -- WiFi scan inject timeout
    0x200, -- LPPe user plan config
    0x400, -- LPPe control plan config
  }
  -- QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS only supports one mask at a time
  for _, mask in ipairs(masks) do
    succ, qerr, resp = self.luaq.req(self.m.QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS, {getProtocolConfigParamMask=mask})
    if not succ then
      self.l.log("LOG_ERR", string.format("QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS (mask 0x%03x) failed, error=0x%02x", mask, qerr))
    else
      self.l.log("LOG_NOTICE", "QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS (mask 0x%03x) succeeded", mask)
    end
  end
end

-- convert WiFi channel number to frequency in MHz
-- QMI_LOC_INJECT_WIFI_AP_DATA expects frequency for apChannel although 80-NV615-17b says 'AP WiFi channel'
-- return nil if conversion fails for any reason. e.g. channel is not a defined valid WiFi channel
local function chan2freq(chan)
  chan = tonumber(chan)
  if not chan or chan < 1 then return end
  -- 2.4 GHz
  if chan <= 13 then
    return 2412 + (chan - 1) * 5
  end
  if chan == 14 then
    return 2484
  end
  if chan < 32 then return end
  -- 5 GHz
  if chan <= 173 then
    if chan % 2 == 1 then
      if chan <= 144 then return end
    else
      if chan > 144 then return end
    end
    return 5160 + (chan - 32) * 5
  end
end

--[[
    Process wifi scan results, and inject into modem
--]]
function QmiLocDecorator:process_inject_wifi_ap_data_event(rdbKey, rdbVal)
  local rdb_scan_service = "service.wifi_ap_scan"
  local result = self.wrdb:get(self.scan_result_rdb)

  -- only process scan data that is the result of our request for a scan
  if not self.ap_scan_in_progress then
      return
  end

  if result == "success" then
      self.l.log("LOG_INFO", "QMI_LOC wifi scan succeeded")
  else
      if result == "offline" then
        self.l.log("LOG_INFO", "QMI_LOC in power saving mode")
      else
        self.l.log("LOG_ERR", "QMI_LOC bad result: " .. result)
      end
      self.l.log("LOG_INFO", "QMI_LOC use cached scan result")
  end

  -- parse rdb scan results into table
  local result_tab = {}
  require 'rdbobject'
  local class = rdbobject.getClass(rdb_scan_service .. ".result")
  local now_ms = math.floor(self.turbo.util.gettimeofday())
  local recv_time_ms = tonumber(self.wrdb:get(rdb_scan_service .. '.recv_time_ms'))
  local req_time_ms = tonumber(self.wrdb:get(rdb_scan_service .. '.req_time_ms'))
  -- temporary fix for IS#26263
  local meas_age = 0 --now_ms - (recv_time_ms or 0)
  -- meas_age is mandatory, if it is missing or negative, modem will ignore the inject (QC 03737354)
  if meas_age < 0 then meas_age = 0 end
  for idx, obj in ipairs(class:getAll()) do
    result_tab[idx] = {bssid=obj.bssid, rssi=tonumber(obj.rssi),
                   chan=chan2freq(obj.chan),
                   meas_age=meas_age}
  end

  self.ap_scan_in_progress = false
  return self:inject_wifi_ap_data(req_time_ms, recv_time_ms, result_tab)
end

-- convert a hex string representation of MAC address to binary string
-- e.g. '41:42:43:44:45:46' -> 'ABCDEF'
local function mac_str2bin(mac_str)
  local mac_bits = mac_str:split(':')
  if #mac_bits ~= 6 then return end
  local mac_bin = ''
  for _, b in ipairs(mac_bits) do
    local x = tonumber(b, 16)
    if not x then return end
    mac_bin =  mac_bin .. string.char(x)
  end
  return mac_bin
end

--[[
    Inject WiFi AP scan result into modem

    10 seconds is the maximum per QMI LOC. Good news is the injection is very
    fast (a few millisecs) and even if scan takes longer than expected, the
    modem will happily accept it.

    @param req_time_ms Timestamp in milliseconds when scan is requested
    @param recv_time_ms Timestamp in milliseconds when scan result is received
    @param data An array of dictionaries, each has at least 4 fields: bssid,rssi,channel,meas_age
--]]
function QmiLocDecorator:inject_wifi_ap_data(req_time_ms, recv_time_ms, data)
  if not data then
    self.l.log("LOG_ERR", "QMI_LOC invalid inject data")
    return false
  end
  local succ, qerr, resp
  local qm = self.luaq.new_msg(self.luaq.m.QMI_LOC_INJECT_WIFI_AP_DATA)
  -- map from entry name to mask and qmi wifiApInfo field name
  local ap_info_map = {
    rssi = {0x10, "apRssi"},
    chan = {0x20, "apChannel"},
  }
  -- map from entry name to mask and qmi wifiApInfoA field name
  local apa_info_map = {
    rssi_time = {0x400, "rssiTimestamp"},
    meas_age = {0x800, "measAge"},
    serv_ap = {0x1000, "servingAccessPoint"},
    freq = {0x2000, "channelFrequency"},
    ssid = {0x4000, "ssid"},
  }
  --[[
      Modem implementation support a maximum of 50 APs. However, tests found
      a smaller number is allowed. If exceeded, no AP info will be included in the
      QMI request according to QXDM log. This is due to MQCSI_MAX_PAYLOAD_LEN=2048 was used.
      See QC03743727 & 03764691 for details.
      r10187.1.177526.1.179979.2 fixed this issue. So make sure to use modem version with this fix.
  --]]
  local MAX_AP_INFO = 50
  if #data > MAX_AP_INFO then
    -- if there are too many APs, we report the ones with strongest RSSIs
    table.sort(data, function(x, y) return x.rssi > y.rssi end)
  end
  local idx = 0
  local apa_valid
  for _, entry in ipairs(data) do
    self.l.log("LOG_INFO", "QMI_LOC " .. self.turbo.escape.json_encode(entry))
    local mac_bin = mac_str2bin(entry.bssid)
    if not mac_bin then
      self.l.log("LOG_ERR", string.format("QMI_LOC Invalid MAC %s - skipped", entry.bssid))
    else
      -- TLV 0x01
      self.ffi.copy(qm.req.wifiApInfo[idx].macAddress, mac_bin, 6)
      local mask = 0
      for name, val in pairs(ap_info_map) do
        if entry[name] ~= nil then
          mask = mask + val[1]
          qm.req.wifiApInfo[idx][val[2]] = entry[name]
        end
      end

      -- TLV 0x14
      for name, val in pairs(apa_info_map) do
        if entry[name] ~= nil then
          mask = mask + val[1]
          apa_valid = true
          if name == 'ssid' then
            self.ffi.copy(qm.req.wifiApInfoA[idx].ssid, entry.ssid)
          else
            qm.req.wifiApInfoA[idx][val[2]] = entry[name]
          end
        end
      end
      qm.req.wifiApInfo[idx].wifiApDataMask = mask

      idx = idx + 1
      if idx >= MAX_AP_INFO then break end
    end
  end
  qm.req.wifiApInfo_len = idx
  qm.req.wifiApInfoA_valid = apa_valid
  qm.req.wifiApInfoA_len = idx

  -- mark the injection as requested by the modem otherwise it will not be passed to protocol module (See QC#03737354)
  qm.req.onDemandScan_valid = true
  qm.req.onDemandScan = true

  -- request and receive timestamps are optional
  if req_time_ms then
    qm.req.requestTimestamp_valid = true
    qm.req.requestTimestamp = req_time_ms
  end
  if recv_time_ms then
    qm.req.receiveTimestamp_valid = true
    qm.req.receiveTimestamp = recv_time_ms
  end

  self.l.log("LOG_INFO", string.format("QMI_LOC sending %d APs' info", idx))

  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", string.format("%s send_msg failed", qm.me.name))
    succ = false
  else
    succ, qerr, resp = self.luaq.ret_qm_resp(qm)
    if not succ then
      self.l.log("LOG_ERR", string.format("%s failed, error=0x%02x", qm.me.name, qerr))
    else
      self.l.log("LOG_NOTICE", string.format("%s succeeded", qm.me.name))
    end
  end

  return succ
end

-- override QmiLoc:init
function QmiLocDecorator:init()
  QmiLocDecorator:__invokeChain("init")(self)

  local event_reg_base_mask = 0

  --[[

  !! note !!

  TODO:

  As Modem does not currently have qmiLocGetRegisteredEventsRespMsgT_v02 struct for QCCI,
  when Modem supports this structure, the following code has to be used to deal with existing events instead.

  -- read existing event mask.
  local succ,err,res = self.luaq.req(self.m.QMI_LOC_GET_REGISTERED_EVENTS)
  if not succ then
    self.l.log("LOG_ERR", string.format("failed to get registered event from QMI_LOC, continue with a new mask"))
  end

  event_reg_base_mask = succ and tonumber(res.eventRegMask) or 0

  eventRegMask = self.bit.bor(
    event_reg_base_mask,
    0x200000, -- inject wifi ap data req
    0x000
  )

  To workaround this missing structure, we re-register all events that the base registers.

  ]]--


  local succ,err,res = self.luaq.req(self.m.QMI_LOC_REG_EVENTS,{
    eventRegMask = self.bit.bor(
      event_reg_base_mask,
      0x001, -- position report
      0x002, -- satellite report event
      0x004, -- NMEA report
      0x200000, -- inject wifi ap data req
      0x008, -- QMI_LOC_EVENT_MASK_NI_NOTIFY_VERIFY_REQ
      0x000
    )
  })

  self.ap_scan_in_progress = false
  self.ap_scan_inject_timeout_secs = 10 -- 10 sec is the maximum per QMI LOC
  self.scan_result_rdb = 'service.wifi_ap_scan.cmd.result'
  self.rdbWatch:addObserver(self.scan_result_rdb, "process_inject_wifi_ap_data_event", self)
end

-- override QMI callbacks
QmiLocDecorator.cbs = {
  "QMI_LOC_EVENT_POSITION_REPORT_IND",
  "QMI_LOC_EVENT_GNSS_SV_INFO_IND",
  "QMI_LOC_EVENT_NMEA_IND",
  "QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND",
  "QMI_LOC_EVENT_INJECT_WIFI_AP_DATA_REQ_IND",
  "QMI_LOC_INJECT_WIFI_AP_DATA_IND",
  "QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND",
  "QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS_IND",
  "QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND",
}

-- decorate QmiLoc
function QmiLocDecorator.doDecorate()
  QmiLocDecorator:__saveChain("init")
  QmiLocDecorator:__changeImplTbl({
    "cbs",
    "init",
    "get_operation_mode",
    "set_proto_config_params",
    "get_proto_config_params",
    "process_inject_wifi_ap_data_event",
    "inject_wifi_ap_data",
    "QMI_LOC_EVENT_INJECT_WIFI_AP_DATA_REQ_IND",
    "QMI_LOC_INJECT_WIFI_AP_DATA_IND",
    "QMI_LOC_GET_OPERATION_MODE_IND",
    "QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND",
    "QMI_LOC_GET_PROTOCOL_CONFIG_PARAMETERS_IND"
  })
end

-- replace the original qmi_loc (within qmiG) with the decorated one
function LppeDecorator.doDecorate()
  LppeDecorator.__inputObj__.qmiG.qs.qmi_loc =
    QmiLocDecorator:decorate(LppeDecorator.__inputObj__.qmiG.qs.qmi_loc)
end

return LppeDecorator
