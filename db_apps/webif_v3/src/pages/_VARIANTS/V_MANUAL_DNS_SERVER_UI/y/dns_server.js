var DnsServerCfg = PageObj("DnsServerCfg", "DNS server configuration",
{
  members: [
    editableIpAddressVariable("dns1", "Primary DNS server")
        .setRdb("service.dns.server.1")
        .setRequired(false),
    editableIpAddressVariable("dns2", "Secondary DNS server")
        .setRdb("service.dns.server.2")
        .setRequired(false),
  ],
});

var pageData : PageDef = {
  title: "DNS server",
  menuPos: ["Services", "DnsServer"],
  pageObjects: [DnsServerCfg],
  onReady: function (){
    appendButtons({"save":"CSsave"});
    setButtonEvent('save', 'click', sendObjects);
  }
}
