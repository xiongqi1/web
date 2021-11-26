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
    //103 has 2 messages
    this.cbrsErrorMessages["103_cpiId"] = "Please verify Installer Credentials";
    this.cbrsErrorMessages["103"] = "Replace OA";
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
    this.requestErrCodeStrDefault = "Internal error";
    //guideline
    this.requestErrGuideline = {};
    this.requestErrGuideline["No SAS response"] = "Check M&P Manual for Steps";
    this.requestErrGuideline["SAS handshake failed"] = "Replace OA";
    this.requestErrGuideline["Internal error"] = "Replace OA";

    this.generalErrMsgElId = "general_error_message";
    this.generalErrGuidelineElId = "general_error_guideline";
    this.sasRegStateElId = "sas_reg_state";
    this.sasErrCodeElId = "sas_err_code";
    this.sasErrMsgElId = "sas_err_msg";
    this.cellIdElId = "scell_id";
    this.regStatusTitleId = "reg_status_title";

    this.okForSasRegister = false;
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
    var regStatusTitleIdEl = document.getElementById(this.regStatusTitleId);

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
            if (sasErrCodeEl.textContent != errorCodeStr) {
                sasErrCodeEl.textContent = errorCodeStr;
                sasErrCodeEl.setAttribute("style", "display:");
            }
            if (data.err_code == "103") {
                if (data.err_data == "cpiSignatureData") {
                    data.err_code = "103_cpiId";
                }
            } else if (data.err_code == "104") {
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

    if (regStatusTitleIdEl) {
        if (data.cbrs_band_mode == "1") {
            regStatusTitleIdEl.innerText = "CBRS Registration Status";
        } else {
            regStatusTitleIdEl.innerText = "Registration Status";
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
