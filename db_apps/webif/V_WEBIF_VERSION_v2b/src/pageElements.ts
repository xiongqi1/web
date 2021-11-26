//Combines to objects into objName
//It returns the merged object
function mergeObject(mainObj, extraObj? : object) {
  var obj = mainObj
  for (var key in extraObj) {
    if (extraObj.hasOwnProperty(key)) {
      obj[key] = extraObj[key];
    }
  }
  return obj;
}

var validaters = {};
#ifdef V_WEBIF_SPEC_vdf
var validatersId = 0;
#else
function validateField(field){
  var field0 = field[0];
  var name = field0.name;
  if (name != ""){
    var pe = validaters[name];
    if (isDefined(pe) && isDefined(pe.fnValidateField)) {
      for (var i = 0; i < pe.fnValidateField.length; i++) {
        if (pe.fnValidateField[i](field0.value) === false) {
          return pe.errMsg[i];
        }
      }
    }
  }
}
#endif

function validateKey(field){
    var name = field.name;
    if (name != ""){
      var member = validaters[name];
      if (isDefined(member) && isFunction(member.fnValidateKey)) {
        member.fnValidateKey(field);
      }
    }
}

class PageElement {
  memberName: string;
  attr: any = {};
  validateLua = [];
  conditionsForCheck: string[] = [];
  readOnly?: boolean;
  unwrapped?: boolean;
  hidden?: boolean;
  class?: string;
  style?: string;
  labelFor?: string;
  genMultiRow? : any;

  constructor (name: string, extras? : any) {
    this.memberName = name;
    mergeObject(this, extras);
  };

  encode?: boolean;
  setEncode(encode: boolean) {
    this.encode = encode;
    return this;
  };

  labelText?: string;
  setLabel(label) {
    this.labelText = label;
    return this;
  };

  htmlId?: string;
  getHtmlId() {
    return this.htmlId ? this.htmlId : this.memberName;
  }

  fnIsVisible? : any;
  setIsVisible(fnIsVisible) {
    this.fnIsVisible = fnIsVisible;
    return this;
  }

  setVisible = (show: boolean) => setDivVisible(show, this.memberName);

  required?: boolean;
  setRequired(req: boolean) {
    this.required = req;
    return this;
  };
#ifdef COMPILE_WEBUI // This block only required for build and not on client
  luaGet? : (po: PageObject) => string | undefined;
  luaSet? : (po: PageObject) => string | undefined;
  setRdb(rdb) {
    this.luaGet = (po: PageObject) => "getRdb('" + (isDefined(po.rdbPrefix) ? po.rdbPrefix + rdb : rdb) + "')";
    this.luaSet = (po: PageObject) => "setRdb('" + (isDefined(po.rdbPrefix) ? po.rdbPrefix + rdb : rdb) + "',v)";
    return this;
  };

  setUci(uci: string) {
    this.luaGet = (po: PageObject) => util.format("getUci(%s)", JSON.stringify(uci));
    this.luaSet = (po: PageObject) => util.format("setUci(%s, v)", JSON.stringify(uci));
    return this;
  };

  setShell(getter: string | null, setter: string | null, unlockRdb: boolean) {
    if(getter) {
      this.luaGet = (po: PageObject) => util.format("shellExecute(%s, %s)", JSON.stringify(getter), unlockRdb);
    }

    if(setter) {
      this.luaSet = (po: PageObject) => util.format(`shellExecute(string.format("set SETVAL='%%s' && ", v) .. %s, %s)`, JSON.stringify(setter), unlockRdb);
    }
    return this;
  };

  // parameter checks is an array of lua tests, typically "member==option"
  setConditionsForCheck(checks: string[]) {
    for(var chk in checks) {
      // Add an o. which is the container to give "o.member==option"
      // o is the object parameter to the lua function that this code is inserted into
      this.conditionsForCheck.push("o." + checks[chk]);
    }
    return this;
  }
#else
    // This is a dummy function.
    // The preprocesser maps build time functions to this
    getThis() {
      return this;
    }
#endif

   unsetValidates() {
     this.validateLua = [];
     this.fnValidateField = [];
     this.errMsg = [];
     var id = "inp_" + this.memberName;
     delete validaters[id];
   }

    setValidate(fnValidateField : (val: string) => boolean, errMsg: string, fnValidateKey? : (fld: any) => any, luaValidateFn? : string) {
      if (isDefined(fnValidateKey)) {
        this.fnValidateKey = fnValidateKey;
      }
      if (isDefined(luaValidateFn)){
        this.validateLua.push(luaValidateFn);
      }
      this.attr.onKeyUp = "validateKey(this);"
      var id = "inp_"+this.memberName;

#ifdef V_WEBIF_SPEC_vdf
      validatersId++;
      var vId = "validator"+validatersId;
      this.validate = this.validate + " " + vId;
#ifndef COMPILE_WEBUI
      $.validator.addMethod(vId, function(c,a) {
        if(c!==$(a).attr("data-watermark")) {
            return fnValidateField(c);
        }
        if($(a).hasClass("required")) {
          return false;
        }
        return true;
      }, _(errMsg));
#endif
#else
      if (!isDefined(this.fnValidateField)){
        this.fnValidateField = [];
        this.errMsg = [];
      }
      this.fnValidateField.push(fnValidateField);
      this.errMsg.push(_(errMsg));
      this.validate = "validate[" + (this.required === false ? "": "required,") + "funcCall[validateField]]";
#endif
      validaters[id] = this;
      return this;
    }

