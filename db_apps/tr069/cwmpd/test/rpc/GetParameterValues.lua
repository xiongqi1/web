dofile('config.lua')

require('Config.Parser')
require('CWMP.Message')
require('CWMP.RPC')
require('Parameter.Tree')

-- fake a client for test purpose
client = {}
function client.runTasks(self)
end

paramTree = Parameter.Tree.parseConfigFile(conf.fs.config)
local root = paramTree:getRootName() .. '.'

local ret
local msg = CWMP.Message.new('GetParameterValues')
msg.id = math.random(0,10000)
msg.version = '1.0'


msg.message.ParameterNames[1] = ''
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterValuesResponse')
assert(ret.message.ParameterList:count() > 1, 'Expected many parameter.')
assert(ret.message.ParameterList[1].Name == root .. 'DeviceSummary', 'Expected DeviceSummary node.')

msg.message.ParameterNames[1] = 'Nonexistant'
ret = CWMP.RPC.call(msg)
assertFault(ret, 9005)

msg.message.ParameterNames[1] = 'InternetGatewayDevice.ManagementServer.URL'
msg.message.ParameterNames[2] = 'InternetGatewayDevice.DeviceSummary'
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterValuesResponse')
assert(ret.message.ParameterList:count() == 2, 'Expected two parameters.')

msg.message.ParameterNames[1] = 'InternetGatewayDevice.ManagementServer.'
msg.message.ParameterNames[2] = 'InternetGatewayDevice.DeviceSummary'
ret = CWMP.RPC.call(msg)
assertNotFault(ret, 'GetParameterValuesResponse')
assert(ret.message.ParameterList:count() > 1, 'Expected many parameter.')

--print('Request:', tostring(msg))
--print('Reply:', tostring(ret))

