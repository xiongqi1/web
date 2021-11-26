<% useSession();
if( request["SESSION_ID"]!=session["sessionid"]) {
	redirect('/index.html');
	exit(403);
}
request['SCRIPT_NAME']="administration";
_sv=get_single('sw.version');
_fn=request['SCRIPT_NAME'];
request['SCRIPT_NAME']="administration";
%>
<script>
(function() {
	var html = document.documentElement;
	html.className = html.className.replace('no-js','') + ' js';
})();
var modules = [];
</script>
<link rel="stylesheet" href="/css/validationEngine.jquery.css" type="text/css"/>
<script src="/js/jquery.min.js" type="text/javascript"></script>
<script src="/js/jquery.validationEngine.min.js" type="text/javascript" charset="utf-8"></script>
<script src="/js/jquery.validate.min.js" type="text/javascript" charset="utf-8"></script>

<!--[if lt IE 9]>
<script src="/js/selectivizr.js"></script>
<script src="/js/modernizr.js"></script>
<noscript>
	<link rel="stylesheet" href="/css/ie.css">
</noscript>
<![endif]-->
<link rel="stylesheet" href="/css/style.css?@@_sv">
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
<script src="/js/script.js?@@_sv"></script>
<!--**********************************************************-->
<script type="text/javascript" src="/lang/b28n.min.js?@@_sv"></script>
<script language='javascript'>
var fn="@@_fn".replace(".html", "");
if(fn=="" || fn=="/") {
	fn="index";
}
else if(fn=="@@_fn") {
	fn="administration";
}
Butterlate.setTextDomain(fn.replace("/", ""));
$(window).load(function(){$("#form").validationEngine();});

var service_pppoe_server_0_enable="<%get_single_direct('service.pppoe.server.0.enable');%>";
#ifdef V_ROUTER_TERMINATED_PPPOE
var service_pppoe_server_0_wanipforward_enable="<%get_single_direct('service.pppoe.server.0.wanipforward_enable');%>";
#endif

</script>
<script type="text/javascript" src="/util.js?@@_sv"></script>
<script language="JavaScript" src="js/jquery.blockUI.min.js"></script>
<!--**********************************************************-->

<div class="header-wrap" id="main-menu"><!--Top Menu--></div>
<div id="content" class="site-content">
	<div class="container">
		<aside class="grid-3 alpha sidemenu" id="side-menu"><!--Side Menu--></aside>
	</div>
	<div class="grid-9 omega">
	<div class="pad">
