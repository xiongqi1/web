--[[
    NetComm OMA-DM Client

    FUMO.lua
    Firmware update management object.

    Copyright Notice:
    Copyright (C) 2018 NetComm Wireless Limited.

    This file or portions thereof may not be copied or distributed in any form
    (including but not limited to printed or electronic forms and binary or object forms)
    without the expressed written consent of NetComm Wireless Limited.
    Copyright laws and International Treaties protect the contents of this file.
    Unauthorized use is prohibited.

    THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
    WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
]]

require("accounts")
require("event")
require("object")
require("node-interior")
require("node-leaf")
require("node-rdb")
require("LuaXml")
require("rebase")
require("wap")

--[[
    RDB Variables

    service.dm.fumo.state          DM tree; ./FwUpdate/Flash/State
    service.dm.fumo.pkgurl         DM tree; ./FwUpdate/Flash/DownloadAndUpdate/PkgURL
    service.dm.fumo.dau.state      DAU persistent internal; current state
    service.dm.fumo.dau.serverid   DAU persistent internal; server ID that owns current operation
    service.dm.fumo.dau.correlator DAU persistent internal; correlator for current operation
    service.dm.fumo.dau.pkgurl     DAU persistent internal; current download target
    service.dm.fumo.dau.priority   DAU persistent internal; current install priority
    service.dm.fumo.dau.result     DAU persistent internal; update result to be reported
    service.dm.fumo.dau.installat  DAU persistent internal; scheduled install timestamp
    service.dm.fumo.dau.rebooting  DAU internal; indicates reboot hasn't happened yet
    service.dm.fumo.dau.dstpath    DAU configuration; destination path for download
    service.dm.fumo.dau.maxsize    DAU configuration; max size for download
    service.dm.fumo.trigger        FUMO trigger; request user-initiated session
    service.dm.fumo.serverid       FUMO configuration; server ID for UI/DI sessions
    service.dm.fumo.di.next        FUMO persistent internal; target timestamp for next DI
    service.dm.fumo.di.prev        FUMO persistent internal; timestamp for previous DI
    service.dm.fumo.ui.prev        FUMO persistent internal; timestamp for previous UI session
    service.dm.fumo.ui.wait        FUMO configuration; minimum delay between UI sessions in seconds
]]

-- FUMO result codes for DM generic alerts.
local DMRESULT = Enum({
    Success              = 200,
    PackageCorrupt       = 402,
    ValidationFailed     = 404,
    PackageNotAcceptable = 405,
    DownloadFailed       = 407,
    InstallFailed        = 410,
    MalformedURL         = 411,
    OutOfMemory          = 501,
})

-- FUMO state as reported via DM tree.
local DMSTATE = Enum({
    Idle                 = 10,
    DownloadFailed       = 20,
    DownloadInProgress   = 30,
    DownloadComplete     = 40,
    ReadyToUpdate        = 50,
    UpdateInProgress     = 60,
    UpdateFailed         = 70,
    UpdateFailedNoData   = 80,
    UpdateComplete       = 90,
    UpdateCompleteNoData = 100
})

-- Internal FUMO state for DownloadAndUpdate machine.
local STATE = Enum({
    Idle        = 1,
    Downloading = 2,
    Installing  = 3,
    Rebooting   = 4,
    Reporting   = 5
})

-- Update package priority, conveyed via download descriptors.
local PRIORITY = Enum({
    Normal   = 1,
    Critical = 2
})

-- Generic FUMO code.
local FUMO_RDB = "service.dm.fumo"
local fumo = {
    db = rdb.Rebase(FUMO_RDB),
    DESCRIPTOR_CT       = "application/vnd.oma.dd+xml",
    USER_REQUEST        = "org.openmobilealliance.dm.firmwareupdate.userrequest",
    DEVICE_REQUEST      = "org.openmobilealliance.dm.firmwareupdate.devicerequest",
    DI_CHECK_PERIOD     = 10000,
    DI_RETRY_COUNT      = 2,
    DI_WAIT_NORMAL      = 2592000,
    DI_WAIT_INITIAL     = 28800,
    DI_WAIT_RETRY       = 86400,
    UI_DEFAULT_WAIT     = 86400,
    DM_RETRY_COUNT      = 2,
    DM_RETRY_DELAY      = 120,
    DL_RETRY_COUNT      = 5,
    DL_RETRY_DELAY      = 120,
    REBOOT_WAIT         = 43200,
    REBOOT_CHECK_PERIOD = 10000
}

