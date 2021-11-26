/*
 * QXDM over Ethernet configuration
 *
 * Copyright (C) 2021 Casa Systems
 */

var QxdmEthernet = PageObj("QxdmEthernet", "QXDM configuration",
{
	members: [
		objVisibilityVariable("qxdmEtherEnable", "Enable").setRdb("service.qxdm_ethernet.enable"),
		editableIpAddressVariable("serverIp", "Server IP address").setRdb("service.qxdm_ethernet.server_ip"),
	]
});

var pageData : PageDef = {
	title: "QXDM over Ethernet",
	menuPos: ["System", "Log", "QxdmEthernet"],
	pageObjects: [ QxdmEthernet ],
	onReady: function() {
		appendButtons({"save":"CSsave"});
		setButtonEvent('save', 'click', sendObjects);
	}
};
