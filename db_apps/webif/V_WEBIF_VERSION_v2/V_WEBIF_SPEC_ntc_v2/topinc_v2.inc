<%_sv=get_single('sw.version');_fn=request['SCRIPT_NAME'];%>
<% include security_def.inc %>
<%checkCsrfToken();%>
#if defined(V_WIFI) || defined(V_WIFI_CLIENT)
<%_wlan_wifi_mode=get_single( 'wlan.0.wifi_mode' );%>
#endif
<%_ip_handover_en=get_single('service.ip_handover.enable');%>
<script>
(function() {
	var html = document.documentElement;
	html.className = html.className.replace('no-js','') + ' js';
})();
var modules = [];
</script>
<meta name = "format-detection" content = "telephone=no">
<%frameBreaker();%>
<link rel="stylesheet" href="/css/validationEngine.jquery.css" type="text/css"/>
<!--[if lt IE 9]>
<script src="/js/selectivizr.js"></script>
<script src="/js/modernizr.js"></script>
<noscript>
	<link rel="stylesheet" href="/css/ie.css">
</noscript>
<![endif]-->
<link rel="stylesheet" href="/css/style.css?@@_sv">
<link rel="stylesheet" href="/css/jquery-ui.css">
</head>
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
<!--**********************************************************-->
<script src="/js/jquery.min.js?@@_sv" type="text/javascript"></script>
<script src="/js/jquery.validationEngine.min.js?@@_sv" type="text/javascript" charset="utf-8"></script>
<script src="/js/jquery.validate.min.js?@@_sv" type="text/javascript" charset="utf-8"></script>
<script src="/js/jquery.blockUI.min.js?@@_sv" type="text/javascript"></script>
<script src="/js/jquery-ui.min.js"></script>
<script src="/js/jquery.ui.touch-punch.min.js"></script>
<script src="/js/script.js?@@_sv"></script>
<script type="text/javascript" src="/lang/b28n.min.js?@@_sv"></script>
<%supportCsrfTokenErrorCgi();%>
<script language='javascript'>
var fn="@@_fn".replace(".html", "");
if(fn=="" || fn=="/") {
	fn="index";
}

$(window).load(function(){$("#form").validationEngine();});
$.blockUI.defaults.css.padding="20px 0 20px 0";
$.blockUI.defaults.css.border="3px solid #008bc6";

#if defined(V_WIFI) || defined(V_WIFI_CLIENT)
wlan_wifi_mode="@@_wlan_wifi_mode";
#endif
ip_handover_en="@@_ip_handover_en";
#if defined(V_LANGUAGE_FR_y) || defined(V_LANGUAGE_AR_y) || defined(V_LANGUAGE_DE_y) || defined(V_LANGUAGE_JP_y)
var multi_lang=0;
var current_lang="<%get_single_direct('webinterface.language');%>";
#ifdef V_LANGUAGE_FR_y
var lang_fr="0<%get_single_direct('webinterface.lang_fr');%>";
multi_lang+=parseInt(lang_fr);
#endif
#ifdef V_LANGUAGE_AR_y
var lang_ar="0<%get_single_direct('webinterface.lang_ar');%>";
multi_lang+=parseInt(lang_ar);
#endif
#ifdef V_LANGUAGE_DE_y
var lang_de="0<%get_single_direct('webinterface.lang_de');%>";
multi_lang+=parseInt(lang_de);
#endif
#ifdef V_LANGUAGE_JP_y
var lang_jp="0<%get_single_direct('webinterface.lang_jp');%>";
multi_lang+=parseInt(lang_jp);
#endif

if(current_lang=="") {
  var cmd="";
  var uboot_hw_id="<%get_single_direct('uboot.hw_id');%>";
  switch(uboot_hw_id) {
  case "NTC-6200-11":
  case "NTC-6200-12":
  case "NTC-6200-13":
    current_lang="jp";
    lang_jp="1";
    multi_lang++;
  break;
  default:
    current_lang="en";
    lang_jp="0";
    multi_lang=0;
  break;
  }
  $.get("/cgi-bin/rdb.cgi?<%appendCsrfTokenToQueryString();%>webinterface.language="+current_lang+"&webinterface.lang_jp="+lang_jp, function(){setLanguage(current_lang)});
}
document.documentElement.lang=current_lang;
document.cookie="language="+current_lang;
#endif
Butterlate.setTextDomain(fn.replace("/", ""));

var service_pppoe_server_0_enable="<%get_single_direct('service.pppoe.server.0.enable');%>";
#ifdef V_ROUTER_TERMINATED_PPPOE
var service_pppoe_server_0_wanipforward_enable="<%get_single_direct('service.pppoe.server.0.wanipforward_enable');%>";
#endif

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
<%supportCsrfTokenOnDocumentReady();%>
