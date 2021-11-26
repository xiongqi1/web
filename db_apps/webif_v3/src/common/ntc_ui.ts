function cachedScriptLoad(url, options) {

  // Allow user to set any option except for dataType, cache, and url
  options = $.extend(options || {}, {
    dataType: "script",
    cache: true,
    url: url
  });

  // Use $.ajax() since it is more flexible than $.getScript
  // Return the jqXHR object so we can chain callbacks
  return jQuery.ajax( options );
};

// This function determines what objects (data) has to be requested and
// issues a request. This is called from the html page just after the page definition
// so the request goes out asap and while rest of the page is still initializing
function requestPageObjects() {
  var objectsToRequest = [];
  pageData.pageObjects.forEach(function(pgeObj) {
    const readGroups = pgeObj.readGroups ?? pageData.readGroups ?? ['root', 'admin'];
    if (pageData.authenticatedOnly === false || checkGroupAccess(readGroups, userGroups)) {
      objectsToRequest[objectsToRequest.length] = pgeObj.objName;
    }
  });
  requestObjects(objectsToRequest,receiveFirstObjects);
}

// This function request a single page object specified by pgeObj
// It can be called when a single page object needs to be updated
function requestSinglePageObject(pgeObj: PageObject) {
  requestObjects([pgeObj.objName], receiveObjects);
}

// return an array of page objects the need to be saved
function getObjectsToSend() {
  var objs=[];
  pageData.pageObjects.forEach(function(pgeObj) {
    const writeGroups = pgeObj.writeGroups ?? pageData.writeGroups ?? ['root', 'admin'];
    if (pageData.authenticatedOnly === false || checkGroupAccess(writeGroups, userGroups)) {
      var obj = pgeObj.packageObj();
      if (obj) objs[objs.length] = obj;
    }
  });
  return objs;
}

// Scan the page objects to see and check if there is a received object
// decodeRdb is called if defined to massage RDB data
// into a friendlier format - See gps.js for a good example
function receiveObjects(resp){
  var validateFailed = true;
  if (resp.result !== 0) {
    if (resp.text.indexOf("Invalid csrfToken") >= 0) {
      alertInvalidCsrfToken();
    }
    else if (isDefined(resp.errorText)) {
      validate_alert( _("invalidRequestTitle"), _(resp.errorText));
    }
    else if (resp.access === "NeedsAuthentication") {
      validate_alert( _("needAuthTitle"), _("needAuthMsg"));
    }
    else {
      alertInvalidRequest();
    }
  } else {
    validateFailed = false;
  }

  pageData.pageObjects.forEach(function(pgeObj) {
    var obj = resp[pgeObj.objName];
    if (obj) {
      // Decode & populate the received object only when
      // arrayEditAllowed is false or validation succeeded.
      // Otherwise, when validation failure happens in editable table page
      // page object is filled with valid fields from the received object and
      // currently entered contents are gone. If pressing Save key again then
      // sendObject sends without current editing content so it ends with success
      // but current editing contents is not written to RDB.
      if (!(pgeObj.arrayEditAllowed && validateFailed)) {
        if (isDefined(pgeObj.decodeRdb)) {
          obj = pgeObj.decodeRdb(obj);
        }

        // Mark as a saved object for editable object
        if (isDefined(pgeObj.editMembers)) {
            obj.forEach(function(obj){
                obj.__saved = true; // mark as saved element
            });
        }

        pgeObj.obj = obj;
        if (!validateFailed) {
            pgeObj.populate();
        }
      }
      if (isDefined(pgeObj.setVisible)) {
        pgeObj.setVisible();
      }
    }
  });
}

function isFormValid(form?: any) {
  if (!isDefined(form)) {
    form = "#form";
  }
#ifdef V_WEBIF_SPEC_vdf
  return $(form).valid();
#else
  if(!($(form)as any).validationEngine("validate")) {
    validate_alert("","");
    return false;
  }
  return true;
#endif
}

type SendCallback = any; // TODO use a better type like (err?: any) => void;

function sendObjects(fnSuccess?: SendCallback, fnFail?: SendCallback) {
  if (isFormValid()) {
    var objs = getObjectsToSend();
    sendTheseObjects(objs, fnSuccess, fnFail);
  }
}

var cgiUrl = '/cgi-bin/jsonSrvr.lua';

declare var queryParams: object; // this is set in the first <script> section of each html page

