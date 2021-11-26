/*
Copyright (C) 2019 NetComm Wireless Limited.

Handle QR code image generation and redirection
*/

/*
 * format a positive integer number to string with 2 digits e.g 1 --> "01"
 * @param number input positive integer number
 * @return formatted string
 */
function format2Digits(number) {
    return number < 10 ? ("0" + number.toString()) : number.toString();
}

/*
 * return current date-time string in YYYYMMDDHHMMSS format
 * @return date-time string in YYYYMMDDHHMMSS format
 */
function getQrDateString() {
    qrCodeDate = new Date();
    var dateTimeStr = qrCodeDate.getFullYear().toString()
        + format2Digits(qrCodeDate.getMonth() + 1)
        + format2Digits(qrCodeDate.getDate())
        + format2Digits(qrCodeDate.getHours())
        + format2Digits(qrCodeDate.getMinutes())
        + format2Digits(qrCodeDate.getSeconds());
    return dateTimeStr;
}

// generated QR code image name
var qrCodeImageName;
// QR code generating checking timer
var qrCodeCheckingTimer;
var qrCodeGenerating;
var qrCodeDate;

/*
 * call back function to process response of successful generating QR code
 */
function successQrCode(resp) {
    qrCodeImageName = resp;
    var re = /^(.+)_\d+\.png$/;
    var reQrImgRes = re.exec(qrCodeImageName);
    if (reQrImgRes !== null) {
        document.getElementById("ban_val").textContent = reQrImgRes[1];
    }
    document.getElementById("date_time_val").textContent = qrCodeDate.toString();
    var qrImg = document.createElement('img');
    qrImg.src = "/qr_code/" + qrCodeImageName;
    document.getElementById("qrcode").appendChild(qrImg);
    qrImg.onload = function() {
        qrCodeGenerating = false;
    }
}

/*
 * call back function to process response of failure in generating QR code
 */
function failedQrCode(code, errorLog) {
    qrCodeGenerating = false;
    clearInterval(qrCodeCheckingTimer);
    var generatingEl = document.getElementById("generating_qr_code");
    if (generatingEl) {
        generatingEl.setAttribute("style", "display:none");
    }
    var statusEl = document.getElementById("qr_code_status");
    var statusTextEl = document.getElementById("qr_code_status_text");
    var errorLogTxt = document.getElementById("qr_code_error_log");
    if (statusEl && statusTextEl && errorLogTxt) {
        statusTextEl.textContent = "Failed to generate QR code. Error code: " + code.toString() + ".";
        errorLogTxt.value = errorLog ? errorLog : "No error log reported.";
        statusEl.setAttribute("style", "display:block");
    }
}

/*
 * Checking whether QR code is ready to redirect to showing QR code image page.
 * Generating QR code is actually very fast so this polling mechanism is just to
 * delay the screen to show the message of generating QR code to users.
 */
function checkQrCodeReady() {
    if (typeof qrCodeImageName !== "undefined" && !qrCodeGenerating) {
        clearInterval(qrCodeCheckingTimer);
        var mainBoxEl = document.getElementById("main-content-box");
        var qrBoxEl = document.getElementById("qr-code-box");
        if (mainBoxEl && qrBoxEl) {
            mainBoxEl.setAttribute("style", "display:none");
            qrBoxEl.setAttribute("style", "display:block");
            var el = document.getElementById("generating_qr_code");
            if (el) {
                el.setAttribute("style", "display:none");
            }
        }
    }
}

/*
 * Close QR code block
 */
function closeQrCodeBlock() {
    var mainBoxEl = document.getElementById("main-content-box");
    var qrBoxEl = document.getElementById("qr-code-box");
    if (mainBoxEl && qrBoxEl) {
        mainBoxEl.setAttribute("style", "display:block");
        qrBoxEl.setAttribute("style", "display:none");
        var el = document.getElementById("main_content");
        if (el) {
            el.setAttribute("style", "display:block");
        }
    }
}

/*
 * Generate QR code
 */
function startGeneratingQrCode() {
    var el = document.getElementById("generating_qr_code");
    if (el) {
        el.setAttribute("style", "display:block");
    }

    qrCodeGenerating = true;
    ajaxPostRequest("/generate_qr_code", "dateTime=" + getQrDateString(), successQrCode, failedQrCode);
    qrCodeCheckingTimer = setInterval(checkQrCodeReady, 2000);
}

/*
 * Return current QR code generating status
 */
function isQrCodeGenerating() {
    return qrCodeGenerating;
}
