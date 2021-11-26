#!/usr/bin/env lua

--[[
    ODU firmware/config web services

    Copyright (C) 2021 Casa Systems Inc.
--]]

require("luasyslog")
luasyslog.open("odu_files_services")

TURBO_SSL = true
local m_turbo = require("turbo")
local m_httpserver = require("turbo.httpserver")
local m_ioloop = m_turbo.ioloop.instance()

local m_config = require("odu_dynamic_installer.config")
local m_auth = require("odu_dynamic_installer.authorization")
local m_cert = require("odu_dynamic_installer.cert_manager")

local function headers_callback(headers)
    local auth_header = headers:get("Authorization")
    if auth_header then
        local prefix_s, prefix_e = auth_header:find("Basic ")
        if not prefix_e then
            return false, m_httpserver.HTTPRequestError:new(401)
        end

        return m_auth.check_password(auth_header:sub(prefix_e + 1)), m_httpserver.HTTPRequestError:new(401)
    else
        return false, m_httpserver.HTTPRequestError:new(401)
    end

end

local m_FileUploadHandler = require("odu_dynamic_installer.rh_odu_upload")
m_FileUploadHandler.uri = "/odu_upload"

require("odu_dynamic_installer.monitor_odu_reboot").setup(m_ioloop)

local handlers = {
    {"^/odu_status$", require("odu_dynamic_installer.rh_odu_status")},
    {"^/odu_files_list$", require("odu_dynamic_installer.rh_odu_files_list")},
    {"^"..m_FileUploadHandler.uri.."$", m_FileUploadHandler},
    {"^/odu_file_info$", require("odu_dynamic_installer.rh_odu_file_info")},
    {"^/odu_push_file$", require("odu_dynamic_installer.rh_odu_push_file")},
    {"^/odu_delete_files$", require("odu_dynamic_installer.rh_odu_delete_files")},
    {"^/change_password$", require("odu_dynamic_installer.rh_change_password")}
}

local m_app = m_turbo.web.Application:new(handlers, {application_name = "ODU files service"})

local m_ssl_options = {
    cert_file = m_cert.cert_file,
    key_file = m_cert.key_file,
    priv_pass = m_cert.key_passphrase
}

m_app:listen(tonumber(m_config.port), nil, {
    max_body_size = 200*1024*1024,
    max_non_file_size = 512*1024,
    build_file_upload_full_path = m_FileUploadHandler.buildFileUploadFullPath,
    headers_callback = headers_callback,
    ssl_options = m_ssl_options
})

m_ioloop:start()

luasyslog.close()
