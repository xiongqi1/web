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

#if defined(V_PRODUCT_hth_70)
var NumProfiles = 1
#else
var NumProfiles = 6
#endif

function getWWanColumns() {
  return [
    {
    members:[
      {hdg: "profileName", genHtml: (obj) => obj.name},
      {hdg: "status"},
      {hdg: "defaultProfile"}
    ]
    },
    {
    members:[
      {hdg: "wwan ipaddr"},
      {hdg: "dnsServer"}
    ]
    },
    {
    members:[
      {hdg: "apn"},
      {hdg: "connectionUpTime"}
    ]
    }
  ]
}

var WWanStatus = PageObj("StsWWanStatus", "wwanStatus",
  {
    customLua: {
      lockRdb: false,
      get: function() {
        return ["=getRdbArray(authenticated,'link.profile',1," + NumProfiles +
                  ",false,{'name','defaultroute','enable','apn','user','autoapn'})"]
      }
    },

    readOnly: true,
    column: 1,
    multiColumns: [],
    genObjHtml: function () {
      var html = [];
      for (var i = 0; i < 2; i++) {
        var columns = getWWanColumns();
        var button = '<button class="secondary sml toggle-area inline-right-button" id="showUsageButton'+i+'" onclick="switchUsage('+i+')" style="margin:30px 255px 15px 0;width:180px"><i class="icon open"></i>'+_('showDataUsage')+'</button>';
        var duration = '<div id="data_usage_duration_toggle'+i+'" class="show-option" style="display:none;padding: 0px 20px 15px; margin-top: -48px;"><a href="javascript:switchDuration('+i+',1)">'+_("showDuration")+'</a></div>';
        html.push(_genCols(this.objName + "_" + i, columns, button) + duration + genUsageTable(i));
        this.multiColumns.push(columns);
      }
      return _genColumnsWrapper(this.labelText, html.join('<div class="hr" style="top:0; margin:0;"></div>'));
    },
    pollPeriod: 1000,

    populate: function () { //objs is an array of objects
      var _this = this;
      var objs = this.obj;
      objs.forEach((obj) => {
        var columns = _this.multiColumns[obj.id-1];
        if (columns)
          _populateCols(columns, obj);
      });
    },

    members: [
      hiddenVariable("imei", "wwan.0.imei")
    ]
});

stsPageObjects.push(WWanStatus);
