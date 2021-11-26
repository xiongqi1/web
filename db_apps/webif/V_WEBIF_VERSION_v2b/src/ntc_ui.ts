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
    objectsToRequest[objectsToRequest.length] = pgeObj.objName;
  });
  requestObjects(objectsToRequest,receiveFirstObjects);
}

// return an array of page objects the need to be saved
function getObjectsToSend() {
  var objs=[];
  pageData.pageObjects.forEach(function(pgeObj) {
    var obj = pgeObj.packageObj();
    if (obj) objs[objs.length] = obj;
  });
  return objs;
}

// Scan the page objects to see and check if there is a received object
// decodeRdb is called if defined to massage RDB data
// into a friendlier format - See gps.js for a good example
function receiveObjects(resp){

  if (resp.result !== 0) {
    if (resp.text.indexOf("Invalid csrfToken") >= 0) {
      alertInvalidCsrfToken();
    }
    else if (isDefined(resp.errorText)){
      validate_alert( _("invalidRequestTitle"), _(resp.errorText));
    }
    else {
      alertInvalidRequest();
    }
  }

  pageData.pageObjects.forEach(function(pgeObj) {
    var obj = resp[pgeObj.objName];
    if (obj) {
      if (isDefined(pgeObj.decodeRdb)) {
        obj = pgeObj.decodeRdb(obj);
      }
      pgeObj.obj = obj;
      pgeObj.populate();
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

var cgiUrl = '/cgi-bin/jsonSrvr.lua'

function sendTheseObjects(objs, fnSuccess?: SendCallback, fnFail?: SendCallback) {
  $("button").prop("disabled", true);
#ifdef V_WEBIF_SERVER_turbo
  $.post(cgiUrl, "data=" + JSON.stringify({setObject: objs, csrfToken: csrfToken}), null, "json")
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

// Request the objects and call the supplied function when complete
function requestObjects(objectstoRequest, fnCallback) {
#ifdef V_WEBIF_SERVER_turbo
  $.getJSON(cgiUrl, "req=" + JSON.stringify({getObject: objectstoRequest, csrfToken: csrfToken}))
#else
  $.getJSON(cgiUrl, JSON.stringify({getObject: objectstoRequest, csrfToken: csrfToken}))
#endif
    .done(fnCallback)
    .fail(function(err) {
        setVisible("#body","1");
    });
}

// This function starts the timers to request objects from the device.
// As an optimisation all equal period polls are done together
var intvs = [];
function startPagePolls() {
  // clear all previous interval timers
  intvs.forEach(function(intv) {
    clearInterval(intv);
  });
  intvs = [];
  // This array is indexed by period and each element is an array of object names
  var pollPeriods = [];
  pageData.pageObjects.forEach(function(pgeObj) {
    if (isDefined(pgeObj.pollPeriod)) {
      var period = pgeObj.pollPeriod;
      if (!isDefined(pollPeriods[period]) || period == 0)
        pollPeriods[period] = [];
      var objNames = pollPeriods[period];     // Get the array of object names
      objNames[objNames.length] = pgeObj.objName; // Add another object
    }
  });
  var idx = 0;
  pollPeriods.forEach(function(objNames, period){
    if (period > 0) {
      intvs[idx++] = setInterval(function() {requestObjects(objNames, receiveObjects);}, period);
    }
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

  receiveObjects(resp);
  if (isDefined(pageData.onReady)) {
    pageData.onReady(resp);
  }
  setVisible("#body","1");

  if (!isDefined(pageData.validateOnload) || pageData.validateOnload === true ) {
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
