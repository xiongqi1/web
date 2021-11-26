// The status page uses a different paradigm t all the other pages.
// For the moment I can't see this being used for any other pages so the framework extension is ket in this page
// Status page objects are based around columns

// This helper function wraps an status object's html with the appropriate html
// heading is the name of the status object
// html is the objects statis html
function _genColumnsWrapper(heading, html) {
    return htmlDiv({class: "box"},
                htmlDiv({class: "box-header"}, htmlTag("h2", {},_(heading))) + htmlDiv({class: "row"}, html ));
}

// This helper function  generates the static html for the columns of info
// objName is used to generate the html ids
// columns is an array of columns that describes the data to be displayed
function _genCols(objName, columns, extra) {
    var html = "";
    columns.forEach(function(col, i) {
        var colhtml = "";
        col.members.forEach(function(mem, j){
        if (!isDefined(mem.name)) { // If an name is not given generate one from its position
            mem.name = objName + "_" + i + "_" + j;
        }
        colhtml += htmlTag("dl", {id: mem.name + "_div"},
            htmlTag("dt", {}, _(mem.hdg)) + htmlTag("dd", {id: mem.name}, "dd"));
        });
        // First column gets alpha attribute, last gets omega
        var attr: any = {class: "each-box" + (i === 0? " alpha": "") + (i === columns.length-1? " omega": "")};
        if (i === columns.length-1) attr.style = "border:none";
        // Add a heading if one given
        html += htmlDiv(attr, (isDefined(col.heading)? htmlTag("h3", {}, _(col.heading)): "") + colhtml);
    });
    return htmlDiv({id: objName, class:"box-content"}, html + extra);
}

// This helper gives default action for simple pageObjects
function genCols() {
    return _genColumnsWrapper(this.labelText, _genCols(this.objName, this.columns, ""));
}

// When data is received this function is called to write values to the page
// this points to a page objects
// Each generates the dynamic html for the boxes
function _populateCols(columns, obj) {
    columns.forEach(function(col) {
        col.members.forEach(function(mem){
        if (isFunction(mem.genHtml)) {
            $("#" + mem.name).html(mem.genHtml(obj));
        }
        if (isFunction(mem.isVisible)) {
            var vis = mem.isVisible(obj);
            if (vis === true)
                $("#" + mem.name + "_div").show();
            if (vis === false)
                $("#" + mem.name + "_div").hide();
        }
        });
    });
}

// This helper gives default action for simple pageObjects
function populateCols() {
    _populateCols(this.columns, this.obj);
}

function isLetter(c) {
    return c.toLowerCase() != c.toUpperCase();
}

function pad(str, max) {
    return str.length < max ? pad("0" + str, max) : str;
}
function isValidValue(val) {
    return isDefined(val) && val != "" && val != "N/A";
}
/* Decodes a Unicode string into ASCII ready for printing on status page */
function UrlDecode(str) {
    var output = "";
    for (var i = 0; i < str.length; i+=3) {
        var val = parseInt("0x" + str.substring(i + 1, i + 3));
        output += String.fromCharCode(val);
    }
    return output;
}

var stsPageObjects = [];
#include "pages/status_components/SystemInfo-Saturn.js"
#include "pages/status_components/LanInfo-Saturn.js"
// enable WanInfo-Saturn.js if necessary
//#include "pages/status_components/WanInfo-Saturn.js"
#include "pages/status_components/CellularInfo-Saturn.js"
#include "pages/status_components/WWanInfo-Saturn.js"
#include "pages/status_components/AdvInfo-Saturn.js"

var pageData : PageDef = {
    title: "Status",
    menuPos: ["Status", "TR"],
    authenticatedOnly: false,
    multiColumn: true,
    pageObjects: stsPageObjects,
}
