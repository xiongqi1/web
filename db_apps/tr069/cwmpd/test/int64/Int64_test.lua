dofile('config.lua')

require('Int64')

function test_create(val)
	local var = Int64.new(val)
	assert(val == tostring(var), 'Failed ' .. val .. ' != ' .. tostring(var))
end

function test_create_num(val)
	local var = Int64.new(val)
	local res = string.format('%.0f', val)
	assert(res == tostring(var), 'Failed ' .. res .. ' != ' .. tostring(var))
end

function test_unm(a,res)
	local a_ = Int64.new(a)
	local res_ = -a_
	assert(res == tostring(res_), 'Failed ' .. res .. ' != -' .. a .. ' == ' .. tostring(res_))
end

function test_add(a,b,res)
	local a_ = Int64.new(a)
	local b_ = Int64.new(b)
	local res_ = a_ + b_
	assert(res == tostring(res_), 'Failed ' .. res .. ' != ' .. a .. ' + ' .. b .. ' == ' .. tostring(res_))
end

function test_sub(a,b,res)
	local a_ = Int64.new(a)
	local b_ = Int64.new(b)
	local res_ = a_ - b_
	assert(res == tostring(res_), 'Failed ' .. res .. ' != ' .. a .. ' - ' .. b .. ' == ' .. tostring(res_))
end

-- instantiation
test_create('1234567')
test_create_num(1234567)

test_create('9223372036854775808')

test_create('18446744073709551615')

test_create('-9223372036854775808')

-- unary negative
test_unm('0', '0')
test_unm('1234567', '-1234567')
test_unm('-1234567', '1234567')
test_unm('-9223372036854775808', '9223372036854775808')
test_unm('9223372036854775808', '-9223372036854775808')

-- add
test_add('1234567', '1234567', '2469134')
test_add('1234567', '-2469134', '-1234567')
test_add('18446744073709551615', '9223372036854775808', '27670116110564327423')
test_add('18446744073709551615', '-9223372036854775808', '9223372036854775807')
test_add('-18446744073709551615', '9223372036854775808', '-9223372036854775807')
-- check wrap
test_add('1234567', '9999999999999', '10000001234566')
test_add('-1234567', '-9999999999999', '-10000001234566')

-- sub
test_sub('9223372036854775808', '18446744073709551615', '-9223372036854775807')
test_sub('-9223372036854775808', '-18446744073709551615', '9223372036854775807')
-- sub check wrap
test_sub('1234567', '1999999999999', '-1999998765432')
test_sub('-1234567', '-1999999999999', '1999998765432')
test_sub('-1234567', '19999999999999', '-20000001234566')

assert(tostring(tobignumber(1234567)) == '1234567', "Failed tobignumber")
