#ifdef COMPILE_WEBUI
var customLuaNr5gServCellInfo = {
    lockRdb : false,
    get: (arr)=>[...arr, `
        local numCc=luardb.get('wwan.0.radio_stack.nr5g.num_active_cc') or 0
        local o=getRdbArray(authenticated,'wwan.0.radio_stack.nr5g.scell',0,numCc-1,false,{'cc_id','cell_id','dl_arfcn','ul_arfcn','band','band_type','dl_bw','ul_bw','dl_max_mimo','ul_max_mimo'})
    `],
};
#endif

var nr5gRdbPrefix = "wwan.0.radio_stack.nr5g.";

var Nr5gServCellInfo = PageTableObj("Nr5gServCellInfo", "NR5G Serving Cell Information",
{
#ifdef COMPILE_WEBUI
    customLua: customLuaNr5gServCellInfo,
#endif
    readOnly: true,
    rdbPrefix: nr5gRdbPrefix,
    pollPeriod: 1000,
    extraAttr: {
        tableAttr: {class:"above-5-column"},
        thAttr: [
            {class:"customTh field2"},{class:"customTh field2"},
            {class:"customTh field2"},{class:"customTh field2"},
            {class:"customTh field2"},{class:"customTh field2"},
            {class:"customTh field2"},{class:"customTh field2"},
            {class:"customTh field2"},{class:"customTh field2"},
        ],
    },
    members: [
        staticTextVariable("cc_id", "CC ID"),
        staticTextVariable("cell_id", "Cell ID"),
        staticTextVariable("dl_arfcn", "Dl. ARFCN"),
        staticTextVariable("ul_arfcn", "Ul. ARFCN"),
        staticTextVariable("band", "Band"),
        staticTextVariable("band_type", "Band type"),
        staticTextVariable("dl_bw", "Dl. bw."),
        staticTextVariable("ul_bw", "Ul. bw."),
        staticTextVariable("dl_max_mimo", "Dl. max. MIMO"),
        staticTextVariable("ul_max_mimo", "Ul. max. MIMO"),
    ],
});

var pageData : PageDef = {
    title: "Field test",
    menuPos: ["System", "FieldTest"],
    pageObjects: [Nr5gServCellInfo],
};
