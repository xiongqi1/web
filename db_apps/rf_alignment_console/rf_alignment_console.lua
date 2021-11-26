#!/usr/bin/lua

require('luardb')
require('termlib')
require('processmgr')
require('stringutil')


local cfg={
	TTY_DEVICE_NAME = '/dev/tty',
	CONSOLE_DEVICE_NAME = '/dev/console',
	SERIAL_DEVICE_NAME = '/dev/ttyO0',
	SERIAL_DEVICE_OPTIONS = '115200',

	-- Enable Cell delta checking
	-- If enabled cells that may interfere with each other (same RF Channel)
	-- Are highlighted, this is used for NBN but is disabled for 
	-- Other networks at this stage
	-- This code has been copied from the NBN RF Alignment console
	-- In case it may be useful in future but has not been tested
	-- If you set this to true you'll need to test that it works. 
	ENABLE_CELL_DELTA_CHECK = false, 

	-- How many cells shall we display ?
	MAX_REPORTED_CELLS = 5,

	RF_START_COL=1,
	BAR_COLS_NUM=24,

	POLL_TIMEOUT =0.5,  --second
	REFRESH_SCREEN_COUNT = 50, -- about 0.5*50 second
	BUILD_FREQ_TIMEOUT=30,

	LONG_REFRESH_SCREEN_COUNT = 40, -- about 5*40 second
	LONG_POLL_TIMEOUT =5,  --second

	RF_TIME_TO_CENTER=50, -- it takes about 50s for caret move to the center


	-- assign a large value, later this value will be fixed from time interface
	RF_HISTORY_SIZE =1001,
	RF_HISTORY_SIZE_SECOND =120,

	RF_ERRMSG_COL=30,
	RF_ERRMSG_NUM=30,

	MAX_COL=120,
	MAX_ROW=40,


--[[
	BG_COLOR = termlib.colour.bg.white,
	FG_COLOR = termlib.colour.fg.black,
	FG_BRGIHT	=termlib.attr.dim,
	BAR_COLOR_NORMAL =termlib.colour.bg.black,
	BAR_COLOR_ALERT	 =termlib.colour.bg.red,
	BAR_COLOR_END	 = termlib.colour.bg.white,
	CARET_COLOR = termlib.colour.fg.red,
	CARET_COLOR_END=termlib.colour.fg.black,
	COLOR_REVERSE=false,
--]]


--[[
	BG_COLOR = termlib.colour.bg.black,
	FG_COLOR = termlib.colour.fg.white,
	FG_BRGIHT	=termlib.attr.bright,
	BAR_COLOR_NORMAL =termlib.colour.fg.black,
	BAR_COLOR_ALERT	 =termlib.colour.fg.red,
	BAR_COLOR_END	=termlib.colour.fg.white,
	CARET_COLOR = termlib.colour.bg.red,
	CARET_COLOR_END=termlib.colour.bg.black,
	COLOR_REVERSE=true,
--]]

---[[
	BG_COLOR = termlib.colour.bg.black,
	FG_COLOR = termlib.colour.fg.white,
	FG_BRGIHT	=termlib.attr.bright,
	BAR_COLOR_NORMAL =termlib.colour.bg.green,
	BAR_COLOR_ALERT	 =termlib.colour.bg.red,
	BAR_COLOR_END	=termlib.colour.bg.black,
	CARET_COLOR = termlib.colour.fg.red,
	CARET_COLOR_END=termlib.colour.fg.white,
	CARET_GREEN = termlib.colour.fg.green,

	COLOR_REVERSE=false,
--]]

}

local rdb_dccd_mode='service.dccd.mode'
local console_mode_online='on_line'
local console_mode_rf_qualification ='rf_qualification'
local console_mode_service_qualification='service_qualification'
local rdb_indoor_product_id
local rdb_indoor_comms_failed;
local usr_bin

-- test mode, loca tr143_http and tr143_ping
local test = false;


if string.match(arg[0], '%./') then
	test = true;
end

if test then
--- debug enable option

	rdb_indoor_product_id='indoor.product_id.test'
	rdb_indoor_comms_failed = 'indoor.comms_failed.test'
	usr_bin= "./" --="/usr/bin/"
else
	rdb_indoor_product_id='indoor.product_id'
	rdb_indoor_comms_failed = 'indoor.comms_failed'
	usr_bin= "/usr/bin/"
	package.path = package.path .. ";/usr/bin/?.lua"

end
local debug =1;

local function dinfo( ...)
	if debug >1 then
		local  printResult="";
		for i = 1, select('#', ...) do
			local v = select(i, ...)
			printResult = printResult .. tostring(v) .. "\t"
		 end
		print(printResult)
	end
end


local fresh_count=0;

term = termlib.new()
local a_tty = os.execute("tty -s >/dev/null 2>&1")
if a_tty == 0 then
	term:open(cfg.TTY_DEVICE_NAME)
else
	a_tty = os.execute("tty -s <"..cfg.CONSOLE_DEVICE_NAME..">/dev/null 2>&1")
	if a_tty == 0 then
		term:open(cfg.CONSOLE_DEVICE_NAME)
	else
		term:open(cfg.SERIAL_DEVICE_NAME, cfg.SERIAL_DEVICE_OPTIONS)
	end
