// htmlTag - wrap contents within html tags
// Args
//  tag, the html tag, ie "h2", "p"
//  attrs, object with the attributes to be associated with the tag
//  contents - whatever goes within the tag. Not used for tags that aren't closed
// returns string of the html
// example a hyperlink can be made with htmlTag('a', {src:"http://"}, "Click here")
function htmlTag(tag: string, attrs: any, contents?: string) {
  var closeTag = true;
  switch(tag) {
    case 'input':
    case 'img':
    case 'link':
    case 'meta':
      closeTag = false;
  }
  var attrStr = '';
  for(var attr in attrs){
    attrStr += ' ' + attr + '="' + attrs[attr] + '"';
  }
  var html = "<" + tag + attrStr + ">";
  if (closeTag) return html + contents + "</" + tag + ">";
  return html;
}

function htmlDiv(attrs: any, contents?: string) {
  return htmlTag("div", attrs, contents);
}

function helperText(pe) {
  if (isFunction(pe.helperText))
    return pe.helperText();
  if (pe.helperText)
    return htmlDiv(pe.helperAttr ? pe.helperAttr : {}, htmlTag('span', {id: pe.memberName + "_helperText", class:"normal-text"}, "&nbsp;" + _(pe.helperText)));
  return "";
}

// v is preferably a bool but can be a string where '0' is false and anything else is true
// returns the bool v
function setVisible(div: string, v: any) : boolean {
  var b = toBool(v)
  if(div) $(div).css("display", b ? "":"none");
  return b;
}

function setDivVisible(show: boolean, divName: string) {
  setVisible("#div_" + divName, show);
}

function setTblVisible(show: boolean, tblName: string) {
  setVisible("#table" + tblName, show);
}

function setEditButtonRowVisible(show: boolean) {
  setVisible("#buttonRow", show);
}

function setPageObjVisible(show: boolean, objName: string) {
  setVisible("#objouterwrapper"+objName, show);
}

function removeValidClass(field: string) {
  $("#" + field).removeClass("valid");
}

// Set text for given html id
// @param htmlId The ID of the HTML element to set its text
// @param txt A text string
function setHtmlText(htmlId: string, txt: string) {
  $("#" + htmlId).html(txt);
}

function setToggle(tName,v) {
  var name = "#" + tName;
  if(v=='1') {
    $(name + "_0").prop("checked", true);
    $(name + "_1").removeAttr('checked');
  }
  else {
    $(name + "_1").prop("checked", true);
    $(name + "_0").removeAttr('checked');
  }
  $(name).val(v);
}

function setVisibilityToggle(name,val) {
  setToggle(name,val);
  setVisible("#objwrapper"+name,val);
}

function genToggleHtml(toggleName, onClick) {

  function halfSwitch(whch) {

    var inpAttrs: any = {  type:"radio",
                      name: toggleName + "_radio",
                      id: toggleName + (whch? "_0": "_1"),
                      class:"access", value: (whch? "1": "0") };
    if (!onClick) onClick = "setToggle"
    inpAttrs.onClick = onClick + "('" + toggleName + "','" + (whch? "1": "0") + "');";

    return htmlTag('input', inpAttrs, "") +
            htmlTag('label',
                    {for: toggleName + (whch ? "_0": "_1"), class: (whch? "on": "off")},
                    whch? _("on"): _("off")
                  )
  }

  var radioSwitch = halfSwitch(true) + halfSwitch(false);
  return htmlTag('input',
            {type: "hidden", /*name: toggleName,*/ id: toggleName /*, value: "??"*/}, "")
            + htmlDiv( {class: "location-settings"},
                              htmlDiv({class: "radio-switch"}, radioSwitch)
                      );
}

// enable/disable a given html element
// @param htmlId The ID of the HTML element to enable/disable
// @param en true for enable; false for disable
function setEnable(htmlId: string, en: boolean) {
  $("#" + htmlId).prop("disabled", !en);
}

function setToggleEnable(togName: string, en: boolean){
  setEnable(togName + "_0", en);
  setEnable(togName + "_1", en);
}

function genRadioButtonHtml(btnName, onClick) {
  var id = btnName + "_btn";
  return htmlDiv({class: "radio-box"},
              htmlTag('input',
                      {type: "radio", class: "access", id: id, name: id, onClick: onClick + "('" + id + "');"}) +
              htmlTag('label', {for: id}, "&nbsp;")
            );
}

