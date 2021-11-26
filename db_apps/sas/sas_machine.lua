--
-- Copyright (C) 2019 NetComm Wireless Limited.
--
local SasMachine = require("wmmd.Class"):new()

function SasMachine:setup(p)
  self.l = p.l
  self.json = p.json
  self.turbo = p.turbo
  self.util = p.util
  self.watcher = p.watcher
  self.tio = p.turbo.ioloop.instance()
  self.rdb = p.rdb

  self.COULD_NOT_CONNECT = -3
  self.CONNECT_TIMEOUT = -5
  self.REQUEST_TIMEOUT = -6
  self.BUSY = -12
  self.METHOD_NOT_ALLOWED = 405

  self.check_crl = require("check_crl_support")

  self.ctx = {}
  self.tabs = require('sas.tabdefn')
  self.eventlog = require('sas.sas_eventlog')

  self.msg_tab = self.tabs[1]
  self.obj_tab = self.tabs[2]

  --self.response_tab = require('./responses')
  self.request_vars = false

  self.default_values = {}

  -- Increment the version number for any significant bug fixes and, or feature additions made.
  self.client_version = '1.10'
  self.rdb.set("sas.client.version", self.client_version)

  self.l.log("LOG_NOTICE", "setup sas_machine complete")
end

function SasMachine:examine_obj(cb, otab, id, isray, k, v)
  for i, t in ipairs(v)
  do
   if t.id then
     if t.tdef then
       if otab[t.tdef] or string.find(t.tdef, '%u') then
         if not otab[t.tdef] then
           self.l.log("LOG_ERR", 'item '..i..' id '.. t.id.. ' of '..k..' refers to non existant object')
         elseif cb.examine then
           cb.examine(self, cb, otab, t.id, t.isray, t.tdef, otab[t.tdef])
         end
       else
         -- primative, no uppercase
         cb.primative(self, cb, t)
       end
     else
       self.l.log("LOG_ERR", 'item '..i..' id '.. t.id..' has no tdef')
     end
   else
      self.l.log("LOG_ERR", 'item '..i..' of '..id..' tdef '..k..' has no id value')
   end
  end
end

function SasMachine:build_vars(cb, t)
   local rtab = cb.is_req and cb.req or cb.res
   if rtab[t.id] ~= nil  then
    if rtab[t.id].tdef ~= t.tdef then
      self.l.log("LOG_ERR", 'item id '.. t.id.. ' has different tdef prev '..t.tdef..' current '..rtab[t.id].tdef)
     end
     return
   end
   rtab[t.id] = self.util.shallow_copy({}, t)
end


function SasMachine:build_examine(cb, otab, id, isray, k, v)
  if (cb['visit']) then
    cb.visit[k] = true
  end
  self:examine_obj(cb, otab, id, isray, k, v)
end

--- Create the inverted variables tables
--  These tables are inverted from variables to the objects
--  Used for checking user table variables

function SasMachine:create_vars()
  local visit = {}
  local cb = {
    -- things needed for all examine_obj calls
    ['primative'] = self.build_vars,
    ['examine']   = self.build_examine,

    -- things needed for the builder calls
    ['req']       = {},
    ['res']       = {},
    ['visit']     = visit,
  }
  local seen = {}
  for k, v in pairs(self.obj_tab)
  do
    local is_req = string.match(k,"(.-)Request$")
    local is_res = string.match(k,"(.-)Response$")
    cb.is_req = is_req
    if (is_req or is_res) then
      self:examine_obj(cb, self.obj_tab, k, false, k, v)
    else
      seen[k] = true
    end
  end
  for k, _ in pairs(seen)
  do
    if not visit[k] then
      self.l.log("LOG_ERR", 'Item '..k..' not visited')
    end
  end
  self.request_vars = cb.req
end

