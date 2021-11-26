-- Copyright (C) 2015 NetComm Wireless limited.
--

--! \file
--! \brief QMI thunk module that talks to libqmi.c via FFI
--!
--!<pre>
--!  * qis structure - Qmi Information Structure
--!  qis[sname] = qi
--!  qi[lua_qmi_id] = qie {
--!    sname,
--!    qmi_lua_sid,
--!    qc,
--!    ver,
--!    qmsg_in_process={},
--!  }
--!
--!  * m structure - qmi C Message structure
--!  m[qmi_msg_str] = me {
--!    name, -- qmi_msg_str
--!    sname,
--!    mname,
--!    msg_id
--!  }
--!
--!  * r structure - qmi message Reverse structure
--!  r[sname][msg_id] = me
--!
--!  * qm structure - Qmi Message structure
--!  qm == {
--!    me,
--!    qie,
--!    c_struct_req,
--!    c_struct_resp,
--!    req,
--!    req_len,
--!    resp,
--!    resp_len,
--!  }
--!</pre>

local modname = ...

-- init syslog
local l = require("luasyslog")
pcall(function() l.open("luaqmi", "LOG_DAEMON") end)

-- lua modules
local ffi = require("ffi")
local config = require("wmmd.config")
local dconfig = require("wmmd.dconfig")
local rdb = require("luardb")
-- turbo and its instance
local t = require("turbo")
local i = t.ioloop.instance()

-- ffi modules
local libluaqmi
local libqmiservices={}
local libqmicsd
local libqmi_cci
local libqmi_client_qmux

-- constant variables
local luaqmi_header_file = "/etc/luaqmi/qmi_call.pch"
local luaqmi_message_file = "/etc/luaqmi/qmi_msg.pch"
local luaqmi_qmi_error_file = "/etc/luaqmi/qmi_err.pch"
local luaqmi_default_timeout = 3000
local luaqmi_recv_buf_size = 64*1024
local luaqmi_service_timeout = 1000
local luaqmi_service_versions = {1,2}
local luaqmi_max_wds = 6
local libqmi_max_array_debug = 16

local qem={}
local e={}

-- local private variables
local qis = {} --! array of qi that is a container of qc and ver
local resp_to_recv --! receive buffer for non-blocking QMI access
local resp_len_to_recv --! receive buffer length for non-blocking QMI access
local servs={}
local c_qmi_sid = 0

watchdog_val = 0

-- public variable
local m={}  --! forward QMI message table
local r={}  --! reverse QMI message table
local im={}
local ir={}
local _q

--! @brief transaction element reference information
local te_ref={}
local te_ref_index = 0
local te_max_count = 1000

--! @brief Match ^QMI_(word1)_(word2) and lowercase
--! @param creq request
--! @return sname word1 lowercased
--! @return mname word2 lowercased
local function qmsg_conv_cdef_to(cdef)

  --[[
        #define QMI_RFRPE_SET_RFM_SCENARIO_REQ_V01 0x0020
        #define QMI_SAR_GET_COMPATIBILITY_KEY_REQ_MSG_V01 0x0020
        #define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_MSG_V01 0x0020
        #define QMI_NAS_GET_SIGNAL_STRENGTH_RESP_MSG_V01 0x0020
]]--

  -- case#1 - QMI_SSS_MMMMM_REQ_MSG_V01
  local sname,mname = cdef:match("^QMI_([%w]+)_([%w_]+)")

  sname = sname and sname:lower()
  mname = mname and mname:lower()

  return sname, mname
end

--! @brief Create string QMI_(sname)_(mname)_REQ and uppercase
--! @param sname
--! @param mname
--! @return string for request
local function qmsg_conv_to_qmireqstr(sname,mname)
  assert(sname and mname)
  return "QMI_" .. sname:upper() .. "_" .. mname .. "_REQ"
end

--! @brief Create string QMI_(sname)_(mname)_RESP and uppercase
--! @param sname
--! @param mname
--! @return string for response
local function qmsg_conv_to_qmirespstr(sname,mname)
  assert(sname and mname)
  return "QMI_" .. sname:upper() .. "_" .. mname .. "_RESP"
end

--! @brief Create HungarianString from underscore_string
--! @param underscore string to HungerianIse
--! @return converted string
local function convert_underscore_to_hungarian(underscore)

  local s = underscore:split("_")
  local l

  for i,v in ipairs(s) do
    l = v:lower()
    s[i] = l:gsub("^%l", string.upper)
  end

  return table.concat(s,"")
end

--! @brief Create request structure name from s/m name pair
--! @param sname s-part
--! @param mname m-part
--! @param ver optional version
--! @return converted string (hungarian with version attached)
local function qmsg_get_req_struct(sname,mname,ver)
  assert(sname and mname)

  if sname == "loc" then
    return string.format("qmi%sReqMsgT_v%02d",convert_underscore_to_hungarian(sname .. "_" .. mname),ver)
  elseif sname == "ssctl" or sname == "csd" then
    return string.format("qmi_%s_%s_req_msg_v%02d",sname,mname,ver)
  end

  return string.format("%s_%s_req_msg_v%02d",sname,mname,ver)
end

--! @brief Create response structure name from s/m name pair
--! @param sname s-part
--! @param mname m-part
--! @param ver optional version
--! @return converted string (hungarian with version attached)
local function qmsg_get_resp_struct(sname,mname,ver)
  assert(sname and mname and ver)

  if sname == "loc" then
    return string.format("qmi%sRespMsgT_v%02d",convert_underscore_to_hungarian(sname .. "_" .. mname),ver)
  elseif sname == "ims" then
    return string.format("%s_%s_rsp_msg_v%02d",sname,mname,ver)
  elseif sname == "imsp" then
    return string.format("%s_%s_resp_v%02d",sname,mname,ver)
  elseif sname == "ssctl" or sname == "csd" then
    return string.format("qmi_%s_%s_resp_msg_v%02d",sname,mname,ver)
  end

  return string.format("%s_%s_resp_msg_v%02d",sname,mname,ver)
