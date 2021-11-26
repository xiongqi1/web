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
    if(attr == "readonly" && attrs[attr]) {
      attrStr += ' readonly';
      continue;
    }
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

function setToggleEnable(togName: string, en: boolean){
  en = !en;
  $("#" + togName + "_0").prop("disabled", en);
  $("#" + togName + "_1").prop("disabled", en);
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
  $("#" + btnName + "_btn").prop("disabled", !en);
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
          htmlTag("form", {class: "validate", name: "form", id: "form", novalidate: "novalidate"}, "") +
          htmlDiv({class: "site-content", id: "content"},
                      htmlDiv({class: "container dashboard"},
                        htmlDiv({class: "grid-9 alpha", id: "htmlGoesHere"}, "") +
                        htmlDiv({class: "grid-3 omega", id: "rightGoesHere"}, "")
                    )
                  );
    }
    else {
        var body =  htmlDiv({id:"content", class: "site-content"},
                      htmlDiv({class: "container"},
                        htmlTag("aside", {class: "grid-3 alpha sidemenu", id:"side-menu"}, "") +
                        htmlDiv({class: "grid-9 omega"},
                            alert +
                            htmlTag("form", {class:"validate", name:"form", id:"form", novalidate:"novalidate"},
                              htmlDiv({class: "right-column white-box"},
                                htmlDiv({class: "pad", id: "htmlGoesHere"}, "")
                              )
                            )
                        )
                      )
                    );
    }
    var footer = htmlTag("footer", {class: "footer"},
                  htmlDiv({class: "container"},
                    htmlTag("p", {class: "copy-right"}, _("powered by netComm"))));

    return menu + body + footer;
}

function genSaveRefreshCancel(isSave,isRefresh,isCancel) {

  var numButtons = 0;
  function genButton(isActive, id, labelText) {
      var attrs: any = {type:"button", id: id};
      if (isActive)
        numButtons++;
      else
        attrs.style = "display: none;"
      return htmlTag("button", attrs, _(labelText));
  }

  var html =  genButton(isRefresh,"refreshButton", "refresh") +
              genButton(isSave,"saveButton", "CSsave") +
              genButton(isCancel,"cancelButton", "cancel");
  var attribs: any = {id: "buttonRow"};
#ifdef V_WEBIF_SPEC_vdf
  attribs.class = "submit-row";
#else
  attribs.class = "submit-row form-row";
#endif
  if (numButtons > 1) attribs.class += " multi-button";
  if (numButtons === 0) attribs.style = "display: none;"
  return htmlDiv(attribs, html);
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

