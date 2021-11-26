require('luardb')
require('luasyslog')
require('tableutil')

local function _maptagged(value)
	local mapper = {
		['DefaultMapped'] = 'default',
		['PriorityTagged'] = 'default',
		['DSCPMapped'] = 'default',
		['Tagged'] = 'tagged',
		['TaggedCos'] = 'tagged',
		['Transparent'] = 'default'
	}
	if not mapper[value] then
		luasyslog.log('LOG_ERR', 'UNI-D maptagged invalid ' .. value)
		return 'default'
	end
	return mapper[value]
end

local function _validate(self, field, value)
	local validation = {
		['enable'] = { '0', '1' },
		['status'] = { 'Disabled', 'Error', 'NoLink', 'Up' },
		['bitrate'] = { 'Auto', '10', '100', '1000' },
		['duplex'] = { 'Auto', 'Half', 'Full' },
		['tagging'] = { 'DefaultMapped', 'PriorityTagged', 'DSCPMapped', 'Tagged', 'TaggedCos', 'Transparent' },
		['maptag'] = { 'default', 'tagged'}
	}

	-- luasyslog.log('LOG_INFO', 'UNI-D validate : ' .. field .. ' [ ' .. value .. ' ]')
	if not validation[field] then
		self:setError('Do not know how to validate field "' .. field .. '".')
	end
	if not table.contains(validation[field], value) then
		self:setError('The value "' .. value .. '" is not valid for UNI-D ' .. field .. '.')
	end
end

local function _addQ(data)
	luasyslog.log('LOG_ERR', 'UNI-D wrong addQ : ' .. table.tostring(data))
	-- os.execute(data)
end

local function init(self)
	local enable	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.enable') or '0'
	local bitrate	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.bitrate') or 'Auto'
	local duplex	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.duplex') or 'Auto'
	local tagging	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.tagging') or 'DefaultMapped'

	self.tagging = '*Undefined*'
	self.maptag = 'undef'
	self.tag_changed = 0

	-- setup for the 1st time
	-- assume nothing, clober port and reset to known state
	self:setElectrical(enable, bitrate, duplex)
	self:setTagging(tagging)

	local status	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.status') or 'Disabled'
	self.status = status
end

local function setError(self, err)
	luasyslog.log('LOG_ERR', err)
	luardb.set(conf.wntd.unidPrefix .. '.' .. self.id .. '.lasterr', err)
	self.status = 'Error'
end

local function setStatus(self, status)
	-- 'Error' must always validate as _validate() can call setStatus('Error')
 	self:validate('status', status)
	if status == self.status then return end
	if conf.wntd.debug > 1 then
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': status  "' .. self.status .. '" -> "' .. status .. '".')
	end
	self.status = status
	if status ~= 'Error' then
		luardb.set(conf.wntd.unidPrefix .. '.' .. self.id .. '.lasterr', '')
	end
	luardb.set(conf.wntd.unidPrefix .. '.' .. self.id .. '.status', status)
end

local function setElectrical(self, enable, bitrate, duplex)
	if conf.wntd.debug > 1 then
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': setElectrical called enable:'..enable..' bitrate:'..bitrate..' duplex:'..duplex)
	end
	self:validate('enable', enable)
	self:validate('bitrate', bitrate)
	self:validate('duplex', duplex)

	self.enable = enable
	self.bitrate = bitrate
	self.duplex = duplex

	if self.status == 'Error' then return end
	if conf.wntd.debug > 1 then
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': enable := ' .. self.enable)
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': bitrate := ' .. self.bitrate)
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': duplex := ' .. self.duplex)
	end

	local arcode = { ['CODES'] = 'phy ' .. self.id .. ' speed ' .. self.bitrate .. ' duplex ' .. self.duplex .. ' enable ' .. self.enable }
	-- os.execute(cmd)
	UNID.addQ(arcode)
end

local function setTagging(self, tagging)
	self:validate('tagging', tagging)

	if self.status == 'Error' then return end
	if conf.wntd.debug > 1 then
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': tagging ' .. self.tagging .. ' -> ' .. tagging .. '.')
	end
	local map = _maptagged(tagging)
	self.tagging = tagging
	self.tag_changed = 1
	if map ~= self.maptag then
		self:setMapTag(map)
	end
end

local function getTagging(self)
	return self.tagging
end

local function setMapTag(self, mapTag)
	self:validate('maptag', mapTag)
	if self.status == 'Error' then return end
	if conf.wntd.debug > 1 then
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': maptag ' .. self.maptag .. ' -> ' .. mapTag .. '.')
	end
	if mapTag ~= self.maptag then
		self.maptag = mapTag
		local arcode = { ['MODE'] = mapTag .. ' ' .. self.id }
		UNID.addQ(arcode)
	end