-- Code related to the DownloadAndUpdate state machine.
local DAU_RDB = "service.dm.fumo.dau"
local dau = {
    db = rdb.Rebase(DAU_RDB),
    DEFAULT_DSTPATH  = "/opt/fw.star",
    DEFAULT_MAXSIZE  = 100000000,
    DEFAULT_PRIORITY = PRIORITY.Critical
}

--[[
    FUMO DM object, firmware update operations are initiated here via
    an 'execute' operation on './FwUpdate/Flash/DownloadAndUpdate'.
]]
local object = Object("FwUpdate", "Get=*", nil,
    Node.Interior("Flash", "Get=*", "urn:oma:mo:oma-fumo:1.0",
        Node.RDB("State", nil, fumo.db:Path("state"), DMSTATE.Idle),
        Node.RDB("PkgName", "Get=*&Replace=*", fumo.db:Path("pkgname")),
        Node.RDB("PkgVersion", "Get=*&Replace=*", fumo.db:Path("pkgversion")),
        Node.Interior("DownloadAndUpdate", "Get=*&Exec=*", nil,
            Node.RDB("PkgURL", "Get=*&Replace=*", fumo.db:Path("pkgurl"))
        ):AddMethod("Execute",
            function(self, path, arg, correlator)
                local url = fumo.db:Get("pkgurl")
                if not url or url == "" then
                    return STATUS.NotAllowed
                end
                if dau.StartDownload(dm.GetServerID(), correlator, url) then
                    return STATUS.AcceptedForProcessing
                end
                return STATUS.NotAllowed
            end
        )
    )
)

--[[
    Wait for all init scripts to finish before trying to do anything.
]]
logger.Info("FUMO: Waiting for system to finish starting.")
event.Subscribe("SystemStart", function()
    --[[
        RDB watcher for user-initiated session.
    ]]
    fumo.db:WatchTrigger("trigger", function (k, v)
        local prev = fumo.db:GetNumber("ui.prev", 0)
        local wait = fumo.db:GetNumber("ui.wait", fumo.UI_DEFAULT_WAIT)
        if dau.GetState() ~= STATE.Idle then
            logger.Warning("FUMO: Ignoring user-initiated session request; update in progress.")
        elseif prev + wait > os.time() then
            logger.Warning("FUMO: Ignoring user-initiated session request; last one was too recent.")
        else
            logger.Notice("FUMO: User-initiated session requested.")
            fumo.DoSession(fumo.USER_REQUEST)
            fumo.db:Set("ui.prev", os.time(), rdb.PERSIST)
        end
    end, true)
    --[[
        RDB watcher for DAU state machine; all DAU actions are dispatched from this point.
    ]]
    dau.db:Watch("state", function (k, v)
        local state = tonumber(v) or STATE.Idle
        if not STATE[state] then
            logger.Error("FUMO: Unknown state '%s', returning to idle.", state)
            dau.Reset()
        elseif state == STATE.Idle then
            logger.Debug("FUMO: Current state is 'Idle'.")
            wap.ProcessMessages()
        else
            logger.Debug("FUMO: Current state is '%s'.", STATE[state])
            local good, err = pcall(function()
                if state == STATE.Downloading then
                    dau.HandleDownload()
                elseif state == STATE.Installing then
                    dau.HandleInstall()
                elseif state == STATE.Rebooting then
                    dau.HandleReboot()
                elseif state == STATE.Reporting then
                    dau.HandleReport()
                end
            end)
            if not good then
                logger.Error(err)
                logger.Error("FUMO: Returning to idle.")
                dau.Reset()
            end
        end
    end, true)
    --[[
        RDB watcher for incoming WAP messages.
    ]]
    rdb.Watch("service.messaging.index", function (k, v)
        if dau.GetState() ~= STATE.Idle then
            logger.Warning("FUMO: Deferring incoming WAP messages; update in progress.")
        else
            wap.ProcessMessages()
        end
    end)
    --[[
        Start DI timer once network time is available.
    ]]
    logger.Info("FUMO: Waiting for network time to become available.")
    event.Subscribe("GotNetworkTime", function()
        logger.Info("FUMO: Got network time, starting DI timer.")
        local target = fumo.db:GetNumber("di.next")
        if not target then
            fumo.ScheduleDI()
        else
            local wait = target - os.time()
            logger.Info("FUMO: Next device-initiated session due at %s, or %s%s seconds%s.",
                os.date("%F %T", target),
                wait < 0 and "" or "in ",
                wait < 0 and -wait or wait,
                wait < 0 and " ago" or "")
        end
        schedule.Event(fumo.DI_CHECK_PERIOD, function()
            fumo.CheckDI()
            return true
        end)
    end)
end)

