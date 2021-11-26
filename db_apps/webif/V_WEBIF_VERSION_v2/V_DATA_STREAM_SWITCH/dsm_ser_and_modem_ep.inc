<script language="JavaScript" src="/cgi-bin/end_points.cgi?getList"></script>
<script language="JavaScript" src="/cgi-bin/end_points.cgi?serialList"></script>
<script language="JavaScript">
<%
var epName = base64_encode(request['QUERY_STRING']);
%>
var EP_Name="@@epName";
EP_Name=Base64.decode(EP_Name);
var host_if_list_length=0;

var scriptname="@@request['SCRIPT_NAME']"
var ep_type=scriptname.match("[0-9]+");

if (ep_type[0] == "undefined")
	ep_type[0]="1";

if(typeof(host_if_list)!="undefined") {
	$.map(host_if_list, function(x,y) {
		host_if_list_length++;
	});
}

var baud_rate_list={"115200":"115200", "57600":"57600", "38400":"38400", "19200":"19200", "9600":"9600", "4800":"4800", "2400":"2400", "1200":"1200", "600":"600", "300":"300"};
var databits_list={"8":_("8bit"), "7":_("7bit"), "6":_("6bit"), "5":_("5bit")};
var stopbits_list={"1":"1", "2":"2"};
var parity_list={"none":_("none"), "even":_("even"), "odd":_("odd")};
var sw_flow_ctrl_list={"0":_("none"), "1":_("Xon/Xoff")};
var hw_flow_ctrl_list={"0":_("none"), "1":_("RTS/CTS")};

function add_sif_options(myid, mylist, def) {
	var attr_disabled="class='disabled'";
	var attr_enabled="class='enabled'";

	mydef="custom";
	$.each( ["#"+myid], function(idx,el) {
		$.each( mylist, function(val,attr) {
			var buildinSerial=0;
#ifndef V_SERIAL_none
			if(attr=="platform") {
				txt=_("builtinSerial");
				attr=attr_enabled;
				buildinSerial=1;
			}
			/* use USB gadget serial for USB gadget */
			else if( attr.match(/^inserted +gadget-.*$/) ) {
				txt=_("gadgetSerial");
				attr=attr_enabled;
			}
			else if( attr.match(/^disabled +gadget-.*$/) ) {
				txt=_("gadgetSerial");
				attr=attr_disabled;
			}
			else if( attr.indexOf("inserted")!=-1) {
				txt=_("externalSerial")+" ("+attr.substring(8)+")";
				attr=attr_enabled;
			}
			else if( attr=="custom") {
				txt=_("custom");
				attr=attr_enabled;
			}
			else {
				txt=_("externalSerial")+" ("+attr.substring(8)+")";
				attr=attr_disabled;
			}
#else
			if(attr!="platform") {
				/* use USB gadget serial for USB gadget */
				if( attr.match(/^inserted +gadget-.*$/) ) {
					txt=_("gadgetSerial");
					attr=attr_enabled;
				}
				else if( attr.match(/^disabled +gadget-.*$/) ) {
					txt=_("gadgetSerial");
					attr=attr_disabled;
				}
				else if( attr.indexOf("inserted")!=-1) {
					txt=_("externalSerial")+" ("+attr.substring(8)+")";
					attr=attr_enabled;
				}
				else if( attr=="custom") {
					txt=_("custom");
					attr=attr_enabled;
				}
				else {
					txt=_("externalSerial")+" ("+attr.substring(8)+")";
					attr=attr_disabled;
				}
			}
			else {
				return true;
			}
#endif
			if(def==val) {
				mydef=val;
			}
			if(ep_type[0]=="1" || buildinSerial == 1)
				$(el).append("<option value='"+val+"' "+attr+">"+txt+"</option>");
		});
	});
	if(def!="") {
		$("#"+myid).val(mydef);
		if(mydef=="custom") {
			$("#custom_if").val(def);
		}
	}
}
</script>
