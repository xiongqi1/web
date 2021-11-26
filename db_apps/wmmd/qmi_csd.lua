-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2019 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi CSD (Core Sound Driver) module,

local QmiCsd = require("wmmd.Class"):new()
local lqmi = require("luaqmi")
local m = lqmi.m
local bit = require("bit")
local watcher = require("wmmd.watcher")
local smachine = require("wmmd.smachine")
local ffi = require("ffi")
local ts = require("turbo.socket_ffi")
local os = require("os")

-- use QMI default timeout (QMI default is 3000 ms second, defined by luaqmi_default_timeout in luaqmi.lua)
local luaqmi_csd_timeout_msec = nil

local l = require("luasyslog")
pcall(function() l.open("qmi_csd", "LOG_DAEMON") end)

-- tty mode names for CSD
local tty_mode_names={
  ["FULL"] = 3,
  ["VCO"] = 2,
  ["HCO"] = 1,
  ["OFF"] = 0,
}

-- ACDB device IDs
local acdb_device_ids_by_device_name={
  ["DEVICE_HANDSET_RX_ACDB_ID"]=7,
  ["DEVICE_HANDSET_TX_ACDB_ID"]=4,
  ["DEVICE_TTY_HEADSET_MONO_RX_ACDB_ID"]=17,
  ["DEVICE_TTY_HEADSET_MONO_TX_ACDB_ID"]=16,
  ["DEVICE_TTY_VCO_HANDSET_TX_ACDB_ID"]=36,
  ["DEVICE_TTY_HCO_HANDSET_RX_ACDB_ID"]=37,
}

local acdb_device_names_by_tty_mode={
  ["OFF"] = {
    device_id_rx = "DEVICE_HANDSET_RX_ACDB_ID",
    device_id_tx = "DEVICE_HANDSET_TX_ACDB_ID"
  },

  ["HCO"] = {
    device_id_rx = "DEVICE_TTY_HCO_HANDSET_RX_ACDB_ID",
    device_id_tx = "DEVICE_TTY_HEADSET_MONO_TX_ACDB_ID"
  },

  ["VCO"] = {
    device_id_rx = "DEVICE_TTY_HEADSET_MONO_RX_ACDB_ID",
    device_id_tx = "DEVICE_TTY_VCO_HANDSET_TX_ACDB_ID"
  },


  ["FULL"] = {
    device_id_rx = "DEVICE_TTY_HEADSET_MONO_RX_ACDB_ID",
    device_id_tx = "DEVICE_TTY_HEADSET_MONO_TX_ACDB_ID"
  },

}

-- device Rx sampling rate
local device_rx_sr = 8000
-- device Tx sampling rate
local device_tx_sr = 8000
-- VOICE_MULTIMODE1_VSID_STR session name for ALSA UCM "Voice Call Multimode1"
-- from apps_proc/mm-audio/audio-qmi/csd-server/inc/csd_alsa.h
local session_name = "11C05000"


-- current tty mode
local current_csd_tty_mode = "OFF"

-----------------------------------------------------------------------------
-- setup
--
-- This function sets up QMI CSD
-----------------------------------------------------------------------------
function QmiCsd:setup(rdbWatch, wrdb, dConfig)
end

-----------------------------------------------------------------------------
-- poll
--
-- This function performs CSD poll
-----------------------------------------------------------------------------
function QmiCsd:poll(type, event, a)
  l.log("LOG_DEBUG","qmi csd poll")

  return true
end

-----------------------------------------------------------------------------
-- init_csd
--
-- This function initates CSD service
-----------------------------------------------------------------------------
function QmiCsd:init_csd()
  return lqmi.req(m.QMI_CSD_INIT,nil,luaqmi_csd_timeout_msec)
end

