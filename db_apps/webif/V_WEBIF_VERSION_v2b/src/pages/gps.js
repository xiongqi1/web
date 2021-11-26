function na(val?:string) {
  if(!isDefined(val) || val == "") return "N/A";
  return val;
}

// Convert the latitude or longitude from the NMEA string format to a decimal representation
// suitable for passing in a Google Maps URL.  The NMEA format is "DDDMM.MMMMM" - degrees *
// 100 + decimal minutes.  If "sign" is true then the position is negative (west/south of
// Greenwich/equator).
// change2degree2("3740.589813", true) returns "-37.676496883333336"
// change2degree2("14455.965856", false) returns "144.93276426666668"
function change2degree2(val, sign) {
  /* get degree minute */
  var degree = parseFloat(val) || 0;
  var degree_only = Math.floor(degree/100);
  var min = degree % 100;
  var decimal = (degree_only + min / 60) * (sign ? -1 : 1)
  return decimal.toString()
}

function change2degree(str, map) {
  /* get degree minute */
  var degree = parseFloat(str) || 0;
  var degree_only = Math.floor(degree/100);
  var min = degree % 100;

  /* no conversion required for map - google understands degree minute */
  /* 6 fractional digits - more than 1 cm accuracy = log ( 4,000,000,000 cm / 360 / 60 ) */
  if(map)
    return degree_only.toString() + " " +min.toFixed(6).toString();

  /* convert min to second */
  var min_only = Math.floor(min);
  var sec = (min%1)*60;

  /* 4 fractional digits - more than 1 cm accuracy = log ( 4,000,000,000 cm / 360 / 60 / 60 ) */
  // (the &#176; gives us a degree symbol).
  return degree_only.toString() + "&#176; " + min_only.toString() + "' " + sec.toFixed(4) + "\" ";
}

function decodeSatelliteData(obj) {
  var satellites = obj.satellitedata.split(";");
  var rows = [];
  var minDisplay = 12;
  // We seem to get a huge array ~ 36 entries
  // but use num_satellites to determine
  var numToDisplay = obj.number_of_satellites;
  if (numToDisplay < minDisplay) {
    numToDisplay = minDisplay;
  }

  var idx = 0;
  satellites.forEach(function(sat){
    if (idx >= numToDisplay) return;
    var bits = sat.split(",");
    if (bits.length < 5) return;
    var row: any = {};
    row.index = idx + 1;
    row.inUse = bits[0];
    row.prn = bits[1];
    row.snr = bits[2];
    row.elevation = bits[3];
    row.azimuth = bits[4];
    rows[idx++] = row;
  });

  //Pad it to minDisplay
  while( idx < minDisplay) {
    var row: any = {};
    row.index = idx + 1;
    row.inUse = 0;
    row.prn = na();
    row.snr = na();
    row.elevation = na();
    row.azimuth = na();
    rows[idx++] = row;
  }
  return rows;
}

var googlePos = "";
function openGoogleMapWindow() {
	if (googlePos.length === 0) {
		blockUI_alert(_("invalidGPSposition"));//Invalid GPS position!
		return;
	}
	// Construct the URL in accordance with Google's published API, as described in
	// https://developers.google.com/maps/documentation/urls/guide
	var map_url = "https://www.google.com/maps/search/?api=1&query=" + googlePos
	var mapWindow=window.open(map_url);
	mapWindow.focus();
}

function massageDateTime(myDate) {  // The awefullness of this code is from the original html
    var dateTime = myDate.toLocaleString()
    var words = dateTime.split(" ");  // This version is a little tidier
    function isNoField(field) {return typeof(field) == "undefined" || field.search('undefined') != -1 || field == "";}
    // if words[2] is invalid which means this date format only has two fields
    if (isNoField(words[2])) {
      return words[0] + ',  ' + words[1];
    }
    if (isNoField(words[3])) {
      var utcoffset = -myDate.getTimezoneOffset();
      dateTime = words[0] + ' ' + words[1] + ' ' + words[2] + ' ' + "(UTC + " + utcoffset/60 + ")" + ' ';
    } else {
      dateTime = words[0] + ' ' + words[1] + ' ' + words[2] + ' ' + words[3] + ',  ';
    }
    for (var i = 4; i < 12; i++) {
      if (isNoField(words[i])){
        break;
      }
      dateTime += words[i] + ' ';
    }
    return dateTime;
}

