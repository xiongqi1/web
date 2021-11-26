root = '../'

conf = {
	fs = {
		code = root .. '/src',
		config = root .. '/test/tr069d.conf',
		randomSource = '/dev/urandom',
	},
	net = {
		connectionRequestPort = 8082,
	},
	rdb = {
		bootstrap = 'tr069.bootstrap',
		eventPrefix = 'tr069.event',
		transferPrefix = 'tr069.transfer',
		requestPrefix = 'tr069.request',
		informTrigger = 'service.tr069.trigger.inform',
		deviceResetReason = 'service.system.reset_reason',
		deviceReset = 'service.system.reset',
		factoryReset = 'service.system.factory',
	},
}

package.path = conf.fs.code .. '/?.lua;' .. conf.fs.code .. '/classes/?/?.lua;' .. conf.fs.code .. '/classes/?.lua;' .. conf.fs.code .. '/utils/?.lua;' .. package.path
package.cpath = conf.fs.code .. '/?.so;' .. conf.fs.code .. '/utils/?.so;' .. package.cpath

require('Logger')
Logger.debug = true

function assertFault(msg, code)
	assert(msg.type == 'Fault' and msg.message.detail.Fault.FaultCode == tostring(code), 'Expected ' .. code .. ' fault, got: ' .. tostring(msg))
end

function assertNotFault(msg, msgType)
	assert(msg.type ~= 'Fault', 'Got unexpected fault: ' .. tostring(msg))
	assert(msg.type == msgType, 'Expected "' .. msgType .. '" message, got: ' .. tostring(msg))
end
