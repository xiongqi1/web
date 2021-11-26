<!--- dial on demand feature ui --->
		
<table class="dod_table1">
	<tr>
		<th id="dial_on_demand_title"></th>
	</tr>
	
	<tr>
		<td>
			<p id="dial_on_demand_desc"></p>
		</td>
	</tr>
	
</table>		
		
<table class="dod_table1">
	<tr>
		<td width="30%" id="dod_select_title">
		</td>

		<td>
			<select id="dod_enable" name="dod_enable">
				<option value="0"></option>
				<option value="1"></option>
			</select>
		</td>
	</tr>
</table>
			
		
<table class="dod_table2">
	<tr>
		<td width="30%" id=dial_on_traffic>
		</td>

		<td>
			<p id="trigger_port"></p>
			<p><input type="checkbox" id="trigger_port_en" name="trigger_port_en"> <label id="trigger_port_en_label"></label></p>
			<p><input type="editbox" id="trigger_ports" name="trigger_ports"></p>
			
			<p id="ignore_protocols"></p>
			<p><input type="checkbox" id="dod_ignore_icmp" name="dod_ignore_icmp"> <label id="dod_ignore_icmp_label"></label></p>
			<p><input type="checkbox" id="dod_ignore_tcp" name="dod_ignore_tcp"> <label id="dod_ignore_tcp_label"></label></p>
			<p><input type="checkbox" id="dod_ignore_udp" name="dod_ignore_udp"> <label id="dod_ignore_udp_label"></label></p>
		
			<p id="ignore_apps"></p>
			<p><input type="checkbox" id="dod_ignore_dns" name="dod_ignore_dns"> <label id="dod_ignore_dns_label"></label></p>
			<p><input type="checkbox" id="dod_ignore_ntp" name="dod_ignore_ntp"> <label id="dod_ignore_ntp_label"></label></p>
			<p><input type="checkbox" id="dod_windows7" name="dod_windows7"> <label id="dod_windows7_label"></label></p>
			
		</td>
	</tr>
<table class="dod_table2">
	<tr>
		<td width="30%" rowspan=6 id=timeout_td></td>
		<td id="online_time_td"></td>
		<td><select id="online_time" name="online_time"></select></td>
	<tr>
		<td width=200px id=min_online_time_td></td>
		<td><select id="min_online_time" name="min_online_time" ></select></td>
	</tr>
	<tr>
		<td id="dial_delay_label"></td>
		<td><select id="dial_delay" name="dial_delay"></select></td>
	</tr>
		
	<tr>
		<td id="deactivation_time_td"></td>
		<td><select id="deactivation_time" name="deactivation_time"></select></td>
	</tr>
		
	
	<tr>
		<td id="periodic_dial_td" rowspan=2></td>
		<td>
			<p><label id="periodic_online_time_label"></label></p>
			<p><select id="periodic_online_time" name="periodic_online_time"></select></p>
		</td>
	</tr>
	<tr>
		<td>
			<p><label id="periodic_online_rndtime_label"></label></p>
			<p><select id="periodic_online_rndtime" name="periodic_online_rndtime"></select></p>
		</td>
		
	</tr>
		
</table>

	
<table class="dod_table2">
		
	<tr>
		<td width="30%" id="control_td">
		</td>
		<td>
			<div style="display:none">
			<p id="power_control_label"></p>
			<select id="power_control" name="power_control">
				<option value="0"></option>
				<option value="1"></optioni>
			</select>
		
			<p></p>
			<p></p>
			
			</div>
		
			<p><label id="online_status_label"></label> : <label id="dod_online_status"></label></p>
			<p></p>
			<p><input type="button" id="dod_connect" value="connect"><input type="button" id="dod_disconnect" value="Disconnect"></p>
		
		</td>
	</tr>
</table>


<!--- dial on demand feature server side script --->
<%

syslog(7,"dod.inc: start - req='"+request["REQUEST_METHOD"]+"'");

function checkbox_to_int(val) {
	if(val=="on")
		return "1";
		
	return "0";
}
		