    authenticatedOnly? : boolean;
    setAuthenticatedOnly() {
      this.authenticatedOnly=true;
      return this;
    }
    // Most of these are just dummy definitions
    // TODO - replace any with proper types
    onInDOM? : () => void;
    genHtml?(): string;
    getVal?() : string;
    setVal(obj: any, val: string){};

    helperText? : any;
    validate? : any;
#ifndef V_WEBIF_SPEC_vdf
    fnValidateField? : any[];
    errMsg? : any[];
#endif
  onKeyUp? : any;
    onClick? : any;
    fnValidateKey? : any;
    visibilityVar? : any;
}

class HiddenVariable extends PageElement {
  constructor (name: string, rdbName: string, extras? : any) {
    super(name, extras);
    this.hidden = true;
    this.setRdb(rdbName);
  };
}

function hiddenVariable(memberName, rdbName) {
  return new HiddenVariable(memberName, rdbName);
}

class ShownVariable extends PageElement {
  onClick?: any;
  constructor (name: string, labelText: string, extras? : any) {
    super(name, extras);
    this.setLabel(labelText);
  }
  genLink() {
    return htmlTag("a", {id: "link_" + this.getHtmlId()}, "");
  }
}

function shownVariable(memberName: string, labelText: string, extras? : any) {
  var pe = new ShownVariable(memberName, labelText, extras);
  return pe;
}
class StaticTextVariable extends ShownVariable {
  style = "margin:6px 0 0 3px;";
  getVal() { return $("#" + this.getHtmlId()).html(); }
  setVal(obj: object, val: string) { $("#" + this.getHtmlId()).html(val); }
  genHtml() {
    return htmlTag('span', { id: this.getHtmlId() }, "");
  };
}

function staticTextVariable( name, label) {
  return new StaticTextVariable(name, label);
}

class StaticI18NVariable extends StaticTextVariable {
  setVal(obj: object, val: string) {
    super.setVal.call(this, obj, _(val));
  }
}

function staticI18NVariable( name, label) {
  return new StaticI18NVariable(name, label);
}

class StaticTextConst extends ShownVariable {
  text: string;
  readOnly = true;
  style = "margin:6px 0 0 3px;";
  constructor(name: string, labelText: string, text: string, extras?: any) {
    super(name, labelText, extras);
    this.text = text;
  }
  getVal() { return this.text; }
  genHtml() {
    return htmlTag('span', {id: this.getHtmlId()}, this.text);
  }
}

function staticText(name, label, text) {
  return new StaticTextConst(name, label, text);
}

class ConstTextArea extends ShownVariable {
  rows: Number;
  cols: Number;
  setVal(obj: object, val: string) { $("#" + this.getHtmlId()).text(val); }
  genHtml() {
    return htmlTag('textarea', { id: this.getHtmlId(), rows: this.rows, cols: this.cols, readonly: true });
  }
}

function constTextArea(name, label, extras? : any) {
  return new ConstTextArea(name, label, extras);
}

class EditableTextVariable extends PageElement {

  mirrorHtmlId?: string;

  isClearText = true;
  setClearText(type: boolean)
  {
    this.isClearText = type;
    return this;
  }

  constructor (memberName: string, labelText: string, extras?: object) {
    super(memberName);
    this.setLabel(labelText);
    mergeObject(this, extras);
    if ( !isDefined(this.required) || this.required !== false ) {
#ifdef V_WEBIF_SPEC_vdf
      this.validate = "required";
#else
      this.validate = "validate[required]";
#endif
    }
    else {
      this.validate = "";
    }
  }

  maxlength?: number;
  setMaxLength(len: number) {
    this.maxlength = len;
    this.validateLua.push("(#v<="+len+")");
    return this;
  }
  minlength?: number
  setMinLength(len:number) {
    this.minlength = len;
    this.validateLua.push("(#v>="+len+")");
    return this;
  }

  makeSmall = false;
  setSmall() {
    this.makeSmall = true;
    return this;
  }

  getVal() {
    return String($("#"+this.getHtmlId()).val());
  }

  setVal(obj: any, val: string) {
    $("#"+this.getHtmlId()).val(val);
    if (this.mirrorHtmlId) $("#"+this.mirrorHtmlId).val(val);
  }

  genHtml() {
    var id = this.getHtmlId();
    this.labelFor = id;
    var attr = mergeObject(this.attr, {name: id, id: id, type: this.isClearText ? "text" : "password", value: "??"});
    if(isDefined(this.maxlength)) attr.maxlength = this.maxlength;
    attr.class = this.validate + (this.makeSmall ? " sml": " large")
    return htmlTag('input', attr );
  }
}

function editableTextVariable(memberName: string, labelText: string, extras?: any) {
  return new EditableTextVariable(memberName, labelText, extras);
}

