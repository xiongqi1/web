#!/usr/bin/env lua

package.path = '../../../../cdcs_libs/luautil/src/?.lua;../src/core/?.lua;' .. package.path

require('tableutil')
require('stringutil')
require('utils')

local tests = {
	{ '',			'',			true },
	{ 'foo',		'foo',			true },
	{ 'foo',		'bar',			false },
	{ 'foo',		'',			false },
	{ '',			'foo',			false },

	{ '*',			'',			true },
	{ '*',			'foo',			true },
	{ '*.*',		'foo',			false },
	{ '*.*',		'foo.bar',		true },

	{ 'foo.bar',		'foo.bar',		true },
	{ 'foo',		'foo.bar',		false },
	{ 'foo.bar',		'foo',			false },
	{ 'foo.bar',		'foo.',			false },
	{ 'foo.',		'foo',			false },
	{ 'foo.',		'foo.',			true },
	{ 'foo.bar',		'foo.bar.qux',		false },
	{ 'foo.bar.qux',	'foo.bar',		false },
	{ 'foo.bar.qux',	'foo.bar.qux',		true },

	{ 'foo.*',		'foo.bar',		true },
	{ 'foo.*',		'foo.bar.qux',		false },
	{ 'foo.*.bar',		'foo.bar',		false },
	{ 'foo.*.bar',		'foo.bar.qux',		false },
	{ 'foo.*.qux',		'foo.bar.qux',		true },
	{ '*.bar',		'foo.bar',		true },
	{ '*.bar',		'foo',			false },

	{ '**.qux',		'foo.bar.qux',		true },
	{ '**.qux',		'foo.bar.blah',		false },
	{ '**.bar.qux',		'foo.bar.qux',		true },
	{ '**.bar.qux',		'foo.bar.blah',		false },
	{ '**.bar.qux',		'foo.foo.bar.qux',	true },
	{ '**.bar.qux',		'foo.foo.bar.blah',	false },
	{ 'foo.**.qux',		'foo.bar.qux',		true },
	{ 'foo.**.qux',		'foo.bar.bar.qux',	true },
	{ 'foo.**.qux',		'foo.bar.bar.bar.qux',	true },
	{ 'foo.**',		'foo.bar.qux',		true },
	{ 'foo.**.',		'foo.bar.qux',		false },
	{ 'foo.**',		'bar.bar.qux',		false },
	{ 'foo.**.bar.qux',	'foo.bar.bar.qux',	true },
	{ 'foo.**.bar.qux',	'foo.bar.bar.bar.qux',	true },
	{ 'foo.**.bar.qux',	'foo.bar.bar.blah.qux',	false },
	{ 'foo.**.bar.qux',	'foo.bar.blah.bar.qux',	true },
	{ 'foo.**.bar.qux',	'foo.bar.bar.blah.bar.qux',	true },
	{ 'foo.**.bar.*',	'foo.bar.bar.blah.bar.qux',	true },
	{ 'foo.**.bar.*.blah',	'foo.bar.bar.blah.bar.qux.blah',	true },

	{ 'foo.foo|bar',	'foo.bar',			true },
	{ 'foo.foo|bar',	'foo.foo',			true },
	{ 'foo.foo|bar',	'foo.qux',			false },
	{ 'foo.foo|bar.qux',	'foo.bar.qux',			true },
	{ 'foo.foo|bar.qux',	'foo.baz.qux',			false },
};

local failures = 0

for _, t in ipairs(tests) do
	local answer
	local ret = pathGlobMatches(t[1], t[2])
	if ret == t[3] then
		answer = 'OK'
	else
		answer = 'FAIL'
		failures = failures + 1
	end
	print(answer, t[1], t[2], t[3], ret)
end

print(failures, 'failures')
os.exit(failures)
