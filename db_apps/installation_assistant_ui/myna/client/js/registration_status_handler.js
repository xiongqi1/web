/*
Copyright (C) 2019 NetComm Wireless Limited.

Class processing network registration status and CBRS status and error state
*/

/*
 * Constructor
 */
function RegistrationStatusHandler() {
    this.cbrsErrorMessages = {};
    this.cbrsErrorMessages["100"] = this.cbrsErrorMessages["101"] = this.cbrsErrorMessages["102"] = "Replace OA";
    this.cbrsErrorMessages["103"] = "One or more parameters have invalid value, try correcting them.";
    //104 has 2 messages
    this.cbrsErrorMessages["104_cpiId"] = "Update or re-validate installer CPI certificate";
    this.cbrsErrorMessages["104"] = "Replace OA";

    this.cbrsErrorMessages["105"] = this.cbrsErrorMessages["200"] = this.cbrsErrorMessages["401"]
        = this.cbrsErrorMessages["502"] = "Power-Cycle OA";
    this.cbrsErrorMessages["202"] = "Replace OA";

    this.cbrsErrorMessages["400"] = this.cbrsErrorMessages["500"] = "Attempt install on alternate cell";

    this.cbrsErrorMessages["501"] = "Failure code 501, Cell has suspended grant, try pointing to other available cells if any.";

    this.sasHandshakeFailMsg = this.simCardError = "Replace OA";

    this.noSasRespMsg = "Check M&P Manual for Steps";
    this.lteLimitedServiceMsg = "Aim antenna and Register";

    /*
     * these error codes come from turbo async.errors
     * --- Errors that can be set in the return object of fetch (HTTPResponse instance).
     * local errors = {
     * INVALID_URL = -1 -- URL could not be parsed.
     * ,INVALID_SCHEMA = -2 -- Invalid URL schema
     * ,COULD_NOT_CONNECT = -3 -- Could not connect, check message.
     * ,PARSE_ERROR_HEADERS = -4 -- Could not parse response headers.
     * ,CONNECT_TIMEOUT = -5 -- Connect timed out.
     * ,REQUEST_TIMEOUT = -6 -- Request timed out.
     * ,NO_HEADERS = -7 -- Shouldnt happen.
     * ,REQUIRES_BODY = -8 -- Expected a HTTP body, but none set.
     * ,INVALID_BODY = -9 -- Request body is not a string.
     * ,SOCKET_ERROR = -10 -- Socket error, check message.
     * ,SSL_ERROR = -11 -- SSL error, check message.
     * ,BUSY = -12 -- Operation in progress.
     * ,REDIRECT_MAX = -13 -- Redirect maximum reached.
     * ,SSL_VERIFY_ERROR = -14 -- SSL verification has failed.
     * }
     * The requirement specifies reporting only "No SAS Response" and "SAS Handshake Fail".
     * Hence map error codes to those 2 error names.
     */
    this.requestErrCodeStr = {};
    this.requestErrCodeStr["-3"] = this.requestErrCodeStr["-4"] = this.requestErrCodeStr["-5"]
        =  this.requestErrCodeStr["-6"] =  this.requestErrCodeStr["-7"]
        =  this.requestErrCodeStr["-8"] =  this.requestErrCodeStr["-9"]
        =  this.requestErrCodeStr["-13"] = "No SAS response";
    this.requestErrCodeStr["-11"] = "SAS handshake failed";
    this.requestErrCodeStr["-14"] = "SSL verification failed";
    this.requestErrCodeStr["-99"] = "Network Reject";
    this.requestErrCodeStrDefault = "Internal error";
    this.requestErrCodeStr["-1"] = this.requestErrCodeStr["-2"] = this.requestErrCodeStr["-10"]
        = this.requestErrCodeStr["-12"] = this.requestErrCodeStrDefault;
    for(var index in this.requestErrCodeStr) {
        this.requestErrCodeStrDefault[index] += "(" + index + ")";
    }

    //guideline
    this.requestErrGuideline = {};
    this.requestErrGuideline["SAS Cannot be Authenticated"] =
    this.requestErrGuideline["No SAS response"] = "Check M&P Manual for Steps";
    this.requestErrGuideline["SAS handshake failed"] = "Replace OA";
    this.requestErrGuideline["Network Reject"] = "Power-Cycle OA";
    this.requestErrGuideline["Internal error"] = "Replace OA";

    this.generalErrMsgElId = "general_error_message";
    this.generalErrGuidelineElId = "general_error_guideline";
    this.sasRegStateElId = "sas_reg_state";
    this.sasErrCodeElId = "sas_err_code";
    this.sasErrMsgElId = "sas_err_msg";
    this.cellIdElId = "scell_id";
    this.modemTxStatusElId = "modem_tx_status";

    this.okForSasRegister = false;

    var self = this;
    ajaxGetRequest("/reg_method", true, function(data) {
        if(data && data.reg_method_selected == "multi" ) {
            self.cbrsErrorMessages["200"] = "Registration pending";
        }
    });
}

