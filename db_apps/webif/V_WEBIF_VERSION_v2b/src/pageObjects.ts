interface CustomLua {
  lockRdb? : boolean;                       // This flag indicates whether we need to lock the rdb with access
  helpers? : string[];                      // An arbitrary set of lua statements added to the lua file
  get? : (auto: string[]) => string[];      // return list of lua statements to fetch data
  set? : (auto: string[]) => string[];      // return list of lua statements to save data
  validate? : (auto: string[]) => string[]; // return list of lua statements to validate data
}

// TODO - this will become a class
interface PageObject {
  editMembers? : PageElement[];
  members? : PageElement[];
  column? : number;
  columns? : any;
  multiColumns? : any;
  genObjHtml? : () => string;
  onInDOM? : () => void;
  objName? : string;
  readOnly?: boolean;
  labelText? : string,
  obj? : any;
  delObj? : (id:any) => boolean;
  initObj? : () => any;
  packageObj? : () => any;
  decodeRdb? : (obj: any) => any;
  encodeRdb? : (any) => any;
  populate? : () => void;
  setVisible? : (v?: string) => void;
  setEnabled? : (boolean) => void;
  pollPeriod? : number;
  customLua? : (boolean | CustomLua);  // true if a lua file has been created
  authenticatedOnly? : boolean;
  rdbPrefix? : string;
  visibilityVar? : string;
  tableAttribs? : any;
  thAttr? : any[];
  colgroup? : string[];
  editLabelText? : string;
  arrayEditAllowed? : boolean;
  editRemovalOnly? : boolean;
  onEdit?: (obj: any) => void;
  offEdit?: () => void;
  saveObj? : (obj: any) => void;
  getValidationError?: (obj: any) => string;
}

function setMemberVisibility(_this: PageObject, objShown: boolean, members: PageElement[]) {
  for (var pe of members) {
    if (pe.hidden) {
      continue;
    }
    if(pe.memberName !== _this.visibilityVar) {
      var show = objShown;
      if (show && isFunction(pe.fnIsVisible)) {
        show = pe.fnIsVisible();
      }
      pe.setVisible(show);
    }
  }
}

function PageObj(name: string, label: string, extras: PageObject) {
    var Obj: PageObject = {
      objName: name,
      labelText: label,
      packageObj: function () {
        if (this.readOnly) return;
        var pgeObj = this;
        var obj: any = {};
        pgeObj.members.forEach(function(mem){
          if (mem.readOnly) return;
          if (mem.getVal) { // Get from the page
            if (isDefined(mem.encode) && mem.encode === true)
              obj[mem.memberName] = encode(mem.getVal());
            else
              obj[mem.memberName] = mem.getVal();
          }
          else { // Take from the object itself
              obj[mem.memberName] = pgeObj.obj[mem.memberName];
          }
        });
        if (pgeObj.encodeRdb) {
          obj = pgeObj.encodeRdb(obj);
        }
        obj.name = pgeObj.objName;
        return obj;
      },

      onInDOM : function () {

        var callMembers = (members?: PageElement[] ) => {
          if (isDefined(members)) {
            for (var pe of members ) {
              if(isFunction(pe.onInDOM)) {
                pe.onInDOM();
              }
            }
          }
        }
        callMembers(this.members);
        callMembers(this.editMembers);
      },

      populate: function () {
        var obj = this.obj;
        this.members.forEach(function(mem) {
          if (mem.hidden) return;
          var val = obj[mem.memberName];
          if (isDefined(val)) {
            if (isDefined(mem.encode) && (mem.encode === true)) {
              val = decode(val);
            }
          }
          else {
            val = "";
          }
          if (isFunction(mem.setVal)) {
            mem.setVal(obj,val);
          }
        });
      }
    };

    mergeObject(Obj, extras);
    // If this object has a visibility toggle
    // Set up its onClick function
    Obj.members.forEach(function(mem){
      if (mem.visibilityVar){
        Obj.visibilityVar = mem.memberName;
        mem.onClick = "onClick" + Obj.visibilityVar;
      }
    });
    return Obj;
}