end

local function dump_table(t, level)

	if debug >= 1 then
		if not level then level =0 end
		for name, v in pairs(t) do
			name =string.rep('-', level).." "..name;
			print(name, v);
			if type(v) =='table' then
				dump_table(v, level+1);
			end
		end
	end
end

-- rest term, set bg fg color and other default term state
local function term_reset()

	term:setAttr(term.attr.reset);
	term:setColour(cfg.BG_COLOR);
	term:setColour(cfg.FG_COLOR);
	term:setAttr(cfg.FG_BRGIHT);
	if cfg.COLOR_REVERSE then
		term:setAttr(term.attr.reverse);
	end
	term:enableLineWrap(false);
	term:cursorHide();

end
-- reset term, erase entire screen
local function term_clear()
	term:reset();
	term_reset()
	--term:clear();
	if not cfg.COLOR_REVERSE then
		term:eraseScreen();
	else
		local str = string.rep(' ', cfg.MAX_COL)
		for i =1, cfg.MAX_ROW do
			term:moveTo(1,i)
			term:write(str);
		end
	end

end
-- erase one line
local function erase_line(cur_col, cur_row)

	term:moveTo(cur_col, cur_row);
	if not cfg.COLOR_REVERSE then
		term:eraseToEOL();
	else
		local str = string.rep(' ', cfg.MAX_COL-cur_col)
		term:write(str);
	end
end
--[[
-- draw test at (x, y)
-- width -- field width
-- left_align -- true -- left aligned
				false -- right aligned
--]]
local function draw_text(x, y, str, width, left_align)

	local len = string.len(str);
	--dinfo(len , width, str);
	term:moveTo(x, y);
	if len < width then
		if left_align then
			str = str ..string.rep(' ' , width-len)
		else
			str = string.rep(' ' , width-len)..str
			--term:moveTo(x+ width-len, y);
		end
	end
	term:write(str);

end

--[[
	draw a bar
	[bbbbbbbbbbbsssssssss]
	length -- length for 'b'
	max_length -- length for 'b' +'s'
	bar_color -- color for 'b'
	bar_color_end -- color for 's'
--]]
function bar_str( length, max_length, bar_color, bar_color_end)

	--term:setColour(cfg.FG_COLOR);

	term:write("|");

	term:setColour(bar_color)

	local str;
	str = string.rep(' ', length);
	--term:setAttr(term.attr.reverse);

	term:write(str);

	--term:setAttr(term.attr.reset);
	--term_reset();

	term:setColour(bar_color_end)

	if length < max_length then
		str = string.rep(' ', max_length - length);
		str = str;
		term:write(str);
	end

	term_reset();

	term:write("|");


end

local function round( value, min, max)

	if value < min  then return min; end;
	if value > max  then return max; end;
	return value;
end

--[[
	draw bar
				    min_value          max_value
	name_attribute |#######            |
					rel_start_col		rel_start_col+cols_num

--]]
local function draw_bar(x,
					y,
					unit,
					rel_start_col,
					cols_num,
					item,
					bar_color,
					bar_color_end)


	local value = item.value;
	local min_value = item.min;
	local max_value = item.max;
	local caret_val1 = item.low_value;
	local caret_val2 = item.high_value;

	--dinfo(item.rdb, caret_val1, caret_val2);

	local start_col =x + rel_start_col;
	local end_col = start_col + cols_num;

	-- min/max label value
	--term:setColour(term.colour.fg.green);
	draw_text(start_col-9, y,  string.format("%+d" ..unit.."<", min_value), 9, false);

	draw_text(end_col+2, y, string.format("<%+d"..unit,max_value), 9, true);


	-- bar
	if value ==0 and value > max_value then
		-- if rssi is 0, change to the min
		value = min_value;
	end

	local percent = (value- min_value)*100/(max_value-min_value);

	percent = round(percent, 0, 100);

	local cur_pos = math.floor(cols_num *percent/100 +0.5);

	term:moveTo(start_col, y);

	bar_str(cur_pos, cols_num, bar_color, bar_color_end);



	--term:setColour(term.colour.fg.blue);

	--term:write(temp_str);

	--- caret
	if caret_val1 and caret_val2 then
		erase_line(start_col, y+1)
		term:setColour(cfg.CARET_COLOR);
		percent = (caret_val1- min_value)*100/(max_value-min_value);
		percent =round(percent, 0, 100);

		cur_pos = math.floor(cols_num *percent/100 + start_col+1 +0.5);
		term:moveTo(cur_pos, y+1);
		term:write('^');

		percent = (caret_val2- min_value)*100/(max_value-min_value);
		percent = round(percent, 0, 100);
		cur_pos = math.floor(cols_num *percent/100 + start_col+1 +0.5);
		term:moveTo(cur_pos, y+1);
		term:write('^');
		--term:setAttr(term.attr.reset);
		term:setColour(cfg.CARET_COLOR_END);
		--term_reset()
	end


end


local function draw_line(x, y, length, char)
	length = length or 80
	char = char or '-'
	term:moveTo(x, y);
	term:write(string.rep(char, length));
end




