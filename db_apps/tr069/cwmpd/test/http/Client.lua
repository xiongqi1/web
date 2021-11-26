dofile('config.lua')

require('HTTP.Client')

local http = HTTP.Client.new()
--http.debug = true
http:setAuth('acs', 'acs')
http:init()

local data = [[
<?xml version="1.0" encoding="UTF-8" ?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
               xmlns:cwmp="urn:dslforum-org:cwmp-1-2">
  <soap:Body>
    <cwmp:GetRPCMethods></cwmp:GetRPCMethods>
  </soap:Body>
</soap:Envelope>
]]
local url = 'http://localhost/acs/cwmp.php'

local code, ret = http:doEmptyPOST(url)
print(code, ret)
assert(code == 200, 'Expected 200 reply.')

local code, ret = http:doRawPOST(url, 'text/xml', data)
print(code, ret)
assert(code == 200, 'Expected 200 reply.')

http:close()
