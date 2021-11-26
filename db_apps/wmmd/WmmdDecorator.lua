--[[

Base class for decorators

--]]

local WmmdDecorator = {}

-- Create a decorator
function WmmdDecorator:new()
  local obj = {}
  setmetatable(obj, self)
  self.__index = self
  -- table to save reference to original or previously decorated implementation in decorator chain
  obj.__savedChain__ = nil
  -- input object to be decorated
  obj.__inputObj__ = nil
  return obj
end

-- Saving current implementation of a member of the decorated object
-- @param name string indicating name of the member
function WmmdDecorator:__saveChain(name)
  self.__savedChain__[name] = self.__inputObj__[name]
end

-- Saving current implementation of multiple members of the decorated object
-- @param nameTbl table of which entry values indicate names of the member
function WmmdDecorator:__saveChainTbl(nameTbl)
  assert(type(nameTbl) == "table")
  for _,name in pairs(nameTbl) do
    self.__savedChain__[name] = self.__inputObj__[name]
  end
end

-- Invoke original or previously decorated implementation in decorator chain of a member
-- @param name string indicating name of the member
function WmmdDecorator:__invokeChain(name)
  return self.__savedChain__[name]
end

-- Setting new implementation for a member of the decorated object.
-- @param name string indicating name of the member
-- @param newImplName string indicating name of new implementation. Default: name of the decorated member
function WmmdDecorator:__changeImpl(name, newImplName)
  newImplName = newImplName or name
  self.__inputObj__[name] = self[newImplName]
end

-- Setting new implementation for multiple members of the decorated object.
-- @param nameTbl table indicating name of the member. Entries with keys as numnber indicating that
-- values representing both name of the members and names of new implementation. Entries with keys
-- as strings representing name of members and values representing names of new implementation.
function WmmdDecorator:__changeImplTbl(nameTbl)
  assert(type(nameTbl) == "table")
  for k,v in pairs(nameTbl) do
    if type(k) == "number" then
      self.__inputObj__[v] = self[v]
    else
      self.__inputObj__[k] = self[v]
    end
  end
end

-- Doing decoration which may includes saving current implementation
-- and setting new implementation for members of the decorated object.
-- This is an abstract function and concrete decorator needs to implement it.
function WmmdDecorator.doDecorate()
  error('abstract method')
end

-- Entry point to decorate an input object
-- @param inputObj input object to be decorated
function WmmdDecorator:decorate(inputObj)
  if self.__savedChain__ ~= nil or self.__inputObj__ ~= nil then
    -- it is designed to support decorate only one object
    return inputObj
  end
  self.__savedChain__ = {}
  self.__inputObj__ = inputObj
  self.doDecorate()
  return inputObj
end

return WmmdDecorator