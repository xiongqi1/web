--[[
This module handles http download and upload diagnostics.
.download(url, dscp, interface, startTime)
.upload(url, length, dscp, interface, startTime)
interface is used for the outgoing dev and when collecting statistics from /sys/class/net.
download/upload testing can be canceled by invoking setTestState('Canceled').
--]]

-----------------------------------------------------------------------------
-- Declare module and import dependencies
-------------------------------------------------------------------------------
local socket = require("socket")
local string = require("string")
local table = require("table")

-- this relies on global table conf
package.path = conf.fs.code .. '/?.lua;' .. conf.fs.code .. '/classes/?/?.lua;' .. conf.fs.code .. '/classes/?.lua;' .. conf.fs.code .. '/utils/?.lua;' .. package.path

require("handlers.hdlerUtil")
require("Daemon")
require("luardb")
require("luacurl")

local debug = conf.log.debug
local log_level = conf.log.level or 'info'
debug = true
log_level = 'info'

local utils = require 'tr143_utils'

-- debug info log utility. relies on luasyslog.open()
local function dinfo(...)
    if debug then
        local printResult = ''
        for _, v in ipairs{...} do
            printResult = printResult .. tostring(v) .. '\t'
        end
        luasyslog.log(log_level, printResult)
    end
end

-----------------------------------------------------------------------------
-- Program constants
-----------------------------------------------------------------------------
-- connection timeout in seconds
CONNECTTIMEOUT = 60
-- user agent field sent in request
USERAGENT = 'curl/' .. socket._VERSION

local systemFS = '/sys/class/net/' -- where to read interface statistics
local iface = 'wwan0' -- default interface

local diagData = {} -- diagnostics result data

local rdbPrefixDownload = conf.rdb.tr143Prefix .. '.download'
local rdbPrefixUpload = conf.rdb.tr143Prefix .. '.upload'

local testState -- this is to cancel in-progress testing
local function setTestState(state)
    testState = state
end

-- read interface statistics from sysfs
local function readStatistics(iface, name)
    local filename = systemFS .. iface .. '/statistics/' .. name
    local defaultV = 0
    if hdlerUtil.IsRegularFile(filename) then
        return Daemon.readIntFromFile(filename) or defaultV
    end
    return defaultV
end

local function updateRdbDownload()
    luardb.set(rdbPrefixDownload .. '.romtime', diagData.romTime)
    luardb.set(rdbPrefixDownload .. '.bomtime', diagData.bomTime)
    luardb.set(rdbPrefixDownload .. '.eomtime', diagData.eomTime)
    luardb.set(rdbPrefixDownload .. '.test_bytes_rx', diagData.testBytesReceived)
    luardb.set(rdbPrefixDownload .. '.total_bytes_rx', diagData.totalBytesReceived)
    luardb.set(rdbPrefixDownload .. '.tcp_open_req_time', diagData.tcpOpenRequestTime)
    luardb.set(rdbPrefixDownload .. '.tcp_open_rsp_time', diagData.tcpOpenResponseTime)
    if diagData.state == 'Completed' then
        luardb.set(rdbPrefixDownload .. '.state', diagData.state)
    end
end

local function updateRdbUpload()
    luardb.set(rdbPrefixUpload .. '.romtime', diagData.romTime)
    luardb.set(rdbPrefixUpload .. '.bomtime', diagData.bomTime)
    luardb.set(rdbPrefixUpload .. '.eomtime', diagData.eomTime)
    luardb.set(rdbPrefixUpload .. '.total_bytes_tx', diagData.totalBytesSent)
    luardb.set(rdbPrefixUpload .. '.test_bytes_tx', diagData.testBytesSent)
    luardb.set(rdbPrefixUpload .. '.tcp_open_req_time', diagData.tcpOpenRequestTime)
    luardb.set(rdbPrefixUpload .. '.tcp_open_rsp_time', diagData.tcpOpenResponseTime)
    if diagData.state == 'Completed' then
        luardb.set(rdbPrefixUpload .. '.state', diagData.state)
    end
end

