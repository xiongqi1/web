--[[
    Utility functions for WMMD

    Copyright (C) 2017 NetComm Wireless Limited.
--]]

local string = require("string")
local l = require("luasyslog")

-- parse string representation of lock mode to array form
-- lockmode: "pci:PCI1,earfcn:EARFCN1,eci:ECI1;pci:PCI2,earfcn:EARFCN2,eci:ECI2;..."
-- return: { {pci=PCIx, earfcn=EARFCNx, eci=ECIx} }
--         nil on errors
local function parse_lockmode(lockmode)
  l.log("LOG_DEBUG",string.format("parsing lockmode=%s",tostring(lockmode)))
  if not lockmode then
    return {}
  end
  local locksets = {}
  local triplets = lockmode:split(";")
  for _,triplet in ipairs(triplets) do
    local kvs = triplet:split(",")
    local lockset = {}
    for _,kv in ipairs(kvs) do
      local k,v = kv:match("^%s*(%a+):%s*([%xx]+)%s*$")
      if not k or not v then
        l.log("LOG_ERR", "Invalid lock mode parameter " .. kv)
        return nil
      end
      local num = tonumber(v)
      if not num then
        l.log("LOG_ERR", "Invalid value " .. v .. " for lock mode parameter " .. k)
        return nil
      end
      if table.contains({"pci", "earfcn", "eci"}, k) then
        lockset[k] = num
      else
        l.log("LOG_ERR", "Unknown lock mode parameter " .. k)
        return nil
      end
    end
    l.log("LOG_INFO",string.format("lockset=%s",table.tostring(lockset)))
    -- at the moment pci & earfcn are mandatory. this could be extended
    if lockset.pci and lockset.earfcn then
      table.insert(locksets, lockset)
    else
      l.log("LOG_WARNING",string.format("lockset=%s is incomplete, ignored",table.tostring(lockset)))
    end
  end
  return locksets
end

-- this is the reverse of parse_lockmode
local function pack_lockmode(locksets)
  if not locksets or next(locksets) == nil then
    return ""
  end
  local locktab = {}
  for _,lockset in ipairs(locksets) do
    local lockentry = {}
    if lockset.pci then
      table.insert(lockentry, "pci:"..lockset.pci)
    end
    if lockset.earfcn then
      table.insert(lockentry, "earfcn:"..lockset.earfcn)
    end
    if lockset.eci then
      table.insert(lockentry, "eci:"..lockset.eci)
    end
    if #lockentry == 0 then
      l.log("LOG_ERR", "Invalid lockset entry")
    else
      table.insert(locktab, table.concat(lockentry, ","))
    end
  end
  return table.concat(locktab, ";")
end

-- parse string representation of lock mode to array form - 5G
-- lockmode: "pci:PCI1,arfcn:ARFCN1,scs:SCS1,band:BAND1;pci:PCI2,arfcn:ARFCN2,scs:SCS2,band:BAND2;..."
-- return: { {pci=PCIx, earfcn=EARFCNx, scs=SCSx, band=BANDx} }
--         nil on errors
local function parse_lockmode_5G(lockmode)
  l.log("LOG_DEBUG",string.format("parsing 5G lockmode=%s",tostring(lockmode)))
  if not lockmode then
    return {}
  end
  local locksets = {}
  local quartets = lockmode:split(";")
  for _,quartet in ipairs(quartets) do
    local kvs = quartet:split(",")
    local lockset = {}
    for _,kv in ipairs(kvs) do
      local k,v = kv:match("^%s*(%a+):%s*(%w+%s*%w*%s*%w*%s*%w*)%s*$")
      if not k or not v then
        l.log("LOG_ERR", "Invalid lock mode parameter " .. kv)
        return nil
      end
      if table.contains({"pci", "arfcn", "scs", "band"}, k) then
        lockset[k] = v
      else
        l.log("LOG_ERR", "Unknown lock mode parameter " .. k)
        return nil
      end
    end
    l.log("LOG_INFO",string.format("lockset=%s",table.tostring(lockset)))
    if lockset.pci and lockset.arfcn and lockset.scs and lockset.band then
      table.insert(locksets, lockset)
    else
      l.log("LOG_WARNING",string.format("lockset=%s is incomplete, ignored",table.tostring(lockset)))
    end
  end
  return locksets