function sendTheseObjects(objs, fnSuccess?: SendCallback, fnFail?: SendCallback) {
  $("button").prop("disabled", true);
#ifdef V_WEBIF_SERVER_turbo
  $.post(cgiUrl, "data=" + encodeURIComponent(JSON.stringify({setObject: objs,
                                           csrfToken: csrfToken,
                                           queryParams: queryParams})), null, "json")
#else
  $.post(cgiUrl, JSON.stringify({setObject: objs, csrfToken: csrfToken}), null, "json")
#endif
  .done(function(resp) {
      receiveObjects(resp);
      // page is reloaded
      if (isDefined(pageData.onDataUpdate)) {
        pageData.onDataUpdate(resp);
      }
      if (resp.result === 0) {
        var success = "";
        if ((isDefined(pageData.suppressAlertTxt) && !pageData.suppressAlertTxt) ||
             !isDefined(pageData.suppressAlertTxt)) {
          if (isDefined(pageData.alertSuccessTxt)) {
          success = isFunction(pageData.alertSuccessTxt) ? _(pageData.alertSuccessTxt()): _(pageData.alertSuccessTxt);
          }
          success_alert("", success);
        }
        if (isFunction(fnSuccess)) {
          fnSuccess();
        }
      }
      $("button").prop("disabled", false);
  })
  .fail(function(err) {
    if (isFunction(fnFail)) {
      fnFail(err);
    }
    alertInvalidRequest();
    $("button").prop("disabled", false);
  });
}

// Send single page objects
// @param pageObj page object to send
// @return none
function sendSingleObject(pgeObj, fnSuccess?: SendCallback, fnFail?: SendCallback) {
  sendTheseObjects([pgeObj.packageObj()], fnSuccess, fnFail);
}

// Request the objects and call the supplied function when complete
function requestObjects(objectstoRequest, fnCallback) {
#ifdef V_WEBIF_SERVER_turbo
  $.getJSON(cgiUrl, "req=" + JSON.stringify({getObject: objectstoRequest,
                                             csrfToken: csrfToken,
                                             queryParams: queryParams}))
#else
  $.getJSON(cgiUrl, JSON.stringify({getObject: objectstoRequest, csrfToken: csrfToken}))
#endif
    .done(fnCallback)
    .fail(function(err) {
        setVisible("#body","1");
    });
}

var pollIntervals = [];

// Stop all previously started page polls
function stopPagePolls() {
  for (const interval of pollIntervals) {
    clearInterval(interval);
  }
  pollIntervals = [];
}

// This function starts the timers to request objects from the device.
// As an optimisation all equal period polls are done together
function startPagePolls() {
  // clear all previous interval timers
  stopPagePolls();
  // This array is indexed by period and each element is an array of object names
  var pollPeriods = [];
  pageData.pageObjects.forEach(function(pgeObj) {
    if (isDefined(pgeObj.pollPeriod)) {
      var period = pgeObj.pollPeriod;
      if (period == 0) {
        return;
      }
      if (!isDefined(pollPeriods[period]))
        pollPeriods[period] = [];
      var objNames = pollPeriods[period];     // Get the array of object names
      objNames[objNames.length] = pgeObj.objName; // Add another object
    }
  });
  pollPeriods.forEach(function(objNames, period){
    pollIntervals.push(setInterval(function() {requestObjects(objNames, receiveObjects);}, period));
  });
}

var pageGenerated = false;
var objectsReceived = null;

// This function is called after the objects(data) have been received.
// for the first time.
// This function is called either from ready() or from the callback, whichever
// occurs last.
// If any of the objects as a poll period specified timers are started
function receiveFirstObjects(resp){

  if (pageGenerated === false) { // We can't do anything yet
    objectsReceived = resp;   // just save the response and ready() will call here
    return;
  }

// With appweb the authentication has been moved to the html load as ESP processing
// So this code is no longer required, we'll keep it here for the moment in case it
// is again needed.
#if 0
  // Redirect browser to login if required
  if ((isDefined(resp.access) && (resp.access === "NeedsAuthentication")) ||
   (isDefined(pageData.rootOnly) && (pageData.rootOnly === true) && (userlevel != '0'))) {
    window.location = "/index.html?src=/" + relUrlOfPage;
    return;
  }
#endif

  if (pageData.authenticatedOnly ?? true) {
    const readGroups = pageData.readGroups ?? ["root", "admin"];
    if (!checkGroupAccess(readGroups, userGroups)) {
      console.log("user group does not have read privilege, redirect to index page");
      window.location.href = "/index.html";
      return;
    }
  }

  receiveObjects(resp);
  if (isDefined(pageData.onReady)) {
    pageData.onReady(resp);
  }
  setVisible("#body","1");

  if (pageData.validateOnload ?? true ) {
#ifdef V_WEBIF_SPEC_vdf
    $("#form").valid();
  }
#else
    ($("#form")as any).validationEngine();
  }
  $.blockUI.defaults.css.border="3px solid #008bc6";
#endif
  $.blockUI.defaults.css.padding="20px 0 20px 0";
  startPagePolls();
}

