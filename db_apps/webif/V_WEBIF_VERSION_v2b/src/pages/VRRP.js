// This bounded integer type is used in a few fields
// so this function removes some repitition
function Counter3_65535(memberName: string, labelText: string, helperText?: string) {
  if (!isDefined(helperText)) {
    helperText = "failover count2";
  }
  return editableBoundedInteger(memberName, labelText, 3, 65535, "field3and65535", {helperText: helperText});
}

// This replaces the auto stub because of the other objects it controls
function onClickVrrpEnable(toggleName, v) {
  Vrrp.setVisible(v);
#ifdef V_VRRP_WAN_WATCHDOG_y
  var enabled = toBool(v);
  onClickWatchdogEnable(toggleName, enabled ? VrrpWanWatchdog.obj.WatchdogEnable: "0");
  setVisible("#objouterwrapperVrrpWanWatchdog", v);
#endif
}

// The main page object
var Vrrp = PageObj("VrrpCfg", "vrrp config",
  {
    rdbPrefix: "service.vrrp.",
    members: [
      objVisibilityVariable("VrrpEnable", "VRRP").setRdb("enable"),
      editableBoundedInteger("deviceId", "virtual id", 1, 255, "virtualIdRange", {helperText: "(1-255)"})
        .setRdb("deviceid").setSmall(),
      editableBoundedInteger("routerPriority", "router priority", 1, 255, "routerPriorityRange", {helperText: "(1-255)"})
        .setRdb("priority").setSmall(),
      editableIpAddressVariable("virtualipaddr", "virtualipaddr").setRdb("address")
    ]
  }
);

#ifdef V_VRRP_WAN_WATCHDOG_y

// This replaces the auto stub because of the other objects it controls
function onClickWatchdogEnable(toggleName, v) {
  VrrpWanWatchdog.setVisible(v);
  setVisible("#objouterwrapperConsecutiveErrorMonitor", v);
  setVisible("#objouterwrapperPeriodicRatioMonitor", v);
}

// The two destination fields have an extra field that is updated
// by the ping test.
// This adds the html to the div
// dst is the object name, pingdst or pingdst2
function addUpdateField(dst) {
	var div = document.getElementById('div_' + dst);
	if (div) {
		div.innerHTML += htmlDiv({style: "inline"}, "&nbsp;" +
                      htmlTag("i", {style: "display:none", id: dst + "_wait", class: "progress-sml"}, "") +
                      htmlTag("label", {id: dst + "_stat", style:"width:40px"}, "" )
			);
	}
}

// This is called on key up in the destination fields
// 'this' is the element in question
// The pinging state is stored in 'this' instead of new variables (like the previous version)
// because it's simpler
function doPingTest() {
	var el = this;
	var id = el.id.split("_");	// Id should be in the form inp_pingdst2
	if (id.length !== 2) return;
	id = id[1];	// pingdst2

	// get peripheral elements
	var waitIcon=$("#" + id + "_wait");
	var pingResult=$("#" + id + "_stat");

	el.value = el.value.trim();     // trim white spaces from copy and paste
	var server = el.value;
	// bypass if no server is available
	if(server.length==0) {
		// hide pinging icon
		waitIcon.hide();
		pingResult.hide();
		return;
	}

	// bypass if we have no change in the server
	if(el.server === server) return;
	el.server = server;

	if (el.pinging === true) return;	// do nothing if already pinging

	// don't ping if IP or domain name is invalid
	if (!(isValidIpAddress(server) || is_valid_domain_name(server) || isValidIpv6Address(server))) {
		pingResult.html(_("fail"));
		pingResult.show();
		return ;
	}

	el.pinging = true;
	waitIcon.show();
	pingResult.hide();

	$.getJSON( "./cgi-bin/ltph.cgi",
		{reqtype :"ping", reqparam: server},
		function(res){
			el.pinging = false;
			waitIcon.hide();
			pingResult.html((res.cgiresult == 0)? _("succ"): _("fail"));
			pingResult.show();
			if(el.server !== el.value) {	// trigger keyup if we have a new server
				$(el).trigger("keyup");
			}
		}
	);
}

function pingableHostname(memberName: string, labelText: string, extras?: any) {
  var pe = editableHostname(memberName, labelText, extras);
  pe.onInDOM = function () {
    addUpdateField(this.memberName);
    $("#inp_" + this.memberName).focusout( doPingTest );
  }
  return pe;
}