-----------------------------------------------------------------------------
-- get_csd_device_entry
--
-- This function returns CSD device entry
-----------------------------------------------------------------------------
function QmiCsd:get_csd_device_entry(device_id)
  local csd_device_entry = ffi.new("qmi_csd_dev_entry_v01")

  csd_device_entry.dev_id = acdb_device_ids_by_device_name[device_id]
  csd_device_entry.dev_attrib.sample_rate="QMI_CSD_DEV_SR_8000_V01"
  csd_device_entry.dev_attrib.bits_per_sample="QMI_CSD_DEV_BPS_16_V01"

  return csd_device_entry
end

function QmiCsd:open_handle_prepare(msg)
  return lqmi.new_msg(m[msg])
end

-----------------------------------------------------------------------------
-- open_handle
--
-- This function opens CSD handle
-----------------------------------------------------------------------------
function QmiCsd:open_handle(msg,qm,handle)

  -- send qmi message
  local succ = lqmi.send_msg(qm,luaqmi_csd_timeout_msec)

  -- check return status
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] %s failed",msg))
    return false
  end

  local resp = qm.resp

  -- get csd status
  local open_status_valid_succ,open_status_valid,open_status=pcall(function() return resp["open_status_valid"], resp["open_status"] end)
  if not open_status_valid_succ then
    open_status_valid_succ,open_status_valid,open_status=pcall(function() return resp["qmi_csd_status_code_valid"], resp["qmi_csd_status_code"] end)
  end

  -- check csd status
  if open_status_valid_succ and lqmi.is_c_true(open_status_valid) and  open_status ~= "QMI_CSD_EOK_V01" then
    l.log("LOG_ERR",string.format("[CSD] CSD error status from %s (open_status=%d)",msg,tonumber(open_status) or -1))
    return false
  end

  -- check handle valid flag
  if not lqmi.is_c_true(resp[handle .. "_valid"]) then
    l.log("LOG_ERR",string.format("[CSD] VC handle not valid from %s",msg))
    return false
  end

  return true,resp[handle]
end

-----------------------------------------------------------------------------
-- close_handle
--
-- This function closes CSD handle
-----------------------------------------------------------------------------
function QmiCsd:close_handle(handle)
  -- open voice stream
  local succ,retcode,resp=lqmi.req(m.QMI_CSD_CLOSE,{handle=handle},luaqmi_csd_timeout_msec)
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] QMI_CSD_CLOSE failed"))
    return false
  end


  if lqmi.is_c_true(resp.qmi_csd_status_code_valid) and (resp.qmi_csd_status_code ~= "QMI_CSD_EOK_V01") then
    l.log("LOG_ERR",string.format("[CSD] CSD error status from QMI_CSD_CLOSE (qmi_csd_status_code=%d)",tonumber(resp.qmi_csd_status_code)))
    return false
  end

  return true
end

-----------------------------------------------------------------------------
-- enable_csd_device
--
-- This function enables or disables CSD device
-----------------------------------------------------------------------------
function QmiCsd:enable_csd_device(enable)

  if not self.qmi_csd_device_handle then
    l.log("LOG_ERR","[CSD] CSD device handle is not available to enable CSD device")
    return false
  end

  -- enable device control
  local qm = lqmi.new_msg(enable and m.QMI_CSD_IOCTL_DEV_CMD_ENABLE or m.QMI_CSD_IOCTL_DEV_CMD_DISABLE)
  qm.req.handle=self.qmi_csd_device_handle

  local payload

  local device_id_rx = acdb_device_names_by_tty_mode[current_csd_tty_mode].device_id_rx
  local device_id_tx = acdb_device_names_by_tty_mode[current_csd_tty_mode].device_id_tx

  local device_id_rx_index = acdb_device_ids_by_device_name[device_id_rx]
  local device_id_tx_index = acdb_device_ids_by_device_name[device_id_tx]

  l.log("LOG_DEBUG",string.format("[CSD] %s csd devices (device_id_rx=%s #%d,device_id_tx=%s #%d)",enable and "enable" or "disable",device_id_rx,device_id_rx_index,device_id_tx,device_id_tx_index))

  if enable then
    payload = ffi.new("qmi_csd_dev_enable_v01")

    payload.devs_len=2
    payload.devs[0]=self:get_csd_device_entry(device_id_rx)
    payload.devs[1]=self:get_csd_device_entry(device_id_tx)

    qm.req.qmi_csd_dev_enable_cmd_payload = payload
  else
    payload = ffi.new("qmi_csd_dev_disable_v01")

    payload.dev_ids_len=2
    payload.dev_ids[0]=acdb_device_ids_by_device_name[device_id_rx]
    payload.dev_ids[1]=acdb_device_ids_by_device_name[device_id_tx]

    qm.req.qmi_csd_dev_disable_cmd_payload = payload
  end

  return lqmi.send_msg(qm,luaqmi_csd_timeout_msec)