local function draw_table_header(obj_table, start_col, start_row, row)
	local str
	for _, obj in pairs(obj_table) do
		local s = '';
		if not row then
			s= obj.title;
		elseif  obj['title'..row] then
			s = obj['title'..row]
		end
		if obj.hide_header then s ='' end
		local width= obj.title_width or obj.width
		local l = string.len(s);
		if l < width then
			if not obj.title_align  then -- center
				s = s..string.rep(' ', (width- l)/2);
				l = string.len(s);
				s = string.rep(' ', (width- l))..s;
			elseif obj.title_align == 'left' then
				s = s..string.rep(' ', (width- l));
			elseif obj.title_align == 'right' then
				s = string.rep(' ', (width- l)) ..s;
			end
		end
		--dinfo(" draw_table_header", s, l, obj.width);
		if obj.hide_line then
			if not str then str =' ' end
			str = str..s..' ';
		else
			if not str then str ='|' end
			str = str..s..'|';
		end
	end
	draw_text(start_col, start_row, str, 0, true);
end

local function draw_table_line(obj_table,  start_col, start_row)
	local str="+"
	for _, obj in pairs(obj_table) do
		if obj.hide_line then
			str = str.. string.rep(' ',obj.width+1);
		else
			str = str..string.rep('-', obj.width)..'+';
		end
	end
	draw_text(start_col, start_row, str, 0, true);

end

local function get_table_width(obj_table)
	local width=0
	for _, obj in pairs(obj_table) do
		width = width + obj.width+1;
	end
	return width
end



local function draw_table_field(obj_table, start_col, start_row, stats)

	local str
	--dump_table(stats);
	for _, obj in pairs(obj_table) do
		local s
		if obj.name then
			s = stats[obj.name] or '';
		else
			s = stats[obj.title] or '';
		end
		-- the fmt is defined, reformat number
		if obj.fmt then
			-- if s is not valid number, don't format it
			if tonumber(s) then

				s= string.format(obj.fmt, s);
			end
		end

		--dump_table(obj);

		local l = string.len(s);
		if l < obj.width then
			if not obj.align then -- default right
				s = string.rep(' ',(obj.width- l))..s;
			elseif obj.align == 'left' then
				s = s..string.rep(' ',(obj.width- l));
			elseif obj.align == 'center' then
				s = s..string.rep(' ', (obj.width- l)/2);
				l = string.len(s);
				s = string.rep(' ', (obj.width- l))..s;
			end
		elseif l > obj.width then

			if type(s) == 'number' or obj.fmt then
				-- truncate the s if it contains.
				local dot_pos = string.find(s, '%.');
				if dot_pos and dot_pos <= obj.width then
					s = string.sub(s, 1, obj.width);
					--print("truncate field", s, "=>", s1, dot_pos, 'width', obj.width);
				else
					s=string.rep(' ',obj.width-1)..'*';
				end
			else
				s=string.rep(' ',obj.width-1)..'*';
			end
		end
		if obj.hide_line then
			if not str then str =' ' end
			str = str..s..' ';
		else
			if not str then str ='|' end
			str = str..s..'|';
		end
	end
	draw_text(start_col, start_row, str, 0, true);

end

local function draw_one_field(obj_table, start_col, start_row, field_title, s)

	local x= start_col;
	for _, obj in pairs(obj_table) do

		if obj.name == field_title or ( not obj.name and obj.title == field_title) then
			--dump_table(obj);
			local l = string.len(s);
			if l < obj.width then
				if not obj.align then -- default right
					s = string.rep(' ',(obj.width- l))..s;
				elseif obj.align == 'left' then
					s = s..string.rep(' ',(obj.width- l));
				elseif obj.align == 'center' then
					s = s..string.rep(' ', (obj.width- l)/2);
					l = string.len(s);
					s = string.rep(' ', (obj.width- l))..s;
				end
			elseif l > obj.width then
				s=string.rep(' ',obj.width-1)..'*';
			end
			--dinfo(s);
			draw_text(x+1, start_row, s, 0, true);
			break;
		else
			x= x+ obj.width+1
		end
	end

end


local g_rf_cfg=
{
	fresh_count=0,

	rdb_cell_qty ='wwan.0.cell_measurement.qty',
	rdb_cell_status='wwan.0.cell_measurement.status',
	rdb_cell_imei ='wwan.0.imei',
	rdb_cell_imsi = 'wwan.0.imsi',
	rdb_cell_sqnd_status ='wwan.0.sqnd_status',
	rdb_cell_ms_status='wwan.0.ms_status',
	rdb_cell_measurement_prefix='wwan.0.cell_measurement.',
	rdb_rsrp_threshold='wwan.0.rsrp.threshold',
	rdb_rsrp_threshold_delta='wwan.0.rsrp.threshold.delta',

	rdb_serialnumber='systeminfo.serialnumber',

	table_header_redraw = true,
	bar_header_redraw = true,


};

