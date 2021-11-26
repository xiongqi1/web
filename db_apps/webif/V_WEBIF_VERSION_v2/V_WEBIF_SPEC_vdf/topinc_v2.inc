<% include security_def.inc %>
<%checkCsrfToken();%>
<meta name="viewport" content="width=1100">
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
<meta http-equiv="format-detection" content="telephone=no">
<%_sv=get_single('sw.version');_fn=request['SCRIPT_NAME'];%>
#if defined(V_WIFI) || defined(V_WIFI_CLIENT)
<%_wlan_wifi_mode=get_single( 'wlan.0.wifi_mode' );%>
#endif
<script>
(function() {
	var html = document.documentElement;
	html.className = html.className.replace('no-js','') + ' js';
})();
var modules = [];
</script>
<%frameBreaker();%>
<!--[if lt IE 9]>
<script src="vdf-lib/js/lib/selectivizr.min.js"></script>
<script src="js/libs/html5-shiv.min.js"></script>

<noscript>
	<link rel="stylesheet" href="vdf-lib/css/ie/ie.min.css">
</noscript>
<![endif]-->
<!--[if IE 6]>
<link rel="stylesheet" href="css/ie6.css">
<![endif]-->
<!--[if IE 7]>
<link rel="stylesheet" href="css/ie7.css">
<![endif]-->
<!--[if lte IE 7]>
<link rel="stylesheet" href="css/ie6-ie7.css">
<![endif]-->
<link rel="stylesheet" href="vdf-lib/css/main.min.css?@@_sv">
<link rel="stylesheet" href="css/style.css?@@_sv">
<link rel="stylesheet" href="/css/jquery-ui.css">
</head>
<!--[if IE 6]>
	<body class="ie ie6">
<![endif]-->
<!--[if IE 7]>
	<body class="ie ie7">
<![endif]-->
<!--[if IE 8]>
	<body class="ie ie8">
<![endif]-->
<!--[if IE 9]>
	<body class="ie ie9">
<![endif]-->
<!--[if gt IE 9]>
	<body>
<![endif]-->
<!--[if !IE]><!-->
<body>
<!--<![endif]-->

<script src="/js/jquery.min.js?@@_sv" type="text/javascript"></script>
<script src="/js/jquery.validate.min.js?@@_sv" type="text/javascript" charset="utf-8"></script>
<script src="js/script.js?@@_sv"></script>
<script src="vdf-lib/js/main.min.js?@@_sv"></script>
<!--**********************************************************-->

<script type="text/javascript" src="/lang/b28n.min.js?@@_sv"></script>
<script language="JavaScript" src="js/jquery.blockUI.min.js"></script>
<script src="/js/jquery-ui.min.js"></script>
<script src="/js/jquery.ui.touch-punch.min.js"></script>
<%supportCsrfTokenErrorCgi();%>
<script language='javascript'>
#if defined(V_IOMGR_ioext4) || defined(V_IOMGR_nguni)
var IO_configurationPage='IO_configuration.html';
#endif
#if defined(V_MODCOMMS_y)
var IO_configurationPage='IO_configuration.html';
#endif
</script>
<script>
// display/hide SD card menu depending on installed package
var nas_installed="<%get_single_direct('system.package.installed.nas');%>";
#ifdef V_MODCOMMS_y
// display/hide mices menu depending on each mice status
var io_mice_ready="<%get_single_direct('modcomms.io-mice.1.status');%>";
var gps_mice_ready="<%get_single_direct('modcomms.gps-mice.1.status');%>";
var gps_can_mice_ready="<%get_single_direct('modcomms.gps-can-mice.1.status');%>";
var rf_mice_ready="<%get_single_direct('modcomms.rf-mice.1.status');%>";
var chubb_mice_ready="<%get_single_direct('modcomms.chubb-mice.1.status');%>";
var aeris_mice_ready="<%get_single_direct('modcomms.aeris-mice.1.status');%>";
#endif
</script>

<script type="text/javascript" src="/util.js?@@_sv"></script>
<script language='javascript'>

var service_pppoe_server_0_enable="<%get_single_direct('service.pppoe.server.0.enable');%>";
#ifdef V_ROUTER_TERMINATED_PPPOE
var service_pppoe_server_0_wanipforward_enable="<%get_single_direct('service.pppoe.server.0.wanipforward_enable');%>";
#endif

var fn="@@_fn".replace(".html", "");
if(fn=="" || fn=="/") {
	fn="index";
}
Butterlate.setTextDomain(fn.replace("/", ""));
$.blockUI.defaults.css.padding="20px 0 20px 0";
#if defined(V_WIFI) || defined(V_WIFI_CLIENT)
var wlan_wifi_mode="@@_wlan_wifi_mode";
#endif
var roam_simcard="<%get_single_direct('manualroam.custom_roam_simcard');%>";
if(!is_touch_device()) {$("head").append("<style type='text/css'>.footer {position:fixed;}</style>");}
</script>
<%supportCsrfTokenOnDocumentReady();%>