class EnabledTextVariable extends StaticTextVariable {
  setVal(obj: any, val: string){$("#"+this.getHtmlId()).html(toBool(val)? "enabled": "disabled");}
}

/*
 * format a positive integer number to string with 2 digits e.g 1 --> "01"
 * @param num input positive integer number
 * @return formatted string
 */
function format2Digits(num: number) {
    return num < 10 ? ("0" + num.toString()) : num.toString();
}

class InstallCertTimeVariable extends StaticTextVariable {
  setVal(obj: any, val: string) {
    var date = new Date();
    var instCertTime = date.getFullYear().toString()
        + "-" + format2Digits(date.getMonth() + 1)
        + "-" + format2Digits(date.getDate())
        + "T" + format2Digits(date.getHours())
        + ":" + format2Digits(date.getMinutes())
        + ":" + format2Digits(date.getSeconds()) + "Z";
    $("#"+this.getHtmlId()).html(instCertTime);
  }
}

// If the "lower" value is negative then permit a leading "-" in the entered digit, otherwise don't.
class EditableBoundedInteger extends EditableTextVariable {
  msg: string;

  constructor (memberName: string, labelText: string, lower: number, upper: number, msg: string, extras?: object) {
    super(memberName, labelText, extras);
    this.msg = msg;
    this.setBounds(lower, upper);
 }

  setBounds(lower: number, upper: number) {
    this.unsetValidates();
    this.setValidate(
      (val: string) => {
        const num = parseInt(val)
        return !isNaN(num) && num >= lower && num <= upper;
      },
      this.msg,
      (field) => lower < 0 ? SignedNumfieldEntry(field) : NumfieldEntry(field),
      lower < 0 ? `isValid.BoundedSignedInteger(v,${lower}, ${upper})`
        : `isValid.BoundedInteger(v,${lower}, ${upper})`);
  }
}

class EditablePasswordVariable extends EditableTextVariable {

  genMultiRow?: (lf:any) => string;

  constructor (memberName: string, labelText: string, extras?: object) {
    super(memberName, labelText, extras);
    this.isClearText = false;
    this.encode = true;
  }
  setVerify = function(verifyLabelText) {
    this.genMultiRow = function(labelAndField) {

      function addVerification(html, matchName) {
#ifdef V_WEBIF_SPEC_vdf
        return html.replace("class=", "equalTo='#inp_" + matchName + "' class=");
#else
        return html.replace("[required]", "[required,equals[inp_" + matchName + "]]");
#endif
      }

      var html = labelAndField(this, this.genHtml());

      var orgId = this.htmlId;
      var orgLabel = this.labelText;
      var orgName = this.memberName;
      this.memberName += "_verify";
      this.htmlId += "_verify";
      this.setVisible = (show: boolean) =>  { setDivVisible(show, this.memberName); setDivVisible(show, this.memberName + "_verify") }
      this.labelText = verifyLabelText;
      this.mirrorHtmlId = this.htmlId;
      html += labelAndField(this, addVerification(this.genHtml(), orgName));
      this.memberName = orgName;
      this.labelText = orgLabel;
      this.htmlId = orgId;
      return html
    }

    return this;
  }
}

function editablePasswordVariable(memberName, labelText) {
  return new EditablePasswordVariable(memberName, labelText, {});
}

#ifndef COMPILE_WEBUI // This block only required for build
declare function updatePassStrengthField(field: HTMLElement, strength: string): void;
declare function getPassStrength(p1:string): string;

function updateStrength(PassStr: string, field: HTMLElement | null) {
  if ( field) {
    var strength =  getPassStrength(PassStr);
    updatePassStrengthField(field, strength);
  }
}

#ifdef V_CHECK_PASSWORD_STRENGTH_y
function checkPassStrength(th, memberName) {
	updateStrength(th.value, document.getElementById(memberName));
}
#endif
#endif

function displayPassword(id) {
	var keyString=<string>$("#"+id).val();
	var key_window = window.open("", "", "toolbar=no,width=500,height=100");
	key_window.document.write(_("key string window")+"<b>&nbsp;"+htmlNumberEncode(keyString)+"</b>");
	key_window.document.close();
}

class EditableStrongPasswordVariable extends EditablePasswordVariable {

  override_genHtml = false;
  passStrength: string;
  genHtml() {
    if (!this.override_genHtml)
      return super.genHtml();
    return htmlTag("span", {
      class: "normal-text",
      id: this.passStrength,
      style: "width:150px"}, "");
  }
#ifndef COMPILE_WEBUI // This block only required for build
  setVal(obj: any, val: string) {
    super.setVal(obj, val);
    if (isDefined(val)) {
      updateStrength(val, document.getElementById(this.passStrength));
    }
  }
#endif
}