--[[
    Start DAU firmware update process by moving to the 'Downloading'
    state, if and only if we're not already peforming an update.
]]
function dau.StartDownload(serverid, correlator, url)
    return rdb.Atomic(function()
        logger.Notice("FUMO: Download and update started by server ID '%s'.", serverid)
        if dau.GetState() == STATE.Idle then
            dau.db:Set("serverid", serverid, rdb.PERSIST)
            dau.db:Set("pkgurl", url, rdb.PERSIST)
            if correlator then
                dau.db:Set("correlator", correlator, rdb.PERSIST)
            else
                dau.db:Unset("correlator")
            end
            dau.SetState(STATE.Downloading)
            fumo.SetTreeState(DMSTATE.DownloadInProgress)
            return true
        end
        logger.Error("FUMO: Rejecting firmware update request; update already in progress.")
        return false
    end)
end

--[[
    Move to the 'Installing' state, with a scheduled install time
    12 hours in the future (or 10 seconds for 'critical' updates)
    or on the next reboot (as per AT&T requirements.)
]]
function dau.StartInstall()
    logger.Notice("FUMO: Download complete; starting installation.")
    return rdb.Atomic(function()
        dau.SetState(STATE.Installing)
        fumo.SetTreeState(DMSTATE.UpdateInProgress)
    end)
end

--[[
    Move to the 'Rebooting' state.
]]
function dau.StartReboot()
    local when = 0
    local prio = dau.db:GetNumber("priority") or dau.DEFAULT_PRIORITY
    if prio == PRIORITY.Critical then
        logger.Notice("FUMO: Critical package; rebooting immediately.")
    else
        when = os.time() + fumo.REBOOT_WAIT
        logger.Notice("FUMO: Non-critical package; scheduling reboot for %s, or in %s seconds.",
            os.date("%F %T", when), fumo.REBOOT_WAIT)
    end
    return rdb.Atomic(function()
        dau.db:Set("reboot_target", when)
        dau.db:Set("reboot_pending", 1)
        dau.SetState(STATE.Rebooting)
    end)
end

--[[
    Move to the 'Reporting' state.
]]
function dau.StartReport(result)
    return rdb.Atomic(function()
        dau.db:Set("result", result, rdb.PERSIST)
        dau.SetState(STATE.Reporting)
    end)
end

