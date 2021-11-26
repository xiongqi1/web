var ray_dhcp_start = new Array();
var ray_dhcp_end = new Array();
var dhcpRange;
var newDhcpRange;
var currLanIp;

// Parse DHCP range and save to variable which to be used for
// DHCP address range and LAN IP address cross check later
// Also save current LAN IP address for redirection condition check
// @params obj A target page object
// @return obj Returns without changed
function parseDhcpRange (obj) {
  dhcpRange = obj.dhcpRange;
  ray_dhcp_start = dhcpRange.split(",")[0].split(".");
  if(ray_dhcp_start.length != 4 ) {
    ray_dhcp_start[0] = ray_dhcp_start[1] = ray_dhcp_start[2] = ray_dhcp_start[3] = '0';
  }
  ray_dhcp_end = dhcpRange.split(",")[1].split(".");
  if(ray_dhcp_end.length != 4 ){
    ray_dhcp_end[0] = ray_dhcp_end[1] = ray_dhcp_end[2] = ray_dhcp_end[3] = '0';
  }
  currLanIp = obj.lanIp;
  return obj;
}

// Perform DHCP address range and LAN IP address cross check
// Ported from legacy v2 WEBUI and this logic was verified already
// The purpose is to adjust DHCP address range dynamically when
// LAN IP address and subnet mask are changed
// @params obj A target page object
// @return newDhcpRange is calculated If no error
//         Error message if the validity check fails
function dhcpRangeCrossCheck(obj: any) {
  var myip = obj.lanIp;
  var mymask = obj.lanMask;
  var ipAr = new Array();
  var maskAr = new Array();
  ipAr = myip.split('.');
  maskAr = mymask.split('.');

  var firstAr = new Array(), negAr = new Array(), lastAr = new Array();
  var DHCPfirstAr = new Array(), DHCPnegAr = new Array(), DHCPlastAr = new Array();
  var same_subnet = 1;
  var i;
  for (i = 0; i < 4; i++) {
    firstAr[i] = ipAr[i] & maskAr[i]; negAr[i] = 255 - maskAr[i]; lastAr[i] = firstAr[i] | negAr[i];
    DHCPfirstAr[i] = ray_dhcp_start[i] & maskAr[i]; DHCPnegAr[i] = 255 - maskAr[i]; DHCPlastAr[i] = DHCPfirstAr[i] | DHCPnegAr[i];
    if (firstAr[i] != DHCPfirstAr[i] || lastAr[i] != DHCPlastAr[i]) {
      same_subnet = 0;
    }
  }
  firstAr[3] += 1; lastAr[3] -= 1;
  DHCPfirstAr[3] += 1; DHCPlastAr[3] -= 1;

  // check ip address validity after subnet mask changed
  if (!(is_large(ipAr, firstAr) && is_large(lastAr, ipAr))) {
    return "Invalid IP address";
  }

  if(ray_dhcp_start[0] && ray_dhcp_start[1] && ray_dhcp_start[2] && ray_dhcp_start[3] &&
     ray_dhcp_end[0] && ray_dhcp_end[1] && ray_dhcp_end[2] && ray_dhcp_end[3]) {
    var dhcp_range_size, ip_range_size;
    ip_range_size = ip_gap(lastAr, firstAr);

    // if DHCP address has different subnet from IP address, reset DHCP address into same subnet before calculation
    if (same_subnet == 0) {
      for (i = 0; i < 3; i++) {
        ray_dhcp_start[i] = ray_dhcp_end[i] = ipAr[i];
      }
    }

    dhcp_range_size = ip_gap(ray_dhcp_end, ray_dhcp_start);

    // if dhcp range is outside of possble ip address range, adjust dhcp range
    if (is_large(firstAr, ray_dhcp_start) || is_large(ray_dhcp_start, lastAr) || is_large(firstAr, ray_dhcp_end)) {
      for (i = 0; i < 4; ray_dhcp_start[i] = firstAr[i], i++);
      ray_dhcp_end = decimal_to_ip(ip_to_decimal(ray_dhcp_start)+dhcp_range_size);
    }
    if (is_large(ray_dhcp_end, lastAr)) {
      for (i = 0; i < 4; ray_dhcp_end[i] = lastAr[i], i++);
    }

    dhcp_range_size = ip_gap(ray_dhcp_end, ray_dhcp_start);

    // if ip address is within dhcp range, check if need to adjust dhcp range
    if (is_large(ipAr, ray_dhcp_start) && is_large(ray_dhcp_end, ipAr)) {
      // if there is enough gap for dhcp range, locate ip addr out side of dhcp range
      if (ip_gap(ipAr, lastAr) >= dhcp_range_size) {
        for (i = 0; i < 3; ray_dhcp_start[i] = ipAr[i], i++);
        ray_dhcp_start[3] = parseInt(ipAr[3])+1;
        ray_dhcp_end = decimal_to_ip(ip_to_decimal(ray_dhcp_start)+dhcp_range_size);
      } else if (ip_gap(ipAr, firstAr) >= dhcp_range_size) {
        for (i = 0; i < 4; ray_dhcp_start[i] = firstAr[i], i++);
        ray_dhcp_end = decimal_to_ip(ip_to_decimal(ray_dhcp_start)+dhcp_range_size);
      }
    }
    if (is_large(ray_dhcp_end, lastAr)) {
      for (i = 0; i < 4; ray_dhcp_end[i] = lastAr[i], i++);
    }

    newDhcpRange = ray_dhcp_start[0]+"."+ray_dhcp_start[1]+"."+
                   ray_dhcp_start[2]+"."+ray_dhcp_start[3]+","+
                   ray_dhcp_end[0]+"."+ray_dhcp_end[1]+"."+
                   ray_dhcp_end[2]+"." +ray_dhcp_end[3];
  }
}

