<script language="JavaScript">

<%
if (request['REQUEST_METHOD'] == "POST") {

	function setRdb(varRdb, val) {
		set_single_direct("-p",varRdb, val);
	}
	function setDsmRdb(varRdb, val) {
		setRdb("service.dsm.ep.conf."+varRdb, val);
	}

	i=validate_number(form['idxEP']);
	EP_Name=form['nameEP'];

	// validate input
	if (string_regex_match(EP_Name, "^[!()*-/0-9;?A-Z_a-z-]+$") == false) {
		onBadRequest();
	}
#if 0
	// The following fields take value from lists so ideally those lists should  be checked.
	// However the lists are at client side only.
	// To prevent code injection, only alphanumeric characters are permitted.
#endif
	if (string_regex_match(form['baud'], "^[0-9]+$") == false) {
		onBadRequest();
	}
	if (string_regex_match(form['databits'], "^[0-9]+$") == false) {
		onBadRequest();
	}
	if (string_regex_match(form['stopbits'], "^[0-9]+$") == false) {
		onBadRequest();
	}
	if (string_regex_match(form['parity'], "^[0-9A-Za-z]+$") == false) {
		onBadRequest();
	}

	setDsmRdb(i+".name", EP_Name);
	setDsmRdb(i+".type",form['typeEP']);
	setDsmRdb(EP_Name+".dev_name",form['devName']);
	setDsmRdb(EP_Name+".bit_rate",form['baud']);
	setDsmRdb(EP_Name+".data_bits",form['databits']);
	setDsmRdb(EP_Name+".stop_bits",form['stopbits']);
	setDsmRdb(EP_Name+".parity",form['parity']);
	set_single_direct("","service.dsm.trigger","1");
	redirect('/end_points.html?success');
}
%>
function submitForm() {
#ifdef V_WEBIF_SPEC_vdf
/*********for vdf validator**********/
	if(!$("#form").valid()) {
		return;
	}
#else
	if(!$("#form").validationEngine("validate")) {
		validate_alert("","");
		return;
	}
#endif
	blockUI_wait(_("GUI pleaseWait"));
	for (var i=0; i<endpoints.length;i++) {
		if(endpoints[i].name==EP_Name) {
			break;
		}
	}

	var host_if;
	if($("#host_if").val()=="custom") {
		host_if=$("#custom_if").val();
	}
	else {
		host_if=$("#host_if").val();
	}

	$("#idxEP").val(i);
	$("#nameEP").val(EP_Name);
	$("#typeEP").val(ep_type[0]);
	$("#devName").val(host_if);
	$("#form").submit();
}

$(document).ready(function() {
	add_options("baud", baud_rate_list, 115200);
	add_options("databits", databits_list, 8);
	add_options("stopbits", stopbits_list, 1);
	add_options("parity", parity_list, "none");
	//add_options("fc", fc_list, 0);
<%
	dev_name=get_single('service.dsm.ep.conf.'+request['QUERY_STRING']+'.dev_name', '', '--base64');
	if (dev_name == "" || dev_name == "N/A") {
%>
	var host_if="";
<%
	} else {
%>
	var host_if=Base64.decode("@@dev_name");
<%	}%>
	if(host_if_list_length>0) {
		$.extend(true, host_if_list, {"custom":"custom"});
		add_sif_options("host_if", host_if_list, host_if);
	}
	else {
		$("#custom_if-div").css("display", "");
		$("#host_if-div").css("display", "none");
	}

	$("#host_if").change(function() {
		if(this.value=="custom") {
			$("#custom_if-div").css("display", "");
		}
		else {
			$("#custom_if-div").css("display", "none");
		}
	});

	/* apply darkgrey colour to hostif */
	$("#host_if").change(function(){
		var greyout=$(this).find(":selected").hasClass("disabled");
		$(this).toggleClass("disabled",greyout);
	});

	$("#host_if").trigger("change");
	var v="<%get_single_direct('service.dsm.ep.conf.'+request['QUERY_STRING']+'.bit_rate');%>";
	$("#baud").val(v==""?"115200":v);
	v="<%get_single_direct('service.dsm.ep.conf.'+request['QUERY_STRING']+'.data_bits');%>";
	$("#databits").val(v==""?"8":v);
	v="<%get_single_direct('service.dsm.ep.conf.'+request['QUERY_STRING']+'.stop_bits');%>";
	$("#stopbits").val(v==""?"1":v);
	v="<%get_single_direct('service.dsm.ep.conf.'+request['QUERY_STRING']+'.parity');%>";
	$("#parity").val(v==""?"none":v);
 	$("#ep_name").html(g_ep_types_list[ep_type[0]]+"  "+_("endPoint")+"   ("+htmlNumberEncode(EP_Name)+")");
});
</script>

