-- Copyright (C) 2018 NetComm Wireless limited.
--
-- Example web request handlers.

require('variants')
require('luardb')
local turbo = require("turbo")
local util = require("rds.util")

local config = require("rds.config")
local wmmdconfig = require("wmmd.config")
local basepath = config.basepath
local logger = require("rds.logging")
local lfs = require("lfs")

--------- SMS handlers ----------
function enc_base64(data)
    -- Base64 character table string
    local base64_chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    return ((data:gsub('.', function(x)
        local r, b = '', x:byte()
        for i = 8, 1, -1 do
            r = r..(b%2^i-b%2^(i-1)>0 and '1' or '0')
        end
        return r;
    end)..'0000'):gsub('%d%d%d?%d?%d?%d?', function(x)
        if #x < 6 then return '' end
        local c = 0
        for i = 1, 6 do
            c = c + (x:sub(i,i)=='1' and 2^(6-i) or 0)
        end
        return base64_chars:sub(c+1,c+1)
    end)..({ '', '==', '=' })[#data%3+1])
end

--[[
  Incoming SMS are saved in /usr/local/cdcs/conf/sms/incoming by default and
  the path is defined in wmmd/config.lua

  Filename format for the messages are;
      complete message file format : rxmsg_yymmddhhmmss_[un]read
              where yymmddhhmmss is time stamp of the message
      partial message file format : partmsg_xx_yy_zz
              where xx : reference number
                    yy : total message number
                    zz : message index

  Message file example:
    From : 764332637249279                     -- sender address
    Time : 25/07/18 - 14:09:14 - gmt: +01:00   -- received time stamp
    LocalTimestamp :    -- UTC timestamp of received time in seconds(where the epoch is 00:00:00 UTC, January 1, 1970)
    LocalTimeOffset : time offset between UTC time and local time.("UTC time" - "local time")
    Coding : UCS2                              -- encoding type [GSM7|8BIT|UCS2]
    Wap : false                                -- wap message (false or true)
    This is SMS test message...                -- message body
]]--

-- read a message from the file
local function readMsgFile(fileName)
    -- pair of field name of SMS file and entry name of converted lua table.
    local field_pairs = {
        {"From", "number"},
        {"Time", "time"},
        {"LocalTimestamp", "local_timestamp", optional=true},
        {"LocalTimeOffset", "local_timeoffset", optional=true},
        {"Coding", "coding"},
        {"Wap", "wap"},
    }
    local msg = {}
    local fn = wmmdconfig.incoming_sms_dir.."/"..fileName
    local fh = io.open(fn, "r")
    if not fh then
        logger.logErr("failed to open file " .. fn)
        return nil
    end

    msg.filename = fileName
    local line_content = nil
    for _, pair in ipairs(field_pairs) do
        local fline = line_content or fh:read() or ''
        local content = string.match(fline, "^" .. pair[1] .. "%s*:%s*(.*)$")
        if not content and not pair.optional then
            logger.logErr("Check sms file " .. fn .. ", invalid " .. pair[1])
            fh:close()
            return nil
        end
        if content then
            msg[pair[2]] = content
            line_content = nil -- read new line
        else
            line_content = fline -- optional field. keep previous line
        end
    end

    -- Convert CRLF(Dos) to LF(Unix)(dos2unix)
    local unixText = string.gsub(fh:read("*a") or '', '\r\n', '\n')
    msg.text = enc_base64(unixText)
    fh:close()

    if (string.match(fileName, "_read$") == "_read") then
           msg.read = 1
        else
           msg.read = 0
        end
    return msg
end

local SmsAllHandler = class("SmsAllHandler", turbo.web.RequestHandler)
function SmsAllHandler:get(path)
    logger.logDbg("get all SMS messages at " .. wmmdconfig.incoming_sms_dir)
    local msgtbl = {}
    local result = true
    for file in lfs.dir(wmmdconfig.incoming_sms_dir) do
        local fn = wmmdconfig.incoming_sms_dir.."/"..file
        local prefix = string.sub(file, 1, 5)
        if lfs.attributes(fn, "mode") == "file" and prefix == "rxmsg" then
            msg = readMsgFile(file)
            if msg then
        table.insert(msgtbl, msg)
      end
    end
    end
    --  sort the messages by rx time
    table.sort(msgtbl, function(msg1, msg2)
          local pattern = "(%d+)/(%d+)/(%d+)%s%-%s(%d+):(%d+):(%d+)"
          local day, month, year, hour, minute, sec = string.match(msg1.time, pattern)
          local rxTime1 = year..month..day..hour..minute..sec
          day, month, year, hour, minute, sec = string.match(msg2.time, pattern)
          local rxTime2 =  year..month..day..hour..minute..sec
          return rxTime1 < rxTime2
    end )
    if result then
      self:write(msgtbl)
      self:set_status(200)
    else
      self:set_status(500)
    end
end




function SmsAllHandler:put(path)
    -- get the number and the description from the client request
    local jsonTable = self:get_json()
    local filename = jsonTable["filename"]
    logger.logDbg("Change SMS file " ..filename .. " status to READ")
    local fn = wmmdconfig.incoming_sms_dir.."/"..filename
    local newfn = string.gsub(fn, "_unread", "_read")
    local res = os.rename(fn, newfn)
    if res == nil then
      logger.logErr("failed to rename file " .. fn)
      self:set_status(500)
    else
      self:set_status(200)
    end
end

function SmsAllHandler:delete(path)
    local jsonTable = self:get_json()
    -- get the filename list to be delete from the client request
    local items = jsonTable["items"]
    -- the filename list is a string, so split it into individual value
    for filename in (items..','):gmatch("(.-)"..',') do
        logger.logDbg("delete message file " .. filename)
        local fn = wmmdconfig.incoming_sms_dir.."/"..filename
        local res = os.remove(fn)
        if res == nil then
            logger.logErr("failed to delete file " .. fn)
            self:set_status(500)
            break
        end
    end
    self:set_status(200)
end

--------- Modem status query handler ----------
-- return the number of unread messages
-- if SMS service is not enabled or inbox directory does not exist, return nil.
local function getSmsUnreadCnt()
	local cnt = 0
	for entry in lfs.dir(wmmdconfig.incoming_sms_dir) do
		local fullpath = wmmdconfig.incoming_sms_dir .. '/' .. entry
		if lfs.attributes(fullpath).mode == 'file' and string.match(entry, "^rxmsg_%d+_unread$") then
			cnt = cnt + 1
		end
	end
	return tostring(cnt)
end

local ModemStatusHandler = class("ModemStatusHandler", turbo.web.RequestHandler)
function ModemStatusHandler:get(path)
    local resp = {
	    -- ["SIM OK"|"SIM BUSY"|"SIM PIN"|"SIM PUK"|"SIM BLOCKED"|"SIM ERR"|""]
	    sim_status = luardb.get('wwan.0.sim.status.status') or '',
	    sim_iccid = luardb.get('wwan.0.system_network_status.simICCID') or '',
	    sim_msisdn = luardb.get('wwan.0.sim.data.msisdn') or '',
	    -- [0~4]
	    -- 0(NOT_REGISTERED), 1(REGISTERED), 2(NOT_REGISTERED_SEARCHING)
	    -- 3(REGISTRATION_DENIED), 4(REGISTRATION_UNKNOWN)
	    network_reg_status = luardb.get('wwan.0.system_network_status.reg_stat') or '',
	    service_provider = luardb.get('wwan.0.system_network_status.network') or '',
	    network_name_long = luardb.get('wwan.0.system_network_status.nw_name.long') or '',
	    network_name_short = luardb.get('wwan.0.system_network_status.nw_name.short') or '',
	    -- lower case string.
	    network_type = luardb.get('wwan.0.system_network_status.service_type') or '',
	    signal_strength_rsrp = luardb.get('wwan.0.signal.0.rsrp') or '', -- 4G
	    signal_strength_rscp = luardb.get('wwan.0.cell_measurement.rscp') or '', -- 3G
	    signal_strength_rssi = luardb.get('wwan.0.signal.0.rssi') or '', -- 2G
	    unread_sms_cnt = getSmsUnreadCnt(), -- If SMS service is not enabled, unset unread_sms_cnt.
	    voicemail_active = luardb.get('wwan.0.mwi.voicemail.active') or '0',
	    voicemail_cnt = luardb.get('wwan.0.mwi.voicemail.count') or '0',
	    modem_imei = luardb.get('wwan.0.imei') or '',
	    router_firmware_version = luardb.get('sw.version') or '',
	    product_hwver = luardb.get('system.product.hwver') or '',
	    battery_present = luardb.get('system.battery.present') or '0', -- [1|0]: battery is installed or not
	    battery_capacity = luardb.get('system.battery.capacity') or '0', -- 0 ~ 100 (approximate percent of capacity)
	    battery_charging_state = luardb.get('system.battery.charging_state') or '', -- [None | Trickle | Fast | Taper]
	    battery_status = luardb.get('system.battery.status') or '', -- [Charge | Discharge | None]
	    battery_dcin_present = luardb.get('system.battery.dcin_present') or '1', -- [1|0]: 9V barrel power is present or not.
    }
    self:write(resp)
    self:set_status(200)
end

--------- Configuration Backup/Restore handler ----------
local ConfigManagementHandler = class("ConfigManagementHandler", turbo.web.RequestHandler)

-- create temporary directory and return fullpath name or nil
local function createTmpDir()
    local res = nil
    local fd = io.popen('mktemp -d -t "tmp.cfgBackup-XXXXXX"')
    if not fd then return res end
    res = fd:read('*l')
    fd:close()
    return res
end

-- file transfer between MDM and IPQ.
-- (Currently, only MDM can initiate file transfer)
--
-- srcPath, desPath: full path name for source and destination.
-- return true or false
local function transferFile(srcPath, desPath)
    -- -o "StrictHostKeyChecking no" option is added to avoid authenticity warning.
    local privKey = luardb.get("ipc.ssh.privkey") or "/tmp/mdm_ssh_key.priv"
    local cmd = string.format('scp -i %s -o "StrictHostKeyChecking no" %s %s 1>/dev/null 2>&1', privKey, srcPath, desPath)
    return os.execute(cmd) == 0
end

local cfgFile_rdb = "backup_mdm.cfg"
local cfgFile_mdm = "backup_mdm.tar.gz"

-- remove lines from a file that match patterns in exclude_patterns table.
--
-- filepath: file path
-- exclude_patterns: pattern table to remove from given file.
--
-- return true or false
local function exclude_lines(filepath, exclude_patterns)
    assert(exclude_patterns and type(exclude_patterns) == 'table', "Invalid pattern table")
    local content = {}
    local fd = io.open(filepath, "r")
    if not fd then return false end

    for line in fd:lines() do
        local excluded = false
        for _, patt in pairs(exclude_patterns) do
            if line:match(patt) then
                excluded = true
                break
            end
        end
        if not excluded then
            table.insert(content, line)
        end
    end

    fd:close()

    fd = io.open(filepath, "w+")
    if not fd then return false end

    for _, line in pairs(content) do
        fd:write(string.format( "%s\n", line ))
    end
    fd:close()
    return true
end

-- create backup file
--
-- targetDir: local target directory backup file located.
-- return: full path name of the file or nil.
local function createBackupFile(targetDir)
    local cfgFileTbl = {}

    local cmd
    -- create rdb backup file
    cmd = string.format("dbcfg_export -o %s/%s", targetDir, cfgFile_rdb)
    if os.execute(cmd) == 0  then
        table.insert(cfgFileTbl, cfgFile_rdb)
    else
        return nil
    end

    local exclude_patts = {
        "^voice_call%.call_history%..?", -- remove call_log list.
    }
    exclude_lines(string.format("%s/%s", targetDir, cfgFile_rdb), exclude_patts)

    -- create mdm backup file
    cmd = string.format("tar -C %s -czvf %s/%s %s", targetDir, targetDir, cfgFile_mdm, table.concat(cfgFileTbl, ' '))
    if os.execute(cmd) == 0  then
        return targetDir .. '/' .. cfgFile_mdm
    else
        return nil
    end
end

-- restore configuration with backup file
--
-- targetDir: local target directory backup file located.
-- return true or false
local function restoreConfig(targetDir)
    local cmd

    -- unzip mdm backup file
    cmd = string.format("tar -C %s -xzvf %s/%s", targetDir, targetDir, cfgFile_mdm)
    if os.execute(cmd) ~= 0  then
        return false
    end

    -- restore rdb backup file
    cmd = string.format("dbcfg_import -i %s/%s", targetDir, cfgFile_rdb)
    if os.execute(cmd) ~= 0  then
        return false
    end

    if variants.V_RESTORE_MODULE_PROFILE == 'fisher' then
        -- restore modem profile, if necessary.
        cmd = string.format('/usr/bin/module_profile_restore "%s/%s" saved "enable" 2>/dev/null', targetDir, cfgFile_rdb)
        if os.execute(cmd) ~= 0  then
            return false
        end
    end

    return true
end

function ConfigManagementHandler:put(path)
    -- configuration backup handler.
    --
    -- req: request body {targetpath:"/path/to/transfer"}
    --      generated backup file is transferred to "targetpath" on IPQ.
    -- return: true or false
    local function l_backup(req)
        if req and req.targetpath then
            local tmpDir = createTmpDir()
            if tmpDir then
                local srcPath = createBackupFile(tmpDir)
                if srcPath then
                    local ipcUser = 'root'
                    local ipcAddr = luardb.get("ipc.app.0.ip_address")
                    local destPath = string.format('%s@%s:%s', ipcUser, ipcAddr, req.targetpath)
                    if transferFile(srcPath, destPath) then
                        os.execute(string.format('rm -rf %s', tmpDir))
                        return true
                    end
                end
                os.execute(string.format('rm -rf %s', tmpDir))
            end
        end
        return false
    end

    -- configuration restore handler.
    --
    -- req: request body {targetpath:"/path/of/backupfile"}
    --      full path of backup file stored on IPQ.
    -- return: true or false
    local function l_restore(req)
        if req and req.targetpath then
            local tmpDir = createTmpDir()
            if tmpDir then
                local ipcUser = 'root'
                local ipcAddr = luardb.get("ipc.app.0.ip_address")
                local srcPath = string.format('%s@%s:%s', ipcUser, ipcAddr, req.targetpath)
                local destPath = string.format('%s/%s', tmpDir, cfgFile_mdm)
                if transferFile(srcPath, destPath) and restoreConfig(tmpDir) then
                    os.execute(string.format('rm -rf %s', tmpDir))
                    -- system restart triggered by IPQ.
                    return true
                end
                os.execute(string.format('rm -rf %s', tmpDir))
            end
        end
        return false
    end

    -- Factory reset handler
    local function l_reset(req)
        os.execute("dbcfg_default -m -f -r&")
        return true
    end

    -- Network reset handler
    local function l_reset_network(req)
        -- By default, module_profile_restore will reset "apn,user,pass,pdp_type,auth_type,apn_type".
        local extraTbl = {'enable'}
        if variants.V_CUSTOM_APN == "y" then
            table.insert(extraTbl, 'defaultroute')
        end
        local extra = table.concat(extraTbl, ',')

        local cmd = string.format('/usr/bin/module_profile_restore "%s" default %s 2>/dev/null', "/etc/cdcs/conf/default.conf", extra)
        if variants.V_PARTITION_LAYOUT == "fisher_ab" then
            -- Normal reboot. Reset BOOT_COUNT
            cmd = string.format('%s && flashtool --accept', cmd)
        end
        cmd = string.format('%s && rdb set service.syslog.lastreboot "network reset" && rdb_set service.system.reset 1', cmd)
        cmd = string.format('( %s )&', cmd)
        if os.execute(cmd) == 0 then
            return true
        end
        return false
    end

    local actionList = {
        ['backup'] = l_backup,
        ['restore'] = l_restore,
        ['reset'] = l_reset,
        ['reset_network'] = l_reset_network,
    }

    local action = path:gsub("(.*/)(.*)", "%2")
    if not actionList[action] then
        self:set_status(404) -- resource not found
        return
    end

    local req = self:get_json()
    local result = actionList[action](req)
    self:set_status(result and 200 or 400)
end

--------- Diagnostic status query handler ----------
local function getCurrentBand()
  luardb.set('wwan.0.currentband.cmd.status', '')
  luardb.set('wwan.0.currentband.cmd.command', 'get')
  local timeout = 5
  local cnt = 0
  local cmdStatus = ''
  while cnt < timeout do
    cmdStatus = luardb.get('wwan.0.currentband.cmd.status') or ''
    if cmdStatus ~= '' then
      break
    end
    os.execute('sleep 1')
    cnt = cnt + 1
  end
  if cnt >= timeout or cmdStatus ~= '[done]' then
    logger.logErr("Failed to get current band selection setting")
    return nil
  end
  return luardb.get('wwan.0.currentband.current_selband') or luardb.get('wwan.0.currentband.config') or nil
end

local function isImsEnabled()
  local testMode = luardb.get('wwan.0.ims.register.test_mode') or '0'
  if testMode == '0' then
    return 'enabled'
  end
  return 'disabled'
end

local DiagStatusHandler = class("DiagStatusHandler", turbo.web.RequestHandler)
function DiagStatusHandler:get(path)
    -- wwan.0.currentband.current_selband is not set until band selection operation is done once
    -- so better to read once if band selection command is empty
    local band_cmd = luardb.get('wwan.0.currentband.cmd.command') or ''
    if band_cmd == '' then
      getCurrentBand()
    end
    local resp = {
      reg_status = luardb.get('wwan.0.system_network_status.registered') or '',
      attached = luardb.get('wwan.0.system_network_status.attached') or '',
      band_selection = luardb.get('wwan.0.currentband.current_selband') or luardb.get('wwan.0.currentband.config') or '',
      serving_system = string.upper(luardb.get('wwan.0.system_network_status.service_type')) or '',
      last_band_cmd = luardb.get('wwan.0.currentband.cmd.param.band') or '',
      -- Common fields in UMTS/LTE
      channel_frequency = luardb.get('wwan.0.system_network_status.channel') or '',
      mcc = luardb.get('wwan.0.system_network_status.MCC') or '',
      mnc = luardb.get('wwan.0.system_network_status.MNC') or '',
      lac = luardb.get('wwan.0.system_network_status.LAC') or '',
      rrc_state = luardb.get('wwan.0.radio_stack.rrc_stat.rrc_stat') or '',
      -- UMTS fields
      umts_rac = luardb.get('wwan.0.system_network_status.RAC') or '',
      umts_cell_id = luardb.get('wwan.0.system_network_status.CellID') or '',
      umts_rscp = luardb.get('wwan.0.cell_measurement.rscp') or '',
      umts_ecio = luardb.get('wwan.0.radio.information.ecio') or '',
      umts_rrc_mode = luardb.get('wwan.0.cell_measurement.wcdma_rrc_state') or '',
      -- LTE fields
      lte_pcid = luardb.get('wwan.0.system_network_status.PCID') or '',
      lte_bandwidth = luardb.get('wwan.0.radio_stack.e_utra_measurement_report.dl_bandwidth') or '',
      lte_freq_band = luardb.get('wwan.0.system_network_status.current_band') or '',
      lte_ul_freq = luardb.get('wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.ul_freq') or '',
      lte_dl_freq = luardb.get('wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq') or '',
      lte_tac = luardb.get('wwan.0.radio.information.tac') or '',
      lte_rsrp = luardb.get('wwan.0.signal.0.rsrp') or '',
      lte_rsrq = luardb.get('wwan.0.signal.rsrq') or '',
      lte_scell_pci = luardb.get('wwan.0.radio_stack.e_utra_measurement_report.scell.pci') or '',
      lte_scell_bandwidth = luardb.get('wwan.0.radio_stack.e_utra_measurement_report.scell.dl_bandwidth') or '',
      lte_scell_band = luardb.get('wwan.0.radio_stack.e_utra_measurement_report.scell.dl_band') or '',
      -- IMS
      ims_enabled = isImsEnabled(),
      ims_status = luardb.get('wwan.0.ims.register.reg_stat') or '',
      -- SIM
      sim_status = luardb.get('wwan.0.sim.status.status') or '',
    }
    self:write(resp)
    self:set_status(200)
end

--------- IMS mode handler ----------
local ImsModeHandler = class("ImsModeHandler", turbo.web.RequestHandler)
function ImsModeHandler:put(path)
    local jsonTable = self:get_json()
    if jsonTable == nil then
      logger.logErr("Failed to get json table")
      self:set_status(500)
      return
    end
    local imsEnable = jsonTable["enable"]
    logger.logNotice(string.format("IMS enable %s", imsEnable))
    -- wwan.0.ims.register.test_mode = '0' --> invoke IMS enabled
    -- wwan.0.ims.register.test_mode = '1' --> invoke IMS disabled
    luardb.set('wwan.0.ims.register.test_mode', imsEnable == 'true' and '0' or '1')
    self:set_status(200)
end

--------- Band selection handler ----------
local function setBand(newband, multi)
  luardb.set('wwan.0.currentband.cmd.status', '')
  if newband == nil or newband == '' then
    logger.logErr("Invalid band setting value")
    return false
  end
  luardb.set('wwan.0.currentband.cmd.param.band', newband)
  luardb.set('wwan.0.currentband.cmd.command', 'set')
  local timeout = 30
  local cnt = 0
  local cmdStatus = ''
  while cnt < timeout do
    cmdStatus = luardb.get('wwan.0.currentband.cmd.status') or ''
    if cmdStatus ~= '' then
      break
    end
    os.execute('sleep 1')
    cnt = cnt + 1
  end
  -- update current band after set command for diag status page
  getCurrentBand()
  if cnt >= timeout or cmdStatus ~= '[done]' then
    logger.logErr("Failed to change band selection")
    return false
  end

  local grouping = luardb.get('wmmd.config.enable_multi_band_sel')

  if multi or grouping == '1' then
    return true
  end
  return getCurrentBand() == newband
end

local function is_multiband(band)
  local fullBandTable = {
    ["All bands"] = true,
    ["LTE all"] = true,
    ["WCDMA all"] = true,
    ["WCDMA/LTE all"] = true
  }
  return not fullBandTable[band]
end

local ModemBandSelHandler = class("ModemBandSelHandler", turbo.web.RequestHandler)
function ModemBandSelHandler:put(path)
  local jsonTable = self:get_json()
  if jsonTable == nil then
    logger.logErr("Failed to get json table")
    self:set_status(500)
    return
  end
  -- band : "All bands", "LTE all", "WCDMA all"
  --        or
  --        combination of below individual bands
  --        "WCDMA PCS 1900", "WCDMA 850", "LTE Band 2 - 1900Mhz",
  --        "LTE Band 4 - 1700Mhz", "LTE Band 5 - 850MHz",
  --        "LTE Band 12 - 700Mhz", "LTE Band 30 - 2300Mhz",
  local band = jsonTable["band"]
  local multiband = is_multiband(band)
  logger.logNotice(string.format(
    "Set modem band selection to %s, multiband %s", band, tostring(multiband)))

  -- Skip current band check for multiband
  if not multiband and getCurrentBand() == band then
    logger.logNotice("Already configured to " ..band)
    self:set_status(200)
    return
  end

  if setBand(band, multiband) then
    logger.logNotice("Modem band selection configured to " ..band)
    self:set_status(200)
  else
    logger.logErr("Failed to set band selection setting")
    self:set_status(500)
  end
end

--------- Modem reset handler ----------
-- When the modem is set to new band which is not supported
-- and loses connection then moves to original band again,
-- it waits 10 minutes before trying to reconnect. Even somstimes
-- keep failing to register/attach. Providing a command to power
-- cycle the modem in order to recover from that situation.
local ModemResetHandler = class("ModemResetHandler", turbo.web.RequestHandler)
function ModemResetHandler:put(path)
  logger.logNotice("Power cycling the modem")
  os.execute('qmisys lowpower')
  os.execute('sleep 0.1')
  os.execute('qmisys online')
  os.execute('sleep 10')
end

--------- Modem Time Info handler ----------
local ModemTimeInfoHandler = class("ModemTimeInfoHandler", turbo.web.RequestHandler)
-- If system time is synchronize, then return seconds since 1970-01-01 00:00:00 UTC.
function ModemTimeInfoHandler:get(path)
    local resp = {}

    -- Current system UTC time from timedaemon.
    if (luardb.get('system.time.source') or '') ~= '' then
        resp.currentUtc = os.date('%s')
    end

    -- Current network time from modem.
    resp.network_datetime = luardb.get('wwan.0.networktime.datetime') -- Ex) 2019-02-19 05:36:12
    resp.network_timezone = luardb.get('wwan.0.networktime.timezone') -- Ex) +11 DST 1

    self:write(resp)
    self:set_status(200)
