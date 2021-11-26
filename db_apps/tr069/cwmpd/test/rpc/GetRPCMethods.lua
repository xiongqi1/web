dofile('config.lua')

require('CWMP.Message')
require('CWMP.RPC')
require('CWMP.Array')

-- fake a client for test purpose
client = {}
function client.runTasks(self)
end

local msg = CWMP.Message.new('GetRPCMethods')
msg.id = math.random(0,10000)
msg.version = '1.0'

local ret = CWMP.RPC.call(msg)
assert(ret.type == 'GetRPCMethodsResponse', tostring(ret))
assert(ret.message.MethodList:count() > 0, 'No methods returned.')

-- print('Request:', tostring(msg))
-- print('Reply:', tostring(ret))
