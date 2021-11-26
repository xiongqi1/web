// TODO : There is no wwan monitor daemon in Cassini at the moment.
// Below usage/duration display function is incomplete so should be implemented
// more if want to use them later.

#ifdef NO_USAGE_NO_DURATION
function switchDuration( idx, action ) {
    if( action ) {
        $("#data_usage_duration_toggle"+idx).html("<a href='javascript:switchDuration("+idx+",0)'>"+_('showEndTime')+"</a>");
        $("#SessionEndDuration"+idx).html(_("sessionDuration"));
    }
    else {
        $("#data_usage_duration_toggle"+idx).html("<a href='javascript:switchDuration("+idx+",1)'>"+_('showDuration')+"</a>");
        $("#SessionEndDuration"+idx).html(_("sessionEnd"));
    }
//	show_duration[idx]=action;
//	updata_usage(idx);
}

function switchUsage(idx) {
    if( $("#data_usage_table"+idx).css("display")=="none" ) {
        $("#data_usage_table"+idx).css("display", "");
        $("#showUsageButton"+idx).html("<i class='icon closed'></i>"+_("hideDataUsage"));
        $("#data_usage_duration_toggle"+idx).css("display", "");
    }
    else {
        $("#data_usage_table"+idx).css("display", "none");
        $("#data_usage_duration_toggle"+idx).css("display", "none");
        $("#showUsageButton"+idx).html("<i class='icon open'></i>"+_("showDataUsage"));
    }
}

function genUsageTable(i) {
    var table = '<table id="data_usage_table'+i+'" style="display:none;margin-bottom: 0px;">'+
    '<colgroup><col width="20%"><col width="20%"><col width="20%"><col width="20%"><col width="20%"></colgroup>'+
    '<thead><tr><th class="sml">'+_("sessionStart")+'</th><th class="sml" id="SessionEndDuration'+i+'">'+_("sessionEnd")+'</th>'+
        '<th class="sml">'+_("dataReceivedBytes")+'</th><th class="sml">'+_("dataSentBytes")+'</th><th class="sml">'+_("totalDataBytes")+'</th></tr></thead>'+
    '<tbody id="TBusage'+i+'">'+
    '<tr><td>22/05/2018 05:20:58 BST</td><td>20:24:01</td><td>870</td><td>1,124</td><td>1,994</td></tr>'+
    '<tr><td>01/01/1970 01:01:41 BST</td><td>00:00:00</td><td>630</td><td>1,124</td><td>1,754</td></tr>'+
    '<tr><td>01/01/1970 01:01:37 BST</td><td>00:00:00</td><td>630</td><td>1,124</td><td>1,754</td></tr>'+
    '<tr><td>01/01/1970 01:01:43 BST</td><td>00:00:00</td><td>630</td><td>1,124</td><td>1,754</td></tr>'+
    '</tbody>			</table>'
    return table;
}
#endif /* NO_USAGE_NO_DURATION */


function getWWanColumns() {
    return [
        {
        members:[
            {hdg: "profileName", genHtml: (obj) => obj.name},
            {hdg: "status", genHtml: (obj) => obj.status_ipv4},
            {hdg: "ipv6 status", genHtml: (obj) => obj.status_ipv6},
            {hdg: "defaultProfile", genHtml: (obj) => obj.defaultroute}
        ]
        },
        {
            members:[
            {hdg: "wwan ipaddr", genHtml: (obj) => obj.iplocal},
            {hdg: "dnsServer", genHtml: (obj) => {return obj.dns1+'<br>'+obj.dns2;}},
            {hdg: "wwan ipv6addr", genHtml: (obj) => obj.ipv6_ipaddr},
            {hdg: "ipv6 dnsServer", genHtml: (obj) => {return obj.ipv6_dns1+'<br>'+obj.ipv6_dns2;}},
        ]
        },
        {
            members:[
            {hdg: "apn", genHtml: (obj) => obj.apn},
            {hdg: "connectionUpTime", genHtml: (obj) => obj.usage_current}
        ]
        }
    ]
}