function SasMachine:creq_prim(cb, t)
  local rtab = cb.req
  local use
  local v
  if rtab[t.id] ~= nil then
    use = true
  --   elseif t.reqd == 'Required' then
    -- use = true
  end
  if not use then return end
  if rtab[t.id] ~= nil then
    v = rtab[t.id]
    -- self.l.log("LOG_DEBUG", 'value for '..t.id..' is '..v..' def is '..t.tdef)
  else
    v = self.default_values[t.id]
  end
  -- convert value to required form
  if t.tdef == 'file' then
    if self.util.ends_with(v, ".lua") then
      local ok, val = pcall(function() return dofile(v)  end)
      if ok then
        v = val
      else
        self.l.log("LOG_ERR", 'file primative '..t.id..' did not compile or execute ' .. val)
      end
    elseif self.util.ends_with(v, ".json") then
      local ok, val = pcall(function() return self.util.slurp(v)  end)
      if ok then
        v = self.turbo.escape.json_decode(val)
      else
        self.l.log("LOG_ERR", 'file primative '..t.id..' did not read ' .. val)
      end
    else
      self.l.log("LOG_ERR", 'file primative '..t.id..' unknown type ' .. v)
    end
  elseif t.tdef == 'jsonstring' then
    v = self.turbo.escape.json_decode(v)
  elseif t.tdef == 'string' then
    v = tostring(v) or ''
  elseif t.tdef == 'number' then
    v = tonumber(v) or 0
  elseif t.tdef == 'boolean' then
    v = not (v == 0  or v == '' or v == false or v == nil)
  elseif t.tdef == 'table' then
   -- all good
  elseif string.match(t.tdef, '^array ') and  type(v) == 'table' then
    cb.curtab[t.id] = v
    return
  else
    self.l.log("LOG_ERR", 'primative '..t.id..' does not have correct type: '..t.tdef..(t.isray and ' isray' or ' not ray'))
  end
  if t.isray then v = {v} end
  cb.curtab[t.id] = v
end

function SasMachine:creq_examine(cb, otab, id, isray, k, v)
  local tab = cb.curtab
  cb.curtab = {}
  self:examine_obj(cb, otab, id, isray, k, v)
  local t = cb.curtab
  if next(cb.curtab) == nil then
    t = {}
  end
  if isray then
    t = { t }
  end
  -- most efficient way to determine is a table is empty
  if next(cb.curtab) ~= nil or (v.reqd and v.reqd == 'Required') then
    tab[id] = t
  end
  cb.curtab = tab
end

function SasMachine:build_request(ctx, msg, grant)
  local req_tab = ctx.req_tab
  if grant then
    if ctx.grants[grant] and ctx.grants[grant].req_tab then
       req_tab = ctx.grants[grant].req_tab
    end
  end
  local cb = {
    -- things needed for all examine_obj calls
    ['primative'] = self.creq_prim,
    ['examine']   = self.creq_examine,

    -- things needed for the build request calls
    ['req']       = req_tab,
    ['curtab']    = {},
  }
  local k = msg
  local v = self.msg_tab[k]
  if not v then
    self.l.log("LOG_ERR", 'Invalid request for msg of type '..msg)
    return {}
  else
    self:examine_obj(cb, self.obj_tab, v.id, v.isray, k, v)
    return cb.curtab
  end
end

-- ssl_options kwargs:
-- ``priv_file`` SSL / HTTPS private key file.
-- ``cert_file`` SSL / HTTPS certificate key file.
-- ``verify_ca`` SSL / HTTPS chain verifification and hostname matching.
--      Verification and matching is on as default.
-- ``ca_path`` SSL / HTTPS CA certificate verify location
--        ['priv_file'] = "./UUTCBSDprivkey.key",
--        ['cert_file'] = "./UUTCBSD.pem",
--        ['ca_path']   = "./TestLabSASPKIchain.pem",


function SasMachine:walk_prim(cb, t)
  local rtab = cb.res
  local body = cb.body
  local v

  -- self.l.log("LOG_DEBUG", 'primative id '..t.id..' body '.. self.util.tprint(cb.body))
  if body[t.id] == nil then
    if t.reqd and t.reqd == 'Required' then
      self.l.log("LOG_ERR", 'primative item '.. t.id.. ' is not present in the response')
    end
    return
  end
  v = body[t.id]
  -- self.l.log("LOG_DEBUG", 'response value for '..t.id..' is '..v)
  rtab[t.id] = v
end

function SasMachine:walk_examine(cb, otab, id, isray, k, v)
  if not cb.body[id] then
    if v.reqd and v.reqd == 'Required' then
      self.l.log("LOG_ERR", 'object item '.. id.. ' is not present in the response')
    end
    return
  end
  local body = cb.body
  cb.body = body[id]
  if cb.body[1] then
    local body2 = cb.body
    for _, t in ipairs(body2) do
      cb.body = t
      self:examine_obj(cb, otab, id, isray, k, v)
    end
  else
    self:examine_obj(cb, otab, id, isray, k, v)
  end
  cb.body = body
