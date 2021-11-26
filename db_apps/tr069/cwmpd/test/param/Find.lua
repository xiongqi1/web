dofile('config.lua')

require('Parameter.Tree')

paramTree = Parameter.Tree.parseConfigFile(conf.fs.config)
local root = paramTree:getRootName() .. '.'

local ret = paramTree:find('')
assert(ret, 'Expected root node.')
print(ret:getPath())

local ret = paramTree:find('.')
assert(ret, 'Expected root node.')
print(ret:getPath())

local ret = paramTree:find('InternetGatewayDevice.DeviceInfo.')
assert(ret, 'Expected a node.')
print(ret:getPath())

local ret = paramTree:find('InternetGatewayDevice.DeviceInfo.Description')
assert(ret, 'Expected a node.')
print(ret:getPath())

local ret = paramTree:find('InternetGatewayDevice.DeviceInfo.Description.')
assert(ret, 'Expected a node.')
print(ret:getPath())

local ret = paramTree:find('InternetGatewayDevice.DeviceInfo.NonExistantParameter')
assert(ret == nil, 'Expected no matching node.')

local ret = paramTree:find('InternetGatewayDevice.DeviceInfo.NonExistantObject.')
assert(ret == nil, 'Expected no matching node.')
