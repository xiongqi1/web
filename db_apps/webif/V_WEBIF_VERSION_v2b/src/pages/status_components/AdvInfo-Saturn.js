function dBmFmt(v, units, thres) {
    if (!isDefined(thres))
        thres = -120;
    var val=parseInt(v);
    if (val != NaN && val <= thres)
        return _("na");
    return v + units;
}

var advStatus = PageObj("StsAdvStatus", "advStatus advancedStatus",
{
    readOnly: true,
    column: 1,
    genObjHtml: genCols,
    pollPeriod: 1000,

    columns : [{
        members:[
        {hdg: "advStatus mcc", genHtml: (obj) => obj.MCC},
        {hdg: "advStatus mcc", genHtml: (obj) => pad(obj.MNC, 2)},
        {hdg: "advStatus iccid", genHtml: (obj) => obj.simICCID},
        ]
    },

    {
        members:[
        {hdg: "advStatus imsi", genHtml: (obj) => obj.IMSI},
        {hdg: "advStatus cellID", genHtml: (obj) => obj.cellId},
        {hdg: "advStatus channelNumber UARFCN", genHtml: (obj) => obj.ChannelNumber},
        ]
    },

    {
        members:[
        {hdg: "rsrq", genHtml: (obj) => dBmFmt(obj.rsrq, " dB", -136), isVisible: (obj) => isValidValue(obj.rsrq)},
        {hdg: "rsrp", genHtml: (obj) => dBmFmt(obj.rsrp, " dB", -136), isVisible: (obj) => isValidValue(obj.rsrp)},
        {hdg: "packet service", genHtml: (obj) => obj.psAttached},
        ]
    }],

    populate: function () {
        _populateCols(this.columns, this.obj);
    },

    decodeRdb: function (obj) {
    if (obj.psAttached == "1") {
        obj.psAttached = _("ps attached");
    } else {
        obj.psAttached = _("ps detached");
    }
    return obj;
    },

    members: [
        hiddenVariable("MCC", "wwan.0.system_network_status.MCC"),
        hiddenVariable("MNC", "wwan.0.system_network_status.MNC"),
        hiddenVariable("cellId", "wwan.0.system_network_status.CellID"),
        hiddenVariable("IMSI", "wwan.0.imsi.msin"),
        hiddenVariable("rsrp", "wwan.0.signal.0.rsrp"),
        hiddenVariable("rsrq", "wwan.0.signal.rsrq"),
        hiddenVariable("simICCID", "wwan.0.system_network_status.simICCID"),
        hiddenVariable("ChannelNumber", "wwan.0.system_network_status.channel"),
        hiddenVariable("psAttached", "wwan.0.system_network_status.attached"),
    ]
});

stsPageObjects.push(advStatus);
