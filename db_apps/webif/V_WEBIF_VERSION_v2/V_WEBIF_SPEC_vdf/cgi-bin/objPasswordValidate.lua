-- objPasswordValidate.lua: counterpart to password_lock_elements.js
--
-- Checks that the WebUI user has entered the correct root password in order to adjust a HTML field
-- with a "password-locked" class.
--
-- Called through jsonSrvr.lua.  We have slightly hijacked the intended get/set/validate logic in
-- order to do the password validation.  The password is stored in a local variable by the set()
-- method and then checked in the get() method which assigns "good" or "bad" to a return field
-- accordingly.
--
-- Copyright (C) 2018 NetComm Wireless Limited.

-- User the client know if we failed due to a bad password or not.
local offered_password = nil


objHander = {

  get = function(authenticated)
    -- Checks the password we stored in set();  We "get" the state of the password.
    if offered_password == getRdb('admin.user.root') then
        password_state = 'good'
    else
        password_state = 'bad'
    end
    return {state=password_state}
  end,

  validate=function(obj)
    return true;
  end,

  set = function(authenticated, obj)
    -- Store away the password for checking by get().
    offered_password = decode(obj.password);
  end
}