function decodeGpsStatus(obj) {

  function get_historical_source(src)
  {
    if (src == "historical-standalone")
      return _("Previously Stored GPS Data (Standalone)");
    if (src == "historical-agps")
      return _("Previously Stored GPS Data (Mobile Assisted)");
    return _("Previously Stored GPS Data (N/A)");
  }

  function getHistoricalSource(){
    var src="";
    if (toBool(GpsCfg.obj.GpsEnable)) {
      if (obj.gps_source == "agps"){
        src = _("MS Assisted GPS");
      } else if (obj.gps_source == "standalone"){
        src = _("Stand-alone GPS");
      } else {
        src = get_historical_source(obj.gps_source);
      }
    } else {
      if (obj.gps_source == "agps"){
        src = _("Previously Stored GPS Data (Mobile Assisted)");
      } else if (obj.gps_source == "standalone"){
        src = _("Previously Stored GPS Data (Standalone)");
      } else {
        src = get_historical_source(obj.gps_source);
      }
    }
    return src;
  }

  obj.positioningDataSource = getHistoricalSource();

  var sgps_valid = 0;
  var sgps_status;
  if (obj.gps_standalone_valid =="N/A"){
    sgps_status="N/A";
  }
  else if (obj.gps_standalone_valid == "valid"){
    sgps_status="Normal";
    sgps_valid = 1;
  }
  else {
    sgps_status="Invalid";
  }
  obj.sgps_status = sgps_status;
#ifdef V_HAS_AGPS_y
  var agps_status;
  if (obj.gps_assisted_valid == "N/A")
    agps_status="N/A";
  else if (obj.gps_assisted_valid == "valid")
    if(sgps_valid == 1)
      agps_status="Normal (Not In Use)";
    else
      agps_status="Normal";
  else
    agps_status="Invalid";
  obj.agps_status = agps_status;
#endif

  if (na(obj.gps_common_date) == "N/A" || na(obj.gps_common_time) == "N/A") {
    obj.datetime = "N/A";
  } else {
    var myDate = new Date();
    myDate.setUTCDate(obj.gps_common_date.substring(0,2));
    myDate.setUTCMonth(obj.gps_common_date.substring(2,4)-1);
    var year=parseInt(obj.gps_common_date.substring(4,6));
    if (year>=80)
      year+=1900;
    else
      year+=2000;
    myDate.setUTCFullYear(year);
    myDate.setUTCHours(obj.gps_common_time.substring(0,2));
    myDate.setUTCMinutes(obj.gps_common_time.substring(2,4));
    myDate.setUTCSeconds(obj.gps_common_time.substring(4,6));
    obj.datetime = massageDateTime(myDate);
  }

  obj.latitudelongitude = change2degree(obj.latitude,0) + obj.latitude_direction + ",&nbsp;" + change2degree(obj.longitude,0) + obj.longitude_direction;
  googlePos = change2degree2(obj.latitude, obj.latitude_direction == "S") + "," + change2degree2(obj.longitude, obj.longitude_direction == "W");

  obj.altitudeheight = na(obj.altitude) + " m,&nbsp;" + na(obj.height_of_geoid) + " m";
  obj.groundspeed = na(obj.ground_speed_kph) + " km/h,&nbsp;" + na(obj.ground_speed_knots) + " knots";
  obj.PDOPHDOPVDOP = na(obj.pdop)  + ",&nbsp;&nbsp;" + na(obj.hdop) + ",&nbsp;&nbsp;" + na(obj.vdop);
  obj.number_of_satellites = na(obj.number_of_satellites);
  return obj;
}

var GpsStatus = PageObj("GpsStatus", "gpsStatus",
{
  readOnly: true,
  pollPeriod: 1000,
  decodeRdb:  decodeGpsStatus,

  members: [
    hiddenVariable("latitude", "sensors.gps.0.common.latitude"),
    hiddenVariable("longitude", "sensors.gps.0.common.longitude"),
    hiddenVariable("gps_source", "sensors.gps.0.source"),
    hiddenVariable("gps_standalone_valid", "sensors.gps.0.standalone.valid"),
    hiddenVariable("gps_assisted_valid", "sensors.gps.0.assisted.valid"),
    hiddenVariable("gps_common_date", "sensors.gps.0.common.date"),
    hiddenVariable("gps_common_time", "sensors.gps.0.common.time"),
    hiddenVariable("latitude_direction", "sensors.gps.0.common.latitude_direction"),
    hiddenVariable("latitude", "sensors.gps.0.common.latitude"),
    hiddenVariable("longitude_direction", "sensors.gps.0.common.longitude_direction"),
    hiddenVariable("longitude", "sensors.gps.0.common.longitude"),
    hiddenVariable("altitude", "sensors.gps.0.standalone.altitude"),
    hiddenVariable("height_of_geoid", "sensors.gps.0.common.height_of_geoid"),
    hiddenVariable("ground_speed_kph", "sensors.gps.0.standalone.ground_speed_kph"),
    hiddenVariable("ground_speed_knots", "sensors.gps.0.standalone.ground_speed_knots"),
    hiddenVariable("pdop", "sensors.gps.0.standalone.pdop"),
    hiddenVariable("hdop", "sensors.gps.0.standalone.hdop"),
    hiddenVariable("vdop", "sensors.gps.0.standalone.vdop"),
    staticTextVariable("positioningDataSource", "positioningDataSource"),
    staticTextVariable("datetime", "date time"),
    staticTextVariable("latitudelongitude", "latitude longitude")
#ifdef V_HAS_SGPS
    ,staticTextVariable("altitudeheight", "altitude height")
    ,staticTextVariable("groundspeed", "ground speed")
    ,staticTextVariable("PDOPHDOPVDOP", "PDOP &amp; HDOP &amp; VDOP")
    ,staticTextVariable("sgps_status", _("standalone") + "&nbsp;" + _("gps device status"))
#endif
#ifdef V_HAS_AGPS_y
    ,staticTextVariable("agps_status", _("mobile assisted") + "&nbsp;" + _("gps device status"))
#endif
#ifdef V_HAS_SGPS
    ,staticTextVariable("number_of_satellites", "number of satellites").setRdb( "sensors.gps.0.standalone.number_of_satellites")
#endif
  ]
});

