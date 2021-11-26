var SystemInfo = PageObj("StsSystemInfo", "sysInfo",
{
    customLua: {
        lockRdb: false,
        get: function(arr) {
            arr.push("for line in io.lines('/proc/uptime') do o.uptime = line break end");
            return arr;
        },
    },

    readOnly: true,
    column: 1,
    genObjHtml: genCols,
    pollPeriod: 1000,

    columns : [{
        heading:"system time",
        members:[{hdg: "status system up time",
            genHtml: (obj) => isDefined(obj.uptime) ? toUpTime(parseInt(obj.uptime)) : "" }
        ]
    },
    {
        heading:"routerVersion",
        members:[
            {hdg: "model", genHtml: (obj) => obj.model},
            {hdg: "boardVersion", genHtml: (obj) => obj.hw_ver === "" ? "1.0" : obj.hw_ver},
            {hdg: "firmware", genHtml: (obj) => obj.version},
        ]
    },
    {
        heading:"advStatus phoneModule",
        members:[
            {hdg: "module firmware",
            genHtml: (obj) => "<span style='display:inline-block;word-wrap:break-word;width:180px'>" + obj.moduleFirmwareVersion + "</span>"},
            {hdg: "module hardware", genHtml: (obj) => obj.moduleHardwareVersion},
            {hdg: "status CSimei", genHtml: (obj) => obj.imei}
        ]
    }],
    populate: populateCols,

    members: [
        hiddenVariable("model", "system.product.model"),
        hiddenVariable("version", "sw.version"),
        hiddenVariable("hw_ver", "wwan.0.hardware_version"),
        hiddenVariable("moduleFirmwareVersion", "wwan.0.firmware_version"),
        hiddenVariable("moduleHardwareVersion", "wwan.0.hardware_version"),
        hiddenVariable("imei", "wwan.0.imei")
    ]
});

stsPageObjects.push(SystemInfo);
