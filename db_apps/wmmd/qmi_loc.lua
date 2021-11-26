-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi loc module, GPS

local QmiLoc = require("wmmd.Class"):new()

function QmiLoc:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_loc", "LOG_DAEMON") end)

  self.rdbWatch = rdbWatch
  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")
  self.turbo = require("turbo")
  self.ioloop = self.turbo.ioloop.instance()


  self.m = self.luaq.m
  self.e = self.luaq.e

  self.session_id = 0

  self.session_status_names={
    [0] = "success",
    [1] = "in progress",
    [2] = "general failure",
    [3] = "timeout",
    [4] = "user end",
    [5] = "bad parameter",
    [6] = "phone offline",
    [7] = "engine locked",
  }
  self.historical_gps_source = "historical"
end


function QmiLoc:convert_gpstime_to_ostime(gpstime)
  local timestamp_utc_sec = self.ffi.new("time_t[1]",gpstime / 1000)
  local tm_gps = self.ffi.new("struct tm[1]")

  self.ffi.C.gmtime_r(timestamp_utc_sec, tm_gps);

  local tm = tm_gps[0]

  return string.format("%02d%02d%02d", tm.tm_mday, tm.tm_mon + 1, (tm.tm_year + 1900) % 100),
    string.format("%02d%02d%02d", tm.tm_hour, tm.tm_min, tm.tm_sec)
end

function QmiLoc:convert_decimal_degree_to_degree_minute(decimal,latitude)

  local dir_result

  -- get direction
  if latitude then
    dir_result = (decimal >= 0) and "N" or "S"
  else
    dir_result = (decimal >= 0) and "E" or "W"
  end

  -- remove sign
  if decimal < 0 then
    decimal = -decimal
  end

  -- degree
  local degree;

  degree = math.floor(decimal)

  -- minute
  decimal = (decimal - degree) * 60

  local degree_result = string.format(latitude and "%02d%09.6f" or "%03d%09.6f",degree, decimal)

  return degree_result,dir_result
end

function QmiLoc:update_common_gps_info(info)
  local gps_prefix = "sensors.gps.0.common."

  self.wrdb:set_if_chg(gps_prefix .. "latitude", info.latitude)
  self.wrdb:set_if_chg(gps_prefix .. "latitude_direction", info.latitude_direction)
  self.wrdb:set_if_chg(gps_prefix .. "latitude_degrees", info.latitude_degrees)
  self.wrdb:set_if_chg(gps_prefix .. "longitude", info.longitude)
  self.wrdb:set_if_chg(gps_prefix .. "longitude_direction", info.longitude_direction)
  self.wrdb:set_if_chg(gps_prefix .. "longitude_degrees", info.longitude_degrees)
  self.wrdb:set_if_chg(gps_prefix .. "altitude", info.altitude)
  self.wrdb:set_if_chg(gps_prefix .. "height_of_geoid", info.height_of_geoid)
  self.wrdb:set_if_chg(gps_prefix .. "pdop", info.pdop)
  self.wrdb:set_if_chg(gps_prefix .. "date", info.date)
  self.wrdb:set(gps_prefix .. "time", info.time)
  self.wrdb:set_if_chg(gps_prefix .. "status", info.session_status)
  self.wrdb:set_if_chg(gps_prefix .. "horizontal_uncertainty", info.horizontal_uncertainty)
  self.wrdb:set_if_chg(gps_prefix .. "vertical_uncertainty", info.vertical_uncertainty)
end

function QmiLoc:update_common_gps_source(source)
  self.wrdb:set_if_chg("sensors.gps.0.source", source)
end