end

--! @brief Create ind structure name from s/m name pair
--! @param sname s-part
--! @param mname m-part
--! @param ver optional version
--! @return converted string (hungarian with version attached)
local function qmsg_get_ind_struct(sname,mname,ver)
  assert(sname and mname and ver)

  if sname == "loc" then
    return string.format("qmi%sMsgT_v%02d",convert_underscore_to_hungarian(sname .. "_" .. mname),ver)
  end

  return string.format("%s_%s_msg_v%02d",sname,mname,ver)
end

--! @brief Create service object name from s/m name pair
--! @param sname s-part
--! @param mname m-part
--! @param ver optional version
--! @return converted string (version attached)
local function qmsg_get_service_object(sname,ver)
  assert(sname and ver)

  -- exceptional rule for ims
  if sname == "ims" then
    sname = "imss"
  end

  return string.format("%s_qmi_idl_service_object_v%02d",sname,ver)
end

local function set_g_mtable(func)
  local om = getmetatable(_G)

  setmetatable(_G, {
    __index=function(t,k)
      return func(k)
    end,
  })

  return function() setmetatable(_G,om) end
end

local function qmsg_build_qem()

  local restore_g_table=set_g_mtable(function() return qem end)
  do
    dofile(luaqmi_qmi_error_file)
    restore_g_table()
  end

  -- build error table
  for k,v in pairs(qem) do
    e[v]=k
  end
end

local function qmsg_build()

  l.log("LOG_INFO", "[luaqmi] build QMI message table" )

  local cdef_file = luaqmi_message_file
  -- load cdef
  local lm={}
  local lim={}

  local restore_g_table=set_g_mtable(function(k) return (k == "m") and lm or lim end)
  do
    dofile(cdef_file)
    restore_g_table()
  end

  -- build public res/resp message table
  for k,v in pairs(lm) do
    local sname,mname = qmsg_conv_cdef_to(k)
    local name=k

    local me = {
      ind = false,
      sname = sname,
      mname = mname,
      name = name,
      msg_id=v,
    }

    -- forward table
    m[name] = me

    -- reverse table
    if not r[sname] then
      r[sname]={}
    end
    r[sname][v]=me
  end

  -- build public ind message table
  for k,v in pairs(lim) do
    local sname,mname = qmsg_conv_cdef_to(k)
    local name=k

    local me = {
      ind = true,
      sname = sname,
      mname = mname,
      name = name,
      msg_id=v,
    }

    -- forward table
    im[name] = me

    -- reverse table
    if not ir[sname] then
      ir[sname]={}
    end
    ir[sname][v]=me
  end

  -- set meta table
  setmetatable(m,{
    __index=function(t,k)
      l.log("LOG_ERR", string.format("QMI message not found - %s",k))
    end
  })

  l.log("LOG_DEBUG", "[luaqmi] * enumerate all QMI services from PCH file")
  for k,v in pairs(r) do
    l.log("LOG_DEBUG", "[luaqmi] QMI service - sname=" .. k )
  end
end

local function delete_qmi_service(sname,qmi_lua_sid)
  servs[sname][qmi_lua_sid]=nil
end

global_blacklist = {}

local function create_qmi_service(sname)

  local serv = servs[sname]

  c_qmi_sid=c_qmi_sid+1

  -- create luaqmi (lua-c binding)
  local qc = libluaqmi.luaqmi_new(c_qmi_sid,serv.serv_obj,luaqmi_service_timeout)
  if qc == nil then
    l.log("LOG_ERR", string.format("[luaqmi] QMI service not ready - sname=%s,ver=%d",serv.sname,serv.serv_ver))
    table.insert(global_blacklist, serv.sname)
    return
  end

  ffi.gc(qc,function(qc) libluaqmi.luaqmi_delete(qc) end )

  -- get pipe handle
  local fd = libluaqmi.luaqmi_get_async_fd(qc)
  if fd<=0 then
    l.log("LOG_ERR", string.format("[luaqmi] failed to get pipe handle - sname=%s,ver=%d",serv.sname,serv.serv_ver))
    return
  end

  if not qis[serv.sname] then
    qis[serv.sname]={}
  end

  -- build qie
  local qmi_lua_sid=1+#qis[serv.sname]
  qis[serv.sname][qmi_lua_sid]={
    sname=serv.sname,
    qmi_lua_sid=qmi_lua_sid,
    qc=qc,
    ver=serv.serv_ver,
    fd = fd,
    qmsg_in_process = {},
  }

  l.log("LOG_DEBUG", string.format("[luaqmi] QMI service ready - sname=%s,ver=%d",serv.sname,serv.serv_ver))

  return qc
end