function editableStrongPasswordVariable(memberName, labelText) {
  var pe = new EditableStrongPasswordVariable(memberName, labelText, {});
#ifdef V_CHECK_PASSWORD_STRENGTH_y
#ifdef COMPILE_WEBUI // This block only required for build
  addExtraScript("/js/zxcvbn.js");
#else // This block only required for client
  pe.passStrength = memberName + "PassStrength"
  pe.attr = {onkeyup:"checkPassStrength(this, '" + pe.passStrength + "')"};
  pe.helperText = "ripKeyLimit";
  var bulletHead=convert_to_html_entity("  • ");
  pe.setValidate(
    function (val) {
      return getPassStrength(val) == 'strong';
    },
    [
	    _("passwordWarning6"),
	    bulletHead + _("passwordWarning2 8to16"),
	    bulletHead + _("passwordWarning3"),
	    bulletHead + _("passwordWarning4") + convert_to_html_entity('`~!@#$%^&*()-_=+[{]}\|;:\'\",\<.>/?.')
    ].join("<br>")
  );

  pe.genMultiRow = function(labelAndField) {

    var html = labelAndField(this, this.genHtml());

    var orgId = this.htmlId;
    var orgLabel = this.labelText;
    var orgName = this.memberName;
    var orghelperText = this.helperText;
    this.override_genHtml = true;
    this.memberName += "_helper";
    this.htmlId += "_helper";
    this.setVisible = (show: boolean) => {
      setDivVisible(show, this.memberName);
      setDivVisible(show, this.memberName + "_helper")
    }
    this.labelText = _("passwordStrength");
    this.helperText = () => htmlTag('a', {
              href: 'javascript:showStrongPasswordInfo();',
              id: "passwordInfo",
              style: 'background-color:transparent;'
            },
            htmlTag('i', {id: "net-info", style: "margin:5px;"}, "")) +
            htmlTag("a", {
                        href:"javascript:displayPassword('"+orgId+"')",
                        style:"color:BLUE"
                      }, _("clickHereToDisplay"));
    html += labelAndField(this,this.genHtml());
    this.memberName = orgName;
    this.labelText = orgLabel;
    this.helperText = orghelperText;
    this.override_genHtml = false;
    this.htmlId = orgId;
    return html
  }
#endif
#endif
  return pe;
}

class EditableIpAddressVariable extends ShownVariable {
  setVal(obj: any, val: string){parse_ip_into_fields(val,this.memberName);}
  getVal() { return parse_ip_from_fields(this.memberName); }

  fnGenIPBlocks?: any;
  genHtml() {
    var fnGenIPBlocks = this.fnGenIPBlocks;
    if (!isDefined(fnGenIPBlocks))
      fnGenIPBlocks = this.required === false ? genHtmlIpBlocksWithoutRequired : genHtmlIpBlocks ;
    return fnGenIPBlocks(this.memberName);
  }
}

function editableIpAddressVariable(memberName: string, labelText: string, _fnGenIPBlocks?: any) {
  var pe = new EditableIpAddressVariable(memberName, labelText);
  pe.fnGenIPBlocks = _fnGenIPBlocks;
  pe.validateLua.push("isValid.IpAddress(v)");
  return pe;
}

// This function takes the name of a IP field in the form *_x (like dmzIpv6_2) and generates the name of the next field *_(x+1) (like dmzIpv6_3)
// Nothing is returned if the name is not valid
function nextField(name) {
  if (isDefined(name)) {
    var split = name.split('_');
    if (split.length >= 2) {
      split[split.length-1] = Number(split[split.length-1]) + 1;
      return split.join('_');
    }
  }
}

// Turn the field name like dmzIpv6_2 into the the group name dmzIpv6
function getGroupName(name) {
    var split = name.split("_");
    split.pop(); // Remove numeric id to give base name
    return split.join('_');
}

// A key has been pressed in an IPv6 field
// Check if the character is allowed
function onKeyPressIpv6(_this, event) {
  if (!isHexaDigit(event.key)) {
    if (event.key == ':') { // Lets move user to next field
      var next = nextField(_this.id);
      if (isDefined(next)) {
        var el = document.getElementById(next);
        if (el) {
          el.focus();
        }
      }
    }
    event.preventDefault(); // This prevents the character getting added to the buffer
  }
}

// A key has been pressed in an IPv4 field
// Check if the character is allowed
function onKeyPressIpv6v4(_this, event) {
  if (!isDigit(event.key) && event.key !== '.') {
    event.preventDefault(); // This prevents the character getting added to the buffer
  }
}

#ifndef COMPILE_WEBUI
// Something has been pasted into one of the IPv6 fields
// Get the data from the clipboard and apply to the fields
function onPasteIpv6(_this, e) {
  e.preventDefault(); // We do all the processing here

  var content;
  if( e.clipboardData ) {
    content = e.clipboardData.getData('text/plain');
  }
  else if( window.clipboardData ) {
    content = window.clipboardData.getData('Text');
  }
  if (content) {
    parse_ipv6_into_fields(content, getGroupName(_this.id));
  }
  return false;
}

// User wants to copy from one of the IPv6 fields
// Get the IP address from the fields and copy to the clipboard
function onCopyIpv6(_this, e) {
  e.preventDefault(); // We do all the processing here

  var textToCopy = parse_ipv6_from_fields(getGroupName(_this.id));
  if( e.clipboardData ) {
    e.clipboardData.setData('Text', textToCopy);
  }
  else if( window.clipboardData ) {
    window.clipboardData.setData('Text', textToCopy);
  }
  return false;
}
#endif

