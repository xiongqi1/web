-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- historyDB is a generic file database for keeping track of json objects of string
-- format in files. The json string is saved in one single line regardless of its length.
-- The string can be compressed if it is required.
--
-- Every type of json object's history is saved in two files. The name of the file is
-- consisted of two parts concatenated by dot.
-- The first part of the name is named after its target name with slash replaced
-- by dot. For example, /api/v1/PDPContext/History would be saved in
-- api.v1.PDPContext.History. The second part is 0 or 1, index of the file.
-- For example,  /api/v1/PDPContext/History would be saved in
-- api.v1.PDPContext.History.0 and api.v1.PDPContext.History.1 .
-- 
-- History file:
--   Every line in the file represents a history item for the corresponding target.
--
-- Metadata file: 
--   historyDB maintains a metadata file ".metadata" in the repo with a line
--   for the item summary of a target history file. Each line contains six fields
--   seperated by comma as follows.
--   <target name>,<compressed>,<history item limit>,<history item limit count>,<current item number>,<start file index>
--   Among them, "compressed" is a string "true" or "false" to indicate whether the string needs
--   to compress, the history items limit is "history item limit" * "history item limit count"
--   and "current item number" is the total number of items have been saved, which can be greater than
--   history item limit but always less than 2*<history item limit>*<history item limit count>
--

local turbo = require("turbo")
local ffi = require("ffi")
local fs = turbo.fs

require("turbo.3rdparty.middleclass")

ffi.cdef[[
unsigned long compressBound(unsigned long sourceLen);
int compress2(uint8_t *dest, unsigned long *destLen,
	      const uint8_t *source, unsigned long sourceLen, int level);
int uncompress(uint8_t *dest, unsigned long *destLen,
	       const uint8_t *source, unsigned long sourceLen);
]]
local zlib = ffi.load("z")