function PageTableObj(name: string, label: string, extras: PageObject) {
  var pgeObj = PageObj(name, label, extras);
  pgeObj.genObjHtml = genTable;
  pgeObj.populate = populateTable;
  pgeObj.packageObj = function () {
    if (this.readOnly) return;
    var obj: any = {};
    if (this.encodeRdb) {
      obj.objs = this.encodeRdb(this.obj);
    }
    else
      obj.objs = this.obj;
    obj.name = this.objName;
    return obj;
  }
  return pgeObj;
}

function _genVariableList(members: PageElement[]) {
  function labelAndField(mem: PageElement, field) {
    var divAttr: any = {class:"field"};
    if (isDefined(mem.style)) divAttr.style = mem.style;
    if (isDefined(mem.class)) divAttr.class = mem.class;
    var lfor = {};
    if (mem.labelFor) lfor = { "for": mem.labelFor};
    return htmlDiv({id: "div_" + mem.memberName, class: "form-row"},
              htmlTag('label', lfor, _(mem.labelText)) +
                            htmlDiv(divAttr, field) + helperText(mem));
  }
  function addVerification(html, matchName) {
    #ifdef V_WEBIF_SPEC_vdf
              return html.replace("class=", "equalTo='#inp_" + matchName + "' class=");
    #else
              return html.replace("[required]", "[required,equals[inp_" + matchName + "]]");
    #endif
  }

var list = "";
  if (members) {

    members.forEach( function(mem) {

      if (mem.hidden) return;
      var id = "inp_" + mem.memberName;
      mem.htmlId = id;
      if (isFunction(mem.genMultiRow)) {
        list += mem.genMultiRow(labelAndField);
      }
      else
        list += mem.unwrapped === true ? mem.genHtml(): labelAndField(mem, mem.genHtml());
     });
  }
  return list;
}

function genTable() {
  var pgeObj = this;
  var button =  "";
  var colgroup = "";
  var thead = "";
  var heading = "";
  if (this.colgroup) {
    this.colgroup.forEach(function (col) {
      colgroup += htmlTag("col",{width:col}, "" );
    });
    colgroup = htmlTag("colgroup", {},colgroup);
  }

  // Return the <th> attributes for column [col]
  // The page object can define an array of attributes.
  // The array size can be less than the columns (last element used)
  // If the page doesn't define <th> atributes return the default.
  function getThAttr(col) {
    if (isDefined(pgeObj.thAttr)) {
       var len = pgeObj.thAttr.length;
       if (col >= len) col = len - 1;
       return pgeObj.thAttr[col];
    }
    return {class:"align10"};
  }

  var idx = 0;
  this.members.forEach(function (mem){
    if (mem.hidden) return;
    thead += htmlTag("th", getThAttr(idx++), _(mem.labelText) );
  });
  if (this.arrayEditAllowed) {
    thead += htmlTag("th", getThAttr(idx++), "");
    thead += htmlTag("th", getThAttr(idx++), "");
    button = htmlDiv({class: "submit-row-condensed"},
                      htmlTag("button",
                            {type: "button", onClick: "table_add('" + this.objName + "');", class: "secondary sml fr"},
                            htmlTag("i", {class: "icon plus"}, "" ) + _("add")));
#ifdef V_WEBIF_SPEC_vdf
    button = htmlDiv({class: "pad omega"}, button);
#endif
    heading = htmlDiv({class: "grid-33"},
                      htmlTag("h2",{}, _(this.labelText)) ) +
               htmlDiv({class: "grid-66"}, button );
  }
  else if(this.editRemovalOnly) {
    thead += htmlTag("th", getThAttr(idx++), "");
    heading = htmlDiv({class: "grid-33"},
                      htmlTag("h2",{}, _(this.labelText)) ) +
               htmlDiv({class: "grid-66"}, button );
  }
  else {
    heading = htmlTag("h2",{}, _(this.labelText));
  }

  thead = htmlTag("thead", {},  htmlTag("tr", {}, thead));

  var tableAttribs = {id: this.objName};
  if (isDefined(this.tableAttribs)) tableAttribs = mergeObject(tableAttribs, this.tableAttribs);
  var table =  htmlDiv({id: "table" + this.objName},
                  heading +
                  htmlTag("table", tableAttribs,
                    colgroup + thead + htmlTag("tbody", {},"")
                  )
                );
  if (this.arrayEditAllowed) {
    table += htmlDiv({id: "edit" + this.objName, style:"display:none"},
                htmlTag("h2",{}, _(this.editLabelText)) +
#ifdef V_WEBIF_SPEC_vdf
                    htmlDiv({class: "grey-box"}, _genVariableList(this.editMembers))
#else
                    _genVariableList(this.editMembers)
#endif
              );
  }

  var members = htmlDiv({id: "objwrapper"+this.objName}, table);

#ifdef V_WEBIF_SPEC_vdf
  return htmlDiv({id: "objouterwrapper" + this.objName}, members);
#else
  return htmlDiv({class: "grey-box form-row", id: "objouterwrapper" + this.objName}, members);
#endif
}