/*
 * Handle status data
 * @param data Status data
 */
RegistrationStatusHandler.prototype.handleData = function(data) {
    var generalErrMsgEl = document.getElementById(this.generalErrMsgElId);
    var generalErrGuidelineEl = document.getElementById(this.generalErrGuidelineElId);
    var sasRegStateEl = document.getElementById(this.sasRegStateElId);
    var sasErrCodeEl = document.getElementById(this.sasErrCodeElId);
    var sasErrMsgEl = document.getElementById(this.sasErrMsgElId);
    var cellIdEl = document.getElementById(this.cellIdElId);
    var modemTxStatusEl = document.getElementById(this.modemTxStatusElId);

    this.okForSasRegister = data.sim_status == "SIM OK";

    if (generalErrMsgEl && generalErrGuidelineEl) {
        if (data.sim_status != "SIM OK") {
            var msg = "SIM card error: " + data.sim_status;
            var guideline = "Guideline: " + this.simCardError;
            if (generalErrMsgEl.textContent != msg || generalErrGuidelineEl.textContent != guideline) {
                generalErrMsgEl.textContent = msg;
                generalErrMsgEl.style["display"] = "";
                generalErrGuidelineEl.textContent = guideline;
                generalErrGuidelineEl.style["display"] = "";
            }
        } else if (data.network_status_system_mode != "lte") {
            var msg = "LTE service status: " + data.network_status_system_mode;
            generalErrMsgEl.style["display"] = "none";
            var guideline = "Guideline: " + this.lteLimitedServiceMsg;
            if (generalErrGuidelineEl.textContent != guideline) {
                generalErrGuidelineEl.textContent = guideline;
                generalErrGuidelineEl.style["display"] = "";
            }
        } else {
            generalErrMsgEl.textContent = "";
            generalErrGuidelineEl.textContent = "";
            generalErrMsgEl.style["display"] = "none";
            generalErrGuidelineEl.style["display"] = "none"
        }
    }

    if (sasRegStateEl && sasRegStateEl.textContent != data.reg_state) {
        sasRegStateEl.textContent = data.reg_state;
        if (data.cbrs_band_mode == "1") {
            if (data.reg_state == "Registered – Grant Authorized") {
                sasRegStateEl.style["color"] = "green";
            } else if (data.reg_state == "Registered – Grant Pending" || data.reg_state == "Registering"
                    || data.reg_state == "Attaching LTE" || data.reg_state == "Registered – Attaching LTE") {
                sasRegStateEl.style["color"] = "orangered";
            } else {
                sasRegStateEl.style["color"] = "red";
            }
        }
    }

    if (data.cbrs_band_mode == "1" && data.reg_state != "Registered – Grant Authorized"
            && sasErrMsgEl && sasErrCodeEl) {
        if (data.err_code != "" && this.cbrsErrorMessages.hasOwnProperty(data.err_code)) {
            var errorCodeStr = "Error code: " + data.err_code;
            if (data.err_code == "103") {
              var msg = data.err_msg;
              if (data.err_data != "") {
                msg += (msg != "" ? ", " : "") + data.err_data;
              }
              if (msg != "") {
                errorCodeStr += ", " + msg.replace(".,",",");
              }
            }
            if (sasErrCodeEl.textContent != errorCodeStr) {
                sasErrCodeEl.textContent = errorCodeStr;
                sasErrCodeEl.setAttribute("style", "display:");
            }
            if (data.err_code == "104") {
                if (data.cpi_id != "" && data.err_data != "" && data.cpi_id == data.err_data) {
                    data.err_code = "104_cpiId";
                }
            }
            var msg = "Guideline: " + this.cbrsErrorMessages[data.err_code];
            if (sasErrMsgEl.textContent != msg) {
                sasErrMsgEl.textContent = msg;
                sasErrMsgEl.setAttribute("style", "display:");
            }
        } else if (data.request_error_code != "") {
            var errorCodeStr;
            if (this.requestErrCodeStr.hasOwnProperty(data.request_error_code)) {
                errorCodeStr = this.requestErrCodeStr[data.request_error_code];
            } else {
                errorCodeStr = this.requestErrCodeStrDefault;
            }
            var msg = "Guideline: " + this.requestErrGuideline[errorCodeStr];
            var errorCodeStr = "Error: " + errorCodeStr;
            if (sasErrCodeEl.textContent != errorCodeStr || sasErrMsgEl.textContent != msg) {
                sasErrCodeEl.textContent = errorCodeStr;
                sasErrCodeEl.setAttribute("style", "display:");
                sasErrMsgEl.textContent = msg;
                sasErrMsgEl.setAttribute("style", "display:");
            }
        } else {
            sasErrCodeEl.textContent = "";
            sasErrMsgEl.textContent = "";
            sasErrCodeEl.setAttribute("style", "display:none");
            sasErrMsgEl.setAttribute("style", "display:none");
        }
    } else if (sasErrMsgEl && sasErrCodeEl) {
        sasErrCodeEl.textContent = "";
        sasErrMsgEl.textContent = "";
        sasErrCodeEl.setAttribute("style", "display:none");
        sasErrMsgEl.setAttribute("style", "display:none");
    }

    if (cellIdEl && cellIdEl.textContent != data.scell_id) {
        cellIdEl.textContent = data.scell_id;
    }

    var show_modem_status_row = false;
    if (modemTxStatusEl && data.hasOwnProperty("modem_tx_status")) {
        if (data.modem_tx_status == "1") {
            modemTxStatusEl.textContent = "On";
            show_modem_status_row = false;
        }
        else if(data.modem_next_tx_time && data.modem_tx_update_time &&
            data.nit_tx_update_time && data.nit_time) {
            var modem_next_tx_time = new Date(data.modem_next_tx_time);
            var modem_tx_update_time = new Date(data.modem_tx_update_time);
            var nit_tx_update_time = new Date(data.nit_tx_update_time);
            var nit_time = new Date(data.nit_time);

            var owa_nit_time_diff = modem_tx_update_time - nit_tx_update_time;

            var seconds_left = parseInt((modem_next_tx_time.getTime() - owa_nit_time_diff - nit_time.getTime())/1000) + 2;
            if (seconds_left<1) {
                seconds_left = 1;
            }

            modemTxStatusEl.innerHTML = "Unauthorized Transmit Time exceeded</br>Next attempt in " + seconds_left + " second(s)";
            show_modem_status_row = true;
        }
        else {
            modemTxStatusEl.textContent = "Off";
            show_modem_status_row = true;
        }

        var modemTxStatusRowEl = modemTxStatusEl.parentNode.parentNode;
        if(show_modem_status_row) {
            modemTxStatusRowEl.style.display = "block";
        }
        else {
            modemTxStatusRowEl.style.display = "none";
        }
    }
}

/*
 * Is current state ok to do SAS Register?
 * @retval true/false
 */
RegistrationStatusHandler.prototype.isOkForSasRegister = function() {
    return this.okForSasRegister;
}