// This is the JQuery function that is called when the page has loaded
// At this point the page is minimal. Only style sheets and JS sources have been loaded.
// There is no html in the <body>
$(document).ready(function() {
  genPage();
  if (isDefined(pageData.showImmediate)) setVisible("#body","1");
  pageGenerated = true;
  if (objectsReceived !== null) {
      receiveFirstObjects(objectsReceived);
      objectsReceived = null;
  }

  if (isDefined(pageData.onSubmit)) {
    $("#form").submit((event)=>{
      return pageData.onSubmit(event);
    });
  }

});

// This is a callback function which is called when the client
// receives JSONP object from the server after sending web_server_status
// request as a JSONP.
var loginStatus;
var webserverStatus;
function statusJsonpParser(res) {
  loginStatus = res.data.loginStatus;
  webserverStatus = res.data.status;
}

// Wait until rebooting complete
// Based on previous booting time, calculate estimated booting time then
// wait until the web server is down.
// Next step is waiting the web server is up after rebooting comlete.
// If the web server is not responsive until timeout then display error message.
// If sucessfully rebooted then display sucess message for 5 seconds and
// redirect to index page.
// @param estRebootingSecs  Estimated rebooting time
// @param defLanIpAddr  Default LAN IP address to which will be redirected
//                      after rebooting. It includes protocol field as well.
//                      ex. http://192.168.3.1
function waitUntilRebootComplete(estRebootingSecs, defLanIpAddr='') {
  var counter = 0;
  var redirect_counter = 0;
  var estTime = estRebootingSecs;
  var numPolls = estTime;     // 1s polling interval
  var expectedStatus = "N/A"  // expected web server status
                              // phase 1 : N/A : waiting for power down
                              // phase 2 : running : waiting for power up
  var indexUrl = "/index.html";
  var statusUrl = "/web_server_status";
  var statusPollUrl;
  console.log("defLanIpAddr = "+defLanIpAddr);
  if (defLanIpAddr != '') {
    // New redirect address on default LAN IP address
    // Use http protocol for safe
    indexUrl = defLanIpAddr + indexUrl;
    // New status check url on default LAN IP address
    statusUrl = defLanIpAddr + statusUrl;
  }
  $("#spinner").show();
  var intv = setInterval(function() {
    function stopPolling() {
      clearInterval(intv);
      $.unblockUI();
      $("button").prop("disabled", false);
    }
    numPolls--;
    counter++;
    // counter increases by 1 per seconds so the percentage formula is
    // percent = 100 * counter / esttime
    var percentage = Math.floor(100*counter/estTime);
    if (numPolls <= 0 || percentage >= 100) {
      console.log("numPolls = "+numPolls+", percentage = "+percentage+", estTime = "+estTime);
      stopPolling();
      $("#rebootMsg").text("The reboot seems to be taking too long. You may need to manually power cycle your device.");
      $("#rebootCt").text("");
      $("#spinner").hide();
      blockUI_alert("The reboot seems to be taking too long. You may need to manually power cycle your device.", function(){$(location).attr('href', indexUrl);});
      return;
    } else {
      if (redirect_counter > 0) {
        $("#rebootCt").text("100 %");
      } else {
        $("#rebootCt").text(percentage + " %");
      }
    }

    // Send status response as JSONP in order to avoid
    // CORS(Cross Origin Resource Sharing) issue which happens
    // when current WEBUI domain address is different from the default
    // address and browser is trying to get status response on
    // the default address during after factory reset.
    // Ex) Set LAN IP to 192.168.4.1 while default LAN IP is 192.168.1.1
    //     then start factory reset.
    //
    // statusJsonpParser is the callback function name which is called
    // when JSONP object is received from the server.
    statusPollUrl = statusUrl + "?callback=statusJsonpParser";
    $.getScript( statusPollUrl).fail(function() { webserverStatus = 'N/A'; });
    console.log("expectedStatus = "+expectedStatus+", webserverStatus = "+webserverStatus);

    // Phase 1 : waiting for web server down
    if (expectedStatus == "N/A" && expectedStatus == webserverStatus) {
      console.log("detected web server power down, waiting for rebooting now");
      expectedStatus = "running";
    }
    // Phase 2 : waiting for web server up again after rebooting
    //           If default LAN IP address is different from current address then
    //           we can't get web server status after factory reset so redirect to
    //           index page on default address after reboot.
    if ((expectedStatus == "running" && expectedStatus == webserverStatus) ||
        (defLanIpAddr != '' && percentage > 80)) {
      // wait 3 seconds before redirect to index page
      // in order to show redirect message to user
      if (redirect_counter == 0) {
        console.log("numPolls = "+numPolls+", percentage = "+percentage+", estTime = "+estTime);
        console.log("detected web server power up, redirect to login page after 3 seconds");
        $("#rebootMsg").text("Reboot successful. Redirecting you to the login page...");
        $("#rebootCt").text("100 %");
      } else if (redirect_counter > 3) {
        stopPolling();
        console.log("redirect to " + indexUrl);
        // Not rely on web sever to redirect anymore so don't need to send logout post with
        // redirecting url address.
        $(location).attr('href', indexUrl);
        return;
      }
      redirect_counter++;
    }
    // reset webserverStatus before next poll
    webserverStatus = 'N/A';
  }, 1000); // 1s
}