local g_rf_obj=
{

	--name='RSRP',
	SS={
		measure='signal_strength',
		min=-120,
		max=-60,
		default=-120,
		threshold=-96,
		unit='dBm',
		caret={}, -- history caret val, index by earfcn +pci caret[earfcn][pci]={high_value, low_value, value}
	},
	delta={
		measure='Delta',
		min=-10,
		max=30,
		default=30,
		threshold=7,
		unit='dB',
		caret={}, -- history caret val, index by earfcn +pci+pci caret[earfcn][pci1][pci2]={high_value, low_value, value}

	},
	SQ ={
		measure='signal_quality',
		item={}, --{value}
	},
	stats={}, 	-- history stats for RSRP table, index by earfcn,pci
				-- [earfcn][pci]={PCI, ARFCN, Result, Min_RSRP Max_RSRP, avg_RSRP, Med_RSRP Delta_RSRP, CINR}
	item={},	--{pci, earfcn, result, min, max, value, high_value, low_value,
				--	delta={pci={min, max, value, high_value, low_value}

}

if cfg.ENABLE_CELL_DELTA_CHECK == true then
g_rsrp_stats_table=
	{

		{
			title='CID',
			hide_line =true,
			align='left',
			width=3,
		},

		{
			title='RFCN',
			hide_line =true,
			width=5,
		},

		{
			title='Result',
			hide_line =true,
			width=5,
		},
		{
			title='Min_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',

		},
		{
			title='Max_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',
		},
		{
			title='Avg_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',
		},
		{
			title='Med_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',
		},
		{
			title='Delta_SS',
			hide_line =true,
			width=9,
			fmt='%.1f',
		},
		{
			title='SQ',
			hide_line =true,
			title_width=4,
			width=6,
			fmt='%.1f',
		},
	}
else
g_rsrp_stats_table=
	{

		{
			title='CID',
			hide_line =true,
			width=3,
		},

		{
			title='RFCN',
			hide_line =true,
			width=5,
		},

		{
			title='Min_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',
			title_align='right',

		},
		{
			title='Max_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',
			title_align='right',
		},
		{
			title='Avg_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',
			title_align='right',
		},
		{
			title='Med_SS',
			hide_line =true,
			width=8,
			fmt='%.1f',
			title_align='right',
		},
		{
			title='Signal Quality',
			name='SQ',
			hide_line =true,
			title_width=14,
			width=6,
			fmt='%.1f',
		},
	}
end


local g_cell_types =
{
	G='GSM',
	U='UMTS',
	E='LTE'
}

-- Each network type (RAT) measures signal strength and quality differently
-- This table allows us to display the correct measurment type for eact RAT
local g_signal_strength_types =
{
	G='RSSI',
	U='RSCP',
	E='RSRP'
}

local g_signal_quality_types =
{
	G='',
	U='ECIO',
	E='RSRQ'
}



-- speed to leave the center.
--- eg: 10 mean, it need 10 step to reach new value point
local STEP_TO_LEAVE=1;
local STEP_TO_CENTER=100;

local function move_caret( old_value, new_value, steps)

	distance = (new_value - old_value)/steps;
	return old_value + distance;

end
local function init_table(t, ...)
	for i, v in ipairs(arg) do
		if not t[v] then
			t[v]={}
		end
		t=t[v]
	end
end

local function set_item_sig_value(caret, item, value)
	value = tonumber(value);

	-- Don't trim the value here, do it at display time if needed.
	--if value  < item.min then value = item.min; end
	--if value  > item.max then value = item.max; end
	--load caret value
	if caret.high_value then
		item.high_value = caret.high_value;
		item.low_value = caret.low_value;
	else
		item.high_value = value;
		item.low_value = value;

	end
	--dump_table(caret);
--	if not value then
--		value = item.default;
--	end

	--dinfo(name, value);

	item.value = value;


	if value >= item.low_value and value <= item.high_value then
		item.low_value = move_caret(item.low_value, value, STEP_TO_CENTER);
		item.high_value = move_caret(item.high_value, value, STEP_TO_CENTER);
	else
		if  value < item.low_value  then
			item.low_value = move_caret(item.low_value, value, STEP_TO_LEAVE);
		end

		if value > item.high_value then

			item.high_value = move_caret(item.high_value, value, STEP_TO_LEAVE);

		end

	end
	-- save caret value
	caret['high_value'] =item.high_value
	caret['low_value'] = item.low_value
	--caret['value'] = value

	--dinfo(value,  item.min, item.max, item.low_value,item.high_value);
end


-- reset rsrp stats
local function rf_reset_stats(obj)
	for earfcn, pci_list in pairs(obj.stats) do
		for j, dj in pairs(pci_list) do
			dj['new_data'] = nil;
		end
	end
end

-- update rsrp stats
--local function rf_update_stats(obj, earfcn, freq, pci, rsrp, cinr)
local function rf_update_stats(obj, cell_type, channel, freq, cell_id, signal_strength, signal_quality)

	signal_strength = tonumber(signal_strength);
	--rsrp = round(rsrp, obj.rsrp.min, obj.rsrp.max)
	signal_quality = tonumber(signal_quality);
	init_table(obj.stats, channel, cell_id, 'history');

	if #obj.stats[channel][cell_id].history > cfg.RF_HISTORY_SIZE then
		table.remove(obj.stats[channel][cell_id].history,1);
	end
	table.insert(obj.stats[channel][cell_id].history, {['SS']=signal_strength, ['SQ']=signal_quality,});

	--dinfo("rf_update_stats, add value ".. rsrp);

	local t ={};
	local sum_sq=0;
	local sum_ss =0;
	for _, x in pairs(obj.stats[channel][cell_id].history) do
		table.insert(t, x.SS);
		sum_sq = sum_sq + x.SQ
		sum_ss = sum_ss + x.SS
	end
	--dump_table(obj.stats[earfcn][pci].history);
	table.sort(t);

	local n = math.floor(#t/2+0.5)
	obj.stats[channel][cell_id].CID = cell_id;
	obj.stats[channel][cell_id].RFCN= channel;
	obj.stats[channel][cell_id].FREQ = freq;
	obj.stats[channel][cell_id].Med_SS=  t[n];

	obj.stats[channel][cell_id]['new_data'] = true;

	-- record min and max value since run
	if not obj.stats[channel][cell_id].Min_SS or obj.stats[channel][cell_id].Min_SS > signal_strength then
		obj.stats[channel][cell_id].Min_SS  = signal_strength
	end
	if not obj.stats[channel][cell_id].Max_SS or obj.stats[channel][cell_id].Max_SS < signal_strength then
		obj.stats[channel][cell_id].Max_SS = signal_strength;
	end

	--dinfo(sum_rsrp, #t, cfg.RF_HISTORY_SIZE);
	obj.stats[channel][cell_id].Avg_SS= sum_ss/#t;
	obj.stats[channel][cell_id].SQ	= sum_sq/#t;


	obj.stats[channel][cell_id].alert = false;

	if  obj.stats[channel][cell_id].Avg_SS<= g_rf_obj.SS.threshold then
		obj.stats[channel][cell_id].alert= true
	end

	--dump_table(obj.stats[earfcn][pci])
end

local rf_scan_cell_qty_rdb;
local rd_scan_cell_qty_val;
-- when signal value changed, update into object list
local function rdb_scan_cell_qty_changed(key, val)
	rf_scan_cell_qty_rdb = key;
	rd_scan_cell_qty_val = val;
end

local function cal_min_delta(earfcn_pci_map, value_name)
	--calculte minimum [earfcn][pci][value_name]
	
	-- for each earfcn
	for earfcn, pci_list in pairs(earfcn_pci_map) do
		-- 1) for each pci, select the max pci
		local max_pci;
		local max_value = -140;

		local count=0
		for pci, di in pairs(pci_list) do
			if  di['new_data'] then
				if di[value_name] > max_value then
					max_pci = pci;
					max_value = di[value_name]
				end
				count = count +1
			end
		end
		
		if max_pci then
			if count ==1 then
				-- 2) if just one pci, mark it as maximum, if has one more pci mark with the max_delta
				pci_list[max_pci].Delta_RSRP = g_rf_obj.delta.default
			
			elseif count >1 then
				-- 3)calculte max dela value with this max pci, mark all as alert
				local max_delta = -140;-- most closing to max_pci
				for pci, di in pairs(pci_list) do
					if  di['new_data'] and pci ~= max_pci then
						local d = di[value_name] - max_value;
						if d > max_delta then
							max_delta =d;
						end
						
						di.Delta_pci = max_pci
						di.Delta_RSRP = d
						di['alert'] = true;
						--print("cal_min_delta ", "earfcn:", earfcn, "value:",di[value_name], "Delta_pic:", di.Delta_pci, "Delta_RSRP:", di.Delta_RSRP)

					end
				end
			
				pci_list[max_pci].Delta_RSRP = math.abs(max_delta)
				if pci_list[max_pci].Delta_RSRP < g_rf_obj.delta.threshold then
					pci_list[max_pci]['alert'] = true;
				end
			end
			--print("cal_min_delta ", "earfcn:", earfcn, "max_value:", max_value,  "pci:", max_pci,  "its delta RSRP:", pci_list[max_pci].Delta_RSRP)
		end
	end --for earfcn, pci_list in pairs(earfcn_pci_map) do	
end

local rf_last_update_time;

-- when signal value changed, update into object list
local function rdb_scan_cell_qty_post_update()

	local key = rf_scan_cell_qty_rdb;
	local val = tonumber(rd_scan_cell_qty_val) or 0;

	rf_scan_cell_qty_rdb =nil;
	
	--erase_line(cfg.RF_ERRMSG_COL,1)
	local errmsg;
	--1) report error when no signal
	if val ==0 then
		-- 2) check imei
		local msg, len
		if (not g_rf_cfg.imei or string.len(g_rf_cfg.imei) ==0)  then
			msg="RF Module not found";
		else
			-- 2.1) check wwan.0.cell_measurement.status
			msg = luardb.get(g_rf_cfg.rdb_cell_status) or '';

		end
		len = string.len(msg);
		-- 3) if wwan.0.cell_measurement.status  is not set, check wwan.0.sqnd_status
		if len ==0 then

			msg = luardb.get(g_rf_cfg.rdb_cell_sqnd_status) or '';
			--4) wwan.0.sqnd_status state 'ms-wait' is the Teal module problem
			if  msg == 'fatal' or msg == 'error' then
				msg= 'RF Module Faulty'
			elseif msg == 'ms-wait' then
				msg = 'Wait for MS'
			else
				msg=''
			end
			len = string.len(msg);
		end
		-- 5) bullet proof
		if len > cfg.RF_ERRMSG_NUM then
			msg ='connection error';
			len = string.len(msg);
		end
		-- 6) normal state ?
		if len >1 then
			errmsg ="<<"..msg .. string.rep(' ', cfg.RF_ERRMSG_NUM-len)..">>"
		end
	end
	--7) no report sim status, because it is avaible at all
	if not errmsg then
			-- clean message field
			errmsg = string.rep(' ', cfg.RF_ERRMSG_NUM+4);