--[[
    Download state handler; called once after transitioning to 'Downloading'
    state, or on reboot/startup if still in 'Downloading' state. Will stay
    here until package download has either succeeded or failed, after which
    we transition to either 'Installing' or 'Reporting'.
]]
function dau.HandleDownload()
    fumo.WaitForDCPower(function()
        local pkgurl = dau.db:Get("pkgurl")
        local dstpath = dau.db:Get("dstpath") or dau.DEFAULT_DSTPATH
        local maxsize = dau.db:GetNumber("maxsize") or dau.DEFAULT_MAXSIZE
        if not pkgurl then
            logger.Error("FUMO: No package URL, reporting failure and returning to idle.")
            dau.StartReport(DMRESULT.MalformedURL)
            fumo.SetTreeState(DMSTATE.DownloadFailed)
        else
            -- Clear out possible locations for existing firmware files.
            os.execute("rm /opt/debuginfo/* /opt/cdcs/upload/*.zip /opt/cdcs/upload/*.star " .. dau.DEFAULT_DSTPATH)

            fumo.Download(pkgurl, dstpath, maxsize, function(success, curlCode, httpResp, size, contentType)
                if not success then
                    logger.Error("FUMO: Reporting download failure and returning to idle.")
                    if curlCode == CURL.WriteError then
                        dau.StartReport(DMRESULT.OutOfMemory)
                    else
                        dau.StartReport(DMRESULT.DownloadFailed)
                    end
                    fumo.SetTreeState(DMSTATE.DownloadFailed)
                else
                    if contentType == fumo.DESCRIPTOR_CT then
                        dau.HandleDescriptor(dstpath, maxsize)
                    else
                        dau.StartInstall()
                    end
                end
            end)
        end
    end)
end

--[[
    Not a real state; called by HandleDownload to process a download
    descriptor. If successful it will restart the 'Downloading' state
    with the new package URL taken from the descriptor, otherwise it
    will transition to 'Reporting'.
]]
function dau.HandleDescriptor(path, maxsize)
    local success, url, size, priority = fumo.ParseDescriptor(path)
    if not success then
        logger.Error("FUMO: Failed to process descriptor, reporting failure and returning to idle.")
        dau.StartReport(DMRESULT.PackageCorrupt)
        fumo.SetTreeState(DMSTATE.DownloadFailed)
    else
        logger.Info("FUMO: Got download descriptor.")
        if size and size > maxsize then
            logger.Error("FUMO: Firmware image too large, reporting failure and returning to idle.")
            dau.StartReport(DMRESULT.OutOfMemory)
            fumo.SetTreeState(DMSTATE.DownloadFailed)
        else
            rdb.Atomic(function()
                logger.Info("FUMO: Restarting download with URL from descriptor.")
                dau.db:Set("pkgurl", url, rdb.PERSIST)
                dau.db:Set("priority", priority)
                dau.SetState(STATE.Downloading)
            end)
        end
    end
end

--[[
    Install state handler; called once after transitioning
    to 'Installing' state, or on reboot/startup if still
    in 'Installing' state. Attempts to run flashtool, after
    which we transition to either 'Rebooting' or 'Reporting'.
]]
function dau.HandleInstall()
    fumo.WaitForDCPower(function()
        local dstpath = dau.db:Get("dstpath") or dau.DEFAULT_DSTPATH
        fumo.Install(dstpath, function(success, code, signal)
            if not success then
                logger.Error("FUMO: Update failed, reporting and returning to idle.")
                if code == 2 then
                    dau.StartReport(DMRESULT.ValidationFailed)
                elseif code == 3 then
                    dau.StartReport(DMRESULT.PackageCorrupt)
                else
                    dau.StartReport(DMRESULT.InstallFailed)
                end
                fumo.SetTreeState(DMSTATE.UpdateFailed)
            else
                logger.Notice("FUMO: Update complete, awaiting reboot.")
                dau.StartReboot()
            end
        end)
    end)
end