local function init_qmi()

  l.log("LOG_DEBUG", "[luaqmi] * seek QMI services")
  -- initiate services
  for sname,_ in pairs(r) do
    -- search service object and its version
    local serv_obj
    local serv_ver
    local serv_libname
    local serv_prior
    for _,v in ipairs(luaqmi_service_versions) do

      local oname = qmsg_get_service_object(sname,v)
      l.log("LOG_DEBUG", "[luaqmi] seek QMI service - sname=" .. sname  .. ", ver=" .. v .. ", ..oname=" .. oname)

      -- add object to ffi
      ffi.cdef(string.format("extern struct qmi_idl_service_object %s;",oname))

      for libname,qmiservice in pairs(libqmiservices) do
        local succ,res = pcall(function() return qmiservice.lib[oname] end)
        if succ and (not serv_prior or (qmiservice.prior < serv_prior)) then
          serv_obj, serv_ver, serv_libname, serv_prior = res, v, libname, qmiservice.prior
        end
      end
    end

    if not serv_obj or not serv_ver then
      l.log("LOG_ERR", string.format("[luaqmi] QMI service not found - sname=%s",sname))
    else
      l.log("LOG_DEBUG", string.format("[luaqmi] QMI service found - sname=%s,ver=%d,lib=%s",sname,serv_ver,serv_libname))

      -- create serv
      servs[sname] = {sname=sname, serv_ver=serv_ver, serv_obj=serv_obj}
    end
  end

  -- remove previous qis
  for k,v in pairs(qis) do
    qis[k]=nil
  end

  l.log("LOG_INFO", "[luaqmi] * connect to QMI services")
  for _,serv in pairs(servs) do

    if serv.sname == "wds" then

      -- ipv4 wds
      for i = 1, luaqmi_max_wds do
        create_qmi_service(serv.sname)
      end

      -- ipv6 wds
      for i = 1, luaqmi_max_wds do
        create_qmi_service(serv.sname)
      end
    else
      create_qmi_service(serv.sname)
    end
  end

  l.log("LOG_INFO", "[luaqmi] * QMI service status")
  for sname,_ in pairs(r) do
    local qi = qis[sname]

    if qi then
      l.log("LOG_INFO", string.format("[luaqmi] QMI service %s/v%d [AVAIL] #%d",sname,qi[1].ver,#qis[sname]))
    else
      l.log("LOG_INFO", string.format("[luaqmi] QMI service %s [NOT AVAIL]",sname))
    end
  end

end

local member_candidates = {}
local memorized_members = {}

local function members(obj, ct)
  ct = ct or ffi.typeof(obj)

  local f = memorized_members[ct]
  if not f then
    local om = {} -- {{offset, "member"}, ...}
    for m,_ in pairs(member_candidates) do
      local o = ffi.offsetof(ct, m)
      if o then
        table.insert(om, {o, m})
      end
    end

    table.sort(om, function(a, b) return a[1] < b[1] end)

    local mms = {}
    for i = 1, #om do
      mms[i] = om[i][2]
    end

    f = function(obj)
      local i = 0
      return function()
        i = i + 1
        if i <= #mms then
          local m = mms[i]
          return m, obj[m]
        end
      end
    end

    memorized_members[ct] = f
  end

  return f(obj)
end

-- constructor
local function init()
  l.log("LOG_INFO", "[luaqmi] luaqmi start"  )

  l.log("LOG_INFO", "[luaqmi] load qmi errors - file=" .. luaqmi_qmi_error_file)
  qmsg_build_qem()

  l.log("LOG_INFO", "[luaqmi] load cdef - file=" .. luaqmi_message_file)
  qmsg_build()

  -- read luaqmi header
  local f = io.open(luaqmi_header_file,"r")
  local qmi_header=f:read("*all")
  f:close()

  -- build member candidates
  for w in string.gmatch(qmi_header, '[%w_]+') do
    member_candidates[w] = true
  end

  -- start ffi
  ffi.cdef(qmi_header)

  -- prepare receive buffer
  resp_to_recv = ffi.new("char[?]",luaqmi_recv_buf_size) -- receive buffer for non-blocking QMI access
  resp_len_to_recv = ffi.new("int[1]",luaqmi_recv_buf_size) -- receive buffer length for non-blocking QMI access

  -- load qmiservices
  libluaqmi = ffi.load("luaqmi")

  -- load optional libraries
  local libqmiservices_modem_exist,libqmiservices_modem = pcall(function() return ffi.load("qmiservices_modem") end)

  l.log("LOG_INFO", string.format("[luaqmi] qmiservices_modem (flag=%s)", libqmiservices_modem_exist))

#ifdef V_PROCESSOR_sdx20
  -- prior : low number of prior is first
  libqmiservices = {
    -- Use Modem IDL instead of apps for DMS. (suggested in QC#02930386)
    libqmiservices_modem = libqmiservices_modem_exist and {lib = libqmiservices_modem, prior=150} or nil,
    libqmiservices = {lib = ffi.load("qmiservices"), prior=200},
  }
#elif defined V_PROCESSOR_sdx55
  -- prior : low number of prior is first
  libqmiservices = {
    libloc = {lib = ffi.load("loc_api_v02"), prior=100},
    -- Use Modem IDL instead of apps for DMS. (suggested in QC#02930386)
    libqmiservices_modem = libqmiservices_modem_exist and {lib = libqmiservices_modem, prior=150} or nil,
    libqmiservices = {lib = ffi.load("qmiservices"), prior=200},
  }
  libqmi_cci = ffi.load("qmi")
#else
  -- prior : low number of prior is first
  libqmiservices = {
    netcomm = {lib = ffi.load("qminetcomm"), prior=100},
    libloc = {lib = ffi.load("loc_api"), prior=100},
    -- Use Modem IDL instead of apps for DMS. (suggested in QC#02930386)
    libqmiservices_modem = libqmiservices_modem_exist and {lib = libqmiservices_modem, prior=150} or nil,
    libqmiservices = {lib = ffi.load("qmiservices"), prior=200},
    libqmiservices_netcomm = {lib = ffi.load("qmiservices_netcomm"), prior=300},
    libqmiservices_ext = {lib = ffi.load("qmiservices_ext"), prior=400},
  }
  libqmi_cci = ffi.load("qmi")
#endif

  libqmi_client_qmux = ffi.load("libqmi_client_qmux")

  -- init qmi services
  init_qmi()
end

--[[ public  member functions ]]--

--! @brief Check if every name is in qis and value is not false
--! @param snames table (array, list) of names
--! @return true if every name
local function check_qmi_services(snames)

  for _,sname in ipairs(snames) do
    if not qis[sname] then
        l.log("LOG_ERR", string.format("service not available - %s", sname))
        return false
    end
  end

  return true
end

--! @brief Run initialisation again
local function reinit()
  init_qmi()
end

--! @brief Check if m is a member of t and return its value
local function is_c_member(t,m)
  return pcall(function() return t[m] end)
end

--! @brief create a QMI message
--! @param me entry from q.m table
--! @param qmi_lua_sid optional version
--! @return qm table suitable for sending
--!
--! This function creates a message structure suitable for sending later.
--! The message structure can look like this:
--! <PRE>
--!	qie:
--!	  ver: 1
--!	  qc: cdata
--!	  sname: wds
--!	  qmi_lua_sid: 1
--!	  fd: 93
--!	req_len: 24
--!	c_struct_resp: wds_bind_mux_data_port_resp_msg_v01
--!	resp_len: 8
--!	resp: cdata
--!	me:
--!	  ind: false
--!	  sname: wds
--!	  mname: bind_mux_data_port
--!	  name: QMI_WDS_BIND_MUX_DATA_PORT
--!	  msg_id: 162
--!	req: cdata
--!	c_struct_req: wds_bind_mux_data_port_req_msg_v01
--! </PRE>
local function new_msg(me,qmi_lua_sid)

  local qm={}

  if not me then
    l.log("LOG_ERR", "incorrect QMI message to create msg")
    return
  end

  if not qmi_lua_sid then
    qmi_lua_sid=1
  end

  -- store me
  qm.me = me

  -- store QMI control object
  qm.qie = qis[me.sname][qmi_lua_sid]
  if not qm.qie then
    l.log("LOG_ERR", "[luaqmi] QMI service not ready - " .. me.sname)
    return
  end

  -- store c struct infomation
  qm.c_struct_req = qmsg_get_req_struct(me.sname,me.mname,qm.qie.ver)
  qm.c_struct_resp= qmsg_get_resp_struct(me.sname,me.mname,qm.qie.ver)

  -- allocate request c struct
  qm.req=ffi.new(qm.c_struct_req)
  qm.req_len=ffi.sizeof(qm.c_struct_req)

  -- allocate resp c struct
  qm.resp=ffi.new(qm.c_struct_resp)
  qm.resp_len=ffi.sizeof(qm.c_struct_resp)

  -- set default error
  if is_c_member(qm.resp,"resp") then

    if is_c_member(qm.resp.resp,"result") then
      qm.resp.resp.result = -1
    end

    if is_c_member(qm.resp.resp,"error") then
      qm.resp.resp.error = -1
    end

  end

  if not qm.req then
    l.log("LOG_INFO", "[luaqmi] C structure for request not found - " .. qm.c_struct_req)
    return
  end

  if not qm.resp then
    l.log("LOG_INFO", "[luaqmi] C structure for response not found - " .. qm.c_struct_resp)
    return
  end

  return qm
end

local function log_cdata(cdata_name,cdata)

  local function get_ct_array_count(cdata)
    local stat, ct = pcall(function() return tostring(ffi.typeof(cdata)) end)

    if stat and ct then
      return tonumber(string.match(ct,"%[(%d+)%]"))
    end

    return
  end

  local function convert_cdata_tostr(cdata)
    local str,str_val=pcall(function() return ffi.string(cdata) end)
    local no,no_val=pcall(function() return tonumber(cdata) end)

    if str and str_val then
      return "'" .. str_val .. "'"
    elseif no and no_val then
      return string.format("%d #0x%04x",no_val, no_val)
    end

    return
  end

  -- do each of items if array
  local array_count = get_ct_array_count(cdata)
  local str = convert_cdata_tostr(cdata)

  if str then
    l.log("LOG_DEBUG",string.format("%s = %s",cdata_name, str))
  elseif array_count then
    local print_array_count = (array_count<libqmi_max_array_debug) and array_count or libqmi_max_array_debug
    for i = 0, print_array_count-1 do
      log_cdata(string.format("%s[%d]",cdata_name,i),cdata[i])
    end

    if print_array_count < array_count then
      l.log("LOG_DEBUG",string.format("%s[...] up to %d",cdata_name,array_count-1))
    end

  else
    for k,v in members(cdata) do
      log_cdata(string.format("%s.%s",cdata_name,k), v)
    end
  end

end

local function log_qm_req(qm)
  l.log("LOG_DEBUG",string.format("* req [%s]#%d", qm.me.name,qm.qie.qmi_lua_sid))

  if dconfig.enable_log_qmi_req then
    log_cdata(string.format("[%s] req", qm.me.name), qm.req)
  end

  l.log("LOG_DEBUG",string.format("[%s] #%d req struct - %s",qm.me.name,qm.qie.qmi_lua_sid,qm.c_struct_req))
end

local function log_qm_resp(qm,stat)
  if dconfig.enable_log_qmi_resp then
    log_cdata(string.format("[%s] resp", qm.me.name), qm.resp)
  end

  l.log("LOG_DEBUG",string.format("[%s] #%d resp struct - %s",qm.me.name,qm.qie.qmi_lua_sid,qm.me.ind and qm.c_struct_ind or qm.c_struct_resp))

  if qm.me.ind then
    return
  end

  if qm.resp.resp.result ~= 0 then
    local error_no=tonumber(qm.resp.resp.error)
    local error_name=qem[error_no] or "unknown"
    l.log("LOG_DEBUG",string.format("[%s] resp fail - %s #%d / stat=%d",qm.me.name,error_name,error_no,stat or 0))

    --[[

    -- disable following to avoid too much processor time when no SIM card is in use

    -- log resp if not req enable
    if not dconfig.enable_log_qmi_req and qm.me and qm.me.name and qm.req then
      log_cdata(string.format("[%s] req", qm.me.name), qm.req)
    end

    -- log resp if not resp enable
    if not dconfig.enable_log_qmi_resp then
      log_cdata(string.format("[%s] resp", qm.me.name), qm.resp)
    end

    ]]--

  else
    l.log("LOG_DEBUG",string.format("[%s] resp succ",qm.me.name))
  end

end

local function ret_qm_resp(qm)
  return tonumber(qm.resp.resp.result) == 0, tonumber(qm.resp.resp.error), qm.resp, qm
end

-- send QMI messagae
local function send_msg(qm,timeout)

  if not timeout or (timeout == 0) then
    timeout = luaqmi_default_timeout
  end

  log_qm_req(qm)

  local stat = libluaqmi.luaqmi_send_msg(qm.qie.qc, qm.me.msg_id, qm.req, qm.req_len, qm.resp, qm.resp_len, timeout)
  if stat and stat == 0 then
    watchdog_val = watchdog_val + 1
  end
  log_qm_resp(qm,stat)

  return stat == 0, stat
end

-- get QMI FD handles
local function get_async_fds()
  local fd_qies={}

  for _,qi in pairs(qis) do
    for _,qie in pairs(qi) do
      table.insert(fd_qies,{qie=qie,fd=qie.fd})
    end
  end

  return fd_qies
end

-- send QMI message in non-blocking mode
local function send_msg_async(qm,timeout_msec,timeout_func)

  local succ

  -- get TE reference
  local ref = nil
  if timeout_msec then
    ref = te_ref_index + 1
  end

  -- check if existing
  if ref then
    if qm.qie.qmsg_in_process[qm.me.name] then
      l.log('LOG_DEBUG', string.format("[qmi-async] QMI message already in process, ignore request [%s]#%d (ref=%d)", qm.me.name,qm.qie.qmi_lua_sid,ref))
      return false
    end

    if te_ref[ref] then
      l.log('LOG_ERR', string.format("[qmi-async] too many TE requests, delete previous request [%s]#%d (ref=%d)", qm.me.name,qm.qie.qmi_lua_sid,ref))

      -- cancel timer
      if te_ref[ref].canceler then
        te_ref[ref].canceler()
      end

      te_ref[ref]=nil
    end
  end

  -- allocate transaction element
  qm.te=ffi.new("void*[1]")

  -- queue QMI command
  succ = libluaqmi.luaqmi_send_msg_async(qm.qie.qc, qm.me.msg_id, qm.req, qm.req_len, qm.resp_len, qm.te, ref or 0) == 0

  -- schedule to cancel if QMI command is successfully queued
  if succ and ref then
    -- increase TE reference index
    te_ref_index = (te_ref_index + 1) % te_max_count;

    local te

    -- canceler closure function
    local canceler = function()
      l.log('LOG_DEBUG', string.format("[qmi-async] timeout is expired, cancel QMI message  [%s]#%d (ref=%d,timer=%d)",
        qm.me.name,qm.qie.qmi_lua_sid,ref,te.timeout_ref))
      libluaqmi.luaqmi_cancel_msg_async(qm.qie.qc,qm.te)

      -- reset TE and process flag
      qm.qie.qmsg_in_process[qm.me.name] = nil
      te_ref[ref] = nil

      -- calling timeout event function if exist
      if timeout_func then
        l.log('LOG_DEBUG', "[qmi-async] calling timeout_func...")
        timeout_func()
      end
    end

    -- get timeout
    local timeout = t.util.gettimemonotonic()+timeout_msec
    te = {
      timeout_ref = i:add_timeout(timeout,canceler,te),
      canceler = canceler,
    }
    l.log("LOG_DEBUG", string.format("[qmi-async] QMI message sent, add cancellation timeout [%s]#%d (ref=%d,timer=%d,duration=%d ms)",
      qm.me.name,qm.qie.qmi_lua_sid,ref,te.timeout_ref,timeout_msec))

    -- set TE and process flags
    qm.qie.qmsg_in_process[qm.me.name] = true
    te_ref[ref] = te
  end

  return succ
end

-- receive QMI message in non-blocking mode
local function recv_msg_async(qie_to_read)

  local msg_id = ffi.new("unsigned int[1]")
  local transp_err = ffi.new("qmi_client_error_type[1]")
  local sname_to_read = qie_to_read.sname
  local ind = ffi.new("int[1]")
  local ref = ffi.new("int[1]")

  -- read from any service
  resp_len_to_recv[0]=luaqmi_recv_buf_size
  local stat =  libluaqmi.luaqmi_recv_msg_async(qie_to_read.qc,msg_id,resp_to_recv,resp_len_to_recv,transp_err,ind,ref)
  local msg_ind = ind[0] ~= 0
  local msg_ref = (ref[0]>0) and ref[0] or nil

  -- bypass if not read
  if not stat or stat ~= 0 then
    return
  end
  watchdog_val = watchdog_val + 1

  -- update pipe flag
  libluaqmi.luaqmi_update_flag(qie_to_read.qc)

  -- build qm
  local qm = {}
  local mid = msg_id[0]

  local me

  -- get me from r or ir
  if msg_ind then
    me = ir[sname_to_read][mid]
  else
    me = r[sname_to_read][mid]
  end

  if not me then
    l.log('LOG_DEBUG', string.format("unknown QMI message received (ind=%s,sname=%s,mid=%d)",msg_ind,sname_to_read,mid))
    return
  end

  qm.me = me
  qm.transp_err = transp_err[0]

  local mname = me.mname
  if not mname then
    l.log('LOG_INFO', '[luaqmi] unknown QMI message received - msg_id=' .. qm.me.msg_id)
    return
  end

  -- build rest of qm
  qm.qie = qie_to_read

  --l.log("LOG_DEBUG", string.format("[luaqmi] got message (msg_id=0x%04x,ref=%d)" , qm.me.msg_id, msg_ref or 0))

  -- get response c structure
  local c_struct_resp = qmsg_get_resp_struct(sname_to_read,mname,qie_to_read.ver)
  local c_struct_ind = qmsg_get_ind_struct(sname_to_read,mname,qie_to_read.ver)
  if not c_struct_resp or not c_struct_ind then
    l.log('LOG_ERR', '[luaqmi] response/ind C structure not found - msg_id=' .. qm.me.msg_id)
    return
  end

  qm.c_struct_req = qmsg_get_req_struct(sname_to_read,mname,qm.qie.ver)
  qm.c_struct_resp = c_struct_resp
  qm.c_struct_ind = c_struct_ind

  -- erase req
  qm.req=nil
  qm.req_len=0

  -- copy resp
  if msg_ind then
    qm.resp=ffi.new(c_struct_ind)
    qm.resp_len=ffi.sizeof(qm.c_struct_ind)
  else
    qm.resp=ffi.new(c_struct_resp)
    qm.resp_len=ffi.sizeof(qm.c_struct_resp)
  end
  ffi.copy(qm.resp,resp_to_recv,qm.resp_len)

  -- remove cancellation timeout
  if not msg_ind and msg_ref then

    local te = te_ref[msg_ref]

    -- remove cancellation timer
    if te and te.timeout_ref then
      l.log("LOG_DEBUG", string.format("[qmi-async] QMI message received, remove cancellation timeout [%s]#%d (ref=%d,timer=%d)", qm.me.name,qm.qie.qmi_lua_sid,msg_ref,te.timeout_ref))
      i:remove_timeout(te.timeout_ref)
    end

    -- reset process flag
    if qm.qie.qmsg_in_process[qm.me.name] then
      l.log('LOG_DEBUG', string.format("[qmi-async] QMI message received, reset process flag [%s]#%d (ref=%d)", qm.me.name,qm.qie.qmi_lua_sid,msg_ref))
    end

    -- reset TE and process flag
    te_ref[msg_ref] = nil
    qm.qie.qmsg_in_process[qm.me.name] = nil
  end

  log_qm_resp(qm)

  return qm
end

local function luaqmi_example_async()

  print("* unit test - QMI_NAS_GET_SIGNAL_STRENGTH [non-blocking with Turbo]")

  local turbo = require("turbo")

  -- get turbo ioloop
  local il = turbo.ioloop.instance()

  local function qmi_write_callback()
    -- write async
    local qm = _q.new_msg(m.QMI_NAS_GET_SIGNAL_STRENGTH)
    -- build request
    qm.req.request_mask_valid=1;
    qm.req.request_mask=0x01;

    for i = 0, 9 do
      print(string.format("[send] async - QMI_NAS_GET_SIGNAL_STRENGTH - #%d",i))
      _q.send_msg_async(qm)
    end
  end

  -- qmi receive callback
  local recv_count = 0
  local function qmi_recv_callback(qie)
    local qm

    print("[recv] async - callback called")

    -- assume rssi
    qm = _q.recv_msg_async(qie)
    if not qm then
      print("[recv] async - not ready")
      return
    end
    print("[recv] QMI message recieved - " .. qm.me.name)

    if qm.me == m.QMI_NAS_GET_SIGNAL_STRENGTH then
      print(string.format("[recv] async - QMI_NAS_GET_SIGNAL_STRENGTH = %d - #%d",qm.resp.rssi[0].rssi,recv_count))

      recv_count=recv_count+1
      if recv_count == 10 then
        il:close()
      end
    end
  end

  -- connect recv. fds
  local fd_qies = _q.get_async_fds()
  for _, fds_qie in ipairs(fd_qies) do
    il:add_handler(fds_qie.fd,turbo.ioloop.READ,qmi_recv_callback,fds_qie.qie)
  end

  -- connect write callback
  print("[send] async - set 2 second interval to write qmi")
  il:set_interval(2000, qmi_write_callback)

  il:start()

  print("[send] async - done")

end

local function luaqmi_example_sync()

  print("* unit test - QMI_DMS_GET_DEVICE_MFR [blocking]")

  local succ,err,resp=_q.req(m.QMI_DMS_GET_DEVICE_MFR)
  _q.log_cdata("resp",resp)

  if succ then
    print(string.format("device_manufacturer = %s",ffi.string(resp.device_manufacturer)))
  else
    l.log("LOG_ERR","modem manufacturer information not available")
  end

  print("* unit test - QMI_NAS_GET_SIGNAL_STRENGTH [blocking]")

  -- create msg
  local qm = _q.new_msg(m.QMI_NAS_GET_SIGNAL_STRENGTH)
  -- build request
  qm.req.request_mask_valid=1;
  qm.req.request_mask=0x01;

  -- send sync
  print("[send] sync - QMI_NAS_GET_SIGNAL_STRENGTH")
  _q.send_msg(qm)
  print("[recv] sync - QMI_NAS_GET_SIGNAL_STRENGTH = " .. qm.resp.rssi[0].rssi)

  print("[send] sync - done")

end

local function luaqmi_example_qmi_dms_get_device_mfr()
  print("* unit test - QMI_DMS_GET_DEVICE_MFR")

  -- create msg
  local qm = _q.new_msg(m.QMI_DMS_GET_DEVICE_MFR)

  -- send sync
  print("[send] sync - QMI_QMI_DMS_GET_DEVICE_MFR")
  _q.send_msg(qm)
  print("[recv] sync - QMI_QMI_DMS_GET_DEVICE_MFR = " .. ffi.string(qm.resp.device_manufacturer))
  print("[send] sync - done")

end

local function is_c_true(c)
  return c and (c ~= 0)
end

--! @brief Check if m is a member of t and its value is true
local function is_c_member_and_true(t,m)
  local succ_valid,valid=pcall(function() return t[m] end)
  return succ_valid and valid ~= 0
end

local function get_resp_c_member(qr,m)

  local succ_valid,valid=pcall(function() return qr[m .. "_valid"] end)
  local succ_value,value=pcall(function() return qr[m] end)

  -- assume value is nil if its valid flag exists and the flag says it is not valid
  if succ_valid and not is_c_true(valid) then
    value=nil
  end

  -- assume value is nul if value does not exist
  if not succ_value then
    value=nil
  end

  return value
end

local function copy_bsettings_to_req(r,t)
  for k,v in pairs(t) do
    r[k] = v

    local succ,valid=pcall(function() return r[k .. "_valid"] end)

    if succ and valid then
      r[k .."_valid"] = true
    end
  end
end

local function req(me,bsettings,timeout,qmi_lua_sid)

  local qm = new_msg(me,qmi_lua_sid)

  if bsettings then
    copy_bsettings_to_req(qm.req,bsettings)
  end

  if not send_msg(qm,timeout) then
    return false, -1, qm.resp, qm
  end

  return ret_qm_resp(qm)
end

local function modify_profile_settings(profile_type,profile_index,bsettings,qmi_lua_sid)
  local qm = _q.new_msg(m.QMI_WDS_MODIFY_PROFILE_SETTINGS,qmi_lua_sid)

  -- build
  qm.req.profile.profile_type=profile_type
  qm.req.profile.profile_index=profile_index
  copy_bsettings_to_req(qm.req,bsettings)

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function create_profile(profile_type,profile_name,qmi_lua_sid)
  local qm = _q.new_msg(m.QMI_WDS_CREATE_PROFILE,qmi_lua_sid)

  -- build
  qm.req.profile_type=profile_type

  qm.req.profile_name_valid = (profile_name~=nil)
  qm.req.profile_name = profile_name

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_profile_settings(profile_index,profile_type,qmi_lua_sid)
  local qm = _q.new_msg(m.QMI_WDS_GET_PROFILE_SETTINGS,qmi_lua_sid)

  -- build
  qm.req.profile.profile_index=profile_index -- ffi.new("uint8_t",profile_index)
  qm.req.profile.profile_type=profile_type -- ffi.new("wds_profile_type_enum_v01",profile_type)

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_operating_mode()
  local qm = _q.new_msg(m.QMI_DMS_GET_OPERATING_MODE)

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function set_operating_mode(mode)
  local qm = _q.new_msg(m.QMI_DMS_SET_OPERATING_MODE)

  -- build
  qm.req.operating_mode=mode

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function initiate_attach(attach)
  local qm = _q.new_msg(m.QMI_NAS_INITIATE_ATTACH)

  -- build
  qm.req.ps_attach_action_valid=true
  qm.req.ps_attach_action=attach

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_serving_system()
  local qm = _q.new_msg(m.QMI_NAS_GET_SERVING_SYSTEM)

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_default_profile_num(profile_type,profile_family,qmi_lua_sid)
  local qm = _q.new_msg(m.QMI_WDS_GET_DEFAULT_PROFILE_NUM,qmi_lua_sid)

  -- build
  qm.req.profile.profile_type = profile_type
  qm.req.profile.profile_family = profile_family
  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

-- set the default supl profile
-- profile_family: 0 means Embedded, 1 means Socket/Tethered
-- profile_index : the profile index in the rdb
local function set_default_profile_num(profile_family, profile_index)
  local resp = false
  local current_supl_profile = -1
  local profile_type = 0  -- 0 means 3GPP profile, 1 is 3GPP2 which we do not use and is not supported at moment(23-01-2019)
  if not profile_family or not profile_index then
    l.log("LOG_ERR", "set supl profile. parameter missing")
    return false
  end
  -- get the current default profile
  result, err, ret, qm =  get_default_profile_num(profile_type,profile_family)
  if result then
    current_supl_profile = tonumber(ret.profile_index)
  else
    l.log("LOG_NOTICE", "failed get supl profle num")
  end

  if current_supl_profile == profile_index then
    l.log("LOG_NOTICE", string.format("set supl profile, profile index:%s is the current setting", profile_index))
    return true
  end

  -- set the default profile
  qm = _q.new_msg(m.QMI_WDS_SET_DEFAULT_PROFILE_NUM)
  qm.req.profile_identifier.profile_type = profile_type
  qm.req.profile_identifier.profile_family = profile_family
  qm.req.profile_identifier.profile_index = profile_index
  resp, stat = _q.send_msg(qm)
  if not resp then
    l.log("LOG_ERR", string.format("set supl profile failed, profile_index:%s", profile_index))
    return false
  else
    l.log("LOG_NOTICE", string.format("current supl profile: %s", current_supl_profile))
    l.log("LOG_NOTICE", string.format("new supl profile: %s", profile_index))
    return true
  end
end

local function start_network_interface(profile_index,qmi_lua_sid)

  local qm = _q.new_msg(m.QMI_WDS_START_NETWORK_INTERFACE,qmi_lua_sid)

  -- build
  qm.req.profile_index_valid=true
  qm.req.profile_index=profile_index

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_pkt_srvc_status(qmi_lua_sid)
  local qm = _q.new_msg(m.QMI_WDS_GET_PKT_SRVC_STATUS,qmi_lua_sid)

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function bind_mux_data_port(ep_type,epid,mux_id,qmi_lua_sid)
  local qm = _q.new_msg(m.QMI_WDS_BIND_MUX_DATA_PORT,qmi_lua_sid)

  -- build
  qm.req.ep_id_valid=true
  qm.req.ep_id.ep_type=ep_type
  qm.req.ep_id.iface_id=epid
  qm.req.mux_id_valid=true
  qm.req.mux_id=mux_id

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_profile_list(profile_type,qmi_lua_sid)
  --[[
  * profile_type
  WDS_PROFILE_TYPE_3GPP_V01
  WDS_PROFILE_TYPE_3GPP2_V01
]]--

  local qm = _q.new_msg(m.QMI_WDS_GET_PROFILE_LIST,qmi_lua_sid)

  -- build
  if profile_type then
    qm.profile_type_valid = true
    qm.profile_type = profile_type
  end

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_runtime_settings(qmi_lua_sid)
  local qm = _q.new_msg(m.QMI_WDS_GET_RUNTIME_SETTINGS,qmi_lua_sid)

  -- build
  qm.req.requested_settings_valid=true
  qm.req.requested_settings=bit.bor(
    0x0001, -- Profile identifier
    0x0002, -- Profile name
    0x0004, -- PDP type
    0x0008, -- APN name
    0x0010, -- DNS address
    0x0020, -- UMTS/GPRS granted QoS
    0x0040, -- User name
    0x0080, -- Authentication protocol
    0x0100, -- IP address
    0x0200, -- Gateway information (address and subnet mask)
    0x0400, -- P-CSCF address using PCO flag
    0x0800, -- P-CSCF server address list
    0x1000, -- P-CSCF domain name list
    0x2000, -- MTU
    0x4000, -- Domain name list
    0x8000, -- IP family
    0x10000, -- IM_CN flag
    0x20000 -- Technology name
  )

  -- send
  if not _q.send_msg(qm) then
    return
  end

  return _q.ret_qm_resp(qm)
end

local function get_conn_id_by_name(dev_id)
  local ep_type=ffi.new("int[1]")
  local epid=ffi.new("int[1]")
  local mux_id=ffi.new("int[1]")

  local conn_id=libqmi_client_qmux.qmi_linux_get_conn_id_by_name_ex(dev_id,ep_type,epid,mux_id)

  return tonumber(conn_id), tonumber(ep_type[0]),tonumber(epid[0]),tonumber(mux_id[0])
end

local function luaqmi_example_operation()

  -- power on
  set_operating_mode("DMS_OP_MODE_ONLINE_V01")
  os.execute("sleep " .. tonumber(1))

  -- bind
  local conn_id,ep_type,epid,mux_id = get_conn_id_by_name("rmnet_data1")
  print(string.format("conn_id=%d,ep_type=%d,epid=%d,mux_id=%d",conn_id,ep_type,epid,mux_id))
  bind_mux_data_port(ep_type,epid,mux_id)

  -- get profile
  get_profile_list()
  get_default_profile_num("WDS_PROFILE_TYPE_3GPP_V01","WDS_PROFILE_FAMILY_SOCKET_V01")
  get_profile_settings(1,"WDS_PROFILE_TYPE_3GPP_V01")
  get_profile_settings(2,"WDS_PROFILE_TYPE_3GPP_V01")

  --create_profile("WDS_PROFILE_TYPE_3GPP_V01","test_profile")
  modify_profile_settings("WDS_PROFILE_TYPE_3GPP_V01",2,{profile_name="test2",apn_name="telstra.extranet"})
  get_profile_settings(2,"WDS_PROFILE_TYPE_3GPP_V01")

  -- create #3
  --create_profile("WDS_PROFILE_TYPE_3GPP_V01","test3")
  print("profile3")
  modify_profile_settings("WDS_PROFILE_TYPE_3GPP_V01",3,{
    profile_name="test3",
    apn_name="telstra.corp",
    username="cdcs_yong@call-direct.com.au",
    password="YonCDCS",
  })
  get_profile_settings(3,"WDS_PROFILE_TYPE_3GPP_V01")

  get_operating_mode()
  get_serving_system()
  get_runtime_settings()

  -- attach
  initiate_attach("NAS_PS_ACTION_ATTACH_V01")

  -- start
  start_network_interface(1,1)

  -- bind #2
  local conn_id,ep_type,epid,mux_id = get_conn_id_by_name("rmnet_data2")
  print(string.format("conn_id=%d,ep_type=%d,epid=%d,mux_id=%d",conn_id,ep_type,epid,mux_id))
  bind_mux_data_port(ep_type,epid,mux_id,2)

  -- start
  start_network_interface(2,2)

  -- bind #3
  local conn_id,ep_type,epid,mux_id = get_conn_id_by_name("rmnet_data3")
  print(string.format("conn_id=%d,ep_type=%d,epid=%d,mux_id=%d",conn_id,ep_type,epid,mux_id))
  bind_mux_data_port(ep_type,epid,mux_id,3)

  -- start
  start_network_interface(3,3)

  while true do
    get_pkt_srvc_status(1)
    get_pkt_srvc_status(2)
    get_pkt_srvc_status(3)
    os.execute("sleep " .. tonumber(1))
  end

end

local function perform_unit_test()
  print([[
> please make sure the result is like the following example
> [recv] sync - QMI_NAS_GET_SIGNAL_STRENGTH = 128
> [recv] async - QMI_NAS_GET_SIGNAL_STRENGTH = 128
> [recv] sync - QMI_QMI_DMS_GET_DEVICE_MFR = QUALCOMM INCORPORATED
]])

  luaqmi_example_sync()
  luaqmi_example_async()
  luaqmi_example_qmi_dms_get_device_mfr()

  luaqmi_example_operation()

  return true
end

-- initiate unit test
if not modname then
  l={log=function(logtype,...) print("luaqmi-unit-test [" .. logtype .. "] " .. ...) end}
end

_q = {
  send_msg_async=send_msg_async,
  send_msg=send_msg,
  new_msg=new_msg,
  reinit=reinit,
  check_qmi_services=check_qmi_services,
  recv_msg_async=recv_msg_async,
  get_async_fds=get_async_fds,
  m=m,
  e=e,
  im=im,
  ret_qm_resp=ret_qm_resp,
  copy_bsettings_to_req=copy_bsettings_to_req,
  get_conn_id_by_name=get_conn_id_by_name,
  req=req,
  qis=qis,
  is_c_true=is_c_true,
  is_c_member_and_true=is_c_member_and_true,
  get_resp_c_member=get_resp_c_member,
  log_cdata=log_cdata,
  is_c_member=is_c_member,

  luaqmi_max_wds=luaqmi_max_wds,
  get_default_profile_num = get_default_profile_num,
  set_default_profile_num = set_default_profile_num,
}

-- create signleton instance
init()

-- perform unit test
if not modname then
  perform_unit_test()

  return
end

return _q

--[[

 * syntax to feed C through FFI
 char[10]    --> ffi.new("char[10]","hello")       // constant length array of char
 char*       --> ffi.new("char[?]",1024)           // 1024 memory buffer of char
 struct s_t* --> ffi.new("struct s_t[1]")          // parameter by reference
 struct s_t  --> ffi.new("struct s_t")
 enum e_t    --> ffi.new("enum e_t","ENUM_VALUE")

 * syntax to extract values from C via FFI
 struct s_t s[10] --> s[0].member                  // access the first entity of array
 char[10]         --> ffi.string(cdata)            // convert to string
 int[10]          --> tonumber(cdata[0])           // get the first entity of array and convert it to Lua number
 char*            --> ffi.string(cdata)
 enum e_t v       --> ffi.string(v)                // not working on MTP - need to investigate
                      or tonumber(v)
 int v            --> tonumber(v)

 * convert from (void*) to (struct s_t*) and access member
 cdata_struct_ptr = ffi.cast("struct s_t*", cdata_void_ptr)
 print(cdata_struct_ptr.member)

--]]