-- The purpose of the following two callbacks is to cancel the current transfer
local function download_callback(userparam, buffer)
    if testState == 'Canceled' or testState == 'Restart' then
        --[[
            Returning 0 signals an error condition to the library, which will
            cause the transfer to get aborted, resulting in a curl.WRITE_ERROR
            error code from curl.perform().
        --]]
        return 0
    end
    return #buffer
end

local len_to_upload = 0
local function upload_callback(userparam, size)
    if testState == 'Canceled' or testState == 'Restart' then
        --[[
            Returning a non-string type will cause luacurl to return
            CURL_READFUNC_ABORT so that the current operation will immediately
            stop, resulting in a curl.ABORTED_BY_CALLBACK error code from
            curl.perform().
        --]]
        return nil
    end
    if size >= len_to_upload then
        size = len_to_upload
        len_to_upload = 0
    else
        len_to_upload = len_to_upload - size
    end
    return string.rep('?', size) -- does not matter what to send
end

local function set_DiagnosticsState(val, downUp)
    downUp = downUp or 'download'
    diagData.state = val
    -- Completed will be set to rdb in updateRdb
    if val ~= 'Completed' then
        luardb.set(conf.rdb.tr143Prefix .. '.' .. downUp .. '.state', val)
    end
end

local function record_bytes_received_begin()
    diagData.rx_bytes_begin = readStatistics(iface, 'rx_bytes')
    dinfo('rx_bytes begin=' .. tostring(diagData.rx_bytes_begin))
    diagData.testBytesReceived = 0
end

local function record_bytes_received_end(rx_bytes_body)
    diagData.rx_bytes_end = readStatistics(iface, 'rx_bytes')
    dinfo('rx_bytes end=' .. tostring(diagData.rx_bytes_end))
    diagData.totalBytesReceived = diagData.rx_bytes_end - diagData.rx_bytes_begin
    diagData.testBytesReceived = diagData.testBytesReceived + rx_bytes_body
end

local function record_bytes_sent_begin()
    diagData.tx_bytes_begin = readStatistics(iface, 'tx_bytes')
    dinfo('tx_bytes begin=' .. tostring(diagData.tx_bytes_begin))
    diagData.testBytesSent = 0
end

local function record_bytes_sent_end(tx_bytes_body)
    diagData.tx_bytes_end = readStatistics(iface, 'tx_bytes')
    dinfo('tx_bytes end=' .. tostring(diagData.tx_bytes_end))
    diagData.totalBytesSent = diagData.tx_bytes_end - diagData.tx_bytes_begin
    diagData.testBytesSent = diagData.testBytesSent + tx_bytes_body
end

CURL_ERR_TO_TR143_STATE = {
    [curl.OK] = 'Completed',
    [curl.ABORTED_BY_CALLBACK] = 'Canceled',
    [curl.WRITE_ERROR] = 'Canceled',
    [curl.URL_MALFORMAT] = 'Error_CannotResolveHostName',
    [curl.COULDNT_RESOLVE_HOST] = 'Error_CannotResolveHostName',
    -- [curl.???] = 'Error_NoRouteToHost',
    [curl.COULDNT_CONNECT] = 'Error_InitConnectionFailed',
    [curl.INTERFACE_FAILED] = 'Error_InitConnectionFailed',
    [curl.HTTP_RETURNED_ERROR] = 'Error_NoResponse',
    [curl.GOT_NOTHING] = 'Error_NoResponse',
    [curl.RECV_ERROR] = 'Error_TransferFailed',
    [curl.SEND_ERROR] = 'Error_TransferFailed',
    -- [curl.???] = 'Error_PasswordRequestFailed',
    [curl.LOGIN_DENIED] = 'Error_LoginFailed',
    -- [curl.???] = 'Error_NoTransferMode',
    -- [curl.???] = 'Error_NoPASV',
    -- [curl.???] = 'Error_NoCWD',
    -- [curl.???] = 'Error_NoSTOR',
    -- [curl.???] = 'Error_NoTransferComplete',
    [curl.PARTIAL_FILE] = 'Error_IncorrectSize',
    [curl.FILESIZE_EXCEEDED] = 'Error_IncorrectSize',
    [curl.OPERATION_TIMEOUTED] = 'Error_Timeout',
    [curl.FAILED_INIT] = 'Error_Internal',
    [curl.OUT_OF_MEMORY] = 'Error_Internal',
    -- [curl.???] = 'Error_Other'
}

