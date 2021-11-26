----
-- Copyright (C) 2012 NetComm Wireless Limited.
--
-- This file or portions thereof may not be copied or distributed in any form
-- (including but not limited to printed or electronic forms and binary or object forms)
-- without the expressed written consent of NetComm Wireless Limited.
-- Copyright laws and International Treaties protect the contents of this file.
-- Unauthorized use is prohibited.
--
-- THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
-- NETCOMM WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
-- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
-- SUCH DAMAGE.
----

Int64 = {}
Int64.tostring = tostr

local max_low = 10^12
local mt = { }

function mt.__tostring(self)
	local s;
	if self.hi ~= 0 then
		local l = self.lo
		if l < 0 then
			l = -l
		end
		s = string.format('%.0f', self.hi) .. string.format('%012.0f', l)
	else
		s = string.format('%.0f', self.lo)
	end
	return s
end

function mt.__eq(a, b)
	return a.hi == b.hi and a.lo == b.lo
end

function mt.__lt(a, b)
	return a.hi < b.hi or (a.hi == b.hi and a.lo < b.lo)
end

function mt.__le(a, b)
	return a.hi <= b.hi or (a.hi == b.hi and a.lo <= b.lo)
end

function sign(x)
	return x>=0 and 1 or -1
end

function bignum_sign(x)
	if x.hi == 0 then
		return sign(x.lo)
	end
	return sign(x.hi)
end

function bignum_normalize(res)
	-- handle carry
	local res_sign = bignum_sign(res)
	-- normalize result
	if res_sign < 0 then
		if res.lo > 0 then
			res.hi = res.hi + 1
			res.lo = res.lo - max_low
		end
		if res.lo <= -max_low then
			res.hi = res.hi - 1
			res.lo = res.lo + max_low
		end
	else
		if res.lo < 0 then
			res.hi = res.hi - 1
			res.lo = res.lo + max_low
		end
		if res.lo >= max_low then
			res.hi = res.hi + 1
			res.lo = res.lo - max_low
		end
	end
	return res
end

function mt.__add(a, b)
	local res = a
	res.hi = a.hi + b.hi
	res.lo = a.lo + b.lo
	res = bignum_normalize(res)
	return res
end

function mt.__sub(a, b)
	local res = a
	res.hi = a.hi - b.hi
	res.lo = a.lo - b.lo
	res = bignum_normalize(res)
	return res
end

function mt.__unm(a)
	local res = setmetatable({}, mt)
	-- without 0-x the result is left as -0 which is dumb
	res.hi = 0 - a.hi
	res.lo = 0 - a.lo
	return res
end

function mt.__mul(a, b)
	error('__mul not supported')
end

function mt.__div(a, b)
	error('__div not supported')
end

function mt.__mod(a, b)
	error('__mod not supported')
end

function mt.__pow(a, b)
	error('__pow not supported')
end

function mt.__call(val)
	return Int64.new(val)
end

-- input is a string or a number
function Int64.new(val)
	if type(val) == 'number' then
		val = string.format('%.0f', val)
	end
	val = tostring(val)
	local low_val = tonumber(val:sub(-12))
	local high_val = val:sub(0, -13)
	if high_val == '' then
		high_val = 0
	elseif high_val == '-' then
		high_val = 0;
		low_val = -low_val;
	end
	high_val = tonumber(high_val)
	if high_val < 0 then
		low_val = - low_val
	end
	local instance = {
		type = 'Int64',
		lo = low_val,
		hi = high_val
	}
	setmetatable(instance, mt)
	return instance
end

function tobignumber(val)
	return Int64.new(val)
end

return Int64