end

function ModemTimeInfoHandler:put(path)
    -- set current time offset between UTC and IPQ.
    -- (offset = "UTC time" - "IPQ system local time")
    local function l_offset(req)
        if req and tonumber(req.timeOffset) then
            luardb.set('system.ipq.timeoffset', tonumber(req.timeOffset), 'p')
            return true
        end
        return false
    end

    local actionList = {
        ['offset'] = l_offset,
    }

    local action = path:gsub("(.*/)(.*)", "%2")
    if not actionList[action] then
        self:set_status(404) -- resource not found
        return
    end

    local req = self:get_json()
    local result = actionList[action](req)
    self:set_status(result and 200 or 400)
end

local function getPathLeaf(path, var)
  local i    = string.find( path, var, 1, true )
  local len  = string.len( var )
  local leaf = string.sub( path, i+len)

  return leaf
end

local function writeJson(key,value)
  local result = string.format("\"%s\": \"%s\"", key, value)
  return result
end



--------- System RDB handler ----------
local SystemRdbHandler = class("SystemRdbHandler", turbo.web.RequestHandler)

function SystemRdbHandler:put(path)
    logger.logErr("Not supported")
    self:set_status(500)
end

function SystemRdbHandler:get(path)
    local theLeaf = getPathLeaf( path, "system/rdb/" )
    local resp    = "{}"

    --logger.logNotice(string.format("JB: get path %s, leaf %s", path, theLeaf ))

    theLeaf = string.gsub( theLeaf, "/", ".")


    resp = "{" .. writeJson(theLeaf,   luardb.get(theLeaf)) .. "}"

    self:write(resp)
    self:set_status(200)