local function trequest(reqt)
    dinfo('trequest begin')

    local downUp = 'download'
    if reqt.method and reqt.method:upper() == 'PUT' then
        downUp = 'upload'
    end

    local urlparser = require('socket.url')
    local parsed_url = urlparser.parse(reqt.url)
    if parsed_url.scheme:upper() ~= 'HTTP' then
        -- only support http url. no https
        set_DiagnosticsState('Error_Other', downUp)
        return
    end

    local host = parsed_url.host
    -- remove bracket from IPv6 address as host. e.g. url=http://[2404:6800:4006:809::200e]:8080/index.html
    local ipv6 = host:match('^%[([0-9A-Fa-f:]+)%]$')
    if ipv6 then
        host = ipv6
    end
    local addr, dev_or_err, addr_type = utils.hostCheck(host, reqt.proto)
    if not addr then
        set_DiagnosticsState(dev_or_err, downUp)
        return
    end

    -- set the interface to collect stats from
    iface = reqt.device
    if not iface or iface == '' then
        -- if user does not specify interface, use routing table
        iface = dev_or_err
    end
    dinfo('iface=' .. iface)

    -- rebuild the url using resolved IP, adding bracket for IPv6
    local ori_host = parsed_url.host
    parsed_url.host = addr_type == 6 and ('[' .. addr .. ']') or addr
    local url = urlparser.build(parsed_url)
    dinfo('url=' .. url)

    ch = curl.new()

    if downUp == 'upload' then
        ch:setopt(curl.OPT_UPLOAD, true)
        ch:setopt(curl.OPT_READFUNCTION, reqt.callback)
        ch:setopt(reqt.len >= 2^31 and curl.OPT_INFILESIZE_LARGE or
                      curl.OPT_INFILESIZE, reqt.len)
        len_to_upload = reqt.len
    else
        ch:setopt(curl.OPT_WRITEFUNCTION, reqt.callback)
    end

    if reqt.device and reqt.device ~= '' then
        ch:setopt(curl.OPT_INTERFACE, reqt.device)
    end

    -- libcurl does not support direct TOS/DSCP setting.
    -- Use curl.OPT_SOCKOPTFUNCTION (needs luacurl patch).
    if reqt.tos then
        ch:setopt(curl.OPT_SOCKOPTFUNCTION, function(userparams, fd, purpose)
                      -- setsockopt expects a 32-bit integer
                      local optval = string.char(reqt.tos, 0, 0, 0)
                      if addr_type == 6 then
                          return curl.setsockopt(fd, curl.IPPROTO_IPV6, curl.IPV6_TCLASS, optval)
                      else
                          return curl.setsockopt(fd, curl.IPPROTO_IP, curl.IP_TOS, optval)
                      end
        end)
    end

    ch:setopt(curl.OPT_URL, url)
    ch:setopt(curl.OPT_HTTPHEADER,'Host: '..ori_host)
    ch:setopt(curl.OPT_USERAGENT, USERAGENT)
    -- ch:setopt(curl.OPT_TIMEOUT, TIMEOUT)
    ch:setopt(curl.OPT_CONNECTTIMEOUT, CONNECTTIMEOUT)

    -- TR-181 requires time resolution of microseconds, os.time() only does
    -- seconds. socket.gettime() does the job here.
    local timeRef = socket.gettime() -- this is the true beginning

    -- record network interface stats
    if downUp == 'upload' then
        record_bytes_sent_begin()
    else
        record_bytes_received_begin()
    end

    local ret, msg, errCode = ch:perform()

    if not ret then
        -- get error result
        local state = CURL_ERR_TO_TR143_STATE[errCode] or 'Error_Other'
        dinfo("Transfer failed: curlErr="..errCode, "state="..state)
        if state == 'Canceled' then
            -- 'Canceled' can be resulted from 'Canceled' or 'Restart'
            if testState == 'Canceled' then
                -- only user requested cancellation should be ack'ed with None
                set_DiagnosticsState('None', downUp)
            end
        else
            -- real errors
            set_DiagnosticsState(state, downUp)
        end
    else
        -- check http response code
        local code = ch:getinfo(curl.INFO_RESPONSE_CODE)
        -- only 200 (GET/PUT) and 201 (PUT) are acceptable
        if code ~= 200 and (code ~= 201 or downUp ~= "upload") then
            -- Error_NoResponse means no successful response as per TR-143
            set_DiagnosticsState("Error_NoResponse", downUp)
        else
            -- get normal result
            -- data size
            if downUp == 'upload' then
                local tx_bytes_body = ch:getinfo(curl.INFO_SIZE_UPLOAD)
                local tx_bytes_header = ch:getinfo(curl.INFO_REQUEST_SIZE)
                record_bytes_sent_end(tx_bytes_body + tx_bytes_header)
            else
                local rx_bytes_body = ch:getinfo(curl.INFO_SIZE_DOWNLOAD)
                local rx_bytes_header = ch:getinfo(curl.INFO_HEADER_SIZE)
                record_bytes_received_end(rx_bytes_body + rx_bytes_header)
            end

            -- time
            diagData.tcpOpenRequestTime = timeRef + ch:getinfo(curl.INFO_NAMELOOKUP_TIME)
            diagData.tcpOpenResponseTime = timeRef + ch:getinfo(curl.INFO_CONNECT_TIME)
            diagData.romTime = timeRef + ch:getinfo(curl.INFO_PRETRANSFER_TIME)
            diagData.bomTime = timeRef + ch:getinfo(curl.INFO_STARTTRANSFER_TIME)
            diagData.eomTime = timeRef + ch:getinfo(curl.INFO_TOTAL_TIME)

            -- state
            set_DiagnosticsState("Completed", downUp)

            -- move all stats from diagData into rdb
            if downUp == 'upload' then
                updateRdbUpload()
            else
                updateRdbDownload()
            end
        end
    end

    dinfo('Close')
    ch:close()
    dinfo('trequest end')