end

local function getMapTag(self)
	return self.maptag
end

local function pollChange(self)
	local enable	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.enable') or '0'
	local bitrate	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.bitrate') or 'Auto'
	local duplex	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.duplex') or 'Auto'
	local tagging	= luardb.get(conf.wntd.unidPrefix .. '.' .. self.id .. '.tagging') or 'DefaultMapped'

	if conf.wntd.debug > 1 then
		luasyslog.log('LOG_INFO', 'UNI-D ' .. self.id .. ': pollChange called enable:'..self.enable..' status:'..self.status)
	end
	-- set changed values only
	if self.enable ~= enable or self.bitrate ~= bitrate or self.duplex ~= duplex then
		self:setElectrical(enable, bitrate, duplex)
	end
	if self.tagging ~= tagging then
		self:setTagging(tagging)
	end
end

local _atuget =  {}

local function initAtuGet()
  if _atuget.err then
    return
  end
  local code = loadfile('atuget.lua')
  if not code then
    code = loadfile('/usr/bin/atuget.lua')
  end
  if not code then
    _atuget['err'] = true
    luasyslog.log('LOG_ERR', 'UNID failed to load atuget.lua')
    return
  end
  _atuget['code'] = code()
  if not _atuget['cmd'] then
    _atuget['cmd'] = conf.wntd.switchGetAtu or 'eth-reg-set eth0.4000 -a atudump'
  end
end

local function rdbmacprefix(i)
  return conf.wntd.unidPrefix ..
     '.' ..
     i ..
     '.' ..
     conf.stats.unidSwitchStatsPrefix ..
     '.macs'
end

local function getAtu()
  if _atuget.err or not _atuget.code then
    return
  end
  local t = _atuget.code.getatuvals(_atuget.cmd)
  for p,s in pairs(t) do
    luardb.set(rdbmacprefix(p), s)
  end
end

local function _pollLinkStatus()

  local needAtu = luardb.get('service.mplsd.want_macs') or 0
  if tonumber(needAtu) > 0 and not _atuget.err then
    if not _atuget['code'] then
      initAtuGet()
    end
    getAtu()
  end

  local cmd = conf.wntd.switchLinkStatus
  if not cmd then return end
  local f = io.popen(cmd, 'r')
  if not f then return end
  while true do
    local s = f:read('*l')
    if not s then
      f:close()
      return
    end
    local v0, v1, v2 = s:match('^G_REG 0x8(%x)%s+(0x%x+)%s+(0x%x*)')
    if v0 and v1 and v2 then
      if v0 == '4' then
        luardb.set(conf.wntd.unidPrefix..'.1.stareg', v1)
        luardb.set(conf.wntd.unidPrefix..'.2.stareg', v2)
      elseif v0 == 'c' or v0 == 'C' then
        luardb.set(conf.wntd.unidPrefix..'.3.stareg', v1)
        luardb.set(conf.wntd.unidPrefix..'.4.stareg', v2)
        f:close()
        return
      end
    end
  end
end

local function pollStatus(self)

	local chkhasbit

	function hasbit(x,p) return x % (p + p) >= p end

	if not self.enable or not self.enable == '1' then
		-- administratively down
		self:setStatus('Disabled')
	else
		if self.status == 'Error' then return end
		--- determine link state
		local rstr = luardb.get(conf.wntd.unidPrefix..'.'..self.id..'.stareg')
		local newstatus = 'NoLink'
		if rstr and hasbit(rstr, 0x100) then
			newstatus = 'Up'
		end
		self:setStatus(newstatus)
	end
end

----
-- Class Methods
----
local _instances = {}

local function _getById(id)
	-- validate id
	id = tonumber(id) or 0
	if id < 1 or id > conf.wntd.unidCount then
		error('Invalid UNI-D ID: ' .. id)
	end

	-- find instance
	local instance = _instances[id]
	if not instance then
		-- none already, make one
		instance = {
			['init'] = init,
			['validate'] = _validate,
			['setStatus'] = setStatus,
			['setError'] = setError,
			['setElectrical'] = setElectrical,
			['setMapTag']  = setMapTag,
			['setTagging'] = setTagging,
			['getTagging'] = getTagging,
			['pollChange'] = pollChange,
			['pollStatus'] = pollStatus,
			['getMapTag']  = getMapTag,
		}
		instance.id = id
		instance:init()
		_instances[id] = instance
	end
	return instance
end

UNID = {
	['pollLinkStatus'] = _pollLinkStatus,
	['getById'] = _getById,
	['addQ'] = _addQ
}

return UNID