// Custom submit function for the save button in this page
// Do DHCP address range and LAN IP address cross check
// Redirect to index page on new address if LAP IP address was changed
function onClickSaveBtnLanCfg(){
  // Get the object to validate
  var obj = LanCfg.packageObj();
  var errMsg = dhcpRangeCrossCheck(obj);
  if (isDefined(errMsg)) {
    validate_alert("", _(errMsg));
    return;
  }
  // If DHCP range is not changed then do not trigger
  // dnsmasq-dhcp
  if (dhcpRange == newDhcpRange) {
    delete LanCfg.obj.dhcpRange;
    delete LanCfg.obj.dhcpTrig;
  }
  sendSingleObject(LanCfg);
  // Redirect to index page on new IP address if changed
  if (currLanIp != obj.lanIp) {
    console.log("redirect to index.html")
    var newAddr = location.protocol + "//" + obj.lanIp + "/index.html";
    // Redirect after some delay time otherwise it may fail to receive objects
    // when the browser response time is too fast.
    setTimeout(function () {
       window.location.href = newAddr;
    }, 2000);
  }
}

var LanCfg = PageObj("LanCfg", "LAN configuration",
{
  members: [
    editableIpAddressVariable("lanIp", "IP address").setRdb("link.profile.0.address"),
    editableIpMaskVariable("lanMask", "Subnet mask").setRdb("link.profile.0.netmask"),
    editableHostname("lanHost", "Hostname").setRdb("link.profile.0.hostname").setEncode(true),
#ifdef NO_USE_UNTIL_BACKEND_READY
    toggleVariable("dnsMasq", "DNS masquerading").setRdb("service.dns.masquerade"),
#endif
    hiddenVariable("dhcpRange", "service.dhcp.range.0"),
    hiddenVariable("dhcpTrig", "service.dhcp.trigger"),
  ],
  // Called before the data is sent to device.
  encodeRdb: function(obj) {
#ifdef NO_USE_UNTIL_BACKEND_READY
    if (!toBool(obj.dnsMasq)) {
      obj.dnsMasq = "0";
    }
#endif
    obj.dhcpRange = newDhcpRange;
    obj.dhcpTrig = "1";
    return obj;
  },
  decodeRdb: parseDhcpRange,
  getValidationError: dhcpRangeCrossCheck,
});

var pageData : PageDef = {
  title: "LAN",
  menuPos: ["Networking", "LAN", "LanSettings"],
  pageObjects: [LanCfg],
  onReady: function (){
    appendButtons({"save":"CSsave"});
    setButtonEvent('save', 'click', onClickSaveBtnLanCfg);
  }
}
