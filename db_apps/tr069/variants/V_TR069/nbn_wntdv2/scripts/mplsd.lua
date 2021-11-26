#!/usr/bin/env lua
require('luardb')
require('luasyslog')
require('stringutil')
require('tableutil')

-- setup for our local includes
local us = debug.getinfo(1, "S").source:match[[^@?(.*[\/])[^\/]-$]]
if us then
  package.path = us.."?.lua;"..package.path
else
  us = ''
end

-- pull in our stuff
require('unid-class')

-- setup syslog facility
luasyslog.open('mpls', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')
require('rdbtable')

assert(conf.wntd.grePrefix)
assert(conf.wntd.bridgePrefix)
assert(conf.wntd.wanProfile or conf.wntd.greAddrVar)
assert(conf.wntd.switchIface)
assert(conf.wntd.switchConfig)

require('gre-ifaces')

--------------------------------------------------
-- lock state, use an incrementing counter so
-- it is safe to lock when already locked

local lock_state = { state = 0 }

function lock_state.lock()
  if lock_state.state > 0 then
    lock_state.state = lock_state.state + 1
  else
    lock_state.state = 1
    luardb.lock()
  end
end

function lock_state.unlock()
  if lock_state.state == 1 then
    lock_state.state = 0
    luardb.unlock()
  else
    lock_state.state = lock_state.state - 1
  end
end

------------------------------------------------
local function dinfo(v)
  if conf.wntd.debug > 1 then
    luasyslog.log('LOG_INFO', v)
  end
end

------------------------------------------------

local function forcenumber(n)
  return tonumber(n) or 0
end
--------------------------------------------------
local ourLocalGreRdbVar = conf.wntd.greAddrVar or 'link.profile.'..conf.wntd.wanProfile..'.address'

local ourLocalGreAddress = luardb.get(ourLocalGreRdbVar) or ''

--------------------------------------------------
-- queue for processing stuff
local vlanQ = {}

local function addVlanQ(data)
  dinfo('VLAN-Q add : ' .. table.tostring(data))
  table.insert(vlanQ, data)
end

UNID.addQ = addVlanQ

local dhcp82


----------------------------------------------------
-- we keep an in memory table of avcs
-- when we look at the rdb version we comapre to in memory
-- key 'sequence' is useful to tell that changes are needed
--
local avc_tab = {}

--------------------------------------------------
-- Much of the functionality relies on the following assumptions:
--
-- vlan interfaces have the pattern :
--     eth0.x where x is the vlan id e.g. eth0.4 for the switch interface
--        conf.wntd.switchIface..'.'..vid
--     brx where x is the vlan id e.g. br20 for the bridge interface for
--        vlan 20 : conf.wntd.bridgePrefix..vid
--     grex where x is the vlan id e.g. gre20 for the GRE interface for
--        vlan 20 : conf.wntd.grePrefix..vid
-- conf.wntd.wanProfile is the profile index for the radio user APN
---  or use conf.wntd.greAddrVar for the rdb variable name
--
--  CPE layer2 untagged unid 4   >--+
--                        tagged by | AR8327 and placed on trunk
--  +--< UBICOM ubi-eth32 iface  <--+
--  |                 Linux vlan code puts on appropriate interface
--  +--> eth0.4 -->+
--                 |  Layer 2 bridged
--  +--<  br4   <--+
--  |                 Bridge in kernel (L2)
--  +-->  gre4  >--+
--                 |  MPLS GRE Circuit
--      NNI     <--+
--
--      NNI     >--+
--                 |  MPLS GRE Circuit
--  +--<  gre4  <--+
--  |                 Bridge in kernel (L2)
--  +-->  br4   >--+
--                 |  Layer 2 bridged
--  +--< eth0.4 <--+
--  |                 Linux vlan code puts vlan id from interface
--  +--> UBICOM ubi-eth32 iface  >--+
--                       taken from | trunk and untagged by AR8327
--  CPE layer2 untagged unid 4   <--+
--
--
-- The vlans are as follows:
--   in default, priority and dscp mapped mode, the vlans are one to
--   one with respect to unid ports. I.e. Unid port 1 uses vid 1,
--   unid port 2 uses vid 2 etc., up to port and vid 4.
--   For tagged mode, vlan ids are defined as follows:
--     unid port 1 uses vids { 10, 11, 12, 13, 14, 15, 16, 17 }
--     unid port 2 uses vids { 20, 21, 22, 23, 24, 25, 26, 27 }
--     unid port 3 uses vids { 30, 31, 32, 33, 34, 35, 36, 37 }
--     unid port 4 uses vids { 40, 41, 42, 43, 44, 45, 46, 47 }
--   Each of those tagged mode vids is associated with a slot or index
--     slot = vid % 10 + 1
--     unid = integer(vid / 10)
--   The customer vlan id for tagged mode is done on egress from the
--    switch
--
-- Care with using numbers as a result of calculations, and numbers
--    that are strings, e.g. from rdb variables and keys
--    for table keys these are different

-- function deleteMpls(avc)
-- in:
--   avc with entries such as trunk_vid and peer_address
-- action:
--   deletes gre circuit, its interface and bridges
--   also deletes any vlan translation in switch, and that internal trunk_vid
--   is no longer valid in the switch
--
local function deleteMpls(avc)
  local trunk_vid = tonumber(avc.trunk_vid)
  if trunk_vid > 9 then
    local unidId = math.floor(trunk_vid / 10)
    local slot = trunk_vid % 10 + 1
    -- delete the vlan translation table entry
    local arcode = { ['MODE'] = 'set_vlan '..unidId..' '..slot..' 0'}
    addVlanQ(arcode)
  end
  if dhcp82 then
    dhcp82.agent(conf.wntd.grePrefix..trunk_vid, 0, nil)
  end
  local mpcode = { ['MPLS'] = 'stop '..conf.wntd.switchIface..' '..trunk_vid}
  addVlanQ(mpcode)
  
  if(avc.route_entry) then
	local delete_route_entry = true;
	for i, avc_obj in pairs(avc_tab ) do
		if (i ~= ind and avc_obj.route_entry == avc.route_entry) then
			delete_route_entry= false;
			break;
		end
	end
	if delete_route_entry then
		local ecode = { ['EXEC'] = 'route del '.. avc.route_entry}
		addVlanQ(ecode)
	end
	avc.route_entry=nil;
  end
end

-- function deleteAllMpls()
-- action:
--   deletes all gre circuits, their interface and bridges
--   also deletes any vlan translations in switch, and that internal trunk_vid
--   is no longer valid in the switch
local function deleteAllMpls()
  local mpls_tab = GRE.getGreVlans()
  -- key will be trunk_vid
  -- 'trunk_vid',    -- tag used on trunk (e.g. 10 to 17, 20 to 27)
  if conf.wntd.debug > 2 then
    print('mplses : '..table.tostring(mpls_tab))
  end
  for k,v in pairs(mpls_tab) do
    if v then deleteMpls(v) end
  end
  local rf_dev = luardb.get('link.profile.'..conf.wntd.wanProfile..'.dev') or 'eth1';
  if rf_dev then
	  ecode = { ['EXEC'] = 'route_del.sh '.. rf_dev}
	  addVlanQ(ecode)	  
  end

end

-- function readdressAllMpls(newAddress)
-- action:
--   sets all gre circuits to use a new local ip address
local function readdressAllMpls()
  local mpls_tab = GRE.getGreVlans()
  -- key will be trunk_vid
  -- 'trunk_vid',    -- tag used on trunk (e.g. 10 to 17, 20 to 27)
  if conf.wntd.debug > 2 then
    print('mplses : '..table.tostring(mpls_tab))
  end
  for k,v in pairs(mpls_tab) do
    GRE.reAddrGreIface(v, ourLocalGreAddress)
  end
end

------------------------------------------------------------
local action = { }

local function execlog(cmd)
  local cmdrd = cmd..' >/dev/null'
  if conf.wntd.debug > 1 then
    luasyslog.log('LOG_INFO', 'VLANQ about to '..cmd)
    cmdrd = cmd
  end
  if os.execute(cmdrd) ~= 0 then
    luasyslog.log('LOG_ERR', 'VLANQ failed to '..cmd)
    print('VLANQ failed to '..cmd)
  end
end

function action.CODES(v)

  execlog('ar_codes -f ' ..v.. '|' .. conf.wntd.switchConfig)
end

function action.EXEC(v)
	execlog(v);
end

function action.MIBD(v)
  if conf.wntd.mibDaemon then
    execlog(conf.wntd.mibDaemon)
  end
end

function action.MODE(v)
  execlog(conf.wntd.set_unid..' do "'..conf.wntd.switchConfig..'" '..v)
end

function action.CONFIG(v)
  execlog(v..' | '..conf.wntd.switchConfig)
end

function action.MPLS(v)
  execlog('hw-mpls-vcnet.sh ' ..v)
end

function action.QDISC(v)
  execlog('qd-cos.sh ' ..v)
end

function action.NEWADDR(v)
  ourLocalGreAddress = luardb.get(ourLocalGreRdbVar) or ''
  if string.len(ourLocalGreAddress) < 5 then
    luasyslog.log('LOG_ERR', 'VLANQ NEWADDR no new address')
    return
  end
  if conf.wntd.debug > 1 then
    luasyslog.log('LOG_INFO', 'VLANQ about to NEWADDR')
  end
  readdressAllMpls()
end

local function changedQueueProc()
  -- wait for changes
  while true do
    data = table.remove(vlanQ,1)
    if not data then
      break
    end
    for k, v in pairs(data) do
      dinfo('VLAN-Q :' .. k .. ' -> ' .. v)
      if not action[k] then
        error('Do not know how to do action "' .. k .. '".')
      end
      action[k](v)
    end
  end
end
-------------------------------------------------
-- Names used by hw-mpls-vcnet
--
local function maptagnames(value)
  local mapper = {
    ['DefaultMapped'] = 'default',
    ['PriorityTagged'] = 'priority',
    ['DSCPMapped'] = 'dscp',
    ['Tagged'] = 'vlanpri',
    ['TaggedCos'] = 'vlanpri',
    ['Transparent'] = 'default'
  }
  if not mapper[value] then
    luasyslog.log('LOG_ERR', 'AVC-D maptagnames invalid ' .. value)
    return 'default'
  end
  return mapper[value]
end

local function mapcos(value)
  local mapper = {
    ['0'] = '0',
    ['5'] = '5',
  }
  if not mapper[value] then
    luasyslog.log('LOG_ERR', 'AVC-D maptagnames invalid ' .. value)
    return '0'
  end
  return mapper[value]
end

local function mapdscpcos(avc)
  local str = ''
  local mapper = {
    ['cos_to_dscp'] = '--costodscp',
    ['pcp_to_cos']  = '--pcptocos',
    ['dscp_to_cos'] = '--dscptocos',
    ['exp_to_cos']  = '--exptocos',
    ['cos_to_pcp']  = '--costopcp',
    ['cos_to_exp']  = '--costoexp'
  }
  for k, v in pairs(mapper) do
    if avc[k] then
      str = str .. ' ' .. v .. ' "' .. avc[k] ..'"'
    end
  end
  return str
end

local function complain(v, m, avc)
  if not v then
    luasyslog.log('LOG_ERR', 'AVC '..m)
    if avc and avc.ind then
      local prefix = conf.wntd.avcPrefix..'.'..avc.ind..'.'
      luardb.set(prefix..'lasterr', m)
      luardb.set(prefix..'status', 'Error')
    end
    return true
  end
  return false
end

local function avcstatusgood(avc, status)
  if avc and avc.ind then
    local prefix = conf.wntd.avcPrefix..'.'..avc.ind..'.'
    luardb.set(prefix..'lasterr', '')
    luardb.set(prefix..'status', status)
  end
end

local function defaultnumber(n, v)
  return tonumber(n) or v
end

local function qdratestring(avc)
  local str = ''
  local rates = {
    ['tc1_cir'] = {'--tc1', 150000},
    ['tc2_cir'] = {'--tc2', 0 },
    ['tc3_cir'] = {'--tc3c',0 },
    ['tc3_pir'] = {'--tc3p', 0 },
    ['tc4_pir'] = {'--tc4', 1000000} }
  for k, v in pairs(rates) do
    local var = v[1]
    local val = v[2]
    local r = tonumber(avc[k]);
    if not r then
      r = luardb.get('service.mplsd.' .. k)
    end
    if not r then
     r = defaultnumber(conf.wntd[k], val)
    end
    if r then
       str = str .. ' ' .. var .. ' ' .. r .. ' '
    end
  end
  return str
end

local trunk_tab = {}

--------------------------------------------------------------
-- function addMpls(trunk_vid, avc_tab)
-- in:
--   trunk_vid as number
-- inout:
--   avc as table
--     in:
--      peer_address
--      mpls_tag
--      unid (must be same as calculated!)
--      vid for tagged mode
--     out:
--      trunk_vid      -- tag used on trunk (e.g. 10 to 17, 20 to 27)
--      slot           -- number from 1 to 8 representing slot (or 0)
-- action:
--   queue commands to set up switch vlan (assert the unid port
--                 in correct mode)
--   queue commands to set up gre/mpls circuit
--   add extra fields to avc
--
local function addMpls(ind, trunk_vid, avc)
  if complain(avc.peer_address, 'ind '..ind..' trunk '..trunk_vid..' peer_address', avc) then return end
  if complain(avc.mpls_tag, 'ind '..ind..' trunk '..trunk_vid..' mpls_tag', avc) then return end
  if string.len(ourLocalGreAddress) < 5 then
    complain(false, 'addMpls: no local interface GRE address', avc)
    return
  end
  local unidId = trunk_vid
  local slot = 0
  local mode = 'default'
  if trunk_vid > 9 then
    if complain(avc.vid, 'ind '..ind..' trunk '..trunk_vid..' vid', avc) then return end
    local nvid = tonumber(avc.vid)
    if complain(nvid > 0 and nvid < 4096, 'ind '..ind..' trunk '..trunk_vid..' nvid', avc) then return end
    unidId = math.floor(trunk_vid / 10)
    slot = trunk_vid % 10 + 1
    mode = 'tagged'
  end
  local unid
  local ret, msg = pcall(UNID.getById, unidId)
  if ret then
     unid = msg
  else
      complain(false, 'addMpls: no UNID trunk '..trunk_vid, avc)
      return nil
  end
  avc.trunk_vid = trunk_vid
  avc.slot = slot
  if complain(tonumber(avc.unid) == unidId, 'ind '..ind..' trunk '..trunk_vid..' unidId', avc) then return end
  -- in case port is in wrong mode
--  local unidMode = unid:getMapTag()
--  if avc.unidMode ~= unidMode or mode ~= unidMode then
--      complain(false, 'addMpls mode or unidMode unid port in wrong mode '..trunk_vid, avc)
--      return nil
--  end
  local rf_dev = luardb.get('link.profile.'..conf.wntd.wanProfile..'.dev') or '';
  if  string.len(rf_dev)  ==0 then 
      complain(false, 'addMpls: Invalid link.profile.'..conf.wntd.wanProfile..'.dev')
	return nil
  end

  -- add/set the vlan translation table entry
  if trunk_vid > 9 then
    local arcode = { ['MODE'] = 'set_vlan '..unidId..' '..slot..' '..avc.vid}
    addVlanQ(arcode)
  end
  local tagging = unid:getTagging()
  local maptagname = maptagnames(tagging)
  local cos = '0'
  if avc.cos and string.len(avc.cos) then
          cos = mapcos(avc.cos)
  end
  local qdisc = conf.wntd.grePrefix..trunk_vid..' '..maptagname..' '..cos

  local flags = ' --mode '..maptagname
  local service = ''
  if avc.pcp_to_dscp and string.len(avc.pcp_to_dscp) then
    flags = flags .. ' --pcpmap ' .. avc.pcp_to_dscp
  end
  if (conf.wntd.debug > 0) or (avc.debug and avc.debug > 0) then
    qdisc = qdisc .. ' --debug'
  end
  local mappings = mapdscpcos(avc)
  if (conf.wntd.debug > 2) or (avc.debug and avc.debug > 0) then
    flags = flags .. ' --debug'
  end
  -- if avc.dscp and string.len(avc.dscp) then
  --   flags = flags .. ' --dscp ' .. avc.dscp
  -- end
  flags = flags .. ' --cos ' .. cos
  if dhcp82 and avc.avcid and string.len(avc.avcid) > 0 then
    dhcp82.agent(conf.wntd.grePrefix..trunk_vid, trunk_vid, avc.avcid)
    service = '--service '..avc.avcid
  end
  
  local ecode = { ['EXEC'] = 'route add '.. avc.peer_address.. ' ' .. rf_dev}
  addVlanQ(ecode)
  avc.route_entry=avc.peer_address;

  local mpcode = { ['MPLS'] = 'start '..conf.wntd.switchIface..' '..trunk_vid..' '..ourLocalGreAddress..' '..avc.peer_address..' '..avc.mpls_tag..' '..flags..' '..service .. ' ' .. mappings}
  addVlanQ(mpcode)
  --local prefix = conf.wntd.avcPrefix..'.'..avc.ind..'.'

  local qdcmd = { ['QDISC'] = 'start '..qdisc..' '.. qdratestring(avc)}
  addVlanQ(qdcmd)

 local frame_size = forcenumber(luardb.get('unid.max_frame_size'))
  if frame_size >= 512 and frame_size <= 1944 then
	frame_size = frame_size -8; -- delete crc and ignore the vlan-id
	local wntu_mtu = frame_size -14;
	-- here framesize will be MTU size for all UNID interface, eth0.xxx etc
	-- MTU is same for defaultMapped, Tagged,etc
	--defaultMapped mode has 4 bytes less than tagged mode
	--At default mode, if user send packet size bwteen MTU-4, MTU.
	-- packet will be fragmented inside WNTD, to avoid this, softgre will
	-- drop this kind of packet
--[[
	if tagging ~= 'Tagged' then
		framesize = framesize -4; -- delete room for vlan
	end
--]]
	local ecode = { ['EXEC'] = 'ifconfig ' .. conf.wntd.switchIface..'.'..trunk_vid..' mtu '.. wntu_mtu }
	addVlanQ(ecode)
	--local gre_framesize = wntu_mtu +14 +20 -- wntd_mtu +14 +20(ip header), 1956
	local ecode = { ['EXEC'] = 'ifconfig ' .. conf.wntd.grePrefix ..trunk_vid..' mtu '.. wntu_mtu}
	addVlanQ(ecode)
  end


  trunk_tab[tonumber(trunk_vid)] = tostring(ind)
  return 1
end

----------------------------------------------------------------
-- set up the free pool of trunk vlan ids
-- we keep a table of free vlans like this:
-- all keys and values are numbers
--  unidVlans = {
--    [1] = { 10, 11, 12, 13, 14, 15, 16, 17 },
--    [2] = { 20, 21, 22, 23, 24, 25, 26, 27 },
--    [3] = { 30, 31, 32, 33, 34, 35, 36, 37 },
--    [4] = { 40, 41, 42, 43, 44, 45, 46, 47 },
--   }
local unidVlans = {}

local function initFreeVlans()
  for unidId = 1,conf.wntd.unidCount do
    local vids = {}
    local vid = unidId * 10
    for slot = 1,8 do
       table.insert(vids, vid + slot -1)
    end
    unidVlans[unidId] = vids
  end
end

local function findFreeVlan(unidId)
    local vids = unidVlans[unidId]
    return table.remove(vids, 1)
end

local function addFreeVlan(vid)
  if vid > 9 and vid < 50 then
    local unidId = math.floor(vid / 10)
    local vids = unidVlans[unidId]
    table.insert(vids, vid)
  end
end

----------------------------------------------------------------
local function makeAvc(ind, avc)
  if complain(avc.peer_address, 'makeAvc: peer_address', avc) then
    return nil
  end
  if complain(avc.mpls_tag, 'makeAvc: mpls_tag', avc) then
    return nil
  end
  local unidId = tonumber(avc.unid)
  if complain((unidId > 0 and unidId < 5), 'makeAvc: unidId value '..unidId, avc) then
    return nil
  end

  local unid
  local ret, msg = pcall(UNID.getById, unidId)
  if ret then
    unid = msg
  else
    complain(false, 'makeAvc no UNID '..ind..' '..avc.unid, avc)
    return nil
  end

  local trunk_vid = unidId

  -- check if vid has value, then could be tagged mode
  local mtMode = unid:getMapTag()
  avc['mtMode'] = mtMode

  if mtMode == 'default' then
    avc['vid'] = unidId
  elseif mtMode == 'tagged' then
    if complain(avc['vid'], 'makeAvc: avc[vid]', avc) then
      return nil
    end
    local nvid = tonumber(avc.vid)
    if complain((nvid > 0 and nvid < 4096), 'makeAvc: avc.vid value 1..4095: '..avc.vid, avc) then
      return nil
    end
    trunk_vid = findFreeVlan(unidId)
    -- no free trunk vlans
    if complain(trunk_vid, 'makeAvc no free vlans avc:'..ind..' uni:'..avc.unid..' vid:'..avc.vid, avc) then
      return nil
    end
  else
      complain(false, 'makeAvc unid mode not correct avc:'..ind..' uni:'..avc.unid..' mtMode:'..mtMode, avc)
      return nil
  end
  if (not addMpls(ind, trunk_vid, avc)) and (trunk_vid > 9) then
    -- return trunk_vid to free pool
    addFreeVlan(trunk_vid)
  end
  avcstatusgood(avc, 'Up')
  return avc
end

-----------------------------------------------------
-- initialisation
-- get all existing mpls circuits, delete them
-- init the pool of free tagged vlan ids
-- gather all rdb avc info into table, keys are the 'i' of avc.{i}
--
local function avcInit()
  deleteAllMpls()
  initFreeVlans()
  if string.len(ourLocalGreAddress) < 5 then
    luasyslog.log('LOG_ERR', 'avcInit - Invalid or no IP address in WWAN profile: ' .. ourLocalGreAddress)
    return
  end
  avc_tab = rdbtable2(conf.wntd.avcPrefix)
  if conf.wntd.debug > 2 then
    print('initial avc_tab : '..table.tostring(avc_tab))
  end
  for k,v in pairs(avc_tab) do
    if string.match(k, "^%d+$") then
      v.ind = k
      makeAvc(k,v)
    else
      avc_tab[k] = nil
    end
  end
end

local function avcPrint()
    print('avc : '..table.tostring(avc_tab))
    print('freeVlans : '..table.tostring(unidVlans))
end

-----------------------------------------------------
-- check if avc has changed or been deleted
-- changed, we delete first
-- ind as string
local function checkAvcDeletion(ind)
  if not avc_tab[ind] then
    return
  end
  local avc = avc_tab[ind]

  local sequence = luardb.get(conf.wntd.avcPrefix .. '.' .. ind .. '.sequence')

  if not avc['sequence'] or not sequence then
    if avc and avc['unid'] and avc['trunk_vid'] then
      trunk_tab[tonumber(avc.trunk_vid)] = nil
      deleteMpls(avc)
      addFreeVlan(avc.trunk_vid)
    end
    avc_tab[ind] = nil
    return true
  end
  local avcrdb = rdbtable(conf.wntd.avcPrefix..'.'..ind)
  if not avcrdb or not avcrdb['sequence'] then
    return
  end
  if avcrdb.sequence == avc.sequence then
  -- assume it is the same, false change
    return
  end
  if avc and avc['unid'] and avc['trunk_vid'] then
    trunk_tab[tonumber(avc.trunk_vid)] = nil
    avcstatusgood(avc, 'Disabled')
    deleteMpls(avc)
    addFreeVlan(avc.trunk_vid)
  end
  avc_tab[ind] = nil
  return true;
end

-----------------------------------------------------
-- check if avc has been added
-- sanity check for changed, as there is a small window for changed
-- ind as string
local function checkAvcAddition(ind)
  local avc = rdbtable(conf.wntd.avcPrefix..'.'..ind)
  if not avc or not avc['sequence'] then
    return
  end
  --print("checkAvcAddition "..ind)
  if avc_tab[ind] then
    if( checkAvcDeletion(ind) ) then
		--print("avc ".. ind.." is removed")
		avc_tab[ind] = nil
    end
  end
  -- do not start if not enabled
  avc.ind = ind
  if not tonumber(avc['enable']) or (tonumber(avc['enable']) < 1) then
    avcstatusgood(avc, 'Disabled')
    return
  end
  if not avc_tab[ind]  and makeAvc(ind, avc) then
	--print("avc ".. ind.." is recreated")
  	avc_tab[ind] = avc
  end
end

local function avcChangedCB()
  if string.len(ourLocalGreAddress) < 5 then
  	ourLocalGreAddress = luardb.get(ourLocalGreRdbVar) or ''
	if string.len(ourLocalGreAddress) < 5 then
    		luasyslog.log('LOG_ERR', 'avcChangedCB - Invalid or no IP address in WWAN profile: ' .. ourLocalGreAddress)
    		return
	end
  end
  lock_state.lock()
  local ids = luardb.get(conf.wntd.avcPrefix .. '.changed')
  -- check deleted first, as we may need the trunk_vid for
  -- an addition
  -- Deleted also accounts for changed, as we delete those
  -- that have changed first before adding
  -- changed may release a trunk_vid for one unid
  --   before claiming one for another
  if ids:len() > 0 then
    for _, ind in ipairs(ids:explode(',')) do
      if string.match(ind, "^%d+$") then
        checkAvcDeletion(ind)
      end
    end
    lock_state.unlock()
    -- allow processing to take place
    changedQueueProc()
    lock_state.lock()
  end
  ids = luardb.get(conf.wntd.avcPrefix .. '.changed')
  if ids:len() > 0 then
    luardb.set(conf.wntd.avcPrefix .. '.changed', '')
    for _, ind in ipairs(ids:explode(',')) do
      if string.match(ind, "^%d+$") then
        checkAvcAddition(ind)
      end
    end
    luardb.set(conf.wntd.avcPrefix .. '.changed', '')
    lock_state.unlock()
    changedQueueProc()
  else
    lock_state.unlock()
  end
end

---------------------------------------------------------
-- unid processing

-- create instances
local unids = {}

local function unidInit()

  luasyslog.log('LOG_INFO', '[mplsd::unidInit] Number of available UNI-Ds: ' .. conf.wntd.unidCount)

  for id = 1,conf.wntd.unidCount do
    -- pcall( UNID.getById(id) )
    local ret, msg = pcall(UNID.getById, id)
    if ret then
      unids[id] = msg
      dinfo('UNI-D ' .. id .. ': initialised OK.')
    else
      luasyslog.log('LOG_ERR', 'UNI-D ' .. id .. ': initialisation failed: ' .. msg)
      os.exit(1) -- we exit at this point, not much else we can do...
    end
  end
end

local function unidPrint()
    print('unids : '..table.tostring(unids))
end



local function setMaxFrSize(update_gre)
  local cmd = conf.wntd.prefixSetSize
  if not cmd or cmd:len() < 2 then
    return
  end
  local framesize = forcenumber(luardb.get('unid.max_frame_size'))
  if framesize < 512 or framesize > 1944 then
    framesize =1522
  end
  --framesize = framesize -4 --wombat need count vlan-id as packet size
  -- because priority mode need a tag to unid
  local ccode = { ['CONFIG'] = 'echo ' .. cmd .. framesize }
  addVlanQ(ccode)
  local wntd_framesize = framesize -8 -- remove crc and ignore the vlan,
  local wntd_mtu = wntd_framesize - 14  -- payload size, eg 1922
  local gre_framesize = wntd_mtu +14 +20 -- wntd_mtu +14 +20(ip header). 1956
  local rf_framesize = gre_framesize + 8  --wntd_mtu +14+ IP header (20) + GRE header (4) + MPLS header (4), 1964
  local base_rf_framesize = rf_framesize + 4  -- + 4 for 8021q

  -- check the minimum mtu size requirement for the rf interface.
  if rf_framesize < 1500 then
	rf_framesize = 1500
  end

  dinfo('mtu wntd='..wntd_mtu, 'rf='..rf_framesize)

  local ecode = { ['EXEC'] = 'ifconfig ' .. conf.wntd.switchIface ..' mtu '.. wntd_mtu}
  addVlanQ(ecode)
  local base_rf_dev = luardb.get('link.profile.'..conf.wntd.wanProfile..'.dev_base')
  if base_rf_dev and base_rf_dev:len() > 0 then
    ecode = { ['EXEC'] = 'ifconfig '.. base_rf_dev..'  mtu '.. base_rf_framesize}
    addVlanQ(ecode)
  end

  local rf_dev = luardb.get('link.profile.'..conf.wntd.wanProfile..'.dev') or 'eth1';
  ecode = { ['EXEC'] = 'ifconfig '.. rf_dev..'  mtu '.. rf_framesize}
  addVlanQ(ecode)
  for _, obj in pairs(unids) do
	local ecode = { ['EXEC'] = 'ifconfig ' .. conf.wntd.switchIface ..'.'..obj.id..' mtu '.. wntd_mtu}
	addVlanQ(ecode)
  end

  if update_gre then
    for _, avc in pairs(avc_tab) do
      if forcenumber(avc.trunk_vid) ~= 0 then
        local ecode = { ['EXEC'] = 'ifconfig ' .. conf.wntd.grePrefix .. avc.trunk_vid .. ' mtu '.. wntd_mtu}
        addVlanQ(ecode)
        ecode = { ['EXEC'] = 'ifconfig ' .. conf.wntd.switchIface ..'.' .. avc.trunk_vid .. ' mtu '.. wntd_mtu}
        addVlanQ(ecode)
      end
    end
  end
end


local function collect_tag_changed()
  local tag_changed = 0;
  local port_map = "";
  local pri_tag_ports = "";
  local ccode;
  for _, unid in ipairs(unids) do
    if unid.tag_changed ~= 0 then
       tag_changed = tag_changed + 1
       unid.tag_changed = 0;
    end
  end
  if tag_changed >0 then
    for id, unid in ipairs(unids) do
      if unid.tagging =='PriorityTagged'  then 
        port_map = port_map .. " " ..id;
        pri_tag_ports = pri_tag_ports .. " " ..id;
	-- set the window parsing register, l2 starts after SA and goes for four bytes
        ccode = { ['CODES'] = 'win_ctl '.. tostring(id +1) .. ' l2_offset 12 l2_length 4' }
        addVlanQ(ccode)
      elseif unid.tagging == 'Tagged' or 
        unid.tagging =='TaggedCos' then
        port_map = port_map .. " " ..id;
      end
    end
    --luasyslog.log('LOG_INFO', 'pfilter port map'..port_map)
    -- this must be extended for TC 2 and 3
    ccode = { ['CODES'] = 'pfilter ' .. port_map .." p 0 5" }
    addVlanQ(ccode)

	-- for ports that are priority tagged, reject packets that are not 802.1q encoded
	if #pri_tag_ports > 0 then
		ccode = { ['CODES'] = 'winfilter '.. pri_tag_ports .. ' p 0x81,0 m 0xff,0xff,0x0f,0xff' }
	else
	-- else disable all win rules
		ccode = { ['CODES'] = 'winfilter 0' }
	end
	addVlanQ(ccode)
  end
end

local function unidChangedCB()
  lock_state.lock()
  local ids = luardb.get(conf.wntd.unidPrefix .. '.changed')
  if ids:len() > 0 then
    luardb.set(conf.wntd.unidPrefix .. '.changed', '')
    for _, id in ipairs(ids:explode(',')) do
      if string.match(id, "^%d+$") then
        id = tonumber(id)
        local unid = unids[id]
        if unid then
          local ret, msg = pcall(unid.pollChange, unid)
          if ret then
            dinfo('UNI-D ' .. unid.id .. ': changed OK.')
          else
            luasyslog.log('LOG_ERR', 'UNI-D ' .. unid.id .. ': change poll failed: ' .. msg)
          end
        else
          luasyslog.log('LOG_ERR', 'Change notification for unknown UNI-D ID: ' .. id)
        end
      elseif id == 'g' then
        setMaxFrSize(true)
      end
    end
    luardb.set(conf.wntd.unidPrefix .. '.changed', '')
    collect_tag_changed();
    lock_state.unlock()
    changedQueueProc()

  else
    lock_state.unlock()
  end
end


----------------------------------------------------------
-- whole daemon processing
--
local function changedCB()
  -- very important to do unids first, in case mode has changed
  unidChangedCB()
  avcChangedCB()
end

local mibPollInterval

local function mibTimerCB()
  local interval
  interval = forcenumber(luardb.get('service.mibd.pollinterval'))
  if interval < 5 then
    interval = forcenumber(conf.stats.unidMibPollInterval)
    if interval < 5 then
      interval = 5
    end
  end
  if conf.wntd.mibDaemon and conf.stats.mibListener then
    local cmd = 'mib '
    if mibPollInterval then
      cmd = cmd .. 'reset 0 '
    end
    cmd = cmd .. 'time '.. interval
    mibPollInterval = interval
    local mibstart = {['CODES'] = cmd }
    addVlanQ(mibstart)
    changedQueueProc()
  end
end

local function initDhcp82()
  local code = loadfile(us..'dhcp82.lua')
  if complain(code, 'dhcp82.lua failed to load', nil) then return end
  dhcp82 = code()
  local han, cmd = dhcp82.init()
  if not han then
    luasyslog.log('LOG_ERR', 'DHCP82 failed to start '..cmd)
    return
  end
end

mibTimerCB()
initDhcp82()
unidInit()
avcInit()

setMaxFrSize()

changedQueueProc()

local function wanProfileAddrChangedCB()
  local comeGood = string.len(ourLocalGreAddress) < 5
  ourLocalGreAddress = luardb.get(ourLocalGreRdbVar) or ''
  if string.len(ourLocalGreAddress) < 5 then
    return
  end
  local code = { ['NEWADDR'] = 'doit' }
  addVlanQ(code)
  changedQueueProc()
  if comeGood then
    changedCB()
  end
end

 
 
-- lock the rdb
luardb.lock()

-- we watch the "<avcPrefix>.changed" variable
-- the TR-069 client will put a comma seperated list
-- of AVC IDs which have been changed into this variable
luardb.watch(conf.wntd.avcPrefix .. '.changed', changedCB)

-- we watch the "<unidPrefix>.changed" variable
-- the TR-069 client will put a comma seperated list
-- of UNI-D IDs which have been changed into this variable
luardb.watch(conf.wntd.unidPrefix .. '.changed', unidChangedCB)
luardb.watch(ourLocalGreRdbVar, wanProfileAddrChangedCB)

-- unlock the rdb
luardb.unlock()

-- pick up any changes in avc before watch.
wanProfileAddrChangedCB()

luardb.set(conf.wntd.unidPrefix .. '.changed', '1,2,3,4')

luardb.watch(conf.wntd.avcPrefix .. '.show', avcPrint)

luardb.watch(conf.wntd.unidPrefix .. '.show', unidPrint)

local waitInterval

local function intervalChangedCB()
  waitInterval = forcenumber(luardb.get('service.mplsd.pollinterval'))
  if waitInterval < 5 then
    waitInterval = forcenumber(conf.wntd.unidStatusPollInterval)
    if waitInterval < 5 then
      waitInterval = 30
    end
  end
end

intervalChangedCB()   -- set to an initial value

luardb.watch('service.mplsd.pollinterval', intervalChangedCB)
luardb.watch('service.mibd.pollinterval', mibTimerCB)

-- wait for changes

-- require('debugger')        --load the debug library

local findq1 = {
  ['qdisc'] = { '^qdisc %S+ %S+ dev '..conf.wntd.grePrefix..'(%d+)'  },
}

local findqd = {
  ['sent']  = { '%s*Sent (%d+) bytes ',
		'%s*Sent %d+ bytes (%d+) pkt',
		'%s*Sent %d+ bytes %d+ pkt %pdropped (%d+),' },
}

local function dofinder(s, ftab)
  local t = {}
  for k,v in pairs(ftab) do
    for i,m in ipairs(v) do
      local word
      word = s:match(m)
      if word then
        table.insert(t, word)
      end
    end
    if #t > 0 then return k, t end
  end
  return nil, nil
end

local collect

local function gen_collect()
  local self = {}
  local fileRollPort  = conf.stats.fileRollPort or 2220
  local client = assert(loadfile(us..'statsclient.lua'))
  local clo = client()
  clo.setport(fileRollPort)

  local data
  self.start = function()
     lock_state.lock()
     data = 'NEWDATA\n'
  end
  self.set = function(k,v)
     luardb.set(k, v)
     data = data..k..' '..v..'\n'
  end
  self.commit = function()
    lock_state.unlock()
    -- print('About to send :'..data)
    if data then
      local len = string.len(data)
      if len > 0 then
        clo.sendbuf(data, len)
        data = ''
      end
    end
  end
  return self
end

local avcQdiscStatsPrefix = conf.stats.avcQdiscStatsPrefix or 'qdStats'

local function readQdisc(cmd)
  local ind
  local avc
  local dosearch, dodata
  local qdreceived = {}
  local qddropped = {}
  local f = io.popen(cmd, 'r')
  if not f then return end

  function dosearch()
    while true do
      local s = f:read('*l')
      if not s then return end
      local m, t = dofinder(s, findq1)
      if m then
        ind = trunk_tab[tonumber(t[1])]
        if ind then
          avc = avc_tab[ind]
          if avc then return dodata() end
        end
      end
    end
  end

  function dodata()
    while true do
      local s = f:read('*l')
      if not s then return end
      local m, t = dofinder(s, findqd)
      --print('pkt match '..tostring(pkt)..' string is '..s)
      if m then
        local prefix = conf.wntd.avcPrefix..'.'..ind..'.'..avcQdiscStatsPrefix..'.'
        collect.start()
        -- use the root QDisc stats only (assumed that the first one on the list is the root)
        if not qdreceived[ind] then
           collect.set(prefix..'stamp', os.time());
           collect.set(prefix..'bytes', t[1]);
           collect.set(prefix..'frames', t[2]);
           qdreceived[ind] = t[2]	
        end
        -- Special case for the dropped frame stats.
        -- add up all of the children QD stats.
        if not qddropped[ind] then  qddropped[ind] = 0 end
        qddropped[ind] = qddropped[ind] + t[3]
        collect.set(prefix..'dropped', qddropped[ind]);
        collect.commit()
        return dosearch()
      end
    end
  end
  dosearch()
  f:close()
end

local avcIfGreStatsPrefix = conf.stats.avcIfGreStatsPrefix or 'greStats'
local avcIfSwitchStatsPrefix = conf.stats.avcIfSwitchStatsPrefix or 'swStats'

local findl1 = {
  ['gre'] = { '^'..conf.wntd.grePrefix..'(%d+) '},
  ['eth'] = { '^'..conf.wntd.switchIface..'.(%d+) ' },
}

local findd = {
  ['rxp'] = {'%s*RX packets:(%d+)'},
  ['txp'] = {'%s*TX packets:(%d+)'},
  ['bytes'] = {'%s*RX bytes:(%d+)','TX bytes:(%d+)'},
}

local function readIface()
  local ind
  local avc
  local prefix
  local res
  local f = io.popen('ifconfig', 'r')
  if not f then return end
  local dosearch, dodata

  -- two recursive tail call functions
  function dosearch()
    while true do
      prefix = nil
      local s = f:read('*l')
      if not s then return end
      local m, t = dofinder(s, findl1)
      if m == 'gre' then
        prefix = avcIfGreStatsPrefix..'.'
      elseif m == 'eth' then
        prefix = avcIfSwitchStatsPrefix..'.'
      end
      -- print('ifconfig match '..tostring(v)..' string is '..s)
      if m and prefix then
        ind = trunk_tab[tonumber(t[1])]
        if ind then
          avc = avc_tab[ind]
          prefix = conf.wntd.avcPrefix..'.'..ind..'.'..prefix
          if avc then return dodata() end
        end
      end
    end
  end

  function dodata()
    res = {}
    while true do
      local s = f:read('*l')
      if not s then return end
      local m, t = dofinder(s, findd)
      if m == 'rxp' then res['rxpkt'] = t[1]
      elseif m == 'txp' then res['txpkt'] = t[1]
      elseif m == 'bytes' then
        res['rxbytes'] = t[1]
        res['txbytes'] = t[2]
        collect.start()
        collect.set(prefix..'stamp', os.time());
        collect.set(prefix..'rxpkt', res['rxpkt']);
        collect.set(prefix..'txpkt', res['txpkt']);
        collect.set(prefix..'rxbytes', res['rxbytes']);
        collect.set(prefix..'txbytes', res['txbytes']);
        collect.commit()
        return dosearch()
      end
    end
  end
  dosearch()
  f:close()
  return s
end

collect = gen_collect()

-- Get the hardware version
hwplatform = luardb.get('version.hardware')

while true do
  luardb.wait(waitInterval)
  if hwplatform == '1.2' then
  	readQdisc('LD_PRELOAD=/lib/libgcc_s.so tc -s qdisc show')
  else
  	readQdisc('tc -s qdisc show')
  end
  readIface()
--   pause('MPLS debug')      --start/resume a debug session

--   changedCB()
-- end
--
-- local function trueloop()
  UNID.pollLinkStatus()
  lock_state.lock()
  for _, unid in ipairs(unids) do
    local ret, msg = pcall(unid.pollStatus, unid)
  end
  lock_state.unlock()
end
