// QOS cbq.init functions

// Load the form input element values from the variables derived from the RDB values
function formLoad() {
  setFormElementsFromRdb();

  // Update derived radio button elements
  if ($("#id_enabled").val() == '1') {
    $("#enabledButton_0").attr("checked", "checked");
  } else {
    $("#enabledButton_1").attr("checked", "checked");
  }

  displayInstalledFiles(cbqFiles);
}

function submitSettings() {
  $("#submit_form").attr("disabled", true);
  blockUI_wait(_("GUI pleaseWait"));
  $("#form").submit();
}

/* jQuery main function */
$(function() {

  //
  // Add click handlers
  //
  $("#submit_form"    ).click(function() { submitSettings(); });
  $("#enabledButton_0").click(function() { $("#id_enabled").val(1); });
  $("#enabledButton_1").click(function() { $("#id_enabled").val(0); });


  // Send a post cmd to upload the file once selected.
  var fileUploader = new cgi("./cgi-bin/qos_cbqInit_upload.cgi");
  function doUpload(cmd, inputId, func) {
    fileUploader.reset();
    fileUploader.setcmd(cmd);
    fileUploader.up("#" + inputId, func, true);
  }
  // Bind to the file upload button to send the files
  $("#id_uploadCbqInitFile").change(function() {
    doUpload("upload", "id_uploadCbqInitFile",
             function(rspData) {
               if (rspData.result == "ok") {
                 location.reload();
               } else {
                 window.alert("<pre>" + _("cbqBadFileName") +
                              "\n\nFile was: " + rspData.name +
                              "\nmsg: " + rspData.result +
                              "</pre>");
               }
             });
  });
});

$(document).ready(function() {
  formLoad();
});

// List the installed cbq files.
// Display clickable "delete", and "show" buttons on each line.
function displayInstalledFiles(files) {

  // Create a new list
  $("#tbody_cbqFiles").empty();
  var i;
  for (i = 0; i < files.length; i++) {
    $("#tbody_cbqFiles").append(
      "<tr>" +
        "<td>" + (i+1)    + "</td>" +
        "<td>" + files[i] + "</td>" +
        "<td>" + "<button type='button' class='secondary short' id='delFile_" + i + "'>" +
                   _("delete") + "</button>" + "</td>" +
        "<td>" + "<button type='button' class='secondary short' id='showFile_" + i + "'>" +
                   _("show") + "</button>" + "</td>" +
      "</tr>");

    setFileClickFuncs("#delFile_" + i, "#showFile_" + i, files[i]);
  }
}

function setFileClickFuncs(delId, showId, file) {
  $(delId).click(function()  { delFile(file); });
  $(showId).click(function() { showFile(file); });
}

function delFile(file)
{
  $.ajax({
    url:"./cgi-bin/qos_cbqInit_file.cgi?delete=" + file,
    async: false,
  });

  // Update the list
  location.reload();
}

function showFile(file)
{
  // Send a get request to the server and interpret the JSON response when it arrives.
  $.ajax({
    url: "./cgi-bin/qos_cbqInit_file.cgi?show=" + file,
    success:
      function(rspData, status) {
        displayFile(rspData, file)
      },
    timeout: 5000,              // 5s
    error:
      function(jqXHR, textStatus, errorThrown) {
        var msg = "Comms error: " + textStatus;
        window.alert(msg);
      }
  });

}

function displayFile(data, name)
{
  // Open a new window
  var win = window.open("", "", "toolbar=no,width=400,height=200,scrollbars=yes");
  win.document.write("<title>" + name + "</title>");
  win.document.write(data);
  win.document.close();
}