end

-- wait until startTime
-- return true if time is up; false if canceled or restart
local function waitStartTime(startTime, downUp)
    startTime = tonumber(startTime) or 0
    local now = os.time()
    dinfo('startTime=' .. tostring(startTime), 'now=' .. tostring(now))
    -- tr143 handler can cancel the waiting by setting testState
    while now < startTime and testState ~= 'Canceled' and testState ~= 'Restart' do
        luardb.wait(1)
        now = os.time()
    end
    if testState == 'Canceled' then
        -- need to set DiagnosticsState properly as per TR-143
        set_DiagnosticsState('None', downUp)
        dinfo('Waiting ' .. downUp .. ' is canceled')
        return false
    end
    -- if Restart is requested, DiagnosticsState should be set when the new upload/download session completes, so we do not need to do anything here.
    return testState ~= 'Restart'
end

local function download(url, dscp, intf, startTime, proto)
    dinfo("download: url="..tostring(url), "dscp="..tostring(dscp), "intf="..tostring(intf), "startTime="..tostring(startTime))
    local reqt = {
        url = url,
        callback = download_callback,
        tos = dscp and dscp*4, -- dscp is the 6 MSBs of DS field
        device = intf,
        proto = proto,
    }
    return waitStartTime(startTime, 'download') and trequest(reqt)
end

local function upload(url, len, dscp, intf, startTime, proto)
    dinfo("upload: url="..tostring(url), "dscp="..tostring(dscp), "intf="..tostring(intf), "startTime="..tostring(startTime))
    len = len or 1000000
    local reqt = {
        url = url,
        method = "PUT",
        len = len,
        callback = upload_callback,
        tos = dscp and dscp*4, -- dscp is the 6 MSBs of DS field
        device = intf,
        proto = proto,
    }
    return waitStartTime(startTime, 'upload') and trequest(reqt)
end

return {
    download = download,
    upload = upload,
    setTestState = setTestState,
}