function setRadioButtonEnable(btnName: string, en: boolean){
  setEnable(btnName + "_btn", en);
}

function getRadioButtonVal(id) {
  var ids = "#" + id + "_btn";
  var attr: any = $(ids).prop('checked');
  // For some browsers, `attr` is undefined; for others,
  // `attr` is false.  Check for both.
  if (!isDefined(attr) && attr !== false) {
      return true;
  }
  return false;
}

function setRadioButtonVal(id, val){
  var ids = "#" + id + "_btn";
    $(ids).prop('checked', toBool(val));
}

function genCheckboxHtml(btnName, onClick) {
  const id = btnName + "_btn";
  const labelHtml = htmlTag('label', {for: id, class: "checkbox-label"}, "&nbsp;");
  return htmlDiv({class: "checkbox", id: "div_"+id},
            htmlTag('input',
                {type: "checkbox", class: "checkbox-access", id: id, name: id, onClick: onClick + "('" + id + "');"}) +
            labelHtml
        );
}

function setCheckboxEnable(id: string, en: boolean){
  setEnable(id, en);
}

function getCheckboxVal(id) {
  const ids = "#" + id;
  const attr: any = $(ids).prop('checked');
  return attr;
}

function setCheckboxVal(id, val){
  const ids = "#" + id;
  $(ids).prop('checked', toBool(val));
}

// Set label text for given html id
// @param htmlId The ID of the HTML element to set its label text
// @param txt Label text
function setLabelText(htmlId: string, txt: string) {
  $("#" + htmlId).siblings('label').text(txt);
}

// This function populates the options in a SELECT getElementById
// param name is the id of the select element
// param options is an array of option strings
// param val is the selected option
function setOption(name: string, options: OptionsType[], v: string) {
  var select = <HTMLSelectElement|null>document.getElementById("sel_" + name);
  if (select == null) return;

  // Remove all previous options
  if (select.options)
    while (select.options.length > 0)
        select.remove(0);

  // Added all the supplied options and select the matching entry
  for(var i = 0; i < options.length; i++) {
      var opt = options[i];
      var el = document.createElement("option");
      if ( typeof opt == "string") {
        el.textContent = _(opt);
        el.value = opt;
      }
      else { // If not a string it's a two string array
        el.textContent = _(opt[1]);
        el.value = String(opt[0]);
      }
      if (el.value == v)
        el.selected = true;
      select.appendChild(el);
  }
}

// This function generates the html template contains the page objects
// and sets the document body.
// The html is based on the V2 WebUI look and feel
// This is called when the page first loads
function genBodyHtml(alert){
    var menu =  htmlDiv({class: "header-wrap", id: "main-menu"}, "" );

    if (pageData.multiColumn) {
        var body =
          htmlTag("form", {class: "validate", name: "form", id: "form", novalidate: "novalidate", onsubmit: "return false"}, "") +
          htmlDiv({class: "site-content", id: "content"},
                      htmlDiv({class: "container dashboard"},
                        htmlDiv({class: "grid-9 alpha", id: "htmlGoesHere"}, "") +
                        htmlDiv({class: "grid-3 omega", id: "rightGoesHere"}, "")
                    )
                  );
    }
    else {
        var titleCssClass = "title-box";
        var bodyCssClass = "body-box";
        var titleDiv = "";
        // Draw separate title box
        if (pageData.noTitleBox !== true) {
          if (isDefined(pageData.style) && isDefined(pageData.style.titleCssClass)) {
            titleCssClass = pageData.style.titleCssClass;
          }
          titleDiv = htmlDiv({class: titleCssClass},
                              htmlDiv({class: "pad", id: "titleGoesHere"}, "")
                            )
        }
        if (isDefined(pageData.style) && isDefined(pageData.style.bodyCssClass)) {
          bodyCssClass = pageData.style.bodyCssClass;
        }
        var body =  htmlDiv({id:"content", class: "site-content"},
                      htmlDiv({class: "container"},
                        htmlTag("aside", {class: "grid-3 alpha sidemenu", id:"side-menu"}, "") +
                        htmlDiv({class: "grid-9 omega"},
                            alert +
                            htmlTag("form", {class:"validate", name:"form", id:"form", novalidate:"novalidate", onsubmit: "return false"},
                              titleDiv +
                              htmlDiv({class: bodyCssClass},
                                htmlDiv({class: "pad", id: "htmlGoesHere"}, "")
                              )
                            )
                        )
                      )
                    );
    }

    var footer = genThemeFooter(pageData);

    return menu + body + footer;
}

