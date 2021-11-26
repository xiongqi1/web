
<script language="JavaScript">
<%
write("var ModcommSlots=[];\n");

function writeElement(rdbname,elem) { // returns 'elem:"value"'
	return elem+':"'+get_single(rdbname+'.'+elem)+'"';
}

function writeModCommsArray() {
	for (var slot=0;slot<10;slot++)
	{
		var rdbname="modcomms.slot."+(slot+1);
		if ( get_single(rdbname+'.type') != 'N/A' ) {
			var objectString = '{'+
				writeElement(rdbname,'type') + ', ' +
				writeElement(rdbname,'status') + ', ' +
				writeElement(rdbname,'bootloaderVersion') + ', ' +
				writeElement(rdbname,'applicationVersion') + '};';
			write("ModcommSlots["+slot+"]="+objectString+"\n");
		}
	}
}

writeModCommsArray();
%>

function makeModCommsSlotList() {
	var h="";
	var len=ModcommSlots.length;
	for( i=0; i<len; i++ ) {
		if (!ModcommSlots[i]) continue;
		h+='<tr><td>'+substMice(ModcommSlots[i].type)
		+'</td><td>'+(i+1)
		+'</td><td>'+ModcommSlots[i].status
		+'</td><td>'+ModcommSlots[i].applicationVersion
		+'</td><td>'+ModcommSlots[i].bootloaderVersion
		+'</td></tr>';
	}
	$("#slotTable").html(h);
}

function writeTableHeadings( headings ) {
	var numHeadings=headings.length;
	for ( var i = 0; i< numHeadings;i++)
		document.write('<th class="align10">'+headings[i]+'</th>');
}
</script>

<div class="grey-box" id="io-div">
	<table class="above-5-column name-list grey-box">
		<colgroup>
			<col width="20%">
			<col width="10%">
			<col width="auto">
			<col width="20%">
			<col width="20%">
		</colgroup>
		<thead>
			<tr>
			<script language=Javascript>
				writeTableHeadings(
					[_("modCommsUnit"),
					_("modCommsSlot"),
					_("modCommsStatus"),
					_("modCommsAppVer"),
					_("modCommsBootloaderVer")]
					);
			</script>
			</tr>
		</thead>
		<tbody id="slotTable"></tbody>
	</table>
</div>