function QmiLoc:QMI_LOC_EVENT_POSITION_REPORT_IND(type, event, qm)

  local resp = qm.resp

  -- bypass if session id does not belong to us
  if resp.sessionId ~= self.session_id then
    return true
  end

  -- set valid flags
  local standalone_valid = self.luaq.is_c_true(resp.technologyMask_valid) and (self.bit.band(resp.technologyMask,0x0001) ~= 0)
  local assist_valid = self.luaq.is_c_true(resp.technologyMask_valid) and (self.bit.band(resp.technologyMask,0x0002) ~= 0)

  -- get gps prefix
  local gps_prefix = standalone_valid and "sensors.gps.0.standalone." or "sensors.gps.0.assisted."
  local lat_long_valid = self.luaq.is_c_true(resp.latitude_valid) and self.luaq.is_c_true(resp.longitude_valid)

  local standalone_lock
  local assist_lock

  -- clear status if not valid
  if not standalone_valid then
    self.wrdb:set("sensors.gps.0.standalone.status","")
  end
  if not assist_valid then
    self.wrdb:set("sensors.gps.0.assisted.status","")
  end

  local valid_gps_source

  -- write gps rdbs
  if standalone_valid or assist_valid then
     standalone_lock = standalone_valid and (resp.sessionStatus == "eQMI_LOC_SESS_STATUS_SUCCESS_V02")
     assist_lock = assist_valid and (resp.sessionStatus == "eQMI_LOC_SESS_STATUS_SUCCESS_V02")
    -- session status
    local session_status = self.session_status_names[tonumber(resp.sessionStatus)]
    self.wrdb:set(gps_prefix .. "status", session_status)

    local lat,lat_dir = self:convert_decimal_degree_to_degree_minute(tonumber(resp.latitude),true)
    self.wrdb:set(gps_prefix .. "latitude",self.luaq.is_c_true(resp.latitude_valid) and lat or "")
    self.wrdb:set(gps_prefix .. "latitude_direction",self.luaq.is_c_true(resp.latitude_valid) and lat_dir or "")
    local lat_degrees = tonumber(resp.latitude)

    local lon,lon_dir = self:convert_decimal_degree_to_degree_minute(tonumber(resp.longitude),false)
    self.wrdb:set(gps_prefix .. "longitude",self.luaq.is_c_true(resp.longitude_valid) and lon or "")
    self.wrdb:set(gps_prefix .. "longitude_direction",self.luaq.is_c_true(resp.longitude_valid) and lon_dir or "")
    local lon_degrees = tonumber(resp.longitude)

    self.wrdb:set(gps_prefix .. "altitude",self.luaq.is_c_true(resp.altitudeWrtEllipsoid_valid) and resp.altitudeWrtEllipsoid or "")
    self.wrdb:set(gps_prefix .. "height_of_geoid",self.luaq.is_c_true(resp.altitudeWrtMeanSeaLevel_valid) and resp.altitudeWrtMeanSeaLevel or "")

    self.wrdb:set(gps_prefix .. "horizontal_uncertainty",self.luaq.is_c_true(resp.horUncCircular_valid) and resp.horUncCircular or "")
    self.wrdb:set(gps_prefix .. "vertical_uncertainty",self.luaq.is_c_true(resp.vertUnc_valid) and resp.vertUnc or "")

    local gps_date,gps_time = self:convert_gpstime_to_ostime(resp.timestampUtc)
    self.wrdb:set(gps_prefix .. "date",self.luaq.is_c_true(resp.timestampUtc_valid) and gps_date or "")
    self.wrdb:set(gps_prefix .. "time",self.luaq.is_c_true(resp.timestampUtc_valid) and gps_time or "")

    local _3d_fix = (resp.altitudeAssumed == 0x01) and "3" or "2"
    self.wrdb:set(gps_prefix .. "3d_fix",self.luaq.is_c_true(resp.altitudeAssumed_valid) and _3d_fix or "")

    self.wrdb:set(gps_prefix .. "hdop",self.luaq.is_c_true(resp.DOP_valid) and resp.DOP.HDOP or "")
    self.wrdb:set(gps_prefix .. "pdop",self.luaq.is_c_true(resp.DOP_valid) and resp.DOP.PDOP or "")
    self.wrdb:set(gps_prefix .. "vdop",self.luaq.is_c_true(resp.DOP_valid) and resp.DOP.VDOP or "")

    local pdop = self.luaq.is_c_true(resp.DOP_valid) and resp.DOP.PDOP or ""

    -- set common gps variables only if all mandatory parameters are valid
    if self.luaq.is_c_true(resp.latitude_valid) and
      self.luaq.is_c_true(resp.longitude_valid) and
      self.luaq.is_c_true(resp.altitudeWrtMeanSeaLevel_valid) and
      self.luaq.is_c_true(resp.altitudeWrtEllipsoid_valid) and
      self.luaq.is_c_true(resp.timestampUtc_valid) then

      if standalone_lock then
        valid_gps_source = "standalone"
      elseif assist_lock then
        valid_gps_source = "agps"
      end

      if valid_gps_source then
        self.historical_gps_source = "historical-" .. valid_gps_source
        self:update_common_gps_info({
          latitude = lat,
          latitude_direction = lat_dir,
          latitude_degrees = lat_degrees,
          longitude = lon,
          longitude_direction = lon_dir,
          longitude_degrees = lon_degrees,
          altitude = resp.altitudeWrtEllipsoid,
          height_of_geoid = resp.altitudeWrtMeanSeaLevel,
          pdop = pdop,
          date = gps_date,
          time = gps_time,
          session_status = session_status,
          horizontal_uncertainty = resp.horUncCircular,
          vertical_uncertainty = resp.vertUnc,
        })
      end
    end
  end

  self.wrdb:set("sensors.gps.0.standalone.valid",(standalone_lock and lat_long_valid) and "valid" or "invalid")
  self.wrdb:set("sensors.gps.0.assisted.valid",(assist_lock and lat_long_valid) and "valid" or "invalid")

  if lat_long_valid and (standalone_lock or assist_lock) then
    self.wrdb:set("sensors.gps.0.common.valid", "valid")
  else
    self.wrdb:set("sensors.gps.0.common.valid", "invalid")
  end

  self:update_common_gps_source(valid_gps_source or self.historical_gps_source)

  return true
