<%
numEventTypes=24;
%>
var numEventTypes=@@numEventTypes;

var EventNotiEnabledClients=[];
var EventNotiDestIdx=[];
var EventNotiInstances=[];
var EventNotiEdges=[];
var EventNotiHighms=[];
var EventNotiLowms=[];
var EventNotiHighthresh=[];
var EventNotiLowthresh=[];

var EventNotiDestName=[];
var EventNotiDestClients=[];
var EventNotiDestSmsAddr=[];
var EventNotiDestEmailAddr=[];
var EventNotiDestTcpAddr=[];
var EventNotiDestTcpPort=[];
var EventNotiDestUdpAddr=[];
var EventNotiDestUdpPort=[];
var EventNotiDestCommand=[];

// This function creates a list of elements - "[elem1,....,elemN];"
function createList(numElements, element ) {
	var def='=['+element(1);
	for ( var i=2;i<=numElements;i++)
		def += ','+element(i);
	def += ']';
	return def;
}

var evtnoti_list=[_("eventNotiType1"), _("eventNotiType2"), _("eventNotiType3"), _("eventNotiType4"), _("eventNotiType5"), _("eventNotiType6"),
	_("eventNotiType7"), _("eventNotiType8"), _("eventNotiType9"), _("eventNotiType10"), _("eventNotiType11"), _("eventNotiType12"),
	_("eventNotiType13"), _("eventNotiType14"), _("eventNotiType15"), _("eventNotiType16"), _("eventNotiType17"), _("eventNotiType18"),
	_("eventNotiType19"), _("eventNotiType20")
#ifdef V_WEBIF_SPEC_vdf
	, _("hardwareResetSettingsChange")
#else
	, _("eventNotiType21")
#endif
	, _("eventNotiType22"),
	_("eventNotiType23"),_("eventNotiType24")];
var evtnoti_desc_list=[_("event noti type 1 desc"), _("event noti type 2 desc"), _("event noti type 3 desc"), _("event noti type 4 desc"), _("event noti type 5 desc"),
	_("event noti type 6 desc"), _("event noti type 7 desc"), _("event noti type 8 desc"), _("event noti type 9 desc"),
	_("event noti type 10 desc"), _("event noti type 11 desc"), _("event noti type 12 desc"), _("event noti type 13 desc"),
	_("event noti type 14 desc"), _("event noti type 15 desc"), _("event noti type 16 desc"), _("event noti type 17 desc"),
	_("event noti type 18 desc"), _("event noti type 19 desc"), _("event noti type 20 desc")
#ifdef V_WEBIF_SPEC_vdf
	, _("hardwareResetSettingsChangeDesc")
#else
	, _("event noti type 21 desc")
#endif
	, _("event noti type 22 desc"), _("event noti type 23 desc"), _("event noti type 24 desc")];

#ifdef V_SINGLE_NETWORK
	// hide wwan tech change
	evtnoti_list[6]="";
	evtnoti_desc_list[6]="";
#endif
#ifdef V_ETH_PORT_0
	// hide number of connected Ethernet interfaces change
	evtnoti_list[7]="";
	evtnoti_desc_list[7]="";
#endif
#ifdef V_POWER_SOURCES_none
	// hide power change
	evtnoti_list[8]="";
	evtnoti_desc_list[8]="";
#endif

#if !defined (V_IOBOARD_nguni) && !defined (V_IOBOARD_kudu)
	// No SD sard slot
	evtnoti_list[10]="";
	evtnoti_desc_list[10]="";
#else
	// display/hide SD card menu depending on installed package
	var nas_installed="<%get_single_direct('system.package.installed.nas');%>";
	if (nas_installed != "1") {
		evtnoti_list[10]="";
		evtnoti_desc_list[10]="";
	}
#endif

#ifndef V_MULTIPLE_LANWAN_UI
	// hide failover
	evtnoti_list[11]="";
	evtnoti_desc_list[11]="";
#endif
#if !defined (V_WIFI_backports) && !defined (V_WIFI_qca_soc_lsdk)
	// hide wifi clients
	evtnoti_list[12]="";
	evtnoti_desc_list[12]="";
	evtnoti_list[13]="";
	evtnoti_desc_list[13]="";
#endif
#ifdef V_MODCOMMS_y
	if (rf_mice_ready != "ready") {
		// hide wifi clients
		evtnoti_list[12]="";
		evtnoti_desc_list[12]="";
		evtnoti_list[13]="";
		evtnoti_desc_list[13]="";
	}
#endif

