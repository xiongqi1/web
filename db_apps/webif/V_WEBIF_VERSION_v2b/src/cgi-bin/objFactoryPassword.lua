-- Logic to check that the factory password given on the landing page is correct.
-- The user should only arrive at landing.html when the system is in the factory reset state and
-- the root password has reverted to the factory defined one.
--
-- Copyright (C) 2018 NetComm Wireless Limited.

require('srvrUtils')

objHander = {
  pageURL='/landing.html',
  authenticatedOnly=false,
  get=function(authenticated)
    return {}
  end,

  validate=function(obj)
  local expect = luardb.get("admin.user.root")
#if defined(V_PRODUCT_ntc_220)
  if check_passwd(obj.factoryDefaultPassword, expect) == "good" then
#else
  if obj.factoryDefaultPassword == expect then
#endif
    return true
  else
    setResponseErrorText("incorrectFactoryPassword")
    return false, "password bad"
  end
end,

  set=function(authenticated,obj)
  end
}