#ifdef V_HAS_SGPS
class InuseVariable extends ShownVariable {
  setVal(obj: any, val: string) {
#ifdef V_WEBIF_SPEC_vdf
    var src = "/img/" + (val == "1" ? "UP" : "down") + ".gif";
#else
    var src = "/img/" + (val == "1" ? "up" : "down") + ".gif";
#endif
    $("#" + this.htmlId).html(function () {
      return htmlTag("img", { src: src, width: "15", height: "15", align: "center" }, "");
    });
  };
}

class SnrVariable extends ShownVariable {
  setVal(obj: any, val: string) {
    function imgIdx() {
      var n = parseInt(val);
      if ( isNaN(n) == true) return 0;
      if (n >= 45) return 6;
      if (n >= 40) return 5;
      if (n >= 35) return 4;
      if (n >= 30) return 3;
      if (n >= 25) return 2;
      if (n >= 20) return 1;
      return 0;
    }
    function genHtml() {
      return htmlTag("img", {src: "/img/csq" + imgIdx() + ".GIF", width: "40", height: "15"}, "")
      + htmlTag("span", {class: "normal-text", style:"top:-2px"}, "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + val);
    }
    $("#" + this.htmlId).html(genHtml());
  }
}

var SatelliteStatus = PageTableObj("SatelliteStatus", "satellites status",
{
  readOnly: true,
  tableAttribs: {class:"no-border"},
  pollPeriod: 1000,
  decodeRdb:  decodeSatelliteData,
  members: [
    hiddenVariable("number_of_satellites", "sensors.gps.0.standalone.number_of_satellites"),
    hiddenVariable("satellitedata", "sensors.gps.0.standalone.satellitedata"),
    staticTextVariable("index", "index"),
    new InuseVariable("inUse", "in use"),
    staticTextVariable("prn", "PRN"),
    new SnrVariable("snr", "SNR"),
    staticTextVariable("elevation", "elevation"),
    staticTextVariable("azimuth", "azimuth")
  ]
});
#endif

function genButton(btnType, btnFn, label) {
  return htmlTag("button",
        {type: btnType, class: "secondary", onClick: btnFn, style: "width:auto;margin-left:0;"},
        _(label));
}

class GoogleMapsVariable extends ShownVariable {
  genHtml() {
    return genButton("button", "openGoogleMapWindow()", "googlemap");
  }
}

var GpsApps = PageObj("GpsApps", "gps applications",
{
  readOnly: true,
  members: [
    new GoogleMapsVariable("google", "")
  ]
});

// This replaces the auto stub because of the other objects it controls
function onClickGpsEnable(toggleName, v) {
  GpsCfg.setVisible(v);
  setVisible("#objouterwrapperGpsApps", v);
  setVisible("#objouterwrapperSatelliteStatus", v);
  setVisible("#objouterwrapperGpsStatus", v);
}

var GpsCfg = PageObj("GpsCfg", "gps configuration",
{
  members: [
      objVisibilityVariable("GpsEnable", "gps operation").setRdb("sensors.gps.0.enable")
#if (defined V_HAS_AGPS_y)
      , editableBoundedInteger("agps_int", "agpsUpdateInterval", 60, 65534, "Msg106", {helperText: "Msg107"})
        .setRdb("sensors.gps.0.assisted.updateinterval")
        .setSmall()
#endif
  ]
});

var pageData : PageDef = {
#if defined V_SERVICES_UI_none || defined V_GPS_none
  onDevice : false,
#endif
  title: "GPS",
  menuPos: ["Services", "GPS"],
  pageObjects: [GpsCfg, GpsApps, GpsStatus
#ifdef V_HAS_SGPS
  , SatelliteStatus
#endif
  ],
  onReady: function (){
    $("#objouterwrapperGpsCfg").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
    // This needs to be called explicitly because of the other objects it controls
    onClickGpsEnable("", GpsCfg.obj.GpsEnable);
  }
}