function genRebootWaitingMsgHtml() {
  return '<div id="rebootMsg" class = "rebootMsg">Rebooting...</div>' +
         '<div id="spinner" class="rebootImgDiv"><img src="/img/spinner_250.gif" class="rebootIcon">' +
         '<b id="rebootCt" class="rebootCnt">0 %</b></div>';
}
// Send RDB command and wait for result
// Legacy RDB command consists of below variables
//    service.cmd.command   - get or set
//    service.cmd.param.xxx - command data
//    service.cmd.status    - [done] or [error]
//
// ex) wwan.0.sim.cmd.command
//     wwan.0.sim.cmd.param.pin
//     wwan.0.sim.cmd.param.newpin
//     wwan.0.sim.cmd.param.puk
//     wwan.0.currentband.cmd.command
//     wwan.0.currentband.cmd.param.band
// @param rdbPrefix A rdb prefix of the command. 'wwan.0.sim' in above example
// @param cmd A command to be sent via RDB command
// @param param A parameter set to be sent via RDB command
//              set command : Contains parameter name and values for set command
//                            Returned param may have no meaning
//              get command : Contains parameter name and values for get command
//                            to expect to get
//              This parameter will be used as below example;
//                  ex) for band set command, if [param] is;
//                          {"band":"LTE all"}
//                      then it will be used to set below RDB variable
//                          wwan.0.currentband.cmd.param.band "LTE ALL"
//                                                        ^       ^
//                                                        |       |
//                                                      name    value
// @param timeout Timeout value for the command
// @param csrfToken A CSRF token
// @param fnSuccess A function to be run on success
//
function sendRdbCommand(rdbPrefix, cmd, param, timeout, csrfToken, fnSuccess) {
  var counter = 0;
  var redirect_counter = 0;
  var numPolls = timeout;     // 1 s polling interval
  var cmdStatus = "";
  var cmdParam = {};
  var paramJson = JSON.stringify(param);
  blockUI_wait(_("GUI pleaseWait"));
  // Send a RDB command
  $.post("rdb_command", {rdbPrefix:rdbPrefix, cmd:cmd, param:paramJson, csrfToken: csrfToken},
    function(data, status) {
      // Wait until get the command result
      var intv = setInterval(function() {
        function stopPolling() {
          clearInterval(intv);
        }
        numPolls--;
        if (numPolls <= 0) {
          stopPolling();
          blockUI_alert("This operation seems to be taking too long. You may need to try again.", function(){location.reload();});
        }
        $.getJSON( "rdb_command", {rdbPrefix:rdbPrefix, param:paramJson},
          function(res){
            cmdStatus = res.data.status;
            cmdParam = res.data.param;
            console.log("cmdStatus = "+cmdStatus);
            console.log("cmdParam = "+cmdParam);
            if (cmdStatus == "[done]" || cmdStatus == "[error]") {
              stopPolling();
              if (cmdStatus == "[done]") {
                console.log("successfully executed the RDB command");
                // Note: fnSuccess should call unblockUI
                fnSuccess(cmdParam);
              } else {
                console.log("RDB command failed");
                $.unblockUI();
                blockUI_alert("The operation failed. You may need to try again.", function(){location.reload();});
              }
            }
          }
        );
      }, 1000); // 1s
    }
  );
}

// Save current settings into file or restore settings from saved file
// @param mode [save|restore]
// @param password A encrypted password to be used for encryption/decription
// @param csrfToken A CSRF token
// @param fnSuccess A callback function to be run on sucess
function saveOrRestoreSettings(mode:string, password:string, csrfToken:string, fnSuccess?: any) {
  blockUI_wait(_("GUI pleaseWait"));
  $.post("settings_backup/"+mode, {password:password, csrfToken: csrfToken},
    function(res, status) {
      if (res.result== "0") {
        console.log("res.data = "+res.data);
        if (mode == "save") {
          console.log("res.data.filename = "+res.data.filename);
          location.href = res.data.filename;
          $.unblockUI();
        } else if (mode == "restore" && isDefined(fnSuccess)) {
          fnSuccess(res.data);
        }
      } else {
        $.unblockUI();
        blockUI_alert("The operation failed. You may need to try again.");
      };
    }
  )
  .fail(function() {
    $.unblockUI();
    blockUI_alert("The operation failed. You may need to try again.");
  });
}
