#!/usr/bin/env lua

_G.TURBO_SSL=true

local turbo = require("turbo")
local ioloop = turbo.ioloop.instance()

local check_crl = require("check_crl_support")

check_crl.init("testing_app", "/etc/ssl/certs/", true)

local ssl_opts = {
  -- testing CA certificates
  -- to test SAS server
  ca_dir = "/etc/ssl/certs/",
  -- to test other public servers
  -- need to copy CA certificate file from your computer (e.g. /etc/ssl/certs/ca-certificates.crt) to target
  ca_path = "/tmp/ca-certificates.crt",

  -- add folowing option for verify CRL
  verify_callback = check_crl.verify_callback
}

local kwargs = {
  ['method']     = 'GET',
  ['user_agent'] = 'Checking CRL Agent'
}

local client = turbo.async.HTTPClient(ssl_opts, ioloop)

local testing = {
  -- SAS server, server certificate is OK, but TLS handshake will fail because client certificate is not provided in this test
  {
    expected = "Server certificate is OK, but TLS handshake fails because client certificate is not provided",
    urls = {
      "https://prod.sascms-prod.net:8443/v1.2/registration"
    }
  },

  -- public testing servers; certificates are revoked
  {
    expected = "Revoked",
    urls = {
      "https://baltimore-cybertrust-root-revoked.chain-demos.digicert.com",
      "https://assured-id-root-ca-revoked.chain-demos.digicert.com",
      "https://assured-id-root-g2-revoked.chain-demos.digicert.com",
      "https://assured-id-root-g3-revoked.chain-demos.digicert.com",
      "https://global-root-ca-revoked.chain-demos.digicert.com",
      "https://global-root-g2-revoked.chain-demos.digicert.com",
      "https://global-root-g3-revoked.chain-demos.digicert.com",
      "https://ev-root-revoked.chain-demos.digicert.com",
      "https://trusted-root-g4-revoked.chain-demos.digicert.com"
    }
  },

  -- public testing servers; certificates are OK
  {
    expected = "OK",
    urls = {
      "https://global-root-ca.chain-demos.digicert.com/",
      "https://baltimore-cybertrust-root.chain-demos.digicert.com/",
      "https://assured-id-root-ca.chain-demos.digicert.com/"
    }
  }
}

local num_testing_set = #testing
local testing_set_idx = 1
local testing_set_item_idx = 0

function query()
  if testing_set_idx <= num_testing_set then
    testing_set_item_idx = testing_set_item_idx + 1
    if testing_set_item_idx > #testing[testing_set_idx].urls then
      testing_set_item_idx = 0
      testing_set_idx = testing_set_idx + 1
    else
      print("EXPECTED: "..testing[testing_set_idx].expected..". URL: "..testing[testing_set_idx].urls[testing_set_item_idx])
      local res = coroutine.yield(client:fetch(testing[testing_set_idx].urls[testing_set_item_idx], kwargs))
      if res.error then
        print("REQUEST IS FAILED. Error: "..res.error.message)
      else
        print("REQUEST IS OK")
      end
    end
    ioloop:add_callback(query)
  else
    print("DONE")
    ioloop:close()
  end
end

ioloop:add_callback(query)

ioloop:start()