end

--------- Hardware ID handler ----------
local SystemHwidHandler = class("SystemHwidHandler", turbo.web.RequestHandler)

function SystemHwidHandler:put(path)
    local jsonTable = self:get_json()

    if jsonTable == nil then
      logger.logErr("Failed to get json table")
      self:set_status(500)
      return
    end

    local theLeaf = getPathLeaf( path, "system/hw_id/" )

    --logger.logNotice(string.format("JB: set path %s, leaf %s", path, theLeaf ))

    if( theLeaf == "hostboard/raw" ) then
      luardb.set('system.hwver.hostboard.raw', jsonTable["hostboard.raw"])
      -- template triggers hwver.lua update

    elseif( string.match( theLeaf, "rdbflags" ) ~= nil ) then
      local rdbVar = getPathLeaf( theLeaf, "rdbflags/")
      rdbVar = string.gsub( rdbVar, "/", ".")
      --logger.logNotice("JB: rdbflags: value: " ..  jsonTable["value"])
      luardb.setFlags( rdbVar, jsonTable["value"])

    elseif( string.match( theLeaf, "rdb" ) ~= nil ) then
      local rdbVar = getPathLeaf( theLeaf, "rdb/")
      rdbVar = string.gsub( rdbVar, "/", ".")
      luardb.set( rdbVar, jsonTable["value"])

    else
      logger.logErr(string.format("On path \'%s\', leaf \'%s\'' is not supported", path, theLeaf ))
      self:set_status(500)
      return
    end

    self:set_status(200)