var WWanStatus = PageObj("StsWWanStatus", "wwanStatus",
{
    customLua: {
        lockRdb: false,
        get: function() {
        var luaScript = `
            o = {}
            local NumProfiles = luardb.get("wwan.0.max_sub_if") or 6
            o.NumProfiles = NumProfiles
            for pf=1,NumProfiles do
                local info = {}
                local m_pf = luardb.get("link.profile."..pf..".module_profile_idx") or pf
                info.id = pf
                info.name = luardb.get("link.profile."..pf..".name") or ""
                info.status_ipv4 = luardb.get("link.policy."..m_pf..".status_ipv4") or ""
                info.status_ipv6 = luardb.get("link.policy."..m_pf..".status_ipv6") or ""
                info.defaultroute = luardb.get("link.profile."..pf..".defaultroute") or ""
                info.iplocal = luardb.get("link.policy."..m_pf..".iplocal") or ""
                info.dns1 = luardb.get("link.policy."..m_pf..".dns1") or ""
                info.dns2 = luardb.get("link.policy."..m_pf..".dns2") or ""
                info.ipv6_ipaddr = luardb.get("link.policy."..m_pf..".ipv6_ipaddr") or ""
                info.ipv6_dns1 = luardb.get("link.policy."..m_pf..".ipv6_dns1") or ""
                info.ipv6_dns2 = luardb.get("link.policy."..m_pf..".ipv6_dns2") or ""
                info.apn = luardb.get("link.profile."..pf..".apn") or ""
                info.usage_current = luardb.get("link.profile."..pf..".usage_current") or ""
                info.enable = luardb.get("link.policy."..m_pf..".enable") or "0"
                table.insert(o, info)
            end
`;
        return luaScript.split("\n");
        }
    },

    readOnly: true,
    column: 1,
    multiColumns: [],

    genObjHtml: function () {
        var html = [];
        for (var i = 0; i < 6; i++) {
            var columns = getWWanColumns();
#ifdef NO_USAGE_NO_DURATION
            var button = '<button class="secondary sml toggle-area inline-right-button" id="showUsageButton'+i+'" onclick="switchUsage('+i+')" style="margin:30px 255px 15px 0;width:180px"><i class="icon open"></i>'+_('showDataUsage')+'</button>';
            var duration = '<div id="data_usage_duration_toggle'+i+'" class="show-option" style="display:none;padding: 0px 20px 15px; margin-top: -48px;"><a href="javascript:switchDuration('+i+',1)">'+_("showDuration")+'</a></div>';
            html.push(_genCols(this.objName + "_" + i, columns, button) + duration + genUsageTable(i));
#else
            html.push(_genCols(this.objName + "_" + i, columns, ""));
#endif /* NO_USAGE_NO_DURATION */
            this.multiColumns.push(columns);
        }
        return _genColumnsWrapper(this.labelText, html.join('<div class="hr" style="top:0; margin:0;"></div>'));
    },

    pollPeriod: 1000,

    populate: function () { //objs is an array of objects
        var _this = this;
        var obj = this.obj;
        for (var i = 1; i <= obj.NumProfiles; i++) {
        var columns = _this.multiColumns[obj[i].id-1];
        if (columns) {
            _populateCols(columns, obj[i]);
        }
        if (obj[i].enable == '1') {
            $("#StsWWanStatus_"+(i-1)).show();
        } else {
            $("#StsWWanStatus_"+(i-1)).hide();
        }
        };
    },

    decodeRdb: function (obj) {
        for (var i = 1; i <= obj.NumProfiles; i++) {
        var ar = obj[i].usage_current.split(",");
        if(ar.length >= 4 ) {
            obj[i].usage_current = toUpTime(parseInt(ar[1])-parseInt(ar[0]));
        }
        else {
            obj[i].usage_current = 0;
        }
        }
        return obj;
    },

    members: [
        hiddenVariable("NumProfiles", "")
    ]
});

stsPageObjects.push(WWanStatus);