end

-----------------------------------------------------------------------------
-- open_csd_device
--
-- This function opens CSD device
-----------------------------------------------------------------------------
function QmiCsd:open_csd_device()

  -- open device control
  local msg = "QMI_CSD_OPEN_DEVICE_CONTROL"
  local qm = self:open_handle_prepare(msg)

  local succ,handle = self:open_handle(msg,qm,"qmi_csd_device_handle")
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] failed to open csd device"))
    return false
  end

  -- store csd_handle
  self.qmi_csd_device_handle=qm.resp.qmi_csd_device_handle
  l.log("LOG_DEBUG",string.format("[CSD] got csd device handle (handle=%d)",self.qmi_csd_device_handle))


  if not self:enable_csd_device(true) then
    l.log("LOG_ERR",string.format("[CSD] failed to enable csd device"))
    return false
  end

  return true
end

-----------------------------------------------------------------------------
-- close_csd_device
--
-- This function closes CSD device
-----------------------------------------------------------------------------
function QmiCsd:close_csd_device()

  if not self.qmi_csd_device_handle then
    l.log("LOG_ERR","[CSD] CSD device handle is not available to close  CSD device")
    return false
  end

  if not self:enable_csd_device(false) then
    l.log("LOG_ERR",string.format("[CSD] failed to disable csd device"))
  end

  l.log("LOG_DEBUG",string.format("[CSD] close csd device"))
  succ = self:close_handle(self.qmi_csd_device_handle)
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] failed to close device"))
  end

  return succ
end

-----------------------------------------------------------------------------
-- open_voice_stream
--
-- This function opens Voice stream
-----------------------------------------------------------------------------
function QmiCsd:open_voice_stream()

  -- open voice stream
  local msg = "QMI_CSD_OPEN_PASSIVE_CONTROL_VOICE_STREAM"
  local qm = self:open_handle_prepare(msg)
  qm.req.session_name = session_name


  local succ,handle = self:open_handle(msg,qm,"qmi_csd_vs_passive_control_handle")
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] failed to open voice stream"))
    return false
  end

  -- store voice stream handle
  self.qmi_csd_vs_passive_control_handle = handle

  return true
end

-----------------------------------------------------------------------------
-- close_voice_stream
--
-- This function closes Voice stream
-----------------------------------------------------------------------------
function QmiCsd:close_voice_stream()
  l.log("LOG_DEBUG",string.format("[CSD] close voice stream"))
  if self.qmi_csd_vs_passive_control_handle and not self:close_handle(self.qmi_csd_vs_passive_control_handle) then
    l.log("LOG_ERR",string.format("[CSD] failed to close voice context"))
  end

  self.qmi_csd_vs_passive_control_handle = nil
end

-----------------------------------------------------------------------------
-- open_voice_context
--
-- This function opens Voice context
-----------------------------------------------------------------------------
function QmiCsd:open_voice_context()

  -- open voice context
  local msg = "QMI_CSD_OPEN_VOICE_CONTEXT"
  local qm = self:open_handle_prepare(msg)
  qm.req.qmi_csd_vc_open_payload.session_name = session_name
  qm.req.qmi_csd_vc_open_payload.direction = "QMI_CSD_VC_DIRECTION_TX_AND_RX_V01"
  qm.req.qmi_csd_vc_open_payload.network_id = "QMI_CSD_NETWORK_ID_DEFAULT_V01"

  local succ,handle = self:open_handle(msg,qm,"qmi_csd_vc_handle")
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] failed to open voice context"))
    return false
  end

  -- store voice stream handle
  self.qmi_csd_vc_handle = handle

  return true