end

function QmiLoc:QMI_LOC_EVENT_GNSS_SV_INFO_IND(type, event, qm)

  return true
end

function QmiLoc:QMI_LOC_EVENT_NMEA_IND(type, event, qm)

  return true
end

function QmiLoc:QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND(type, event, qm)
  return true
end

QmiLoc.cbs={
  "QMI_LOC_EVENT_POSITION_REPORT_IND",
  "QMI_LOC_EVENT_GNSS_SV_INFO_IND",
  "QMI_LOC_EVENT_NMEA_IND",
  "QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND",
  "QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND",
}

function QmiLoc:start_gps(type,event,a)

  local succ,err,res

  succ,err,res = self.luaq.req(self.m.QMI_LOC_START,{
    sessionId=self.session_id,

    fixRecurrence="eQMI_LOC_RECURRENCE_PERIODIC_V02",
    horizontalAccuracyLevel="eQMI_LOC_ACCURACY_HIGH_V02",
    intermediateReportState="eQMI_LOC_INTERMEDIATE_REPORTS_ON_V02",
    minInterval=1000, -- 1000 ms
  })

  return succ
end

function QmiLoc:stop_gps(type,event,a)
  -- FIXME: qmiLocStopRespMsgT_v02 is not defined in *.pch files and luaqmi will raise error in new_msg function:
  --        Uncaught error in callback: /usr/share/lua/5.1/luaqmi.lua:621: declaration specifier expected near 'qmiLocStopRespMsgT_v02'
  local succ,err,res = self.luaq.req(self.m.QMI_LOC_STOP,{sessionId=self.session_id})

  return succ
end

