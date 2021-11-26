// This function creates and/or populates the rows/cells in a table
// The html for the table has previously been
function populateTable(pageObj? : PageObject){
  var pgeObj: PageObject = this;
  if (isDefined(pageObj)) {
    pgeObj = pageObj;
  }

  var disableWrite = false;
  if (pageData.authenticatedOnly ?? true) {
    const writeGroups = pgeObj.writeGroups ?? pageData.writeGroups ?? ["root", "admin"];
    disableWrite = !checkGroupAccess(writeGroups, userGroups);
  }

  var table = <HTMLTableElement | null>document.getElementById(pgeObj.objName);
  if (!table) return;
  var tbody = <HTMLTableSectionElement | null>table.getElementsByTagName("tbody")[0];
  if (!tbody) return;

  while(tbody.rows.length > pgeObj.obj.length)
    tbody.deleteRow(tbody.rows.length-1);

  pgeObj.obj.forEach(function(ob,rowIndex) {
    function addIcon(row, type) {
      var cell = row.cells[colIndex++];
      if(!cell) cell = row.insertCell(-1);
      var icon = iconLink(type, "");
      var properHref = "javascript:table_" + icon.icon + "('" + pgeObj.objName + "'," + ob.__id + ");";
      cell.innerHTML = icon.genHtml(ob).replace('href=""', 'href="'+properHref+'"');
    }

    var row = tbody.rows[rowIndex];
    if (!row) row = tbody.insertRow(-1);
    var colIndex = 0;
    pgeObj.members.forEach(function(mem){
      if (mem.hidden) return;
      mem.htmlId = pgeObj.objName + "_" + rowIndex + "_" + mem.memberName;
      var genHtml = false;
      var cell = row.cells[colIndex++];
      if(!cell) {
         cell = row.insertCell(-1);
         genHtml = true;
      }
      cell.id = mem.htmlId;
      var val = ob[mem.memberName];
      // If pgeObj.textOnly is true then do not call mem.genHtml where
      // <span> tag is attached automatically for StaticTextVariable.
      // Chrome browser is incredibly slow when there are many <span> tags
      // are included in page. For example, logfile page takes more than 10
      // minutes when the log message file is 5MB.
      if (!pgeObj.textOnly && genHtml && isDefined(mem.genHtml)) {
        cell.innerHTML = mem.genHtml();
      }
      if (isDefined(val)) mem.setVal(ob, val);
      if ((pgeObj.readOnly || mem.readOnly || disableWrite) && isFunction(mem.setEnable)) {
        mem.setEnable(false);
      }
    });

    if (pgeObj.arrayEditAllowed) {
      addIcon(row, "edit");
      addIcon(row, "close");
    }

    if (pgeObj.editRemovalOnly) {
      addIcon(row, "close");
    }

    if (isFunction(pgeObj.postPopulate)) {
      pgeObj.postPopulate();
    }
  });
}

// get the row index of a table element
// @param htmlId The string representation of the html ID of the table element
// @return A number representation of the row index. Returns NaN on invalid htmlId
function getRowIndex(htmlId: string) {
  // htmlId of a select row member has the form: sel_<objName>_<rowIndex>_<memberName>
  // htmlId of any other row member has the form: <objName>_<rowIndex>_<memberName>
  var fields = htmlId.split("_");
  return fields[0] == "sel" ? parseInt(fields[2]) : parseInt(fields[1]);
}

