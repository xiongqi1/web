local function popenr(cmd)
  local tmpname = os.tmpname()
  os.execute(cmd..'>'..tmpname)
  local l = ''
  for line in io.lines(tmpname) do
    l = l..line
  end
  return l
end

local function capture (cmd, raw)
  local f = assert(io.popen(cmd, 'r'))
  local s = assert(f:read('*a'))
  f:close()
  if raw then return s end
  s = string.gsub(s, '^%s+', '')
  s = string.gsub(s, '%s+$', '')
  s = string.gsub(s, '[\n\r]+', ' ')
  return s
end

local function _getGreIfaces()
  local devs={}

  for Line in io.lines('/proc/net/dev') do
    local v=Line:match('^%s*'..conf.wntd.grePrefix..'(%d+):')
    if v~=nil then 
      -- print('add ' .. k)
      devs[v]={}
    end
  end

  if conf.wntd.debug > 2 then
    for k, v in pairs(devs) do
      print('vlan '..k)
    end
  end
  return devs
end

--        conf.wntd.set_unid -- /usr/bin/set_unid.sh'
--        conf.wntd.switchConfig -- 'eth-reg-set eth0.4000 -a -'
local function _getGreVlans()
  local vlans=_getGreIfaces()

  for k, v in pairs(vlans) do
    -- 'vc20 local 192.168.0.70 remote 192.168.0.100 label 4000 flags 0x3 mpls-ttl 64
    local ipLine=capture('ip vcconf '..conf.wntd.grePrefix..k)
    if conf.wntd.debug > 2 then
       print('ip vcconf result : '..ipLine)
    end
    local tag = ipLine:match('label%s+(%d+)')
    local ouraddr = ipLine:match('local%s+(%S+)')
    local peer = ipLine:match('remote%s+(%S+)')
    local n = tonumber(k)
    if tag and peer then
      local gre={}
      gre.trunk_vid = n
      if n < 10 then
        gre.unid = n
        gre.slot = 0
      elseif n < 50 and n > 9 then
        gre.unid = math.floor(n / 10)
        gre.slot = n % 10 + 1
      end
      gre.name = conf.wntd.grePrefix..k
      gre.peer_address = peer
      gre.local_address = ouraddr
      gre.mpls_tag = tag
      vlans[k] = gre
    else
      vlans[k] = nil
    end
  end
  return vlans
end

local function _reAddrGreIface(gre, newAddr)
  if gre.local_address == newAddr then
    return
  end
  os.execute('ip vcconf '..gre.name..' local '..newAddr)
  gre.local_address = newAddr
end

GRE = {
	['getGreVlans'] = _getGreVlans,
	['getGreIfaces'] = _getGreIfaces,
	['reAddrGreIface'] = _reAddrGreIface
}

return GRE