end

-----------------------------------------------------------------------------
-- close_voice_context
--
-- This function closes Voice manager
-----------------------------------------------------------------------------
function QmiCsd:close_voice_context()
  l.log("LOG_DEBUG",string.format("[CSD] close voice context"))
  if self.qmi_csd_vc_handle and not self:close_handle(self.qmi_csd_vc_handle) then
    l.log("LOG_ERR",string.format("[CSD] failed to close voice context"))
  end

  self.qmi_csd_vc_handle = nil
end

-----------------------------------------------------------------------------
-- open_voice_manager
--
-- This function opens Voice manager
-----------------------------------------------------------------------------
function QmiCsd:open_voice_manager()

  -- open voice manager
  local msg = "QMI_CSD_OPEN_VOICE_MANAGER"
  local qm = lqmi.new_msg(m.QMI_CSD_OPEN_VOICE_MANAGER)
  qm.req.qmi_csd_vm_open_payload.session_name=session_name

  local succ,handle = self:open_handle(msg,qm,"qmi_csd_vm_handle")
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] failed to open voice manager"))
    return false
  end

  -- store voice manager handle
  self.qmi_csd_vm_handle = qm.resp.qmi_csd_vm_handle

  return true
end

-----------------------------------------------------------------------------
-- close_voice_manager
--
-- This function disables Voice context
-----------------------------------------------------------------------------
function QmiCsd:close_voice_manager()
  l.log("LOG_DEBUG",string.format("[CSD] close voice manager"))
  if self.qmi_csd_vm_handle and not self:close_handle(self.qmi_csd_vm_handle) then
    l.log("LOG_ERR",string.format("[CSD] failed to close voice manager"))
  end

  self.qmi_csd_vm_handle = nil
end

-----------------------------------------------------------------------------
-- enable_voice_context
--
-- This function enables Voice context
-----------------------------------------------------------------------------
function QmiCsd:enable_voice_context()

  if not self.qmi_csd_vc_handle then
    l.log("LOG_ERR","[CSD] CSD device handle is not available to enable voice context")
    return false
  end

  local device_id_rx = acdb_device_names_by_tty_mode[current_csd_tty_mode].device_id_rx
  local device_id_tx = acdb_device_names_by_tty_mode[current_csd_tty_mode].device_id_tx

  l.log("LOG_DEBUG",string.format("[CSD] enable_voice_context (device_id_rx=%s,device_id_tx=%s)",device_id_rx,device_id_tx))

  -- set device config
  local qm = lqmi.new_msg(m.QMI_CSD_IOCTL_VC_CMD_SET_DEVICE_CONFIG)
  qm.req.handle=self.qmi_csd_vc_handle
  qm.req.qmi_csd_vc_ioctl_set_device_config_payload.cmd_token=1
  qm.req.qmi_csd_vc_ioctl_set_device_config_payload.rx_dev_num=acdb_device_ids_by_device_name[device_id_rx]
  qm.req.qmi_csd_vc_ioctl_set_device_config_payload.tx_dev_num=acdb_device_ids_by_device_name[device_id_tx]
  qm.req.qmi_csd_vc_ioctl_set_device_config_payload.rx_dev_sr=device_rx_sr
  qm.req.qmi_csd_vc_ioctl_set_device_config_payload.tx_dev_sr=device_tx_sr
  local succ = lqmi.send_msg(qm,luaqmi_csd_timeout_msec)


  -- open voice stream
  local succ,retcode,resp=lqmi.req(m.QMI_CSD_IOCTL_VC_CMD_ENABLE,{handle=self.qmi_csd_vc_handle,cmd_token=1},luaqmi_csd_timeout_msec)
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] QMI_CSD_IOCTL_VC_CMD_ENABLE_REQ failed"))
    return false
  end


  if lqmi.is_c_true(resp.qmi_csd_status_code_valid) and (resp.qmi_csd_status_code ~= "QMI_CSD_EOK_V01") then
    l.log("LOG_ERR",string.format("[CSD] CSD error status from QMI_CSD_IOCTL_VC_CMD_ENABLE_REQ (qmi_csd_status_code=%d)",tonumber(resp.qmi_csd_status_code)))
    return false
  end

  return true
