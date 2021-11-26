--[[

An example illustrating how to implement a decorator

--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")

local ADecorator = WmmdDecorator:new()

-- decorate a function
function ADecorator:a_function()
  print("Invoked a_function from a V-feature decorator")

  -- call original implemenation if necessary
  ADecorator:__invokeChain("a_function")(self)

  -- implement additional processes
  print(self.a_data_member)
end

-- decorate a function - different name
function ADecorator:another_function_diff_name()
  print("Invoked another_function from a V-feature decorator")

  -- call original implemenation if necessary
  ADecorator:__invokeChain("another_function")(self)

  -- implement additional processes
end

-- decorate a function
function ADecorator:function_1()
  print("Invoked function_1 from a V-feature decorator")

  -- call original implemenation if necessary
  ADecorator:__invokeChain("function_1")(self)

  -- implement additional processes
end

-- decorate a function
function ADecorator:function_2_diff_name()
  print("Invoked function_2 from a V-feature decorator")

  -- call original implemenation if necessary
  ADecorator:__invokeChain("function_2")(self)

  -- implement additional processes
end

-- decorate a function
function ADecorator:function_3()
  print("Invoked function_3 from a V-feature decorator")

  -- call original implemenation if necessary
  ADecorator:__invokeChain("function_3")(self)

  -- implement additional processes
end

-- a data member
ADecorator.a_data_member = 1

-- do decoration
function ADecorator.doDecorate()
  ADecorator:__saveChain("a_function")
  ADecorator:__saveChain("another_function")
  ADecorator:__changeImpl("a_function")
  ADecorator:__changeImpl("another_function", "another_function_diff_name")
  ADecorator:__saveChainTbl({"function_1", "function_2", "function_3"})
  ADecorator:__changeImplTbl({"function_1", function_2 = "function_2_diff_name", "function_3"})
  ADecorator:__changeImpl("a_data_member")
end

--[[ Test

local anObj = {}
anObj.a_data_member = 99
function anObj:a_function()
  print("Invoked original a_function")
  print(self.a_data_member)
end

function anObj:another_function()
  print("Invoked original another_function")
end

function anObj:function_1()
  print("Invoked original function_1")
end

function anObj:function_2()
  print("Invoked original function_2")
end

function anObj:function_3()
  print("Invoked original function_3")
end

anObj = ADecorator:decorate(anObj)

anObj:a_function()
anObj:another_function()
anObj:function_1()
anObj:function_2()
anObj:function_3()

--]]

return ADecorator