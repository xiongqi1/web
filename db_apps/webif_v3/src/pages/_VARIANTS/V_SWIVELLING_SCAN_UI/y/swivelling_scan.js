// Send a Swivelling Scan command
// @params cmd A swivelling scan command
//             [moveToHomePosition|startSwivellingScan|stopSwivellingScan]
// @params failMsg A message to be displayed when command fails
function sendSwivellingCmd(cmd: string, failMsg) {
    var succMsg, failMsg;
    var formData = new FormData();
    formData.append("csrfTokenPost",csrfToken);
    blockUI_wait(_("GUI pleaseWait"));
    $.ajax({
        xhr: function() {
            var xhr = new XMLHttpRequest();
            return xhr;
        },
        url: "swivelling_scan/"+cmd,
        data: formData,
        processData: false,
        contentType: false,
        type: 'post',
        success: (respObj) => {
            $.unblockUI();
            var result = JSON.parse(respObj.result)
            if (!result.success) {
                blockUI_alert(failMsg);
            }
        },
        error: function(XMLHttpRequest, textStatus, errorThrown) {
            $.unblockUI();
            blockUI_alert(failMsg);
        }
    });
}

// Enable/disable buttons per swivelling status
// status : checkingMotor, initialising, swivellingScan, swivellingScanMoving,
//          swivellingScanAtPosition, movingToBestPosition, successFinal, failed, none, busy, stopped
function setButtonStatus(status) {
    switch( status ) {
        case 'checkingMotor':
        case 'initialising':
        case 'swivellingScan':
        case 'busy':
            setEnable("homeButton", false);
            setEnable("startButton", false);
            setEnable("stopButton", false);
            $("#spinner").show();
            break;
        case 'swivellingScanMoving':
        case 'swivellingScanAtPosition':
        case 'movingToBestPosition':
            setEnable("homeButton", false);
            setEnable("startButton", false);
            setEnable("stopButton", true);
            $("#spinner").show();
            break;
        case 'successFinal':
        case 'failed':
        case 'none':
        case 'stopped':
            setEnable("homeButton", true);
            setEnable("startButton", true);
            setEnable("stopButton", false);
            $("#spinner").hide();
            break;
    }
}

function isMoving() {
    var stObj = JSON.parse(swivellingScanStatus.obj.scanStatus);
    return (stObj.movingToPositionIndex != '-1');
}

function cvtIdxToDeg(targetIdx, homeIdx, coor) {
    var retDeg = -1;
    if (isNaN(targetIdx) || isNaN(homeIdx)) {
        return retDeg;
    }
    var targetDeg = targetIdx != -1? coor[targetIdx].coordinate:targetIdx;
    var homeDeg = homeIdx != -1? coor[homeIdx].coordinate:homeIdx;
    if (targetDeg >= 0 && homeDeg >= 0) {
        retDeg = targetDeg - homeDeg;
        if (retDeg < 0) {
            retDeg = retDeg + 360;
        }
    }
    if (retDeg >= 0) {
        return retDeg.toString() + " degrees";
    }
    return 'N/A';
}

var prettyStr = {
    "checkingMotor" : "self-testing motor",
    "initialising" : "initialising",
    "swivellingScan" : "swivelling scan",
    "swivellingScanMoving" : "moving to a location",
    "swivellingScanAtPosition" : "moving to a location",
    "movingToBestPosition" : "moving to best position",
    "successFinal" : "moved to best position",
    "failed" : "swivelling scan failed",
    "none" : "none",
    "busy" : "busy",
    "stopped" : "stopped"
}

#ifdef COMPILE_WEBUI
var customLuaSwivellingScanStatus = {
  lockRdb: false,
  get: function(arr) {
      arr.push("local pos_num=tonumber(luardb.get('swivelling_scan.conf.position.number'))");
      arr.push("o.coor=getRdbArray(authenticated,'swivelling_scan.conf.position',0,pos_num-1,true,{'coordinate'})")
      return arr;
  },
};
#endif

var swivellingScanRdbPrefix = "swivelling_scan.";
var swivellingScanStatus = PageObj("swivellingScanStatus", "mmWave AutoScan status",
{
#ifdef COMPILE_WEBUI
    customLua: customLuaSwivellingScanStatus,
#endif
    readOnly: true,
    pollPeriod: 1000,
    rdbPrefix: swivellingScanRdbPrefix,
    members: [
        staticTextVariableWait("homePosIdxDeg", "Home position")
            .setRdb("conf.home_position_index"),
        staticTextVariable("currPosIdxDeg", "Current position"),
        staticTextVariable("movToPosIdxDeg", "Moving to position")
            .setIsVisible(function() {return isMoving();}),
        staticTextVariable("currSwiScanStep", "Current swivelling scan step")
            .setIsVisible(function() {return isMoving();}),
        staticTextVariable("status", "Status"),
        staticTextVariable("error", "Error")
            .setIsVisible(function() {
                return toBool(swivellingScanStatus.obj.error != "");})
            .addStyle('color:var(--CasaOrange);'),
        hiddenVariable("homePosIdx", "conf.home_position_index"),
        hiddenVariable("currPosIdx", ""),
        hiddenVariable("scanStatus", "status")
    ],
    decodeRdb: function(obj) {
        if (obj.scanStatus != "") {
            var stObj = JSON.parse(obj.scanStatus);
            obj.currPosIdx = stObj.currentPositionIndex;
            obj.homePosIdxDeg = cvtIdxToDeg(obj.homePosIdx, obj.homePosIdx, obj.coor);
            obj.currPosIdxDeg = cvtIdxToDeg(obj.currPosIdx, obj.homePosIdx, obj.coor);
            obj.movToPosIdxDeg = cvtIdxToDeg(stObj.movingToPositionIndex, obj.homePosIdx, obj.coor);
            obj.currSwiScanStep = stObj.currentSwivellingScanStep;
            if (stObj.maxSwivellingScanStep != 0) {
                var percent = 100*obj.currSwiScanStep/stObj.maxSwivellingScanStep
                obj.currSwiScanStep = Math.round(percent) + ' %';
            }
            //obj.maxSwiScanStep = stObj.maxSwivellingScanStep;
            obj.status = prettyStr[stObj.status];
            obj.error = stObj.error;
            setButtonStatus(stObj.status);
        }
        return obj;
    },
});

var pageData : PageDef = {
    title: "mmWave AutoScan",
    menuPos: ["Networking", "SwivellingScan"],
    pageObjects: [swivellingScanStatus],
    onReady: function () {
        appendButtons({"home":"home", "start":"start", "stop":"stop"});
        setButtonEvent("home", "click", function() {
            sendSwivellingCmd("moveToHomePosition", _("moveToHomePosition fail"));});
        setButtonEvent("start", "click", function() {
            sendSwivellingCmd("startSwivellingScan", _("startSwivellingScan fail"));});
        setButtonEvent("stop", "click", function() {
            sendSwivellingCmd("stopSwivellingScan", _("stopSwivellingScan fail"));});
        setEnable("stopButton", false);
    }
}