--[[
    Reboot state handler; called once after transitioning
    to 'Rebooting' state, or on reboot/startup if still
    in 'Rebooting' state. Waits for scheduled reboot time,
    then triggers a reboot once all active voice calls have
    terminated. Transitions to 'Reporting' after reboot.
]]
function dau.HandleReboot()
    if dau.db:Get("reboot_pending") then
        logger.Info("FUMO: Waiting for scheduled reboot time.")
        schedule.Event(fumo.REBOOT_CHECK_PERIOD, function()
            if dau.db:GetNumber("reboot_target", 0) > os.time() then
                return true
            end
            logger.Info("FUMO: Reboot time reached.")
            fumo.WaitForVoiceCalls(function()
                fumo.WaitForDCPower(function()
                    logger.Notice("FUMO: Rebooting.")
                    rdb.Set("service.system.upgrade", "1")
                    -- Lastupdate can be set to old time due to race condition after
                    -- rebooting when dau.HandleReboot() is called before system time
                    -- is set to correct time so set lastupdate time here.
                    rdb.Set("service.dm.lastupdate", os.time(), rdb.PERSIST)
                end)
            end)
        end)
    else
        logger.Info("FUMO: Firmware update succeeded, reporting success to server.")
        dau.StartReport(DMRESULT.Success)
        fumo.SetTreeState(DMSTATE.UpdateComplete)
    end
end

--[[
    Terminal DAU state; attempts to report result to DM server
    and then returns to 'Idle', regardless of the outcome.
]]
function dau.HandleReport()
    local result = dau.db:GetNumber("result")
    local serverid = dau.db:Get("serverid")
    local correlator = dau.db:Get("correlator")
    fumo.ReportResult(serverid, correlator, result, function(success)
        logger.Log(success and logger.INFO or logger.ERROR, "FUMO: Returning to idle state.")
        -- If firmware upgrade succeeded and reported successfully.
        if success and result == DMRESULT.Success then
            dm.Execute("flashtool --sync-register")
        end
        dau.Reset()
    end)
end

--[[
    Set DAU machine state.
]]
function dau.SetState(state)
    logger.Debug("FUMO: Changing state from '%s' to '%s'.",
        STATE[dau.GetState()] or "Unknown",
        STATE[state] or "Unknown")
    dau.db:Set("state", state, rdb.PERSIST)
end

--[[
    Get current DAU machine state.
]]
function dau.GetState()
    return dau.db:GetNumber("state") or STATE.Idle
end

--[[
    Reset DAU state machine to Idle.
]]
function dau.Reset()
    rdb.Atomic(function()
        dau.db:Unset("correlator")
        dau.db:Unset("pkgurl")
        dau.db:Unset("serverid")
        dau.db:Unset("priority")
        dau.db:Unset("result")
        dau.db:Unset("reboot_target")
        dau.db:Unset("reboot_pending")
        dau.SetState(STATE.Idle)
    end)
end

--[[
    Run callback when there are no voice active calls.
]]
function fumo.WaitForVoiceCalls(callback)
    local function notReadyYet()
        local count = rdb.GetNumber("voice_call.current_call_count", 0)
        for index = 1, count do
            local status = rdb.Get("voice_call.call_status." .. index)
            if status and not status:find("end") then
                return true
            end
        end
        return false
    end
    if notReadyYet() then
        logger.Info("FUMO: Waiting for voice calls to terminate before continuing.")
        schedule.Event(10000, function()
            if notReadyYet() then
                return true
            end
            callback()
        end)
    else
        callback()
    end
end

--[[
    Run callback when DC power is connected on platforms
    that have battery support, otherwise run immediately.
]]
function fumo.WaitForDCPower(callback)
    local function notReadyYet()
        local val = rdb.Get("system.battery.dcin_present")
        return val and val ~= "1"
    end
    if notReadyYet() then
        logger.Info("FUMO: Waiting for DC power to be reconnected before continuing.")
        schedule.Event(10000, function()
            if notReadyYet() then
                return true
            end
            callback()
        end)
    else
        callback()
    end
end

--[[
    Return the serverid to use for UI/DI sessions.
]]
function fumo.GetServerID()
    local serverid = fumo.db:Get("serverid")
    if not serverid then
        local acct = accounts.Get(3) or accounts.Get(1)
        if acct then
            return acct:Get("ServerID")
        end
    end
    return serverid
end

