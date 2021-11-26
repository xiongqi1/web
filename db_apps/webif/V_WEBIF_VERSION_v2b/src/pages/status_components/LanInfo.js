function lan_str(pos) {
    if (!isValidValue(pos) || pos.charAt(0) != 'u' || pos.charAt(1) != 'r') {
            return "<span style='display:inline-block;width:140px'>"+_("status down")+"</span><i class='warning-sml'></i>";
    }
    var status = " /  ";
    if(pos.charAt(2) == 'g')
        status += "1000 Mbps   /   ";
    else if(pos.charAt(2) == 'h')
        status += "100.0 Mbps   /   ";
    else
        status += "10.0 Mbps   /   ";

    if(pos.charAt(3) == 'f')
        status += "FDX";
    else
        status += "HDX";
    return "<span style='display:inline-block;width:140px'>"+_("status up") + status + "</span><i class='success-sml'/>";
}

var LanInfo = PageObj("StsLanInfo", "lan",
  {
    readOnly: true,
    column: 2,
    genObjHtml: genCols,
    columns : [{
    members:[
        {hdg: "IP", genHtml: (obj) => obj.lan_ip + " / " + obj.lan_mask},
        {hdg: "macAddress", genHtml: (obj) => obj.eth0mac},
        {hdg: "lan port status", genHtml: (obj) => lan_str(obj.port0sts)}
    ]
    }],

    populate: populateCols,

    members: [
    hiddenVariable("lan_ip","link.profile.0.address"),
    hiddenVariable("lan_mask","link.profile.0.netmask"),
#ifndef PLATFORM_Serpent
    hiddenVariable("eth0mac", "systeminfo.mac.eth0"),
#endif
    hiddenVariable("port0sts","hw.switch.port.0.status")
    ]
  }
);

stsPageObjects.push(LanInfo);
