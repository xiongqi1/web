//This is international translation function
// param str is the phrase to be translated
// retval is the translated string or str if no tranlation is possible
function _(str){
    if (isDefined(xlat)){
        var newStr=xlat[str];
        if(newStr) {
            newStr =  newStr.replace(/&#x2F;/g, '/');
            newStr =  newStr.replace(/&amp;/g, '&');
            return newStr;
        }
    }
    return str;
}

function encode(data) {return Base64.encode(data);}
function decode(data) {return Base64.decode(data);}

function toBool(b) {
    if (typeof b === "string")
        return b !== '0' && b !== '';
    if (typeof b === "number")
        return b !== 0;
    if (typeof b === "boolean")
        return b;
    return false;
}

function validateEmail(email) {
    // As can be seen from https://stackoverflow.com/questions/46155/how-to-validate-email-address-in-javascript
    // proper email validation is not trivial
    // The following commented lines where the best answer there
/*
    var re = /^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
    return re.test(email);
*/
    // but we'll go even simpler and just look for a @
    return email.indexOf("@") > 0;
}

// poll a URL periodically
// @param url The URL to poll
// @param timeout The timeout in milliseconds for each poll
// @param period The duration in milliseconds between two consecutive polls
// @param fnSuccess The callback function for successful poll
// @param fnError The callback function for failed poll
// @return A value that can be used in clearInterval to cancel the periodic poll
// Note) There is a CORS issue when fw upgrading with factory reset option.
//       To solve this, JSONP should be used so "script" data type is defined here.
function pollUrl(url: string, timeout: number, period: number, fnSuccess?: any, fnError?: any) {
    return setInterval(() => {
        $.ajax({
            url: url,
            dataType: "script",
            timeout: timeout,
            success: fnSuccess,
            error: fnError
        });
    }, period);
}

// Download given text and save to a file
// @params text A text to save to the file
// @params name A file name
// @params type A text type to be saved
function downloadToFile(text, name, type) {
  // A workaround to restore button property after clicking
  // because visited pseudo class does not work as expected
  setEnable("downloadButton", false);
  var a = <HTMLAnchorElement>document.getElementById("fileDownload");
  var file = new Blob([text], {type: type});
  a.href = URL.createObjectURL(file);
  a.download = name;
  // A workaround to restore button property after clicking
  // because visited pseudo class does not work as expected
  setEnable("downloadButton", true);
}

// Download a url to file
// @params url
// @params name A file name
function downloadUrl(url, name) {
    // A workaround to restore button property after clicking
    // because visited pseudo class does not work as expected
    setEnable("downloadButton", false);
    var a = <HTMLAnchorElement>document.getElementById("fileDownload");
    a.href = url;
    a.download = name;
    // A workaround to restore button property after clicking
    // because visited pseudo class does not work as expected
    setEnable("downloadButton", true);
}


function clearLogFiles(csrfToken: string) {
  // A workaround to restore button property after clicking
  // because visited pseudo class does not work as expected
  setEnable("clearButton", false);
  $.post("clear_log", {csrfToken: csrfToken})
    .done(function(resp) {
      success_alert("","Logs were cleared successfully.");
    })
    .fail(function(err) {
      alertInvalidRequest();
    });
  // A workaround to restore button property after clicking
  // because visited pseudo class does not work as expected
  setEnable("clearButton", true);
}

// Get server certificate information
// @param succFunc A callback function for success event
function getServerCertiInfo(succFunc: any) {
    var url="server_certi/info";
    var result;
    $.getJSON( url, {}, succFunc
    ).fail(function() { console.log("failed to get server certificate info")});
}

// Generate server certificate
// @param succFunc A callback function for success event
// @param failFunc A callback function for failure event
function generateServerCa(succFunc: any, failFunc: any) {
    var url1, url2, errMsg;
    var formData = new FormData();
    formData.append("csrfTokenPost",csrfToken);
    url1 = "server_certi/gen_ca";
    url2 = "server_certi/info";
    errMsg = _("ca fail");

    // send a command to generate server certificate
    blockUI_wait(_("GUI pleaseWait"));
    $.ajax({
        xhr: function() {
            var xhr = new XMLHttpRequest();
            return xhr;
        },
        url: url1,
        data: formData,
        processData: false,
        contentType: false,
        type: 'post',
        success: (respObj) => {
            if (respObj.result == "0") {
                $.getJSON(url2, {}, function(res) {
                    if (res.result == "0") {
                        $.unblockUI();
                        succFunc(res);
                    }
                }).fail(function() {$.unblockUI(); failFunc();});
            }
            else {
                $.unblockUI();
                blockUI_alert(errMsg);
            }
        },
        error: function(XMLHttpRequest, textStatus, errorThrown) {
            $.unblockUI();
            failFunc();
        }
    });
}

// Check if access is allowed for user groups
// @param allowedGroups Groups that have access
// @param userGroups Groups of the current user
function checkGroupAccess(allowedGroups: string[], userGroups: string[]) {
    for (const userGroup of userGroups) {
        if (allowedGroups.indexOf(userGroup) >= 0) {
            return true;
        }
    }
    return false;
}