end

function SystemHwidHandler:get(path)
    local theLeaf = getPathLeaf( path, "system/hw_id/" )
    local resp    = "{}"

    --logger.logNotice(string.format("JB: get path %s, leaf %s", path, theLeaf ))

    if( string.match( theLeaf, "rdb" ) ~= nil ) then
      local rdbVar = getPathLeaf( theLeaf, "rdb/")

      rdbVar = string.gsub( rdbVar, "/", ".")

      resp = "{" .. writeJson(rdbVar,   luardb.get(rdbVar)) .. "}"

    else
      logger.logErr(string.format("On path \'%s\', leaf \'%s\'' is not supported", path, theLeaf ))
      self:set_status(500)
      return
    end

    self:write(resp)
    self:set_status(200)
end

--------- System log download handler ----------
local SystemLogDownloadHandler = class("SystemLogDownloadHandler", turbo.web.RequestHandler)

-- The directory shared with nfs.
local baseDir_syslog = '/tmp/nfs_monitor/'

local msgfile_name = 'napa_systemlog.tar.gz'
local rdbfile_name = 'napa_rdbdump'

-- create system log archive
--
-- targetDir: local target directory archive file located.
-- return: true or false.
local function createSyslogArchive(targetDir)
    -- System log
    local msg_dir = '/var/log/'
    local msgfile_wildcard = "messages messages.[0-9] messages.[0-9][0-9]"
    cmd = string.format("cd %s && tar -czf %s/%s $(ls %s) 2>/dev/null", msg_dir, targetDir, msgfile_name, msgfile_wildcard)
    if os.execute(cmd) ~= 0  then
        return false
    end

    -- Rdb dump
    cmd = string.format("rdb dump > %s/%s 2>/dev/null", targetDir, rdbfile_name)
    if os.execute(cmd) ~= 0  then
        return false
    end

    return true