--[[
    Perform a device or user-initiated FUMO DM session, with configurable retries.
]]
function fumo.DoSession(type, callback)
    local serverid = fumo.GetServerID()
    local rCount = fumo.DM_RETRY_COUNT
    local alert = {
        type       = type,
        source     = "./FwUpdate/Flash",
        format     = "int",
        data       = 200
    }
    if not serverid then
        logger.Error("FUMO: No server ID found for %s-initiated sessions.",
            type == fumo.USER_REQUEST and "user" or "device")
        if callback then
            callback(false)
        end
    else
        local function try()
            logger.Info("FUMO: Starting %s-initiated session with server ID '%s'.",
                type == fumo.USER_REQUEST and "user" or "device", serverid)
            dm.DoSession(serverid, 1, alert, function (success)
                if not success and rCount > 0 then
                    logger.Error("FUMO: Session failed, retrying in %i seconds.", fumo.DM_RETRY_DELAY)
                    rCount = rCount - 1
                    schedule.Event(fumo.DM_RETRY_DELAY * 1000, try)
                else
                    if not success then
                        logger.Error("FUMO: %s-initiated session failed after %i attempts.",
                            type == fumo.USER_REQUEST and "User" or "Device", fumo.DM_RETRY_COUNT + 1)
                    else
                        logger.Info("FUMO: %s-initiated session complete.",
                            type == fumo.USER_REQUEST and "User" or "Device")
                    end
                    if callback then
                        callback(success)
                    end
                end
            end)
        end
        try()
    end
end

function fumo.ReportResult(serverid, correlator, result, callback)
    local rCount = fumo.DM_RETRY_COUNT
    local alert = {
        type       = "org.openmobilealliance.dm.firmwareupdate.downloadandupdate",
        source     = "./FwUpdate/Flash",
        format     = "int",
        data       = result,
        correlator = correlator
    }
    local function try()
        logger.Info("FUMO: Reporting result '%s' to server ID '%s'.", DMRESULT[result], serverid)
        dm.DoSession(serverid, 1, alert, function (success)
            if not success and rCount > 0 then
                logger.Error("FUMO: Report failed, retrying in %i seconds.", fumo.DM_RETRY_DELAY)
                rCount = rCount - 1
                schedule.Event(fumo.DM_RETRY_DELAY * 1000, try)
            else
                if not success then
                    logger.Error("FUMO: Report failed after %i attempts.", fumo.DM_RETRY_COUNT + 1)
                else
                    logger.Info("FUMO: Report complete.")
                end
                if callback then
                    callback(success)
                end
            end
        end)
    end
    try()
end

--[[
    Check the device-initiated session timer and trigger a
    session if the target time is reached and auto update check is enabled.
]]
local di_inProgress = false
function fumo.CheckDI()
    local now = os.time()
    local target = fumo.db:GetNumber("di.next")
    if not target then
        fumo.ScheduleDI()
    else
        if now > target and not di_inProgress then
            if dau.GetState() ~= STATE.Idle then
                logger.Warning("FUMO: Deferring device-initiated session; update in progress.")
                fumo.ScheduleRetryDI()
            else
                if fumo.db:Get("auto_firmware_check") ~= "1" then
                    logger.Notice("FUMO: Target time reached but auto update check is disabled, reschedule DI time")
                    fumo.ScheduleDI()
                else
                    logger.Notice("FUMO: Target time reached, requesting device-initiated session.")
                    fumo.db:Set("di.prev", now, rdb.PERSIST)
                    di_inProgress = true
                    fumo.DoSession(fumo.DEVICE_REQUEST, function(success)
                        di_inProgress = false
                        if success then
                            fumo.ScheduleDI()
                        else
                            fumo.ScheduleRetryDI()
                        end
                    end)
                end
            end
        end
    end
    return true
end

