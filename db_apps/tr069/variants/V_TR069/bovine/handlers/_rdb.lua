----
-- Copyright (C) 2012 NetComm Wireless Limited.
--
-- This file or portions thereof may not be copied or distributed in any form
-- (including but not limited to printed or electronic forms and binary or object forms)
-- without the expressed written consent of NetComm Wireless Limited.
-- Copyright laws and International Treaties protect the contents of this file.
-- Unauthorized use is prohibited.
--
-- THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
-- NETCOMM WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
-- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
-- SUCH DAMAGE.
----
require('Logger')
require('Daemon')
require('CWMP.Error')
require('Parameter.Validator')
require('luardb')
require('rdbevent')

----
-- Validated RDB Variable <--> TR-069 Parameter Binding
----

local function makeWatcher(node)
	return function(key, value)
		if value ~= nil then
			Logger.log('Parameter', 'debug', 'rdb watcher: change notification for "' .. key .. '"')
			if value ~= node.value then
				Logger.log('Parameter', 'info', 'rdb watcher: "' .. key .. '" changed: "' .. node.value .. '" -> "' .. value .. '"')
				-- validate new RDB value
				local ret, msg = node:validateValue(value)
				if ret then
					-- if it is invalid for this node moan loudly and force it back to default
					-- FIXME: should we really force it, especially for read-only parameters?
					Logger.log('Parameter', 'error', 'rdb watcher: "' .. key .. '" for "' .. node:getPath() .. '": ' .. msg)
					Logger.log('Parameter', 'warning', 'rdb watcher: defaulting to "' .. node.default .. '".')
					luardb.set(node.rdbKey, node.default)
					node.value = value
					return
				end
				client:asyncParameterChange(node, node:getPath(), value)
				node.value = value
			else
				Logger.log('Parameter', 'debug', 'rdb watcher: "' .. key .. '" unchanged from "' .. node.value .. '"')
			end
		else
			Logger.log('Parameter', 'warning', 'rdb watcher: "' .. key .. '" appears to have been deleted.')
		end
	end
end

local function persistNotify(node)
	if node.notify ~= node.defaultNotify then
		luardb.set(node.rdbKey .. '.@notify', node.notify)
		if node.rdbPersist == '1' then
			luardb.setFlags(node.rdbKey .. '.@notify', Daemon.flagOR(luardb.getFlags(node.rdbKey .. '.@notify'), 'p'))
		end
	else
		luardb.unset(node.rdbKey .. '.@notify')
	end
end

return {
	['**'] = {
		init = function(node, name)
			assert(node:isParameter(), name .. ' is not a parameter.')

			-- validate default value consistency
			local ret, msg = node:validateValue(node.default)
			if ret then
				Logger.log('Parameter', 'error', 'rdb.init(' .. name .. ') config default value error: ' .. msg)
				return ret, msg
			end

			-- grab current value from RDB
			node.value = luardb.get(node.rdbKey)
			if node.value == nil or (node.value == '' and node.default ~= '') then
				-- absent from RDB - create it as default value
				Logger.log('Parameter', 'notice', 'rdb.init(' .. name .. '): defaulting rdb variable "' .. node.rdbKey .. '" := "' .. node.default .. '"')
				luardb.set(node.rdbKey, node.default)
				node.value = node.default
			else
				-- validate RDB value for consistency
				Logger.log('Parameter', 'debug', 'rdb.init(' .. name .. '): rdb value "' .. node.value .. '"')
				local ret, msg = node:validateValue(node.value)
				if ret then
					-- maybe we shouldn't die here, but rather default it?
					Logger.log('Parameter', 'error', 'rdb.init(' .. name .. ') rdb "' .. node.rdbKey .. '" value error: ' .. msg)
					return ret, msg
				end
			end

			-- if this is a persistent parameter we force the RDB persist flag
			if node.rdbPersist == '1' then
				luardb.setFlags(node.rdbKey, Daemon.flagOR(luardb.getFlags(node.rdbKey), 'p'))
			end

			-- look for a non-default notification persisted in RDB
			local persistedNotify = luardb.get(node.rdbKey .. '.@notify')
			if persistedNotify then
				persistedNotify = tonumber(persistedNotify)
				if persistedNotify > node.maxNotify then
					Logger.log('Parameter', 'warning', 'rdb.init(' .. name .. '): persisted notify exceeds maximum.')
					persistedNotify = node.maxNotify
				elseif persistedNotify < node.minNotify then
					Logger.log('Parameter', 'warning', 'rdb.init(' .. name .. '): persisted notify is less than minimum.')
					persistedNotify = node.minNotify
				end
				node.notify = persistedNotify
				persistNotify(node)
			end

			-- Always need to set onChange watcher for ManagementServerURL.
			-- ManagementServerURL should trigger "0 BOOTSTRAP" inform event,
			-- whenever the value has been changed.
			--
			-- install watcher for external RDB value changes
			if node.notify > 0 or client.tree:isManagementServerURL(name) then
				Logger.log('Parameter', 'debug', 'rdb.init(' .. name .. '): installed watcher for rdb variable "'.. node.rdbKey .. '"')
				node.rdbListenerId = rdbevent.onChange(node.rdbKey, makeWatcher(node))
			end

			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.rdbKey) or ''
			Logger.log('Parameter', 'debug', 'rdb.get(' .. name .. ') = "' .. node.value .. '"')
			return 0, node.value
		end,
		set = function(node, name, value)
			Logger.log('Parameter', 'debug', 'rdb.set(' .. name .. ', "' .. value .. '")')
			local ret, msg = node:validateValue(value)
			if ret then
				Logger.log('Parameter', 'error', 'rdb.set(' .. name .. ', "' .. value .. '") error: ' .. msg)
				return ret, msg
			end

			if node.value == value then
				return 0
			end

			if node:getPath() ~= conf.topRoot .. ".ManagementServer.URL" then
				node.value = value
			end

			luardb.set(node.rdbKey, value)
			if node.rdbPersist == '1' then
				luardb.setFlags(node.rdbKey, Daemon.flagOR(luardb.getFlags(node.rdbKey), 'p'))
			end
			return 0
		end,
		attrib = function(node, name)
			Logger.log('Parameter', 'debug', 'rdb.attrib(' .. name .. ')')
			persistNotify(node)

			-- Always need to set onChange watcher for ManagementServerURL.
			-- ManagementServerURL should trigger "0 BOOTSTRAP" inform event,
			-- whenever the value has been changed.
			if client.tree:isManagementServerURL(name) then
				if not node.rdbListenerId then
					node.rdbListenerId = rdbevent.onChange(node.rdbKey, makeWatcher(node))
				end
			else
				if node.notify < 1 and node.rdbListenerId ~= nil then
					Logger.log('Parameter', 'debug', 'rdb.attrib(' .. name .. '): removed watcher for rdb variable "'.. node.rdbKey .. '"')
					rdbevent.removeListener(node.rdbListenerId)
					node.rdbListenerId = nil
				elseif node.notify > 0 and not node.rdbListenerId then
					Logger.log('Parameter', 'debug', 'rdb.attrib(' .. name .. '): installed watcher for rdb variable "'.. node.rdbKey .. '"')
					node.rdbListenerId = rdbevent.onChange(node.rdbKey, makeWatcher(node))
				end
			end
			return 0
		end,
	}
}