--		end

	end
	draw_text(cfg.RF_ERRMSG_COL, 1, errmsg, 0, true);


	if not key then return end;

	local now = os.time();

	if rf_last_update_time then
		if now > rf_last_update_time then
			-- calculate the step to move to center
			STEP_TO_CENTER = cfg.RF_TIME_TO_CENTER/(now - rf_last_update_time)
			--print(now, STEP_TO_CENTER);
			-- prevent update history size too quick
			if cfg.RF_HISTORY_SIZE >1000 or cfg.RF_HISTORY_SIZE < 10 then
				cfg.RF_HISTORY_SIZE = cfg.RF_HISTORY_SIZE_SECOND/(now - rf_last_update_time)
			end
		end

	end
	rf_last_update_time =now;


	--dinfo('rdb_scan_cell_qty_changed >>>', key ,val)

	--wwan.0.cell_measurement.[n] <type>,<channel>,<cell_id>,<signal_strength>,<signal_quality>
	local rf_data={}; -- clear old data
	rf_reset_stats(g_rf_obj);
	for i =1, val do
		local t= luardb.get(g_rf_cfg.rdb_cell_measurement_prefix..i-1)
		if t then
			local l = t:explode(',');
			if #l ==5 then
				rf_data[i]=
				{
					cell_type=(l[1]),
					channel=tonumber(l[2]),
					cell_id=tonumber(l[3]),
					signal_strength=tonumber(l[4]),
					signal_quality=tonumber(l[5]),
					freq = 0;
				}

				-- update stats immediately
				rf_update_stats(g_rf_obj, rf_data[i].cell_type, rf_data[i].channel, rf_data[i].freq, rf_data[i].cell_id, rf_data[i].signal_strength, rf_data[i].signal_quality);
	
				rf_data[i].weight =rf_data[i].signal_strength;
			end
		end
	end
	--calculate min delta for stats['AVG']
	cal_min_delta(g_rf_obj.stats, 'Avg_SS');

	-- calculate min delta for rd_data['RSRP']
	--1) sort by pci, RSRP ascend order
	table.sort(rf_data,  function(a,b)  if a.channel == b.channel then return a.signal_strength > b.signal_strength else return a.channel > b.channel end end);

	--2) calculate delta  for each group of PCI
	local group_rfcn;
	local group_first_cid;
	local group_first_value;
	local group_first_i;
	local group_item_count=0;
	for i, di in ipairs(rf_data) do

		if group_rfcn ~= di.channel then
			group_rfcn = di.channel
			group_first_cid = di.cell_id;
			group_first_value = di.signal_strength
			group_first_i =i;
			di.delta = g_rf_obj.delta.default;
			di['alert'] = false;
			group_item_count =1;
		else -- same pci group	
			di.delta = di.signal_strength - group_first_value;
			di.delta_cid = group_first_cid
			di['alert'] = true;
			di.weight = di.weight -200
			-- when to update the first item in the group ?
			group_item_count = group_item_count +1
			if group_item_count ==2 then
				rf_data[group_first_i].delta = math.abs(di.delta);
				rf_data[group_first_i].delta_cid = di.cid;
				if rf_data[group_first_i].delta < g_rf_obj.delta.threshold then
					rf_data[group_first_i].alert = true;
				end
			end
		end	
		--print("rf_data","earfcn", di.earfcn, "pci:", di.pci, "RSRP", di.RSRP, "delta", di.delta, "delta_pci", di.delta_pci);
		
		
	end	
	
	
	-- sort table
	table.sort(rf_data,  function(a,b) return a.weight > b.weight end);

	--dump_table(rf_data);

	-- rebuild the rsrp/delta data list, from current scan data
	g_rf_obj.item={};
	for i, data in ipairs(rf_data) do
		--line 1 RSRP display bar item

		local item_i ={};

		cell_type = g_cell_types[data.cell_type] or '???';
		item_i.cell_type = cell_type
		item_i.cell_id   = data.cell_id;
		item_i.channel   = data.channel;
		-- earfcn to freq formula for TDD band 40 only
		--F = (ARFCN – 15650)  / 10.

		--item_i.freq =  data.freq;-- string.format("%d", (data.earfcn -15650)/10);

		item_i.min = g_rf_obj.SS.min;
		item_i.max = g_rf_obj.SS.max;
		--dump_table(g_rf_obj.rsrp.caret);
		init_table(g_rf_obj.SS.caret, data.channel, data.cell_id);

		item_i.signal_strength_type = g_signal_strength_types[data.cell_type] or '???';
		item_i.signal_quality_type = g_signal_quality_types[data.cell_type] or '???';

		set_item_sig_value(g_rf_obj.SS.caret[data.channel][data.cell_id], item_i, data[g_rf_obj.SS.measure]);
		--print(data.earfcn, data.pci, data[g_rf_obj.rsrp.measure]);
		--dump_table(g_rf_obj.rsrp.caret[data.earfcn][data.pci]);
		if item_i.value < g_rf_obj.SS.threshold then
			item_i['alert'] = true;
		end

		--line 2 Delta display bar item
		-- build delta bar data, if it exist
		item_i.delta={min = g_rf_obj.delta.min, max =g_rf_obj.delta.max;};
		init_table(g_rf_obj.delta.caret, data.channel, data.cell_id);
		set_item_sig_value(g_rf_obj.delta.caret[data.channel][data.cell_id], item_i.delta, data.delta);
		if data.alert then
			item_i.delta['alert'] = true;
		end
		item_i.delta.value = data.delta;
		item_i.delta.cell_id = data.delta_cell_id;
		
		--line 3 CINR display item
		item_i.signal_quality = data['signal_quality']
		--dump_table(item_i);
		g_rf_obj.item[i]=item_i

	end --for each data item in rf_data
	g_rf_obj.modified = true;


	--dinfo('rdb_scan_cell_qty_changed <<<')

