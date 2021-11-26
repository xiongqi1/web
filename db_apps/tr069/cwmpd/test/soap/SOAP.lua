dofile('config.lua')

require('SOAP')
require('CWMP.Message')

local msg = CWMP.Message.new('Inform')
msg.version = '1.2'
msg.id = '' .. math.random(10000)
msg.holdRequests = false

local inform = msg.message

inform.DeviceId.Manufacturer = 'NetComm "Wireless"'
inform.DeviceId.OUI = '123456'
inform.DeviceId.ProductClass = 'Tester 1 & 2 > 3 < 4'
inform.DeviceId.SerialNumber = '1234567890'

local event = CWMP.Object.new('EventStruct')
event.EventCode = '0 BOOTSTRAP'
event.CommandKey = ''
inform.Event:add(event)

inform.MaxEnvelopes = 1
inform.CurrentTime = os.date('%s')
inform.RetryCount = 0

local param = CWMP.Object.new('ParameterValueStruct')
param.Name = 'InternetGatewayDevice.DeviceSummary'
param.Value = 'InternetGatewayDevice:1.1[](Baseline:1)'
param:setMemberType('Value', 'string')
inform.ParameterList:add(param)

param = CWMP.Object.new('ParameterValueStruct')
param.Name = 'A.Test.Int'
param.Value = '-456'
param:setMemberType('Value', 'int')
inform.ParameterList:add(param)

param = CWMP.Object.new('ParameterValueStruct')
param.Name = 'A.Test.UnsignedInt'
param.Value = '789'
param:setMemberType('Value', 'unsignedInt')
inform.ParameterList:add(param)

param = CWMP.Object.new('ParameterValueStruct')
param.Name = 'A.Test.Bool.False'
param.Value = false
param:setMemberType('Value', 'boolean')
inform.ParameterList:add(param)
param = CWMP.Object.new('ParameterValueStruct')
param.Name = 'A.Test.Bool.True'
param.Value = true
param:setMemberType('Value', 'boolean')
inform.ParameterList:add(param)

param = CWMP.Object.new('ParameterValueStruct')
param.Name = 'A.Test.DateTime'
param.Value = '2023-07-23T13:56:34.23'
param:setMemberType('Value', 'dateTime')
inform.ParameterList:add(param)

param = CWMP.Object.new('ParameterValueStruct')
param.Name = 'A.Test.Base64'
param.Value = 'FIXME=='
param:setMemberType('Value', 'base64')
inform.ParameterList:add(param)

local xml = SOAP.serialise(msg)
print(xml)

local inform2 = SOAP.deserialise(xml)
print(inform2)
--print(table.tostring(inform2))
