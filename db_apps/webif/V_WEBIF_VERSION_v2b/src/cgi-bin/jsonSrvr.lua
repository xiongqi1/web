#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
-- All GET data  requests and PUT data sets come through this Lua CGI script
-- All data transfers are done on JSON format and are object based.
-- A request for a certain object is dispatched to a Lua snippet that is usually auto generated
-- at build time but could also be a hand crafted Lua script

require('luardb')
require('rdbobject')
require('stringutil')
require('tableutil')
require('variants')
local json = require('JSON')
local url = require("socket.url")

require('srvrUtils')

local query_string = os.getenv('QUERY_STRING')
local request_method = os.getenv('REQUEST_METHOD')
local SESSION_ID = os.getenv("SESSION_ID")
local csrfToken = os.getenv('csrfToken')
local csrfToken_for = os.getenv('csrfToken_for')

local response = {}
response.sendJsonResponse = true
local authenticated = SESSION_ID and  SESSION_ID == os.getenv("sessionid")

response.authenticated = authenticated

-- TODO implement if required (GET can only retrieve data)
-- CSRF token must be valid
-- local csrfToken = os.getenv('csrfToken')
-- local csrfTokenGet = os.getenv('csrfTokenGet')
-- if ( csrfToken == "nil" or csrfTokenGet == "nil" or csrfToken ~= csrfTokenGet ) then
--   os.exit(254)
-- end

response.text = ''

function csrfValid(objHander)
  if objHander.pageURL then
    return csrfToken_for and csrfToken_for == objHander.pageURL
  end
  return true
end

function getObjectHandler(objName)
  local fileName = '/www/cgi-bin/obj'..objName..'.lua'
  if file_exists(fileName) then
    dofile(fileName)
    if objHander then
      if csrfValid(objHander) == false then
        response.text = 'Invalid csrfToken'
        response.result = 1
      elseif objHander.authenticatedOnly and (authenticated == false) then
        response.access = 'NeedsAuthentication'
        response.text = response.text .. 'Object ' .. objName .. ' denied '
      else
        return objHander
      end
    else
      response.text = response.text .. 'Unknown Object' .. objName .. ' '
      response.result = 1
    end
  else
    response.text = response.text .. 'Missing ' .. objName .. ' handler '
    response.result = 1
  end
  return nil
end

function getObject(objName)
  local objHander = getObjectHandler(objName)
  if objHander then
    response[objName] = objHander.get(authenticated)
    response.text = response.text .. 'Object ' .. objName .. ' got '
    response.result = 0
  end
end

-- This function validates an object or an array of objects
function validateObject(objHander, value)
  local objs = value.objs
  local valid, err
  if objs ~= nil then
    for _, obj in ipairs(objs) do
      valid, err = objHander.validate(obj)
      if valid == false then
        break
      end
    end
  else
    valid, err = objHander.validate(value)
  end
  return valid, err
end

function postObject(value)
  local objName = value.name
  local objHander = getObjectHandler(objName)
  if objHander then
      local valid, err = validateObject(objHander, value)
      if valid == false then
        response[objName] = objHander.get(authenticated)
        response.text = response.text .. 'Object ' .. objName .. ' invalid because ' .. err
        response.result = 1
      else
        objHander.set(authenticated,value)
        response[objName] = objHander.get(authenticated)
        response.text = response.text .. 'Object ' .. objName .. ' set '
        response.result = 0
      end
  end
end

-- Lets the server specify the text of the message that appears in the "Invalid Request" box.
-- This would be called from within an objHandler validate() method when it is about to return false
-- I.e. the request is invalid.
function setResponseErrorText(text)
    response.errorText = text
end


if request_method == 'POST' then
  -- Create an post object from the posted data
  local stdinput = io.read("*all")
  local postObj = json:decode(url.unescape(stdinput))
  if postObj then
    if (postObj.csrfToken == nil) or (csrfToken == nil) or (postObj.csrfToken ~= csrfToken) then
      response.text = 'Invalid csrfToken'
      response.result = 1
    elseif postObj.setObject then
      -- we have an array of objects to setObject
      -- for each use the appropriate helper lua snippet
      for key,value in pairs(postObj.setObject) do
        local status, err = pcall(postObject, value)
        if status == false then
          response.text = 'Error ' .. err
          response.result = 1
          break
        end
        if response.result ~= 0 then
          break
        end
      end
    else
      response.text = 'Unknown POST request'
      response.result = 1
    end
  else
    response.text = 'Invalid POST request'
    response.result = 1
  end

elseif request_method == 'GET' then
  -- Create an request object from the query string
  local queryObj = json:decode(url.unescape(query_string))
  if queryObj then
    if (queryObj.csrfToken == nil) or (csrfToken == nil) or (queryObj.csrfToken ~= csrfToken) then
      response.text = 'Invalid csrfToken'
      response.result = 1
    elseif queryObj.getObject then
      -- we have an array of objects names getObject
      -- for each use the appropriate helper lua snippet
      for key,value in pairs(queryObj.getObject) do
        local status, err = pcall(getObject, value)
        if status == false then
          response.text = 'Error ' .. err
          response.result = 1
          break
        end
        if response.result ~= 0 then
          break
        end
      end
    else
      response.text = 'Unknown GET request'
      response.result = 1
    end
  else
    response.text = 'Invalid GET request'
    response.result = 1
  end
else
  response.text = 'Unsupported request '..request_method
  response.result = 1
end

if response.sendJsonResponse == true then
  print("Content-type: text/json\n")
  print(json:encode(response))
end