<div class="header-wrap" id="main-menu"><!--Top Menu--></div>
<div id="content" class="site-content">
<div class="container">
	<aside class="grid-3 alpha sidemenu" id="side-menu"><!--Side Menu--></aside>
	<div class="grid-9 omega">
		<form><!--place holder for validation--></form>
		<div class="right-column white-box">
			<form name="form" id="form" class="validate" novalidate="novalidate" method="POST" action="@@request['SCRIPT_NAME']">
			<%appendCsrfToken();%>
			<input type="hidden" name='idxEP' id='idxEP' value="">
			<input type="hidden" name='nameEP' id='nameEP' value="">
			<input type="hidden" name='typeEP' id='typeEP' value="">
			<input type="hidden" name='devName' id='devName' value="">
			<div class="pad" id="setup">
				<h2 id="ep_name"></h2>
				<div class="grey-box">
					<div class="form-row" id="host_if-div">
						<label for="host_if"><script language=Javascript>document.write(_("hostPort"))</script></label>
						<div class="field">
		 					<select id="host_if" name="host_if" style="width:auto"></select>
<!-- 							<input type="text" name="host_if" id="host_if" class="large required" value=""> -->
						</div>
					</div>
					<div class="form-row" id="custom_if-div">
						<label for="custom_if"><script language=Javascript>document.write(_("customHost"))</script></label>
						<div class="field">
							<input type="text" name="custom_if" id="custom_if" class="large required validate[required]" onKeyUp="hostNameFilter(this);" value="">
						</div>
					</div>
					<div class="form-row">
						<label for="baud"><script language=Javascript>document.write(_("baud rate"))</script></label>
						<div class="field">
							<select id="baud" name="baud"></select>
						</div>
					</div>
					<div class="form-row">
						<label for="databits"><script language=Javascript>document.write(_("dataBits"))</script></label>
						<div class="field">
							<select id="databits" name="databits"></select>
						</div>
					</div>
					<div class="form-row">
						<label for="stopbits"><script language=Javascript>document.write(_("stopBits"))</script></label>
						<div class="field">
							<select id="stopbits" name="stopbits"></select>
						</div>
					</div>
					<div class="form-row">
						<label for="parity"><script language=Javascript>document.write(_("parity"))</script></label>
						<div class="field">
							<select id="parity" name="parity"></select>
						</div>
					</div>
				</div>
				<div class="submit-row" id="submit-div">
					<button type="button" onClick="submitForm();"><script language=Javascript>document.write(_("CSsave"))</script></button>
					<button type="button" class="secondary" onClick="window.location='end_points.html'"><script language=Javascript>document.write(_("cancel"))</script></button>
				</div>
			</div>
			</form>
		</div>
	</div>
</div>
</div>
<footer class="footer">
	<div class="container">
		<p class="copy-right"><script language=Javascript>document.write(_("powered by netComm"))</script></p>
	</div>
</footer>

<script language='javascript'>
        set_menu("Services", "EDP", <%_val = session["user"];%>"@@_val");
</script>