end

-----------------------------------------------------------------------------
-- attach_voice_context_to_voice_manager
--
-- This function attaches Voice context to Voice manager
-----------------------------------------------------------------------------
function QmiCsd:attach_voice_context_to_voice_manager()
  if not self.qmi_csd_vm_handle then
    l.log("LOG_ERR","[CSD] CSD VM handle is not available to attach VC to VM")
    return false
  end

  if not self.qmi_csd_vc_handle then
    l.log("LOG_ERR","[CSD] CSD VC handle is not available to attach VC to VM")
    return false
  end

  local qm = lqmi.new_msg(m.QMI_CSD_IOCTL_VM_CMD_ATTACH_CONTEXT)
  qm.req.handle=self.qmi_csd_vm_handle
  qm.req.qmi_csd_vm_ioctl_attach_context_payload.cmd_token=1
  qm.req.qmi_csd_vm_ioctl_attach_context_payload.context_handle=self.qmi_csd_vc_handle
  local succ = lqmi.send_msg(qm,luaqmi_csd_timeout_msec)

  if not succ then
    l.log("LOG_ERR",string.format("[CSD] QMI_CSD_IOCTL_VM_CMD_ATTACH_CONTEXT failed"))
    return false
  end

  local resp = qm.resp

  if lqmi.is_c_true(resp.qmi_csd_status_code_valid) and (resp.qmi_csd_status_code ~= "QMI_CSD_EOK_V01") then
    l.log("LOG_ERR",string.format("[CSD] CSD error status from QMI_CSD_IOCTL_VM_CMD_ATTACH_CONTEXT (qmi_csd_status_code=%d)",tonumber(resp.qmi_csd_status_code)))
    return false
  end

  return true
end

-----------------------------------------------------------------------------
-- detach_voice_context_from_voice_manager
--
-- This function detaches Voice context from Voice manager
-----------------------------------------------------------------------------
function QmiCsd:detach_voice_context_from_voice_manager()

  if not self.qmi_csd_vm_handle then
    l.log("LOG_ERR","[CSD] CSD VM handle is not available for QMI_CSD_IOCTL_VM_CMD_DETACH_CONTEXT")
    return false
  end

  if not self.qmi_csd_vc_handle then
    l.log("LOG_ERR","[CSD] CSD VC handle is not available to detach  VC to VM")
    return false
  end

  local qm = lqmi.new_msg(m.QMI_CSD_IOCTL_VM_CMD_DETACH_CONTEXT)
  qm.req.handle=self.qmi_csd_vm_handle
  qm.req.qmi_csd_vm_ioctl_detach_context_payload.cmd_token=1
  qm.req.qmi_csd_vm_ioctl_detach_context_payload.context_handle=self.qmi_csd_vc_handle
  local succ = lqmi.send_msg(qm,luaqmi_csd_timeout_msec)

  if not succ then
    l.log("LOG_ERR",string.format("[CSD] QMI_CSD_IOCTL_VM_CMD_DETACH_CONTEXT failed"))
    return false
  end

  local resp = qm.resp

  if lqmi.is_c_true(resp.qmi_csd_status_code_valid) and (resp.qmi_csd_status_code ~= "QMI_CSD_EOK_V01") then
    l.log("LOG_ERR",string.format("[CSD] CSD error status from QMI_CSD_IOCTL_VM_CMD_DETACH_CONTEXT (qmi_csd_status_code=%d)",tonumber(resp.qmi_csd_status_code)))
    return false
  end

  return true