end

function SasMachine:walk_response(ctx, msg, body)
  local cb = {
    -- things needed for all examine_obj calls
    ['primative'] = self.walk_prim,
    ['examine']   = self.walk_examine,

    -- things needed for the walk
    ['res']       = {},
    ['body']      = body,
  }
  local k = msg
  local v = self.msg_tab[k]
  if not v then
    self.l.log("LOG_ERR", 'Invalid response for msg of type '..msg)
    return {}
  else
    self:examine_obj(cb, self.obj_tab, v.id, v.isray, k, v)
    ctx.current_response = cb.res
    self.util.shallow_copy(ctx.res_tab, cb.res)
    return cb.res
  end
end

function SasMachine:init_client_http(ctx)
  if not ctx.client then
    local ssl_opts = {}
    local ssl_vars =  { 'priv_file', 'cert_file', 'verify_ca', 'ca_dir', 'priv_pass' }
    self.util.copy_only_vars(ssl_opts, ctx.state_machine.ssl_opts, ssl_vars)

    if self.rdb.get("sas.crl_checking_enabled") == "1" then
      ssl_opts["verify_callback"] = self.check_crl.verify_callback
      self.check_crl.init("sas_client", ssl_opts["ca_dir"], true)
    end
    local keep_alive = self.rdb.get('sas.config.keep_alive') == "1"
    ctx.kwargs = {
      ['method']     = 'POST',
      ['body']       = '',
      ['on_headers'] = function(headers) headers:set("Content-Type", "application/json") end,
      -- ['allow_redirects'] = true,
      ['ssl_options'] = ssl_opts,
      ['request_timeout'] = '20',  -- does not include connect timeout, docs are incorrect
      ['connect_timeout'] = '15',  -- cater for ssl verification time ~ 3 seconds average, could be more
      ['user_agent'] = string.format('Casa Systems SAS Client v%s', self.client_version),
      ['keep_alive'] = keep_alive,
      ['tcp_keep_alive'] = true,
      ['tcp_keep_alive_idle_secs'] = 7,
      ['tcp_keep_alive_intvl_secs'] = 3,
      ['tcp_keep_alive_cnt'] = 2,
      --[''] = '',
    }
    ctx.client = self.turbo.async.HTTPClient(ssl_opts, self.tio)
  end
end

function SasMachine:logRequestKeyMsg(request, jsObject, js)
  local obj = nil
  self.l.log("LOG_NOTICE", string.format('Sending %s request', request))
  if request == 'registration' then
    obj = jsObject.registrationRequest[1]
    self.l.log("LOG_NOTICE", string.format('userId:%s, fccId:%s, SN:%s', obj.userId, obj.fccId, obj.cbsdSerialNumber))
  elseif request == 'grant' then
    obj = jsObject.grantRequest[1]
    self.l.log("LOG_NOTICE", string.format('cbsdid:%s, freq:%s-%s', obj.cbsdId, tonumber(obj.operationParam.operationFrequencyRange.lowFrequency)/1000000, tonumber(obj.operationParam.operationFrequencyRange.highFrequency)/1000000))
  elseif request == 'heartbeat' then
    local grants = {}
    for i=1, #jsObject.heartbeatRequest do
      obj = jsObject.heartbeatRequest[i]
      table.insert(grants, obj.grantId)
    end
    self.l.log("LOG_NOTICE", string.format('grantId:%s', table.concat(grants,',')))
  elseif request == 'relinquishment' then
    obj = jsObject.relinquishmentRequest[1]
    self.l.log("LOG_NOTICE", string.format('grantId:%s', obj.grantId))
  elseif request == 'spectrumInquiry' then
    obj = jsObject.spectrumInquiryRequest[1]
  elseif request == 'deregistration' then
    obj = jsObject.deregistrationRequest[1]
    self.l.log("LOG_NOTICE", string.format('cbsdId:%s', obj.cbsdId))
  end

  for line in self.util.splitlines(js) do
    self.l.log("LOG_DEBUG", line)
  end
end