// Generate the html for the 8 IPv6 hextets with joining colons
function genHtmlIpv6Blocks(name: string, required?:boolean) {
  var html = "";
  var classDef = "ipv6-hextet";
  if (required)
    classDef = "required " + classDef;
  [":",":",":",":",":",":",":",""].forEach(function(dot, i) {
    var id = name + "_" + i;
    var inputAttribs = {
              type:'text', class: classDef, maxLength: 4, name: id, id: id,
              onKeyPress: "onKeyPressIpv6(this,event);",
              onPaste: "onPasteIpv6(this,event);",
              onCopy: "onCopyIpv6(this,event);"
              };
    var label = (i > 0) ? htmlTag("label", {for: id, class:'input-connect-colon'}, dot): "";
    html += htmlDiv({id: "div_" + id, class:'field'}, label + htmlTag("input", inputAttribs, ""));
  });
  return html;
}

// This function takes an IPv6 string and populates the page fields
// ipaddr is the IPv6 string. It can be in various forms including a version ending in IPv4 like ::127.0.0.1
// name is the group name of the fields
// If an IPv4 form is used the IP fields are modified from 8 hextets to 6 hextexts and one bigger IPv4 fields
function parse_ipv6_into_fields(ipaddr, name) {
  ipaddr = ipaddr.replace(/\s+/g, '');  //Remove all spaces

  var hextets = ipaddr.split(":");
  var octets = hextets[hextets.length-1].split('.'); // Any Ipv4 at end?
  var numFields = 8;
  var field6 =  $("#" + name + "_6"); // This is the field that changes form

  if (octets.length === 4) {  // If we have an IPv4 component we need to change the size and filter of the fields
    numFields = 7;
    field6.removeClass("ipv6-hextet");
    field6.addClass("ipv4-address");
    field6.attr("maxlength", 16);
    field6.attr("onKeyPress","onKeyPressIpv6v4(this,event);");
    $("#div_" + name + "_7").hide();  // This is surplus to requirements so hide it
  }
  else { // This reverses the if for standard Ipv6 presentation
    field6.removeClass("ipv4-address");
    field6.addClass("ipv6-hextet");
    field6.attr("maxlength", 4);
    field6.attr("onKeyPress", "onKeyPressIpv6(this,event);");
    $("#div_" + name + "_7").show();
  }

  // Expand the ::
  var shortfall = numFields - hextets.length;
  if(shortfall > 0 && hextets.length !== 1) {
    for (var i = 0; i < hextets.length; i++) { // find the ::
      if (hextets[i] == "") {
        hextets[i] = "0";
        if (i === 0) {
          i--;
          continue;
        }
        // Add in the missing zeros
        for (var j = 0; j < shortfall; j++) {
          hextets.splice(i, 0, "0");
        }
        break; // Only one expansion of :: allowed
      }
    }
  }
  // Display the fields
  for(var i = 0; i < numFields; i++) {
    $("#" + name + "_" + i).val(hextets[i] || "0");
  }
}

// This function reads the html fields and returns a string representation of the address
// no attempt to shorten the string
function parse_ipv6_from_fields(name) {
  var hextets=[];
  for(var i = 0; i < 8; i++) {
    var val = $("#" + name + "_" + i).val();
    hextets[i] = (val != "") ? val : "0";
    if (hextets[i].indexOf('.') >= 0) break; // If it has dotted address it must be IPv4 at end
  }
  return hextets.join(":");
}

class EditableIpv6AddressVariable extends ShownVariable {
  setVal(obj: any, val: string){parse_ipv6_into_fields(val,this.memberName);}
  getVal() { return parse_ipv6_from_fields(this.memberName);}
  genHtml() { return genHtmlIpv6Blocks(this.memberName);}
}

function editableIpv6AddressVariable(memberName, labelText) {
  var pe = new EditableIpv6AddressVariable(memberName, labelText);
  pe.validateLua.push("isValid.Ipv6Address(v)");
  return pe;
}

class EditableIpMaskVariable extends ShownVariable {
  fnGenIPBlocks?: any;
  setVal(obj: any, val: string) { parse_ip_into_fields(val,this.memberName);}
  getVal() { return parse_ip_from_fields(this.memberName);}
  genHtml() { return this.fnGenIPBlocks(this.memberName);}
}

function editableIpMaskVariable(memberName: string, labelText: string, _fnGenIPBlocks?: any) {
  var fnGenIPBlocks = _fnGenIPBlocks;
#ifndef COMPILE_WEBUI
  if (!isDefined(_fnGenIPBlocks))
    fnGenIPBlocks = genHtmlMaskBlocks;
#endif
  var pe = new EditableIpMaskVariable(memberName, labelText,
          {
            setEnable: function(en) {}
          });
  pe.fnGenIPBlocks = fnGenIPBlocks;
  pe.validateLua.push("isValid.IpMask(v)");
  return pe;
}

function editableURL(memberName: string, labelText: string, extras?: any) {
  var pe = editableTextVariable(memberName, labelText, extras);
  pe.setValidate(
          function (val) { return true;}, "",
          function (field) {return urlFilter(field);},
          "isValid.URL(v)"
        );
  return pe;
}