if (request["REQUEST_METHOD"] == "POST") {
	syslog(7,"dod.inc: POST");
	
	//syslog(7,"dod.inc: dod_windows7='"+form["dod_windows7"]+"'");
	//syslog(7,"dod.inc: min_online_time='"+form["min_online_time"]+"'");
	//syslog(7,"dod.inc: periodic_online_rndtime='"+form["periodic_online_rndtime"]+"'");
	//syslog(7,"dod.inc: dod_ignore_tcp='"+form["dod_ignore_tcp"]+"'");


	// ports list
	set_single_direct("-p","dialondemand.ports_en",checkbox_to_int(form["trigger_port_en"]));
	if(form["trigger_ports"]!=undefined)
		set_single_direct("-p","dialondemand.ports_list",form["trigger_ports"]);
		
	// store only when trigger_port_en is not enabled
	if(checkbox_to_int(form["trigger_port_en"])=="0") {
		set_single_direct("-p","dialondemand.ignore_icmp",checkbox_to_int(form["dod_ignore_icmp"]));
		set_single_direct("-p","dialondemand.ignore_tcp",checkbox_to_int(form["dod_ignore_tcp"]));
		set_single_direct("-p","dialondemand.ignore_udp",checkbox_to_int(form["dod_ignore_udp"]));
		set_single_direct("-p","dialondemand.ignore_ntp",checkbox_to_int(form["dod_ignore_ntp"]));
		set_single_direct("-p","dialondemand.ignore_dns",checkbox_to_int(form["dod_ignore_dns"]));
	}
		
	set_single_direct("-p","dialondemand.ignore_win7",checkbox_to_int(form["dod_windows7"]));
	
	if(form["min_online_time"]!=undefined)
		set_single_direct("-p","dialondemand.min_online",form["min_online_time"]);
		
	set_single_direct("-p","dialondemand.dial_delay",form["dial_delay"]);
	set_single_direct("-p","dialondemand.traffic_online",form["online_time"]);
	set_single_direct("-p","dialondemand.deactivation_timer",form["deactivation_time"]);
	set_single_direct("-p","dialondemand.periodic_online",form["periodic_online_time"]);
	
	if(form["periodic_online_rndtime"]!=undefined)
		set_single_direct("-p","dialondemand.periodic_online_random",form["periodic_online_rndtime"]);
		
	set_single_direct("-p","dialondemand.poweron",form["power_control"]);
	
	// dod template has the following rdb variable as a trigger
	set_single_direct("-p","dialondemand.enable",form["dod_enable"]);
	
	
	syslog(7,"dod.inc: done");
}

%>
		
<!--- dial on demand feature client side script --->
		
<script language="JavaScript" >