--[[
    Schedule the next device-initiated session, either:
     - in eight hours, if this is our first activation, or
     - after the specified wait time, or
     - at the next interval of 30 days from the initial activation date.
]]
function fumo.ScheduleDI(wait)
    local now = os.time()
    local initial = rdb.GetNumber("service.dm.initialdate")
    if not initial then
        logger.Notice("FUMO: First run; initial activation date is %s.", os.date("%F %T", now))
        rdb.Set("service.dm.initialdate", now, rdb.PERSIST)
        wait = fumo.DI_WAIT_INITIAL
    end
    if not wait then
        wait = fumo.DI_WAIT_NORMAL - ((now - initial) % fumo.DI_WAIT_NORMAL)
    end
    local target = now + wait
    logger.Info("FUMO: Next device-initiated session due at %s, or in %s seconds.",
        os.date("%F %T", target), wait)
    fumo.db:Set("di.next", target, rdb.PERSIST)
    fumo.db:Unset("di.retries")
end

--[[
    Reschedule the next device-initiated session, either:
     - 24 hours in the future, up to two times, or
     - at the next 30 day interval, if retries fail.
]]
function fumo.ScheduleRetryDI()
    local retries = fumo.db:GetNumber("di.retries", 0)
    if retries < fumo.DI_RETRY_COUNT then
        fumo.ScheduleDI(fumo.DI_WAIT_RETRY)
        fumo.db:Set("di.retries", retries + 1, rdb.PERSIST)
    else
        fumo.ScheduleDI()
    end
end

function fumo.ParseDescriptor(path, maxsize)
    local doc = xml.load(path)
    if not doc then
        logger.Error("FUMO: Failed to parse download descriptor.")
        return false
    end
    logger.TableDebug("Descriptor", doc, 0, true)
    local function check(elem)
        return elem and elem[1] and type(elem[1]) == "string" and elem[1]
    end
    local url = check(doc:find("objectURI"))
    if not url then
        logger.Error("FUMO: Descriptor does not contain a URI.")
        return false
    end
    local size = tonumber(check(doc:find("size")))
    local param = check(doc:find("installParam"))
    if param and param:find("priorityflag=critical") then
        return true, url, size, PRIORITY.Critical
    end
    return true, url, size, PRIORITY.Normal
end

function fumo.Download(srcURL, dstPath, maxSize, callback)
    local rCount = fumo.DL_RETRY_COUNT
    local function try()
        if rdb.Get("link.profile.1.status") ~= "up" then
            logger.Info("FUMO: data link is down, retrying download in %i seconds.", fumo.DL_RETRY_DELAY)
            schedule.Event(fumo.DL_RETRY_DELAY * 1000, try)
            return
        end
        logger.Info("FUMO: Downloading '%s' to '%s'.", srcURL, dstPath)
        dm.Download(srcURL, dstPath, maxSize, function (success, curlCode, httpResp, size, contentType)
            if not success and curlCode == CURL.WriteError then
                logger.Error("FUMO: Download failed, no space left on device.")
                if callback then
                    callback(success, curlCode, httpResp)
                end
            elseif not success and rCount > 0 then
                logger.Error("FUMO: Download failed, retrying in %i seconds.", fumo.DL_RETRY_DELAY)
                rCount = rCount - 1
                schedule.Event(fumo.DL_RETRY_DELAY * 1000, try)
            else
                if not success then
                    logger.Error("FUMO: Download failed after %i attempts.", fumo.DL_RETRY_COUNT + 1)
                else
                    logger.Info("FUMO: Download complete.")
                end
                if callback then
                    callback(success, curlCode, httpResp, size, contentType)
                end
            end
        end)
    end
    try()
end

function fumo.Install(path, callback)
    dm.Execute("flashtool " .. path, function (success, code, signal)
        if not success then
            logger.Error("FUMO: Flashtool %s %s.",
                code == 0 and "received signal" or "exited with code",
                code == 0 and signal or code)
        else
            logger.Info("FUMO: Flashtool exited successfully.")
        end
        if callback then
            callback(success, code, signal)
        end
    end)
end

function fumo.SetTreeState(state)
    logger.Debug("FUMO: Changing DM tree state from '%s' to '%s'.",
        DMSTATE[fumo.db:GetNumber("state", DMSTATE.Idle)] or "Unknown",
        DMSTATE[state] or "Unknown")
    fumo.db:Set("state", state, rdb.PERSIST)
end