function editableHostname(memberName: string, labelText: string, extras?: any) {
  var pe = editableTextVariable(memberName, labelText, extras);
  pe.setValidate(
          function (val) { return true;}, "",
          function (field) {return hostNameFilter(field);},
          "isValid.Hostname(v)"
        );
  return pe;
}

function editableUsername(memberName: string, labelText: string, extras?: any) {
  var pe = editableTextVariable(memberName, labelText, extras);
  pe.setValidate(
          function (val) { return true;}, "Invalid user name.",
          function (field) {return nameFilter(field);},
          "isValid.Username(v)"
        );
  return pe;
}

function editableUserOrEmail(memberName: string, labelText: string, extras?: any) {
  var pe = editableTextVariable(memberName, labelText, extras);
  pe.setValidate(
          function (val){ return true;}, "Invalid user name.",
          function (field) {return emailFilter(field);},
          "isValid.UserOrEmail(v)"
        );
  return pe;
}

function editableEmailAddress(memberName: string, labelText: string, extras?: any) {
  var pe = editableTextVariable(memberName, labelText, extras);
  pe.setValidate(
          function (val) {
            if (pe.required === false && val === "") {
              return true;
            }
            return validateEmail(val);
          }, "Invalid email address.",
          function (field) {/* TODO return emailFilter(field);*/},
          "isValid.EmailAddress(v)"
        );
  return pe;
}

function editableInteger(memberName: string, labelText: string, extras?: any) {
  var pe = editableTextVariable(memberName, labelText, extras);
  pe.setValidate(
          function (val) { return true;}, "",
          function (field) {return NumfieldEntry(field);},
           "isValid.Integer(v)"
        );
  return pe;
}

function editableBoundedInteger(memberName: string, labelText: string, lower: number, upper: number, msg: string, extras?: any) {
  return new EditableBoundedInteger(memberName, labelText, lower, upper, msg, extras);
}

// If the "lower" value is negative then permit a leading "-" in the entered digit, otherwise don't.
function editableBoundedFloat(memberName: string, labelText: string, lower: number, upper: number, msg: string, extras?: any) {
  var pe = editableTextVariable(memberName, labelText, extras);
  if (lower < 0) {
      pe.setValidate(
              function (val) { var n = +val;
                               return val != "" && isFinite(n) && n >= lower && n <= upper;},
              msg,
              function (field) {return field;},
              "isValid.BoundedSignedFloat(v," + lower + "," + upper + ")"
            );
  } else {
  pe.setValidate(
          function (vals: string) { var n = +vals;
                                    return vals != "" && isFinite(n) && n >= lower && n <= upper;},
          msg,
          function (field) {return field;},
          "isValid.BoundedFloat(v," + lower + "," + upper + ")"
        );
  }
  return pe;
}

function editablePortNumber(memberName: string, labelText: string) {
  return editableBoundedInteger(memberName, labelText, 1, 65535, "Msg126", {helperText: "1-65535"}).setSmall();
}

class ToggleVariable extends ShownVariable {
  setVal(obj: any, val: string){ setToggle(this.getHtmlId(), val);}
  getVal() { return String($("#"+this.getHtmlId()).val());}
  genHtml() { return genToggleHtml(this.getHtmlId(), this.onClick);}
}


function toggleVariable(memberName: string, labelText: string, onClick?: any) {
  var pe = new ToggleVariable(memberName, labelText,
          {
            setEnable: function(en) { setToggleEnable(this.getHtmlId(), en);}
          });
  pe.onClick = onClick;
  pe.validateLua.push("isValid.Bool(v)");
  return pe;
}

class RadioButtonVariable extends ShownVariable {
  setVal(obj: any, val: string) { setRadioButtonVal(this.getHtmlId(), val); }
  getVal() { return getRadioButtonVal(this.getHtmlId()) ? "1" : "0" }
  genHtml() { return genRadioButtonHtml(this.getHtmlId(), this.onClick);}
}

function radioButtonVariable(memberName: string, labelText: string, onClick?: any) {
  var pe = new RadioButtonVariable(memberName, labelText,
          {
            setEnable: function(en) { setRadioButtonEnable(this.getHtmlId(), en);}
          });
  pe.onClick = onClick;
  pe.validateLua.push("isValid.Bool(v)");
  return pe;
}

class ButtonAction extends ShownVariable {
  buttonText: string;
  getVal(){ return ""}
  genHtml() {
    var id = this.getHtmlId();
    return htmlTag( "button", {
      class:"secondary", id:id, type:"button", onClick: this.onClick + "('" + id + "');",
      style:"width:auto;margin-left:0;"
    },_(this.buttonText));
  }
}

function buttonAction(memberName: string, buttonText: string, onClick?:any, labelText?: string, extras?: any) {
  if(!isDefined(labelText)) {
    labelText = ""
  }
  var pe = new ButtonAction(memberName, labelText, extras);
  pe.onClick = onClick;
  pe.buttonText = buttonText;
  return pe;
}
// The objVisibilityVariable is just a toggleVariable with some
// implicit properties (hence less parameters)
// The onClick will automatically be setup by the PageObj()
function objVisibilityVariable(memberName, labelText) {
  var pe = toggleVariable(memberName, labelText, "");
  pe.visibilityVar = true;
  return pe;
}