-- callback for QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND
function QmiLoc:QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND(type, event, qm)
  local resp = qm.resp
  local notiType = tonumber(resp.notificationType) or -1

  self.l.log("LOG_INFO", string.format("QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND: notiType=%d", notiType))

  --[[
    typedef struct {
      qmiLocNiNotifyVerifyEnumT_v02 notificationType;
      uint8_t NiVxInd_valid;
      qmiLocNiVxNotifyVerifyStructT_v02 NiVxInd;
      uint8_t NiSuplInd_valid;
      qmiLocNiSuplNotifyVerifyStructT_v02 NiSuplInd;
      uint8_t NiUmtsCpInd_valid;
      qmiLocNiUmtsCpNotifyVerifyStructT_v02 NiUmtsCpInd;
      uint8_t NiVxServiceInteractionInd_valid;
      qmiLocNiVxServiceInteractionStructT_v02 NiVxServiceInteractionInd;
      uint8_t NiSuplVer2ExtInd_valid;
      qmiLocNiSuplVer2ExtStructT_v02 NiSuplVer2ExtInd;
      uint8_t suplEmergencyNotification_valid;
      qmiLocEmergencyNotificationStructT_v02 suplEmergencyNotification;
    }qmiLocEventNiNotifyVerifyReqIndMsgT_v02;

    typedef struct {
      qmiLocNiUserRespEnumT_v02 userResp;
      qmiLocNiNotifyVerifyEnumT_v02 notificationType;
      uint8_t NiVxPayload_valid;
      qmiLocNiVxNotifyVerifyStructT_v02 NiVxPayload;
      uint8_t NiSuplPayload_valid;
      qmiLocNiSuplNotifyVerifyStructT_v02 NiSuplPayload;
      uint8_t NiUmtsCpPayload_valid;
      qmiLocNiUmtsCpNotifyVerifyStructT_v02 NiUmtsCpPayload;
      uint8_t NiVxServiceInteractionPayload_valid;
      qmiLocNiVxServiceInteractionStructT_v02 NiVxServiceInteractionPayload;
      uint8_t NiSuplVer2ExtPayload_valid;
      qmiLocNiSuplVer2ExtStructT_v02 NiSuplVer2ExtPayload;
      uint8_t suplEmergencyNotification_valid;
      qmiLocEmergencyNotificationStructT_v02 suplEmergencyNotification;
    }qmiLocNiUserRespReqMsgT_v02;
  ]]--

  if resp.notificationType ~= "eQMI_LOC_NI_USER_NO_NOTIFY_NO_VERIFY_V02" and
     resp.notificationType ~= "eQMI_LOC_NI_USER_NOTIFY_VERIFY_PRIVACY_OVERRIDE_V02" then
    -- Informational only. Show time of last NI location request if allowed.
    self.wrdb:setp("loc.ni_req_time", os.date())
  end

  local qmi_user_response
  if resp.notificationType == "eQMI_LOC_NI_USER_NOTIFY_VERIFY_ALLOW_NO_RESP_V02" or
     resp.notificationType == "eQMI_LOC_NI_USER_NOTIFY_VERIFY_NOT_ALLOW_NO_RESP_V02" then

    -- Notification requires verification. Get user response. Use "noresp" if user
    -- response is not set.
    local user_response=self.wrdb:getp("loc.ni_verify_resp") or "noresp"
    local user_response_map = {
      accept="eQMI_LOC_NI_LCS_NOTIFY_VERIFY_ACCEPT_V02",
      deny="eQMI_LOC_NI_LCS_NOTIFY_VERIFY_DENY_V02",
      noresp="eQMI_LOC_NI_LCS_NOTIFY_VERIFY_NORESP_V02",
    }
    qmi_user_response=user_response_map[user_response] or "eQMI_LOC_NI_LCS_NOTIFY_VERIFY_NORESP_V02"
    self.l.log("LOG_INFO", string.format("QMI_LOC_NI_USER_RESPONSE user_response=%s, qmi_response=%s",user_response,qmi_user_response))
  else
    -- Notification does not require verificaiton. Need to set a value anyway so set to accept.
    qmi_user_response="eQMI_LOC_NI_LCS_NOTIFY_VERIFY_ACCEPT_V02"
  end

  local opt = {
    userResp=qmi_user_response,
    notificationType=resp.notificationType,
    NiVxPayload=self.luaq.is_c_true(resp.NiVxInd_valid) and resp.NiVxInd or nil,
    NiSuplPayload=self.luaq.is_c_true(resp.NiSuplInd_valid) and resp.NiSuplInd or nil,
    NiUmtsCpPayload=self.luaq.is_c_true(resp.NiUmtsCpInd_valid) and resp.NiUmtsCpInd or nil,
    NiVxServiceInteractionPayload=self.luaq.is_c_true(resp.NiVxServiceInteractionInd_valid) and resp.NiVxServiceInteractionInd or nil,
    NiSuplVer2ExtPayload=self.luaq.is_c_true(resp.NiSuplVer2ExtInd_valid) and resp.NiSuplVer2ExtInd or nil,
    suplEmergencyNotification=self.luaq.is_c_true(resp.suplEmergencyNotification_valid) and resp.suplEmergencyNotification or nil,
  }

  -- it is required to reply for all noti types including for NO_NOTIFY_NO_USER.
  local succ,err,res = self.luaq.req(self.m.QMI_LOC_NI_USER_RESPONSE,opt)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_LOC_NI_USER_RESPONSE failed"))
  end

  return succ
end

QmiLoc.cbs_system={
  "start_gps",
  "stop_gps",
}

function QmiLoc:init()

  self.l.log("LOG_INFO", "initiate qmi_loc")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- initiate rdb
  self.wrdb:set("sensors.gps.0.standalone.valid","invalid")
  self.wrdb:set("sensors.gps.0.assisted.valid","invalid")
  self.wrdb:set("sensors.gps.0.source","historical")

  local succ,err,res = self.luaq.req(self.m.QMI_LOC_REG_EVENTS,{
    eventRegMask = self.bit.bor(
      0x001, -- position report
      0x002, -- satellite report event
      0x004, -- NMEA report
      0x008, -- QMI_LOC_EVENT_MASK_NI_NOTIFY_VERIFY_REQ
      0x000
    )
  })

end

return QmiLoc