#if !defined(V_IOMGR_ioext4) && !defined(V_IOMGR_kudu) && !defined(V_IOMGR_clarke)
#ifdef V_MODCOMMS_y
	if (io_mice_ready != "ready" && aeris_mice_ready != "ready") {
#endif
		// hide all IOs
		evtnoti_list[14]="";
		evtnoti_desc_list[14]="";
		evtnoti_list[15]="";
		evtnoti_desc_list[15]="";
		evtnoti_list[16]="";
		evtnoti_desc_list[16]="";
#ifdef V_MODCOMMS_y
	}
#endif
#endif
#if defined(V_IOMGR_kudu)
	evtnoti_list[15]="";
	evtnoti_desc_list[15]="";
#endif
#if (!defined(V_WIFI_CLIENT_backports) && !defined (V_WIFI_CLIENT_qca_soc_lsdk)) || ! defined(V_WIFI_WDS)
	// hide WDS
	evtnoti_list[17]="";
	evtnoti_desc_list[17]="";
#endif

#ifdef V_MODCOMMS_y
	if (rf_mice_ready != "ready") {
		// hide WDS
		evtnoti_list[17]="";
		evtnoti_desc_list[17]="";
	}
#endif

	evtnoti_list[18]="";
	evtnoti_desc_list[18]="";

#ifndef V_HW_PUSH_BUTTON_SETTINGS_y
	evtnoti_list[20]="";
	evtnoti_desc_list[20]="";
#endif

#ifndef V_VRRP
	evtnoti_list[21]="";
	evtnoti_desc_list[21]="";
#endif

#ifndef V_USB_OTG_MANUAL_MODE_SELECTION
	evtnoti_list[22]="";
	evtnoti_desc_list[22]="";
#endif

#ifndef V_GPS_GEOFENCE_y
	evtnoti_list[23]="";
	evtnoti_desc_list[23]="";
#endif

<%
noti_enabled=get_single("service.eventnoti.conf.enable");
rdbBase="";

function getRdb(variable) {
	var val=get_single(rdbBase+variable);
	if(val=="N/A") {
		return "";
	}
	return val;
}

function setRdb(key, val) {
	set_single_direct("-p",rdbBase+key, val);
}

// returns true if valued changed
function updateRdb(key, val) {
	var cur=get_single(rdbBase+key);
	if (cur==val) return false;
	setRdb(key, val);
	return true;
}

function defineDestinationsVars() {
	for (var i=1; 1; i++) {
		rdbBase="service.eventnoti.conf.dest."+i+".";
		var name=getRdb("name");
		if ( name=="" ) return;
		var join='['+i+']="';
		write('EventNotiDestName'+join+name+'";');
		write('EventNotiDestEmailAddr'+join+getRdb("email_addr")+'";');
		write('EventNotiDestSmsAddr'+join+getRdb("sms_addr")+'";');
		write('EventNotiDestTcpAddr'+join+getRdb("tcp_addr")+'";');
		write('EventNotiDestTcpPort'+join+getRdb("tcp_port")+'";');
		write('EventNotiDestUdpAddr'+join+getRdb("udp_addr")+'";');
		write('EventNotiDestUdpPort'+join+getRdb("udp_port")+'";');
		write('EventNotiDestCommand'+join+base64_encode(getRdb("cmd"))+'";');
	}
}

function defineConfigVars() {
	rdbBase="service.eventnoti.conf.";
	write('EventNotiEn="'+getRdb("enable")+'";');
	write('MaxEventNotiBufSize="'+getRdb("max_size")+'";');
	write('MaxEventNotiRetryCount="'+getRdb("max_retry")+'";');
	write('WriteEventNotiToFile="'+getRdb("save_to_file")+'";');
	write('EventNotiLogFile="'+getRdb("log_file")+'";');
	write('EventNotiPrefixText="'+getRdb("prefix_text")+'";');

	for (var i=1; i<=numEventTypes; i++) {
		rdbBase="service.eventnoti.conf.type."+i+".";
		var dest_idx=getRdb("dest_idx");
		var join='['+i+']="';
		write('EventNotiEnabledClients'+join+getRdb("enabled_clients")+'";');
		write('EventNotiDestIdx'+join+dest_idx+'";');
		write('EventNotiInstances'+join+getRdb("instances")+'";');
		write('EventNotiEdges'+join+getRdb("debounce.edges")+'";');
		write('EventNotiHighms'+join+getRdb("debounce.highms")+'";');
		write('EventNotiLowms'+join+getRdb("debounce.lowms")+'";');
		write('EventNotiHighthresh'+join+getRdb("thresh.highthresh")+'";');
		write('EventNotiLowthresh'+join+getRdb("thresh.lowthresh")+'";');
	}
}

if (request['REQUEST_METHOD'] == "GET") {
defineDestinationsVars();
defineConfigVars();
}

%>