end

-- this is the reverse of parse_lockmode_5G
local function pack_lockmode_5G(locksets)
  if not locksets or next(locksets) == nil then
    return ""
  end
  local locktab = {}
  for _,lockset in ipairs(locksets) do
    local lockentry = {}
    if lockset.pci then
      table.insert(lockentry, "pci:"..lockset.pci)
    end
    if lockset.arfcn then
      table.insert(lockentry, "arfcn:"..lockset.arfcn)
    end
    if lockset.scs then
      table.insert(lockentry, "scs:"..lockset.scs)
    end
    if lockset.band then
      table.insert(lockentry, "band:"..lockset.band)
    end
    if #lockentry == 0 then
      l.log("LOG_ERR", "Invalid lockset entry")
    else
      table.insert(locktab, table.concat(lockentry, ","))
    end
  end
  return table.concat(locktab, ";")
end

-- check if two lock modes in array form are equal
local function lock_equal(a,b)
  if a == b then return true end
  if not a or not b then return false end
  if #a ~= #b then return false end

  local function lockentry_equal(ea,eb)
    if ea == eb then return true end
    if not ea or not eb then return false end
    return ea.pci == eb.pci and ea.earfcn == eb.earfcn
  end

  for _,ea in ipairs(a) do
    local found = false
    for _,eb in ipairs(b) do
      if lockentry_equal(ea,eb) then
        found = true
        break
      end
    end
    if not found then return false end
  end

  return true
end

-- returns max_pci_pair_entry_count of pci:earfcn entries as locksets table
-- pci_search_list: the pci(s) to search, single or csv list of pci(s)
-- pci_earfcn_list: the pci:earfcn pair list to search in, format:
--                  "pci:PCI1,earfcn:EARFCN1,eci:ECI1;pci:PCI2,earfcn:EARFCN2,eci:ECI2;..."
-- max_pci_pair_entry_count: max number of results to return
-- return: { {pci=PCIx, earfcn=EARFCNx, eci=ECIx} }
--         nil on errors
local function match_pci_in_frequency_scan_list(pci_search_list, pci_earfcn_list, max_pci_pair_entry_count)
  local locksets = {}
  local entry_count = 0

  l.log("LOG_DEBUG", string.format("pci(s) to search: %s, pci_earfcn_list: %s",
    pci_search_list, pci_earfcn_list))

  if not pci_search_list or pci_search_list == "" or
      not pci_earfcn_list or pci_earfcn_list == "" then
    return nil
  end

  local pci_list = pci_search_list:split(",")
  local parsed_pci_earfcn_list = parse_lockmode(pci_earfcn_list)

  for _,pci in ipairs(pci_list) do
    for _,pci_pair in ipairs(parsed_pci_earfcn_list) do
      if tonumber(pci) == pci_pair.pci then
        table.insert(locksets, {pci=pci_pair.pci, earfcn=pci_pair.earfcn})
        entry_count = entry_count + 1
      end

      if entry_count >= tonumber(max_pci_pair_entry_count) then
        return locksets
      end
    end
  end

  if entry_count == 0 then
    return nil
  end

  return locksets
end

-- get all earfcn entries from frequencey scan list
-- pci_earfcn_list: the pci:earfcn pair list to search in, format:
--                  "pci:PCI1,earfcn:EARFCN1,eci:ECI1;pci:PCI2,earfcn:EARFCN2,eci:ECI2;..."
-- returns: all earfcn entries from frequencey scan as csv values
--          "EARFCN1,EARFCN2, ..."
local function get_earfcn_from_pci_earfcn_list(pci_earfcn_list)
  local pList = parse_lockmode(pci_earfcn_list)
  local earfcnList = {}
  for _,v in ipairs(pList) do
    table.insert(earfcnList, v.earfcn)
  end
  return table.concat(earfcnList, ",")
end

