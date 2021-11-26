// This function creates and/or populates the rows/cells in a table
// The html for the table has previously been
function populateTable(){
  var pgeObj: PageObject = this;
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
      var properHref = "javascript:table_" + icon.icon + "('" + pgeObj.objName + "'," + ob.id + ");";
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
          if (genHtml && isDefined(mem.genHtml)) cell.innerHTML = mem.genHtml();
          if (isDefined(val)) mem.setVal(ob, val);
    });

    if (pgeObj.arrayEditAllowed) {
      addIcon(row, "edit");
      addIcon(row, "close");
    }

    if (pgeObj.editRemovalOnly) {
      addIcon(row, "close");
    }
  });
}

// This function copies the selected row data into the edit html section
// The visibility is then shifted from the table to the edit form
// Buttons are set to save/cancel
function _table_edit(pgeObj: PageObject, obj) {
  var tableName = pgeObj.objName;

  clear_alert();

  function cancelTableEdit() {
    setVisible("#edit"+tableName, "0");
    setVisible("#cancelButton", "0");
    setVisible("#table"+tableName, "1");
    $("#saveButton").off('click').on('click', sendObjects);
    if (isDefined(pgeObj.offEdit)) pgeObj.offEdit();
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

    sendObjects(function() {
      pgeObj.populate();
      cancelTableEdit();
    });
  }

  if (isFunction(pgeObj.onEdit)) pgeObj.onEdit(obj);

  setVisible("#table"+tableName, "0");      // Hide static table
  pgeObj.editMembers.forEach(function(mem){ // transfer row data
      mem.setVal(obj, isDefined(mem.memberName) ? obj[mem.memberName]: "");
  });
  setVisible("#edit"+tableName, "1");       // Show edit area

  // Setup the buttons
  $("#cancelButton").click(cancelTableEdit);
  $("#saveButton").off('click').on('click', saveTableEdit);
  setVisible("#cancelButton", "1");
}

// The user had hit the edit button to modify a row in a table
// This is a global button so first locate the page object
function table_edit(tableName, id) {
  pageData.pageObjects.forEach(function(pgeObj) {
    if (pgeObj.objName == tableName) {
      pgeObj.obj.forEach(function(obj){ // Find the array element to edit
        if (obj.id === id)
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
        var arr = pgeObj.obj;
        arr.splice(id,1);
        // Resequence the ids
        for (var idx = 0; idx < arr.length; idx++) {
          arr[idx].id = idx;
        }
      }
      sendObjects();
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
        if (!isDefined(newObj.id)) {
          newObj.id = idx;
        }
        pgeObj.obj[idx] = newObj;
        _table_edit(pgeObj, pgeObj.obj[idx]);
      }
    }
  });
}