end







local function rf_console_init()

	-- only watch wwan.0.cell_measurement.qty
	luardb.watch(g_rf_cfg.rdb_cell_qty, rdb_scan_cell_qty_changed);
	-- load rsrp threshold
	g_rf_obj.SS.threshold = tonumber(luardb.get(g_rf_cfg.rdb_rsrp_threshold)) or -96

	g_rf_obj.delta.threshold= tonumber(luardb.get(g_rf_cfg.rdb_rsrp_threshold_delta)) or 7

	-- initialize rf data item
	rdb_scan_cell_qty_changed(g_rf_cfg.rdb_cell_qty, luardb.get(g_rf_cfg.rdb_cell_qty));

	-- immediately refresh screen
	g_rf_cfg.fresh_count = cfg.REFRESH_SCREEN_COUNT;
	g_rf_cfg.table_width = get_table_width(g_rsrp_stats_table);
	g_rf_cfg.bar_width = 78

end

local function rf_console_end()
	-- no longer watcher
	luardb.watch(g_rf_cfg.rdb_cell_qty, nil);

	-- cleanup rf data to save memory
	g_rf_obj.item={};
	g_rf_obj.rsrp.caret={};
	g_rf_obj.delta.caret={};
	if g_rf_obj.stats  then
		g_rf_obj.stats={}
	end

