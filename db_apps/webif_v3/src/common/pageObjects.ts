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
  columns? : any;       // supporting multiple column display
  genObjHtml? : () => string;
  onInDOM? : () => void;
  objName? : string;
  readOnly?: boolean;
  textOnly?: boolean;	// if true then do not add <span> tag
  labelText? : string,
  obj? : any;
  delObj? : (id:any) => boolean;
  initObj? : () => any;
  packageObj? : () => any;
  decodeRdb? : (obj: any) => any;
  encodeRdb? : (any) => any;
  populate? : () => void;
  postPopulate? : () => void;
  setVisible? : (v?: string) => void;
  setEnabled? : (boolean) => void;
  pollPeriod? : number;
  customLua? : (boolean | CustomLua);  // true if a lua file has been created
  authenticatedOnly? : boolean;
  readGroups? : string[]
  writeGroups? : string[]
  rdbPrefix? : string;
  visibilityVar? : string;
  extraAttr? : any;         // array of extra attributes to replace previous
                            // tableAttr, thAttr etcs.
  colgroup? : string[];
  editLabelText? : string;
  arrayEditAllowed? : boolean;
  editRemovalOnly? : boolean;
  onEdit?: (obj: any) => void;
  offEdit?: () => void;
  saveObj? : (obj: any) => void;
  getValidationError?: (obj: any) => string;
  clear? : () => void;
  editClear? : () => void;
  sendThisObjOnly? : boolean;   // If set to true, send only this object in form submission.
                                // By default all page objects are included.
  rdbIndexQueryParam? : string; // query string param to be used as RDB index
  defaultId? : number; // default id value if query string does not contain id=<idVal>
  pageWarning? : string[] // warning message string array
                          // first element is header and others are normal text
  autoScrollTable? : boolean;   // If set to true, then separate table header and body
                                // to fix the header and auto scroll body contents
  postPageObj?: () => void;
  invisible?: boolean; // to be hidden if set this to true
  setPageObjectVisible?: (visible: boolean) => void; // set page object visible or invisible
  indexHoleAllowed? : boolean;  // Allow index hole in RDB array
}