type OptionsType = string | [(number|string),string];
type GetOptions = (obj?: any) => OptionsType[];

class SelectVariable extends ShownVariable {
  fnOptions: GetOptions;
  onChange?: string;
  constructor(name: string, labelText: string, fnOptions: GetOptions, onChange?: string, extras?: any) {
    super(name, labelText, extras);
    this.fnOptions = fnOptions;
    this.onChange = onChange;
  }
  setVal(obj: any, val:string) { setOption(this.memberName,  this.fnOptions(obj),val);};
  getVal() { return String($("#sel_"+this.memberName).val()); };
  genHtml() {
    var attr : any =  {id: "sel_" +this.memberName, style:"width:auto"};
    if(isDefined(this.onChange)) {
      attr.onChange = this.onChange + "(this);";
    }
    return htmlTag('select', attr, "");
  };
}

function selectVariable(memberName: string, labelText: string, fnOptions: GetOptions, onChange?: string) {
  var pe = new SelectVariable(memberName,labelText, fnOptions, onChange);

  // Added all the supplied options
  var options = fnOptions();
  var luaOptions = [];
  options.forEach(function(opt){
      if ( typeof opt == "string") {
        luaOptions.push(opt);
      }
      else { // If not a string it's a two string array
        luaOptions.push(opt[0]);
      }
  });
  if (luaOptions.length > 0) {
    pe.validateLua.push("isValid.Enum(v,{'" + luaOptions.join("','") + "'})");
  }
  return pe;
}

function iconLink(icon, labelText) {
  var pe: any = {};
  pe.labelText = labelText;
  pe.icon = icon;
#ifdef V_WEBIF_SPEC_vdf
  if (icon == "edit")
    pe.genHtml = function(){ return htmlTag('a', {href: "", class: 'secondary sml'},
                                htmlTag('i', {class: 'icon ' + icon}, _(icon)));};
  else
#endif
    pe.genHtml = function(){ return htmlTag('a', {href: "", class: 'secondary sml', style: 'padding:0;border:0;'},
                                htmlTag('i', {class: 'icon ' + icon}, ''));};
  return pe;
}

// hostname with optional port number
function editableHostnameWPort(memberName, labelText, extras) {
  var pe = editableTextVariable(memberName, labelText, extras);
  pe.setValidate(
          function (val) { return true;}, "",
          function (field) {
            // apply filter to hostname and port separately
            var tokens=field.value.split(":");
            var host={value:tokens[0]};
            hostNameFilter(host);
            if (tokens.length > 1) {
              var port={value:tokens[1]};
              NumfieldEntry(port);
              host.value=host.value.concat(":",port.value);
            }
            if (field.value.length != host.value.length){
              field.value = host.value;
            }
          },
          "isValid.HostnameWPort(v)"
        );

  return pe;
}

class SectionHeader extends ShownVariable {
  genHtml() {
    return htmlTag('h2', {}, this.labelText );
  }
}

function sectionHeader(labelText) {
  var pe = new SectionHeader("noId", _(labelText));
  pe.unwrapped = true;
  return pe;
}

class FileUploader extends ShownVariable {
  onUploaderFileChange? : any;
  onPosted? : (obj:object) => void;
  targetFile: string;
  fileExt: string[];
  readOnly = true;
  // target file is the file page to the file on the modem
  // fileExt is an array of valid file extensions - [".gz",".zip"]
  constructor (memberName: string, labelText: string, targetFile: string, fileExt: string[], getParams: null | ((fname: string) => string[]), errExt: string, errLoad: string, showProgress?: boolean) {
    super(memberName, labelText);
#ifdef COMPILE_WEBUI
    if (!isDefined(filesUploadable)) {
      filesUploadable = {};
    }
    filesUploadable[memberName] = targetFile;
    this.luaGet = (po: PageObject) => "getFiles('"+targetFile+"')";
#endif
    this.targetFile = targetFile;
    this.fileExt = fileExt;
    this.helperText = _("not uploaded");
  #ifndef COMPILE_WEBUI
    this.onUploaderFileChange = (url, fileName, commit) => {

      var params = isFunction(getParams)? getParams(fileName): [];
      var startIndex = fileName.lastIndexOf("\\");
      if (startIndex >= 0) {
          fileName = fileName.substr(startIndex);
      }

      if (fileExt.length > 0 && isDefined(errExt) && !commit){
        var extIndex = fileName.lastIndexOf(".");
        if ((extIndex == -1) || fileExt.indexOf(fileName.substr(extIndex))==-1) {
          blockUI_alert(_(errExt));
          return;
        }
      }

#ifndef V_WEBIF_SERVER_turbo
      this.setFileUploadStatus("not uploaded");
#endif
      var formData = new FormData();
      params.forEach(function(p,i){
        formData.append("param"+i, p);
      });
      formData.append("csrfTokenPost",csrfToken);
#ifdef V_WEBIF_SERVER_turbo
      url = "/upload" + url.split('.').slice(0, -1).join('.');

      // name field must present
      formData.append("name", targetFile);
      formData.append("target", memberName);
      formData.append("commit", commit?"1":"0");

      // file field must be the last part of the form data
      if(!commit) {
        formData.append("file", document.getElementById("inp_" + memberName).files[0]);
      }

#else
      formData.append(memberName, document.getElementById("inp_" + memberName).files[0]);
#endif

      if(!showProgress) {
        blockUI_wait(_("GUI pleaseWait"));
      }
      else {
        blockUI_wait_progress(_("GUI pleaseWait"));
      }

      let self = this;
      $.ajax({
        xhr: function() {
          var xhr = new XMLHttpRequest();
          xhr.upload.addEventListener("progress", function(evt){
            if (evt.lengthComputable && showProgress) {
              var percentComplete = evt.loaded / evt.total * 100;
              $("#progressbar").progressbar({value: percentComplete});
            }
          }, false);
          return xhr;
        },
        url: url,
        data: formData,
        processData: false,
        contentType: false,
        type: 'post',
        success: (respObj) => {
          $.unblockUI();
          if ( respObj.result == "0" ) {
            var message = "uploaded";

#ifdef V_WEBIF_SERVER_turbo
            message = respObj.message;
            self.lengthyOperation(respObj);
            fileName = undefined;
#endif
            self.setFileUploadStatus(message, fileName);
          }
          else {
            blockUI_alert(_(errLoad));
          }

          if(self.onPosted) {
            self.onPosted(respObj);
          }
        },
        error: function(XMLHttpRequest, textStatus, errorThrown) {
          $.unblockUI();
        }
      });
    }
#endif
  }