end





local function rf_refresh_table(start_col, start_row)
	local  offset =10

	if g_rf_cfg.table_header_redraw then
		draw_text(start_col, start_row, 'Statistics', 0, true);
		draw_table_header(g_rsrp_stats_table, start_col+offset, start_row);
		g_rf_cfg.table_header_redraw = false;
	end
	-- left title
	if g_rf_obj.modified then

		--local y =start_row+2
		local y =start_row+1
		for i, item in pairs(g_rf_obj.item) do
			if g_rf_obj.stats[item.channel][item.cell_id] then
				-- draw all the field except 'result'
				draw_table_field(g_rsrp_stats_table, start_col+offset, y , g_rf_obj.stats[item.channel][item.cell_id]);

				if cfg.ENABLE_CELL_DELTA_CHECK == true then
					if g_rf_obj.stats[item.channel][item.cell_id].alert or  g_rf_obj.stats[item.channel].alert then
						term:setColour(cfg.CARET_COLOR);
						draw_one_field(g_rsrp_stats_table, start_col+offset, y, 'Result', 'FAIL');
					else
						term:setColour(cfg.CARET_GREEN);
						draw_one_field(g_rsrp_stats_table, start_col+offset, y, 'Result', 'PASS');
					end

					term:setColour(cfg.CARET_COLOR_END);
				end

				--draw_table_line(g_rsrp_stats_table, start_col+10, y+1);
				y = y+1;
			end
			if i>=cfg.MAX_REPORTED_CELLS then break; end;
		end
	end
end