// generate buttons
// The order of arguments is the order of buttons in row
// @params button A object buttons with button name and its label text
//                ex) {"save":"CSsave", "refresh":"refresh", "cancel":"cancel"}
// @return html div contents
function genButtons(buttons) {
  function genButton(id, labelText) {
      var attrs: any = {type:"button", id: id};
      return htmlTag("button", attrs, _(labelText));
  }
  var html = "";
  for (const key in buttons) {
    html += genButton(key+"Button", buttons[key]);
  }

  var attribs: any = {id: "buttonRow"};
#ifdef V_WEBIF_SPEC_vdf
  attribs.class = "submit-row";
#else
  attribs.class = "submit-row form-row";
#endif
  if (buttons.length == 0) {
    return "";
  } else if ( buttons.length > 1) {
    attribs.class += " multi-button";
  }
  return htmlDiv(attribs, html);
}

// Assign a function to an event of given button
// @param btn target button which event function will be assigned to
// @param event event name which given function to be assigned to
// @param fn a function name to assign to the button event
// @return none
// Note) not checking button name here to make generic function
function setButtonEvent(btn, event, fn) {
  $("#"+btn+"Button").on(event, fn);
}

// Append button(s) at the end of main div
// The order of arguments is the order of buttons in row
// @params button A object buttons with button name and its label text
//                ex) {"save":"CSsave", "refresh":"refresh", "cancel":"cancel"}
function appendButtons(buttons) {
  $("#htmlGoesHere").append(genButtons(buttons));
}

function disablePageIfPppoeEnabled()
{
  if (isDefined(service_pppoe_server_0_enable) && service_pppoe_server_0_enable == '1') {
    if (isDefined(service_pppoe_server_0_wanipforward_enable) && service_pppoe_server_0_wanipforward_enable == '1') {
      pageData.disabled = true;
      pageData.alertHeading = "pppoeEnabled";
      pageData.alertText = "functionNotAvailable";
    }
  }
}

function genWarningBlock() {
  return htmlTag('h2', {}, "warning line 1")+
         htmlTag('p', {}, "warning line 2");
}

// Show or hide logoff icon in the pages.
// By default logoff icon is shown in all pages except for being
// explicitly defined in a page. Index page does not need and status page
// shows/hides depending on login status.
// @params show A flag to show or hide the logoff icon
function setLogoffVisible(show: boolean) {
  setVisible("#logOff", show);
}

// generate download/clear buttons for logfile page
// @return html div contents
function genLogDownloadClear() {

  function genButton(id, labelText) {
    var attrs: any = {class: "secondary "+id, type:"button", id: id};
    return htmlTag("button", attrs, _(labelText));
  }
  var html = "<a href='' id='fileDownload' download='logmsg.txt'>";
  html += genButton("downloadButton", "download") + "</a>";
  html += genButton("clearButton", "clear");

  var attribs: any = {id: "buttonRow"};
  attribs.class = "submit-row";
  attribs.class += " multi-button";
  return htmlDiv(attribs, html);
}

// Append download/clear buttons for logfile page at the end of title box div
function appendLogDownloadClearButton() {
  $("#objouterwrapperlogfile").append(genLogDownloadClear());
}

// generate select all/unselect all buttons and reset buttons for band selection page
// @params selBtn A button name for Select All function
// @params unselBtn A button name for Unselect All function
// @params resetBtn A button name for Reset All function
// @return html div contents
function genSelUnselAllReset(selBtn: string, unselBtn: string, resetBtn: string) {
  function genButton(id: string, labelText: string) {
    let btnClass = "secondary";
    if (labelText == "Unselect all" ) {
      btnClass += " large-button";
    }
    const attrs: any = {class: btnClass, type:"button", id: id};
    return htmlTag("button", attrs, _(labelText));
  }
  const html = genButton(selBtn+"Button", "Select all") +
             genButton(unselBtn+"Button", "Unselect all") +
             genButton(resetBtn+"Button", "Reset all");

  const attribs: any = {id: "buttonRow"};
  attribs.class = "submit-row  multi-button bottom-padding-60";
  return htmlDiv(attribs, html);
}