  lengthyOperation(respObj) {
    if(respObj.ping_delay > 0) {
      blockUI_wait_progress(_(respObj.message));
      let seconds_passed = 0;
      let extras = respObj.extras.split(";");

      // update lengthy operation progress
      setInterval(() => {
        seconds_passed += 1;
        let percent = seconds_passed / respObj.ping_delay * 100;

        // step the progress bar back a little bit so it would keep
        // moving forward though it takes longer than expected
        if(percent > 98) {
          respObj.ping_delay *= 1.02;
          percent = seconds_passed / respObj.ping_delay * 100;
        }
        $("#progressbar").progressbar({value: percent});
      }, 1000);

      // check if the web is back
      setTimeout(() => {
        let timer = setInterval(() => {
          $.ajax({
            url: respObj.ping_url,
            timeout: 800,
            success: () => {
              clearInterval(timer);

              if(extras[1] && extras[1].length) {
                blockUI_wait_confirm(_(extras[1]), _("CSok"), () => {
                  $(location).attr('href', respObj.ping_url);
                });
              }
              else {
                $(location).attr('href', respObj.ping_url);
              }
            },
            error: () => {
              if(extras[0] && extras[0].length) {
                $("#progress-message").text(_(extras[0]));
              }
            }
          });
        }, 1000);
      }, respObj.ping_delay * 1000);
    }
  }

  getVal() {return ""; }
  setVal(obj: any, val: string) {
    var sts = _("not uploaded");
    var files: string[] | undefined = obj[this.memberName];
    let fileNameMatch = this.targetFile;
    if (isDefined(obj.id)) {
      fileNameMatch = this.targetFile.replace("*", String(obj.id))
    }

    function lookForMatch(fileToMatch) {
      for (var file of files) {
        if (file.indexOf(fileToMatch) >= 0)
          sts = _("uploaded");
      }
    }

    if (isDefined(files)) {
      // 2nd * is for extension
      if (fileNameMatch.indexOf("*") >= 0) {
        for (let ext of this.fileExt) {
          lookForMatch(fileNameMatch.replace("*", ext));
        }
      }
      else {
        lookForMatch(fileNameMatch);
      }
    }
#ifndef V_WEBIF_SERVER_turbo
    this.setFileUploadStatus(sts);
#endif
  }
  genHtml() {
    var attrs: any = {id: "inp_" + this.memberName, type: "file"};
    if (this.fileExt.length > 0) {
      attrs.accept = this.fileExt.join(",");
    }
    return  htmlTag("span", {class: "file-wrapper"},
                htmlTag("input", attrs, "") +
                htmlTag("span", {class: "secondary button"}, _("chooseFile")));
  };
/*
  fileNameMatch?: string;
  setMatchFile(fn: string) {
    this.fileNameMatch = fn;
    return this;
  }
*/
  onInDOM = () => {
    var el = <HTMLInputElement|null>document.getElementById("inp_" + this.memberName);
    if (el)
      el.onchange = (ev: Event) => {
        var el = <HTMLInputElement>ev.currentTarget;
        this.onUploaderFileChange("/" + relUrlOfPage, el.value);
      }
  }

  setFileUploadStatus(sts: string, fileName?: string) {
    let id = "#" + this.memberName + "_helperText";
    $(id).html((isDefined(fileName) &&  fileName != "" ? "File '" + fileName + "' - ": "") +  _(sts));
  }

}