function SasMachine:logResponseKeyMsg(request, jsObject, responseCode, responseData)
  local obj = nil
  self.l.log("LOG_NOTICE", string.format('Received %s response, responseCode:%s', request, responseCode))
  if request == 'registration' then
    obj = jsObject.registrationResponse[1]
    self.l.log("LOG_NOTICE", string.format('cbsdId:%s', obj.cbsdId))
  elseif request == 'grant' then
    local obj = jsObject.grantResponse[1]
    self.l.log("LOG_NOTICE", string.format('grantId:%s', obj.grantId))
  elseif request == 'heartbeat' then
    local obj = jsObject.heartbeatResponse[1]
    self.l.log("LOG_NOTICE", string.format('grantId:%s', obj.grantId))
  elseif request == 'relinquishment' then
    local obj = jsObject.relinquishmentResponse[1]
    self.l.log("LOG_NOTICE", string.format('grantId:%s', obj.grantId))
  elseif request == 'spectrumInquiry' then
    obj = jsObject.spectrumInquiryResponse[1]
  elseif request == 'deregistration' then
    obj = jsObject.deregistrationResponse[1]
    self.l.log("LOG_NOTICE", string.format('cbsdId:%s', obj.cbsdId))
  end
  if responseCode ~= 0 and responseData then self.l.log("LOG_NOTICE", 'responseData: '..table.concat(responseData, ',')) end

  local js = self.json:encode_pretty(jsObject)
  for line in self.util.splitlines(js) do
    self.l.log("LOG_DEBUG", line)
  end
end

-- log the related debug info
function SasMachine:log_fetch_error_ctx(ctx, res)
  self.l.log("LOG_NOTICE", '== SYSTEM STATE ==')
  if res.error then
    self.l.log("LOG_ERR", 'There was an error in fetch '.. res.error.message)
  end
  -- dump the related rdb
  rdbList = { "wwan.0.radio_stack.rrc_stat.rrc_stat",
              "wwan.0.system_network_status.attached",
              "wwan.0.radio_stack.e_utra_measurement_report.servphyscellid",
              "wwan.0.radio.information.signal_strength",
              "wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq",
              "wwan.0.radio_stack.e_utra_measurement_report.dl_bandwidth",
              "wwan.0.radio_stack.e_utra_pusch_transmission_status.total_pusch_txpower",
              "wwan.0.radio_stack.e_utra_measurement_report.cellid",
              "wwan.0.system_network_status.eci_pci_earfcn",
              "sas.neighbour_grant_list",
              "wwan.0.neighbour_lock_list",
              "wwan.0.txctrl.tx_state",
              "sas.grant.0.id",
              "sas.ip_state",
              "sas.transmit_enabled",
            }
  for i=1, #rdbList do
     self.l.log("LOG_NOTICE", string.format('%s = %s', rdbList[i], self.rdb.get(rdbList[i]) or 'empty'))
  end

  -- log the current modem pci lock info
  self.rdb.set("wwan.0.modem_pci_lock_query", "1")

  -- dump the route table
  local tmpfile = '/tmp/saserr.txt'
  os.execute('rdb dump cell_meas > '..tmpfile)
  os.execute('ifconfig | grep -A2 rmnet_data >> '..tmpfile)
  os.execute('ip rule list >> '..tmpfile)
  os.execute('ip route list >> '..tmpfile)
  os.execute('iptables -S >> '..tmpfile)
  os.execute('route >> '..tmpfile)
  local f = io.open(tmpfile)
  if f then
    for line in f:lines() do
      self.l.log("LOG_NOTICE", string.format('%s', line))
    end
    f:close()
  end
  self.l.log("LOG_NOTICE", '== END SYSTEM STATE ==')
end

function SasMachine:error_recovery(err)
  local last_err = self.rdb.get("sas.error") or ''
  if err then
    local now = os.time()
    if last_err ~= tostring(err) then
      self.rdb.set("sas.error", err)
      self.rdb.set("sas.error.ts", now)
    end
    local duration = now - (tonumber(self.rdb.get("sas.error.ts")) or now)
    local count = self.recovery_attempt and self.recovery_attempt or 1
    if duration > (120 * count) then
      self.l.log("LOG_ERR", string.format("error:%s for %s sec, recovery attempt:%s", err, duration, count))
      os.execute("/usr/bin/sas_route.sh")
      self.ctx.client = nil
      self:init_client_http(self.ctx)
      self.recovery_attempt = count + 1
    end
    return
  end
  if last_err ~= '' then
    self.rdb.set("sas.error", '')
    self.recovery_attempt = nil
  end
