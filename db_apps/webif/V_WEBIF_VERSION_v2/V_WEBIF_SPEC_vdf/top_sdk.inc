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
<script language='javascript'>
var fn="@@_fn".replace(".html", "");
Butterlate.setTextDomain(fn.replace("/", ""));
var service_pppoe_server_0_enable="<%get_single_direct('service.pppoe.server.0.enable');%>";
var nas_installed="<%get_single_direct('system.package.installed.nas');%>";
</script>
<script type="text/javascript" src="/util.js?@@_sv"></script>
<script language="JavaScript" src="js/jquery.blockUI.min.js"></script>

<div class="header-wrap" id="main-menu"><!--Top Menu--></div>
<div id="content" class="site-content">
	<div class="container">
		<aside class="grid-3 alpha sidemenu" id="side-menu"><!--Side Menu--></aside>
	</div>
	<div class="grid-9 omega">
	<div class="pad">
