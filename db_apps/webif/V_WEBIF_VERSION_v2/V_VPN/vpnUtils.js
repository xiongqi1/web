// Utilities for the VPN pages

// Get a new link profile from the router.
function getNewLinkProfile() {
  var num = NaN;
  $.ajax({
    url:"./cgi-bin/linkProfileLib.cgi",
    dataType: 'json',
    data: {cmd:"get"},
    async: false,               // Do this synchronously
    success:
      function(rspData, status) {
        if (status == "success") {
          if (rspData.result == "ok") {
            num = rspData.newProfileNum;
          } else {
            alert(_("maxEnabledProfilesExceeded"));
          }
        } else {
          alert(_("errorsTitle"));
        }
      },
    timeout: 5000,
    error:
      function(jqXHR, textStatus, errorThrown) {
        console.log("Comms error: " + textStatus);
        alert(_("errorsTitle"));
      }
  });
  return num;
}
