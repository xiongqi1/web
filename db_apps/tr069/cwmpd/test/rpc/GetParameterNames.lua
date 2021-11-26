dofile('config.lua')

require('Config.Parser')
require('Parameter.Tree')
require('CWMP.Message')
require('CWMP.RPC')

-- fake a client for test purpose
client = {}
function client.runTasks(self)
end

paramTree = Parameter.Tree.parseConfigFile(conf.fs.config)
local root = paramTree:getRootName() .. '.'

local ret
local msg = CWMP.Message.new('GetParameterNames')
msg.id = math.random(0,10000)
msg.version = '1.0'


msg.message.ParameterPath = ''
msg.message.NextLevel = true
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterNamesResponse')
assert(ret.message.ParameterList:count() == 1, 'Expected just one parameter.')
assert(ret.message.ParameterList[1].Name == root, 'Expected root node.')

msg.message.ParameterPath = '.'
msg.message.NextLevel = true
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterNamesResponse')
assert(ret.message.ParameterList:count() == 1, 'Expected just one parameter.')
assert(ret.message.ParameterList[1].Name == root, 'Expected root node.')

msg.message.ParameterPath = 'Nonexistant.'
msg.message.NextLevel = true
ret = CWMP.RPC.call(msg)
assertFault(ret, 9005)

msg.message.ParameterPath = 'InternetGatewayDevice.Nonexistant.'
msg.message.NextLevel = true
ret = CWMP.RPC.call(msg)
assertFault(ret, 9005)

msg.message.ParameterPath = 'InternetGatewayDevice.'
msg.message.NextLevel = true
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterNamesResponse')
assert(ret.message.ParameterList:count() > 1, 'Expected more than one parameter.')
for i = 1, ret.message.ParameterList:count() do
	assert(ret.message.ParameterList[i].Name ~= root, 'Did not expect root node in result set.')
end

msg.message.ParameterPath = ''
msg.message.NextLevel = false
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterNamesResponse')
assert(ret.message.ParameterList:count() > 1, 'Expected more than one parameter.')

msg.message.ParameterPath = 'InternetGatewayDevice.DeviceInfo'
msg.message.NextLevel = false
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterNamesResponse')
assert(ret.message.ParameterList:count() > 1, 'Expected more than one parameter.')

-- print('Request:', tostring(msg))
-- print('Reply:', tostring(ret))