-- checks whether the pci:earfcn pairs contains pci values from the pci_list and
-- vice versa.
-- pci_list: the pci(s) to check for, single or csv list of pci(s)
-- pci_earfcn_list: the pci:earfcn pair list to validate, format:
--                  "pci:PCI1,earfcn:EARFCN1,eci:ECI1;pci:PCI2,earfcn:EARFCN2,eci:ECI2;..."
-- return: true if all pci(s) in earfcn list valid, false otherwise.
--         NOTE: at lease one entry must be present in earfcn list for each pci
local function validate_pci_earfcn_pairs(pci_list, pci_earfcn_list)
  if not pci_list or pci_list == "" or not pci_earfcn_list or pci_earfcn_list == "" then
    return false
  end

  local pci_list_split = pci_list:split(",")
  local pList = parse_lockmode(pci_earfcn_list)

  local match
  for _,v in ipairs(pList) do
    match = false
    for _,pci in ipairs(pci_list_split) do
      if tonumber(pci) == v.pci then
        match = true
        break
      end
    end
    if not match then
      return false
    end
  end

  for _,pci in ipairs(pci_list_split) do
    match = false
    for _,v in ipairs(pList) do
      if tonumber(pci) == v.pci then
        match = true
        break
      end
    end
    if not match then
      return false
    end
  end

  return true
end

-- check if value (string) exists in a list (strings)
-- value: value to find (string)
-- list: list (csv format strings) to find value in
-- return: true if value found, false otherwise
local function find_value_in_list(value, list)
  l.log("LOG_DEBUG", string.format("value to find: '%s', in list: '%s'", value, list))
  if list and list ~= "" then
    local list_split = list:split(",")
    for _,v in pairs(list_split) do
      if value == v then
        return true
      end
    end
  end
  return false
end

-- parse string representation of lock mode to array form
-- pci_earfcn_pairs: "pci:PCI1,earfcn:EARFCN1;pci:PCI2,earfcn:EARFCN2;..."
-- return: { {pci=PCIx, earfcn=EARFCNx} }
--         nil on errors
local function parse_pci_earfcn_pairs(pci_earfcn_pairs)
  return parse_lockmode(pci_earfcn_pairs)
end

-- merge multiple lists, without duplication of elements
-- lists: input lists to merge, each list is a comma separated list
-- return: a single, megered, comma separated list
function merge_unique_lists(lists)
  local new_list = {}
  for _, list in ipairs(lists) do
    if list and list ~= '' then
      for _, item in ipairs(list:split(',')) do
        new_list[item] = item
      end
    end
  end
  local ret_list = {}
  for _, item in pairs(new_list) do
    table.insert(ret_list, item)
  end
  return table.concat(ret_list, ',')
end

-- checks whether the pci_lists are same ignoring value location
-- i.e "1,2,3" is equal to "3,1,2"
-- pci_list1: pci list, single or csv list of pci(s)
-- pci_list2: pci list, single or csv list of pci(s)
-- return: true if pci lists same, false otherwise.
--         NOTE: at lease one entry must be present in each list
local function compare_pci_lists(pci_list1, pci_list2)
  if not pci_list1 or pci_list1 == "" or not pci_list2 or pci_list2 == "" then
    return false
  end

  local pci_list1_split = pci_list1:split(",")
  local pci_list2_split = pci_list2:split(",")

  local match
  for _,pci1 in ipairs(pci_list1_split) do
    match = false
    for _,pci2 in ipairs(pci_list2_split) do
      if tonumber(pci1) == tonumber(pci2) then
        match = true
        break
      end
    end
    if not match then
      return false
    end
  end

  for _,pci2 in ipairs(pci_list2_split) do
    match = false
    for _,pci1 in ipairs(pci_list1_split) do
      if tonumber(pci1) == tonumber(pci2) then
        match = true
        break
      end
    end
    if not match then
      return false
    end
  end

  return true
end

local _m = {
  parse_lockmode = parse_lockmode,
  pack_lockmode = pack_lockmode,
  parse_lockmode_5G = parse_lockmode_5G,
  pack_lockmode_5G = pack_lockmode_5G,
  lock_equal = lock_equal,
  match_pci_in_frequency_scan_list = match_pci_in_frequency_scan_list,
  get_earfcn_from_pci_earfcn_list = get_earfcn_from_pci_earfcn_list,
  validate_pci_earfcn_pairs = validate_pci_earfcn_pairs,
  find_value_in_list = find_value_in_list,
  parse_pci_earfcn_pairs = parse_pci_earfcn_pairs,
  merge_unique_lists = merge_unique_lists,
  compare_pci_lists = compare_pci_lists
}

return _m