local function	rf_refresh_bar( start_col, start_row, bar_size)
	-- draw 6 text + bar
	if g_rf_cfg.bar_header_redraw then
		draw_text(start_col, start_row, "Type", 4, true);
		draw_text(start_col+5, start_row, "CID", 3, false);
		draw_text(start_col+9, start_row, "RFCN", 4, false);
		draw_text(start_col+20, start_row, "Measure", 7, false);
		draw_text(start_col+29, start_row, "Value", 5, false);
		draw_line(start_col, start_row +1 , g_rf_cfg.bar_width);
		g_rf_cfg.bar_header_redraw =false;
	end

	local y = start_row +1
	-- draw left title
	if g_rf_obj.modified then
		-- draw each bar
		for i, item in pairs(g_rf_obj.item) do

			draw_line(start_col, y, g_rf_cfg.bar_width);
			y = y+1

			-- draw Signal Strength bar
			draw_text(start_col, y, item.cell_type , 3, true)
			draw_text(start_col+5, y, item.cell_id , 3, true)
			draw_text(start_col+8, y, item.channel, 5, false)
			
			draw_text(start_col+20 , y,  item.signal_strength_type, 5, false)
			draw_text(start_col+28 , y,  string.format("%.1f",item.value), 6, false)
			if  item.alert then
				draw_bar(start_col+35, y,  g_rf_obj.SS.unit, 10, bar_size, item, cfg.BAR_COLOR_ALERT, cfg.BAR_COLOR_END);
			else
				draw_bar(start_col+35, y,  g_rf_obj.SS.unit, 10, bar_size, item, cfg.BAR_COLOR_NORMAL, cfg.BAR_COLOR_END);
			end
			
			if cfg.ENABLE_CELL_DELTA_CHECK == true then
				-- draw Delta bar
				y = y+2
				if  item.delta then
					draw_text(start_col+20 , y,  g_rf_obj.delta.measure, 5, false)
					draw_text(start_col+28 , y,  item.delta.value, 6, false)
					if item.delta.alert then
						draw_bar(start_col+35, y,  g_rf_obj.delta.unit, 10, bar_size, item.delta, cfg.BAR_COLOR_ALERT, cfg.BAR_COLOR_END);
					else
						draw_bar(start_col+35, y,  g_rf_obj.delta.unit, 10, bar_size, item.delta, cfg.BAR_COLOR_NORMAL, cfg.BAR_COLOR_END);
					end
				else
					erase_line(start_col+20, y)
				end
			end

			-- print Signal Quality text
			y= y+2
			draw_text(start_col+20 , y,  item.signal_quality_type, 5, false)
			draw_text(start_col+28 , y,  item.signal_quality, 6, false)

			-- print Delta Cell ID
			if  item.delta and item.delta.cell_id then
				draw_text(start_col+35 , y,  'Delta CID '..item.delta.cell_id, 15, true)
			else
				erase_line(start_col+35, y)
			end
			y = y+1
			if i >= cfg.MAX_REPORTED_CELLS then break; end


		end
		-- clear the rest bar
---[[
		for i = #g_rf_obj.item, 2  do
			if i ~=0 then
			erase_line(start_col, y);
			end
			erase_line(start_col, y+1);
			erase_line(start_col+20, y+2);
			erase_line(start_col+20, y+3);
			erase_line(start_col+20, y+4);
			erase_line(start_col+20, y+5);
			y= y+6
		end
--]]
		g_rf_obj.modified =false;
	end --if obj.modified then

end

---- rf alignment
local function rf_console_set_redraw()

	g_rf_cfg.table_header_redraw = true;
	g_rf_cfg.bar_header_redraw = true;
	g_rf_obj.modified =true;

end

---- rf alignment
local function rf_console_poll()

	-- prevent the display damage by third party, it refresh screen periodically
	g_rf_cfg.fresh_count = g_rf_cfg.fresh_count+1;
	-- if cfg modifed, refresh whole screen too
	if  g_rf_cfg.fresh_count > cfg.REFRESH_SCREEN_COUNT then
		--reset clear
		term_clear();
		term:moveTo(cfg.RF_START_COL, 1);
		--term:setBigFont(true);

		term:write('Cell Info '.. luardb.get("sw.version")..'/'..luardb.get("hw.version"));
		--term_reset()
		g_rf_cfg.imei = luardb.get(g_rf_cfg.rdb_cell_imei);

		if g_rf_cfg.imei then
			-- if imei not available, it is waiting for dccd scanmode start,
			-- then refresh the screen every 3*POLL_TIMEOUT = 3s
			draw_text(cfg.RF_ERRMSG_COL + cfg.RF_ERRMSG_NUM+5, 1, g_rf_cfg.imei, 0, true);

		end

		g_rf_cfg.fresh_count =0;
		rf_console_set_redraw()
	end

	rdb_scan_cell_qty_post_update();

	--luardb.wait(5);
	--rf_refresh_table(cfg.RF_START_COL, 23);
	rf_refresh_table(cfg.RF_START_COL, 21);

	--rf_refresh_bar(cfg.RF_START_COL, 3, cfg.BAR_COLS_NUM);
	rf_refresh_bar(cfg.RF_START_COL, 2, cfg.BAR_COLS_NUM);


	term:moveTo(1,1);
	term:flush()

	--set_color(term_cfg.Attr_Hidden);

end


--[[
    Wait for 10 seconds and check to see if anything
    is plugged into eth0. If not, then we assume we're
    being powered by an alignment tool and signal DCCD
    to start in RF alignment mode.
]]
local function detect_indoorunit()

    for i = 1, 10 do
        if os.execute("ifconfig eth0 | grep RUNNING > /dev/null 2>&1") == 0 then
            luardb.set("service.dccd.mode", "on_line")
            return
        end
        os.execute("sleep 1")
    end
    luardb.set("service.dccd.mode", "rf_qualification")

end


--term:reset();
--reset clear
term_clear();
--term:cursorHide();
--term:setBigFont(true);

draw_text(30,10, "WNTD Booting ...", 0, true)

term:flush()


detect_indoorunit()


term:moveTo(cfg.RF_START_COL, 1);
term:write('Cell Info '.. luardb.get("sw.version")..'/'..luardb.get("hw.version"));

erase_line(28,10);
draw_text(28,10, "WNTD RF alignment...", 0, true)

rf_console_init()
while(1) do

	luardb.wait(cfg.POLL_TIMEOUT);
		
	rf_console_poll()
end
	
term:cursorShow()
term:close()