local function compress(txt)
	local n = zlib.compressBound(#txt)
	local buf = ffi.new("uint8_t[?]", n)
	local buflen = ffi.new("unsigned long[1]", n)
	local res = zlib.compress2(buf, buflen, txt, #txt, 9)
	assert(res == 0)
	return ffi.string(buf, buflen[0])
end

local function uncompress(comp, n)
	local buf = ffi.new("uint8_t[?]", n)
	local buflen = ffi.new("unsigned long[1]", n)
	local res = zlib.uncompress(buf, buflen, comp, #comp)
	assert(res == 0)
	return ffi.string(buf, buflen[0])
end


historyDB = class("historyDB")

-- Initialize history database
-- @repo A directory 
-- @return true for success, false and error string for failure
function historyDB:initialize(repo)
	self._repo = repo
	self._metadata = {}
	if not fs.is_dir(repo) then
		return false, "repo should be a directory"
	end
	local f = io.open(self._repo.."/.metadata", "r")
	if f == nil then -- No metadata file
		f = io.open(self._repo.."/.metadata", "w")
		f:close()
		return true
	end
	while true do
		local line = f:read("*line")
		if line == nil then break end
		local metas = line:split(",")
		if #metas < 6 then
			turbo.log.notice("Corrupted metadata")
		else
			self._metadata[metas[1]] = {}
			if metas[2] == "true" then
				self._metadata[metas[1]]["compressed"] = true
			else
				self._metadata[metas[1]]["compressed"] = false
			end
			self._metadata[metas[1]]["limit"] = tonumber(metas[3])
			self._metadata[metas[1]]["count"] = tonumber(metas[4])
			self._metadata[metas[1]]["current"] = tonumber(metas[5])
			self._metadata[metas[1]]["index"] = tonumber(metas[6])
		end
	end
	f:close()
	return true
end

-- Svae meta data file
function historyDB:_saveMetaData()
	local f = assert(io.open(self._repo.."/.metadata", "w"))
	for n, v in pairs(self._metadata) do
		f:write(string.format("%s,%s,%d,%d,%d,%d\n", n, tostring(v["compressed"]), v["limit"], v["count"], v["current"], v["index"]))
	end
	f:close()
end


-- Close the database
-- It does
--    1. Save metadata
function historyDB:close()
	self:_saveMetaData()
end

-- Register a target to database.
-- @param target Target name
-- @param limit Target history item limit. Default is 96.
-- @param count Target history item limit count. Default is 2.
-- @param compressed Indicator to compress the history item. Default is no.
function historyDB:registerTarget(target, limit, count, compressed)
	if limit == nil then limit = 96 end
	if count == nil then count = 2 end
	if compressed == nil then compressed = false end
	if self._metadata[target] then -- The target has been added, nothing to do
		return true
	end
	local meta = {}
	meta["compressed"] = false -- Fixme: set sompressed always here since line compression makes line reading out of order
	meta["limit"] = limit
	meta["count"] = count
	meta["current"] = 0
	meta["index"] = 0
	self._metadata[target] = meta
	self:_saveMetaData()
	return true
end

-- Save a json string for a target
-- Save algorithm
--  If current item number < history item limit, then
--      save to the file with start file index.
--  else
--      save to the file with (start file index + 1)%2 index.
--      if current item number == history item limit * 2 then
--           current item number =  history item limit
--           truncate the file with start file index
--           start file index = (start file index + 1)%2
--  end
-- @target The target name like "/api/v1/ServCell/History"
-- jsonObj json object
function historyDB:save(target, jsonObj)
	local fileIndex
	local truncated = false
	local meta = self._metadata[target]
	local jsonStr = turbo.escape.json_encode(jsonObj)
	local record_limit
	-- maximum record count in one history file
	record_limit = meta.limit*meta.count

	if meta.current < record_limit then -- The current index file is not full and can be written
		fileIndex = meta.index
	elseif meta.current == record_limit*2 then -- The other file is full
		meta.current = record_limit
		fileIndex = meta.index -- The current index file is obsolete now and can be written
		truncated = true
		meta.index = (meta.index+1)%2 -- Index rolls back to the other file
	else -- The current index file is full, switched to the other file
		fileIndex = (meta.index+1)%2
	end

	if meta.compressed then -- Compressed needed
		jsonStr = compress(jsonStr)
	end

	local targetFile = self._repo.."/"..target:gsub("/(.*)","%1"):gsub("/",".")..tostring(fileIndex)
	local f
	if truncated then
		f = assert(io.open(targetFile, "w"))
	else
		f = assert(io.open(targetFile, "a"))
	end
	f:write(jsonStr)
	f:write("\n")
	meta.current = meta.current + 1
	f:close()

	self:_saveMetaData() -- Save MetaData every time it is updated
end


-- Get all history from the target with oldest coming firstly
-- @target The target name
-- @compare Optional, a function judges whether a history should be returned.
-- @field Optional, the field name, whose value would be compared
function historyDB:get(target, compare, field)
	return historyDB:_get(target, compare, field, false)
end

-- Get all history from the target with latest coming firstly
-- @target The target name
-- @compare Optional, a function judges whether a history should be returned.
-- @field Optional, the field name, whose value would be compared
function historyDB:getLatestFirst(target, compare, field)
	return historyDB:_get(target, compare, field, true)
end

function historyDB:_get(target, compare, field, latestFirst, reverseCompare)
	local ret = {}
	local meta = self._metadata[target]
	local startFile = self._repo.."/"..target:gsub("/(.*)","%1"):gsub("/",".")..tostring(meta.index)
	local f = io.open(startFile, "r")
	if f == nil then return ret end -- No history yet
	local skip = 0
	local i = 0
	local fieldCount = {}
	local record_limit
	local line_count = 0
	-- maximum record count in one history file
	record_limit = meta.limit*meta.count

	if meta.current <= record_limit then -- All history sits in one file
		skip = 0
	else
		skip = meta.current - record_limit
	end

	-- Read the index file firstly
	while true do
		local line = f:read("*line")
		if line == nil then break end
		if skip > 0 then
			skip = skip -1
		else
			if meta.compressed then -- Decompress if necessary
				line = uncompress(line, 8092)
			end
			local jsonObj = turbo.escape.json_decode(line)
			if compare then
				local fieldValue = jsonObj[field]
				if (not reverseCompare and compare(fieldValue)) or (reverseCompare and not compare(fieldValue)) then -- If it meets the criteria
					if not fieldCount[fieldValue] then
						fieldCount[fieldValue] = 0
					end
					--skip all records over the individual limit
					if fieldCount[fieldValue] < meta.limit then
						if latestFirst then
							table.insert(ret, 1, jsonObj)
						else
							table.insert(ret, jsonObj)
						end
						fieldCount[fieldValue] = fieldCount[fieldValue] + 1
					end
				end
			else
				-- if no comparison function and field are given then count
				-- all records up to meta.limit
				if line_count < record_limit then
					if latestFirst then
						table.insert(ret, 1, jsonObj)
					else
						table.insert(ret, jsonObj)
					end
				end
				line_count = line_count + 1
			end
		end
	end
	f:close()

	if meta.current > record_limit then -- There are remaining data in the other file
		local index = (meta.index+1)%2
		startFile = self._repo.."/"..target:gsub("/(.*)","%1"):gsub("/",".")..tostring(index)
		f = io.open(startFile, "r")
		while true do
			local line = f:read("*line")
			if line == nil then break end
			if meta.compressed then -- Decompress if necessary
				line = uncompress(line, 8092) -- Todo: decompress here
			end
			local jsonObj = turbo.escape.json_decode(line)
			if compare then
				local fieldValue = jsonObj[field]
				if (not reverseCompare and compare(fieldValue)) or (reverseCompare and not compare(fieldValue)) then -- If it meets the criteria
					if not fieldCount[fieldValue] then
						fieldCount[fieldValue] = 0
					end
					--skip all records over the individual limit
					if fieldCount[fieldValue] < meta.limit then
						if latestFirst then
							table.insert(ret, 1, jsonObj)
						else
							table.insert(ret, jsonObj)
						end
						fieldCount[fieldValue] = fieldCount[fieldValue] + 1
					end
				end
			else
				-- if no comparison function and field are given then count
				-- all records up to meta.limit
				if line_count < record_limit then
					if latestFirst then
						table.insert(ret, 1, jsonObj)
					else
						table.insert(ret, jsonObj)
					end
				end
				line_count = line_count + 1
			end
		end
		f:close()
	end

	return ret
end


function historyDB:_clearTarget(target)
	local meta = self._metadata[target]
	meta.current = 0
	meta.index = 0
	local index = 0
	while index < 2 do -- truncate the files
		local hfile = self._repo.."/"..target:gsub("/(.*)","%1"):gsub("/",".")..tostring(index)
		io.open(hfile, "w")
		io.close()
		index = index+1
	end
	self:_saveMetaData() -- Save MetaData every time it is updated
end


-- Delet all the history for a target
--@target The target name
function historyDB:delete(target, compare, field)
	if compare == nil then -- no compare function provided, delete all history for this target
		historyDB:_clearTarget(target)
		return
	end
	-- Get all data to survive
	local ret = historyDB:_get(target, compare, field, false, true)

	-- Clear target history now
	historyDB:_clearTarget(target)

	local meta = self._metadata[target]
	local fileIndex = 0
	local targetFile = self._repo.."/"..target:gsub("/(.*)","%1"):gsub("/",".")..tostring(fileIndex)
	local f = assert(io.open(targetFile, "a"))
	for _, jsonObj in ipairs(ret) do
		local jsonStr = turbo.escape.json_encode(jsonObj)
		if meta.compressed then -- Compressed needed
			jsonStr = compress(jsonStr)
		end
		f:write(jsonStr)
		f:write("\n")
		meta.current = meta.current + 1
	end
	f:close()
	self:_saveMetaData()
end

return historyDB