function genVariableList() {
  var heading = htmlTag("h2",{}, _(this.labelText));
  var members = htmlDiv({id: "objwrapper" + this.objName},
                        _genVariableList(this.members));
  this.setVisible = function(v?: string){
    var pgeObj = this;
    var visVar = pgeObj.visibilityVar;
    if (!isDefined(visVar)) {
            setMemberVisibility(<PageObject>pgeObj, true, pgeObj.members);
    } else {
        if(isDefined(v)) { // This means we got here from a click
            if (isFunction(pgeObj.setEnabled)) {
                pgeObj.setEnabled(v=="1");
            }
        }
        else {
            v = this.obj[visVar];
        }
        setToggle("inp_" + visVar, v);
        setMemberVisibility(<PageObject>pgeObj, toBool(v), pgeObj.members);
    }
  };
  return htmlDiv({id: "objouterwrapper" + this.objName}, heading + members);
}

// generate HTML for displaying an alert with heading and text
// the function can be called during interaction with user
function displayAlert(heading, text) {
   var alert = htmlDiv({class:"grid-9 alpha"},
                  htmlDiv({class: "note-lrg"},
                  htmlDiv({class: "wrap alert clearfix" },
                     htmlTag("h2", {}, heading) +
                     htmlTag("p", {}, text)
                 )));

   // Generate the HTML template for the look and feel
   document.body.innerHTML = genBodyHtml(alert);

   setVisible('#form','0');
   setVisible("#body","1");
   set_menu(pageData.menuPos[0], pageData.menuPos[1], user);
}

function genPage() {

    if (pageData.disabled === true) {
      displayAlert(_(pageData.alertHeading), _(pageData.alertText));
      return;
    }

    document.body.innerHTML = genBodyHtml(""); // Generate the HTML template for the look and feel
    var columns = []; columns[0] = ""; columns[1] = "";
    pageData.pageObjects.forEach(function(pgeObj) { // Added html for the objects
        var col = 0;
        if (typeof pgeObj.column !== "undefined") col = pgeObj.column - 1;
        if (!pgeObj.genObjHtml) {
          pgeObj.genObjHtml = genVariableList
        }
        columns[col] += pgeObj.genObjHtml();
    });
    $("#htmlGoesHere").append(columns[0]);
    if( columns[1].length > 0) $("#rightGoesHere").append(columns[1]);

    setVisible('#alertMsg','0');  // No success/failure messages on page load
    for (var pgeObj of pageData.pageObjects) { // Added html for the objects
      if (isFunction(pgeObj.onInDOM)) {
        pgeObj.onInDOM();
      }
    }
   set_menu(pageData.menuPos[0], pageData.menuPos[1], user);
}
interface PageDef {
  multiColumn? : any
  disabled? : boolean
  alertHeading? : string
  alertText? : string
  pageObjects? : PageObject[],
  alertSuccessTxt? : string | (() => string)
  suppressAlertTxt? : boolean
  onReady? : ((Object) => void)
  onSubmit? : ((Object) => boolean)
  onDataUpdate? : ((Object) => void)
  validateOnload? : boolean
  menuPos? : string[]
  rootOnly? : boolean
  authenticatedOnly? : boolean
  onDevice? : boolean
  URL? : string
  title? : string
}