end

function SasMachine:do_request(ctx, request)
  local url = ctx.state_machine.url .. request
  local requestName = request .. 'Request'
  local responseName = request .. 'Response'
  local get_request = function()
    local r
    if ctx.bundle then
      -- build request for each grant then bundle them into an array to be sent in one request
      r = {}
      r.heartbeatRequest = {}
      for g,_ in pairs(ctx.bundle.grants) do
        local req = self:build_request(ctx, requestName, g)
        table.insert(r[requestName], req[requestName][1])
      end
    else
      r = self:build_request(ctx, requestName, ctx.grant)
    end
    return r
  end
  local send_request = function(res, err, scode)
    if err then
      -- print debug info for MAG-1119 when connection issues observed
      self:log_fetch_error_ctx(ctx, res)
      self.l.log("LOG_NOTICE", string.format("Retrying request:%s after connection failure", request))
      if request == 'heartbeat' then
        ctx.kwargs.body = self.json:encode_pretty(get_request())
        for line in self.util.splitlines(ctx.kwargs.body) do self.l.log("LOG_DEBUG", line) end
      end
    end
    if scode and scode == self.METHOD_NOT_ALLOWED then
      if ctx.kwargs.keep_alive == false then return res, err, scode end
      self.l.log("LOG_NOTICE", "HTTP ERROR 405, set ctx.kwargs.keep_alive = false, retry")
      -- some SAS eg. Federated Wireless don't support turbo's keep_alive option
      ctx.kwargs.keep_alive = false
    end
    local res = coroutine.yield(ctx.client:fetch(url, ctx.kwargs))
    local err = res.error and res.error.code or false
    local scode = res.headers and res.headers:get_status_code() or false
    local emessage = res.error and res.error.message or ""
    return res, err, scode, emessage
  end

  local r = get_request()
  ctx.kwargs.body = self.json:encode_pretty(r)
  -- log the simplified request
  self:logRequestKeyMsg(request, r, ctx.kwargs.body)

  local res, err, scode, emessage = send_request()
  if err == self.COULD_NOT_CONNECT or err == self.CONNECT_TIMEOUT or
     err == self.REQUEST_TIMEOUT or err == self.BUSY or scode == self.METHOD_NOT_ALLOWED then
     -- immediate retry for some error types
    res, err, scode = send_request(res, err, scode)
  end
  self:error_recovery(res.error and res.error.code)
  ctx.res_tab['error_code'] = err
  ctx.res_tab['status_code'] = scode
  ctx.res_tab['error_message'] = emessage
  self.l.log("LOG_DEBUG", 'status code '..tostring(ctx.res_tab['status_code']))
  if ctx.res_tab['error_code'] or ctx.res_tab['status_code'] ~= 200 then
    -- need to alert client and perhaps repeat after a timeout
    self.watcher.invoke("sas", "on_sas_response", {r='error', ctx=ctx, req=request,
        ec=ctx.res_tab['error_code'] or 'ok',
        sc=ctx.res_tab['status_code'] or 'not present',
        em=emessage or ''})
    local logdata = r[requestName] and r[requestName][1] or {}
    self.eventlog.log(request, logdata)
    return
  end

  local body = self.turbo.escape.json_decode(res.body)
  ctx.res_body = body
  -- ensure received reponse matches the request
  if not body[responseName] then
    local rxresponse = nil
    for k,v in pairs(body) do rxresponse = k break end -- get first response name (should be only one)
    local message = string.format("un-expected response for TX:%s, RX:%s", request, rxresponse)
    self.l.log("LOG_ERR", message)
    self.watcher.invoke("sas", "on_sas_response", {r='error', ctx=ctx, req=request, ec='IO out of sync', sc='NA', em=message})
    return
  end

  local responseCode = 0
  if ctx.bundle then
    ctx.bundle.responses = {}
    for i=1, #body[responseName] do
      local resp = {[responseName] = {body[responseName][i]}}
      local wres = self:walk_response(ctx, responseName, resp)
      self.eventlog.log(request, wres)
      self:logResponseKeyMsg(request, resp, wres.responseCode, wres.responseData)
      table.insert(ctx.bundle.responses, wres)
    end
  else
    local wres = self:walk_response(ctx, responseName, body)
    if not wres.grantId then wres.grantId = ctx.grant end
    self.eventlog.log(request, wres)
    -- log the simplified response
    self:logResponseKeyMsg(request, body, wres.responseCode, wres.responseData)
    responseCode = wres.responseCode
  end

  self.watcher.invoke("sas", "on_sas_response", {r=request, ctx=ctx, rc=responseCode})