end

-----------------------------------------------------------------------------
-- start_voice
--
-- This function starts CSD voice call
-----------------------------------------------------------------------------
function QmiCsd:start_voice()
  if not self.qmi_csd_vm_handle then
    l.log("LOG_ERR","[CSD] CSD VM handle is not available to start voice")
    return false
  end

  -- start voice stream
  local succ,retcode,resp=lqmi.req(m.QMI_CSD_IOCTL_VM_CMD_START_VOICE,{handle=self.qmi_csd_vm_handle,cmd_token=1},luaqmi_csd_timeout_msec)
  if not succ then
    l.log("LOG_ERR","[CSD] QMI_CSD_IOCTL_VM_CMD_START_VOICE failed")
    return false
  end

  if lqmi.is_c_true(resp.qmi_csd_status_code_valid) and resp.qmi_csd_status_code ~= "QMI_CSD_EOK_V01" then
    l.log("LOG_ERR",string.format("[CSD] CSD error status from QMI_CSD_IOCTL_VM_CMD_START_VOICE (qmi_csd_status_code=%d)",tonumber(resp.qmi_csd_status_code)))
    return false
  end

  return true
end

-----------------------------------------------------------------------------
-- stop_voice
--
-- This function stops CSD voice call
-----------------------------------------------------------------------------
function QmiCsd:stop_voice()

  if not self.qmi_csd_vm_handle then
    l.log("LOG_ERR","[CSD] CSD VM handle is not available for QMI_CSD_IOCTL_VM_CMD_STOP_VOICE")
    return false
  end

  -- stop voice stream
  local succ,retcode,resp=lqmi.req(m.QMI_CSD_IOCTL_VM_CMD_STOP_VOICE,{handle=self.qmi_csd_vm_handle,cmd_token=1},luaqmi_csd_timeout_msec)
  if not succ then
    l.log("LOG_ERR",string.format("[CSD] QMI_CSD_IOCTL_VM_CMD_STOP_VOICE failed"))
    return false
  end

  if lqmi.is_c_true(resp.qmi_csd_status_code_valid) and resp.qmi_csd_status_code ~= "QMI_CSD_EOK_V01" then
    l.log("LOG_ERR",string.format("[CSD] CSD error status from QMI_CSD_IOCTL_VM_CMD_STOP_VOICE (qmi_csd_status_code=%d)",tonumber(resp.qmi_csd_status_code)))
    return false
  end

  return true
end

-----------------------------------------------------------------------------
-- init_voice_call
--
-- This function initiates voice call
-----------------------------------------------------------------------------
function QmiCsd:init_voice_call()

  --[[

  This voice call initiation procedure follows Qualcomm Core Sound Driver API Interface Specification
  (ref. "3.12 Voice Startup" in Core Sound Driver API Interface Specification [80-N4404-1 B])

  1. Obtain a sound device CSD handle (i.e., a handset microphone and handset speaker) and enable it.
  2. Obtain handles to the CSD VS, VC, and VM.
  3. Configure and enable the voice context with the device pair in step 1.
  4. Attach the voice context to the VM obtained in step 2.
  5. Enable the side tone and start voice on the VM.

]]--

  if self.csd_initiated_flag then
    l.log("LOG_DEBUG","[CSD] CSD is already initiated. Skip CSD initiation")
    return true
  end

  l.log("LOG_DEBUG",string.format("[CSD] start CSD initiation (tty_mode=%s,csd_initiated_flag=%s)",current_csd_tty_mode,self.csd_initiated_flag))

  if not self:open_csd_device() then
    return false
  end

  if not self:open_voice_stream() then
    return false
  end

  if not self:open_voice_context() then
    return false
  end

  if not self:open_voice_manager() then
    return false
  end

  if not self:set_vm_tty_mode(current_csd_tty_mode) then
    return false
  end

  if not self:enable_voice_context() then
    return false
  end

  if not self:attach_voice_context_to_voice_manager() then
    return false
  end

  if not self:start_voice() then
    return false
  end

  -- set csd initiated flag
  self.csd_initiated_flag = true

  return true
