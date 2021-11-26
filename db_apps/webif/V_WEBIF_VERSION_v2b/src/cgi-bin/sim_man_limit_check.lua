-- SIM Management CGI code
--
-- This code builds the structure used for processing CGI interaction with the sim_management WebUI
-- page, specifically the SMS usage and data usage limit conditions.  Each of the four corresponding
-- page objects has a simple custom Lua script that requires this one.
-- A custom Lua script is needed for these cases because we are mapping one of three
-- selectVariable() widgets onto a single RDB variable.
--
-- Copyright (C) 2018 NetComm Wireless Limited.

-- Generate an object handler for the specified limit_quantity ('sms' or 'data') and sim (1 or 2).
-- The structure and behaviour is the same as for an automatically generated Lua function with the
-- exception of the mapping between the one service.simmgmt.?.on_???_limit.period_start RDB variable
-- and the three possible selectVariable() widgets that permit user update.
function build_obj_handler(limit_quantity, sim)
    local cgi_base = string.format("on_%s_limit_%d_", limit_quantity, sim)
    local rdb_base = string.format("service.simmgmt.%d.on_%s_limit.", sim, limit_quantity)

    return {
      pageURL='/sim_management.html',
      authenticatedOnly=true,
      get=function(authenticated)
        local o = {}
        luardb.lock()
        local period_type = getRdb(rdb_base .. 'period_type')
        local period_start = getRdb(rdb_base .. 'period_start')
        o[cgi_base .. 'enabled'] = getRdb(rdb_base .. 'enabled')
        o[cgi_base .. 'weight'] = getRdb(rdb_base .. 'weight')
        o[cgi_base .. 'max_count'] = getRdb(rdb_base .. 'max_count')
        o[cgi_base .. 'period_type'] = period_type
        if period_type == 'month' then
            o[cgi_base .. 'month_select'] = period_start
        elseif period_type == 'week' then
            o[cgi_base .. 'week_select'] = period_start
        else
            o[cgi_base .. 'day_select'] = period_start
        end
        luardb.unlock()
        return o
      end,
      validate=function(o)
        local v
        v=o[cgi_base .. 'enabled']
        if not isValid.Bool(v) then
            return false, 'oops! '..rdb_base .. 'enabled'
        end
        if not toBool(v) then
            return true
        end
        if not isValid.BoundedInteger(o[cgi_base .. 'weight'], 1, 5) then
            return false,'oops! weight'
        end
        if not isValid.BoundedInteger(o[cgi_base .. 'max_count'],1,10000) then
            return false,'oops! max_count'
        end
        if not isValid.Enum(o[cgi_base .. 'period_type'],{'month','week','day'}) then
            return false,'oops! period_type'
        end
        if not isValid.BoundedInteger(o[cgi_base .. 'month_select'],1,28) then
            return false,'oops! month_select'
        end
        if not isValid.BoundedInteger(o[cgi_base .. 'week_select'],1,7) then
            return false,'oops! week_select'
        end
        if not isValid.BoundedInteger(o[cgi_base .. 'day_select'],0,23) then
            return false,'oops! day_select'
        end
        return true
      end,
      set=function(authenticated,o)
        local v
        luardb.lock()
        repeat

            local enabled = o[cgi_base .. 'enabled']
            setRdb(rdb_base .. 'enabled', enabled)
            if not toBool(enabled) then
                break
            end
            setRdb(rdb_base .. 'weight', o[cgi_base .. 'weight'])
            setRdb(rdb_base .. 'max_count', o[cgi_base .. 'max_count'])
            period_type = o[cgi_base .. 'period_type']
            setRdb(rdb_base .. 'period_type', period_type)
            local period_start
            if period_type == 'month' then
                period_start = o[cgi_base .. 'month_select']
            elseif period_type == 'week' then
                period_start = o[cgi_base .. 'week_select']
            else
                period_start = o[cgi_base .. 'day_select']
            end
            setRdb(rdb_base .. 'period_start', period_start)
        until(true)
        luardb.unlock()
      end
    }
end