end

function SasMachine:create_ctx(ctx)
  if not self.request_vars then
    self:create_vars()
  end
  ctx.req_tab = {}
  ctx.res_tab = {}
  ctx.state_machine = {}
  ctx.queued_req_tabs = {}
end

SasMachine.cbs_system = {
  "start_machine",
  "do_action",
}

--- Initialize an sas client
--  This must be done before the state machine can be run
-- @param user_tab a table of user paramters to fill in over the default values
--
-- @return a context
--    'req_tab': client set user values
--    'res_tab': response values for the user
--    'state_machine' : current state machine
function SasMachine:init()
  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sas", v, self, v)
  end

  self:create_ctx(self.ctx)
end

--- Set values for an sas client
--  This can be done anytime after initialisation
-- @param ctx The context table returned from initialize
-- @param user_tab a table of user paramters to fill in
-- @return a table of user parameters that are not in any object
-- This can be checked for spelling mistakes

function SasMachine:set_values(ctx, user_tab, grant)
  local req_tab = ctx.req_tab
  if not user_tab then return end
  if grant then
    if ctx.grants[grant] and ctx.grants[grant].req_tab then
       req_tab = ctx.grants[grant].req_tab
    end
  end
  self.util.shallow_copy(req_tab, user_tab)
  local ret = {}
  for k, v in pairs(user_tab)
  do
    if not self.request_vars[k] then
      ret[k] = v
    end
  end
  return ret
end

function SasMachine:requested_values(ctx, grant)
  local req_tab = ctx.req_tab
  if grant then
    if ctx.grants[grant] and ctx.grants[grant].req_tab then
       req_tab = ctx.grants[grant].req_tab
    end
  end
  return req_tab
end

--- Del values for an sas client
--  This can be done anytime after initialisation
-- @param ctx The context table returned from initialize
-- @param user_tab a table of user paramters to delete
-- This can be checked for spelling mistakes

function SasMachine:set_values_nil(ctx, user_tab, grant)
  local req_tab = ctx.req_tab
  if not user_tab then return end
  if grant then
    if ctx.grants[grant] and ctx.grants[grant].req_tab then
       req_tab = ctx.grants[grant].req_tab
    end
  end
  for k, _ in pairs(user_tab)
  do
    if req_tab[k] ~= nil then
      req_tab[k] = nil
    end
  end
end

function SasMachine:do_action(type, event, a)
  self.l.log("LOG_DEBUG", string.format('do_action(): %s', self.ctx.action))
  if self.ctx.action == 'wait_user' then
    return
  end

  local ip_state = self.rdb.get('sas.ip_state')
  local attached = self.rdb.get('wwan.0.system_network_status.attached')
  if ip_state ~= "1" or attached ~= "1" then
    -- network is not ready, do nothing, let it times out
    self.l.log("LOG_NOTICE", string.format("NO network, can't send a:%s g:%s with attached:%s ip_state:%s, retry later",
      self.ctx.action, self.ctx.grant, attached, ip_state))
    self.watcher.invoke("sas", "on_sas_response", {r='error', ctx=self.ctx, req=self.ctx.action, ec='abort', sc='no network'})
    return
  end

  self:do_request(self.ctx, self.ctx.action)
end

--- Start the state machine
--  If you have saved state machine values, these can be set here
-- @param a.ctx The context table returned from initialize
-- @param a.saved_state optional previously saved state
--
function SasMachine:start_machine(type, event, a)
  if not a.ctx.state_machine then ctx.state_machine = {} end
  if (a.saved_state) then
    self.util.shallow_copy(a.ctx.state_machine, a.saved_state)
  else
   self.l.log("LOG_ERR", 'Software requires a saved state')
  end
  self:error_recovery()
  self:init_client_http(a.ctx)
end

return SasMachine