end

-----------------------------------------------------------------------------
-- fini_voice_call
--
-- This function finalises voice call
-----------------------------------------------------------------------------
function QmiCsd:fini_voice_call(type,event,a)

  --[[

  This voice call initiation procedure follows Qualcomm Core Sound Driver API Interface Specification
  (ref. "3.13 Voice Stop" in Core Sound Driver API Interface Specification [80-N4404-1 B])

  1. Disable the side tone and stop voice on the VM.
  2. Detach the voice context from the VM.
  3. Close the voice stream, voice context, and VM.
  4. Disable and close the sound device.

]]--

  local force_to_fini = a and a.force_to_fini

  -- force to fini
  if force_to_fini then
    l.log("LOG_DEBUG","[CSD] force to finalise CSD")

    -- do not fini CSD and keep CSD initiated unless TTY mode is enabled
  elseif current_csd_tty_mode == "OFF" then
    l.log("LOG_DEBUG",string.format("[CSD] non-TTY mode is detected. Skip CSD finalisation (tty_mode=%s,csd_initiated_flag=%s)",current_csd_tty_mode,self.csd_initiated_flag))
    return true
  end

  l.log("LOG_DEBUG",string.format("[CSD] start CSD finalisation (tty_mode=%s,csd_initiated_flag=%s)",current_csd_tty_mode,self.csd_initiated_flag))

  self:stop_voice()
  self:detach_voice_context_from_voice_manager()

  self:close_voice_manager()
  self:close_voice_context()
  self:close_voice_stream()

  self:close_csd_device()

  -- clear csd initiated flag
  self.csd_initiated_flag = false

  return true
end

-----------------------------------------------------------------------------
-- set_vm_tty_mode
--
-- This function sets VM tty mode
-----------------------------------------------------------------------------
function QmiCsd:set_vm_tty_mode(tty_mode)
  l.log("LOG_DEBUG",string.format("[CSD] set tty mode (tty_mode=%s)",tty_mode))

  if not self.qmi_csd_vm_handle then
    l.log("LOG_ERR","[CSD] CSD VM handle is not available for QMI_CSD_IOCTL_VM_CMD_SET_TTY_MODE")
    return false
  end

  local qm = lqmi.new_msg(m.QMI_CSD_IOCTL_VM_CMD_SET_TTY_MODE)

  qm.req.handle = self.qmi_csd_vm_handle
  qm.req.qmi_csd_vm_ioctl_set_tty_mode_payload.cmd_token = 1
  qm.req.qmi_csd_vm_ioctl_set_tty_mode_payload.mode= tty_mode_names[tty_mode]

  return lqmi.send_msg(qm,luaqmi_csd_timeout_msec)
end

-----------------------------------------------------------------------------
-- set_csd_tty_mode
--
-- This function sets CSD tty mode
-----------------------------------------------------------------------------
function QmiCsd:set_csd_tty_mode(type,event,a)
  l.log("LOG_DEBUG",string.format("[CSD] update tty mode (tty_mode=%s)",a.tty_mode))

  current_csd_tty_mode=a.tty_mode

  l.log("LOG_DEBUG",string.format("update amix tty mode (tty_mode=%s)",current_csd_tty_mode or "OFF"))
  os.execute(string.format("set_tty_mode.sh '%s'",current_csd_tty_mode))

  return true
end


-- QMI callback functions
QmiCsd.cbs={
  }


-- SYS callback functions
QmiCsd.cbs_system={
  "poll",
  "init_voice_call",
  "fini_voice_call",
  "set_csd_tty_mode",
}

-----------------------------------------------------------------------------
-- init
--
-- This function initiates QMI CSD
-----------------------------------------------------------------------------
function QmiCsd:init()

  l.log("LOG_INFO", "initiate qmi_csd")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    watcher.add("sys", v, self, v)
  end

  -- initiate qmi
  self:init_csd()
end

return QmiCsd