function setMemberVisibility(_this: PageObject, objShown: boolean, members: PageElement[]) {
  for (var pe of members) {
    if (pe.hidden) {
      continue;
    }
    if(pe.memberName !== _this.visibilityVar) {
      var show = objShown;
      if (isFunction(pe.fnIsVisible)) {
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
              // Object can be deleted before
            if (isDefined(pgeObj.obj[mem.memberName])) {
              obj[mem.memberName] = pgeObj.obj[mem.memberName];
            }
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
        var pgeObj = this;
        var disableWrite = false;
        if (pageData.authenticatedOnly ?? true) {
          const writeGroups = pgeObj.writeGroups ?? pageData.writeGroups ?? ["root", "admin"];
          disableWrite = !checkGroupAccess(writeGroups, userGroups);
        }

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
          if ((pgeObj.readOnly || mem.readOnly || disableWrite) && isFunction(mem.setEnable)) {
            mem.setEnable(false);
          }
          if (isFunction(pgeObj.postPopulate)) {
            pgeObj.postPopulate();
          }
        });
      },

      // clear contents of all page elements of this page object
      clear: function() {
        this.members.forEach(function(mem) {
          mem.setVal(this, "");
        });
      },

      // clear contents of all edit page elements of this page object
      editClear: function() {
        this.editMembers.forEach(function(mem) {
          mem.setVal(this, "");
        });
      },

      // set entire page object visible (if parameter visible is true) or invisible
      // condition: DOM is ready
      setPageObjectVisible: function(visible: boolean) {
        this.invisible = !visible;
        if (this.invisible) {
          setVisible("#objouterwrapper"+this.objName, false);
        } else {
          setVisible("#objouterwrapper"+this.objName, true);
        }
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

    if ((typeof Obj.postPageObj === "function")) {
        Obj.postPageObj();
    }

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

// @params extraAttr Provide extra attributes. Label attributes is used here at the moment
function _genVariableList(members: PageElement[], extraAttr?: any) {
  function labelAndField(mem: PageElement, field) {
    var divAttr: any = {class:"field"};
    if (isDefined(mem.style)) divAttr.style = mem.style;
    if (isDefined(mem.class)) divAttr.class = mem.class;
    var lfor = {};
    if (mem.labelFor) lfor = { "for": mem.labelFor};
    var labelTag = "label";
    if (isDefined(extraAttr) && isDefined(extraAttr.labelTag)) {
      labelTag = extraAttr.labelTag;
    }
    return htmlDiv({id: "div_" + mem.memberName, class: "form-row"},
              htmlTag(labelTag, lfor, _(mem.labelText)) +
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
  var headerTag = "h2-body";
  var boxAttr = "body-box";   // default body box
  var tAttr = pgeObj.extraAttr;
  if (isDefined(tAttr) && isDefined(tAttr.headerTag)) {
    headerTag = tAttr.headerTag;
  }
  if (isDefined(tAttr) && isDefined(tAttr.boxAttr)) {
    boxAttr = tAttr.boxAttr;
  }
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
    if (isDefined(tAttr) && isDefined(tAttr.thAttr)) {
       var len = tAttr.thAttr.length;
       if (col >= len) col = len - 1;
       return tAttr.thAttr[col];
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
                            {type: "button", onClick: "table_add('" + this.objName + "');", class: "secondary editAdd fr", id: this.objName + "_addBtn"},
                            htmlTag("i", {class: "icon plus"}, "" ) + _("add")));
#ifdef V_WEBIF_SPEC_vdf
    button = htmlDiv({class: "pad omega"}, button);
#endif
    heading = htmlDiv({class: "grid-66"},
                      htmlTag(headerTag,{}, _(this.labelText)) ) +
               htmlDiv({class: "grid-33"}, button );
  }
  else if (this.editRemovalOnly) {
    thead += htmlTag("th", getThAttr(idx++), "");
    heading = htmlDiv({class: "grid-66"},
                      htmlTag(headerTag,{}, _(this.labelText)) ) +
               htmlDiv({class: "grid-33"}, button );
  }
  else if (this.labelText != "") {
    heading = htmlTag(headerTag,{}, _(this.labelText));
  }

  thead = htmlTag("thead", {},  htmlTag("tr", {}, thead));

  // If autoScrollTable is true, then separate table header and body
  // to fix the header and auto scroll body contents
  var table;
  if (this.autoScrollTable) {
    var tableHeaderAttribs = {id: this.objName + "Heder"};
    var tableAttribs = {id: this.objName};
    if (isDefined(tAttr) && isDefined(tAttr.tableAttr)) {
      tableHeaderAttribs = mergeObject(tableHeaderAttribs, tAttr.tableAttr);
      tableAttribs = mergeObject(tableAttribs, tAttr.tableAttr);
    }
    table = htmlDiv({id: "table" + this.objName + "Header", class: "autoScrollTable"},
                    heading +
                    htmlTag("table", tableHeaderAttribs, colgroup + thead)) +
            htmlDiv({id: "table" + this.objName, class: "autoScrollTable"},
                    htmlTag("table", tableAttribs,
                      htmlTag("tbody", {},"")));
  } else {
    var tableAttribs = {id: this.objName};
    if (isDefined(tAttr) && isDefined(tAttr.tableAttr)) {
      tableAttribs = mergeObject(tableAttribs, tAttr.tableAttr);
    }
    table = htmlDiv({id: "table" + this.objName},
                    heading +
                    htmlTag("table", tableAttribs,
                      colgroup + thead + htmlTag("tbody", {},"")
                    )
                  );
  }
  if (this.arrayEditAllowed) {
    table += htmlDiv({id: "edit" + this.objName, style:"display:none"},
                htmlTag(headerTag,{}, _(this.editLabelText)) +
#ifdef V_WEBIF_SPEC_vdf
                    htmlDiv({class: boxAttr}, _genVariableList(this.editMembers))
#else
                    _genVariableList(this.editMembers)
#endif
              );
  }

  var members = htmlDiv({id: "objwrapper"+this.objName}, table);

  var styleCss = "";
  if (this.invisible) {
    styleCss += "display:none;";
  }

#ifdef V_WEBIF_SPEC_vdf
  return htmlDiv({id: "objouterwrapper" + this.objName}, members);
#else
  return htmlDiv({class: boxAttr+" form-row", id: "objouterwrapper" + this.objName, style: styleCss}, members);
#endif
}

function genVariableList() {
  var headerTag = "h2-body";
  if (isDefined(this.extraAttr) && isDefined(this.extraAttr.headerTag)) {
    headerTag = this.extraAttr.headerTag;
  }
  var heading = "";
  if (isDefined(this.labelText) && this.labelText !== "") {
    heading = htmlTag(headerTag,{}, _(this.labelText));
  }
  var members = htmlDiv({id: "objwrapper" + this.objName},
                        _genVariableList(this.members, this.extraAttr));
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

   genThemeHeader(pageData, userGroups);
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
        // Add warning message section first
        if (isDefined(pgeObj.pageWarning)) {
          columns[col] +=  "<h2-body><span>"+pgeObj.pageWarning[0]+"</span></h2-body>";
          for (var i = 1; i < pgeObj.pageWarning.length;i++) {
            columns[col] += "<p>"+pgeObj.pageWarning[i]+"</p>";
          }
        }
        if (typeof pgeObj.column !== "undefined") col = pgeObj.column - 1;
        if (!pgeObj.genObjHtml) {
          pgeObj.genObjHtml = genVariableList;
        }
        columns[col] += pgeObj.genObjHtml();
    });

    // Draw separate title box
    if (pageData.noTitleBox !== true) {
      var headerTag = "h2-titleBox-large";
      if (isDefined(pageData.style) && isDefined(pageData.style.headerCssClass)) {
          headerTag = pageData.style.headerCssClass;
      }
      var title = "";
      var titleText = _(pageData.title);
      if (isDefined(pageData.overrideTitle)) {
        titleText = pageData.overrideTitle;
      }
      title = htmlDiv({id: "objouterwrapper" + "TitleBox"},
                      htmlTag(headerTag, {}, titleText.toUpperCase()));
      $("#titleGoesHere").append(title);
    }

    $("#htmlGoesHere").append(columns[0]);
    if( columns[1].length > 0) $("#rightGoesHere").append(columns[1]);

    setVisible('#alertMsg','0');  // No success/failure messages on page load
    for (var pgeObj of pageData.pageObjects) { // Added html for the objects
      if (isFunction(pgeObj.onInDOM)) {
        pgeObj.onInDOM();
      }
    }

    genThemeHeader(pageData, userGroups);
}
interface PageDef {
  multiColumn? : any
  readGroups? : string[]
  writeGroups? : string[]
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
  noTitleBox? : boolean   // If false then draw separate title box, if true then no title box
  style? : any    // define page style
                  // style: {
                  //   titleCssClass: "title-box",  // default title box
                  //   bodyCssClass: "body-box"     // default body box
                  //   headerCssClass: "h2-titleBox"// default header style
                  // },
  overrideTitle? : string // Show this title string instead of title in the page
  showImmediate? : boolean // If true, then display the body before the page is fully loaded
}