var VrrpWanWatchdog = PageObj("VrrpWanWatchdog", "vrrp wan watchdog",
  {
    customLua : {
      lockRdb: true,
      validate: function(luaValidateScript) {
        // We want to bypass all the checks if the block is disabled
        // First validate script has set v to WatchdogEnable
        luaValidateScript.splice( 1, 0, "if toBool(v)==false then return true end");
        return luaValidateScript;
       }
    },
    rdbPrefix: "service.vrrp.wanwatchdog.",
    members: [
      objVisibilityVariable("WatchdogEnable", "vrrp wan watchdog").setRdb("enable"),
      toggleVariable("verboseLogging", "verbose logging").setRdb("verbose_logging"),
      pingableHostname("pingdst", "monitor destinationAddress").setRdb("destaddress"),
      pingableHostname("pingdst2", "monitor secondAddress").setRdb("destaddress2"),
      Counter3_65535("pingTimer", "monitor pingTimer","failover period").setRdb("periodicpingtimer"),
      editableBoundedInteger("retryTimer", "retry timer", 2, 65535, "field2and65535", {helperText: "failover period2"})
        .setRdb("pingacceleratedtimer")
    ]
  }
);

var ConsecutiveErrorMonitor = PageObj("ConsecutiveErrorMonitor", "failover consecutive error monitor",
  {
    rdbPrefix: "service.vrrp.wanwatchdog.",
    members: [
      objVisibilityVariable("ConsecutiveMonEnable", "failover consecutive error monitor").setRdb("cons_mon_enable"),
      Counter3_65535("cFailCnt", "failover monitor failCount").setRdb("failcount"),
      Counter3_65535("cSuccCnt", "failover monitor succCount").setRdb("succcount")
    ]
  }
);

var PeriodicRatioMonitor = PageObj("PeriodicRatioMonitor", "failover periodic ratio monitor",
  {
    rdbPrefix: "service.vrrp.wanwatchdog.",
    members: [
      objVisibilityVariable("PeriodicMonEnable", "failover periodic ratio monitor").setRdb("rand_mon_enable"),
      Counter3_65535("totalCnt", "failover monitor randerr totalCount").setRdb("randerrtotalcount")
        .setValidate(
          function (vals: string) {
                var val = Number(vals);
                var obj = PeriodicRatioMonitor.packageObj(); // Get the html variables from the page
                var f=parseInt(obj.failCnt);
                if( !isNaN(f) && val < f && val != 0 ) {
                    return false;
                }
				return true;
              }, "fieldgreaterthanfailcnt"
        )
        .setValidate(
          function (vals: string) {
                var val = Number(vals);
                var obj = PeriodicRatioMonitor.packageObj(); // Get the html variables from the page
                var s=parseInt(obj.succCnt);
                if( !isNaN(s) && val < s && val != 0 ) {
                    return false;
                }
				return true;
              }, "fieldgreaterthansucccnt"
        ),
      Counter3_65535("failCnt", "failover monitor failCount").setRdb("randerrfailcount")
        .setValidate(
          function (vals: string) {
                var val = Number(vals);
                var obj = PeriodicRatioMonitor.packageObj(); // Get the html variables from the page
                var t=parseInt(obj.totalCnt);
                if( !isNaN(t) && val > t && val != 0 ) {
                    return false;
                }
				return true;
              }, "fieldlessthantotal"
        ),
      Counter3_65535("succCnt", "failover monitor succCount").setRdb("randerrsucccount")
        .setValidate(
          function (vals: string) {
                var val = Number(vals);
                var obj = PeriodicRatioMonitor.packageObj(); // Get the html variables from the page
                var t=parseInt(obj.totalCnt);
                if( !isNaN(t) && val > t && val != 0 ) {
                    return false;
                }
				return true;
              }, "fieldlessthantotal"
        )
    ]
  }
);
#endif

var pageData : PageDef = {
#if defined V_NETWORKING_UI_none || defined V_VRRP_none
  onDevice : false,
#endif
  title: "VRRP",
  menuPos: ["Internet", "VRRP"],
  pageObjects: [Vrrp
#ifdef V_VRRP_WAN_WATCHDOG_y
		,VrrpWanWatchdog,ConsecutiveErrorMonitor,PeriodicRatioMonitor
#endif
  ],
  alertSuccessTxt: "vrrpSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
    // This needs to be called explicitly because of the other objects it controls
    if(isDefined(Vrrp.obj) && isDefined(Vrrp.obj.VrrpEnable)) {
      onClickVrrpEnable("", Vrrp.obj.VrrpEnable);
    }
  }
}

disablePageIfPppoeEnabled();
