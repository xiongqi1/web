dofile('config.lua')

require('XML')

local tests = {
	['0123456789'] = '0123456789',
	['abcdefghijklmnopqrstuvwxyz'] = 'abcdefghijklmnopqrstuvwxyz',
	['ABCDEFGHIJKLMNOPQRSTUVWXYZ'] = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
	['&<>"\'\0\8'] = '&amp;&lt;&gt;&quot;&apos;&#x0;&#x8;',
	['&foo;'] = '&amp;foo;',
}

for raw, encoded in pairs(tests) do
	local enc = XML.encode(raw)
	local dec = XML.decode(encoded)
	assert(enc == encoded, 'Unexpected entity encoding: "' .. raw .. '" -> "' .. enc .. '".')
	assert(dec == raw, 'Unexpected entity decoding: "' .. encoded .. '" -> "' .. dec .. '".')
end

local doc = [[
<?xml version="1.0" encoding="UTF-8" ?>
<root xmlns:a="urn:example:1" xmlns:b="urn:example:2">
	<foo xmlns="urn:example:3">
		<a:bar/>
		<b:qux/>
		<quux/>
	</foo>
</root>
]]

xml = XML.parse(doc)
nses = XML.getAllNSDeclarations(xml)
for _, ns in ipairs(nses) do
	print(table.tostring(ns))
end