end

function SystemLogDownloadHandler:put(path)
    -- system log download handler.
    --
    -- req: request body {targetPathname:"target path name on baseDir_syslog""}
    --      generated archive files are generated with targetPathname in baseDir_syslog,
    --      which is shared between MDM and IPQ with nfs.
    -- return: true or false
    local function l_download(req)
        if req and req.targetPathname and req.targetPathname ~= ''  then
            local tmpDir = baseDir_syslog .. req.targetPathname
            if require("lfs").attributes(tmpDir, "mode") == "directory" then
                if createSyslogArchive(tmpDir) then
                        return true
                end
            end
        end
        return false
    end

    local actionList = {
        ['download'] = l_download,
    }

    local action = path:gsub("(.*/)(.*)", "%2")
    if not actionList[action] then
        self:set_status(404) -- resource not found
        return
    end

    local req = self:get_json()
    local result = actionList[action](req)
    self:set_status(result and 200 or 400)
end


-- Return array of path/handler mappings
return {{basepath .. "/messages$",                 SmsAllHandler},
        {basepath .. "/modem/modem_status",        ModemStatusHandler},
        {basepath .. "/config_management/[_%w]+$", ConfigManagementHandler},
        {basepath .. "/modem/diag_status",         DiagStatusHandler},
        {basepath .. "/modem/ims_mode",            ImsModeHandler},
        {basepath .. "/modem/band_sel",            ModemBandSelHandler},
        {basepath .. "/modem/reset",               ModemResetHandler},
        {basepath .. "/modem/time_info[/%w+]*$",   ModemTimeInfoHandler},
        {basepath .. "/system/hw_id[/%w%p]*",      SystemHwidHandler},
        {basepath .. "/system/rdb[/%w%p]*",        SystemRdbHandler},
        {basepath .. "/system_log_download/%w+$",  SystemLogDownloadHandler},
}

