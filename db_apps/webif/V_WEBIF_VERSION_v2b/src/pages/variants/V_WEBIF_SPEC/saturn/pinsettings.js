var toggle = 1;
function onChangePin() {
	if (toggle) {
		$("#div_newPin").show();
		$("#div_confNewPin").show();
	} else {
		$("#div_newPin").hide();
		$("#div_confNewPin").hide();
	}
	toggle = (toggle+1)%2;
}

var pinSettingsCfg = PageObj("PinSettings", "pin settings",
{
	members: [
		staticTextVariable("securityWarning", "simSecurityWarningMsg"),
		toggleVariable("pinProtect", "pinProtection").setRdb("wwan.0.sim.status.pin_enabled"),
		buttonAction("changePin", "changePIN", "onChangePin"),
		editablePasswordVariable("currPin", "current pin").setMaxLength(256),
		editablePasswordVariable("confCurrPin", "confirm current pin").setMaxLength(256),
		editablePasswordVariable("newPin", "newPin").setMaxLength(256),
		editablePasswordVariable("confNewPin", "confirmNewPin").setMaxLength(256),
		toggleVariable("rememberPin", "rememberPin").setRdb("wwan.0.sim.autopin")
	],
	decodeRdb: function (obj) {
		if (obj.pinProtect == "Disabled") {
			obj.pinProtect = "0";
		} else {
			obj.pinProtect = "1";
		}
		return obj;
	},
	encodeRdb: function(obj) {
		if (obj.pinProtect == "0") {
			obj.pinProtect = "Disabled";
		} else {
			obj.pinProtect = "Enabled";
		}
		return obj;
	}
});

var pageData : PageDef = {
	title: "PIN Setting",
	menuPos: ["Internet", "SIM_Security"],
	pageObjects: [pinSettingsCfg],
	alertSuccessTxt: "simSubmitSuccess",
	onReady: function () {
		$("#htmlGoesHere").after(genSaveRefreshCancel(true, false, false));
		$("#saveButton").on('click', sendObjects);
		$("#div_newPin").hide();
		$("#div_confNewPin").hide();
	}
}
