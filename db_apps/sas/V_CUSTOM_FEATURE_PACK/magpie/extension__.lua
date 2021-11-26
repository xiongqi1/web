--[[
  SAS client decorator for Magpie

  Copyright (C) 2019 Casa Systems Inc.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")
local MagpieDecorator = WmmdDecorator:new()
local SasMachineDecorator = WmmdDecorator:new()
local SasClientDecorator = WmmdDecorator:new()

-- Prepare the cell lock list for attaching for first registration/grant
function SasClientDecorator:prepare_install_cell_lock_list()
  -- initialize last cell scan sequence number
  if not self.last_seq or self.last_seq == "" then
    self.last_seq = self.rdb.get("wwan.0.manual_cell_meas.seq")
    return false
  end

  local seq = self.rdb.get("wwan.0.manual_cell_meas.seq")
  if self.last_seq ~= seq then
    self.last_seq = seq
    local temp_table = {}     -- all cells
    local cell_table = {}
    local cell_table_lock = {}-- installer cells for locking sorted on rsrp, assuming the required cell has best rsrp
    local dup_table = {}      -- to avoid duplicate addition
    local cell_rsrp_tab = {}  -- cell measurement data, for rsrp sorting

    -- get rsrp for the cells
    local qty = tonumber(self.rdb.get("wwan.0.manual_cell_meas.qty")) or 0
    for j=0,qty-1 do
      -- type,earfcn,pci,rsrp,rsrq
      local cell_data = self.rdb.get(string.format("wwan.0.manual_cell_meas.%s", j))
      self.l.log("LOG_DEBUG", string.format("rrc_info cell data : %s", cell_data))
      local cell = cell_data:split(",")
      cell_rsrp_tab[cell[3]..cell[2]] = tonumber(cell[4])
    end

    local qty = tonumber(self.rdb.get("wwan.0.rrc_info.cell.qty")) or 0
    for j=0,qty-1 do
      -- mcc,mnc,earfcn,pci,cellId
      local cell_data = self.rdb.get(string.format("wwan.0.rrc_info.cell.%s", j))
      self.l.log("LOG_DEBUG", string.format("rrc_info cell data : %s", cell_data))
      local cell = cell_data:split(",")
      table.insert(temp_table, cell)
      dup_table[cell[4]..cell[3]] = true
    end

    for i=0,self.MAX_FILTER_CELLS-1 do
      local install_cell_ecgi = self.rdb.get(string.format("installation.cell_filter.%s.ecgi", i)) or ""
      local install_cell_heading = self.rdb.get(string.format("installation.cell_filter.%s.heading", i)) or ""

      if install_cell_ecgi ~= "" then
        for j=1,#temp_table do
          cell = temp_table[j]
          local plmn = cell[1]..cell[2]
          local cid = tonumber(cell[5])
          -- ecgi should be 15 digits
          local str = plmn.."%0"..(15 - #plmn).."d"
          local ecgi = string.format(str, cid)

          if ecgi == install_cell_ecgi then
            self.l.log("LOG_DEBUG", string.format("index %d: cell found with install cell ecgi: %s, heading: %s",
              j, install_cell_ecgi,install_cell_heading))
            -- make sure it has not been added before, and has a rsrp value
            if dup_table[cell[4]..cell[3]] and cell_rsrp_tab[cell[4]..cell[3]] then
              -- pci, earfcn, rsrp, heading
              table.insert(cell_table, {cell[4], cell[3], cell_rsrp_tab[cell[4]..cell[3]], install_cell_heading})
              dup_table[cell[4]..cell[3]] = false
            end
            table.remove(temp_table, j)
            break
          end
        end
      end
    end
    if not cell_table[1] then return false end

    -- sort based on rsrp, max at top
    table.sort(cell_table, function(a,b) return a[3] > b[3] end)

    -- use the first cell as the reference cell and set lock for all cells with same heading
    table.insert(cell_table_lock, "pci:"..cell_table[1][1]..",earfcn:"..cell_table[1][2])
    for i=2,#cell_table do
      if cell_table[1][4] == cell_table[i][4] then
        table.insert(cell_table_lock, "pci:"..cell_table[i][1]..",earfcn:"..cell_table[i][2])
      end
    end

    self.install_cell_list = table.concat(cell_table_lock, ";")
    self.l.log("LOG_NOTICE", string.format("install lock list - %s", self.install_cell_list))
    return true
  end
  return false
end

function SasClientDecorator:doDecorate()
  SasClientDecorator:__changeImplTbl({
    "prepare_install_cell_lock_list",
    })
end

function SasMachineDecorator:log_fetch_error_ctx(ctx, res)
  SasMachineDecorator:__invokeChain("log_fetch_error_ctx")(self, ctx, res)
  -- dump specific magpie RDBs
  rdbList = { "link.profile.1.status",
              "link.profile.1.iplocal",
              "link.profile.1.mask",
              "link.profile.4.status",
              "link.profile.4.iplocal",
              "link.profile.4.mask",
              "link.profile.5.status",
              "link.profile.5.iplocal",
              "link.profile.5.mask",
            }
  for i=1, #rdbList do
     self.l.log("LOG_NOTICE", string.format('%s = %s', rdbList[i], self.rdb.get(rdbList[i]) or 'empty'))
  end
end

function SasMachineDecorator:doDecorate()
  SasMachineDecorator:__saveChain("log_fetch_error_ctx")
  SasMachineDecorator:__changeImplTbl({
    "log_fetch_error_ctx",
    "prepare_install_cell_lock_list",
    })
end

function MagpieDecorator.doDecorate()
  MagpieDecorator.__inputObj__.sasc = SasClientDecorator:decorate(MagpieDecorator.__inputObj__.sasc)
  MagpieDecorator.__inputObj__.sasc.qs.sas_m = SasMachineDecorator:decorate(MagpieDecorator.__inputObj__.sasc.qs.sas_m)
end

return MagpieDecorator