$(function() {
		/*
			apply localization and build elements
		*/
		
		$.each(
  			{
				"#dod_select_title":_("dial on demand config"),
				"#dial_on_traffic":_("dial on traffic"),
				"#dod_ignore_icmp_label":_("ignore icmp"),
				"#dod_ignore_tcp_label":_("ignore tcp"),
				"#dod_ignore_udp_label":_("ignore udp"),
				"#dod_ignore_ntp_label":_("ignore ntp"),
				"#dod_ignore_dns_label":_("ignore dns"),
				"#dod_windows7_label":_("ignore windows 7"),
				"#min_online_time_td":_("minimum online time after dial"),
				"#dial_delay_label":_("dial delay after hangup"),
				"#online_time_td":_("online time after access"),
				"#timeout_td":_("timeout"),
				"#control_td":_("control connection"),
				"#dod_connect":_("connect"),
				"#dod_disconnect":_("disconnect"),
				"#power_control_label":_("module power control"),
				"#power_control option[value='0']":_("power off"),
				"#power_control option[value='1']":_("power on"),
				"#periodic_dial_td":_("periodic dial"),
				"#online_status_label":_("online status"),
				"#dod_enable option[value=1]":_("enable"),
				"#dod_enable option[value=0]":_("disable"),
				"#ignore_protocols":_("ignore protocols"),
				"#ignore_apps":_("ignore apps"),
				"#dial_on_demand_title":_("dial on demand"),
				"#dial_on_demand_desc":_("dial on demand desc"),
				"#periodic_online_time_label":_("periodic online time"),
				"#periodic_online_rndtime_label":_("randomization"),
				"#deactivation_time_td":_("deactivation timer"),
				"#trigger_port":_("trigger ports"),
				"#trigger_port_en_label":_("enable port trigger"),
				
			},
   
			function(el,str){
				if($(el).is("input:button"))
					$(el).val(str);
				else
					$(el).html(str);
			}
		);
				
		$.each(
			["#dial_delay"],
     			function(idx,el) {
				$.each(
					{
						"0":_("immediately"),
						"3":"3 "+_("seconds"),
						"5":"5 "+_("seconds"),
						"10":"10 "+_("seconds"),
						"15":"15 "+_("seconds"),
						"20":"20 "+_("seconds"),
						"25":"25 "+_("seconds"),
						"30":"30 "+_("seconds"),
						"35":"35 "+_("seconds"),
						"45":"45 "+_("seconds"),
						"60":"1 "+_("minute"),
						"120":"2 "+_("minutes"),
						"180":"3 "+_("minutes"),
						"240":"5 "+_("minutes"),
					},
	 
	 				function(val,txt){
						$(el).append("<option value='"+val+"'>"+txt+"</option>");
					}
				);
			}
		);
		
		$.each(
			["#min_online_time","#online_time","#periodic_online_rndtime"],
     			function(idx,el) {
				$.each(
					{
						"1":"1 "+_("minute"),
						"2":"2 "+_("minutes"),
						"3":"3 "+_("minutes"),
						"5":"5 "+_("minutes"),
						"5":"5 "+_("minutes"),
						"10":"10 "+_("minutes"),
						"15":"15 "+_("minutes"),
						"20":"20 "+_("minutes"),
						"25":"25"+_("minutes"),
						"30":"30 "+_("minutes"),
						"35":"35 "+_("minutes"),
						"45":"45 "+_("minutes"),
						"60":"1 "+_("hour"),
					},
	 
	 				function(val,txt){
						$(el).append("<option value='"+val+"'>"+txt+"</option>");
					}
				);
			}
		);
		
		
		$.each(
			["#deactivation_time"],
     			function(idx,el) {
				$.each(
					{
						"0":_("never"),
						"1":"1 "+_("minute"),
						"2":"2 "+_("minutes"),
						"3":"3 "+_("minutes"),
						"5":"5 "+_("minutes"),
						"5":"5 "+_("minutes"),
						"10":"10 "+_("minutes"),
						"15":"15 "+_("minutes"),
						"20":"20 "+_("minutes"),
						"25":"25"+_("minutes"),
						"30":"30 "+_("minutes"),
						"35":"35 "+_("minutes"),
						"45":"45 "+_("minutes"),
						"60":"1 "+_("hour"),
					},
		
					function(val,txt){
						$(el).append("<option value='"+val+"'>"+txt+"</option>");
					}
				);
			}
		);
		
		$.each(
			["#periodic_online_time"],
     			function(idx,el) {
				$.each(
					{
						"0":_("never"),
						"1":"1 "+_("minute"),
						"2":"2 "+_("minutes"),
						"3":"3 "+_("minutes"),
						"5":"5 "+_("minutes"),
						"5":"5 "+_("minutes"),
						"10":"10 "+_("minutes"),
						"15":"15 "+_("minutes"),
						"20":"20 "+_("minutes"),
						"25":"25"+_("minutes"),
						"30":"30 "+_("minutes"),
						"35":"35 "+_("minutes"),
						"45":"45 "+_("minutes"),
						"60":"1 "+_("hour"),
						"120":"2 "+_("hours"),
						"180":"3 "+_("hours"),
						"240":"4 "+_("hours"),
						"300":"5 "+_("hours"),
						"360":"6 "+_("hours"),
						"720":"12 "+_("hours"),
},
		
					function(val,txt){
						$(el).append("<option value='"+val+"'>"+txt+"</option>");
					}
				);
			}
		);
		
		$("#periodic_online_time").change(function() {
				var period;
				var last;

				period=parseInt($(this).val());
				
				// disable bigger time period
				$("#periodic_online_rndtime option").each(function(){
						$(this).attr("disabled",parseInt($(this).val())>=period);
					}
				);
				
				// get last enabled
				last=$("#periodic_online_rndtime option:enabled:last");
				// disable if we have no enabled option
				$("#periodic_online_rndtime").attr("disabled",last.length==0);
				// change value if disabled
				if($("#periodic_online_rndtime option:selected").is(":disabled") && (last.length>0)) {
					$("#periodic_online_rndtime").val( last.val() );
				}
					
			}
		);
		

		/*
			poll ajax
		*/

		// ajax setup - 10 second timeout and no cache
		$.ajaxSetup({
			timeout:10000,
			cache:false,
			error:function(req,stat,err) {
				switch(stat) {
					case "timeout":
						alert(_("connection timeout"));
						break;
				}
			}
		})

		var disable_dod_control_buttons=0;
		var dod_control_command="";
		
		// ajax - dod online status
		function do_poll_dod_status() {
			$.getJSON("./cgi-bin/3gwwan.cgi",{reqtype:"get_status"},function(res,stat,req) {
					if(res.dod_enable)
						$("#dod_online_status").html( (res.status==1)?_("online"):_("offline") );
					else
						$("#dod_online_status").html( _("disabled") );
					
			
					if(dod_control_command=="connect" && res.status)
						disable_dod_control_buttons=0;
					else if (dod_control_command=="disconnect" && !res.status)
						disable_dod_control_buttons=0;

					$("#dod_connect").attr( "disabled",(res.status==1) || (res.dod_enable==0) || (disable_dod_control_buttons>0));
					$("#dod_disconnect").attr( "disabled",(res.status==0) || (res.dod_enable==0) || (disable_dod_control_buttons>0));
					$("#dod_online_status").attr(res.dod_enable==0);
					
					if(disable_dod_control_buttons>0)
						disable_dod_control_buttons--;
					
					// reschedule next poll
					setTimeout(do_poll_dod_status,500);
				}
			);
		}

		// disable all dod connect and disconnect buttons
		$("#dod_connect,#dod_disconnect").attr("disabled",true);
		

/*
		* from ajax.cgi

		stpf={
			[conntype:"always"],
		};
*/

		/* 
			event handlers
		*/

		min_online_time="<% get_single_direct('dialondemand.min_online'); %>";
		
		$("#min_online_time").change(function() {
			// update user prefer value
			min_online_time=$(this).val();
		});
		
		$("#online_time").change(function(){
			var ontime_time;
			var first;

			ontime_time=parseInt($(this).val());
			
			// restore user prefer value
			$("#min_online_time").val(min_online_time);
			
			// disable bigger time ontime_time
			$("#min_online_time option").each(function(){
					$(this).attr("disabled",parseInt($(this).val())<ontime_time);
				}
			);
			
			// get first enabled
			first=$("#min_online_time option:enabled:first");
			// disable if we have no enabled option
			$("#min_online_time").attr("disabled",first.length==0);
			// change value if disabled
			if($("#min_online_time option:selected").is(":disabled") && (first.length>0)) {
				$("#min_online_time").val( first.val() );
			}
		});
		
		$("#trigger_port_en").change(function(){
			var port_en;
			
			port_en=$(this).is(":checked");
			$("#trigger_ports").attr("disabled",!port_en);
			
			// disable or enable ignore settings
			$("#dod_ignore_icmp,#dod_ignore_tcp,#dod_ignore_udp,#dod_ignore_ntp,#dod_ignore_dns").attr("disabled",port_en);
			
		});
		
		$("#trigger_ports").keydown(function(e){
			// Allow: backspace, delete, tab, escape, and enter
			if ( e.keyCode == 46 || e.keyCode == 8 || e.keyCode == 9 || e.keyCode == 27 || e.keyCode == 13 || 
				// Allow: Ctrl+A
				(e.keyCode == 65 && e.ctrlKey === true) || 
				// Allow: home, end, left, right
				(e.keyCode >= 35 && e.keyCode <= 39))
				return true;
				
			// Ensure that it is a number and stop the keypress
			if ( (!e.shiftKey && (e.keyCode>=48 && e.keyCode<=57)) || (e.keyCode>=96 && e.keyCode<=105 ) )
				return true;
				
			if( e.keyCode == 188)
				return true;
		
			return false;
		});
		
		$("#dod_connect,#dod_disconnect").click(function(){
				var reqtype;
				
				reqtype=$(this).attr("id")=="dod_connect"?"connect":"disconnect";
				
				$("#dod_connect,#dod_disconnect").attr("disabled",true);
				
				disable_dod_control_buttons=30; // 10 times query (about 5 seconds)
				dod_control_command=reqtype;
				
				$.get("./cgi-bin/3gwwan.cgi",{reqtype:reqtype},function(res){
				 		if(res.cgiresult) {
				 			alert(_("cannot control dod"));
						}
					}
				);
			}
		);


		$("#dod_enable").change(function(){
				$(".dod_table2").toggle(!$(this).is(":checked") && ($(this).val()=="1"));
			}
		);

		$("#pppprofilenum").change(function(){
				var idx;
				idx=$(this).val();

				// use default selected item
				if( ($.type(stpf[idx].conntype)=="undefined") && (stpf[idx].conntype=="") )
					stpf[idx].conntype=$("#dod_enable").val();
				
				$("#dod_enable").val(stpf[idx].conntype).trigger("change");
			}
		);
		
		var savewwan_onclick;
		
		// unbind default html onclick
		savewwan_onclick=$("#saveWWAN").attr("OnClick");
		$("#saveWWAN").removeAttr("OnClick");
		
		$("#saveWWAN").click(function(){
		
			// re-arrage port list
			var ports;
			ports=$("#trigger_ports").val().split(",");
			
			// convert to numbers
			$.each(ports,function(idx,port){
				ports[idx]=parseInt(port,10)||0;
			});
			
			// sort
			ports.sort(function(a,b){
				return a-b;
			});
			
			// remove zeros and unique
			var prev=0;
			ports=$.grep(ports,function(n,i){
				var res;
				
				res=n!=prev;
				prev=n;
				
				return res;
			});
			
			// convert back to comma string
			$("#trigger_ports").val(ports.join(","));
			
			// check port list validation
			if($("#trigger_port_en").is(":checked") && (ports.length==0)) {
				alert(_("incorrect dod port"));
				$("#trigger_ports").focus();
				return false;
			}
			
			if($.type(savewwan_onclick)=="string")
				eval(savewwan_onclick);
				
			return false;
		});

		/* 
			apply configuration
		*/
		
		$("#dod_enable").val("<% get_single_direct('dialondemand.enable'); %>");

		$.each(
			{
				"#dod_ignore_icmp":"<% get_single_direct('dialondemand.ignore_icmp'); %>" == "1",
				"#dod_ignore_tcp":"<% get_single_direct('dialondemand.ignore_tcp'); %>" == "1",
    				"#dod_ignore_udp":"<% get_single_direct('dialondemand.ignore_udp'); %>" == "1",
				"#dod_ignore_ntp":"<% get_single_direct('dialondemand.ignore_ntp'); %>" == "1",
				"#dod_ignore_dns":"<% get_single_direct('dialondemand.ignore_dns'); %>" == "1",
				"#dod_windows7":"<% get_single_direct('dialondemand.ignore_win7'); %>"=="1",
				"#min_online_time":min_online_time,
				"#dial_delay":"<% get_single_direct('dialondemand.dial_delay'); %>",
				"#online_time":"<% get_single_direct('dialondemand.traffic_online'); %>",
				"#deactivation_time":"<% get_single_direct('dialondemand.deactivation_timer'); %>",
				"#periodic_online_time":"<% get_single_direct('dialondemand.periodic_online'); %>",
				"#periodic_online_rndtime":"<% get_single_direct('dialondemand.periodic_online_random'); %>",
				"#power_control":("<% get_single_direct('dialondemand.poweron'); %>"!="0")?1:0,
				"#trigger_port_en":"<% get_single_direct('dialondemand.ports_en'); %>" == "1",
				"#trigger_ports":"<% get_single_direct('dialondemand.ports_list'); %>"
			},
			function(el,val) {
					
					if($(el).is("input:checkbox"))
						$(el).attr("checked",val);
					else
						$(el).val(val);
			}
		);
		
		/*
			triggering to update
		*/
		
		$("#dod_enable").trigger("change");
		$("#periodic_online_time").trigger("change");
		$("#trigger_port_en").trigger("change");
		$("#online_time").trigger("change");

		// start poll timer
		do_poll_dod_status();
	}
);

</script>

<!--- dial on demand feature end --->