// This function copies the selected row data into the edit html section
// The visibility is then shifted from the table to the edit form
// Buttons are set to save/cancel
function _table_edit(pgeObj: PageObject, obj) {
  var tableName = pgeObj.objName;

  clear_alert();

  function cancelTableEdit() {
    // remove error class
    $(".error").each(function(){
      $(this).removeClass("error");
    });
    // remove valid class
    $(".valid").each(function(){
      $(this).removeClass("valid");
    });

    setVisible("#edit"+tableName, "0");
    setVisible("#cancelButton", "0");
    setVisible("#table"+tableName, "1");
    $("#saveButton").off('click').on('click', sendObjects);
    if (isDefined(pgeObj.offEdit)) pgeObj.offEdit();

    // Throw away if the element is the new one which is not saved yet
    if (isDefined(pgeObj.obj) && pgeObj.obj.length > 0) {
        let lastObj = pgeObj.obj[pgeObj.obj.length-1];
        if (!lastObj.__saved) {
            pgeObj.obj.pop();
        }
    }
  }

  function cancelTableEditClearAlert() {
    clear_alert();
    cancelTableEdit();
  }

  function saveTableEdit() {
    pgeObj.editMembers.forEach(function(mem){
      if (isDefined(mem.memberName) && isFunction(mem.getVal)) {
        obj[mem.memberName] = mem.getVal();
      }
    });
    if (isFunction(pgeObj.saveObj)) pgeObj.saveObj(obj);
    obj.dirty = true;

    if (isFunction(pgeObj.getValidationError)) {
      var errMsg = pgeObj.getValidationError(obj);
      if (isDefined(errMsg)) {
        validate_alert("", _(errMsg));
        return;
      }
    }
    if (pgeObj.sendThisObjOnly) {
      // Send only this object
      sendSingleObject(pgeObj, function() {
        pgeObj.populate();
        cancelTableEdit();
      });
    } else {
      // Send all the objects in the page
      sendObjects(function() {
        pgeObj.populate();
        cancelTableEdit();
      });
    }
  }

  if (isFunction(pgeObj.onEdit)) pgeObj.onEdit(obj);

  setVisible("#table"+tableName, "0");      // Hide static table
  pgeObj.editMembers.forEach(function(mem){ // transfer row data
      mem.setVal(obj, isDefined(mem.memberName) ? obj[mem.memberName]: "");
  });
  setVisible("#edit"+tableName, "1");       // Show edit area

  // Setup the buttons
  $("#cancelButton").click(cancelTableEditClearAlert);
  $("#saveButton").off('click').on('click', saveTableEdit);
  setVisible("#cancelButton", "1");

  if ((typeof pgeObj.setVisible === "function") && (typeof pgeObj.visibilityVar !== "undefined")) {
    pgeObj.setVisible(obj[pgeObj.visibilityVar]);
  }
}

// The user had hit the edit button to modify a row in a table
// This is a global button so first locate the page object
function table_edit(tableName, id) {
  pageData.pageObjects.forEach(function(pgeObj) {
    if (pgeObj.objName == tableName) {
      pgeObj.obj.forEach(function(obj){ // Find the array element to edit
        if (obj.__id === id)
        _table_edit(pgeObj, obj);
      });
    }
  });
}

// The user had hit the delete button to remove a row in a table
// This is a global button so first locate the page object
function table_close(tableName, id) {
  pageData.pageObjects.forEach(function(pgeObj) {
    if (pgeObj.objName == tableName){

      // The default behaviour is to delete the array element and send the shorter array
      // Sometimes we keep the array element and mark the entry as deleted (link profiles)
      var delObj = true;
      if (isFunction(pgeObj.delObj)) {
        delObj = pgeObj.delObj(id);
      }
      if (delObj) {
        // If index hole is allowed, then the RDB array instance should be
        // deleted so mark here as deleted and actual deleting will be done
        // by set function in the page.
        if (pgeObj.indexHoleAllowed) {
          // Find the array element to delete
          for (const obj of pgeObj.obj) {
            if (obj.__id === id) {
                obj.__deleted = true;
                break;
            }
          }
        } else {
          var arr = pgeObj.obj;
          arr.splice(id,1);
          // Resequence the ids if index hole is not allowed.
          for (var idx = 0; idx < arr.length; idx++) {
            arr[idx].__id = idx;
          }
        }
      }
      if (pgeObj.sendThisObjOnly) {
        // Send only this object
        sendSingleObject(pgeObj);
      } else {
        // Send all the objects in the page
        sendObjects();
      }
    }
  });
}

// The user has hit the plus button to add a new row to a table
// This is a global button so first locate the page object
function table_add(tableName) {
  pageData.pageObjects.forEach(function(pgeObj) {
    if (pgeObj.objName == tableName) {
      // Create a new object at end
      var idx = pgeObj.obj.length;
      var newObj: any = {};
      if (isFunction(pgeObj.initObj)) {
        newObj = pgeObj.initObj();
      }
      if (isDefined(newObj)) {
        // Set the id to the index if it has not yet been set
        // If there is an index hole then find the first empty index to reuse
        if (!isDefined(newObj.__id)) {
          let lastId = -1;
          let holeId = -1;
          for (const obj of pgeObj.obj) {
            if (obj.__id > lastId + 1) {
              holeId = lastId + 1;
              break;
            }
            lastId = obj.__id;
          }
          newObj.__id = (holeId < 0)? idx:holeId;
        }
        // Mark this is a new object not saved yet.
        newObj.__saved = false;
        // Mark this is a new object so not a deleted object
        newObj.__deleted = false;
        pgeObj.obj[idx] = newObj;
        _table_edit(pgeObj, pgeObj.obj[idx]);
      }
    }
  });
}
