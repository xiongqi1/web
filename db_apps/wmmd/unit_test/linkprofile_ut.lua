-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- unit test for linkprofile.lua

local modName = ...

local UnitTest = require("ut_base")

local LinkProfileUt = UnitTest:new()
LinkProfileUt.name = "LinkProfile-UnitTest"

LinkProfileUt.mockWatcherCbs = {
  sys = {
    "read_rmnet",
    "start_rmnet",
    "get_rmnet_stat",
    "stop_rmnet",
    "write_rmnet",
  },
}

function LinkProfileUt:initiateModules()


  -- select a testing link profile
  self.rdbLpPrefix = "link.profile.3."
  local initSettings = {
    apn="",
    auth_type="",
    user="",
    pass="",
    enable="0",
    pdp_type="ipv4v6",
  }
  for k,v in pairs(initSettings) do
    self.rdb:set(self.rdbLpPrefix .. k, v)
  end
  -- PS attached
  self.rdb:setp("system_network_status.attached", "1")

  self.linkProfile = require("wmmd.linkprofile"):new()
  self.linkProfile:setup(self.rdbWatch, self.rdb)
  self.linkProfile:init()
end

function LinkProfileUt:setupTests()

  self.testTimeoutMs = 40000

  local lp = self.linkProfile.lps[3]

  local watchRdb = {
    self.rdbLpPrefix .. "connect_progress",
    self.rdbLpPrefix .. "pdp_result",
    self.rdbLpPrefix .. "pdp_result_verbose",
    self.rdbLpPrefix .. "interface",
    self.rdbLpPrefix .. "connect_progress",
    self.rdbLpPrefix .. "status",
  }
  for k,v in pairs(watchRdb) do
    self.rdbWatch:addObserver(v, "rdbNotify", self)
  end

  local testData = {
    -- start linkprofiles start_linkprofiles
    {
      class = "OnWatchCbTest",
      name = "sys:start_linkprofiles watcher callback",
      delay = 3000,
      type = "sys", event = "start_linkprofiles",
      arg = nil,
      expectList = {
        -- on state machine sync process
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "read_rmnet",
          arg = {ref=lp,profile_index=3},
          retVal = true,
        },
      -- lp.sync = true now
      -- lp.trigger = false now
      },
    },
    {
      class = "OnRdbTest",
      name = "LinkProfile RDB enable",
      delay = 10000,
      rdbName = self.rdbLpPrefix .. "enable",
      rdbValue = "1",
      expectList = {
        -- sl.switch_state_machine_to("lps:connect") now
        {
          class = "RdbNotified",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "establishing",
        },
        -- sl.switch_state_machine_to("lps:connect_ipv4") now
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "start_rmnet",
          arg = {
            network_interface=lp.network_interface,
            profile_index=lp.rdb_members.module_profile_idx,
            service_id=lp.lp_index,

            apn="",
            pdp_type="ipv4v6",
            auth_type="",
            user="",
            pass="",

            ipv6_enable=false,
          },
        },
        -- sl.switch_state_machine_to("lps:post_connect_ipv4",delay) with delay=30s
        -- sl.switch_state_machine_to("lps:connect_ipv6")
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "start_rmnet",
          arg = {
            network_interface=lp.network_interface,
            profile_index=lp.rdb_members.module_profile_idx,
            service_id=lp.lp_index+self.linkProfile.linkprofile_max_profile,

            apn="",
            pdp_type="ipv4v6",
            auth_type="",
            user="",
            pass="",

            ipv6_enable=true,
          },
        },
      -- sl.switch_state_machine_to("lps:post_connect_ipv6",delay)
      },
    },
    -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_rmnet_change watcher callback - after a link profile is enabled",
      type = "sys", event = "modem_on_rmnet_change",
      arg = {
        ip_family=6,
        service_id=9,
        status="up",
        call_end_reason="CALLENDREASON",
        verbose_call_end_reason_type="TYPE",
        verbose_call_end_reason=99,
      },
      expectList = {
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result",
          value = "CALLENDREASON",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result_verbose",
          value = "TYPE #99",
        },
        -- sl.switch_state_machine_to("lps:post_connect")
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "get_rmnet_stat",
          arg = {
            service_id=9,
            info={},
          },
          retVal = true,
          deepCmp = true,
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "interface",
          value = lp.network_interface,
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "established",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "status",
          value = "up",
        },

      },
    },
    -- write linkprofile: disable IPv6
    {
      class = "OnRdbTest",
      name = "LinkProfile - writing to linkprofile rdb - disable IPv6",
      rdbName = self.rdbLpPrefix .. "pdp_type",
      rdbValue = "ipv4",
      expectList = {},
    },
    {
      class = "OnRdbTest",
      name = "LinkProfile RDB trigger after writting to linkprofile rdb to disable IPv6",
      rdbName = self.rdbLpPrefix .. "trigger",
      rdbValue = "1",
      expectList = {
        -- sl.switch_state_machine_to("lps:transit")
        -- sl.switch_state_machine_to("lps:disconnect")
        {
          class = "RdbNotified",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "disconnecting",
        },
        -- sl.switch_state_machine_to("lps:disconnect_ipv6")
        -- sl.switch_state_machine_to("lps:post_disconnect_ipv6",self.linkprofile_disconnect_retry_interval)
        -- self:invoke_stop_rmnet(lp,true)
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "stop_rmnet",
          arg = {
            profile_index=lp.rdb_members.module_profile_idx,
            service_id=9,
          },
          retVal = true,
        },
      -- somethings should invoke sys:modem_on_rmnet_change to update online_ipv6 status
      },
    },
    -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_rmnet_change watcher callback - informing Ipv6 down",
      type = "sys", event = "modem_on_rmnet_change",
      arg = {
        ip_family=6,
        service_id=9,
        status="down",
        call_end_reason="CALLENDREASON",
        verbose_call_end_reason_type="TYPE",
        verbose_call_end_reason=99,
      },
      expectList = {
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result",
          value = "CALLENDREASON",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result_verbose",
          value = "TYPE #99",
        },
        -- lps:post_disconnect_ipv6 will do sl.switch_state_machine_to("lps:disconnect_ipv4")
        -- sl.switch_state_machine_to("lps:post_disconnect_ipv4")
        -- sl.switch_state_machine_to("lps:post_disconnect")
        {
          class = "RdbNotified",
          name = self.rdbLpPrefix .. "status",
          value = "down",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "disconnected",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "status_ipv6",
          value = "down",
        },
        -- sl.switch_state_machine_to("lps:transit")
        -- sl.switch_state_machine_to("lps:connect")
        {
          class = "RdbNotified",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "establishing",
        },
        -- sl.switch_state_machine_to("lps:connect_ipv4") now
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "start_rmnet",
          arg = {
            network_interface=lp.network_interface,
            profile_index=lp.rdb_members.module_profile_idx,
            service_id=lp.lp_index,

            apn="",
            pdp_type="ipv4",
            auth_type="",
            user="",
            pass="",

            ipv6_enable=false,
          },
          deepCmp = true,
        },
      -- sl.switch_state_machine_to("lps:post_connect_ipv4",delay) with delay=30s
      -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
      },
    },
    -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_rmnet_change watcher callback - informing IPv4 up",
      type = "sys", event = "modem_on_rmnet_change",
      arg = {
        ip_family=4,
        service_id=3,
        status="up",
        call_end_reason="CALLENDREASON",
        verbose_call_end_reason_type="TYPE",
        verbose_call_end_reason=99,
      },
      expectList = {
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result",
          value = "CALLENDREASON",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result_verbose",
          value = "TYPE #99",
        },
        -- sl.switch_state_machine_to(next_sls[sl_stat]) --> lps:post_connect_ipv4 ...
        -- sl.switch_state_machine_to("lps:post_connect")
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "get_rmnet_stat",
          arg = {
            service_id=3,
            info={},
          },
          retVal = true,
          deepCmp = true,
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "interface",
          value = lp.network_interface,
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "established",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "status",
          value = "up",
        },

      },
    },
    -- --------------------------------------
    -- update data and write to writeflag rdb
    -- --------------------------------------
    {
      class = "OnRdbTest",
      name = "LinkProfile - writing APN to linkprofile rdb",
      rdbName = self.rdbLpPrefix .. "apn",
      rdbValue = "sample-apn",
      expectList = {},
    },
    {
      class = "OnRdbTest",
      name = "LinkProfile RDB writeflag after writting APN to linkprofile rdb",
      rdbName = self.rdbLpPrefix .. "writeflag",
      rdbValue = "1",
      expectList = {
        -- sl.switch_state_machine_to("lps:transit")
        -- sl.switch_state_machine_to("lps:update")
        -- self:invoke_stop_rmnet(lp,false)
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "stop_rmnet",
          arg = {
            profile_index=lp.rdb_members.module_profile_idx,
            service_id=3,
          },
          retVal = true,
        },
      -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
      },
    },
    -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_rmnet_change watcher callback - informing Ipv4 down",
      type = "sys", event = "modem_on_rmnet_change",
      arg = {
        ip_family=4,
        service_id=3,
        status="down",
        call_end_reason="CALLENDREASON",
        verbose_call_end_reason_type="TYPE",
        verbose_call_end_reason=99,
      },
      expectList = {
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result",
          value = "CALLENDREASON",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result_verbose",
          value = "TYPE #99",
        },
        -- sl.switch_state_machine_to("lps:transit")
        -- state machine "lps:update" again as writeflag still = true
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "write_rmnet",
          arg = {
            profile_index=lp.rdb_members.module_profile_idx,
            apn="sample-apn",
            pdp_type="ipv4",
            auth_type="",
            user="",
            pass="",
          },
          retVal = true,
          deepCmp = true,
        },
        -- sl.switch_state_machine_to("lps:transit")
        -- sl.switch_state_machine_to("lps:connect")
        {
          class = "RdbNotified",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "establishing",
        },
        -- sl.switch_state_machine_to("lps:connect_ipv4") now
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "start_rmnet",
          arg = {
            network_interface=lp.network_interface,
            profile_index=lp.rdb_members.module_profile_idx,
            service_id=lp.lp_index,

            apn="sample-apn",
            pdp_type="ipv4",
            auth_type="",
            user="",
            pass="",

            ipv6_enable=false,
          },
          deepCmp = true,
        },
      -- sl.switch_state_machine_to("lps:post_connect_ipv4",delay) with delay=30s
      -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
      },
    },
    -- somethings need to invoke sys:modem_on_rmnet_change and mark online status of IPv4 and IPv6
    {
      class = "OnWatchCbTest",
      name = "sys:modem_on_rmnet_change watcher callback - informing IPv4 up",
      type = "sys", event = "modem_on_rmnet_change",
      arg = {
        ip_family=4,
        service_id=3,
        status="up",
        call_end_reason="CALLENDREASON",
        verbose_call_end_reason_type="TYPE",
        verbose_call_end_reason=99,
      },
      expectList = {
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result",
          value = "CALLENDREASON",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_result_verbose",
          value = "TYPE #99",
        },
        -- sl.switch_state_machine_to(next_sls[sl_stat]) --> lps:post_connect_ipv4 ...
        -- sl.switch_state_machine_to("lps:post_connect")
        {
          class = "WatchCbInvoked",
          type = "sys",
          event = "get_rmnet_stat",
          arg = {
            service_id=3,
            info={},
          },
          retVal = true,
          deepCmp = true,
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "interface",
          value = lp.network_interface,
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "connect_progress",
          value = "established",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "status",
          value = "up",
        },

      },
    },
    -- sys read_linkprofiles
    {
      class = "OnWatchCbTest",
      name = "sys:read_linkprofiles watcher callback",
      type = "sys", event = "read_linkprofiles",
      arg = nil,
      expectList = {},
    },
    -- sys read_rmnet_on_read
    {
      class = "OnWatchCbTest",
      name = "sys:read_rmnet_on_read watcher callback",
      type = "sys", event = "read_rmnet_on_read",
      arg = {
        ref=lp,
        profile_index=3,
        apn="sample-apn2",
        user="user",
        pass="pass",
        pdp_type="ipv4",
        auth_type="foobar",
      },
      expectList = {
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "apn",
          value = "sample-apn2",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "user",
          value = "user",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pass",
          value = "pass",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "pdp_type",
          value = "ipv4",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "auth_type",
          value = "foobar",
        },
        {
          class = "RdbWritten",
          name = self.rdbLpPrefix .. "module_profile_idx",
          value = "3",
        },

      },
    },
    -- sys wakeup_linkprofiles
    {
      class = "OnWatchCbTest",
      name = "sys:wakeup_linkprofiles watcher callback",
      type = "sys", event = "wakeup_linkprofiles",
      arg = nil,
      expectList = {},
    },
    {
      class = "OnWatchCbTest",
      name = "sys:stop_linkprofiles watcher callback",
      type = "sys", event = "stop_linkprofiles",
      arg = nil,
      expectList = {
      -- onhold = true and sl.switch_state_machine_to("lps:disconnect") etc. similar to previous tests
      },
    },
  }

  self:installTestList(testData)
end

if not modName then
  local linkProfileUt = LinkProfileUt:new()
  linkProfileUt:setup()
  linkProfileUt:run()

end

return LinkProfileUt